// Snes9x PS5 frontend: settings file.
// SPDX-License-Identifier: MIT

#include "fe_settings.h"

#include "OrbisPaths.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace fe
{
namespace
{
std::string IniPath()
{
	return OrbisRoot() + "/snes9x-ps5.ini";
}

int Clamp(int v, int lo, int hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}
} // namespace

Settings& Config()
{
	static Settings s;
	return s;
}

void Settings::Load()
{
	FILE* f = fopen(IniPath().c_str(), "r");
	if (!f)
		return;
	char line[1024];
	while (fgets(line, sizeof(line), f))
	{
		line[strcspn(line, "\r\n")] = 0;
		char* eq = strchr(line, '=');
		if (!eq || line[0] == '#' || line[0] == ';')
			continue;
		*eq = 0;
		const std::string key = line;
		const char* val = eq + 1;
		if (key == "aspect")
			aspect = Clamp(atoi(val), 0, 3);
		else if (key == "scanlines")
			scanlines = atoi(val) != 0;
		else if (key == "show_fps")
			show_fps = atoi(val) != 0;
		else if (key == "audio")
			audio = atoi(val) != 0;
		else if (key == "state_slot")
			state_slot = Clamp(atoi(val), 0, 9);
		else if (key == "transparency")
			transparency = atoi(val) != 0;
		else if (key == "superfx_clock")
			superfx_clock = Clamp(atoi(val), 50, 400);
		else if (key == "covers_download")
			covers_download = atoi(val) != 0;
		else if (key == "last_dir")
			last_dir = val;
		else if (key == "last_rom")
			last_rom = val;
	}
	fclose(f);
	OrbisLog("[settings] loaded %s", IniPath().c_str());
}

void Settings::Save() const
{
	const std::string tmp = IniPath() + ".tmp";
	FILE* f = fopen(tmp.c_str(), "w");
	if (!f)
	{
		OrbisLog("[settings] can't write %s", tmp.c_str());
		return;
	}
	fprintf(f, "# Snes9x PS5\n");
	fprintf(f, "aspect=%d\n", aspect);
	fprintf(f, "scanlines=%d\n", scanlines ? 1 : 0);
	fprintf(f, "show_fps=%d\n", show_fps ? 1 : 0);
	fprintf(f, "audio=%d\n", audio ? 1 : 0);
	fprintf(f, "state_slot=%d\n", state_slot);
	fprintf(f, "transparency=%d\n", transparency ? 1 : 0);
	fprintf(f, "superfx_clock=%d\n", superfx_clock);
	fprintf(f, "covers_download=%d\n", covers_download ? 1 : 0);
	fprintf(f, "last_dir=%s\n", last_dir.c_str());
	fprintf(f, "last_rom=%s\n", last_rom.c_str());
	fclose(f);
	rename(tmp.c_str(), IniPath().c_str());
}
} // namespace fe
