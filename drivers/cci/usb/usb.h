/* driver/cci/lightsensors/lightsensor.h
 *
 * Copyright (C) 2009 CCI, Inc.
 * Author:	Ahung.Cheng
 * Date:	2009.3.16
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

#ifndef _MSM_CCI_LS_H
#define _MSM_CCI_LS_H

#define CCIPROG	0x30001000
#define CCIVERS	0x00010001

#define MSM_SHUTDOWN_LIGHT_SENSOR_GPIO  32

//#define ONCRPC_CCI_REGISTER_LIGHT_SENSOR_PROC 7
#define ONCRPC_CCI_READ_LIGHT_SENSOR 27

#define LIGHTSENSOR_POWER_ON 0
#define LIGHTSENSOR_POWER_OFF 1

struct cci_rpc_lig_sen_req {
	struct rpc_request_hdr hdr;
};

struct cci_rpc_lig_sen_rep {
	struct rpc_reply_hdr hdr;
	uint32_t err_flag;
};	

#endif //_MSM_CCI_LS_H
