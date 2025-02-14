// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * This file is part of libnvme.
 * Copyright (c) 2021 Code Construct Pty Ltd
 *
 * Authors: Jeremy Kerr <jk@codeconstruct.com.au>
 */
#ifndef _LIBNVME_MI_MI_H
#define _LIBNVME_MI_MI_H

#include <endian.h>
#include <stdint.h>

#include "types.h"
#include "tree.h"

/* Message type; this is defined by MCTP, but is referenced as part of the
 * NVMe-MI message spec. This is the MCTP NVMe message type (0x4), with
 * the message-integrity bit (0x80) set.
 */
#define NVME_MI_MSGTYPE_NVME 0x84

/* Basic message definitions */
enum nvme_mi_message_type {
	NVME_MI_MT_CONTROL = 0,
	NVME_MI_MT_MI = 1,
	NVME_MI_MT_ADMIN = 2,
	NVME_MI_MT_PCIE = 4,
};

enum nvme_mi_ror {
	NVME_MI_ROR_REQ = 0,
	NVME_MI_ROR_RSP = 1,
};

struct nvme_mi_msg_hdr {
	__u8	type;
	__u8	nmp;
	__u8	meb;
	__u8	rsvd0;
} __attribute__((packed));

/* MI command definitions */
enum nvme_mi_mi_opcode {
	nvme_mi_mi_opcode_mi_data_read = 0x00,
	nvme_mi_mi_opcode_subsys_health_status_poll = 0x01,
};

struct nvme_mi_mi_req_hdr {
	struct nvme_mi_msg_hdr hdr;
	__u8	opcode;
	__u8	rsvd0[3];
	__le32	cdw0, cdw1;
};

struct nvme_mi_mi_resp_hdr {
	struct nvme_mi_msg_hdr hdr;
	__u8	status;
	__u8	nmresp[3];
};

enum nvme_mi_dtyp {
	nvme_mi_dtyp_subsys_info = 0x00,
	nvme_mi_dtyp_port_info = 0x01,
	nvme_mi_dtyp_ctrl_list = 0x02,
	nvme_mi_dtyp_ctrl_info = 0x03,
	nvme_mi_dtyp_opt_cmd_support = 0x04,
	nvme_mi_dtyp_meb_support = 0x05,
};

/* Admin command definitions */

struct nvme_mi_admin_req_hdr {
	struct nvme_mi_msg_hdr hdr;
	__u8	opcode;
	__u8	flags;
	__le16	ctrl_id;
	__le32	cdw1, cdw2, cdw3, cdw4, cdw5;
	__le32	doff;
	__le32	dlen;
	__le32	rsvd0, rsvd1;
	__le32	cdw10, cdw11, cdw12, cdw13, cdw14, cdw15;
} __attribute((packed));

struct nvme_mi_admin_resp_hdr {
	struct nvme_mi_msg_hdr hdr;
	__u8	status;
	__u8	rsvd0[3];
	__le32	cdw0, cdw1, cdw3;
} __attribute__((packed));

/* MI Command API */

/* library-level API object */
nvme_root_t nvme_mi_create_root(FILE *fp, int log_level);
void nvme_mi_free_root(nvme_root_t);

/* Top level management object: NVMe-MI Management Endpoint */
struct nvme_mi_ep;
typedef struct nvme_mi_ep *nvme_mi_ep_t;

/* An endpoint may expose multiple controllers */
struct nvme_mi_ctrl;
typedef struct nvme_mi_ctrl *nvme_mi_ctrl_t;

/* Transport-specific endpoint initialisation. Once an endpoint is created,
 * the rest of the API is transport-independent. */
nvme_mi_ep_t nvme_mi_open_mctp(nvme_root_t root, unsigned int netid, uint8_t eid);
void nvme_mi_close(nvme_mi_ep_t ep);

nvme_mi_ctrl_t nvme_mi_init_ctrl(nvme_mi_ep_t ep, __u16 ctrl_id);
void nvme_mi_close_ctrl(nvme_mi_ctrl_t ctrl);

/* Management Interface functions; nvme_mi_mi_ prefix. */
int nvme_mi_mi_read_mi_data_subsys(nvme_mi_ep_t ep,
				   struct nvme_mi_read_nvm_ss_info *s);
int nvme_mi_mi_read_mi_data_port(nvme_mi_ep_t ep, __u8 portid,
				 struct nvme_mi_read_port_info *p);
int nvme_mi_mi_read_mi_data_ctrl_list(nvme_mi_ep_t ep, __u8 start_portid,
				      struct nvme_ctrl_list *list);
int nvme_mi_mi_read_mi_data_ctrl(nvme_mi_ep_t ep, __u16 ctrl_id,
				 struct nvme_mi_read_ctrl_info *ctrl);
int nvme_mi_mi_subsystem_health_status_poll(nvme_mi_ep_t ep, bool clear,
					    struct nvme_mi_nvm_ss_health_status *nshds);

/* Admin channel functions */

/* "raw" admin transfer. req_data_size and resp_data_size are the sizes of
 * the data portion of the payload, so do not include the length of
 * the header, and start at 0 for no payload.
 */
int nvme_mi_admin_xfer(nvme_mi_ctrl_t ctrl,
		       struct nvme_mi_admin_req_hdr *admin_req,
		       size_t req_data_size,
		       struct nvme_mi_admin_resp_hdr *admin_resp,
		       off_t resp_data_offset,
		       size_t *resp_data_size);

int nvme_mi_admin_identify_partial(nvme_mi_ctrl_t ctrl,
				   struct nvme_identify_args *args,
				   off_t offset, size_t size);

/* Helpers for identify commands */
static inline int nvme_mi_admin_identify(nvme_mi_ctrl_t ctrl,
					 struct nvme_identify_args *args)
{
	return nvme_mi_admin_identify_partial(ctrl, args,
					      0, NVME_IDENTIFY_DATA_SIZE);
}

static inline int nvme_mi_admin_identify_cns_nsid(nvme_mi_ctrl_t ctrl,
						  enum nvme_identify_cns cns,
						  __u32 nsid, void *data)
{
	struct nvme_identify_args args = {
		.result = NULL,
		.data = data,
		.args_size = sizeof(args),
		.cns = cns,
		.csi = NVME_CSI_NVM,
		.nsid = nsid,
		.cntid = NVME_CNTLID_NONE,
		.cns_specific_id = NVME_CNSSPECID_NONE,
		.uuidx = NVME_UUID_NONE,
	};

	return nvme_mi_admin_identify(ctrl, &args);
}

static inline int nvme_mi_admin_identify_ctrl(nvme_mi_ctrl_t ctrl,
					      struct nvme_id_ctrl *id)
{
	return nvme_mi_admin_identify_cns_nsid(ctrl, NVME_IDENTIFY_CNS_CTRL,
					       NVME_NSID_NONE, id);
}

static inline int nvme_mi_admin_identify_ctrl_list(nvme_mi_ctrl_t ctrl,
						   __u16 cntid,
						   struct nvme_ctrl_list *list)
{
	struct nvme_identify_args args = {
		.result = NULL,
		.data = list,
		.args_size = sizeof(args),
		.cns = NVME_IDENTIFY_CNS_CTRL_LIST,
		.csi = NVME_CSI_NVM,
		.nsid = NVME_NSID_NONE,
		.cntid = cntid,
		.cns_specific_id = NVME_CNSSPECID_NONE,
		.uuidx = NVME_UUID_NONE,
	};

	return nvme_mi_admin_identify(ctrl, &args);
}

/**
 * Retreives log page data as specified by @args. On return, @args->len is
 * updated to the amount of log data written to @args->log.
 *
 * This request may be implemented as multiple log page commands, in order
 * to fit within MI message-size limits.
 *
 * @ctrl:	MI controller
 * @args:	Log page request parameters
 */
int nvme_mi_admin_get_log_page(nvme_mi_ctrl_t ctrl,
			       struct nvme_get_log_args *args);

int nvme_mi_admin_security_send(nvme_mi_ctrl_t ctrl,
				struct nvme_security_send_args *args);

int nvme_mi_admin_security_recv(nvme_mi_ctrl_t ctrl,
				struct nvme_security_receive_args *args);


#endif /* _LIBNVME_MI_MI_H */
