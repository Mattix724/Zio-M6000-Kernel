/*
 *
 * Copyright (C) 2009 CCI, Inc.
 * Author:	Erix Cheng
 * Date:	2009.8.14
 *
 */
typedef unsigned long rpc_uint32;

#define CCIPROG         0x30001000
#define CCIVERS          0x10001

#define ONCRPC_CCI_VIB 26
#define ONCRPC_CCI_VIB_ON 28 
#define ONCRPC_CCI_VIB_OFF 29

struct cci_rpc_vib_on_req {
    struct rpc_request_hdr hdr;
    u32 duration;
};

struct cci_rpc_vib_off_req {
    struct rpc_request_hdr hdr;
};
