/* drivers/video/backlight/ili9320.h
 *
 * ILI9320 LCD controller driver core.
 *
 * Copyright 2007 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * http://armlinux.simtec.co.uk/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

struct ili9341_page {
	unsigned short x;
	unsigned short y;
	unsigned short *buffer;
	unsigned short *oldbuffer;
	unsigned short len;
	int must_update;
};

/* ILI9341 device state. */
struct ili9341 {
	struct spi_device		*spi;	/* SPI attachged device. */
	struct device			*dev;
	struct fb_info			*info;
	unsigned int			pages_count;
	struct ili9341_page	*pages;
	unsigned long			pseudo_palette[17];
	int						backlight;

	int				gpiodc;
	int				gpiorst;

	int				 power; /* current power state. */
	int				 initialised;
};
