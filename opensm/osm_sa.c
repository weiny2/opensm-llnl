/*
 * Copyright (c) 2004-2009 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2012 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 * Copyright (c) 2008 Xsigo Systems Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

/*
 * Abstract:
 *    Implementation of osm_sa_t.
 * This object represents the Subnet Administration object.
 * This object is part of the opensm family of objects.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <complib/cl_qmap.h>
#include <complib/cl_passivelock.h>
#include <complib/cl_debug.h>
#include <iba/ib_types.h>
#include <opensm/osm_file_ids.h>
#define FILE_ID OSM_FILE_SA_C
#include <opensm/osm_sa.h>
#include <opensm/osm_madw.h>
#include <opensm/osm_log.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_mad_pool.h>
#include <opensm/osm_msgdef.h>
#include <opensm/osm_opensm.h>
#include <opensm/osm_multicast.h>
#include <opensm/osm_inform.h>
#include <opensm/osm_service.h>
#include <opensm/osm_guid.h>
#include <opensm/osm_helper.h>
#include <vendor/osm_vendor_api.h>
#include <sys/time.h>

#define  OSM_SA_INITIAL_TID_VALUE 0xabc

extern void osm_cpi_rcv_process(IN void *context, IN void *data);
extern void osm_gir_rcv_process(IN void *context, IN void *data);
extern void osm_infr_rcv_process(IN void *context, IN void *data);
extern void osm_infir_rcv_process(IN void *context, IN void *data);
extern void osm_lftr_rcv_process(IN void *context, IN void *data);
extern void osm_lr_rcv_process(IN void *context, IN void *data);
extern void osm_mcmr_rcv_process(IN void *context, IN void *data);
extern void osm_mftr_rcv_process(IN void *context, IN void *data);
extern void osm_mpr_rcv_process(IN void *context, IN void *data);
extern void osm_nr_rcv_process(IN void *context, IN void *data);
extern void osm_pr_rcv_process(IN void *context, IN void *data);
extern void osm_pkey_rec_rcv_process(IN void *context, IN void *data);
extern void osm_pir_rcv_process(IN void *context, IN void *data);
extern void osm_sr_rcv_process(IN void *context, IN void *data);
extern void osm_slvl_rec_rcv_process(IN void *context, IN void *data);
extern void osm_smir_rcv_process(IN void *context, IN void *data);
extern void osm_sir_rcv_process(IN void *context, IN void *data);
extern void osm_vlarb_rec_rcv_process(IN void *context, IN void *data);
extern void osm_sr_rcv_lease_cb(IN void *context);

void osm_sa_construct(IN osm_sa_t * p_sa)
{
	memset(p_sa, 0, sizeof(*p_sa));
	p_sa->state = OSM_SA_STATE_INIT;
	p_sa->sa_trans_id = OSM_SA_INITIAL_TID_VALUE;

	cl_timer_construct(&p_sa->sr_timer);
}

void osm_sa_shutdown(IN osm_sa_t * p_sa)
{
	OSM_LOG_ENTER(p_sa->p_log);

	cl_timer_stop(&p_sa->sr_timer);

	/* unbind from the mad service */
	osm_sa_mad_ctrl_unbind(&p_sa->mad_ctrl);

	/* remove any registered dispatcher message */
	cl_disp_unregister(p_sa->nr_disp_h);
	cl_disp_unregister(p_sa->pir_disp_h);
	cl_disp_unregister(p_sa->gir_disp_h);
	cl_disp_unregister(p_sa->lr_disp_h);
	cl_disp_unregister(p_sa->pr_disp_h);
#if defined (VENDOR_RMPP_SUPPORT) && defined (DUAL_SIDED_RMPP)
	cl_disp_unregister(p_sa->mpr_disp_h);
#endif
	cl_disp_unregister(p_sa->smir_disp_h);
	cl_disp_unregister(p_sa->mcmr_disp_h);
	cl_disp_unregister(p_sa->sr_disp_h);
	cl_disp_unregister(p_sa->infr_disp_h);
	cl_disp_unregister(p_sa->infir_disp_h);
	cl_disp_unregister(p_sa->vlarb_disp_h);
	cl_disp_unregister(p_sa->slvl_disp_h);
	cl_disp_unregister(p_sa->pkey_disp_h);
	cl_disp_unregister(p_sa->lft_disp_h);
	cl_disp_unregister(p_sa->sir_disp_h);
	cl_disp_unregister(p_sa->mft_disp_h);
	osm_sa_mad_ctrl_destroy(&p_sa->mad_ctrl);

	OSM_LOG_EXIT(p_sa->p_log);
}

void osm_sa_destroy(IN osm_sa_t * p_sa)
{
	OSM_LOG_ENTER(p_sa->p_log);

	p_sa->state = OSM_SA_STATE_INIT;

	cl_timer_destroy(&p_sa->sr_timer);

	OSM_LOG_EXIT(p_sa->p_log);
}

/* =========================================================================
 * Begin RDMA hack code
 * ========================================================================= */

/** =========================================================================
 * Issues
 *    1) Where in the SA packet are we going to store the QPN of the client?
 *       At the end of the packet in sa_eth_info
 *
 */

#include <infiniband/verbs.h>
#include <infiniband/sa.h>

#define SA_RDMA_REQUEST (CL_HTON16(0x0001))
#define SA_RDMA_COMPLETE (CL_HTON16(0x0009))
#define SA_SEND_WRID (uint64_t)(0xDEADBEEF)

struct rdma_memory {
	struct ibv_mr *mr;
	uint8_t       *buf;
	size_t         size;
};

/* Information about the clients QP and memory buffer */
struct sa_eth_info {
	uint32_t qpn;
	uint64_t addr;
	uint32_t r_key;
	uint32_t length;
} __attribute__((packed));

/* information sent back to the client about the buffer written */
struct sa_rdma_res_info {
	uint32_t length;
} __attribute__((packed));

struct sa_rdma_qp {
	struct sa_rdma_qp *next;
	struct ibv_qp *qp;
	cl_spinlock_t attached;
	/* perhaps will want to have a separate CQ for each QP? */
	/* struct ibv_cq *cq; */
};

ib_api_status_t osm_sa_rdma_add_qp(IN osm_sa_t *sa, struct ibv_qp *qp)
{
	struct sa_rdma_qp *tmp = malloc(sizeof(*tmp));
	if (!tmp) {
		return (IB_INSUFFICIENT_RESOURCES);
	}

	tmp->qp = qp;
	cl_spinlock_construct(&tmp->attached);
	cl_spinlock_init(&tmp->attached);
	tmp->next = sa->rdma_ctx.qps_head;
	sa->rdma_ctx.qps_head = tmp;

	return (IB_SUCCESS);
}

#if 0
ib_api_status_t osm_sa_rdma_remove_qp(struct sa_rdma_qp *qp)
{
	// FIXME need to clean this up sometime...
	return (IB_SUCCESS);
}
#endif

ib_api_status_t osm_sa_rdma_detach_qp(IN osm_sa_t *p_sa,
				struct sa_rdma_qp *sa_qp)
{
	// Transition to RESET
	{
		struct ibv_qp_attr attr = {
			.qp_state        = IBV_QPS_RESET
		};

		if (ibv_modify_qp(sa_qp->qp, &attr, IBV_QP_STATE)) {
			OSM_LOG(p_sa->p_log, OSM_LOG_ERROR,
				"RDMA: Failed to modify QP to RESET\n");
			goto DestroyQP;
		}
	}

	// Transition from Reset to INIT
	{
		struct ibv_qp_attr attr = {
			.qp_state        = IBV_QPS_INIT,
			.pkey_index      = 0,
			.port_num        = p_sa->rdma_ctx.device_port,
			.qp_access_flags = 0
		};

		if (ibv_modify_qp(sa_qp->qp, &attr,
				  IBV_QP_STATE              |
				  IBV_QP_PKEY_INDEX         |
				  IBV_QP_PORT               |
				  IBV_QP_ACCESS_FLAGS)) {
			OSM_LOG(p_sa->p_log, OSM_LOG_ERROR,
				"RDMA: Failed to modify QP to INIT\n");
			goto DestroyQP;
		}
	}
	cl_spinlock_release(&sa_qp->attached);

	return (IB_SUCCESS);
DestroyQP:
	// FIXME remove QP completely
fprintf(stderr, "Well that didn't work.  QP RTS -> RESET -> INIT");
	exit(1);
}

ib_api_status_t osm_sa_rdma_get_qp(IN osm_sa_t *p_sa,
				struct sa_eth_info *eth_info,
				struct ibv_sa_path_rec *path,
				struct sa_rdma_qp **sa_qp)
{
	/* FIXME find eth_info->qpn from list */
	struct sa_rdma_qp *rc = p_sa->rdma_ctx.qps_head;
	struct ibv_qp *qp = p_sa->rdma_ctx.qps_head->qp;

	/* transition it to RTR/RTS with eth_info and path record */
	struct ibv_qp_attr attr = {
		.qp_state		= IBV_QPS_RTR,
		.path_mtu		= path->mtu,
		.dest_qp_num		= eth_info->qpn,
		.rq_psn			= 1,
		.max_dest_rd_atomic	= 1,
		.min_rnr_timer		= 12,
		.ah_attr		= {
			.is_global	= 0,
			.dlid		= path->dlid,
			.sl		= path->sl,
			.src_path_bits	= 0,
			.port_num	= p_sa->rdma_ctx.device_port
		}
	};

	*sa_qp = NULL;

	OSM_LOG(p_sa->p_log, OSM_LOG_DEBUG,
		"qp ptr:        %p\n"
		"qpn:           %u\n"
		"path->mtu:     %d\n"
		"path->dlid:    %d\n"
		"path->sl:      %d\n"
		"device_port:   %d\n"
		,
		qp,
		eth_info->qpn,
		path->mtu,
		path->dlid,
		path->sl,
		p_sa->rdma_ctx.device_port);

	cl_spinlock_acquire(&rc->attached);

	if (ibv_modify_qp(qp, &attr,
			  IBV_QP_STATE              |
			  IBV_QP_PATH_MTU           |
			  IBV_QP_DEST_QPN           |
			  IBV_QP_RQ_PSN             |
			  IBV_QP_MAX_DEST_RD_ATOMIC |
			  IBV_QP_MIN_RNR_TIMER      |
			  IBV_QP_AV)) {
		char *err_str = strerror(errno);
		cl_spinlock_release(&rc->attached);
		OSM_LOG(p_sa->p_log, OSM_LOG_ERROR,
			"Failed to modify QP to RTR: %s\n", err_str);
		return 1;
	}

	attr.qp_state	    = IBV_QPS_RTS;
	attr.timeout	    = 14;
	attr.retry_cnt	    = 7;
	attr.rnr_retry	    = 7;
	attr.sq_psn	    = 1;
	attr.max_rd_atomic  = 1;
	if (ibv_modify_qp(qp, &attr,
			  IBV_QP_STATE              |
			  IBV_QP_TIMEOUT            |
			  IBV_QP_RETRY_CNT          |
			  IBV_QP_RNR_RETRY          |
			  IBV_QP_SQ_PSN             |
			  IBV_QP_MAX_QP_RD_ATOMIC)) {
		// NOTE detach releases rc->attached!
		osm_sa_rdma_detach_qp(p_sa, rc);
		OSM_LOG(p_sa->p_log, OSM_LOG_ERROR,
			"Failed to modify QP to RTS: %s\n", strerror(errno));
		return 1;
	}

	OSM_LOG(p_sa->p_log, OSM_LOG_DEBUG, "QP 0x%x connected to Client QPn 0x%x\n",
		qp->qp_num, eth_info->qpn);

	*sa_qp = rc;
	return 0;
}

ib_api_status_t osm_sa_rdma_create_qp(IN osm_sa_t *p_sa)
{
	struct ibv_qp *qp;

	struct ibv_qp_init_attr attr = {
		.send_cq = p_sa->rdma_ctx.cq,
		.recv_cq = p_sa->rdma_ctx.cq,
		.cap     = {
			.max_send_wr  = 10,
			.max_recv_wr  = 500,
			.max_send_sge = 1,
			.max_recv_sge = 1
		},
		.qp_type = IBV_QPT_RC
	};
	
	qp = ibv_create_qp(p_sa->rdma_ctx.pd, &attr);
	if (!qp) {
		OSM_LOG(p_sa->p_log, OSM_LOG_ERROR,
			"RDMA: Failed to create QP\n");
		return (IB_INSUFFICIENT_RESOURCES);
	}

	{
		struct ibv_qp_attr attr = {
			.qp_state        = IBV_QPS_INIT,
			.pkey_index      = 0,
			.port_num        = p_sa->rdma_ctx.device_port,
			.qp_access_flags = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_LOCAL_WRITE
		};

		if (ibv_modify_qp(qp, &attr,
				  IBV_QP_STATE              |
				  IBV_QP_PKEY_INDEX         |
				  IBV_QP_PORT               |
				  IBV_QP_ACCESS_FLAGS)) {
			OSM_LOG(p_sa->p_log, OSM_LOG_ERROR,
				"RDMA: Failed to modify QP to INIT\n");
			goto DestroyQP;
		}
	}

	if (osm_sa_rdma_add_qp(p_sa, qp) != IB_SUCCESS) {
		OSM_LOG(p_sa->p_log, OSM_LOG_ERROR,
			"RDMA: Failed to add QP to pool\n");
		goto DestroyQP;
	}

	OSM_LOG(p_sa->p_log, OSM_LOG_INFO, "QPn = %d\n", qp->qp_num);
	printf("qp %p; QPn = %d\n", qp, qp->qp_num);
	return (IB_SUCCESS);

DestroyQP:
	ibv_destroy_qp(qp);
	return (IB_INSUFFICIENT_RESOURCES);
}

struct ibv_device *osm_find_RDMA_dev_from_port_guid(
				uint64_t port_guid,
				struct ibv_device **dev_list, int num_dev,
				int *port,
				struct ibv_context **rc)
{
	int i = 0, j = 0, k=0;
	struct ibv_device_attr attr;
	struct ibv_port_attr pattr;
	union ibv_gid gid;
	struct ibv_context *ctx;

	if (port_guid == 0)
		return (NULL);

	/* this is ugly... */
	for (i = 0; i<num_dev; i++) {
		ctx = ibv_open_device(dev_list[i]);
		if (ctx && !ibv_query_device(ctx, &attr)) {
			for (j = 1; j<(attr.phys_port_cnt+1); j++) {
				if (!ibv_query_port(ctx, j, &pattr)) {
					for (k = 0; k<pattr.gid_tbl_len; k++) {
						if (!ibv_query_gid(ctx, j, k, &gid)) {
							if (cl_ntoh64(gid.global.interface_id) == port_guid) {
								*port = j;
								*rc = ctx;
								return dev_list[i];
							}
						}
					}
				}
			}
		}
		ibv_close_device(ctx);
	}
	return (NULL);
}

ib_api_status_t osm_sa_rdma_init(IN osm_sa_t * p_sa, IN ib_net64_t port_guid)
{
	struct ibv_device *dev = NULL;
	struct ibv_device **dev_list;
	int num_dev;
	uint64_t guid = cl_ntoh64(port_guid);

	memset(&p_sa->rdma_ctx, 0, sizeof(p_sa->rdma_ctx));

	dev_list = ibv_get_device_list(&num_dev);
	if (!dev_list) {
		OSM_LOG(p_sa->p_log, OSM_LOG_ERROR,
			"RDMA: Failed to get device list\n");
		return (IB_INSUFFICIENT_RESOURCES);
	}

	dev = p_sa->rdma_ctx.dev = osm_find_RDMA_dev_from_port_guid(
					guid,
					dev_list, num_dev,
					&p_sa->rdma_ctx.device_port,
					&p_sa->rdma_ctx.dev_ctx);

	if (!dev) {
		OSM_LOG(p_sa->p_log, OSM_LOG_ERROR,
			"RDMA: Failed to find RDMA device for port guid: "
			"%"PRIx64"\n", guid);
		return (IB_INSUFFICIENT_RESOURCES);
	}

	OSM_LOG(p_sa->p_log, OSM_LOG_DEBUG,
			"RDMA: opened RDMA device: %s\n",
			ibv_get_device_name(dev));

	// Looks like you may not be able to do this here.
	// If so store the list for later "freeing"
	ibv_free_device_list(dev_list);

#if 0
	p_sa->rdma_ctx.comp_ch = ibv_create_comp_channel(p_sa->rdma_ctx.dev_ctx);
	if (!p_sa->rdma_ctx.comp_ch) {
		OSM_LOG(p_sa->p_log, OSM_LOG_ERROR,
			"RDMA: Failed to create completion channel: %s\n",
			ibv_get_device_name(dev));
		goto CloseDevice;
	}
#endif
	// skip this for now
	p_sa->rdma_ctx.comp_ch = NULL;

	p_sa->rdma_ctx.cq = ibv_create_cq(p_sa->rdma_ctx.dev_ctx, 1000,
			     (void *)&p_sa->rdma_ctx,
			     p_sa->rdma_ctx.comp_ch,
			     0);
	if (!p_sa->rdma_ctx.cq) {
		OSM_LOG(p_sa->p_log, OSM_LOG_ERROR,
			"RDMA: Failed to create CQ: %s\n",
			ibv_get_device_name(dev));
		goto DestroyCompCh;
	}

	if (p_sa->rdma_ctx.comp_ch) {
		if ((ibv_req_notify_cq(p_sa->rdma_ctx.cq, 0)) != 0) {
			OSM_LOG(p_sa->p_log, OSM_LOG_ERROR,
				"RDMA: Request Notify CQ failed: %s\n",
				ibv_get_device_name(dev));
			goto DestroyCQ;
		}
	}

	p_sa->rdma_ctx.pd = ibv_alloc_pd(p_sa->rdma_ctx.dev_ctx);
	if (!p_sa->rdma_ctx.pd) {
		OSM_LOG(p_sa->p_log, OSM_LOG_ERROR,
			"RDMA: Failed to allocate the PD: %s\n",
			ibv_get_device_name(dev));
		goto DestroyCQ;
	}

	OSM_LOG(p_sa->p_log, OSM_LOG_INFO, "SA RDMA configured on %s:%d\n",
			ibv_get_device_name(dev),
			p_sa->rdma_ctx.device_port);
	return (osm_sa_rdma_create_qp(p_sa)); // FIXME create more than 1 QP

DestroyCQ:
	ibv_destroy_cq(p_sa->rdma_ctx.cq);
DestroyCompCh:
	if (p_sa->rdma_ctx.comp_ch) {
		ibv_destroy_comp_channel(p_sa->rdma_ctx.comp_ch);
	}
//CloseDevice:
	ibv_close_device(p_sa->rdma_ctx.dev_ctx);
	return (IB_INSUFFICIENT_RESOURCES);
}

uint8_t * rdma_mem_get_buf(struct rdma_memory *mem)
{
	return mem->buf;
}

/* allocate a buffer and register it */
struct rdma_memory * osm_sa_rdma_malloc(struct ibv_pd *pd, size_t size)
{
	struct rdma_memory *rc = malloc(sizeof(*rc));
	if (!rc)
		return (NULL);

	rc->buf = malloc(size);
	if (!rc->buf) {
		free(rc);
		return (NULL);
	}

	rc->size = size;
	rc->mr = ibv_reg_mr(pd, rc->buf, rc->size,
				IBV_ACCESS_LOCAL_WRITE |
				IBV_ACCESS_REMOTE_READ |
				IBV_ACCESS_REMOTE_WRITE);
	if (!rc->mr) {
		free(rc);
		free(rc->buf);
		return (NULL);
	}
	return (rc);
}

void osm_sa_rdma_free(struct rdma_memory *mem)
{
	ibv_dereg_mr(mem->mr);
	free(mem->buf);
	free(mem);
}

int osm_sa_rdma_post_send(IN osm_sa_t *p_sa, struct sa_rdma_qp *sa_qp,
			struct rdma_memory *buf, struct sa_eth_info *eth_info)
{
	int rc = 0;
	struct ibv_qp *qp = sa_qp->qp;
	struct ibv_sge list = {
		.addr	= (uintptr_t) buf->buf,
		.length = buf->size,
		.lkey	= buf->mr->lkey
	};
	struct ibv_send_wr wr = {
		.next                = NULL,
		.wr_id	             = SA_SEND_WRID,
		.send_flags	     = IBV_SEND_SIGNALED,
		.sg_list             = &list,
		.num_sge             = 1,
		.opcode              = IBV_WR_RDMA_WRITE,
		.wr.rdma.remote_addr = eth_info->addr,
		.wr.rdma.rkey        = eth_info->r_key,
	};
	struct ibv_send_wr *bad_wr;

printf("addr %"PRIx64"; rkey %x; length %x\n",
	(uint64_t)eth_info->addr,
	eth_info->r_key,
	eth_info->length);


	if ((rc = ibv_post_send(qp, &wr, &bad_wr)) != 0) {
		OSM_LOG(p_sa->p_log, OSM_LOG_ERROR,
			"SA RDMA: Failed to post send: %d\n", rc);
		return (-EFAULT);
	}

	return (0);
}

ib_api_status_t osm_sa_rdma_wait_completion(IN osm_sa_t *p_sa,
			struct ibv_comp_channel *channel)
{
	struct ibv_cq *cq = p_sa->rdma_ctx.cq;
	struct ibv_wc wc;
	int rc = 0;

	// FIXME use channel
	if (channel) {
		ibv_get_cq_event(channel, &cq, NULL);
		ibv_ack_cq_events(cq, 1);
		ibv_req_notify_cq(cq, 0);
	}
	do {
		rc = ibv_poll_cq(cq, 1, &wc);
		if (rc < 0)
			break;
	} while (rc == 0);

	if (rc <= 0 || wc.status != IBV_WC_SUCCESS) {
		OSM_LOG(p_sa->p_log, OSM_LOG_ERROR,
			"RDMA Work completion error, rc %d; status: '%s'\n",
			rc, ibv_wc_status_str(wc.status));
		return (IB_ERROR);
	}
	return (IB_SUCCESS);
}

ib_api_status_t osm_sa_rdma_close(IN osm_sa_t * p_sa)
{
	ibv_dealloc_pd(p_sa->rdma_ctx.pd);
	ibv_destroy_cq(p_sa->rdma_ctx.cq);
	if (p_sa->rdma_ctx.comp_ch) {
		ibv_destroy_comp_channel(p_sa->rdma_ctx.comp_ch);
	}
	ibv_close_device(p_sa->rdma_ctx.dev_ctx);
	return (IB_SUCCESS);
}

/* =========================================================================
 * End RDMA hack code
 * ========================================================================= */

ib_api_status_t osm_sa_init(IN osm_sm_t * p_sm, IN osm_sa_t * p_sa,
			    IN osm_subn_t * p_subn, IN osm_vendor_t * p_vendor,
			    IN osm_mad_pool_t * p_mad_pool,
			    IN osm_log_t * p_log, IN osm_stats_t * p_stats,
			    IN cl_dispatcher_t * p_disp, IN cl_plock_t * p_lock)
{
	ib_api_status_t status;

	OSM_LOG_ENTER(p_log);

	p_sa->sm = p_sm;
	p_sa->p_subn = p_subn;
	p_sa->p_vendor = p_vendor;
	p_sa->p_mad_pool = p_mad_pool;
	p_sa->p_log = p_log;
	p_sa->p_disp = p_disp;
	p_sa->p_lock = p_lock;

	p_sa->state = OSM_SA_STATE_READY;
	p_sa->fabric_change = TRUE; // on startup the fabric is changed

	status = osm_sa_mad_ctrl_init(&p_sa->mad_ctrl, p_sa, p_sa->p_mad_pool,
				      p_sa->p_vendor, p_subn, p_log, p_stats,
				      p_disp);
	if (status != IB_SUCCESS)
		goto Exit;

	status = cl_timer_init(&p_sa->sr_timer, osm_sr_rcv_lease_cb, p_sa);
	if (status != IB_SUCCESS)
		goto Exit;

	status = IB_INSUFFICIENT_RESOURCES;
	p_sa->cpi_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_CLASS_PORT_INFO,
					    osm_cpi_rcv_process, p_sa);
	if (p_sa->cpi_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sa->nr_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_NODE_RECORD,
					   osm_nr_rcv_process, p_sa);
	if (p_sa->nr_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sa->pir_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_PORTINFO_RECORD,
					    osm_pir_rcv_process, p_sa);
	if (p_sa->pir_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sa->gir_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_GUIDINFO_RECORD,
					    osm_gir_rcv_process, p_sa);
	if (p_sa->gir_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sa->lr_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_LINK_RECORD,
					   osm_lr_rcv_process, p_sa);
	if (p_sa->lr_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sa->pr_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_PATH_RECORD,
					   osm_pr_rcv_process, p_sa);
	if (p_sa->pr_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

#if defined (VENDOR_RMPP_SUPPORT) && defined (DUAL_SIDED_RMPP)
	p_sa->mpr_disp_h =
	    cl_disp_register(p_disp, OSM_MSG_MAD_MULTIPATH_RECORD,
			     osm_mpr_rcv_process, p_sa);
	if (p_sa->mpr_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;
#endif

	p_sa->smir_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_SMINFO_RECORD,
					     osm_smir_rcv_process, p_sa);
	if (p_sa->smir_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sa->mcmr_disp_h =
	    cl_disp_register(p_disp, OSM_MSG_MAD_MCMEMBER_RECORD,
			     osm_mcmr_rcv_process, p_sa);
	if (p_sa->mcmr_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sa->sr_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_SERVICE_RECORD,
					   osm_sr_rcv_process, p_sa);
	if (p_sa->sr_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sa->infr_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_INFORM_INFO,
					     osm_infr_rcv_process, p_sa);
	if (p_sa->infr_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sa->infir_disp_h =
	    cl_disp_register(p_disp, OSM_MSG_MAD_INFORM_INFO_RECORD,
			     osm_infir_rcv_process, p_sa);
	if (p_sa->infir_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sa->vlarb_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_VL_ARB_RECORD,
					      osm_vlarb_rec_rcv_process, p_sa);
	if (p_sa->vlarb_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sa->slvl_disp_h =
	    cl_disp_register(p_disp, OSM_MSG_MAD_SLVL_TBL_RECORD,
			     osm_slvl_rec_rcv_process, p_sa);
	if (p_sa->slvl_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sa->pkey_disp_h =
	    cl_disp_register(p_disp, OSM_MSG_MAD_PKEY_TBL_RECORD,
			     osm_pkey_rec_rcv_process, p_sa);
	if (p_sa->pkey_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sa->lft_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_LFT_RECORD,
					    osm_lftr_rcv_process, p_sa);
	if (p_sa->lft_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sa->sir_disp_h =
	    cl_disp_register(p_disp, OSM_MSG_MAD_SWITCH_INFO_RECORD,
			     osm_sir_rcv_process, p_sa);
	if (p_sa->sir_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	p_sa->mft_disp_h = cl_disp_register(p_disp, OSM_MSG_MAD_MFT_RECORD,
					    osm_mftr_rcv_process, p_sa);
	if (p_sa->mft_disp_h == CL_DISP_INVALID_HANDLE)
		goto Exit;

	status = IB_SUCCESS;
Exit:
	OSM_LOG_EXIT(p_log);
	return status;
}

ib_api_status_t osm_sa_bind(IN osm_sa_t * p_sa, IN ib_net64_t port_guid)
{
	ib_api_status_t status;

	OSM_LOG_ENTER(p_sa->p_log);

/* Open a QP for rdma transfers */
	osm_sa_rdma_init(p_sa, port_guid);

	status = osm_sa_mad_ctrl_bind(&p_sa->mad_ctrl, port_guid);

	if (status != IB_SUCCESS) {
		OSM_LOG(p_sa->p_log, OSM_LOG_ERROR, "ERR 4C03: "
			"SA MAD Controller bind failed (%s)\n",
			ib_get_err_str(status));
		goto Exit;
	}

Exit:
	OSM_LOG_EXIT(p_sa->p_log);
	return status;
}

ib_api_status_t osm_sa_send(osm_sa_t *sa, IN osm_madw_t * p_madw,
			    IN boolean_t resp_expected)
{
	ib_api_status_t status;

	cl_atomic_inc(&sa->p_subn->p_osm->stats.sa_mads_sent);
	status = osm_vendor_send(p_madw->h_bind, p_madw, resp_expected);
	if (status != IB_SUCCESS) {
		cl_atomic_dec(&sa->p_subn->p_osm->stats.sa_mads_sent);
		OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 4C04: "
			"osm_vendor_send failed, status = %s\n",
			ib_get_err_str(status));
	}
	return status;
}

void osm_sa_send_error(IN osm_sa_t * sa, IN const osm_madw_t * p_madw,
		       IN ib_net16_t sa_status)
{
	osm_madw_t *p_resp_madw;
	ib_sa_mad_t *p_resp_sa_mad;
	ib_sa_mad_t *p_sa_mad;

	OSM_LOG_ENTER(sa->p_log);

	/* avoid races - if we are exiting - exit */
	if (osm_exit_flag) {
		OSM_LOG(sa->p_log, OSM_LOG_DEBUG,
			"Ignoring requested send after exit\n");
		goto Exit;
	}

	p_resp_madw = osm_mad_pool_get(sa->p_mad_pool,
				       p_madw->h_bind, MAD_BLOCK_SIZE,
				       &p_madw->mad_addr);

	if (p_resp_madw == NULL) {
		OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 4C07: "
			"Unable to acquire response MAD\n");
		goto Exit;
	}

	p_resp_sa_mad = osm_madw_get_sa_mad_ptr(p_resp_madw);
	p_sa_mad = osm_madw_get_sa_mad_ptr(p_madw);

	/*  Copy the MAD header back into the response mad */
	*p_resp_sa_mad = *p_sa_mad;
	p_resp_sa_mad->status = sa_status;

	if (p_resp_sa_mad->method == IB_MAD_METHOD_SET)
		p_resp_sa_mad->method = IB_MAD_METHOD_GET;
	else if (p_resp_sa_mad->method == IB_MAD_METHOD_GETTABLE)
		p_resp_sa_mad->attr_offset = 0;

	p_resp_sa_mad->method |= IB_MAD_METHOD_RESP_MASK;

	/*
	 * C15-0.1.5 - always return SM_Key = 0 (table 185 p 884)
	 */
	p_resp_sa_mad->sm_key = 0;

	/*
	 * o15-0.2.7 - The PathRecord Attribute ID shall be used in
	 * the response (to a SubnAdmGetMulti(MultiPathRecord)
	 */
	if (p_resp_sa_mad->attr_id == IB_MAD_ATTR_MULTIPATH_RECORD)
		p_resp_sa_mad->attr_id = IB_MAD_ATTR_PATH_RECORD;

	if (OSM_LOG_IS_ACTIVE_V2(sa->p_log, OSM_LOG_FRAMES))
		osm_dump_sa_mad_v2(sa->p_log, p_resp_sa_mad, FILE_ID, OSM_LOG_FRAMES);

	osm_sa_send(sa, p_resp_madw, FALSE);

Exit:
	OSM_LOG_EXIT(sa->p_log);
}

static void osm_sa_respond_int(osm_sa_t *sa, osm_madw_t *madw, size_t attr_size,
		    cl_qlist_t *list, struct rdma_memory * rdma_buf, int num_rec)
{
	struct item_data {
		cl_list_item_t list;
		char data[0];
	};
	cl_list_item_t *item;
	osm_madw_t *resp_madw;
	ib_sa_mad_t *sa_mad, *resp_sa_mad;
	unsigned i;
#ifndef VENDOR_RMPP_SUPPORT
	unsigned trim_num_rec;
#endif
	unsigned char *p;
	struct sa_rdma_qp *sa_qp = NULL;
	struct sa_eth_info eth_info;
	boolean_t buf_needs_copy = TRUE;

	sa_mad = osm_madw_get_sa_mad_ptr(madw);

	OSM_LOG(sa->p_log, OSM_LOG_DEBUG, "REQUEST MAD\n");
	osm_dump_sa_mad_v2(sa->p_log, sa_mad, FILE_ID, OSM_LOG_FRAMES);

	/*
	 * C15-0.1.30:
	 * If we do a SubnAdmGet and got more than one record it is an error!
	 */
	if (sa_mad->method == IB_MAD_METHOD_GET && num_rec > 1) {
		OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 4C05: "
			"Got %u records for SubnAdmGet(%s) comp_mask 0x%016" PRIx64 "\n",
			num_rec, ib_get_sa_attr_str(sa_mad->attr_id),
			cl_ntoh64(sa_mad->comp_mask));
		osm_sa_send_error(sa, madw, IB_SA_MAD_STATUS_TOO_MANY_RECORDS);
		goto Exit;
	}

#ifndef VENDOR_RMPP_SUPPORT
	trim_num_rec = (MAD_BLOCK_SIZE - IB_SA_MAD_HDR_SIZE) / attr_size;
	if (trim_num_rec < num_rec) {
		OSM_LOG(sa->p_log, OSM_LOG_VERBOSE,
			"Number of records:%u trimmed to:%u to fit in one MAD\n",
			num_rec, trim_num_rec);
		num_rec = trim_num_rec;
	}
#endif

	OSM_LOG(sa->p_log, OSM_LOG_DEBUG, "Returning %u records\n", num_rec);

	if (sa_mad->method == IB_MAD_METHOD_GET && num_rec == 0) {
		osm_sa_send_error(sa, madw, IB_SA_MAD_STATUS_NO_RECORDS);
		goto Exit;
	}

	/*
	 * Get a MAD to reply. Address of Mad is in the received mad_wrapper
	 */

	/* if this is an RDMA request the resp mad is a single packet */
	if (sa_mad->resv1 & SA_RDMA_REQUEST) {
		/* get ETH info from user */
		struct sa_eth_info *eth = (struct sa_eth_info *)
				(((uint8_t *)sa_mad) + 256 - sizeof(*eth));

		eth_info.qpn    = cl_ntoh32(eth->qpn);
		eth_info.addr   = cl_ntoh64(eth->addr);
		eth_info.r_key  = cl_ntoh32(eth->r_key);
		eth_info.length = cl_ntoh32(eth->length);

		/* verify users buffer size */
		if (eth_info.length < (uint32_t)(num_rec * attr_size)) {
			OSM_LOG(sa->p_log, OSM_LOG_ERROR,
				"user RDMA buffer too small %u < %u\n",
				eth_info.length, (uint32_t)(num_rec * attr_size));
			osm_sa_send_error(sa, madw, IB_SA_MAD_STATUS_NO_RESOURCES);
			goto Exit;
		}
		/* the RDMA resp MAD is a single packet */
		resp_madw = osm_mad_pool_get(sa->p_mad_pool, madw->h_bind,
					     sizeof(ib_sa_mad_t),
					     &madw->mad_addr);
	} else {
		/* else allocate "full" RMPP packet */
		resp_madw = osm_mad_pool_get(sa->p_mad_pool, madw->h_bind,
					     num_rec * attr_size + IB_SA_MAD_HDR_SIZE,
					     &madw->mad_addr);
	}

	if (!resp_madw) {
		OSM_LOG(sa->p_log, OSM_LOG_ERROR, "ERR 4C06: "
			"osm_mad_pool_get failed\n");
		osm_sa_send_error(sa, madw, IB_SA_MAD_STATUS_NO_RESOURCES);
		goto Exit;
	}

	resp_sa_mad = osm_madw_get_sa_mad_ptr(resp_madw);

	/*
	   Copy the MAD header back into the response mad.
	   Set the 'R' bit and the payload length,
	   Then copy all records from the list into the response payload.
	 */

	memcpy(resp_sa_mad, sa_mad, IB_SA_MAD_HDR_SIZE);
	if (resp_sa_mad->method == IB_MAD_METHOD_SET)
		resp_sa_mad->method = IB_MAD_METHOD_GET;
	resp_sa_mad->method |= IB_MAD_METHOD_RESP_MASK;
	/* C15-0.1.5 - always return SM_Key = 0 (table 185 p 884) */
	resp_sa_mad->sm_key = 0;

	/* Fill in the offset (paylen will be done by the rmpp SAR) */
	resp_sa_mad->attr_offset = num_rec ? ib_get_attr_offset(attr_size) : 0;

	p = ib_sa_mad_get_payload_ptr(resp_sa_mad);

#ifndef VENDOR_RMPP_SUPPORT
	/* we support only one packet RMPP - so we will set the first and
	   last flags for gettable */
	if (resp_sa_mad->method == IB_MAD_METHOD_GETTABLE_RESP) {
		resp_sa_mad->rmpp_type = IB_RMPP_TYPE_DATA;
		resp_sa_mad->rmpp_flags =
		    IB_RMPP_FLAG_FIRST | IB_RMPP_FLAG_LAST |
		    IB_RMPP_FLAG_ACTIVE;
	}
#else
	/* forcefully define the packet as RMPP one */
	if (resp_sa_mad->method == IB_MAD_METHOD_GETTABLE_RESP)
		resp_sa_mad->rmpp_flags = IB_RMPP_FLAG_ACTIVE;
#endif

	OSM_LOG(sa->p_log, OSM_LOG_DEBUG, "RESPONSE MAD\n");
	osm_dump_sa_mad_v2(sa->p_log, resp_sa_mad, FILE_ID, OSM_LOG_FRAMES);

	/* if the request is for RMDA... */
	if (sa_mad->resv1 & SA_RDMA_REQUEST) {
		/* allocate a buffer */
		uint32_t buf_size = IB_SA_MAD_HDR_SIZE + (num_rec * attr_size);
		if (rdma_buf) {
			buf_needs_copy = FALSE;
		} else {
			rdma_buf = osm_sa_rdma_malloc(sa->rdma_ctx.pd, buf_size);
			if (!rdma_buf) {
				osm_mad_pool_put(sa->p_mad_pool, resp_madw);
				osm_sa_send_error(sa, madw, IB_SA_MAD_STATUS_NO_RESOURCES);
				goto Exit;
			}
		}
		p = rdma_buf->buf;

		/* "fake" the MAD response in the buffer */
		memcpy(p, resp_sa_mad, IB_SA_MAD_HDR_SIZE);
		p += IB_SA_MAD_HDR_SIZE;

		/* write the length of the data at the end of the response MAD */
		((uint32_t*)(resp_sa_mad->data))[49] = buf_size;

		/* in this case we only need a 1 packet response */
		resp_sa_mad->rmpp_type = IB_RMPP_TYPE_DATA;
		resp_sa_mad->rmpp_flags =
		    IB_RMPP_FLAG_FIRST | IB_RMPP_FLAG_LAST |
		    IB_RMPP_FLAG_ACTIVE;
	}

	if (buf_needs_copy) {
		if (rdma_buf) {
			// here we have a non-rdma request but a rdma buffer built so
			// we have to copy the rdma buffer to the mad packet for
			// sending...  Wouldn't it be nice if the vendor mad layer
			// could be rebuilt?
			memcpy(p, rdma_buf->buf, num_rec * attr_size);
		} else {
			for (i = 0; i < num_rec; i++) {
				item = cl_qlist_remove_head(list);
				memcpy(p, ((struct item_data *)item)->data, attr_size);
				p += attr_size;
				free(item);
			}
		}
	}

	/* send the RDMA buffer */
	if (sa_mad->resv1 & SA_RDMA_REQUEST) {
		struct timeval tv_data_start;
		struct timeval tv_data_end;
		struct timeval tv_data_diff;
		struct timeval tv_start;
		struct timeval tv_end;
		struct timeval tv_diff;
		/* FIXME get real PathRecord from internals */
		struct ibv_sa_path_rec path = {
			.dlid = cl_ntoh16(madw->mad_addr.dest_lid),
			.sl = madw->mad_addr.addr_type.gsi.service_level,
			.mtu = IBV_MTU_2048
		};

		OSM_LOG(sa->p_log, OSM_LOG_DEBUG,
			"Attaching qp to remote lid %d, sl %d\n",
			path.dlid, path.sl);

		fprintf(stderr, "RDMA ((%u * %lu) + %lu = %lu bytes)...\n",
			num_rec, attr_size, IB_SA_MAD_HDR_SIZE,
			rdma_buf->size);
		gettimeofday(&tv_start, NULL);
		/* attach a qp */
		if (osm_sa_rdma_get_qp(sa, &eth_info, &path, &sa_qp)) {
			osm_mad_pool_put(sa->p_mad_pool, resp_madw);
			osm_sa_send_error(sa, madw, IB_SA_MAD_STATUS_NO_RESOURCES);
			goto Exit;
		}
		gettimeofday(&tv_data_start, NULL);
		/* send data */
		if (osm_sa_rdma_post_send(sa, sa_qp, rdma_buf, &eth_info)) {
			osm_mad_pool_put(sa->p_mad_pool, resp_madw);
			osm_sa_send_error(sa, madw, IB_SA_MAD_STATUS_NO_RESOURCES);
			osm_sa_rdma_detach_qp(sa, sa_qp);
			goto Exit;
		}
		/* wait for verification */
		if (osm_sa_rdma_wait_completion(sa, sa->rdma_ctx.comp_ch)) {
			osm_mad_pool_put(sa->p_mad_pool, resp_madw);
			osm_sa_send_error(sa, madw, IB_SA_MAD_STATUS_NO_RESOURCES);
			osm_sa_rdma_detach_qp(sa, sa_qp);
			goto Exit;
		}
		gettimeofday(&tv_data_end, NULL);
		/* detach to allow for others to use */
		osm_sa_rdma_detach_qp(sa, sa_qp);

		gettimeofday(&tv_end, NULL);
		diff_time(&tv_start, &tv_end, &tv_diff);
		diff_time(&tv_data_start, &tv_data_end, &tv_data_diff);
		fprintf(stderr, "RDMA time:\n"
			"\ttotal: %ld.%06ld sec\n"
			"\tdata : %ld.%06ld sec\n",
			tv_diff.tv_sec, tv_diff.tv_usec,
			tv_data_diff.tv_sec, tv_data_diff.tv_usec);

		resp_sa_mad->status = SA_RDMA_COMPLETE;
		if (buf_needs_copy) {
			osm_sa_rdma_free(rdma_buf);
		}
	}

	osm_dump_sa_mad_v2(sa->p_log, resp_sa_mad, FILE_ID, OSM_LOG_FRAMES);

	if (sa_mad->resv1 & SA_RDMA_REQUEST) {
		OSM_LOG(sa->p_log, OSM_LOG_INFO, "QPn = %d\n",
			sa->rdma_ctx.qps_head->qp->qp_num);
	}
	/* in the RDMA case this is just to notify of the write completion */
	osm_sa_send(sa, resp_madw, FALSE);

Exit:
	if (list) {
		/* need to set the mem free ... */
		item = cl_qlist_remove_head(list);
		while (item != cl_qlist_end(list)) {
			free(item);
			item = cl_qlist_remove_head(list);
		}
	}
}

/* originial */
void osm_sa_respond(osm_sa_t *sa, osm_madw_t *madw, size_t attr_size,
		    cl_qlist_t *list)
{
	osm_sa_respond_int(sa, madw, attr_size, list, NULL, cl_qlist_count(list));
}

/* new buf version */
void osm_sa_respond_buf(osm_sa_t *sa, osm_madw_t *madw, size_t attr_size,
		    struct rdma_memory *buf, unsigned num_rec)
{
	osm_sa_respond_int(sa, madw, attr_size, NULL, buf, num_rec);
}


/*
 *  SA DB Dumper
 *
 */

struct opensm_dump_context {
	osm_opensm_t *p_osm;
	FILE *file;
};

static int
opensm_dump_to_file(osm_opensm_t * p_osm, const char *file_name,
		    void (*dump_func) (osm_opensm_t * p_osm, FILE * file))
{
	char path[1024];
	FILE *file;

	snprintf(path, sizeof(path), "%s/%s",
		 p_osm->subn.opt.dump_files_dir, file_name);

	file = fopen(path, "w");
	if (!file) {
		OSM_LOG(&p_osm->log, OSM_LOG_ERROR, "ERR 4C01: "
			"cannot open file \'%s\': %s\n",
			file_name, strerror(errno));
		return -1;
	}

	chmod(path, S_IRUSR | S_IWUSR);

	dump_func(p_osm, file);

	fclose(file);
	return 0;
}

static void mcast_mgr_dump_one_port(cl_map_item_t * p_map_item, void *cxt)
{
	FILE *file = ((struct opensm_dump_context *)cxt)->file;
	osm_mcm_alias_guid_t *p_mcm_alias_guid = (osm_mcm_alias_guid_t *) p_map_item;

	fprintf(file, "mcm_port: "
		"port_gid=0x%016" PRIx64 ":0x%016" PRIx64 " "
		"scope_state=0x%02x proxy_join=0x%x" "\n\n",
		cl_ntoh64(p_mcm_alias_guid->port_gid.unicast.prefix),
		cl_ntoh64(p_mcm_alias_guid->port_gid.unicast.interface_id),
		p_mcm_alias_guid->scope_state, p_mcm_alias_guid->proxy_join);
}

static void sa_dump_one_mgrp(osm_mgrp_t *p_mgrp, void *cxt)
{
	struct opensm_dump_context dump_context;
	osm_opensm_t *p_osm = ((struct opensm_dump_context *)cxt)->p_osm;
	FILE *file = ((struct opensm_dump_context *)cxt)->file;

	fprintf(file, "MC Group 0x%04x %s:"
		" mgid=0x%016" PRIx64 ":0x%016" PRIx64
		" port_gid=0x%016" PRIx64 ":0x%016" PRIx64
		" qkey=0x%08x mlid=0x%04x mtu=0x%02x tclass=0x%02x"
		" pkey=0x%04x rate=0x%02x pkt_life=0x%02x sl_flow_hop=0x%08x"
		" scope_state=0x%02x proxy_join=0x%x" "\n\n",
		cl_ntoh16(p_mgrp->mlid),
		p_mgrp->well_known ? " (well known)" : "",
		cl_ntoh64(p_mgrp->mcmember_rec.mgid.unicast.prefix),
		cl_ntoh64(p_mgrp->mcmember_rec.mgid.unicast.interface_id),
		cl_ntoh64(p_mgrp->mcmember_rec.port_gid.unicast.prefix),
		cl_ntoh64(p_mgrp->mcmember_rec.port_gid.unicast.interface_id),
		cl_ntoh32(p_mgrp->mcmember_rec.qkey),
		cl_ntoh16(p_mgrp->mcmember_rec.mlid),
		p_mgrp->mcmember_rec.mtu,
		p_mgrp->mcmember_rec.tclass,
		cl_ntoh16(p_mgrp->mcmember_rec.pkey),
		p_mgrp->mcmember_rec.rate,
		p_mgrp->mcmember_rec.pkt_life,
		cl_ntoh32(p_mgrp->mcmember_rec.sl_flow_hop),
		p_mgrp->mcmember_rec.scope_state,
		p_mgrp->mcmember_rec.proxy_join);

	dump_context.p_osm = p_osm;
	dump_context.file = file;

	cl_qmap_apply_func(&p_mgrp->mcm_alias_port_tbl,
			   mcast_mgr_dump_one_port, &dump_context);
}

static void sa_dump_one_inform(cl_list_item_t * p_list_item, void *cxt)
{
	FILE *file = ((struct opensm_dump_context *)cxt)->file;
	osm_infr_t *p_infr = (osm_infr_t *) p_list_item;
	ib_inform_info_record_t *p_iir = &p_infr->inform_record;

	fprintf(file, "InformInfo Record:"
		" subscriber_gid=0x%016" PRIx64 ":0x%016" PRIx64
		" subscriber_enum=0x%x"
		" InformInfo:"
		" gid=0x%016" PRIx64 ":0x%016" PRIx64
		" lid_range_begin=0x%x"
		" lid_range_end=0x%x"
		" is_generic=0x%x"
		" subscribe=0x%x"
		" trap_type=0x%x"
		" trap_num=0x%x"
		" qpn_resp_time_val=0x%x"
		" node_type=0x%06x"
		" rep_addr: lid=0x%04x path_bits=0x%02x static_rate=0x%02x"
		" remote_qp=0x%08x remote_qkey=0x%08x pkey_ix=0x%04x sl=0x%02x"
		"\n\n",
		cl_ntoh64(p_iir->subscriber_gid.unicast.prefix),
		cl_ntoh64(p_iir->subscriber_gid.unicast.interface_id),
		cl_ntoh16(p_iir->subscriber_enum),
		cl_ntoh64(p_iir->inform_info.gid.unicast.prefix),
		cl_ntoh64(p_iir->inform_info.gid.unicast.interface_id),
		cl_ntoh16(p_iir->inform_info.lid_range_begin),
		cl_ntoh16(p_iir->inform_info.lid_range_end),
		p_iir->inform_info.is_generic,
		p_iir->inform_info.subscribe,
		cl_ntoh16(p_iir->inform_info.trap_type),
		cl_ntoh16(p_iir->inform_info.g_or_v.generic.trap_num),
		cl_ntoh32(p_iir->inform_info.g_or_v.generic.qpn_resp_time_val),
		cl_ntoh32(ib_inform_info_get_prod_type(&p_iir->inform_info)),
		cl_ntoh16(p_infr->report_addr.dest_lid),
		p_infr->report_addr.path_bits,
		p_infr->report_addr.static_rate,
		cl_ntoh32(p_infr->report_addr.addr_type.gsi.remote_qp),
		cl_ntoh32(p_infr->report_addr.addr_type.gsi.remote_qkey),
		p_infr->report_addr.addr_type.gsi.pkey_ix,
		p_infr->report_addr.addr_type.gsi.service_level);
}

static void sa_dump_one_service(cl_list_item_t * p_list_item, void *cxt)
{
	FILE *file = ((struct opensm_dump_context *)cxt)->file;
	osm_svcr_t *p_svcr = (osm_svcr_t *) p_list_item;
	ib_service_record_t *p_sr = &p_svcr->service_record;

	fprintf(file, "Service Record: id=0x%016" PRIx64
		" gid=0x%016" PRIx64 ":0x%016" PRIx64
		" pkey=0x%x"
		" lease=0x%x"
		" key=0x%02x%02x%02x%02x%02x%02x%02x%02x"
		":0x%02x%02x%02x%02x%02x%02x%02x%02x"
		" name=\'%s\'"
		" data8=0x%02x%02x%02x%02x%02x%02x%02x%02x"
		":0x%02x%02x%02x%02x%02x%02x%02x%02x"
		" data16=0x%04x%04x%04x%04x:0x%04x%04x%04x%04x"
		" data32=0x%08x%08x:0x%08x%08x"
		" data64=0x%016" PRIx64 ":0x%016" PRIx64
		" modified_time=0x%x lease_period=0x%x\n\n",
		cl_ntoh64(p_sr->service_id),
		cl_ntoh64(p_sr->service_gid.unicast.prefix),
		cl_ntoh64(p_sr->service_gid.unicast.interface_id),
		cl_ntoh16(p_sr->service_pkey),
		cl_ntoh32(p_sr->service_lease),
		p_sr->service_key[0], p_sr->service_key[1],
		p_sr->service_key[2], p_sr->service_key[3],
		p_sr->service_key[4], p_sr->service_key[5],
		p_sr->service_key[6], p_sr->service_key[7],
		p_sr->service_key[8], p_sr->service_key[9],
		p_sr->service_key[10], p_sr->service_key[11],
		p_sr->service_key[12], p_sr->service_key[13],
		p_sr->service_key[14], p_sr->service_key[15],
		p_sr->service_name,
		p_sr->service_data8[0], p_sr->service_data8[1],
		p_sr->service_data8[2], p_sr->service_data8[3],
		p_sr->service_data8[4], p_sr->service_data8[5],
		p_sr->service_data8[6], p_sr->service_data8[7],
		p_sr->service_data8[8], p_sr->service_data8[9],
		p_sr->service_data8[10], p_sr->service_data8[11],
		p_sr->service_data8[12], p_sr->service_data8[13],
		p_sr->service_data8[14], p_sr->service_data8[15],
		cl_ntoh16(p_sr->service_data16[0]),
		cl_ntoh16(p_sr->service_data16[1]),
		cl_ntoh16(p_sr->service_data16[2]),
		cl_ntoh16(p_sr->service_data16[3]),
		cl_ntoh16(p_sr->service_data16[4]),
		cl_ntoh16(p_sr->service_data16[5]),
		cl_ntoh16(p_sr->service_data16[6]),
		cl_ntoh16(p_sr->service_data16[7]),
		cl_ntoh32(p_sr->service_data32[0]),
		cl_ntoh32(p_sr->service_data32[1]),
		cl_ntoh32(p_sr->service_data32[2]),
		cl_ntoh32(p_sr->service_data32[3]),
		cl_ntoh64(p_sr->service_data64[0]),
		cl_ntoh64(p_sr->service_data64[1]),
		p_svcr->modified_time, p_svcr->lease_period);
}

static void sa_dump_one_port_guidinfo(cl_map_item_t * p_map_item, void *cxt)
{
	FILE *file = ((struct opensm_dump_context *)cxt)->file;
	osm_port_t *p_port = (osm_port_t *) p_map_item;
	uint32_t max_block;
	int block_num;

	if (!p_port->p_physp->p_guids)
		return;

	max_block = (p_port->p_physp->port_info.guid_cap + GUID_TABLE_MAX_ENTRIES - 1) /
		     GUID_TABLE_MAX_ENTRIES;

	for (block_num = 0; block_num < max_block; block_num++) {
		fprintf(file, "GUIDInfo Record:"
			" base_guid=0x%016" PRIx64 " lid=0x%04x block_num=0x%x"
			" guid0=0x%016" PRIx64 " guid1=0x%016" PRIx64
			" guid2=0x%016" PRIx64 " guid3=0x%016" PRIx64
			" guid4=0x%016" PRIx64 " guid5=0x%016" PRIx64
			" guid6=0x%016" PRIx64 " guid7=0x%016" PRIx64
			"\n\n",
			cl_ntoh64((*p_port->p_physp->p_guids)[0]),
			cl_ntoh16(osm_port_get_base_lid(p_port)), block_num,
			cl_ntoh64((*p_port->p_physp->p_guids)[block_num * GUID_TABLE_MAX_ENTRIES]),
			cl_ntoh64((*p_port->p_physp->p_guids)[block_num * GUID_TABLE_MAX_ENTRIES + 1]),
			cl_ntoh64((*p_port->p_physp->p_guids)[block_num * GUID_TABLE_MAX_ENTRIES + 2]),
			cl_ntoh64((*p_port->p_physp->p_guids)[block_num * GUID_TABLE_MAX_ENTRIES + 3]),
			cl_ntoh64((*p_port->p_physp->p_guids)[block_num * GUID_TABLE_MAX_ENTRIES + 4]),
			cl_ntoh64((*p_port->p_physp->p_guids)[block_num * GUID_TABLE_MAX_ENTRIES + 5]),
			cl_ntoh64((*p_port->p_physp->p_guids)[block_num * GUID_TABLE_MAX_ENTRIES + 6]),
			cl_ntoh64((*p_port->p_physp->p_guids)[block_num * GUID_TABLE_MAX_ENTRIES + 7]));
	}
}

static void sa_dump_all_sa(osm_opensm_t * p_osm, FILE * file)
{
	struct opensm_dump_context dump_context;
	osm_mgrp_t *p_mgrp;

	dump_context.p_osm = p_osm;
	dump_context.file = file;
	OSM_LOG(&p_osm->log, OSM_LOG_DEBUG, "Dump guidinfo\n");
	cl_plock_acquire(&p_osm->lock);
	cl_qmap_apply_func(&p_osm->subn.port_guid_tbl,
			   sa_dump_one_port_guidinfo, &dump_context);
	OSM_LOG(&p_osm->log, OSM_LOG_DEBUG, "Dump multicast\n");
	for (p_mgrp = (osm_mgrp_t *) cl_fmap_head(&p_osm->subn.mgrp_mgid_tbl);
	     p_mgrp != (osm_mgrp_t *) cl_fmap_end(&p_osm->subn.mgrp_mgid_tbl);
	     p_mgrp = (osm_mgrp_t *) cl_fmap_next(&p_mgrp->map_item))
		sa_dump_one_mgrp(p_mgrp, &dump_context);
	OSM_LOG(&p_osm->log, OSM_LOG_DEBUG, "Dump inform\n");
	cl_qlist_apply_func(&p_osm->subn.sa_infr_list,
			    sa_dump_one_inform, &dump_context);
	OSM_LOG(&p_osm->log, OSM_LOG_DEBUG, "Dump services\n");
	cl_qlist_apply_func(&p_osm->subn.sa_sr_list,
			    sa_dump_one_service, &dump_context);
	cl_plock_release(&p_osm->lock);
}

int osm_sa_db_file_dump(osm_opensm_t * p_osm)
{
	int res = 1;
	if (p_osm->sa.dirty) {
		res = opensm_dump_to_file(
			p_osm, "opensm-sa.dump", sa_dump_all_sa);
		p_osm->sa.dirty = FALSE;
	}
	return res;
}

/*
 *  SA DB Loader
 */
static osm_mgrp_t *load_mcgroup(osm_opensm_t * p_osm, ib_net16_t mlid,
				ib_member_rec_t * p_mcm_rec,
				unsigned well_known)
{
	ib_net64_t comp_mask;
	osm_mgrp_t *p_mgrp;

	cl_plock_excl_acquire(&p_osm->lock);

	p_mgrp = osm_get_mgrp_by_mgid(&p_osm->subn, &p_mcm_rec->mgid);
	if (p_mgrp) {
		if (p_mgrp->mlid == mlid) {
			OSM_LOG(&p_osm->log, OSM_LOG_DEBUG,
				"mgrp %04x is already here.", cl_ntoh16(mlid));
			goto _out;
		}
		OSM_LOG(&p_osm->log, OSM_LOG_VERBOSE,
			"mlid %04x is already used by another MC group. Will "
			"request clients reregistration.\n", cl_ntoh16(mlid));
		p_mgrp = NULL;
		goto _out;
	}

	comp_mask = IB_MCR_COMPMASK_MTU | IB_MCR_COMPMASK_MTU_SEL
	    | IB_MCR_COMPMASK_RATE | IB_MCR_COMPMASK_RATE_SEL;
	if (!(p_mgrp = osm_mcmr_rcv_find_or_create_new_mgrp(&p_osm->sa,
							    comp_mask,
							    p_mcm_rec)) ||
	    p_mgrp->mlid != mlid) {
		OSM_LOG(&p_osm->log, OSM_LOG_ERROR,
			"cannot create MC group with mlid 0x%04x and mgid "
			"0x%016" PRIx64 ":0x%016" PRIx64 "\n", cl_ntoh16(mlid),
			cl_ntoh64(p_mcm_rec->mgid.unicast.prefix),
			cl_ntoh64(p_mcm_rec->mgid.unicast.interface_id));
		p_mgrp = NULL;
	} else if (well_known)
		p_mgrp->well_known = TRUE;

_out:
	cl_plock_release(&p_osm->lock);

	return p_mgrp;
}

static int load_svcr(osm_opensm_t * p_osm, ib_service_record_t * sr,
		     uint32_t modified_time, uint32_t lease_period)
{
	osm_svcr_t *p_svcr;
	int ret = 0;

	cl_plock_excl_acquire(&p_osm->lock);

	if (osm_svcr_get_by_rid(&p_osm->subn, &p_osm->log, sr)) {
		OSM_LOG(&p_osm->log, OSM_LOG_VERBOSE,
			"ServiceRecord already exists\n");
		goto _out;
	}

	if (!(p_svcr = osm_svcr_new(sr))) {
		OSM_LOG(&p_osm->log, OSM_LOG_ERROR,
			"cannot allocate new service struct\n");
		ret = -1;
		goto _out;
	}

	p_svcr->modified_time = modified_time;
	p_svcr->lease_period = lease_period;

	OSM_LOG(&p_osm->log, OSM_LOG_DEBUG, "adding ServiceRecord...\n");

	osm_svcr_insert_to_db(&p_osm->subn, &p_osm->log, p_svcr);

	if (lease_period != 0xffffffff)
		cl_timer_trim(&p_osm->sa.sr_timer, 1000);

_out:
	cl_plock_release(&p_osm->lock);

	return ret;
}

static int load_infr(osm_opensm_t * p_osm, ib_inform_info_record_t * iir,
		     osm_mad_addr_t * addr)
{
	osm_infr_t infr, *p_infr;
	int ret = 0;

	infr.h_bind = p_osm->sa.mad_ctrl.h_bind;
	infr.sa = &p_osm->sa;
	/* other possible way to restore mad_addr partially is
	   to extract qpn from InformInfo and to find lid by gid */
	infr.report_addr = *addr;
	infr.inform_record = *iir;

	cl_plock_excl_acquire(&p_osm->lock);
	if (osm_infr_get_by_rec(&p_osm->subn, &p_osm->log, &infr)) {
		OSM_LOG(&p_osm->log, OSM_LOG_VERBOSE,
			"InformInfo Record already exists\n");
		goto _out;
	}

	if (!(p_infr = osm_infr_new(&infr))) {
		OSM_LOG(&p_osm->log, OSM_LOG_ERROR,
			"cannot allocate new infr struct\n");
		ret = -1;
		goto _out;
	}

	OSM_LOG(&p_osm->log, OSM_LOG_DEBUG, "adding InformInfo Record...\n");

	osm_infr_insert_to_db(&p_osm->subn, &p_osm->log, p_infr);

_out:
	cl_plock_release(&p_osm->lock);

	return ret;
}

static int load_guidinfo(osm_opensm_t * p_osm, ib_net64_t base_guid,
			 ib_guidinfo_record_t *gir)
{
	osm_port_t *p_port;
	uint32_t max_block;
	int i, ret = 0;
	osm_alias_guid_t *p_alias_guid, *p_alias_guid_check;

	cl_plock_excl_acquire(&p_osm->lock);

	p_port = osm_get_port_by_guid(&p_osm->subn, base_guid);
	if (!p_port)
		goto _out;

	if (!p_port->p_physp->p_guids) {
		max_block = (p_port->p_physp->port_info.guid_cap + GUID_TABLE_MAX_ENTRIES - 1) /
			     GUID_TABLE_MAX_ENTRIES;
		p_port->p_physp->p_guids = calloc(max_block * GUID_TABLE_MAX_ENTRIES,
						  sizeof(ib_net64_t));
		if (!p_port->p_physp->p_guids) {
			OSM_LOG(&p_osm->log, OSM_LOG_ERROR,
				"cannot allocate GUID table for port "
				"GUID 0x%" PRIx64 "\n",
				cl_ntoh64(p_port->p_physp->port_guid));
			goto _out;
		}
	}

	for (i = 0; i < GUID_TABLE_MAX_ENTRIES; i++) {
		if (!gir->guid_info.guid[i])
			continue;
		/* skip block 0 index 0 */
		if (gir->block_num == 0 && i == 0)
			continue;
		if (gir->block_num * GUID_TABLE_MAX_ENTRIES + i >
		    p_port->p_physp->port_info.guid_cap)
			break;

		p_alias_guid = osm_alias_guid_new(gir->guid_info.guid[i],
						  p_port);
		if (!p_alias_guid) {
			OSM_LOG(&p_osm->log, OSM_LOG_ERROR,
				"Alias guid %d memory allocation failed"
				" for port GUID 0x%" PRIx64 "\n",
				gir->block_num * GUID_TABLE_MAX_ENTRIES + i,
				cl_ntoh64(p_port->p_physp->port_guid));
			goto _out;
		}

		p_alias_guid_check =
			(osm_alias_guid_t *) cl_qmap_insert(&p_osm->subn.alias_port_guid_tbl,
							    p_alias_guid->alias_guid,
							    &p_alias_guid->map_item);
		if (p_alias_guid_check != p_alias_guid) {
			/* alias GUID is a duplicate */
			OSM_LOG(&p_osm->log, OSM_LOG_ERROR,
				"Duplicate alias port GUID 0x%" PRIx64
				" index %d base port GUID 0x%" PRIx64 "\n",
				cl_ntoh64(p_alias_guid->alias_guid),
				gir->block_num * GUID_TABLE_MAX_ENTRIES + i,
				cl_ntoh64(p_alias_guid->p_base_port->guid));
			osm_alias_guid_delete(&p_alias_guid);
			goto _out;
		}
	}

	memcpy(&(*p_port->p_physp->p_guids)[gir->block_num * GUID_TABLE_MAX_ENTRIES],
	       &gir->guid_info, sizeof(ib_guid_info_t));

	osm_queue_guidinfo(&p_osm->sa, p_port, gir->block_num);

_out:
	cl_plock_release(&p_osm->lock);

	return ret;
}

#define UNPACK_FUNC(name,x) \
static int unpack_##name##x(char *p, uint##x##_t *val_ptr) \
{ \
	char *q; \
	unsigned long long num; \
	num = strtoull(p, &q, 16); \
	if (num > ~((uint##x##_t)0x0) \
	    || q == p || (!isspace(*q) && *q != ':')) { \
		*val_ptr = 0; \
		return -1; \
	} \
	*val_ptr = cl_hton##x((uint##x##_t)num); \
	return (int)(q - p); \
}

#define cl_hton8(x) (x)

UNPACK_FUNC(net, 8);
UNPACK_FUNC(net, 16);
UNPACK_FUNC(net, 32);
UNPACK_FUNC(net, 64);

static int unpack_string(char *p, uint8_t * buf, unsigned len)
{
	char *q = p;
	char delim = ' ';

	if (*q == '\'' || *q == '\"')
		delim = *q++;
	while (--len && *q && *q != delim)
		*buf++ = *q++;
	*buf = '\0';
	if (*q == delim && delim != ' ')
		q++;
	return (int)(q - p);
}

static int unpack_string64(char *p, uint8_t * buf)
{
	return unpack_string(p, buf, 64);
}

#define PARSE_AHEAD(p, x, name, val_ptr) { int _ret; \
	p = strstr(p, name); \
	if (!p) { \
		OSM_LOG(&p_osm->log, OSM_LOG_ERROR, \
			"PARSE ERROR: %s:%u: cannot find \"%s\" string\n", \
			file_name, lineno, (name)); \
		ret = -2; \
		goto _error; \
	} \
	p += strlen(name); \
	_ret = unpack_##x(p, (val_ptr)); \
	if (_ret < 0) { \
		OSM_LOG(&p_osm->log, OSM_LOG_ERROR, \
			"PARSE ERROR: %s:%u: cannot parse "#x" value " \
			"after \"%s\"\n", file_name, lineno, (name)); \
		ret = _ret; \
		goto _error; \
	} \
	p += _ret; \
}

int osm_sa_db_file_load(osm_opensm_t * p_osm)
{
	char line[1024];
	char *file_name;
	FILE *file;
	int ret = 0;
	osm_mgrp_t *p_mgrp = NULL;
	unsigned rereg_clients = 0;
	unsigned lineno;

	if (!p_osm->subn.first_time_master_sweep) {
		OSM_LOG(&p_osm->log, OSM_LOG_VERBOSE,
			"Not first sweep - skip SA DB restore\n");
		return 0;
	}

	file_name = p_osm->subn.opt.sa_db_file;
	if (!file_name) {
		OSM_LOG(&p_osm->log, OSM_LOG_VERBOSE,
			"sa db file name is not specifed. Skip restore\n");
		return 0;
	}

	file = fopen(file_name, "r");
	if (!file) {
		OSM_LOG(&p_osm->log, OSM_LOG_ERROR | OSM_LOG_SYS, "ERR 4C02: "
			"Can't open sa db file \'%s\'. Skip restoring\n",
			file_name);
		return -1;
	}

	OSM_LOG(&p_osm->log, OSM_LOG_VERBOSE,
		"Restoring SA DB from file \'%s\'\n",
		file_name);

	lineno = 0;

	while (fgets(line, sizeof(line) - 1, file) != NULL) {
		char *p;
		uint8_t val;

		lineno++;

		p = line;
		while (isspace(*p))
			p++;

		if (*p == '#')
			continue;

		if (!strncmp(p, "MC Group", 8)) {
			ib_member_rec_t mcm_rec;
			ib_net16_t mlid;
			unsigned well_known = 0;

			p_mgrp = NULL;
			memset(&mcm_rec, 0, sizeof(mcm_rec));

			PARSE_AHEAD(p, net16, " 0x", &mlid);
			if (strstr(p, "well known"))
				well_known = 1;
			PARSE_AHEAD(p, net64, " mgid=0x",
				    &mcm_rec.mgid.unicast.prefix);
			PARSE_AHEAD(p, net64, ":0x",
				    &mcm_rec.mgid.unicast.interface_id);
			PARSE_AHEAD(p, net64, " port_gid=0x",
				    &mcm_rec.port_gid.unicast.prefix);
			PARSE_AHEAD(p, net64, ":0x",
				    &mcm_rec.port_gid.unicast.interface_id);
			PARSE_AHEAD(p, net32, " qkey=0x", &mcm_rec.qkey);
			PARSE_AHEAD(p, net16, " mlid=0x", &mcm_rec.mlid);
			PARSE_AHEAD(p, net8, " mtu=0x", &mcm_rec.mtu);
			PARSE_AHEAD(p, net8, " tclass=0x", &mcm_rec.tclass);
			PARSE_AHEAD(p, net16, " pkey=0x", &mcm_rec.pkey);
			PARSE_AHEAD(p, net8, " rate=0x", &mcm_rec.rate);
			PARSE_AHEAD(p, net8, " pkt_life=0x", &mcm_rec.pkt_life);
			PARSE_AHEAD(p, net32, " sl_flow_hop=0x",
				    &mcm_rec.sl_flow_hop);
			PARSE_AHEAD(p, net8, " scope_state=0x",
				    &mcm_rec.scope_state);
			PARSE_AHEAD(p, net8, " proxy_join=0x", &val);
			mcm_rec.proxy_join = val;

			p_mgrp = load_mcgroup(p_osm, mlid, &mcm_rec,
					      well_known);
			if (!p_mgrp)
				rereg_clients = 1;
			if (cl_ntoh16(mlid) > p_osm->sm.mlids_init_max)
				p_osm->sm.mlids_init_max = cl_ntoh16(mlid);
		} else if (p_mgrp && !strncmp(p, "mcm_port", 8)) {
			ib_member_rec_t mcmr;
			ib_net64_t guid;
			osm_port_t *port;
			boolean_t proxy;

			PARSE_AHEAD(p, net64, " port_gid=0x",
				    &mcmr.port_gid.unicast.prefix);
			PARSE_AHEAD(p, net64, ":0x",
				    &mcmr.port_gid.unicast.interface_id);
			PARSE_AHEAD(p, net8, " scope_state=0x", &mcmr.scope_state);
			PARSE_AHEAD(p, net8, " proxy_join=0x", &val);
			proxy = val;

			guid = mcmr.port_gid.unicast.interface_id;
			port = osm_get_port_by_alias_guid(&p_osm->subn, guid);
			if (port &&
			    cl_qmap_get(&p_mgrp->mcm_port_tbl, guid) ==
			    cl_qmap_end(&p_mgrp->mcm_port_tbl) &&
			    !osm_mgrp_add_port(&p_osm->subn, &p_osm->log,
						p_mgrp, port, &mcmr, proxy))
				rereg_clients = 1;
		} else if (!strncmp(p, "Service Record:", 15)) {
			ib_service_record_t s_rec;
			uint32_t modified_time, lease_period;

			p_mgrp = NULL;
			memset(&s_rec, 0, sizeof(s_rec));

			PARSE_AHEAD(p, net64, " id=0x", &s_rec.service_id);
			PARSE_AHEAD(p, net64, " gid=0x",
				    &s_rec.service_gid.unicast.prefix);
			PARSE_AHEAD(p, net64, ":0x",
				    &s_rec.service_gid.unicast.interface_id);
			PARSE_AHEAD(p, net16, " pkey=0x", &s_rec.service_pkey);
			PARSE_AHEAD(p, net32, " lease=0x",
				    &s_rec.service_lease);
			PARSE_AHEAD(p, net64, " key=0x",
				    (ib_net64_t *) (&s_rec.service_key[0]));
			PARSE_AHEAD(p, net64, ":0x",
				    (ib_net64_t *) (&s_rec.service_key[8]));
			PARSE_AHEAD(p, string64, " name=", s_rec.service_name);
			PARSE_AHEAD(p, net64, " data8=0x",
				    (ib_net64_t *) (&s_rec.service_data8[0]));
			PARSE_AHEAD(p, net64, ":0x",
				    (ib_net64_t *) (&s_rec.service_data8[8]));
			PARSE_AHEAD(p, net64, " data16=0x",
				    (ib_net64_t *) (&s_rec.service_data16[0]));
			PARSE_AHEAD(p, net64, ":0x",
				    (ib_net64_t *) (&s_rec.service_data16[4]));
			PARSE_AHEAD(p, net64, " data32=0x",
				    (ib_net64_t *) (&s_rec.service_data32[0]));
			PARSE_AHEAD(p, net64, ":0x",
				    (ib_net64_t *) (&s_rec.service_data32[2]));
			PARSE_AHEAD(p, net64, " data64=0x",
				    &s_rec.service_data64[0]);
			PARSE_AHEAD(p, net64, ":0x", &s_rec.service_data64[1]);
			PARSE_AHEAD(p, net32, " modified_time=0x",
				    &modified_time);
			PARSE_AHEAD(p, net32, " lease_period=0x",
				    &lease_period);

			if (load_svcr(p_osm, &s_rec, cl_ntoh32(modified_time),
				      cl_ntoh32(lease_period)))
				rereg_clients = 1;
		} else if (!strncmp(p, "InformInfo Record:", 18)) {
			ib_inform_info_record_t i_rec;
			osm_mad_addr_t rep_addr;
			ib_net16_t val16;

			p_mgrp = NULL;
			memset(&i_rec, 0, sizeof(i_rec));
			memset(&rep_addr, 0, sizeof(rep_addr));

			PARSE_AHEAD(p, net64, " subscriber_gid=0x",
				    &i_rec.subscriber_gid.unicast.prefix);
			PARSE_AHEAD(p, net64, ":0x",
				    &i_rec.subscriber_gid.unicast.interface_id);
			PARSE_AHEAD(p, net16, " subscriber_enum=0x",
				    &i_rec.subscriber_enum);
			PARSE_AHEAD(p, net64, " gid=0x",
				    &i_rec.inform_info.gid.unicast.prefix);
			PARSE_AHEAD(p, net64, ":0x",
				    &i_rec.inform_info.gid.unicast.
				    interface_id);
			PARSE_AHEAD(p, net16, " lid_range_begin=0x",
				    &i_rec.inform_info.lid_range_begin);
			PARSE_AHEAD(p, net16, " lid_range_end=0x",
				    &i_rec.inform_info.lid_range_end);
			PARSE_AHEAD(p, net8, " is_generic=0x",
				    &i_rec.inform_info.is_generic);
			PARSE_AHEAD(p, net8, " subscribe=0x",
				    &i_rec.inform_info.subscribe);
			PARSE_AHEAD(p, net16, " trap_type=0x",
				    &i_rec.inform_info.trap_type);
			PARSE_AHEAD(p, net16, " trap_num=0x",
				    &i_rec.inform_info.g_or_v.generic.trap_num);
			PARSE_AHEAD(p, net32, " qpn_resp_time_val=0x",
				    &i_rec.inform_info.g_or_v.generic.
				    qpn_resp_time_val);
			PARSE_AHEAD(p, net32, " node_type=0x",
				    (uint32_t *) & i_rec.inform_info.g_or_v.
				    generic.reserved2);

			PARSE_AHEAD(p, net16, " rep_addr: lid=0x",
				    &rep_addr.dest_lid);
			PARSE_AHEAD(p, net8, " path_bits=0x",
				    &rep_addr.path_bits);
			PARSE_AHEAD(p, net8, " static_rate=0x",
				    &rep_addr.static_rate);
			PARSE_AHEAD(p, net32, " remote_qp=0x",
				    &rep_addr.addr_type.gsi.remote_qp);
			PARSE_AHEAD(p, net32, " remote_qkey=0x",
				    &rep_addr.addr_type.gsi.remote_qkey);
			PARSE_AHEAD(p, net16, " pkey_ix=0x", &val16);
			rep_addr.addr_type.gsi.pkey_ix = cl_ntoh16(val16);
			PARSE_AHEAD(p, net8, " sl=0x",
				    &rep_addr.addr_type.gsi.service_level);

			if (load_infr(p_osm, &i_rec, &rep_addr))
				rereg_clients = 1;
		} else if (!strncmp(p, "GUIDInfo Record:", 16)) {
			ib_guidinfo_record_t gi_rec;
			ib_net64_t base_guid;

			p_mgrp = NULL;
			memset(&gi_rec, 0, sizeof(gi_rec));

			PARSE_AHEAD(p, net64, " base_guid=0x", &base_guid);
			PARSE_AHEAD(p, net16, " lid=0x", &gi_rec.lid);
			PARSE_AHEAD(p, net8, " block_num=0x",
				    &gi_rec.block_num);
			PARSE_AHEAD(p, net64, " guid0=0x",
				    &gi_rec.guid_info.guid[0]);
			PARSE_AHEAD(p, net64, " guid1=0x",
				    &gi_rec.guid_info.guid[1]);
			PARSE_AHEAD(p, net64, " guid2=0x",
				    &gi_rec.guid_info.guid[2]);
			PARSE_AHEAD(p, net64, " guid3=0x",
				    &gi_rec.guid_info.guid[3]);
			PARSE_AHEAD(p, net64, " guid4=0x",
				    &gi_rec.guid_info.guid[4]);
			PARSE_AHEAD(p, net64, " guid5=0x",
				    &gi_rec.guid_info.guid[5]);
			PARSE_AHEAD(p, net64, " guid6=0x",
				    &gi_rec.guid_info.guid[6]);
			PARSE_AHEAD(p, net64, " guid7=0x",
				    &gi_rec.guid_info.guid[7]);

			if (load_guidinfo(p_osm, base_guid, &gi_rec))
				rereg_clients = 1;
		}
	}

	/*
	 * If loading succeeded, do whatever 'no_clients_rereg' says.
	 * If loading failed at some point, turn off the 'no_clients_rereg'
	 * option (turn on re-registration requests).
	 */
	if (rereg_clients)
		p_osm->subn.opt.no_clients_rereg = FALSE;

	/* We've just finished loading SA DB file - clear the "dirty" flag */
	p_osm->sa.dirty = FALSE;

_error:
	fclose(file);
	return ret;
}
