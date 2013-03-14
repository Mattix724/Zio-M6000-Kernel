/*
 * Copyright (C) 2009-2010 CCI Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#ifndef OV3640_H
#define OV3640_H

#include <mach/board.h>
#include <mach/camera.h>

extern struct ov3640_reg_t ov3640_regs;
extern struct register_zoom_ae_t ov3640_zoom_ae;

enum ov3640_width_t {
	WORD_LEN,
	BYTE_LEN
};
#if 0
struct ov3640_i2c_reg_conf {
	unsigned short waddr;
	unsigned short wdata;
	enum ov3640_width_t width;
	unsigned short mdelay_time;
};
#endif

struct ov3640_reg_t {

	struct register_address_value_pair const *init_reg_settings;
	uint16_t init_reg_settings_size;
	struct register_address_value_pair const *AF_reg_settings;
	uint16_t AF_reg_settings_size;
	struct register_address_value_pair const *PV_reg_settings;
	uint16_t PV_reg_settings_size;
	struct register_address_value_pair const *CP_reg_settings;
	uint16_t CP_reg_settings_size;
	struct register_address_value_pair const *MV_reg_settings;
	uint16_t MV_reg_settings_size;
	unsigned char *NEW_AF_settings;
	uint16_t NEW_AF_settings_size;
	/*B: CCI, 20100809, For new device DW7914 AF setting */
	unsigned char *NEW_AF_settings_DW9714;
	uint16_t NEW_AF_settings_size_DW9714;
	/*E: CCI, 20100809, For new device DW7914 AF setting */

	#if 0
	struct register_address_value_pair_t const
	  *noise_reduction_reg_settings;
	uint16_t noise_reduction_reg_settings_size;
	struct ov3640_i2c_reg_conf const *plltbl;
	uint16_t plltbl_size;
	struct ov3640_i2c_reg_conf const *stbl;
	uint16_t stbl_size;
	struct ov3640_i2c_reg_conf const *rftbl;
	uint16_t rftbl_size;
	#endif
};

struct register_value_cord {
	uint16_t start_x;
	uint16_t start_y;
	uint16_t size_x;
	uint16_t size_y;	
};

struct register_zoom_ae_t {
	struct register_value_cord const *zoom_ae_settings;
	uint16_t zoom_ae_settings_size;
};


#endif /* OV3640_H */
