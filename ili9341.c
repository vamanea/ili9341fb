/* drivers/video/backlight/ili9320.c
 *
 * ILI9320 LCD controller driver core.
 *
 * Copyright 2007 Simtec Electronics
 *	http://armlinux.simtec.co.uk/
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/fb.h>
#include <asm/io.h>

#include <linux/spi/spi.h>

#include "ili9341_reg.h"

#include "ili9341.h"

#define BLOCKLEN (4096)

static int ili9341_spi_write(struct ili9341 *ili, uint8_t byte, bool data)
{
	gpio_set_value(ili->gpiodc, data);
	return spi_write(ili->spi, &byte, 1);
}

static int ili9341_send_command(struct ili9341 *ili, uint8_t byte)
{
	return ili9341_spi_write(ili, byte, 0);
}

static int ili9341_send_byte(struct ili9341 *ili, uint8_t byte)
{
	return ili9341_spi_write(ili, byte, 1);
}

/*
This routine will clock in len words of pixel data.
*/
static int ili9341_spi_write_datablock(struct ili9341 *ili, 
					uint8_t *block, int len) 
{
	int x, t;
	unsigned short value;
#if 0
	//ToDo: send in parts if needed
	if (len > BLOCKLEN) {
		dev_err(ili->dev, "%s: len > blocklen (%i > %i)\n",
			__func__, len, BLOCKLEN);
		len = BLOCKLEN;
	}
#endif
	dev_dbg(ili->dev, "%s: item=0x%p\n", __func__, (void *)ili);
	gpio_set_value(ili->gpiodc, 1);

	return spi_write(ili->spi, block, len);
}


static int ili9341_set_window(struct ili9341 *ili, uint16_t x0, 
							  uint16_t y0, uint16_t x1, uint16_t y1)
{
	ili9341_send_command(ili, ILI9341_CASET); // Column addr set
	ili9341_send_byte(ili, x0 >> 8);
	ili9341_send_byte(ili, x0 & 0xFF); // XSTART
	ili9341_send_byte(ili, x1 >> 8);
	ili9341_send_byte(ili, x1 & 0xFF); // XEND
	ili9341_send_command(ili, ILI9341_PASET); // Row addr set
	ili9341_send_byte(ili, y0>>8);
	ili9341_send_byte(ili, y0); // YSTART
	ili9341_send_byte(ili, y1>>8);
	ili9341_send_byte(ili, y1); // YEND
	ili9341_send_command(ili, ILI9341_RAMWR); // write to RAM
	return 0;
}

static void ili9341_reset(struct ili9341 *ili)
{
	gpio_set_value(ili->gpiorst, 1);
	mdelay(50);

	gpio_set_value(ili->gpiorst, 0);
	mdelay(50);

	gpio_set_value(ili->gpiorst, 1);
	mdelay(100);
}


/* This routine will allocate the buffer for the complete framebuffer. This
 * is one continuous chunk of 16-bit pixel values; userspace programs
 * will write here */
static int __init ili9341_video_alloc(struct ili9341 *ili)
{
	unsigned int frame_size;

	dev_dbg(ili->dev, "%s: item=0x%p\n", __func__, (void *)ili);

	frame_size = ili->info->fix.line_length * ili->info->var.yres;
	dev_dbg(ili->dev, "%s: item=0x%p frame_size=%u\n",
		__func__, (void *)ili, frame_size);

	ili->pages_count = frame_size / PAGE_SIZE;
	if ((ili->pages_count * PAGE_SIZE) < frame_size) {
		ili->pages_count++;
	}
	dev_dbg(ili->dev, "%s: item=0x%p pages_count=%u\n",
		__func__, (void *)ili, ili->pages_count);

	ili->info->fix.smem_len = ili->pages_count * PAGE_SIZE;
	ili->info->fix.smem_start =
	    (unsigned long)vmalloc(ili->info->fix.smem_len);
	if (!ili->info->fix.smem_start) {
		dev_err(ili->dev, "%s: unable to vmalloc\n", __func__);
		return -ENOMEM;
	}
	memset((void *)ili->info->fix.smem_start, 0, ili->info->fix.smem_len);

	return 0;
}

static void ili9341_video_free(struct ili9341 *ili)
{
	dev_dbg(ili->dev, "%s: item=0x%p\n", __func__, (void *)ili);

	kfree((void *)ili->info->fix.smem_start);
}

/* This routine will allocate a ili9341_page struct for each vm page in the
 * main framebuffer memory. Each struct will contain a pointer to the page
 * start, an x- and y-offset, and the length of the pagebuffer 
 * which is in the framebuffer. */
static int __init ili9341_pages_alloc(struct ili9341 *ili)
{
	unsigned short pixels_per_page;
	unsigned short yoffset_per_page;
	unsigned short xoffset_per_page;
	unsigned short index;
	unsigned short x = 0;
	unsigned short y = 0;
	unsigned short *buffer;
	unsigned short *oldbuffer;
	unsigned int len;

	dev_dbg(ili->dev, "%s: item=0x%p\n", __func__, (void *)ili);

	ili->pages = kmalloc(ili->pages_count * sizeof(struct ili9341_page),
			      GFP_KERNEL);
	if (!ili->pages) {
		dev_err(ili->dev, "%s: unable to kmalloc for ili9341_page\n",
			__func__);
		return -ENOMEM;
	}

	pixels_per_page = PAGE_SIZE / (ili->info->var.bits_per_pixel / 8);
	yoffset_per_page = pixels_per_page / ili->info->var.xres;
	xoffset_per_page = pixels_per_page -
	    (yoffset_per_page * ili->info->var.xres);
	dev_info(ili->dev, "%s: item=0x%p pixels_per_page=%hu "
		"yoffset_per_page=%hu xoffset_per_page=%hu\n",
		__func__, (void *)ili, pixels_per_page,
		yoffset_per_page, xoffset_per_page);

	oldbuffer = kmalloc(ili->pages_count * pixels_per_page * 2,
			      GFP_KERNEL);
	if (!oldbuffer) {
		dev_err(ili->dev, "%s: unable to kmalloc for ili9341_page oldbuffer\n",
			__func__);
		return -ENOMEM;
	}
	buffer = (unsigned short *)ili->info->fix.smem_start;
	for (index = 0; index < ili->pages_count; index++) {
		len = (ili->info->var.xres * ili->info->var.yres) -
		    (index * pixels_per_page);
		if (len > pixels_per_page) {
			len = pixels_per_page;
		}
		dev_info(ili->dev,
			"%s: page[%d]: x=%3hu y=%3hu buffer=0x%p len=%3hu\n",
			__func__, index, x, y, buffer, len);
		ili->pages[index].x = x;
		ili->pages[index].y = y;
		ili->pages[index].buffer = buffer;
		ili->pages[index].oldbuffer = oldbuffer;
		ili->pages[index].len = len;

		x += xoffset_per_page;
		if (x >= ili->info->var.xres) {
			y++;
			x -= ili->info->var.xres;
		}
		y += yoffset_per_page;
		buffer += pixels_per_page;
		oldbuffer += pixels_per_page;
	}

	return 0;
}

static void ili9341_pages_free(struct ili9341 *ili)
{
	dev_dbg(ili->dev, "%s: ili=0x%p\n", __func__, (void *)ili);

	kfree(ili->pages);
}

static void ili9341_copy(struct ili9341 *ili, unsigned int index)
{
	unsigned int ystart, yend;
	int chstart, chend;
	unsigned int x;
	unsigned int y;
	unsigned short *buffer, *oldbuffer;
	unsigned int len;
	x = ili->pages[index].x;
	y = ili->pages[index].y;
	len = ili->pages[index].len;
	ystart = y;
	yend = y+(len/(ILI9341_TFTWIDTH))+1;
	dev_dbg(ili->dev,
		"%s: page[%u]: x=%3hu y=%3hu buffer=0x%p len=%3hu\n",
		__func__, index, x, y, buffer, len);

	//Move to start of line.
	buffer = ili->pages[index].buffer-x;
	oldbuffer = ili->pages[index].oldbuffer-x;
	x = 0;

	//If we arrive here, we basically assume something is written at the lines
	//starting with y and lasting the page.
	for (y = ystart; y < yend; y++) {
		//Find start and end of changed data
		chstart = -1;
		for (x = 0; x < ILI9341_TFTWIDTH; x++) {
			if (buffer[x] != oldbuffer[x]) {
				oldbuffer[x] = buffer[x];
				if (chstart == -1)
					chstart = x;
				chend=x;
			}
		}
		if (chstart!=-1) {
			/* Something changed this line! chstart and chend 
			 * contain start and end x-coords. */
			ili9341_set_window(ili, chstart, y, chend, y);
			ili9341_spi_write_datablock(ili, &buffer[chstart], chend - chstart);
		}
		buffer += ILI9341_TFTWIDTH;
		oldbuffer += ILI9341_TFTWIDTH;
	}
}

static void ili9341_update_all(struct ili9341 *ili)
{
	unsigned short i;
	struct fb_deferred_io *fbdefio = ili->info->fbdefio;
	for (i = 0; i < ili->pages_count; i++) {
		ili->pages[i].must_update=1;
	}
	schedule_delayed_work(&ili->info->deferred_work, fbdefio->delay);
}

static void ili9341_update(struct fb_info *info, struct list_head *pagelist)
{
	struct ili9341 *ili = (struct ili9341 *)info->par;
	struct page *page;
	int i;

	/* We can be called because of pagefaults (mmap'ed framebuffer, pages
	 * returned in *pagelist) or because of kernel activity
	 * (pages[i]/must_update!=0). Add the former to the list of the latter. */
	list_for_each_entry(page, pagelist, lru) {
		ili->pages[page->index].must_update=1;
	}

	//Copy changed pages.
	for(i = 0; i < ili->pages_count; i++) {
		/*ToDo: Small race here between checking and setting must_update, 
		 * maybe lock? */
		if (ili->pages[i].must_update) {
			ili->pages[i].must_update=0;
			ili9341_copy(ili, i);
		}
	}

}

static inline __u32 CNVT_TOHW(__u32 val, __u32 width)
{
	return ((val<<width) + 0x7FFF - val)>>16;
}


/* This routine is needed because the console driver won't work without it. */
static int ili9341_setcolreg(unsigned regno,
			       unsigned red, unsigned green, unsigned blue,
			       unsigned transp, struct fb_info *info)
{
	int ret = 1;

	/*
	 * If greyscale is true, then we convert the RGB value
	 * to greyscale no matter what visual we are using.
	 */
	if (info->var.grayscale)
		red = green = blue = (19595 * red + 38470 * green +
				      7471 * blue) >> 16;
	switch (info->fix.visual) {
	case FB_VISUAL_TRUECOLOR:
		if (regno < 16) {
			u32 *pal = info->pseudo_palette;
			u32 value;

			red = CNVT_TOHW(red, info->var.red.length);
			green = CNVT_TOHW(green, info->var.green.length);
			blue = CNVT_TOHW(blue, info->var.blue.length);
			transp = CNVT_TOHW(transp, info->var.transp.length);

			value = (red << info->var.red.offset) |
				(green << info->var.green.offset) |
				(blue << info->var.blue.offset) |
				(transp << info->var.transp.offset);

			pal[regno] = value;
			ret = 0;
		}
		break;
	case FB_VISUAL_STATIC_PSEUDOCOLOR:
	case FB_VISUAL_PSEUDOCOLOR:
		break;
	}
	return ret;
}

static int ili9341_blank(int blank_mode, struct fb_info *info)
{
	struct ili9341 *ili = (struct ili9341 *)info->par;
	if (blank_mode == FB_BLANK_UNBLANK)
		ili->backlight=1;
	else
		ili->backlight=0;
	/* Item->backlight won't take effect until the LCD is written to. Force that
	 * by dirty'ing a page. */
	ili->pages[0].must_update=1;
	schedule_delayed_work(&info->deferred_work, 0);
	return 0;
}

static void ili9341_touch(struct fb_info *info, int x, int y, int w, int h) 
{
	struct fb_deferred_io *fbdefio = info->fbdefio;
	struct ili9341 *ili = (struct ili9341 *)info->par;
	int i, ystart, yend;
	if (fbdefio) {
		/* Touch the pages the y-range hits, so the deferred io will update them. */
		for (i=0; i<ili->pages_count; i++) {
			ystart=ili->pages[i].y;
			yend=ili->pages[i].y+(ili->pages[i].len/info->fix.line_length)+1;
			if (!((y+h)<ystart || y>yend)) {
				ili->pages[i].must_update=1;
			}
		}
		/* Schedule the deferred IO to kick in after a delay.*/
		schedule_delayed_work(&info->deferred_work, fbdefio->delay);
	}
}

static void ili9341_fillrect(struct fb_info *p, const struct fb_fillrect *rect) 
{
	sys_fillrect(p, rect);
	ili9341_touch(p, rect->dx, rect->dy, rect->width, rect->height);
}

static void ili9341_imageblit(struct fb_info *p, const struct fb_image *image) 
{
	sys_imageblit(p, image);
	ili9341_touch(p, image->dx, image->dy, image->width, image->height);
}

static void ili9341_copyarea(struct fb_info *p, const struct fb_copyarea *area) 
{
	sys_copyarea(p, area);
	ili9341_touch(p, area->dx, area->dy, area->width, area->height);
}

static ssize_t ili9341_write(struct fb_info *p, const char __user *buf, 
				size_t count, loff_t *ppos) 
{
	ssize_t res;
	res = fb_sys_write(p, buf, count, ppos);
	ili9341_touch(p, 0, 0, p->var.xres, p->var.yres);
	return res;
}


static struct fb_ops ili9341_fbops = {
	.owner        = THIS_MODULE,
	.fb_read      = fb_sys_read,
	.fb_write     = ili9341_write,
	.fb_fillrect  = ili9341_fillrect,
	.fb_copyarea  = ili9341_copyarea,
	.fb_imageblit = ili9341_imageblit,
	.fb_setcolreg	= ili9341_setcolreg,
	.fb_blank	= ili9341_blank,
};

static struct fb_fix_screeninfo ili9341_fix __initdata = {
	.id          = "ILI9341",
	.type        = FB_TYPE_PACKED_PIXELS,
	.visual      = FB_VISUAL_TRUECOLOR,
	.accel       = FB_ACCEL_NONE,
	.line_length = ILI9341_TFTWIDTH * 2,
};

static struct fb_var_screeninfo ili9341_var __initdata = {
	.xres		= ILI9341_TFTWIDTH,
	.yres		= ILI9341_TFTHEIGHT,
	.xres_virtual	= ILI9341_TFTWIDTH,
	.yres_virtual	= ILI9341_TFTHEIGHT,
	.width		= ILI9341_TFTWIDTH,
	.height		= ILI9341_TFTHEIGHT,
	.bits_per_pixel	= 16,
	/* Pixel format is RGB565 packed */
	.red		= {11, 5, 0},
	.green		= {5, 6, 0},
	.blue		= {0, 5, 0},
	.activate	= FB_ACTIVATE_NOW,
	.vmode		= FB_VMODE_NONINTERLACED,
};

static struct fb_deferred_io ili9341_defio = {
	.delay          = HZ / 50,
	.deferred_io    = &ili9341_update,
};

void ili9341_set_orientation(struct ili9341 *ili, uint8_t flags)
{
	uint8_t madctl = 0x48;

	if (flags & ILI9341_FLIP_X) {
		madctl &= ~(1 << 6);
	}

	if (flags & ILI9341_FLIP_Y) {
		madctl |= 1 << 7;
	}

	if (flags & ILI9341_SWITCH_XY) {
		madctl |= 1 << 5;
	}

	ili9341_send_command(ili, ILI9341_MADCTL);
	ili9341_send_byte(ili, madctl);
}


#define SCREEN_TEST
static inline int ili9341_init_chip(struct ili9341 *ili)
{
	int ret = 0;
#ifdef SCREEN_TEST
	int x,y;
#endif

	ili9341_reset(ili);

	ili9341_send_command(ili, 0xEF);
	ili9341_send_byte(ili, 0x03);
	ili9341_send_byte(ili, 0x80);
	ili9341_send_byte(ili, 0x02);
	ili9341_send_command(ili, 0xCF);
	ili9341_send_byte(ili, 0x00);
	ili9341_send_byte(ili, 0XC1);
	ili9341_send_byte(ili, 0X30);
	ili9341_send_command(ili, 0xED);
	ili9341_send_byte(ili, 0x64);
	ili9341_send_byte(ili, 0x03);
	ili9341_send_byte(ili, 0X12);
	ili9341_send_byte(ili, 0X81);
	ili9341_send_command(ili, 0xE8);
	ili9341_send_byte(ili, 0x85);
	ili9341_send_byte(ili, 0x00);
	ili9341_send_byte(ili, 0x78);
	ili9341_send_command(ili, 0xCB);
	ili9341_send_byte(ili, 0x39);
	ili9341_send_byte(ili, 0x2C);
	ili9341_send_byte(ili, 0x00);
	ili9341_send_byte(ili, 0x34);
	ili9341_send_byte(ili, 0x02);
	ili9341_send_command(ili, 0xF7);
	ili9341_send_byte(ili, 0x20);
	ili9341_send_command(ili, 0xEA);
	ili9341_send_byte(ili, 0x00);
	ili9341_send_byte(ili, 0x00);
	ili9341_send_command(ili, ILI9341_PWCTR1); //Power control
	ili9341_send_byte(ili, 0x23); //VRH[5:0]
	ili9341_send_command(ili, ILI9341_PWCTR2); //Power control
	ili9341_send_byte(ili, 0x10); //SAP[2:0];BT[3:0]
	ili9341_send_command(ili, ILI9341_VMCTR1); //VCM control
	ili9341_send_byte(ili, 0x3e); //¶Ô±È¶Èµ÷½Ú
	ili9341_send_byte(ili, 0x28);
	ili9341_send_command(ili, ILI9341_VMCTR2); //VCM control2
	ili9341_send_byte(ili, 0x86); //--
	ili9341_send_command(ili, ILI9341_MADCTL); // Memory Access Control
	ili9341_send_byte(ili, 0x48);
	ili9341_send_command(ili, ILI9341_PIXFMT);
	ili9341_send_byte(ili, 0x55);
	ili9341_send_command(ili, ILI9341_FRMCTR1);
	ili9341_send_byte(ili, 0x00);
	ili9341_send_byte(ili, 0x18);
	ili9341_send_command(ili, ILI9341_DFUNCTR); // Display Function Control
	ili9341_send_byte(ili, 0x08);
	ili9341_send_byte(ili, 0x82);
	ili9341_send_byte(ili, 0x27);
	ili9341_send_command(ili, 0xF2); // 3Gamma Function Disable
	ili9341_send_byte(ili, 0x00);
	ili9341_send_command(ili, ILI9341_GAMMASET); //Gamma curve selected
	ili9341_send_byte(ili, 0x01);
	ili9341_send_command(ili, ILI9341_GMCTRP1); //Set Gamma
	ili9341_send_byte(ili, 0x0F);
	ili9341_send_byte(ili, 0x31);
	ili9341_send_byte(ili, 0x2B);
	ili9341_send_byte(ili, 0x0C);
	ili9341_send_byte(ili, 0x0E);
	ili9341_send_byte(ili, 0x08);
	ili9341_send_byte(ili, 0x4E);
	ili9341_send_byte(ili, 0xF1);
	ili9341_send_byte(ili, 0x37);
	ili9341_send_byte(ili, 0x07);
	ili9341_send_byte(ili, 0x10);
	ili9341_send_byte(ili, 0x03);
	ili9341_send_byte(ili, 0x0E);
	ili9341_send_byte(ili, 0x09);
	ili9341_send_byte(ili, 0x00);
	ili9341_send_command(ili, ILI9341_GMCTRN1); //Set Gamma
	ili9341_send_byte(ili, 0x00);
	ili9341_send_byte(ili, 0x0E);
	ili9341_send_byte(ili, 0x14);
	ili9341_send_byte(ili, 0x03);
	ili9341_send_byte(ili, 0x11);
	ili9341_send_byte(ili, 0x07);
	ili9341_send_byte(ili, 0x31);
	ili9341_send_byte(ili, 0xC1);
	ili9341_send_byte(ili, 0x48);
	ili9341_send_byte(ili, 0x08);
	ili9341_send_byte(ili, 0x0F);
	ili9341_send_byte(ili, 0x0C);
	ili9341_send_byte(ili, 0x31);
	ili9341_send_byte(ili, 0x36);
	ili9341_send_byte(ili, 0x0F);
	ili9341_send_command(ili, ILI9341_SLPOUT); //Exit Sleep
	mdelay(120);
	
	ili9341_set_orientation(ili, ILI9341_SWITCH_XY | ILI9341_FLIP_X);
	ili9341_send_command(ili, ILI9341_DISPON); //Display on
	
#ifdef SCREEN_TEST
	ili9341_set_window(ili, 0, 0, ILI9341_TFTWIDTH, ILI9341_TFTHEIGHT);
	for(y=ILI9341_TFTHEIGHT; y>0; y--) {
		for(x=ILI9341_TFTWIDTH; x>0; x--) {
			ili9341_send_byte(ili, 0x55);
			ili9341_send_byte(ili, 0x55);
		}
	}
#endif
/*	if (ret != 0) {
		dev_err(ili->dev, "failed to initialise display\n");
		return ret;
	}*/

	ili->initialised = 1;
	return ret;
}

static inline int ili9341_power_on(struct ili9341 *ili)
{

	if (!ili->initialised)
		ili9341_init_chip(ili);

	ili9341_send_command(ili, ILI9341_DISPON); //Display on

	return 0;
}

static inline int ili9341_power_off(struct ili9341 *ili)
{
	ili9341_send_command(ili, ILI9341_DISPOFF); //Display on
	return 0;
}

#define POWER_IS_ON(pwr)	((pwr) <= FB_BLANK_NORMAL)

static int ili9341_power(struct ili9341 *lcd, int power)
{
	int ret = 0;
#if 0
	dev_dbg(lcd->dev, "power %d => %d\n", lcd->power, power);

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = ili9341_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = ili9341_power_off(lcd);

	if (ret == 0)
		lcd->power = power;
	else
		dev_warn(lcd->dev, "failed to set power mode %d\n", power);
#endif
	return ret;
}

int ili9341_probe_spi(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct ili9341 *ili;
	struct fb_info *info;
	int ret = 0;

	/* verify we where given some information */
	
	ili = devm_kzalloc(&spi->dev, sizeof(struct ili9341), GFP_KERNEL);
	if (ili == NULL) {
		dev_err(dev, "no memory for device\n");
		return -ENOMEM;
	}
	ili->dev = dev;
	spi->mode = SPI_MODE_0;
	spi_setup(spi);

	spi_set_drvdata(spi, ili);
	dev_info(&spi->dev, "spi registered, item=0x%p\n", (void *)ili);

	ili->gpiodc = 16;
	gpio_request(ili->gpiodc, "ili9341 dc pin");
	gpio_direction_output(ili->gpiodc, 0);
	dev_info(&spi->dev, "gpio %i registered\n", ili->gpiodc);

	ili->gpiorst = 12;
	gpio_request(ili->gpiorst, "ili9341 reset pin");
	gpio_direction_output(ili->gpiorst, 0);
	dev_info(&spi->dev, "gpio %i registered\n", ili->gpiorst);

	ili->spi = spi;
	
	info = framebuffer_alloc(sizeof(struct ili9341), &spi->dev);
	if (!info) {
		ret = -ENOMEM;
		dev_err(&spi->dev,
			"%s: unable to framebuffer_alloc\n", __func__);
		goto out_item;
	}
	info->pseudo_palette = &ili->pseudo_palette;
	ili->info = info;
	info->par = ili;
	info->dev = &spi->dev;
	info->fbops = &ili9341_fbops;
	info->flags = FBINFO_FLAG_DEFAULT | FBINFO_VIRTFB;
	info->fix = ili9341_fix;
	info->var = ili9341_var;

	ret = ili9341_video_alloc(ili);
	if (ret) {
		dev_err(&spi->dev,
			"%s: unable to ili9341_video_alloc\n", __func__);
		goto out_info;
	}
	info->screen_base = (char __iomem *)ili->info->fix.smem_start;

	ret = ili9341_pages_alloc(ili);
	if (ret < 0) {
		dev_err(&spi->dev,
			"%s: unable to ili9341_pages_init\n", __func__);
		goto out_video;
	}

	info->fbdefio = &ili9341_defio;
	fb_deferred_io_init(info);

	ret = register_framebuffer(info);
	if (ret < 0) {
		dev_err(&spi->dev,
			"%s: unable to register_frambuffer\n", __func__);
		goto out_pages;
	}
	
	ili9341_init_chip(ili);
	ili9341_update_all(ili);

	return ret;

out_pages:
	ili9341_pages_free(ili);
out_video:
	ili9341_video_free(ili);
out_info:
	framebuffer_release(info);
out_item:
	kfree(ili);
	return ret;
}

int ili9341_remove(struct spi_device *spi)
{
	struct ili9341 *ili = spi_get_drvdata(spi);
	struct fb_info *info = dev_get_drvdata(&spi->dev);

	dev_set_drvdata(&spi->dev, NULL);
	unregister_framebuffer(info);
	ili9341_pages_free(ili);
	ili9341_video_free(ili);
	framebuffer_release(info);
	
	return 0;
}

#ifdef CONFIG_PM_SLEEP1
int ili9341_suspend(struct ili9341 *lcd)
{
	int ret;

	ret = ili9341_power(lcd, FB_BLANK_POWERDOWN);

/*	if (lcd->platdata->suspend == ILI9341_SUSPEND_DEEP) {
		lcd->initialised = 0;
	}
*/
	return ret;
}

int ili9341_resume(struct ili9341 *lcd)
{
	dev_info(lcd->dev, "resuming from power state %d\n", lcd->power);
#if 0
	if (lcd->platdata->suspend == ILI9341_SUSPEND_DEEP)
		ili9341_write(lcd, ILI9341_POWER1, 0x00);
#endif
	return ili9341_power(lcd, FB_BLANK_UNBLANK);
}
#endif

/* Power down all displays on reboot, poweroff or halt */
void ili9341_shutdown(struct ili9341 *lcd)
{
	ili9341_power(lcd, FB_BLANK_POWERDOWN);
}

static struct spi_driver ili9341_driver = {
	.driver = {
		.name		= "ili9341",
		.owner		= THIS_MODULE,
//		.pm		= &vgg2432a4_pm_ops,
	},
	.probe		= ili9341_probe_spi,
	.remove		= ili9341_remove,
//	.shutdown	= vgg2432a4_shutdown,
};

module_spi_driver(ili9341_driver);
#if 0
static int __init ili9341_init(void)
{
#if 0
	int ret;

	ret = gpio_request_one(GPIO_EEPROM_OE, GPIOF_OUT_INIT_HIGH,
				"93xx46 EEPROMs OE");
	if (ret) {
		pr_err("can't request gpio %d\n", GPIO_EEPROM_OE);
		return ret;
	}
#endif
	int ret = spi_register_driver(&ili9341_driver);
	if (ret) {
			pr_err("Failed to register ILI9341 SPI driver: %d\n", ret);
			return ret;
	}
	return ret;
}

static void __exit ili9341_cleanup(void)
{
	spi_unregister_driver(&ili9341_driver);
}
#endif

//module_init(ili9341_init);
//module_exit(ili9341_cleanup);

MODULE_AUTHOR("Ben Dooks <ben-linux@fluff.org>");
MODULE_DESCRIPTION("ILI9320 LCD Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("spi:ili9341");
