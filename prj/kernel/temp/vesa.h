#ifndef __VESA_H__
#define __VESA_H__

#include <types.h>

#define VESA_MAX_OEM_STR_LEN 255

struct x86_regs {
	uint16_t ax, bx, cx, dx, di, si, sp, ip;
	uint16_t ds, es, ss, fs, gs, cs;
};

enum vesaMemoryModels {
	VESA_TEXT = 0,
	VESA_PLANAR = 3,
	VESA_PACKED = 4, // only for 8bpp: palette
	VESA_DIRECT_COLOR = 6, // seek _mask and _position for color info
};

struct vesaModeInfo {
	uint16_t mode_no;
	uint16_t Xres, Yres;
	uint16_t pitch; // bytes per scanline

	uint8_t memory_model;
	uint8_t bpp;

	uint8_t red_mask, red_position;
	uint8_t green_mask, green_position;
	uint8_t blue_mask, blue_position;
	uint32_t physbase;  // frame buffer address
};

struct vesaControllerInfo {
	short version;
	char oemString[VESA_MAX_OEM_STR_LEN];
	uint8_t caps[4];
	int modeCount;
	short totalMem;

	short oemRev;
	char oemVendor[VESA_MAX_OEM_STR_LEN];
	char oemProduct[VESA_MAX_OEM_STR_LEN];
	char oemProductRev[VESA_MAX_OEM_STR_LEN];
};

// Better not use this! Unless you are sure....
// mode would be filled into regs in some calls....
int vesa_call(struct x86_regs *regs, uint8_t call, uint16_t mode);

// Modes related functions
// get_modes: 
//		@in modes: modes buffer	@in size: buf size
//		@out modes: modes		@out size: filled in modes
//		better call get_controller_info first to get modeCount
// get_mode_info:
//		@in mode: mode to know @in info: ptr to mode info
//		@out info: filled mode info
int vesa_set_mode(int mode);
int vesa_get_mode(int *mode);
int vesa_get_modes(int* modes, int* size);
int vesa_get_mode_info(int mode, struct vesaModeInfo *info);
int vesa_get_mode_info_byid(int modeid, struct vesaModeInfo *info);

// Save/restore
// get_state_size:
//		@out size: state size in bytes
// save_state:
//		@in size: buf size
//		@out buf: saved state
// restore_state:
//		@in buf: saved state	@in size: buf size
int vesa_get_state_size(int *size);
int vesa_save_state(char *buf, int size);
int vesa_restore_state(char *buf, int size);

// Palette
// set_palette:
//		@in buf: palette, format reserved/r/g/b
//		@in size: number of colors to set, so sizeof(*buf) == 4*size
//		@in start: start num. of color to set
// get_palette:
//		@out buf: palette, format reserved/r/g/b
//		@in size: number of colors to get, so sizeof(*buf) == 4*size
//		@in start: start num. of color to get
int vesa_get_palette_width(int *width);
int vesa_set_palette_width(int *width);
int vesa_set_palette(char *buf, int size, int start);
int vesa_get_palette(char *buf, int size, int start);

int vesa_get_controller_info(struct vesaControllerInfo *info);

int vesa_draw_point(int x, int y, uint8_t r, uint8_t g, uint8_t b);

int vesa_init(void);

#endif
