/*
 * Copyright (c) 2012 Lawrence Livermore National Security. All rights reserved.
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
 */

#include <sys/time.h>

#include <complib/cl_types.h>
#include <complib/cl_spinlock.h>

#include <iba/ib_types.h>

#include <opensm/osm_log.h>


#include <infiniband/verbs.h>
#include <infiniband/sa.h>


#ifndef _OSM_SA_RDMA_H_
#define _OSM_SA_RDMA_H_

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

#define SA_RDMA_REQUEST (CL_HTON16(0x0001))
#define SA_RDMA_COMPLETE (CL_HTON16(0x0009))
#define SA_SEND_WRID (uint64_t)(0xDEADBEEF)

struct rdma_memory {
	struct ibv_mr *mr;
	uint8_t       *buf;
	size_t         size;
};

/* Information about the clients QP and memory buffer */
struct rdma_client_eth {
	uint32_t qpn;
	uint64_t addr;
	uint32_t r_key;
	uint32_t length;
} __attribute__((packed));

struct rdma_ctx;
struct rdma_qp {
	struct rdma_qp *next;
	struct rdma_ctx *ctx;
	struct ibv_qp *qp;
	cl_spinlock_t attached;
	/* perhaps will want to have a separate CQ for each QP? */
	/* struct ibv_cq *cq; */
};

struct rdma_ctx {
	osm_log_t *p_log;
	struct ibv_device *dev;
	int device_port;
	struct ibv_context *dev_ctx;
	struct ibv_pd *pd;
	/* perhaps will want to have a separate CQ for each QP?
	 * for now just have 1
	 */
	struct ibv_cq *cq;
	struct ibv_comp_channel *comp_ch;
	struct rdma_qp *qps_head;
};

ib_api_status_t osm_rdma_init(ib_net64_t port_guid,
				osm_log_t *p_log,
				struct rdma_ctx *ctx);

ib_api_status_t osm_rdma_attach_qp(struct rdma_ctx *rdma_ctx,
				struct rdma_client_eth *eth_info,
				struct ibv_sa_path_rec *path,
				struct rdma_qp **qp);

int osm_rdma_post_send(struct rdma_qp *sm_qp,
			struct rdma_memory *buf, struct rdma_client_eth *eth_info);

ib_api_status_t osm_rdma_detach_qp(struct rdma_ctx *rdma_ctx,
				struct rdma_qp *qp);

ib_api_status_t osm_rdma_wait_completion(struct rdma_ctx * rdma_ctx);
ib_api_status_t osm_rdma_close(struct rdma_ctx * rdma_ctx);


struct rdma_memory *osm_rdma_malloc(struct rdma_ctx * rdma_ctx, size_t size);
void osm_rdma_free(struct rdma_memory *mem);
static inline uint8_t * rdma_mem_get_buf(struct rdma_memory *mem) { return mem->buf; }


/* after and diff can be the same struct */
static inline void diff_time(struct timeval *before, struct timeval *after,
			     struct timeval *diff)
{
	struct timeval tmp = *after;
	if (tmp.tv_usec < before->tv_usec) {
		tmp.tv_sec--;
		tmp.tv_usec += 1000000;
	}
	diff->tv_sec = tmp.tv_sec - before->tv_sec;
	diff->tv_usec = tmp.tv_usec - before->tv_usec;
}

END_C_DECLS
#endif				/* _OSM_SA_RDMA_H_ */
