// Snes9x PS5 frontend: the Snes9x core and its port callbacks.
//
// Frame pacing:
//   - 60 Hz (NTSC) games: Present waits for the flip, so the console's vsync paces the emulation, and
//     Snes9x's dynamic rate control stretches the sound by up to 0.5% to keep the audio ring half full
//     (the SNES runs at 60.10 Hz, the TV at 60.00 Hz).
//   - 50 Hz (PAL) games: the audio clock paces instead (wait while the ring is more than half full), and
//     the flips don't wait (each frame still waits, before writing, for the buffer it last flipped).
//   - In any case, if the ring gets 3/4 full (vsync not blocking for some reason), wait on audio.
//   - Fast forward (hold R2): no waiting, sound off, 1 frame in 4 drawn.
//
// SPDX-License-Identifier: MIT

#include "fe_emu.h"

#include "fe_games.h"
#include "fe_settings.h"

#include "OrbisPaths.h"
#include "ProsperoAudio.h"
#include "ProsperoInput.h"
#include "ProsperoSce.h"
#include "ProsperoVideo.h"

#include "snes9x.h"
#include "apu/apu.h"
#include "cheats.h"
#include "conffile.h"
#include "controls.h"
#include "display.h"
#include "fscompat.h"
#include "gfx.h"
#include "memmap.h"
#include "ppu.h"
#include "snapshot.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <vector>

namespace
{
// SNES button ids for S9xMapButton/S9xReportButton: (player << 4) | button
enum SnesBtn
{
	B_A,
	B_B,
	B_X,
	B_Y,
	B_L,
	B_R,
	B_START,
	B_SELECT,
	B_UP,
	B_DOWN,
	B_LEFT,
	B_RIGHT,
	B_COUNT
};
const char* const kBtnNames[B_COUNT] = {"A", "B", "X", "Y", "L", "R", "Start", "Select", "Up", "Down", "Left", "Right"};

// PS5 -> SNES, by position: Cross (bottom) = B, Circle (right) = A, Square (left) = Y, Triangle (top) = X.
const uint32_t kPadFor[B_COUNT] = {
	SCE_PAD_BUTTON_CIRCLE, SCE_PAD_BUTTON_CROSS, SCE_PAD_BUTTON_TRIANGLE, SCE_PAD_BUTTON_SQUARE,
	SCE_PAD_BUTTON_L1, SCE_PAD_BUTTON_R1, SCE_PAD_BUTTON_OPTIONS, SCE_PAD_BUTTON_TOUCH_PAD,
	SCE_PAD_BUTTON_UP, SCE_PAD_BUTTON_DOWN, SCE_PAD_BUTTON_LEFT, SCE_PAD_BUTTON_RIGHT};

uint32_t MakeId(int player, int btn)
{
	return uint32_t(((player + 1) << 4) | btn);
}

struct State
{
	bool core_ok = false;
	bool loaded = false;
	std::string rom_path;
	bool multitap = false;
	bool fast_forward = false;
	bool quit = false;
	uint32_t frame = 0;
	uint32_t prev_p1 = 0;
	std::vector<int16_t> mix;
	// last frame, for the pause menu
	std::vector<uint16_t> last;
	int last_w = 0, last_h = 0;
};
State g;

bool IsPal()
{
	return Memory.ROMFramesPerSecond == 50;
}

void SetControllers(bool multitap)
{
	S9xSetController(0, CTL_JOYPAD, 0, 0, 0, 0);
	if (multitap)
		S9xSetController(1, CTL_MP5, 1, 2, 3, -1);
	else
		S9xSetController(1, CTL_JOYPAD, 1, 0, 0, 0);
	S9xVerifyControllers();
	g.multitap = multitap;
	OrbisLog("[emu] port 2: %s", multitap ? "multitap (3-4 players)" : "joypad");
}

void MapButtons()
{
	S9xUnmapAllControls();
	char cmd[64];
	for (int p = 0; p < ps5input::kMaxPads; p++)
		for (int b = 0; b < B_COUNT; b++)
		{
			snprintf(cmd, sizeof(cmd), "Joypad%d %s", p + 1, kBtnNames[b]);
			S9xMapButton(MakeId(p, b), S9xGetCommandT(cmd), false);
		}
}

double Now()
{
	timespec ts = {};
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

void WaitAudioBelow(int frames)
{
	// at most ~100 ms, so a stalled audio thread can't freeze the game
	for (int i = 0; i < 100 && ps5audio::Queued() > frames; i++)
		usleep(1000);
}

std::string StatePath(int slot)
{
	char ext[8];
	snprintf(ext, sizeof(ext), ".%03d", slot);
	return S9xGetFilename(ext, SNAPSHOT_DIR);
}
} // namespace

// =========================================================================================================
// Port callbacks the Snes9x core calls
// =========================================================================================================

bool8 S9xInitUpdate()
{
	return TRUE;
}

bool8 S9xDeinitUpdate(int width, int height)
{
	const fe::Settings& cfg = fe::Config();
	const ps5video::Rect r = ps5video::DrawSnes(GFX.Screen, GFX.Pitch, width, height,
		ps5video::Aspect(cfg.aspect), cfg.scanlines, cfg.shader);
	// keep a copy for the pause menu
	g.last_w = width;
	g.last_h = height;
	g.last.resize(size_t(width) * height);
	for (int y = 0; y < height; y++)
		memcpy(&g.last[size_t(y) * width], reinterpret_cast<const uint8_t*>(GFX.Screen) + size_t(y) * GFX.Pitch,
			size_t(width) * 2);

	const bool wait_vsync = !g.fast_forward && !IsPal();
	ps5video::Present(r.x, r.y, r.w, r.h, wait_vsync);
	return TRUE;
}

bool8 S9xContinueUpdate(int width, int height)
{
	return S9xDeinitUpdate(width, height);
}

void S9xSyncSpeed()
{
	if (Settings.Mute || g.fast_forward)
	{
		S9xClearSamples();
		return;
	}
	const int samples = S9xGetSampleCount(); // int16 values, stereo
	if (samples <= 0)
		return;
	if (int(g.mix.size()) < samples)
		g.mix.resize(samples);
	S9xMixSamples(reinterpret_cast<uint8*>(g.mix.data()), samples);
	ps5audio::Push(g.mix.data(), samples / 2);
	S9xUpdateDynamicRate(ps5audio::Free(), ps5audio::kCapacity);
}

void S9xInitInputDevices()
{
}

bool8 S9xOpenSoundDevice()
{
	return TRUE;
}

void S9xToggleSoundChannel(int)
{
}

void S9xMessage(int type, int number, const char* message)
{
	(void)number;
	if (!message || !*message)
		return;
	OrbisLog("[s9x%s] %s", type == S9X_ERROR ? " error" : (type == S9X_WARNING ? " warning" : ""), message);
}

const char* S9xStringInput(const char*)
{
	return nullptr;
}

std::string S9xGetDirectory(enum s9x_getdirtype dirtype)
{
	switch (dirtype)
	{
		case ROMFILENAME_DIR:
		case ROM_DIR:
		{
			const size_t slash = g.rom_path.find_last_of('/');
			if (slash != std::string::npos)
				return g.rom_path.substr(0, slash);
			return OrbisDir("roms");
		}
		case SRAM_DIR:
		case SAT_DIR: return OrbisDir("saves");
		case SNAPSHOT_DIR: return OrbisDir("states");
		case SCREENSHOT_DIR:
		case SPC_DIR: return OrbisDir("screenshots");
		case CHEAT_DIR: return OrbisDir("cheats");
		case PATCH_DIR: return OrbisDir("patches");
		case BIOS_DIR: return OrbisDir("bios");
		case LOG_DIR: return OrbisDir("logs");
		default: return OrbisRoot();
	}
}

std::string S9xGetFilenameInc(std::string ext, enum s9x_getdirtype dirtype)
{
	const std::string dir = S9xGetDirectory(dirtype);
	const std::string base = S9xBasenameNoExt(Memory.ROMFilename);
	char name[64];
	for (int i = 0; i < 1000; i++)
	{
		snprintf(name, sizeof(name), ".%03d", i);
		const std::string path = dir + "/" + base + name + ext;
		if (!OrbisIsFile(path))
			return path;
	}
	return dir + "/" + base + ".999" + ext;
}

bool8 S9xOpenSnapshotFile(const char* filepath, bool8 read_only, STREAM* file)
{
	*file = OPEN_STREAM(filepath, read_only ? "rb" : "wb");
	return *file != nullptr;
}

void S9xCloseSnapshotFile(STREAM file)
{
	CLOSE_STREAM(file);
}

void S9xAutoSaveSRAM()
{
	const std::string path = S9xGetFilename(".srm", SRAM_DIR);
	const bool ok = Memory.SaveSRAM(path.c_str());
	OrbisLog("[emu] battery save -> %s (%s)", path.c_str(), ok ? "ok" : "failed");
}

void S9xExit()
{
	g.quit = true;
}

void S9xParsePortConfig(ConfigFile&, int)
{
}

void S9xExtraUsage()
{
}

void S9xParseArg(char**, int&, int)
{
}

void S9xHandlePortCommand(s9xcommand_t, int16, int16)
{
}

bool S9xPollButton(uint32, bool*)
{
	return false;
}

bool S9xPollAxis(uint32, int16*)
{
	return false;
}

bool S9xPollPointer(uint32, int16*, int16*)
{
	return false;
}

// =========================================================================================================
// emu::
// =========================================================================================================
namespace emu
{
bool InitCore(int argc, char** argv)
{
	memset(&Settings, 0, sizeof(Settings));
	// Defaults, then /data/snes9x/snes9x.conf if the user wrote one (Snes9x's own format and keys).
	S9xLoadConfigFiles(argv, argc);

	// What the port needs whatever the config file says.
	Settings.SoundPlaybackRate = ps5audio::kRate;
	Settings.SoundInputRate = 32040;
	Settings.SixteenBitSound = TRUE;
	Settings.Stereo = TRUE;
	Settings.DynamicRateControl = TRUE;
	if (Settings.DynamicRateLimit <= 0)
		Settings.DynamicRateLimit = 5;
	Settings.SoundSync = FALSE;
	Settings.FrameTimeNTSC = 16639;
	Settings.FrameTimePAL = 20000;
	Settings.OneClockCycle = 6;
	Settings.OneSlowClockCycle = 8;
	Settings.TwoClockCycles = 12;
	Settings.AutoDisplayMessages = TRUE;
	if (Settings.InitialInfoStringTimeout == 0)
		Settings.InitialInfoStringTimeout = 120;
	Settings.AutoSaveDelay = 3; // seconds after the game writes its battery RAM
	Settings.DontSaveOopsSnapshot = TRUE;
	Settings.StopEmulation = TRUE;
	CPU.Flags = 0;

	if (!Memory.Init() || !S9xInitAPU())
	{
		OrbisLog("[emu] Memory.Init / S9xInitAPU failed");
		Memory.Deinit();
		S9xDeinitAPU();
		return false;
	}
	S9xInitSound(32);
	S9xSetSoundMute(FALSE);
	S9xSetSamplesAvailableCallback(nullptr, nullptr);
	S9xGraphicsInit();
	S9xInitInputDevices();
	MapButtons();
	SetControllers(false);
	ApplySettings();
	g.core_ok = true;
	OrbisLog("[emu] core ready (Snes9x %s)", VERSION);
	return true;
}

void DeinitCore()
{
	if (!g.core_ok)
		return;
	CloseGame();
	S9xGraphicsDeinit();
	S9xDeinitAPU();
	Memory.Deinit();
	g.core_ok = false;
}

bool LoadGame(const std::string& path)
{
	if (!g.core_ok)
		return false;
	CloseGame();
	g.rom_path = path;
	OrbisLog("[emu] loading %s", path.c_str());
	if (!Memory.LoadROM(path.c_str()))
	{
		OrbisLog("[emu] LoadROM failed");
		g.rom_path.clear();
		return false;
	}
	const std::string srm = S9xGetFilename(".srm", SRAM_DIR);
	Memory.LoadSRAM(srm.c_str());

	S9xDeleteCheats();
	S9xCheatsEnable();
	const std::string cht = S9xGetFilename(".cht", CHEAT_DIR);
	if (OrbisIsFile(cht) && S9xLoadCheatFile(cht))
		OrbisLog("[emu] cheats from %s", cht.c_str());

	g.loaded = true;
	g.frame = 0;
	g.fast_forward = false;
	g.quit = false;
	g.prev_p1 = 0;
	ps5video::InvalidateSnes();
	ApplySettings();
	OrbisLog("[emu] \"%s\" %s, %d fps, %s", Memory.ROMName, Memory.ROMFilename.c_str(), Memory.ROMFramesPerSecond,
		Settings.PAL ? "PAL" : "NTSC");
	return true;
}

void CloseGame()
{
	if (!g.loaded)
		return;
	if (Memory.SRAMSize > 0)
		S9xAutoSaveSRAM();
	if (Settings.ApplyCheats || !Cheat.group.empty())
		S9xSaveCheatFile(S9xGetFilename(".cht", CHEAT_DIR));
	g.loaded = false;
	g.rom_path.clear();
}

bool GameLoaded()
{
	return g.loaded;
}

std::string GameName()
{
	// the official title, as on the shelf: by the ROM's CRC (Snes9x computes it on load), else by file name
	const std::string base = S9xBasenameNoExt(g.rom_path);
	std::string name = g.loaded ? fe::gamedb::ByCrc(Memory.ROMCRC32) : std::string();
	if (name.empty())
		name = fe::gamedb::Exact(base);
	if (name.empty())
		name = fe::gamedb::Loose(base);
	return name.empty() ? base : fe::gamedb::Title(name);
}

void ApplySettings()
{
	const fe::Settings& cfg = fe::Config();
	Settings.Transparency = cfg.transparency;
	Settings.DisplayFrameRate = cfg.show_fps;
	Settings.SuperFXClockMultiplier = cfg.superfx_clock;
	Settings.Mute = !cfg.audio;
	S9xSetSoundMute(Settings.Mute);
	ps5video::InvalidateSnes();
}

void Osd(const char* fmt, ...)
{
	static char text[256]; // S9xSetInfoString keeps the pointer
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(text, sizeof(text), fmt, ap);
	va_end(ap);
	S9xSetInfoString(text);
	OrbisLog("[osd] %s", text);
}

bool SaveState(int slot)
{
	if (!g.loaded)
		return false;
	const std::string path = StatePath(slot);
	const bool ok = S9xFreezeGame(path.c_str());
	OrbisLog("[emu] save state %d -> %s: %s", slot, path.c_str(), ok ? "ok" : "failed");
	return ok;
}

bool LoadState(int slot)
{
	if (!g.loaded)
		return false;
	const std::string path = StatePath(slot);
	if (!OrbisIsFile(path))
		return false;
	const bool ok = S9xUnfreezeGame(path.c_str());
	OrbisLog("[emu] load state %d <- %s: %s", slot, path.c_str(), ok ? "ok" : "failed");
	return ok;
}

bool StateExists(int slot)
{
	return g.loaded && OrbisIsFile(StatePath(slot));
}

void Reset()
{
	if (g.loaded)
		S9xSoftReset();
}

void RedrawLastFrame()
{
	if (g.last.empty())
		return;
	const fe::Settings& cfg = fe::Config();
	ps5video::InvalidateSnes();
	ps5video::DrawSnes(g.last.data(), g.last_w * 2, g.last_w, g.last_h, ps5video::Aspect(cfg.aspect), cfg.scanlines,
		cfg.shader);
}

FrameResult RunFrame()
{
	if (!g.loaded)
		return FrameResult::Quit;

	ps5input::Poll();
	const ps5input::PadState& p1 = ps5input::Pad(0);
	const uint32_t raw = p1.buttons;
	const uint32_t pressed = raw & ~g.prev_p1;
	g.prev_p1 = raw;

	// L3 + R3: pause menu
	const uint32_t menu_combo = SCE_PAD_BUTTON_L3 | SCE_PAD_BUTTON_R3;
	if ((raw & menu_combo) == menu_combo && (pressed & menu_combo))
		return FrameResult::OpenMenu;

	// L2 + D-pad: quick save / load / slot (the D-pad doesn't reach the game while L2 is held)
	fe::Settings& cfg = fe::Config();
	const bool l2 = (p1.raw_buttons & SCE_PAD_BUTTON_L2) != 0;
	if (l2)
	{
		if (pressed & SCE_PAD_BUTTON_UP)
			Osd(SaveState(cfg.state_slot) ? "State saved to slot %d" : "Could not save slot %d", cfg.state_slot);
		else if (pressed & SCE_PAD_BUTTON_DOWN)
			Osd(LoadState(cfg.state_slot) ? "State loaded from slot %d" : "Slot %d is empty", cfg.state_slot);
		else if (pressed & (SCE_PAD_BUTTON_LEFT | SCE_PAD_BUTTON_RIGHT))
		{
			cfg.state_slot = (cfg.state_slot + ((pressed & SCE_PAD_BUTTON_RIGHT) ? 1 : 9)) % 10;
			Osd("State slot: %d%s", cfg.state_slot, StateExists(cfg.state_slot) ? " (used)" : " (empty)");
		}
	}

	// R2 held: fast forward
	g.fast_forward = (p1.raw_buttons & SCE_PAD_BUTTON_R2) != 0;

	// Multitap only when a third controller shows up.
	const bool want_tap = ps5input::Pad(2).connected || ps5input::Pad(3).connected;
	if (want_tap != g.multitap)
		SetControllers(want_tap);

	for (int p = 0; p < ps5input::kMaxPads; p++)
	{
		const ps5input::PadState& ps = ps5input::Pad(p);
		uint32_t b = ps.connected ? ps.buttons : 0;
		if (p == 0 && l2)
			b &= ~uint32_t(SCE_PAD_BUTTON_UP | SCE_PAD_BUTTON_DOWN | SCE_PAD_BUTTON_LEFT | SCE_PAD_BUTTON_RIGHT);
		for (int i = 0; i < B_COUNT; i++)
			S9xReportButton(MakeId(p, i), (b & kPadFor[i]) != 0);
	}

	IPPU.RenderThisFrame = !g.fast_forward || (g.frame % 4) == 0;
	S9xMainLoop();
	g.frame++;
	if (g.frame == 60 || g.frame == 240 || g.frame % 3600 == 0)
		OrbisLog("[emu] frame %u: audio queued %d, underruns %llu, shader %.1f ms", g.frame, ps5audio::Queued(),
			(unsigned long long)ps5audio::Underruns(), ps5video::TakeShaderMs());

	if (!g.fast_forward && !Settings.Mute)
	{
		if (IsPal())
			WaitAudioBelow(ps5audio::kCapacity / 2);
		else if (ps5audio::Queued() > ps5audio::kCapacity * 3 / 4)
			WaitAudioBelow(ps5audio::kCapacity / 2);
	}
	else if (!g.fast_forward && IsPal())
	{
		// muted PAL game: pace on the clock instead
		static double next = 0;
		const double now = Now();
		if (next < now - 0.1)
			next = now;
		next += 0.02;
		if (next > now)
			usleep(useconds_t((next - now) * 1e6));
	}

	if (g.quit)
		return FrameResult::Quit;
	return FrameResult::Continue;
}
} // namespace emu
