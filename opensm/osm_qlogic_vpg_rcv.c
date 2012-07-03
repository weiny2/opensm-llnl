/*
 * Copyright (c) 2010 QLogic, Inc. All rights reserved.
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
 * This object represents the Vendor PortGroup Receiver object.
 * This object is part of the opensm family of objects.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <iba/ib_types.h>
#include <opensm/osm_log.h>
#include <opensm/osm_opensm.h>
#include <opensm/osm_qlogic_ar.h>

void qlogic_vpg_rcv_process(IN void *context, IN void *data)
{
	osm_sm_t *sm = context;
	osm_madw_t *p_madw = data;
	ib_net64_t node_guid;
	osm_ar_context_t *p_context;
	uint32_t amod;
	osm_switch_t *p_sw;
	uint8_t *p_block;
	ib_smp_t *p_smp;

	CL_ASSERT(sm);

	OSM_LOG_ENTER(sm->p_log);

	CL_ASSERT(p_madw);

	/*
	   Acquire the switch object for this switch.
	 */
	p_smp = osm_madw_get_smp_ptr(p_madw);
	p_block = ib_smp_get_payload_ptr(p_smp);
	p_context = osm_madw_get_ar_context_ptr(p_madw);
	node_guid = p_context->node_guid;
	amod = cl_ntoh32(p_smp->attr_mod);

	OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
		"Vendor Port Group GUID 0x%016" PRIx64 ", TID 0x%" PRIx64 "\n",
		cl_ntoh64(node_guid), cl_ntoh64(p_smp->trans_id));

	CL_PLOCK_EXCL_ACQUIRE(sm->p_lock);
	p_sw = osm_get_switch_by_guid(sm->p_subn, node_guid);
	if (!p_sw) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR AE06: "
			"Vendor PortGroup received for nonexistent node "
			"0x%" PRIx64 "\n", cl_ntoh64(node_guid));
	} else {
		qlogic_switch_set_vpg(p_sw, p_block, amod>>16);
	}

	CL_PLOCK_RELEASE(sm->p_lock);

	OSM_LOG_EXIT(sm->p_log);
}
