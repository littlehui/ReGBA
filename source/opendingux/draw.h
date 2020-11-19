#ifndef __DRAW_H__
#define __DRAW_H__

#include "port.h"

/* Hack to enable SDL_SWIZZLEBGR support when
 * using official (old) GCW0 toolchain */
#ifdef GCW_ZERO
#ifndef SDL_SWIZZLEBGR
#define SDL_SWIZZLEBGR	0x00000040
#endif
#endif

extern SDL_Surface* GBAScreenSurface;
extern SDL_Surface* OutputSurface;

extern uint_fast8_t AudioFrameskip;
extern uint_fast8_t AudioFrameskipControl;
extern uint_fast8_t UserFrameskipControl;

// Incremented by the video thread when it decides to skip a frame due to
// fast forwarding.
extern volatile unsigned int VideoFastForwarded;

// How many back buffers have had ApplyScaleMode called upon them?
// At 3, even a triple-buffered surface has had ApplyScaleMode called on
// all back buffers, so stopping remanence with ApplyScaleMode is wasteful.
extern uint_fast8_t FramesBordered;

typedef enum
{
  post_process_type_none = 0,
  post_process_type_cc,
  post_process_type_mix,
  post_process_type_cc_mix
} video_post_process_type;

typedef enum
{
  scaled_aspect = 0,
  fullscreen,
  scaled_aspect_bilinear,
  fullscreen_bilinear,
  scaled_aspect_subpixel,
  fullscreen_subpixel,
  unscaled,
  hardware,
  hardware_2x,
  hardware_2x_scanline_vert,
  hardware_2x_scanline_horz,
  hardware_2x_scanline_grid,
  hardware_scale2x
} video_scale_type;

enum HorizontalAlignment {
  LEFT = 0,
  CENTER,
  RIGHT
};

enum VerticalAlignment {
  TOP = 0,
  MIDDLE,
  BOTTOM
};

extern video_scale_type PerGameScaleMode;
extern video_scale_type ScaleMode;

extern uint32_t PerGameColorCorrection;
extern uint32_t ColorCorrection;

extern uint32_t PerGameInterframeBlending;
extern uint32_t InterframeBlending;

void init_video();
extern bool ApplyBorder(const char* Filename);

extern void ApplyScaleMode(video_scale_type NewMode);

/*
 * Tells the drawing subsystem that it should start calling ApplyScaleMode
 * again to stop image remanence. This is called by the menu and progress
 * functions to indicate that their screens may have created artifacts around
 * the GBA screen.
 */
extern void ScaleModeUnapplied();

extern void gba_render_half(uint16_t* Dest, uint16_t* Src, uint32_t DestX, uint32_t DestY,
	uint32_t SrcPitch, uint32_t DestPitch);

void PrintString(const char* String, uint16_t TextColor,
	void* Dest, uint32_t DestPitch, uint32_t X, uint32_t Y, uint32_t Width, uint32_t Height,
	enum HorizontalAlignment HorizontalAlignment, enum VerticalAlignment VerticalAlignment);

void PrintStringOutline(const char* String, uint16_t TextColor, uint16_t OutlineColor,
	void* Dest, uint32_t DestPitch, uint32_t X, uint32_t Y, uint32_t Width, uint32_t Height,
	enum HorizontalAlignment HorizontalAlignment, enum VerticalAlignment VerticalAlignment);

extern uint32_t GetRenderedWidth(const char* str);

extern uint32_t GetRenderedHeight(const char* str);

void ReGBA_VideoFlip();

void SetMenuResolution();

void SetGameResolution();

/*
 * Returns a new allocation containing a copy of the GBA screen. Its pixels
 * and lines are packed (the pitch is 480 bytes), and its pixel format is
 * XBGR 1555.
 * Output assertions:
 *   The returned pointer is non-NULL. If it is NULL, then this is a fatal
 *   error.
 */
extern uint16_t* copy_screen();

#ifdef SDL_SWIZZLEBGR
#define RGB888_TO_NATIVE(r, g, b) ( \
  (((b) & 0xF8) << 7) | \
  (((g) & 0xF8) << 2) | \
  (((r) & 0xF8) >> 3) \
  )
#else
#define RGB888_TO_NATIVE(r, g, b) ( \
  (((r) & 0xF8) << 8) | \
  (((g) & 0xFC) << 3) | \
  (((b) & 0xF8) >> 3) \
  )
#endif

#endif /* __DRAW_H__ */
