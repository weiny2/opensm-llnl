/*
 * Copyright (c) 2012 Lawrence Livermore National Security.  All rights * reserved
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
 *    Additional implementation to supplement opensm with RDMA
 *    xfer's when requested from a client
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <opensm/osm_rdma.h>

#define FILE_ID OSM_FILE_RDMA_C

/* =========================================================================
 * Begin RDMA code
 * ========================================================================= */

static ib_api_status_t osm_rdma_add_qp(IN struct rdma_ctx *rdma_ctx, struct ibv_qp *qp)
{
	struct rdma_qp *tmp = malloc(sizeof(*tmp));
	if (!tmp) {
		return (IB_INSUFFICIENT_RESOURCES);
	}

	tmp->qp = qp;
	cl_spinlock_construct(&tmp->attached);
	cl_spinlock_init(&tmp->attached);
	tmp->next = rdma_ctx->qps_head;
	rdma_ctx->qps_head = tmp;
	tmp->ctx = rdma_ctx;

	return (IB_SUCCESS);
}

/*
// FIXME need to clean this up sometime...
static ib_api_status_t osm_rdma_remove_qp(struct rdma_qp *qp)
{
	return (IB_SUCCESS);
}
*/

ib_api_status_t osm_rdma_detach_qp(IN struct rdma_ctx *rdma_ctx,
				struct rdma_qp *qp)
{
	// Transition to RESET
	{
		struct ibv_qp_attr attr = {
			.qp_state        = IBV_QPS_RESET
		};

		if (ibv_modify_qp(qp->qp, &attr, IBV_QP_STATE)) {
			OSM_LOG(rdma_ctx->p_log, OSM_LOG_ERROR,
				"RDMA: Failed to modify QP to RESET\n");
			goto DestroyQP;
		}
	}

	// Transition from Reset to INIT
	{
		struct ibv_qp_attr attr = {
			.qp_state        = IBV_QPS_INIT,
			.pkey_index      = 0,
			.port_num        = rdma_ctx->device_port,
			.qp_access_flags = 0
		};

		if (ibv_modify_qp(qp->qp, &attr,
				  IBV_QP_STATE              |
				  IBV_QP_PKEY_INDEX         |
				  IBV_QP_PORT               |
				  IBV_QP_ACCESS_FLAGS)) {
			OSM_LOG(rdma_ctx->p_log, OSM_LOG_ERROR,
				"RDMA: Failed to modify QP to INIT\n");
			goto DestroyQP;
		}
	}
	cl_spinlock_release(&qp->attached);

	return (IB_SUCCESS);
DestroyQP:
	// FIXME remove QP completely
fprintf(stderr, "Well that didn't work.  QP RTS -> RESET -> INIT");
	exit(1);
}

ib_api_status_t osm_rdma_attach_qp(IN struct rdma_ctx *rdma_ctx,
				struct rdma_client_eth *eth_info,
				struct ibv_sa_path_rec *path,
				struct rdma_qp **rdma_qp)
{
	/* FIXME find eth_info->qpn from list */
	struct rdma_qp *rc = rdma_ctx->qps_head;
	struct ibv_qp *qp = rdma_ctx->qps_head->qp;

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
			.port_num	= rdma_ctx->device_port
		}
	};

	*rdma_qp = NULL;

	OSM_LOG(rdma_ctx->p_log, OSM_LOG_DEBUG,
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
		rdma_ctx->device_port);

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
		OSM_LOG(rdma_ctx->p_log, OSM_LOG_ERROR,
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
		osm_rdma_detach_qp(rdma_ctx, rc);
		OSM_LOG(rdma_ctx->p_log, OSM_LOG_ERROR,
			"Failed to modify QP to RTS: %s\n", strerror(errno));
		return 1;
	}

	OSM_LOG(rdma_ctx->p_log, OSM_LOG_DEBUG, "QP 0x%x connected to Client QPn 0x%x\n",
		qp->qp_num, eth_info->qpn);

	*rdma_qp = rc;
	return 0;
}

static ib_api_status_t osm_rdma_create_qp(IN struct rdma_ctx *rdma_ctx)
{
	struct ibv_qp *qp;

	struct ibv_qp_init_attr attr = {
		.send_cq = rdma_ctx->cq,
		.recv_cq = rdma_ctx->cq,
		.cap     = {
			.max_send_wr  = 10,
			.max_recv_wr  = 500,
			.max_send_sge = 1,
			.max_recv_sge = 1
		},
		.qp_type = IBV_QPT_RC
	};
	
	qp = ibv_create_qp(rdma_ctx->pd, &attr);
	if (!qp) {
		OSM_LOG(rdma_ctx->p_log, OSM_LOG_ERROR,
			"RDMA: Failed to create QP\n");
		return (IB_INSUFFICIENT_RESOURCES);
	}

	{
		struct ibv_qp_attr attr = {
			.qp_state        = IBV_QPS_INIT,
			.pkey_index      = 0,
			.port_num        = rdma_ctx->device_port,
			.qp_access_flags = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_LOCAL_WRITE
		};

		if (ibv_modify_qp(qp, &attr,
				  IBV_QP_STATE              |
				  IBV_QP_PKEY_INDEX         |
				  IBV_QP_PORT               |
				  IBV_QP_ACCESS_FLAGS)) {
			OSM_LOG(rdma_ctx->p_log, OSM_LOG_ERROR,
				"RDMA: Failed to modify QP to INIT\n");
			goto DestroyQP;
		}
	}

	if (osm_rdma_add_qp(rdma_ctx, qp) != IB_SUCCESS) {
		OSM_LOG(rdma_ctx->p_log, OSM_LOG_ERROR,
			"RDMA: Failed to add QP to pool\n");
		goto DestroyQP;
	}

	OSM_LOG(rdma_ctx->p_log, OSM_LOG_INFO, "QPn = %d\n", qp->qp_num);
	printf("qp %p; QPn = %d\n", qp, qp->qp_num);
	return (IB_SUCCESS);

DestroyQP:
	ibv_destroy_qp(qp);
	return (IB_INSUFFICIENT_RESOURCES);
}

static struct ibv_device *osm_find_RDMA_dev_from_port_guid(
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

ib_api_status_t osm_rdma_init(IN ib_net64_t port_guid,
				IN osm_log_t *p_log,
				OUT struct rdma_ctx *rdma_ctx)
{
	struct ibv_device *dev = NULL;
	struct ibv_device **dev_list;
	int num_dev;
	uint64_t guid = cl_ntoh64(port_guid);

	memset(rdma_ctx, 0, sizeof(*rdma_ctx));

	rdma_ctx->p_log = p_log;

	dev_list = ibv_get_device_list(&num_dev);
	if (!dev_list) {
		OSM_LOG(p_log, OSM_LOG_ERROR,
			"RDMA: Failed to get device list\n");
		return (IB_INSUFFICIENT_RESOURCES);
	}

	dev = rdma_ctx->dev = osm_find_RDMA_dev_from_port_guid(
					guid,
					dev_list, num_dev,
					&rdma_ctx->device_port,
					&rdma_ctx->dev_ctx);

	if (!dev) {
		OSM_LOG(p_log, OSM_LOG_ERROR,
			"RDMA: Failed to find RDMA device for port guid: "
			"%"PRIx64"\n", guid);
		return (IB_INSUFFICIENT_RESOURCES);
	}

	OSM_LOG(p_log, OSM_LOG_DEBUG,
			"RDMA: opened RDMA device: %s\n",
			ibv_get_device_name(dev));

	// Looks like you may not be able to do this here.
	// If so store the list for later "freeing"
	ibv_free_device_list(dev_list);

#if 0
	rdma_ctx->comp_ch = ibv_create_comp_channel(rdma_ctx->dev_ctx);
	if (!rdma_ctx->comp_ch) {
		OSM_LOG(p_log, OSM_LOG_ERROR,
			"RDMA: Failed to create completion channel: %s\n",
			ibv_get_device_name(dev));
		goto CloseDevice;
	}
#endif
	// skip this for now
	rdma_ctx->comp_ch = NULL;

	rdma_ctx->cq = ibv_create_cq(rdma_ctx->dev_ctx, 1000,
			     (void *)rdma_ctx,
			     rdma_ctx->comp_ch,
			     0);
	if (!rdma_ctx->cq) {
		OSM_LOG(p_log, OSM_LOG_ERROR,
			"RDMA: Failed to create CQ: %s\n",
			ibv_get_device_name(dev));
		goto DestroyCompCh;
	}

	if (rdma_ctx->comp_ch) {
		if ((ibv_req_notify_cq(rdma_ctx->cq, 0)) != 0) {
			OSM_LOG(p_log, OSM_LOG_ERROR,
				"RDMA: Request Notify CQ failed: %s\n",
				ibv_get_device_name(dev));
			goto DestroyCQ;
		}
	}

	rdma_ctx->pd = ibv_alloc_pd(rdma_ctx->dev_ctx);
	if (!rdma_ctx->pd) {
		OSM_LOG(p_log, OSM_LOG_ERROR,
			"RDMA: Failed to allocate the PD: %s\n",
			ibv_get_device_name(dev));
		goto DestroyCQ;
	}

	OSM_LOG(p_log, OSM_LOG_INFO, "SA RDMA configured on %s:%d\n",
			ibv_get_device_name(dev),
			rdma_ctx->device_port);
	return (osm_rdma_create_qp(rdma_ctx)); // FIXME create more than 1 QP

DestroyCQ:
	ibv_destroy_cq(rdma_ctx->cq);
DestroyCompCh:
	if (rdma_ctx->comp_ch) {
		ibv_destroy_comp_channel(rdma_ctx->comp_ch);
	}
//CloseDevice:
	ibv_close_device(rdma_ctx->dev_ctx);
	return (IB_INSUFFICIENT_RESOURCES);
}

/* allocate a buffer and register it */
struct rdma_memory * osm_rdma_malloc(struct rdma_ctx *rdma_ctx, size_t size)
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
	rc->mr = ibv_reg_mr(rdma_ctx->pd, rc->buf, rc->size,
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

void osm_rdma_free(struct rdma_memory *mem)
{
	ibv_dereg_mr(mem->mr);
	free(mem->buf);
	free(mem);
}

int osm_rdma_post_send(struct rdma_qp *sm_qp,
			struct rdma_memory *buf, struct rdma_client_eth *eth_info)
{
	int rc = 0;
	struct ibv_qp *qp = sm_qp->qp;
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

	//OSM_LOG(sm_qp->ctx->p_log, OSM_LOG_VERBOSE,
	printf("addr %"PRIx64"; rkey %x; length %x\n",
		(uint64_t)eth_info->addr,
		eth_info->r_key,
		eth_info->length);


	if ((rc = ibv_post_send(qp, &wr, &bad_wr)) != 0) {
		OSM_LOG(sm_qp->ctx->p_log, OSM_LOG_ERROR,
			"SA RDMA: Failed to post send: %d\n", rc);
		return (-EFAULT);
	}

	return (0);
}

ib_api_status_t osm_rdma_wait_completion(IN struct rdma_ctx *rdma_ctx)
{
	struct ibv_cq *cq = rdma_ctx->cq;
	struct ibv_wc wc;
	int rc = 0;

	/*
	// FIXME use channel
	if (rdma_ctx->comp_ch) {
		ibv_get_cq_event(rdma_ctx->comp_ch, &cq, NULL);
		ibv_ack_cq_events(cq, 1);
		ibv_req_notify_cq(cq, 0);
	}
	*/
	do {
		rc = ibv_poll_cq(cq, 1, &wc);
		if (rc < 0)
			break;
	} while (rc == 0);

	if (rc <= 0 || wc.status != IBV_WC_SUCCESS) {
		OSM_LOG(rdma_ctx->p_log, OSM_LOG_ERROR,
			"RDMA Work completion error, rc %d; status: '%s'\n",
			rc, ibv_wc_status_str(wc.status));
		return (IB_ERROR);
	}
	return (IB_SUCCESS);
}

ib_api_status_t osm_rdma_close(IN struct rdma_ctx * rdma_ctx)
{
	ibv_dealloc_pd(rdma_ctx->pd);
	ibv_destroy_cq(rdma_ctx->cq);
	if (rdma_ctx->comp_ch) {
		ibv_destroy_comp_channel(rdma_ctx->comp_ch);
	}
	ibv_close_device(rdma_ctx->dev_ctx);
	return (IB_SUCCESS);
}

