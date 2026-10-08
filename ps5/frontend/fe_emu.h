// Snes9x PS5 frontend: the Snes9x core behind a small API, and the S9x* port callbacks.
// SPDX-License-Identifier: MIT
#pragma once

#include <string>

namespace emu
{
bool InitCore(int argc, char** argv);
void DeinitCore();

bool LoadGame(const std::string& path);
void CloseGame(); // writes the battery save
bool GameLoaded();
std::string GameName(); // the file name without its extension

// Settings from fe::Config() -> Snes9x (aspect, scanlines, FPS counter, sound, ...).
void ApplySettings();

enum class FrameResult
{
	Continue,
	OpenMenu, // L3 + R3 pressed together
	Quit, // the core asked to exit (S9xExit)
};
// Polls the pads, runs one SNES frame, shows it and paces to the console's audio/video clock.
FrameResult RunFrame();

bool SaveState(int slot);
bool LoadState(int slot);
bool StateExists(int slot);
void Reset();
void Osd(const char* fmt, ...) __attribute__((format(printf, 1, 2))); // message drawn in the picture

// Redraws the last frame into the surface (the pause menu draws over it).
void RedrawLastFrame();
// The pause menu closed: the buttons still held (Cross, Circle, L3 + R3) don't reach the game until let go.
void AfterMenu();
} // namespace emu
