/* Copyright (c) 2008-2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef __ARCH_ARM_MACH_MSM_MPP_H
#define __ARCH_ARM_MACH_MSM_MPP_H

#include <mach/msm_rpcrouter.h>
struct mpp {
	const char *name;
	unsigned id;
	int status;
};

#ifdef CONFIG_PMIC8058
#define	MPPS		12
#else
#define	MPPS		22
#endif

/* Digital Logical Output Level */
enum {
	MPP_DLOGIC_LVL_MSME,
	MPP_DLOGIC_LVL_MSMP,
	MPP_DLOGIC_LVL_RUIM,
	MPP_DLOGIC_LVL_MMC,
	MPP_DLOGIC_LVL_VDD,
};

/* Digital Logical Output Control Value */
enum {
	MPP_DLOGIC_OUT_CTRL_LOW,
	MPP_DLOGIC_OUT_CTRL_HIGH,
	MPP_DLOGIC_OUT_CTRL_MPP,	/* MPP Output = MPP Input */
	MPP_DLOGIC_OUT_CTRL_NOT_MPP,	/* MPP Output = Inverted MPP Input */
};

enum
{
	PM_MPP__DLOGIC_IN__DBUS_NONE,
	PM_MPP__DLOGIC_IN__DBUS1,
	PM_MPP__DLOGIC_IN__DBUS2,
	PM_MPP__DLOGIC_IN__DBUS3,
	PM_MPP__DLOGIC_IN__DBUS_INVALID
};

enum
{
	PM_MPP__DLOGIC__LVL_MSME,
	PM_MPP__DLOGIC__LVL_MSMP,
	PM_MPP__DLOGIC__LVL_RUIM,
	PM_MPP__DLOGIC__LVL_MMC,
	PM_MPP__DLOGIC__LVL_VDD,
	PM_MPP__DLOGIC__LVL_INVALID
};
/* I Sink Level Type */
enum {
	  PM_MPP__I_SINK__LEVEL_5mA,
          PM_MPP__I_SINK__LEVEL_10mA,
          PM_MPP__I_SINK__LEVEL_15mA,
          PM_MPP__I_SINK__LEVEL_20mA,
          PM_MPP__I_SINK__LEVEL_25mA,
          PM_MPP__I_SINK__LEVEL_30mA,
          PM_MPP__I_SINK__LEVEL_35mA,
          PM_MPP__I_SINK__LEVEL_40mA,
          PM_MPP__I_SINK__LEVEL_INVALID
};

/* I Sink Switch type */
enum {

  PM_MPP__I_SINK__SWITCH_DIS,
  PM_MPP__I_SINK__SWITCH_ENA,
  PM_MPP__I_SINK__SWITCH_ENA_IF_MPP_HIGH,
  PM_MPP__I_SINK__SWITCH_ENA_IF_MPP_LOW,
  PM_MPP__I_SINK__SWITCH_INVALID
};

/* LED State Blink type */
enum {
  PM_MPP__LED_STATE__BLINK_QUICK,
  PM_MPP__LED_STATE__BLINK_SLOW,
  PM_MPP__LED_STATE__BLINK_SOLID,
  PM_MPP__LED_STATE__BLINK_OFF,
  PM_MPP__LED_STATE__BLINK_INVALID
};

/* LED State Color type */
enum {
  PM_MPP__LED_STATE__COLOR_RED,
  PM_MPP__LED_STATE__COLOR_GREEN,
  PM_MPP__LED_STATE__COLOR_INVALID
};

struct mpp *mpp_get(struct device *dev, const char *id);
int  rpc_mpp_config_i_sink(struct mpp *mpp,  int level,  int onoff); //  wayne add
int  rpc_mpp_config_led_state(int blink,  int color); //  wayne add
int rpc_mpp_config_digital_input(struct mpp *mpp, int level,  int dbus);
//wayne added for
#define CCIPROG 0x30001000
#define CCIVERS 0x00010001
//
#define ONCRPC_PM_MPP_CONFIG_DIGITAL_INPUT 4
#define ONCRPC_PM_MPP_CONFIG_I_SINK 6
#define ONCRPC_PM_MPP_CONFIG_LED_INDI 31
//

struct cci_rpc_mpp_id_req {
    struct rpc_request_hdr hdr;
    unsigned mpp_id;
    int level;
    int dbus;
};

struct rpc_reply_hwid_reply {
	struct rpc_reply_hdr hdr;
	int hwid_data;
};

struct cci_rpc_mpp_sen_req {
    struct rpc_request_hdr hdr;
    unsigned mpp_id;
    int level;
    int control;
};

struct cci_rpc_mpp_led_req {
    struct rpc_request_hdr hdr;
    int blink;
    int color;
};

/* Digital Logical Input Value */
enum {
	MPP_DLOGIC_IN_DBUS_NONE,
	MPP_DLOGIC_IN_DBUS_1,
	MPP_DLOGIC_IN_DBUS_2,
	MPP_DLOGIC_IN_DBUS_3,
};

#define MPP_CFG(level, control) ((((level) & 0x0FFFF) << 16) | \
				 ((control) & 0x0FFFF))
#define MPP_CFG_INPUT(level, dbus) ((((level) & 0x0FFFF) << 16) | \
				 ((dbus) & 0x0FFFF))

/* Use mpp number starting from 0 */
int mpp_config_digital_out(unsigned mpp, unsigned config);
int mpp_config_digital_in(unsigned mpp, unsigned config);

/* PM8058/PM8901 definitions */

/* APIs */
#ifdef CONFIG_PMIC8058
int pm8058_mpp_config(unsigned mpp, unsigned type, unsigned level,
		      unsigned control);
#else
static inline int pm8058_mpp_config(unsigned mpp, unsigned type,
				    unsigned level, unsigned control)
{
	return -EINVAL;
}
#endif

#ifdef CONFIG_PMIC8901
int pm8901_mpp_config(unsigned mpp, unsigned type, unsigned level,
		      unsigned control);
#else
static inline int pm8901_mpp_config(unsigned mpp, unsigned type,
				    unsigned level, unsigned control)
{
	return -EINVAL;
}
#endif

/* MPP Type: type */
#define	PM_MPP_TYPE_D_INPUT		0
#define	PM_MPP_TYPE_D_OUTPUT		1
#define	PM_MPP_TYPE_D_BI_DIR		2
#define	PM_MPP_TYPE_A_INPUT		3
#define	PM_MPP_TYPE_A_OUTPUT		4
#define	PM_MPP_TYPE_SINK		5
#define	PM_MPP_TYPE_DTEST_SINK		6
#define	PM_MPP_TYPE_DTEST_OUTPUT	7


/* Digital Input/Output: level [8058] */
#define	PM8058_MPP_DIG_LEVEL_VPH	0
#define	PM8058_MPP_DIG_LEVEL_S3		1
#define	PM8058_MPP_DIG_LEVEL_L2		2
#define	PM8058_MPP_DIG_LEVEL_L3		3

/* Digital Input/Output: level [8901] */
#define	PM8901_MPP_DIG_LEVEL_MSMIO	0
#define	PM8901_MPP_DIG_LEVEL_DIG	1
#define	PM8901_MPP_DIG_LEVEL_L5		2
#define	PM8901_MPP_DIG_LEVEL_S4		3
#define	PM8901_MPP_DIG_LEVEL_VPH	4

/* Digital Input: control */
#define	PM_MPP_DIN_TO_INT		0
#define	PM_MPP_DIN_TO_DBUS1		1
#define	PM_MPP_DIN_TO_DBUS2		2
#define	PM_MPP_DIN_TO_DBUS3		3

/* Digital Output: control */
#define	PM_MPP_DOUT_CTL_LOW		0
#define	PM_MPP_DOUT_CTL_HIGH		1
#define	PM_MPP_DOUT_CTL_MPP		2
#define	PM_MPP_DOUT_CTL_INV_MPP		3

/* Bidirectional: control */
#define	PM_MPP_BI_PULLUP_1KOHM		0
#define	PM_MPP_BI_PULLUP_OPEN		1
#define	PM_MPP_BI_PULLUP_10KOHM		2
#define	PM_MPP_BI_PULLUP_30KOHM		3

/* Analog Input: level */
#define	PM_MPP_AIN_AMUX_CH5		0
#define	PM_MPP_AIN_AMUX_CH6		1
#define	PM_MPP_AIN_AMUX_CH7		2
#define	PM_MPP_AIN_AMUX_CH8		3
#define	PM_MPP_AIN_AMUX_CH9		4
#define	PM_MPP_AIN_AMUX_ABUS1		5
#define	PM_MPP_AIN_AMUX_ABUS2		6
#define	PM_MPP_AIN_AMUX_ABUS3		7

/* Analog Output: level */
#define	PM_MPP_AOUT_LVL_1V25		0
#define	PM_MPP_AOUT_LVL_1V25_2		1
#define	PM_MPP_AOUT_LVL_0V625		2
#define	PM_MPP_AOUT_LVL_0V3125		3
#define	PM_MPP_AOUT_LVL_MPP		4
#define	PM_MPP_AOUT_LVL_ABUS1		5
#define	PM_MPP_AOUT_LVL_ABUS2		6
#define	PM_MPP_AOUT_LVL_ABUS3		7

/* Analog Output: control */
#define	PM_MPP_AOUT_CTL_DISABLE		0
#define	PM_MPP_AOUT_CTL_ENABLE		1
#define	PM_MPP_AOUT_CTL_MPP_HIGH_EN	2
#define	PM_MPP_AOUT_CTL_MPP_LOW_EN	3

/* Current Sink: level */
#define	PM_MPP_CS_OUT_5MA		0
#define	PM_MPP_CS_OUT_10MA		1
#define	PM_MPP_CS_OUT_15MA		2
#define	PM_MPP_CS_OUT_20MA		3
#define	PM_MPP_CS_OUT_25MA		4
#define	PM_MPP_CS_OUT_30MA		5
#define	PM_MPP_CS_OUT_35MA		6
#define	PM_MPP_CS_OUT_40MA		7

/* Current Sink: control */
#define	PM_MPP_CS_CTL_DISABLE		0
#define	PM_MPP_CS_CTL_ENABLE		1
#define	PM_MPP_CS_CTL_MPP_HIGH_EN	2
#define	PM_MPP_CS_CTL_MPP_LOW_EN	3

/* DTEST Current Sink: control */
#define	PM_MPP_DTEST_CS_CTL_EN1		0
#define	PM_MPP_DTEST_CS_CTL_EN2		1
#define	PM_MPP_DTEST_CS_CTL_EN3		2
#define	PM_MPP_DTEST_CS_CTL_EN4		3

/* DTEST Digital Output: control */
#define	PM_MPP_DTEST_DBUS1		0
#define	PM_MPP_DTEST_DBUS2		1
#define	PM_MPP_DTEST_DBUS3		2
#define	PM_MPP_DTEST_DBUS4		3

/* Helper APIs */
static inline int pm8058_mpp_config_digital_in(unsigned mpp, unsigned level,
					       unsigned control)
{
	return pm8058_mpp_config(mpp, PM_MPP_TYPE_D_INPUT, level, control);
}

static inline int pm8058_mpp_config_digital_out(unsigned mpp, unsigned level,
						unsigned control)
{
	return pm8058_mpp_config(mpp, PM_MPP_TYPE_D_OUTPUT, level, control);
}

static inline int pm8058_mpp_config_bi_dir(unsigned mpp, unsigned level,
					   unsigned control)
{
	return pm8058_mpp_config(mpp, PM_MPP_TYPE_D_BI_DIR, level, control);
}

static inline int pm8058_mpp_config_analog_input(unsigned mpp, unsigned level,
						 unsigned control)
{
	return pm8058_mpp_config(mpp, PM_MPP_TYPE_A_INPUT, level, control);
}

static inline int pm8058_mpp_config_analog_output(unsigned mpp, unsigned level,
						  unsigned control)
{
	return pm8058_mpp_config(mpp, PM_MPP_TYPE_A_OUTPUT, level, control);
}

static inline int pm8058_mpp_config_current_sink(unsigned mpp, unsigned level,
						 unsigned control)
{
	return pm8058_mpp_config(mpp, PM_MPP_TYPE_SINK, level, control);
}

static inline int pm8058_mpp_config_dtest_sink(unsigned mpp, unsigned level,
					       unsigned control)
{
	return pm8058_mpp_config(mpp, PM_MPP_TYPE_DTEST_SINK, level, control);
}

static inline int pm8058_mpp_config_dtest_output(unsigned mpp, unsigned level,
						 unsigned control)
{
	return pm8058_mpp_config(mpp, PM_MPP_TYPE_DTEST_OUTPUT,
				 level, control);
}

static inline int pm8901_mpp_config_digital_in(unsigned mpp, unsigned level,
					       unsigned control)
{
	return pm8901_mpp_config(mpp, PM_MPP_TYPE_D_INPUT, level, control);
}

static inline int pm8901_mpp_config_digital_out(unsigned mpp, unsigned level,
						unsigned control)
{
	return pm8901_mpp_config(mpp, PM_MPP_TYPE_D_OUTPUT, level, control);
}

static inline int pm8901_mpp_config_bi_dir(unsigned mpp, unsigned level,
					   unsigned control)
{
	return pm8901_mpp_config(mpp, PM_MPP_TYPE_D_BI_DIR, level, control);
}

static inline int pm8901_mpp_config_analog_input(unsigned mpp, unsigned level,
						 unsigned control)
{
	return pm8901_mpp_config(mpp, PM_MPP_TYPE_A_INPUT, level, control);
}

static inline int pm8901_mpp_config_analog_output(unsigned mpp, unsigned level,
						  unsigned control)
{
	return pm8901_mpp_config(mpp, PM_MPP_TYPE_A_OUTPUT, level, control);
}

static inline int pm8901_mpp_config_current_sink(unsigned mpp, unsigned level,
						 unsigned control)
{
	return pm8901_mpp_config(mpp, PM_MPP_TYPE_SINK, level, control);
}

static inline int pm8901_mpp_config_dtest_sink(unsigned mpp, unsigned level,
					       unsigned control)
{
	return pm8901_mpp_config(mpp, PM_MPP_TYPE_DTEST_SINK, level, control);
}

static inline int pm8901_mpp_config_dtest_output(unsigned mpp, unsigned level,
						 unsigned control)
{
	return pm8901_mpp_config(mpp, PM_MPP_TYPE_DTEST_OUTPUT,
				 level, control);
}
#endif
