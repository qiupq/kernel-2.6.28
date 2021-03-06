/*
 * drivers/video/s3c/s3cfb_hsd050.c
 *
 * $Id: s3cfb_wx4300f.c,v 1.12 2009/09/13 02:13:24 
 *
 * Copyright (C) 2009 Figo Wang <sagres_2004@163.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *	S3C Frame Buffer Driver
 *	based on skeletonfb.c, sa1100fb.h, s3c2410fb.c
 */

#include <linux/wait.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/platform_device.h>

#include <plat/regs-gpio.h>
#include <plat/regs-lcd.h>

#include "s3cfb.h"

#define S3CFB_HFP		16	/* front porch */
#define S3CFB_HSW		10	/* hsync width */
#define S3CFB_HBP		144	/* back porch */

#define S3CFB_VFP		32	/* front porch */
#define S3CFB_VSW		2	/* vsync width */
#define S3CFB_VBP		13	/* back porch */

#define S3CFB_HRES		640	/* horizon pixel  x resolition */
#define S3CFB_VRES		480	/* line cnt       y resolution */

#define S3CFB_HRES_VIRTUAL	     640	/* horizon pixel  x resolition */
#define S3CFB_VRES_VIRTUAL 	480*2	/* line cnt       y resolution */

#define S3CFB_HRES_OSD		640	/* horizon pixel  x resolition */
#define S3CFB_VRES_OSD		480	/* line cnt       y resolution */

#define S3CFB_VFRAME_FREQ     	60	/* frame rate freq */

#define S3CFB_PIXEL_CLOCK	(S3CFB_VFRAME_FREQ * (S3CFB_HFP + S3CFB_HSW + S3CFB_HBP + S3CFB_HRES) * (S3CFB_VFP + S3CFB_VSW + S3CFB_VBP + S3CFB_VRES))
/*
        pDeviceInfo->VBPD_Value = 29;
        pDeviceInfo->VFPD_Value = 13;
        pDeviceInfo->VSPW_Value = 3;
        pDeviceInfo->HBPD_Value = 40;
        pDeviceInfo->HFPD_Value = 40;
        pDeviceInfo->HSPW_Value = 48;
        pDeviceInfo->VCLK_Polarity = IVCLK_FALL_EDGE;
        pDeviceInfo->HSYNC_Polarity = IHSYNC_HIGH_ACTIVE;
        pDeviceInfo->VSYNC_Polarity = IVSYNC_HIGH_ACTIVE;
        pDeviceInfo->VDEN_Polarity = IVDEN_HIGH_ACTIVE;
        pDeviceInfo->PNR_Mode = PNRMODE_RGB_P;
        pDeviceInfo->VCLK_Source = CLKSEL_F_LCDCLK;
        pDeviceInfo->VCLK_Direction = CLKDIR_DIVIDED;
*/
static void s3cfb_set_fimd_info(void)
{
	s3cfb_fimd.vidcon1 =  S3C_VIDCON1_IHSYNC_INVERT | S3C_VIDCON1_IVSYNC_INVERT | S3C_VIDCON1_IVDEN_NORMAL | S3C_VIDCON1_IVCLK_FALL_EDGE;
	s3cfb_fimd.vidtcon0 = S3C_VIDTCON0_VBPD(S3CFB_VBP - 1) | S3C_VIDTCON0_VFPD(S3CFB_VFP - 1) | S3C_VIDTCON0_VSPW(S3CFB_VSW - 1);
	s3cfb_fimd.vidtcon1 = S3C_VIDTCON1_HBPD(S3CFB_HBP - 1) | S3C_VIDTCON1_HFPD(S3CFB_HFP - 1) | S3C_VIDTCON1_HSPW(S3CFB_HSW - 1);
	s3cfb_fimd.vidtcon2 = S3C_VIDTCON2_LINEVAL(S3CFB_VRES - 1) | S3C_VIDTCON2_HOZVAL(S3CFB_HRES - 1);
	s3cfb_fimd.vidosd0a = S3C_VIDOSDxA_OSD_LTX_F(0) | S3C_VIDOSDxA_OSD_LTY_F(0);
	s3cfb_fimd.vidosd0b = S3C_VIDOSDxB_OSD_RBX_F(S3CFB_HRES - 1) | S3C_VIDOSDxB_OSD_RBY_F(S3CFB_VRES - 1);
	s3cfb_fimd.vidosd1a = S3C_VIDOSDxA_OSD_LTX_F(0) | S3C_VIDOSDxA_OSD_LTY_F(0);
	s3cfb_fimd.vidosd1b = S3C_VIDOSDxB_OSD_RBX_F(S3CFB_HRES_OSD - 1) | S3C_VIDOSDxB_OSD_RBY_F(S3CFB_VRES_OSD - 1);
	s3cfb_fimd.width = S3CFB_HRES;
	s3cfb_fimd.height = S3CFB_VRES;
	s3cfb_fimd.xres = S3CFB_HRES;
	s3cfb_fimd.yres = S3CFB_VRES;
#if defined(CONFIG_FB_S3C_VIRTUAL_SCREEN)
	s3cfb_fimd.xres_virtual = S3CFB_HRES_VIRTUAL;
	s3cfb_fimd.yres_virtual = S3CFB_VRES_VIRTUAL;
#else
	s3cfb_fimd.xres_virtual = S3CFB_HRES;
	s3cfb_fimd.yres_virtual = S3CFB_VRES;
#endif
	s3cfb_fimd.osd_width = S3CFB_HRES_OSD;
	s3cfb_fimd.osd_height = S3CFB_VRES_OSD;
	s3cfb_fimd.osd_xres = S3CFB_HRES_OSD;
	s3cfb_fimd.osd_yres = S3CFB_VRES_OSD;
	s3cfb_fimd.osd_xres_virtual = S3CFB_HRES_OSD;
	s3cfb_fimd.osd_yres_virtual = S3CFB_VRES_OSD;
	s3cfb_fimd.pixclock = S3CFB_PIXEL_CLOCK;
	s3cfb_fimd.hsync_len = S3CFB_HSW;
	s3cfb_fimd.vsync_len = S3CFB_VSW;
	s3cfb_fimd.left_margin = S3CFB_HFP;
	s3cfb_fimd.upper_margin = S3CFB_VFP;
	s3cfb_fimd.right_margin = S3CFB_HBP;
	s3cfb_fimd.lower_margin = S3CFB_VBP;
}

void s3cfb_init_hw(void)
{
	printk(KERN_INFO "LCD TYPE :: AT056TN52 will be initialized\n");
	s3cfb_set_fimd_info();
	s3cfb_set_gpio();
}
