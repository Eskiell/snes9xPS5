// Snes9x PS5: the picture, through libSceVideoOut (no GPU driver needed).
//
// The frontend draws into a linear 1920x1080 A8B8G8R8 surface (0xAABBGGRR in memory order R,G,B,A).
// Present() swizzles the changed part into one of two tiled scan-out buffers and flips it.
//
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace ps5video
{
constexpr int kWidth = 1920;
constexpr int kHeight = 1080;

bool Init();
void Shutdown();

uint32_t* Surface(); // kWidth * kHeight pixels, pitch kWidth

// Copies the rectangle (x, y, w, h) of the surface (everything when w or h is 0) to the back buffer and
// flips it. wait_vsync: return once the flip happened (the frame pacing for 60 Hz games); otherwise the
// next Present waits for it instead, before it touches that buffer again.
void Present(int x, int y, int w, int h, bool wait_vsync);

inline uint32_t Rgb(uint8_t r, uint8_t g, uint8_t b)
{
	return 0xff000000u | (uint32_t(b) << 16) | (uint32_t(g) << 8) | r;
}

// Fills / darkens part of the surface (clipped).
void FillRect(int x, int y, int w, int h, uint32_t color);
void DarkenRect(int x, int y, int w, int h); // halves every channel

// The SNES picture: how GFX.Screen (RGB565) is scaled into the surface.
enum class Aspect : int
{
	Ratio4x3 = 0, // 1440x1080, what a CRT showed
	PixelPerfect, // 8:7 square pixels at full height
	Integer, // largest whole multiple that fits (4x: 1024x896)
	Stretch, // 1920x1080
	Count
};
const char* AspectName(Aspect a);

// Scales a w x h RGB565 picture (pitch in bytes) into the surface (nearest neighbour); returns the
// rectangle that changed so the caller can Present just that. 'scanlines' darkens the last output row of
// every source line, CRT style, when the scale is 3x or more.
struct Rect
{
	int x, y, w, h;
};
Rect DrawSnes(const uint16_t* src, int pitch_bytes, int w, int h, Aspect aspect, bool scanlines);
// The last rectangle DrawSnes covered (to darken it for the menu, or to clear around it).
Rect LastSnesRect();
// Forget the cached geometry: the next DrawSnes clears the borders.
void InvalidateSnes();
} // namespace ps5video
