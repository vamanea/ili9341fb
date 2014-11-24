/* include/video/ili9320.c
 *
 * ILI9320 LCD controller configuration control.
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

#define ILI9341_FLIP_X 1
#define ILI9341_FLIP_Y 2
#define ILI9341_SWITCH_XY 4

//#define ILI9341_TFTWIDTH 240
//#define ILI9341_TFTHEIGHT 320
#define ILI9341_TFTWIDTH 320
#define ILI9341_TFTHEIGHT 240


#define ILI9341_NOP 0x00
#define ILI9341_SWRESET 0x01
#define ILI9341_RDDID 0x04
#define ILI9341_RDDST 0x09
#define ILI9341_SLPIN 0x10
#define ILI9341_SLPOUT 0x11
#define ILI9341_PTLON 0x12
#define ILI9341_NORON 0x13
#define ILI9341_RDMODE 0x0A
#define ILI9341_RDMADCTL 0x0B
#define ILI9341_RDPIXFMT 0x0C
#define ILI9341_RDIMGFMT 0x0A
#define ILI9341_RDSELFDIAG 0x0F
#define ILI9341_INVOFF 0x20
#define ILI9341_INVON 0x21
#define ILI9341_GAMMASET 0x26
#define ILI9341_DISPOFF 0x28
#define ILI9341_DISPON 0x29
#define ILI9341_CASET 0x2A
#define ILI9341_PASET 0x2B
#define ILI9341_RAMWR 0x2C
#define ILI9341_RAMRD 0x2E
#define ILI9341_PTLAR 0x30
#define ILI9341_MADCTL 0x36
#define ILI9341_PIXFMT 0x3A
#define ILI9341_FRMCTR1 0xB1
#define ILI9341_FRMCTR2 0xB2
#define ILI9341_FRMCTR3 0xB3
#define ILI9341_INVCTR 0xB4
#define ILI9341_DFUNCTR 0xB6
#define ILI9341_PWCTR1 0xC0
#define ILI9341_PWCTR2 0xC1
#define ILI9341_PWCTR3 0xC2
#define ILI9341_PWCTR4 0xC3
#define ILI9341_PWCTR5 0xC4
#define ILI9341_VMCTR1 0xC5
#define ILI9341_VMCTR2 0xC7
#define ILI9341_RDID1 0xDA
#define ILI9341_RDID2 0xDB
#define ILI9341_RDID3 0xDC
#define ILI9341_RDID4 0xDD
#define ILI9341_GMCTRP1 0xE0
#define ILI9341_GMCTRN1 0xE1
/*
#define ILI9341_PWCTR6 0xFC
*/
// Color definitions
#define	ILI9341_BLACK 0x0000
#define	ILI9341_BLUE 0x001F
#define	ILI9341_RED 0xF800
#define	ILI9341_GREEN 0x07E0
#define ILI9341_CYAN 0x07FF
#define ILI9341_MAGENTA 0xF81F
#define ILI9341_YELLOW 0xFFE0
#define ILI9341_WHITE 0xFFFF

/* Register contents definitions. */

#define ILI9341_OSCILATION_OSC		(1 << 0)

#define ILI9341_DRIVER_SS		(1 << 8)
#define ILI9341_DRIVER_SM		(1 << 10)

#define ILI9341_DRIVEWAVE_EOR		(1 << 8)
#define ILI9341_DRIVEWAVE_BC		(1 << 9)
#define ILI9341_DRIVEWAVE_MUSTSET	(1 << 10)

#define ILI9341_ENTRYMODE_AM		(1 << 3)
#define ILI9341_ENTRYMODE_ID(x)		((x) << 4)
#define ILI9341_ENTRYMODE_ORG		(1 << 7)
#define ILI9341_ENTRYMODE_HWM		(1 << 8)
#define ILI9341_ENTRYMODE_BGR		(1 << 12)
#define ILI9341_ENTRYMODE_DFM		(1 << 14)
#define ILI9341_ENTRYMODE_TRI		(1 << 15)


#define ILI9341_RESIZING_RSZ(x)		((x) << 0)
#define ILI9341_RESIZING_RCH(x)		((x) << 4)
#define ILI9341_RESIZING_RCV(x)		((x) << 8)


#define ILI9341_DISPLAY1_D(x)		((x) << 0)
#define ILI9341_DISPLAY1_CL		(1 << 3)
#define ILI9341_DISPLAY1_DTE		(1 << 4)
#define ILI9341_DISPLAY1_GON		(1 << 5)
#define ILI9341_DISPLAY1_BASEE		(1 << 8)
#define ILI9341_DISPLAY1_PTDE(x)	((x) << 12)


#define ILI9341_DISPLAY2_BP(x)		((x) << 0)
#define ILI9341_DISPLAY2_FP(x)		((x) << 8)


#define ILI9341_RGBIF1_RIM_RGB18	(0 << 0)
#define ILI9341_RGBIF1_RIM_RGB16	(1 << 0)
#define ILI9341_RGBIF1_RIM_RGB6		(2 << 0)

#define ILI9341_RGBIF1_CLK_INT		(0 << 4)
#define ILI9341_RGBIF1_CLK_RGBIF	(1 << 4)
#define ILI9341_RGBIF1_CLK_VSYNC	(2 << 4)

#define ILI9341_RGBIF1_RM		(1 << 8)

#define ILI9341_RGBIF1_ENC_FRAMES(x)	(((x) - 1)<< 13)

#define ILI9341_RGBIF2_DPL		(1 << 0)
#define ILI9341_RGBIF2_EPL		(1 << 1)
#define ILI9341_RGBIF2_HSPL		(1 << 3)
#define ILI9341_RGBIF2_VSPL		(1 << 4)


#define ILI9341_POWER1_SLP		(1 << 1)
#define ILI9341_POWER1_DSTB		(1 << 2)
#define ILI9341_POWER1_AP(x)		((x) << 4)
#define ILI9341_POWER1_APE		(1 << 7)
#define ILI9341_POWER1_BT(x)		((x) << 8)
#define ILI9341_POWER1_SAP		(1 << 12)


#define ILI9341_POWER2_VC(x)		((x) << 0)
#define ILI9341_POWER2_DC0(x)		((x) << 4)
#define ILI9341_POWER2_DC1(x)		((x) << 8)


#define ILI9341_POWER3_VRH(x)		((x) << 0)
#define ILI9341_POWER3_PON		(1 << 4)
#define ILI9341_POWER3_VCMR		(1 << 8)


#define ILI9341_POWER4_VREOUT(x)	((x) << 8)


#define ILI9341_DRIVER2_SCNL(x)		((x) << 0)
#define ILI9341_DRIVER2_NL(x)		((x) << 8)
#define ILI9341_DRIVER2_GS		(1 << 15)


#define ILI9341_BASEIMAGE_REV		(1 << 0)
#define ILI9341_BASEIMAGE_VLE		(1 << 1)
#define ILI9341_BASEIMAGE_NDL		(1 << 2)


#define ILI9341_INTERFACE4_RTNE(x)	(x)
#define ILI9341_INTERFACE4_DIVE(x)	((x) << 8)

/* SPI interface definitions */

#define ILI9341_SPI_IDCODE		(0x70)
#define ILI9341_SPI_ID(x)		((x) << 2)
#define ILI9341_SPI_READ		(0x01)
#define ILI9341_SPI_WRITE		(0x00)
#define ILI9341_SPI_DATA		(0x02)
#define ILI9341_SPI_INDEX		(0x00)

/* platform data to pass configuration from lcd */

enum ili9341_suspend {
	ILI9341_SUSPEND_OFF,
	ILI9341_SUSPEND_DEEP,
};

struct ili9341_platdata {
	unsigned short	hsize;
	unsigned short	vsize;

	enum ili9341_suspend suspend;

	/* set the reset line, 0 = reset asserted, 1 = normal */
	void		(*reset)(unsigned int val);

	unsigned short	entry_mode;
	unsigned short	display2;
	unsigned short	display3;
	unsigned short	display4;
	unsigned short	rgb_if1;
	unsigned short	rgb_if2;
	unsigned short	interface2;
	unsigned short	interface3;
	unsigned short	interface4;
	unsigned short	interface5;
	unsigned short	interface6;
};

