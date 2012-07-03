/*
 * Copyright (c) 2010 QLogic, Inc. All rights reserved.
 * Copyright (c) 2004-2009 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2005,2008 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
 * Copyright (c) 2009 HNR Consulting. All rights reserved.
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
 * This object represents the Vendor SwitchInfo Receiver object.
 * Called from process lock with in place.
 * This object is part of the opensm family of objects.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <string.h>
#include <iba/ib_types.h>
#include <complib/cl_qmap.h>
#include <complib/cl_passivelock.h>
#include <complib/cl_debug.h>
#include <opensm/osm_log.h>
#include <opensm/osm_switch.h>
#include <opensm/osm_subnet.h>
#include <opensm/osm_helper.h>
#include <opensm/osm_opensm.h>
#include <opensm/osm_qlogic_ar.h>


void qlogic_vsi_rcv_process(IN void *context, IN void *data)
{
	osm_sm_t *sm = context;
	osm_madw_t *p_madw = data;
	qlogic_vendor_swinfo_t *p_vsi;
	ib_smp_t *p_smp;
	osm_node_t *p_node;
	ib_net64_t node_guid;
	osm_si_context_t *p_context;
	qlogic_vendor_data_t *p_vendor_data = NULL;

	CL_ASSERT(sm);

	OSM_LOG_ENTER(sm->p_log);

	CL_ASSERT(p_madw);

	p_smp = osm_madw_get_smp_ptr(p_madw);
	p_vsi = ib_smp_get_payload_ptr(p_smp);
	p_context = osm_madw_get_si_context_ptr(p_madw);
	node_guid = p_context->node_guid;

	OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
		"Switch GUID 0x%016" PRIx64 ", TID 0x%" PRIx64 "\n",
		cl_ntoh64(node_guid), cl_ntoh64(p_smp->trans_id));

	p_node = osm_get_node_by_guid(sm->p_subn, node_guid);
	if (!p_node) {
		OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR 3606: "
			"SwitchInfo received for nonexistent node "
			"with GUID 0x%" PRIx64 "\n", cl_ntoh64(node_guid));
		goto Exit;
	}

	/* Process vendor switch info here */
	osm_switch_t *p_sw = p_node->sw;

	if (is_qlogic_switch(p_sw->p_node)) {
		if (qlogic_vsi_get_ar_capable(&p_vsi->vendor_info)) {
			if (qlogic_vsi_get_adaptive_routing_enable(&p_vsi->vendor_info)) {
				OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
					"Vendor Switchinfo for node 0x%" PRIx64
					" indicates Adaptive Routing support\n",
					cl_ntoh64(osm_node_get_node_guid(p_node)));
			}

			if (!p_sw->vendor_data) {
				p_sw->vendor_data = malloc(sizeof(qlogic_vendor_data_t));
				if (p_sw->vendor_data) {
					memset(p_sw->vendor_data, 0, sizeof(qlogic_vendor_data_t));
				}
			}

			if (!p_sw->vendor_data) {
				OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR AE08: "
					"No memory for node 0x%" PRIx64
					" associated with Adaptive Routing support\n",
					cl_ntoh64(osm_node_get_node_guid(p_node)));
			} else {
				p_vendor_data = (qlogic_vendor_data_t *)p_sw->vendor_data;
	
				qlogic_switch_set_vendor_info(p_node->sw, &p_vsi->vendor_info);
	
				if (qlogic_adaptive_routing_enabled(sm->p_subn)) {
					p_vendor_data->ar_support = 1;
					if (!p_vendor_data->tier0) {
						p_vendor_data->tier0 = malloc(sizeof(uint8_t)*(p_sw->num_ports));
						if (p_vendor_data->tier0) {
							memset(p_vendor_data->tier0, 0, sizeof(uint8_t)*(p_sw->num_ports));
						} else {
							OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR AE08: "
								"No memory for node 0x%" PRIx64
								" associated with Adaptive Routing support\n",
								cl_ntoh64(osm_node_get_node_guid(p_node)));
						}
					}
			
					if (qlogic_ar_tier1_fat_tree_enabled(sm->p_subn)) {
						if (!p_vendor_data->tier1) {
							p_vendor_data->tier1 = malloc(sizeof(uint8_t)*(p_sw->num_ports));
							if (p_vendor_data->tier1) 
								memset(p_vendor_data->tier1, 0, sizeof(uint8_t)*(p_sw->num_ports));
						}
						if (!p_vendor_data->new_tier1) {
							p_vendor_data->new_tier1 = malloc(sizeof(uint8_t)*(p_sw->num_ports));
							if (p_vendor_data->new_tier1)
								memset(p_vendor_data->new_tier1, 0, sizeof(uint8_t)*(p_sw->num_ports));
						}
		
						if (!p_vendor_data->tier1 || !p_vendor_data->new_tier1) {
							if (p_vendor_data->tier1) {
								free(p_vendor_data->tier1);
								p_vendor_data->tier1 = NULL;
							}
							if (p_vendor_data->new_tier1) {
								free(p_vendor_data->new_tier1);
								p_vendor_data->new_tier1 = NULL;
							}
							OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR AE08: "
								"No memory for node 0x%" PRIx64
								" associated with Adaptive Routing support\n",
								cl_ntoh64(osm_node_get_node_guid(p_node)));
						}
					}
				}
			}
	
		} else if (qlogic_adaptive_routing_enabled(sm->p_subn)) {
			OSM_LOG(sm->p_log, OSM_LOG_VERBOSE,
					"Vendor Switchinfo for node 0x%" PRIx64
					" indicates no Adaptive Routing support\n",
					cl_ntoh64(osm_node_get_node_guid(p_node)));
		}
	}

Exit:
	OSM_LOG_EXIT(sm->p_log);
}
