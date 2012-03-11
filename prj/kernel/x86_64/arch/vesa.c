#include <string.h>
#include <io.h>

#include <malloc.h>
#include <page.h>
#include <lib/low_io.h>
#include <arch/mmu.h>
#include <arch/memlayout.h>

#include "x86emu/include/x86emu.h"
#include "vesa.h"

x86emu_t *emu;
char *rm_image;
#define RM_IMAGE_PAGES 0x110

typedef uint16_t FarPtr[2];
#define REALPTR(x,y) (rm_image + ((int)x << 4) + y)
#define REALPTR1(x) (rm_image + ((int)(x[1]) << 4) + x[0])
#define REALPTR3(x) (((int)(x[1]) << 4) + x[0])

#define FARPTR(x) uint16_t x[2]
#define MAX_MODES 100

#define VGA_RESET	0x03c6
#define VGA_READ	0x03c7
#define VGA_WRITE	0x03c8
#define VGA_DATA	0x03c9

enum ModeAttrs {
	MODE_ATTR_COLOR = 3,
	MODE_ATTR_GRAPHICS = 4,
};

static int debug = 0;

static int16_t *vesaModes = NULL;
static struct vesaMode *modeInfo = NULL;
static char *fBuf = NULL;
static struct x86_regs regs;
static int curMode = 0;
static struct modeData *modesData = NULL;
static struct vesaControllerInfo *ctlInfo = NULL;

struct vesaInfo {
	char signature[4];             // == "VESA"
	short version;                 // == 0x0300 for VBE 3.0
	FarPtr oemString;            // isa vbeFarPtr
	unsigned char capabilities[4];
	FarPtr videomodes;			 // isa vbeFarPtr
	short totalMemory;             // as # of 64KB blocks

	// VBE2
	short oemRev;
	FarPtr oemVendor;
	FarPtr oemProduct;
	FarPtr oemProductRev;
	char res[222];
	char oemData[256];
};

struct vesaMode {
	uint16_t attributes;
	uint8_t winA,winB;
	uint16_t granularity;
	uint16_t winsize;
	uint16_t segmentA, segmentB;

	short realFctPtr[2]; // vbeFarPtr
	uint16_t pitch; // uint8_ts per scanline

	uint16_t Xres, Yres;
	uint8_t Wchar, Ychar, planes, bpp, banks;
	uint8_t memory_model, bank_size, image_pages;
	uint8_t reserved0;

	uint8_t red_mask, red_position;
	uint8_t green_mask, green_position;
	uint8_t blue_mask, blue_position;
	uint8_t rsv_mask, rsv_position;
	uint8_t directcolor_attributes;

	uint32_t physbase;  // your LFB address ;)
	uint32_t reserved1;
	short reserved2;
};

struct modeData {
	uint8_t red_mask, green_mask, blue_mask;
};

static void flush_log(x86emu_t *emu, char *buf, unsigned size)
{
	if (debug)
	{
		char x = buf[size];
		buf[size] = 0;
		cprintf("%s", buf);
		buf[size] = x;
	}
}

static int init_emu_mem()
{
	rm_image = VADDR_DIRECT(PAGE_TO_PHYS(page_alloc_atomic(RM_IMAGE_PAGES)));
	memmove(rm_image, VADDR_DIRECT(0), RM_IMAGE_PAGES << PGSHIFT);
	 
	emu = x86emu_new(X86EMU_PERM_VALID | X86EMU_PERM_RWX, X86EMU_PERM_RW);
	 
	int i;
	for (i = 0; i != (RM_IMAGE_PAGES << PGSHIFT) >> X86EMU_PAGE_BITS; ++ i)
	{
		uint32_t addr = i << X86EMU_PAGE_BITS;
		/* int j; */
		/* for (j = 0; j != 1 << X86EMU_PAGE_BITS; ++ j) */
		/* { */
		/* 	   x86emu_write_byte_noperm(emu, addr + j, *(rm_image + addr + j)); */
		/* } */
		x86emu_set_page(emu, addr, rm_image + addr);
	}
	x86emu_set_perm(emu, 0, RM_IMAGE_PAGES << PGSHIFT, X86EMU_PERM_VALID | X86EMU_PERM_RWX);

	// initial code, terminate at 0x7f00
	for (i = 0x7c00; i != 0x7f00; ++ i)
	{
		x86emu_write_byte(emu, i, 0x90);
	}
	x86emu_set_perm(emu, 0x7f00, 0x8000, X86EMU_PERM_VALID | X86EMU_PERM_RW);
	return 0;
}

static void
set_regs(struct x86_regs *regs)
{
	emu->x86.R_AX = regs->ax;
	emu->x86.R_BX = regs->bx;
	emu->x86.R_CX = regs->cx;
	emu->x86.R_DX = regs->dx;
	emu->x86.R_DI = regs->di;
	emu->x86.R_SI = regs->si;

	emu->x86.R_IP = regs->ip;
	emu->x86.R_SP = regs->si;

	x86emu_set_seg_register(emu, emu->x86.R_DS_SEL, regs->ds);
	x86emu_set_seg_register(emu, emu->x86.R_ES_SEL, regs->es);
	x86emu_set_seg_register(emu, emu->x86.R_CS_SEL, regs->cs);
	x86emu_set_seg_register(emu, emu->x86.R_SS_SEL, regs->ss);
	x86emu_set_seg_register(emu, emu->x86.R_FS_SEL, regs->fs);
	x86emu_set_seg_register(emu, emu->x86.R_GS_SEL, regs->gs);

}

static void
get_regs(struct x86_regs *regs)
{
	regs->ax = emu->x86.R_AX;
	regs->bx = emu->x86.R_BX;
	regs->cx = emu->x86.R_CX;
	regs->dx = emu->x86.R_DX;
	regs->di = emu->x86.R_DI;
	regs->si = emu->x86.R_SI;

	regs->ip = emu->x86.R_IP;
	regs->si = emu->x86.R_SP;
}

static void
init_regs(struct x86_regs *regs)
{
	memset(regs, 0, sizeof(struct x86_regs));
	regs->ip = 0x7c00;
	regs->sp = 0x7c00;
}

static void
make_intr(struct x86_regs *regs, uint8_t intno)
{
	regs->ip = 0x7c00;
	regs->sp = 0x7c00;
	set_regs(regs);

	x86emu_set_log(emu, 2000000, flush_log);
	if (debug)
	{
		emu->log.trace |= X86EMU_TRACE_CODE;
		emu->log.trace |= X86EMU_TRACE_REGS;
		emu->log.trace |= X86EMU_TRACE_IO;
	}
	x86emu_intr_raise(emu, intno, INTR_TYPE_SOFT, 0);
	x86emu_run(emu, X86EMU_RUN_NO_CODE | X86EMU_RUN_LOOP);
	x86emu_clear_log(emu, 1);

	get_regs(regs);
}

int vesa_call(struct x86_regs *regs, uint8_t call, uint16_t mode)
{
	regs->ax = 0x4F00 | call;
	switch (call)
	{
	case 0x01:
		regs->cx = mode;
		break;
	case 0x02:
		regs->bx = mode;
		break;
	case 0x04:
		regs->cx = mode;
		break;
	case 0x09:
		regs->cx = mode;
		break;
	}
	regs->di = 0x7f00;

	struct vesaInfo *info = (struct vesaInfo *)REALPTR(regs->es, regs->di);
	const char *signature = "VBE2";
	memcpy(info->signature, signature, 4);

	make_intr(regs, 0x10);

	if ((regs->ax & 0xff) != 0x4F)
		return -1;
	else 
		return regs->ax >> 8;
}

static int test_mode_attr(struct vesaMode *mode, int attr)
{
	return (mode->attributes & (1 << attr)) ? 1 : 0;
}

int vesa_set_mode(int mode)
{
	if (ctlInfo == NULL)
		if (vesa_get_controller_info(NULL) != 0)
			return -1;
	
	int i;
	for (i=0; i<ctlInfo->modeCount; i++)
	{
		if (vesaModes[i] == mode)
		{
			if (modeInfo[i].physbase == 0)
			{
				if (debug)
				{
					cprintf("ERROR: mode %x does not support linear framebuffer mode\n", mode);
				}
				return -1;
			}
			// use linear / clear memory
			int ret = vesa_call(&regs, 2, mode | 0x4000);
			if (ret)
			{
				if (debug)
				{
					cprintf("ERROR: set mode to %x failed.\n", mode);
				}
				return -3;
			}
			if (fBuf != NULL)
			{
				fBuf = NULL;
			}

			fBuf = VADDR_DIRECT(modeInfo[i].physbase);
			curMode = i;

			if ((modeInfo[i].bpp == 8) && (modeInfo[i].memory_model == 4))
			{
				unsigned char *buf = kmalloc(sizeof(unsigned char) * 256 * 4);
				int j;
				for (j=0; j<256; j++)
				{
					buf[j * 4] = 0;
					buf[j * 4 + 1] = j & 0xE0;
					buf[j * 4 + 2] = (j & 0x18) << 3;
					buf[j * 4 + 3] = (j & 0x7) << 5;
				}

				if ((ret = vesa_set_palette((char *)buf, 256, 0)))
					if (debug)
						cprintf("Warn: set palette failed: %d\n", ret);

				kfree(buf);
			}
			return 0;
		}
	}
	if (debug)
	{
		cprintf("ERROR: mode %x not supported.\n", mode);
	}
	return -2;
}

int vesa_draw_point(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
	struct vesaMode *mode = &modeInfo[curMode];
	struct modeData *data = &modesData[curMode];
	uint32_t v;
	if ((mode->bpp == 8) && (mode->memory_model == 4))
	{
		// pattle...
		// RGB -> VGA palette
		// faster
		v = (r & 0xE0) | ((g & 0xC0) >> 3) | ((b & 0xE0) >> 5);
	} else {
		v = (r & data->red_mask) >> (8 - mode->red_mask);
		v = v << mode->red_position;
		uint32_t gg = (g & data->green_mask) >> (8 - mode->green_mask);
		gg = gg << mode->green_position;
		v = v | gg;
		gg = (b & data->blue_mask) >> (8 - mode->blue_mask);
		gg = gg << mode->blue_position;
		v = v | gg;
	}
	if (mode->bpp <= 8)
	{
		fBuf[y * mode->pitch + x] = v & 0xff;
	} else if (mode->bpp <= 16)
	{
		fBuf[y * mode->pitch + x * 2 + 1] = (v & 0xff00) >> 8;
		fBuf[y * mode->pitch + x * 2] = v & 0xff;
	} else if (mode->bpp <= 24)
	{
		fBuf[y * mode->pitch + x * 3 + 2] = (v & 0xff0000) >> 16;
		fBuf[y * mode->pitch + x * 3 + 1] = (v & 0xff00) >> 8;
		fBuf[y * mode->pitch + x * 3] = v & 0xff;
	} else if (mode->bpp <= 32)
	{
		fBuf[y * mode->pitch + x * 4 + 3] = (v & 0xff000000) >> 24;
		fBuf[y * mode->pitch + x * 4 + 2] = (v & 0xff0000) >> 16;
		fBuf[y * mode->pitch + x * 4 + 1] = (v & 0xff00) >> 8;
		fBuf[y * mode->pitch + x * 4] = v & 0xff;
	}
	return 0;
}

int vesa_get_mode(int *mode)
{
	if (vesa_call(&regs, 3, 0) != 0)
		return 1;
	*mode = regs.bx;
	return 0;
}

int vesa_get_modes(int* modes, int *size)
{
	if (ctlInfo == NULL)
		if (vesa_get_controller_info(NULL) != 0)
			return -1;
	int i;
	for (i=0; i<*size; i++)
	{
		if (i == ctlInfo->modeCount)
		{
			*size = ctlInfo->modeCount;
			return 0;
		}
		modes[i] = vesaModes[i];
	}

	return 0;
}

int vesa_get_mode_info_byid(int modeid, struct vesaModeInfo *info)
{
	if (ctlInfo == NULL)
		if (vesa_get_controller_info(NULL) != 0)
			return -1;

	if ((modeid < 0) || (modeid >= ctlInfo->modeCount))
		return -2;
	info->mode_no = vesaModes[modeid];
	info->Xres = modeInfo[modeid].Xres;
	info->Yres = modeInfo[modeid].Yres;
	info->pitch = modeInfo[modeid].pitch;
	if ((modeInfo[modeid].memory_model == 4) && (modeInfo[modeid].bpp == 8))
		info->memory_model = VESA_DIRECT_COLOR;
	else
		info->memory_model = modeInfo[modeid].memory_model;
	info->bpp = modeInfo[modeid].bpp;
	info->red_mask = modeInfo[modeid].red_mask;
	info->green_mask = modeInfo[modeid].green_mask;
	info->blue_mask = modeInfo[modeid].blue_mask;
	info->red_position = modeInfo[modeid].red_position;
	info->green_position = modeInfo[modeid].green_position;
	info->blue_position = modeInfo[modeid].blue_position;
	info->physbase = modeInfo[modeid].physbase;
	return 0;
}

int vesa_get_mode_info(int mode, struct vesaModeInfo *info)
{
	if (ctlInfo == NULL)
		if (vesa_get_controller_info(NULL) != 0)
			return -1;
	int i;
	for (i=0; i<ctlInfo->modeCount; i++)
	{
		if (vesaModes[i] == mode)
		{
			return vesa_get_mode_info_byid(i, info);
		}
	}
	return -2;
}

int vesa_get_controller_info(struct vesaControllerInfo *xinfo)
{
	int i;
	if (ctlInfo != NULL)
	{
		if (xinfo != NULL)
			memcpy(xinfo, ctlInfo, sizeof(struct vesaControllerInfo));
		return 0;
	}
	
	int ret = vesa_call(&regs, 0, 0);
	if (ret != 0)
	{
		if (debug)
		{
			cprintf("ERROR: VESA get info not supported\n");
		}
		return 1;
	}

	// Not only get the controller info,
	// but also get the modes, so we know modeCount
	struct vesaInfo *info = (struct vesaInfo *)REALPTR(regs.es, regs.di);
	if (strncmp(info->signature, "VESA", 4) != 0)
	{
		if (debug)
		{
			cprintf("ERROR: signature mismatch in VESA info\n");
		}
		return 2;
	}

	int16_t *modes = (int16_t*)REALPTR1(info->videomodes);
	int mc = 0;
	for (i=0; i<MAX_MODES; i++)
	{
		if (modes[i] == -1) 
		{
			mc = i;
			break;
		}
	}

	if (vesaModes)
		kfree(vesaModes);
	if (modeInfo)
		kfree(modeInfo);
	if (modesData)
		kfree(modesData);
	vesaModes = kmalloc(mc * sizeof(int16_t));
	modeInfo = kmalloc(mc * sizeof(struct vesaMode));
	modesData = kmalloc(mc * sizeof(struct modeData));
	for (i=0; i<mc; i++)
		vesaModes[i] = modes[i];

	ctlInfo = kmalloc(sizeof(struct vesaControllerInfo));
	ctlInfo->version = info->version;
	strncpy(ctlInfo->oemString, REALPTR1(info->oemString), VESA_MAX_OEM_STR_LEN);
	memcpy(ctlInfo->caps, info->capabilities, 4);
	ctlInfo->modeCount = mc;
	ctlInfo->totalMem = info->totalMemory;
	ctlInfo->oemRev = info->oemRev;
	strncpy(ctlInfo->oemVendor, REALPTR1(info->oemVendor), VESA_MAX_OEM_STR_LEN);
	strncpy(ctlInfo->oemProduct, REALPTR1(info->oemProduct), VESA_MAX_OEM_STR_LEN);
	strncpy(ctlInfo->oemProductRev, REALPTR1(info->oemProductRev), VESA_MAX_OEM_STR_LEN);

	for (i=0; i<mc; i++)
	{
		vesa_call(&regs, 1, vesaModes[i]);
		struct vesaMode *mode = (struct vesaMode *)REALPTR(regs.es, regs.di);
#if 0
		cprintf("Mode %x: ", vesaModes[i]);
		cprintf("%dx%d %s %s base: %08x banks: %d banksize: %d model: %d planes: %d bpp: %d redshift: %d redsize: %d\n", mode->Xres, mode->Yres, 
				test_mode_attr(mode, MODE_ATTR_GRAPHICS) ? "graphics" : "text",
				test_mode_attr(mode, MODE_ATTR_COLOR) ? "color" : "mono", 
				mode->physbase, mode->banks, mode->bank_size, mode->memory_model, mode->planes, mode->bpp, mode->red_position, mode->red_mask);
#endif
		memcpy(&modeInfo[i], mode, sizeof(struct vesaMode));
		mode = &modeInfo[i];
		if (mode->memory_model == 4)
		{
			if (mode->bpp == 8)
			{
				// 8bpp Packed -> pattle...
				mode->red_position = 5;
				mode->green_position = 3;
				mode->blue_position = 0;
				mode->red_mask = 3;
				mode->green_mask = 2;
				mode->blue_mask = 3;
				// Cannot set memory model=DIRECT here, since we need
				// to know model==PACKED in set_mode to init palette
			} else if ((mode->bpp == 15) || (mode->bpp == 16))
			{
				// 15: 5,5,5
				mode->red_position = 10;
				mode->green_position = 5;
				mode->blue_position = 0;
				mode->red_mask = 5;
				mode->green_mask = 5;
				mode->blue_mask = 5;
				mode->memory_model = VESA_DIRECT_COLOR;
			} else if ((mode->bpp == 24) || (mode->bpp == 32))
			{
				// 16: 1,5,5,5
				mode->red_position = 16;
				mode->green_position = 8;
				mode->blue_position = 0;
				mode->red_mask = 8;
				mode->green_mask = 8;
				mode->blue_mask = 8;
				mode->memory_model = VESA_DIRECT_COLOR;
			}
		}
		if (mode->red_mask != 0)
		{
			modesData[i].red_mask = ((1 << mode->red_mask) - 1) << (8 - mode->red_mask);
			modesData[i].green_mask = ((1 << mode->green_mask) - 1) << (8 - mode->green_mask);
			modesData[i].blue_mask = ((1 << mode->blue_mask) - 1) << (8 - mode->blue_mask);

		}
	}

	if (xinfo != NULL)
		memcpy(xinfo, ctlInfo, sizeof(struct vesaControllerInfo));
	return 0;
}

int vesa_get_state_size(int *size)
{
	regs.dx = 0;
	int ret = vesa_call(&regs, 4, 0xf);
	if (ret)
		return -1;
	*size = (int)regs.bx * 64;
	return 0;
}

int vesa_save_state(char *buf, int size)
{
	int xsize;
	if (vesa_get_state_size(&xsize))
		return -1;

	if (size < xsize)
		return -3;

	regs.dx = 1;
	regs.bx = 0x7f00;
	regs.es = 0;
	int ret = vesa_call(&regs, 4, 0xf);
	if (ret)
		return -2;

	memcpy(buf, REALPTR(regs.es, regs.bx), xsize);
	return 0;
}

int vesa_restore_state(char *buf, int size)
{
	int xsize;
	if (vesa_get_state_size(&xsize))
		return -1;

	if (size < xsize)
		return -3;

	regs.dx = 2;
	regs.bx = 0x7f00;
	regs.es = 0;
	memcpy(REALPTR(regs.es, regs.bx), buf, xsize);

	if (vesa_call(&regs, 4, 0xf) != 0)
		return -2;

	return 0;
}

int vesa_set_palette(char *buf, int size, int start)
{
	int width;
	if (vesa_get_palette_width(&width))
	{
		if (debug)
		{
			cprintf("ERR: get pat width fail\n");
			width = 6;
		}
	} else {
		width = 8;
		if (vesa_set_palette_width(&width))
			if (debug)
				cprintf("WARN: cannot extend palette width\n");
	}
	if (debug)
		cprintf("Palette width: %d\n", width);
#ifdef PALETTE_WITH_VBE
	regs.es = 0;
	regs.di = 0x8000;
	memcpy(REALPTR(regs.es, regs.di), buf, size * 4);
	regs.bx = 0;
	regs.dx = start;
	return vesa_call(&regs, 9, size);
#else
	__outb(VGA_RESET, 0xff);
	__outb(VGA_WRITE, 0);
	int i;
	if (width == 8)
		for (i=0; i<size; i++)
		{
			__outb(VGA_DATA, buf[i*4+1]);
			__outb(VGA_DATA, buf[i*4+2]);
			__outb(VGA_DATA, buf[i*4+3]);
		}
	else
		for (i=0; i<size; i++)
		{
			__outb(VGA_DATA, buf[i*4+1] >> 2);
			__outb(VGA_DATA, buf[i*4+2] >> 2);
			__outb(VGA_DATA, buf[i*4+3] >> 2);
		}
	return 0;
#endif
}

int vesa_get_palette(char *buf, int size, int start)
{
	int width;
	if (vesa_get_palette_width(&width))
		if (debug)
		{
			cprintf("ERR: get pat width fail\n");
			width = 6;
		}
	if (debug)
		cprintf("Palette width: %d\n", width);
#ifdef PALETTE_WITH_VBE
	regs.es = 0;
	regs.di = 0x8000;
	regs.bx = 1;
	regs.dx = start;
	int ret = vesa_call(&regs, 9, size);
	if (ret == 0)
		memcpy(buf, REALPTR(regs.es, regs.di), size * 4);
	return ret;
#else
	__outb(VGA_RESET, 0xff);
	__outb(VGA_READ, 0);
	int i;
	if (width == 8)
		for (i=0; i<size; i++)
		{
			buf[i*4+1] = __inb(VGA_DATA);
			buf[i*4+2] = __inb(VGA_DATA);
			buf[i*4+3] = __inb(VGA_DATA);
		}
	else
		for (i=0; i<size; i++)
		{
			buf[i*4+1] = __inb(VGA_DATA) << 2;
			buf[i*4+2] = __inb(VGA_DATA) << 2;
			buf[i*4+3] = __inb(VGA_DATA) << 2;
		}
	return 0;
#endif
}

int vesa_get_palette_width(int *width)
{
	regs.bx = 1;
	int ret;
	if ((ret = vesa_call(&regs, 8, 0)))
		return ret;
	*width = regs.bx >> 8;
	return 0;
}

int vesa_set_palette_width(int *width)
{
	regs.bx = (*width & 0xFF) << 8;
	int ret;
	if ((ret = vesa_call(&regs, 8, 0)))
		return ret;
	*width = regs.bx >> 8;
	return 0;
}

static int vesa_demo()
{
	int i;
	 
	struct vesaControllerInfo *info = kmalloc(sizeof(struct vesaControllerInfo));
	if (vesa_get_controller_info(info) != 0)
	{
		cprintf("Failed to get controller info.\n");
		return 2;
	}

	/* for (i = 0; i != 512; ++ i) */
	/* { */
	/* 	  unsigned char c = (unsigned char)x86emu_read_byte_noperm(emu, 0x7f00 + i); */
	/* 	  cprintf("%02x(%c)", c, c); */
	/* 	  if (i % 10 == 9) cprintf("\n"); */
	/* 	  else cprintf(" "); */
	/* } */
	/* if (i % 10 != 0) cprintf("\n"); */

	/* cprintf("ax: %04x\n", emu->x86.R_AX); */

	/* cprintf("Signature: "); */
	/* for (i = 0; i < 4; i++) */
	/* 	  cprintf("%c", info->signature[i]); */
	/* cprintf("\n"); */

	cprintf("VESA %d.%d BIOS %d.%d\n", info->version >> 8, info->version & 0xff, info->oemRev & 0xff, info->oemRev >> 8);
	cprintf("OEM: %s\n", info->oemString);
	cprintf("Vendor: %s\nProduct: %s rev. %s\n", 
			info->oemVendor, info->oemProduct, info->oemProductRev);

	int cmode;
	vesa_get_mode(&cmode);
	cprintf("Current mode: %d\n", cmode);
	 
	int ssize;
	int ret;
	if (vesa_get_state_size(&ssize))
		cprintf("FAILED to get state size\n");
	char *buf = kmalloc(ssize);
	if ((ret = vesa_save_state(buf, ssize)))
		cprintf("FAILED to save state err %d\n", ret);
	int modeno = 0x110;
	for (; modeno < 0x150; modeno++)
	{
		cprintf("current testing: %x ", modeno);
		int ret = vesa_set_mode(modeno);
		if (ret)
		{
			cprintf("set mode error\n");
			continue;
		}
		struct vesaModeInfo info;
		vesa_get_mode_info(modeno, &info);
		int w = info.Xres;
		int h = info.Yres;
		cprintf("%dx%dx%d ", w, h, info.bpp);
		int newmode;
		vesa_get_mode(&newmode);
		if ((newmode & 0x1FF) == modeno)
			cprintf("SET OK");
		else
			cprintf("SET ERR?");

		int j;
		if ((w > 256) && (h > 256))
			for (i=0; i<256; i++)
				for (j=0; j<256; j++)
				{
					//	vesa_draw_point(i, j, i, 0, 0);
					vesa_draw_point(i, j, i, j, 0);
				}
		else
			for (i=0; i<w; i++)
				for (j=0; j<h; j++)
					vesa_draw_point(i, j, i % 256, j % 256, 0);
		if ((w > 256* 2) && (h > 256*2))
			for (i=0; i<256; i++)
				for (j=0; j<256; j++)
				{
					vesa_draw_point(i+256, j, i, 0, j);
					vesa_draw_point(i+256, j+256, 0, i, j);
					vesa_draw_point(i, j+256, (i+j)/2, i, j);
				}
		if ((w > 256 * 3) && (h > 256 * 2))
			for (i=0; i<256; i++)
				for (j=0; j<256; j++)
				{
					vesa_draw_point(i+256*2, j, i, (i+j)/2, j);
					vesa_draw_point(i+256*2, j+256, i, j, (i+j)/2);
				}

		cprintf("\n");
		for (i=0; i<100000000; i++) {}
	}

	if ((ret = vesa_restore_state(buf, ssize)))
		cprintf("FAILED to restore state err %d\n", ret);

	cprintf("TEST finished!\n");
	return 0;
}

static int load(void)
{
	init_regs(&regs);
	init_emu_mem();
	return 0;
}

static int unload(void)
{
	if (fBuf != NULL)
	{
		// ekf_mmio_free(fBuf);
		fBuf = NULL;
	}

	if (vesaModes)
		kfree(vesaModes);
	vesaModes = NULL;
	if (modeInfo)
		kfree(modeInfo);
	modeInfo = NULL;
	if (modesData)
		kfree(modesData);
	modesData = NULL;
	if (ctlInfo)
		kfree(ctlInfo);
	ctlInfo = NULL;

	return 0;
}

int
vesa_init(void)
{
	load();

	vesa_demo();

	while (1);

	if (vesa_get_controller_info(NULL) != 0)
	{
		cprintf("Failed to get controller info.\n");
		return -1;
	}

	int i, mode = -1, maxpixel = 0;
	for (i = 0; i < ctlInfo->modeCount; ++ i)
	{
		/* cprintf("Mode %x: ", vesaModes[i]); */
		/* cprintf("%dx%d %s %s base: %08x banks: %d banksize: %d model: %d planes: %d bpp: %d redshift: %d redsize: %d\n", */
		/* 			  modeInfo[i].Xres, modeInfo[i].Yres,  */
		/* 			  test_mode_attr(&modeInfo[i], MODE_ATTR_GRAPHICS) ? "graphics" : "text", */
		/* 			  test_mode_attr(&modeInfo[i], MODE_ATTR_COLOR) ? "color" : "mono",  */
		/* 			  modeInfo[i].physbase, modeInfo[i].banks, modeInfo[i].bank_size, */
		/* 			  modeInfo[i].memory_model, modeInfo[i].planes, */
		/* 			  modeInfo[i].bpp, modeInfo[i].red_position, modeInfo[i].red_mask); */
		  
		if (test_mode_attr(&modeInfo[i], MODE_ATTR_GRAPHICS) &&
			((modeInfo[i].bpp == 24) || (modeInfo[i].bpp == 32)) &&
			modeInfo[i].memory_model == VESA_DIRECT_COLOR)
		{
			if (modeInfo[i].Xres * modeInfo[i].Yres > maxpixel)
				/* && */
				/* modeInfo[i].Xres <= 1024 && modeInfo[i].Yres <= 1024) */
			{
				mode = i;
				maxpixel = modeInfo[i].Xres * modeInfo[i].Yres;
			}
		}
	}
	
	cprintf("Would choose mode 0x%04x, Resolution = %dx%d bpp = %d\n",
			vesaModes[mode], modeInfo[mode].Xres, modeInfo[mode].Yres, modeInfo[mode].bpp);
	cprintf("Press any key to confirm\n");
	cgetchar();


	int ret = vesa_set_mode(vesaModes[mode]);
	if (ret)
	{
		cprintf("set mode error\n");
		return -1;
	}
	 
	int newmode;
	vesa_get_mode(&newmode);
	if ((newmode & 0x1FF) == vesaModes[mode])
	{
		cprintf("SET OK");
		return 0;
	}
	else
	{
		cprintf("SET ERR?");
		return -1;
	}
}
