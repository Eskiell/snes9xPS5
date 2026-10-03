# Snes9x PS5

Snes9x 1.63, the Super Nintendo emulator, running on a jailbroken PS5 as a **native home-screen app** with its
own icon and background. The PS5 layer follows the layout of PS5SX2 (the PCSX2 port):

- `ps5/coreorbis` holds `main-boot.cpp`, the `orbis-shims/` and `include-orbis/`;
- `ps5/frontend` holds the user interface and the 3D game shelf;
- `ps5/proto/native` builds and signs `eboot.bin`;
- `ps5/installer` is the installer and helper payload.

Everything outside `ps5/` is the original Snes9x source, unchanged.

> **Status (2.0):** runs on the console: the app opens from its icon, the shelf, the controller, video and sound
> work, and games play. It builds with the ps5-payload-dev SDK and passes 88 host tests, which run the same code
> on Linux with the PS5 calls simulated. If something fails, the logs in `/data/snes9x/logs/` say where.

## How it works (the PS5SX2 model)

The PS5 gives the controller only to the app in front. Versions 1.0 to 1.5 ran Snes9x as a payload next to the
system UI, so the controller never worked. Since 1.6 Snes9x follows PS5SX2's model:

| Piece | What it is | In PS5SX2 |
|---|---|---|
| `eboot.bin` in `/data/homebrew/PPSA99009/` | **Snes9x itself**, a native app opened from its icon | PCSX2 (PPSA99203) |
| `sce_module/libc.prx` | the C runtime module every native app carries | the same file, byte for byte |
| `Snes9xPS5.elf` (payload) | the **installer**: installs or updates the app, then stays running as the **helper** | PS5SX2Installer.elf + PS5SX2Helper.elf |

When it opens, the app asks the helper to let it out of its sandbox; without that an app sees neither `/data`
nor USB drives. The request is the one PS5SX2 makes:

- **Who is asked, in order:** the Snes9x helper (127.0.0.1:9075), then etaHEN (9028) and the daemon on port 9069.
- **If nobody answers:** the app carries a copy of the helper (`Snes9xPS5-helper.elf`), sends it to the ELF
  loader (127.0.0.1:9021) and asks again. So the icon keeps working after a reboot, as long as the ELF loader
  runs.
- **What the helper allows:** only title PPSA99009. It gives the process the system's root folder and uid 0, as
  elfldr does for payloads. No other app is touched.

Snes9x only uses `/data/snes9x/`, `/data/homebrew/PPSA99009/` and its own `/user/appmeta/PPSA99009/`. Nothing is
written to PS5SX2's folders (`/data/PCSX2`, `/data/homebrew/PPSA99203`).

## Versions

Every release carries its version in the file name: `Snes9xPS5-v2.0.elf` and `snes9x-ps5-v2.0-src.zip`
(`make dist`). When updating, replace the old ELF with the new one in your autoload or Payload Manager. In this
README, "`Snes9xPS5.elf`" always means the current release's ELF.

## Language

Every screen and notification of Snes9x PS5 is in English.

## Requirements

- A jailbroken PS5 with **kstuff** (or kstuff-lite) and an **ELF loader** on port 9021: elfldr, etaHEN, or the
  one PS5 Payload Manager uses.
- **ShadowMountPlus**, so the icon appears on the home screen (the same one PS5SX2 uses).
- Your own games (SNES ROMs). No games are included.

## Install and play

1. **Send `Snes9xPS5.elf`** with PS5 Payload Manager, or from a PC on the same network:
   ```sh
   nc -q0 PS5_IP 9021 < Snes9xPS5-v2.0.elf
   ```
   It installs the app in `/data/homebrew/PPSA99009/` (`eboot.bin`, `sce_module/libc.prx`, `param.json`, the
   icon and the backgrounds), shows **"Snes9x PS5 2.0 installed. Open it from the Snes9x PS5 icon on the home
   screen."** and stays running as the helper.
2. **Open the Snes9x PS5 icon.** The game shelf appears and the controller works.
3. **Copy your ROMs** (`.sfc .smc .swc .fig .bs .st .zip .gz`) to `/data/snes9x/roms`, over FTP for example, or
   to a `snes9x/roms` folder on a USB drive. Subfolders work.

Tip: put `Snes9xPS5.elf` in your autoload, as PS5SX2 recommends for its payloads.

**Updating:** send the new `Snes9xPS5.elf` once. It compares every app file with the copy it carries and rewrites
only what changed; each file is written to a temporary file and then renamed, `eboot.bin` last. The notification
says "Snes9x PS5 updated to 2.0". A deleted or damaged icon is put back the same way. A second copy sent while
the helper is already running only installs and exits.

**If "Snes9x PS5 has no access to /data" appears:** no helper answered and the ELF loader wasn't running. Send
`Snes9xPS5.elf` and open the icon again.

| Folder | Contents |
|---|---|
| `/data/snes9x/roms` | your games |
| `/data/snes9x/saves` | battery saves (`.srm`), written 3 s after the game saves and on exit |
| `/data/snes9x/states` | save states (`.000` to `.009`) |
| `/data/snes9x/cheats` | cheats (`.cht`, Snes9x format), loaded with the game |
| `/data/snes9x/patches` | IPS/UPS/BPS patches named after the ROM |
| `/data/snes9x/bios` | `BS-X.bin`, `STBIOS.bin` (Satellaview, Sufami Turbo) |
| `/data/snes9x/covers` | downloaded covers and your own |
| `/data/snes9x/logs` | `boot.log` (the app), `installer.log` (installer/helper), `helper.log` (the helper the app starts), and the previous session's `.prev.log` files |
| `/data/snes9x/snes9x-ps5.ini` | the menu settings |
| `/data/snes9x/snes9x.conf` | optional: Snes9x's own configuration file, for advanced options |

### The home-screen app

```
/data/homebrew/PPSA99009/
  eboot.bin              Snes9x (a native app, signed as PS5SX2's is), with the helper inside
  sce_module/libc.prx    the native app's C runtime (the same as PS5SX2's and ps5-native-app-boilerplate's)
  sce_sys/param.json     title "Snes9x PS5", ID PPSA99009
  sce_sys/icon0.png      the icon (512x512; ps5/app/sce_sys/icon0.png in the source)
  sce_sys/pic0.dds       the home-screen background while the icon is selected (3840x2160, BC7)
  sce_sys/pic1.dds       the launch background (the same image)
```

To change the icon, replace `ps5/app/sce_sys/icon0.png` (512x512 PNG) and rebuild. The background comes from
`ps5/app/sce_sys/background-source.png`, converted to `pic0.dds`/`pic1.dds` with ps5-native-app-boilerplate's
`tools/prepare-assets.sh --background`. For another title ID: `make ps5 TITLE_ID=XXXX00000`.

**Home-screen art:** ShadowMountPlus copies the art in `sce_sys` (icon, backgrounds, `param.json`) to
`/user/appmeta/PPSA99009/` only when it first registers the title, and the system keeps the art it saw at that
moment. The installer keeps that folder current on every update, but if the title was registered before the art
changed, register it again once:

1. On the home screen, select **Snes9x PS5**, press **OPTIONS** and choose **Delete** (your games, saves, states
   and covers live in `/data/snes9x/` and are not removed).
2. Send `Snes9xPS5.elf` again; it reinstalls the app folder if it is gone.
3. Wait for ShadowMountPlus to register the title; the icon comes back with the new art.

## The game shelf and covers

The start screen is a 3D shelf of game covers, like PS5SX2's. In the top-left corner, under the wordmark, is the
author's line with the GitHub mark: **github.com/MisterTemaki** (PS5SX2 shows its author's handles there).

- the selected cover sits in the middle, with a glow in the cover's own colour over the game's blurred art;
- the others are tilted on both sides, in perspective, reflected on the floor;
- the shelf slides when you change games.

- **All your games at once:** the shelf gathers the ROMs in `/data/snes9x/roms` and on USB drives
  (`snes9x/roms`), subfolders included, sorted by game name.
- **Official names:** SNES ROMs have no serial. A game is identified by its file name or, when that doesn't
  match, by the ROM's CRC32, looked up in a No-Intro table of 4268 games built into the app.
  - The CRC is taken without a copier header; for a `.zip`, the CRC stored in the zip is used.
  - Loose names (`super mario world.smc`) are recognised too.
  - CRCs are cached in `covers/crc-cache.txt`, so each ROM is read once.
- **Automatic covers, the PS5SX2 way:** covers come from
  [libretro-thumbnails](https://github.com/libretro-thumbnails/Nintendo_-_Super_Nintendo_Entertainment_System)
  (`Named_Boxarts`) over HTTPS, with the console's own `libSceHttp2`/`libSceSsl`, in a **prefetch** step right as
  the app opens, before it asks for `/data` (30 s budget), exactly as PS5SX2 does. After that the shelf downloads
  nothing (on the console HTTPS fails at that stage; the 1.6.1 logs showed it).
  - Every start writes the missing covers to `/data/snes9x/covers/wanted.txt`; the helper hands that list to the
    app at the next start, and the prefetch fetches them.
  - **New games:** when the app finds covers it hasn't tried yet, it shows "Downloading covers..." and
    **restarts itself** (as PS5SX2 re-executes its own eboot); the covers arrive on that start.
  - Covers are kept in `/data/snes9x/covers/` and never downloaded twice. A cover the server doesn't have is
    marked (`.missing`) and only looked for again after 30 days; **Square** forces a new try (the app restarts
    to fetch it).
  - Without a network nothing is tried and the shelf works the same.
- **Your own covers:** a `.png` or `.jpg` named after the ROM file, in `/data/snes9x/covers/` or next to the ROM,
  takes priority over downloads.
- **No cover:** the game gets a card with its title.
- **Turning downloads off:** Settings, "Download covers".

**How it is drawn:** PS5SX2 draws its shelf on the GPU with the Vulkan driver of a private ps5vk fork. Without
that driver Snes9x PS5 draws the shelf on the CPU, still in real 3D perspective:

- each cover is a quad turned about the vertical axis, drawn column by column;
- bilinear filtering, three mipmap levels and anti-aliased edges;
- the work is split across several cores.

On the test PC a frame takes about 15 ms on 2 cores.

## Controls

**On the shelf**

| Button | Does |
|---|---|
| Left / Right (D-pad or stick) | change game (hold to speed up) |
| L1 / R1 | skip 10 games |
| Cross | play |
| Triangle | settings |
| Square | download this game's cover again |
| OPTIONS | quit Snes9x (asks first) |

**In a game** (buttons by position, as on the SNES pad)

| PS5 | SNES |
|---|---|
| Cross / Circle / Square / Triangle | B / A / Y / X |
| L1 / R1 | L / R |
| OPTIONS | Start |
| touchpad click | Select |
| D-pad or left stick | D-pad |

| Combination | Does |
|---|---|
| L3 + R3 | pause menu (save/load state, slot, aspect ratio, scanlines, FPS, sound, reset, back to the list, quit) |
| L2 + Up / Down | save / load the state in the current slot |
| L2 + Left / Right | change slot (0–9) |
| hold R2 | fast forward |

Up to 4 controllers: players 2 to 4 are the other signed-in users. With 3 or 4 controllers the multitap is turned
on in port 2. The light bar shows the player (blue, red, green, pink).

## Picture and sound

- 1920x1080 output through `libSceVideoOut`, flipping on vsync. With little video memory it falls back to
  1280x720.
- Aspect ratio: **4:3** (default), **8:7** (square pixels), **integer scale** (4x, 1024x896) or **16:9**.
  Optional scanlines.
- Sound through `libSceAudioOut` at 48 kHz, on its own thread.
  - Snes9x's *dynamic rate control* adjusts the sound by up to 0.5% to follow the TV's 60 Hz without crackles.
  - PAL (50 Hz) games follow the audio clock.

## Debugging (logs and crashes)

If something fails, send the files in `/data/snes9x/logs/`: `boot.log` (the app), `installer.log` and
`helper.log`, plus the previous session's `.prev.log` files. They record every step:

- the sandbox request and who answered;
- the cover prefetch, cover by cover;
- every `sceVideoOut*` call;
- the controller handle and its first read;
- the shelf and the cover thread, stage by stage.

Since 1.6.1, if the app dies on a signal (the PS5's "Game or App Error" screen), a `== CRASH ==` block is written
at the end of `boot.log`: the signal, the address, the **stage** each thread was in (`stage[...]`), and the return
addresses. That block points to the exact line of code that failed.

## Known limitations

- The PS and Create buttons are not reported by `scePadReadState`, so Select is on the touchpad.
- No netplay, rewind, or Snes9x video filters (hq2x, NTSC…) yet.

## Building

Requirements: the [ps5-payload-dev SDK](https://github.com/ps5-payload-dev/sdk) v0.42 or newer, clang/lld 18, and
g++ with ASan for the tests. zlib 1.3.1 is vendored in `third_party/zlib`.

```sh
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
cd ps5
make ps5 -j$(nproc)              # build/ps5/Snes9xPS5.elf (installer + helper, with the app inside)
make send PS5_HOST=192.168.0.10  # sends it to elfldr (port 9021)
make dist                        # build/dist/Snes9xPS5-v<version>.elf + the source zip
make app                         # only build/app/PPSA99009/, to copy by hand
make test                        # Linux builds (app, installer, helper) + 88 tests (ASan/UBSan)
```

The build has three stages:

1. `Snes9xPS5-helper.elf`, the helper alone.
2. `eboot.bin`: a PIE with the boilerplate's `app_crt.cpp` and the emulator's objects.
   - It is linked against the SDK's stubs and static libc++, with `ps5-pie.ld` + `ehframe.ld` and every symbol
     local.
   - Then it goes through `ps5-native-tool link` and `self --sign`, exactly like PS5SX2's `link-vk.sh`.
   - `libc.prx` goes with it, generated by `libc_builder` and checked against the published SHA-256
     (`proto/native/libc.prx.sha256`).
3. `Snes9xPS5.elf`, with the app folder built in.

## Source layout

- **`ps5/coreorbis/main-boot.cpp`**: the app's entry point (`eboot.bin`).
  - Prefetches covers, asks to leave the sandbox, then brings up log, folders, video, sound and pads, in that
    order.
  - Hands over to the frontend on a thread with an 8 MiB stack.
  - Exits through the system (`sceSystemServiceLoadExec("exit")`), as PS5SX2 does.
- **`ps5/installer/installer_main.cpp`**: `Snes9xPS5.elf` (installs the app and stays as the helper) and, built
  with `SNES9X_HELPER_ONLY`, `Snes9xPS5-helper.elf`.
- **`ps5/coreorbis/orbis-shims/`**: the PS5 layer.
  - `ProsperoVideo.cpp`: `libSceVideoOut`, direct memory, two scan-out buffers, AVX2 tiling, scaling of the
    SNES picture;
  - `ProsperoAudio.cpp`: `libSceAudioOut`, lock-free ring buffer, output thread;
  - `ProsperoInput.cpp`: `libScePad` + `libSceUserService`, up to 4 players;
  - `ProsperoJailbreak.cpp` / `ProsperoHelper.cpp`: both sides of the sandbox request and the covers list;
    `helper_data.cpp` builds the helper into the app;
  - `ProsperoInstall.cpp` + `install_data.cpp`: the app folder built into the installer, the install, and the
    `/user/appmeta` art;
  - `ProsperoCrash.cpp`: the crash printer and stage markers;
  - `ProsperoNotify.cpp`: system notifications (the same kernel toast as PS5SX2);
  - `orbis_paths.cpp`: `/data/snes9x`, USB drives and the logs.
- **`ps5/coreorbis/include-orbis/ProsperoSce.h`**: prototypes of the system functions (the SDK ships the import
  stubs but not the headers).
- **`ps5/frontend/`**: the 3D shelf (`fe_shelf.cpp`), covers (`fe_covers.cpp`, `fe_prefetch.cpp`), the library
  and No-Intro names (`fe_games.cpp`, `data/snes-nointro.tsv`), HTTPS (`fe_http.cpp`), text with PS5SX2's fonts
  (`fe_text.cpp`), menus, settings, and `fe_emu.cpp`, which connects the Snes9x core to the PS5 layer.
- **`ps5/proto/native/`**: ps5-native-app-boilerplate's tools (BlackBearReloaded, GPL-3.0), taken from PS5SX2
  and PS5_Vulkan (mihawk-99):
  - `ps5-native-tool`, for linking and signing;
  - `app_crt.cpp`;
  - `ps5-pie.ld`;
  - `libc_builder.cpp` and its manifests.
- **`ps5/app/sce_sys/`**: param.json, icon and backgrounds.
- **`ps5/host/sce_host.cpp`** and **`ps5/tests/`**: the PS5 functions implemented on Linux, and the tests. They
  check the picture, controller, save states, PAL, zip, sound, covers, install, helper and sandbox request.

## License and credits

- **Snes9x**: the Snes9x license (`LICENSE` at the root: personal, non-commercial use, with source).
- **New code in `ps5/`**: MIT.
- **ps5-payload-dev SDK** (John Törnblom): toolchain, CRT, kernel access and import stubs (GPLv3+).
  - The payloads are linked with the SDK's CRT;
  - the app is linked with the SDK's static libc++.
- **ps5-native-app-boilerplate** (BlackBearReloaded, GPL-3.0-or-later): `ps5-native-tool`, `app_crt.cpp`,
  `app_cpp_runtime.cpp`, `ps5-pie.ld` and the `libc.prx` generator, via PS5SX2 and PS5_Vulkan (mihawk-99).
- **PS5SX2** (Spyros): the PS5 layer's pattern (Prospero* shims, kernel toast, `/data` layout, the sandbox request,
  the cover prefetch and exiting through the system).
- The VideoOut tiling and setup follow the SDK's SDL2 port (zlib license).
- **zlib** (Jean-loup Gailly and Mark Adler): zlib license.
- **stb_image / stb_image_resize2 / stb_truetype** (Sean Barrett): public domain or MIT, the same as PS5SX2.
- **UI fonts**, the same as PS5SX2's (which takes them from PCSX2), in `frontend/assets/fonts/` with their
  licenses:
  - **Roboto Regular** (Google, Apache 2.0);
  - **PromptFont** (Yukari "Shinmera" Hafner, SIL OFL 1.1);
  - **Font Awesome Brands** (Fonticons, Inc.; font SIL OFL 1.1, icons CC BY 4.0), for the GitHub mark on the
    shelf.
- **Covers:** [libretro-thumbnails](https://github.com/libretro-thumbnails), downloaded on the console, not
  included. **Names and CRCs:** [libretro-database](https://github.com/libretro/libretro-database) (No-Intro).
- **Port:** [github.com/MisterTemaki](https://github.com/MisterTemaki).
