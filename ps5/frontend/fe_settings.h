// Snes9x PS5 frontend: settings kept in /data/snes9x/snes9x-ps5.ini (key=value lines).
// SPDX-License-Identifier: MIT
#pragma once

#include <string>

namespace fe
{
struct Settings
{
	int aspect = 0; // ps5video::Aspect
	bool scanlines = false;
	bool show_fps = false;
	bool audio = true;
	int state_slot = 0; // 0..9
	bool transparency = true; // Snes9x "Transparency"
	int superfx_clock = 100; // % (Snes9x SuperFXClockMultiplier)
	bool covers_download = true; // fetch box art from libretro-thumbnails
	std::string last_dir; // the browser reopens here
	std::string last_rom; // and puts the cursor on this file

	void Load();
	void Save() const;
};

Settings& Config();
} // namespace fe
