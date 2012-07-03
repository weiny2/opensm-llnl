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
 *   This file implements the QLogic Adaptive Routing functions.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iba/ib_types.h>
#include <complib/cl_qmap.h>
#include <complib/cl_debug.h>
#include <complib/cl_qlist.h>
#include <opensm/osm_ucast_mgr.h>
#include <opensm/osm_log.h>
#include <opensm/osm_node.h>
#include <opensm/osm_switch.h>
#include <opensm/osm_helper.h>
#include <opensm/osm_msgdef.h>
#include <opensm/osm_opensm.h>
#include <opensm/osm_qlogic_ar.h>
#include <opensm/osm_qlogic_ar_config.h>
#include <opensm/osm_qlogic_vendor_attr.h>


static uint8_t
qlogic_verify_adaptive_routing_config(IN osm_subn_t *p_subn,
									  IN osm_switch_t *p_sw)
{
	uint8_t	update_needed = 0;
	qlogic_vendor_data_t *p_vendor_data = NULL;

	if (!p_sw->vendor_data) return 0;

	p_vendor_data = (qlogic_vendor_data_t *)p_sw->vendor_data;

	if (qlogic_vsi_get_ar_capable(&p_vendor_data->vendor_info)) {
		if (!qlogic_adaptive_routing_enabled(p_subn)) {
			if (qlogic_vsi_get_adaptive_routing_enable(&p_vendor_data->vendor_info)) {
				update_needed = 1;
			}
		} else {
			if (!qlogic_vsi_get_adaptive_routing_enable(&p_vendor_data->vendor_info)) {
				update_needed = 1;
			}
			if (!qlogic_ar_lost_routes_only(p_subn) &&
				qlogic_vsi_get_lost_routes_only(&p_vendor_data->vendor_info)) {
				update_needed = 1;
			}
			if (qlogic_ar_lost_routes_only(p_subn) &&
				!qlogic_vsi_get_lost_routes_only(&p_vendor_data->vendor_info)) {
				update_needed = 1;
			}
			if (!qlogic_ar_tier1_fat_tree_enabled(p_subn) &&
				qlogic_vsi_get_tier1_enable(&p_vendor_data->vendor_info)) {
				update_needed = 1;
			}
			if (qlogic_ar_tier1_fat_tree_enabled(p_subn) &&
				!qlogic_vsi_get_tier1_enable(&p_vendor_data->vendor_info)) {
				update_needed = 1;
			}
		}
	}
	return update_needed;
}

uint8_t 
qlogic_vswinfo_update_required(IN osm_subn_t *p_subn,
						 	   IN osm_switch_t *p_sw)
{
	if (!p_sw->vendor_data) return 0;
	if (qlogic_adaptive_routing_enabled(p_subn)) return 0;
	return (qlogic_verify_adaptive_routing_config(p_subn, p_sw));
}

void
qlogic_set_vswitch_info(IN osm_sm_t * sm,
						IN osm_switch_t * p_sw,
						IN uint8_t set_pause) {

	ib_api_status_t status;
	osm_dr_path_t *p_path;
	osm_madw_context_t context;
	qlogic_vendor_swinfo_t vsi;
	osm_node_t *p_node = p_sw->p_node;
    uint16_t lin_top;
    uint8_t life_state;
	qlogic_vendor_data_t *p_vendor_data = NULL;

	if (!p_sw->vendor_data) return;

	p_vendor_data = (qlogic_vendor_data_t *)p_sw->vendor_data;

	vsi.switch_info = p_sw->switch_info;
	vsi.vendor_info = p_vendor_data->vendor_info;
    /*
       Set the top of the unicast forwarding table.
     */
    lin_top = cl_hton16(p_sw->max_lid_ho);
    if (lin_top != vsi.switch_info.lin_top) {
        vsi.switch_info.lin_top = lin_top;
    }

    /* check to see if the change state bit is on. If it is - then we
       need to clear it unless we are clearing the pause setting. */
    if ((!p_vendor_data->ar_support || set_pause) && ib_switch_info_get_state_change(&vsi.switch_info))
        life_state = ((sm->p_subn->opt.packet_life_time << 3)
                  | (vsi.switch_info.life_state & IB_SWITCH_PSC)) & 0xfc;
    else
        life_state = (sm->p_subn->opt.packet_life_time << 3) & 0xf8;

    if (life_state != vsi.switch_info.life_state || ib_switch_info_get_state_change(&vsi.switch_info)) {
        vsi.switch_info.life_state = life_state;
    }

	if (qlogic_vsi_get_ar_capable(&p_vendor_data->vendor_info)) {
		if (!qlogic_adaptive_routing_enabled(sm->p_subn)) {
			if (qlogic_vsi_get_adaptive_routing_enable(&p_vendor_data->vendor_info)) {
				// turn it off on switch.
				qlogic_vsi_set_adaptive_routing_enable(&vsi.vendor_info, 0);
			}
		} else {
			if (set_pause) {
				qlogic_vsi_set_adaptive_routing_pause(&vsi.vendor_info, 1);
			} else {
				qlogic_vsi_set_adaptive_routing_pause(&vsi.vendor_info, 0);
			}

			OSM_LOG(sm->p_log, OSM_LOG_DEBUG,
					"Setting adaptive routing pause (%d) for switch 0x%" PRIx64 "\n",
					qlogic_vsi_get_adaptive_routing_pause(&vsi.vendor_info), 
					cl_ntoh64(osm_node_get_node_guid(p_node)));

			if (!qlogic_vsi_get_adaptive_routing_enable(&p_vendor_data->vendor_info)) {
				qlogic_vsi_set_adaptive_routing_enable(&vsi.vendor_info, 1);
			}

			if (!qlogic_ar_lost_routes_only(sm->p_subn) &&
				qlogic_vsi_get_lost_routes_only(&p_vendor_data->vendor_info)) {
				qlogic_vsi_set_lost_routes_only(&vsi.vendor_info, 0);
			}

			if (qlogic_ar_lost_routes_only(sm->p_subn) &&
				!qlogic_vsi_get_lost_routes_only(&p_vendor_data->vendor_info)) {
				qlogic_vsi_set_lost_routes_only(&vsi.vendor_info, 1);
			}

			if (!qlogic_ar_tier1_fat_tree_enabled(sm->p_subn) &&
				qlogic_vsi_get_tier1_enable(&p_vendor_data->vendor_info)) {
				qlogic_vsi_set_tier1_enable(&vsi.vendor_info, 0);
			}

			if (qlogic_ar_tier1_fat_tree_enabled(sm->p_subn) &&
				!qlogic_vsi_get_tier1_enable(&p_vendor_data->vendor_info)) {
				qlogic_vsi_set_tier1_enable(&vsi.vendor_info, 1);
			}
		}
	}

	p_path = osm_physp_get_dr_path_ptr(osm_node_get_physp_ptr(p_node, 0));
    context.si_context.light_sweep = FALSE;
    context.si_context.node_guid = osm_node_get_node_guid(p_node);
    context.si_context.set_method = TRUE;

   	status = osm_req_set(sm, p_path, (uint8_t *) &vsi,
                    sizeof(qlogic_vendor_swinfo_t), IB_MAD_ATTR_VENDOR_QLOGIC_SWITCH_INFO,
                    0, CL_DISP_MSGID_NONE, &context);

   	if (status != IB_SUCCESS) {
       	OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR AE01: "
            	"Setting adaptive routing pause (%s) failed for switch 0x%" PRIx64 "\n",
               	ib_get_err_str(status), cl_ntoh64(osm_node_get_node_guid(p_node)));
	}
}

static ib_api_status_t
qlogic_ar_switch_update(IN osm_sm_t * sm,
						IN osm_switch_t * p_sw) {

	ib_api_status_t		status;
	osm_dr_path_t		*p_path;
	osm_madw_context_t	context;
	osm_node_t 			*p_neighbor_node;
    osm_physp_t			*p_physp, *p_next_physp;
	uint8_t				tier0[256];
	uint8_t				tier1[256];
	uint8_t				next_tier0=1;
	int					index, nextindex;
	int					low_lid=0;
	int					high_lid=p_sw->max_lid_ho;
	int					blk;
	uint32_t			amod=0;
	int					lid;
	int					do_tier0=0;
	int					do_tier1=0;
	int					tier1_enabled=0;
	osm_port_t			*p_port=NULL;
	qlogic_port_group_table_t	port_group;
	qlogic_lidmask_block_t		lidmask_blk;
	qlogic_vendor_data_t *p_vendor_data = NULL;

	if (!p_sw->vendor_data) return IB_SUCCESS;

	p_vendor_data = (qlogic_vendor_data_t *)p_sw->vendor_data;

	if (!qlogic_switch_ar_support(sm->p_subn, p_sw)) {
		return IB_SUCCESS;
	}

	if (!qlogic_vsi_get_adaptive_routing_pause(&p_vendor_data->vendor_info)) {
       	OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR AE0B: "
			"Failed to Set pause for node 0x%" PRIx64 "\n",
			cl_ntoh64(osm_node_get_node_guid(p_sw->p_node)));
	}

	memset((void*)&tier0, 0, sizeof(uint8_t)*256);
	memset((void*)&tier1, 0, sizeof(uint8_t)*256);

    for (index = 1; index < p_sw->num_ports; index++) {
		if (tier0[index] != 0) continue;
        p_physp = osm_node_get_physp_ptr(p_sw->p_node, (uint8_t)index);
		if (!p_physp || !p_physp->p_remote_physp || (osm_physp_get_port_state(p_physp) <= IB_LINK_DOWN))
			continue;

		p_neighbor_node = p_physp->p_remote_physp->p_node;

		if (!p_neighbor_node->sw ||
			p_sw == p_neighbor_node->sw)  {
			continue;
		}

		for (nextindex = index+1; nextindex< p_sw->num_ports; nextindex++) {
			if (tier0[nextindex] != 0) continue;

       		p_next_physp = osm_node_get_physp_ptr(p_sw->p_node, (uint8_t) nextindex);
			if (!p_next_physp || (osm_physp_get_port_state(p_next_physp) <= IB_LINK_DOWN))
				continue;

			if (p_physp->hop_wf != p_next_physp->hop_wf) continue;

			if (!p_next_physp->p_remote_physp ||
				!p_next_physp->p_remote_physp->p_node) {
				continue;
			}

			if (p_neighbor_node == p_next_physp->p_remote_physp->p_node) {
				// link to same switch
				if (!tier0[index]) {
					tier0[index] = next_tier0++;
				}
				tier0[nextindex] = tier0[index];
			}
		}
	}

	if (p_sw->need_update || sm->p_subn->need_update) {
		do_tier0 = 1;
		do_tier1 = 1;
	}

	if (p_vendor_data->tier0 && 
		memcmp(p_vendor_data->tier0, tier0, sizeof(uint8_t)*p_sw->num_ports)) {
		do_tier0 = 1;
		p_sw->need_update = 1;
	}

	if (p_vendor_data->new_tier1) {
		for (index=1; index < p_sw->num_ports; index++) {
			if (p_vendor_data->new_tier1[index] == 0xff) {
				p_vendor_data->new_tier1[index] = 0;
			}
			if (p_vendor_data->new_tier1[index]) {
				tier1_enabled = 1;
				tier1[index] = p_vendor_data->new_tier1[index];
			}
		}

		if (!p_vendor_data->tier1 ||
			memcmp(p_vendor_data->tier1, tier1, sizeof(uint8_t)*p_sw->num_ports)) {
			do_tier1 = 1;
			p_sw->need_update = 1;
		}
	}

	p_path = osm_physp_get_dr_path_ptr(osm_node_get_physp_ptr(p_sw->p_node, 0));
    context.ar_context.node_guid = osm_node_get_node_guid(p_sw->p_node);
    context.ar_context.set_method = TRUE;

	if (do_tier0 || do_tier1) {
		for (blk=0; blk<=(p_sw->num_ports/QLOGIC_PG_LIST_COUNT); blk++) {
			amod = blk;

			if (do_tier0) {
				memcpy(port_group.list, &tier0[blk*QLOGIC_PG_LIST_COUNT], QLOGIC_PG_LIST_COUNT);

   				status = osm_req_set(sm, p_path, (uint8_t *) &port_group,
					sizeof(qlogic_port_group_table_t), IB_MAD_ATTR_VENDOR_QLOGIC_PORT_GROUP,
					cl_hton32(amod), CL_DISP_MSGID_NONE, &context);

   				if (status != IB_SUCCESS) {
       				OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR AE02: "
	   					"Failed to Set PortGroup for node 0x%" PRIx64 ": status = %s\n",
               			cl_ntoh64(osm_node_get_node_guid(p_sw->p_node)), ib_get_err_str(status));
					return status;
				}
			}

			if (do_tier1) {
				amod |= (1<<16);
				memcpy(port_group.list, &tier1[blk*QLOGIC_PG_LIST_COUNT], QLOGIC_PG_LIST_COUNT);
	
   				status = osm_req_set(sm, p_path, (uint8_t *) &port_group,
					sizeof(qlogic_port_group_table_t), IB_MAD_ATTR_VENDOR_QLOGIC_PORT_GROUP,
					cl_hton32(amod), CL_DISP_MSGID_NONE, &context);

   				if (status != IB_SUCCESS) {
       				OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR AE03: "
	   					"Failed to Set Tier1 PortGroup for node 0x%" PRIx64": status = %s\n",
               			cl_ntoh64(osm_node_get_node_guid(p_sw->p_node)), ib_get_err_str(status));
					return status;
				}
			}
		}
	}

	if (p_vendor_data->tier1 || p_sw->need_update || sm->p_subn->need_update) {
		if (!tier1_enabled) high_lid = 0;

		for (blk=low_lid/512; blk<=(high_lid/512); blk++) {
			amod = (1<<17) | blk;	// tier 1 only so far
			memset((void*)&lidmask_blk, 0, sizeof(lidmask_blk));

			if (tier1_enabled) {
				// Add all switch lids that are in this block
				for (lid=blk*512; lid<blk*512+512; lid++) {
					if (lid > high_lid) break;
	
           			if (p_sw->new_lft[lid] == OSM_NO_PATH)
						continue;
		
           			p_port = cl_ptr_vector_get(&sm->p_subn->port_lid_tbl, lid);
	
           			/* we're interested only in switches */
           			if (!p_port || !p_port->p_node->sw)
               			continue;
					
					lidmask_blk.lidmask[(QLOGIC_LIDMASK_LEN-1) - (lid%512)/8] |= (1 << ((lid%512)%8));
					if (p_vendor_data->new_lidmask) {
						memcpy(&p_vendor_data->new_lidmask[blk*IB_SMP_DATA_SIZE], &lidmask_blk, IB_SMP_DATA_SIZE);
					}
				}
				if (!p_sw->need_update && !sm->p_subn->need_update && p_vendor_data->lidmask &&
					!memcmp((uint8_t*)&lidmask_blk, p_vendor_data->lidmask + blk*IB_SMP_DATA_SIZE, IB_SMP_DATA_SIZE)) {
					continue;
				}
			} else {
				// set high order bit to purge existing lidmask
				amod |= (1<<31);
			}

   			status = osm_req_set(sm, p_path, (uint8_t *) &lidmask_blk,
				sizeof(qlogic_lidmask_block_t), IB_MAD_ATTR_VENDOR_QLOGIC_AR_LIDMASK,
				cl_hton32(amod), CL_DISP_MSGID_NONE, &context);

   			if (status != IB_SUCCESS) {
       			OSM_LOG(sm->p_log, OSM_LOG_ERROR, "ERR AE04: "
	   				"Failed to Set Adaptive Routing LidMask for node 0x%" PRIx64": status = %s\n",
               		cl_ntoh64(osm_node_get_node_guid(p_sw->p_node)), ib_get_err_str(status));
				return status;
			}
		}
	}
	return IB_SUCCESS;
}

void
qlogic_ar_setup_all_switches(IN osm_sm_t * sm)
{
	osm_switch_t *p_sw;

	if (!qlogic_adaptive_routing_enabled(sm->p_subn)) {
		return;
	}

	for (p_sw = (osm_switch_t *) cl_qmap_head(&sm->p_subn->sw_guid_tbl);
	     p_sw != (osm_switch_t *) cl_qmap_end(&sm->p_subn->sw_guid_tbl);
	     p_sw = (osm_switch_t *) cl_qmap_next(&p_sw->map_item)) {
		if (qlogic_switch_ar_support(sm->p_subn, p_sw)) {
			qlogic_ar_switch_update(sm, p_sw);
		}
	}
}

static void
qlogic_tier1_ports(IN const osm_switch_t * p_sw, 
				   IN uint8_t *port_group,
				   IN int num_ports_in_group)
{
	int p, current_port;
	int current_group=0;
	int pgInUse=0;
	uint8_t tier1_bitmap[256];
	int bits = 0;
	
	if (num_ports_in_group <= 0) return;

	qlogic_vendor_data_t *p_vendor_data = NULL;

	if (!p_sw->vendor_data) return;

	p_vendor_data = (qlogic_vendor_data_t *)p_sw->vendor_data;

	if (!p_vendor_data->new_tier1) return;

	if (num_ports_in_group == 1) {
		p_vendor_data->new_tier1[port_group[0]] = 0xff;
		return;
	}

	memset((void *)tier1_bitmap, 0, sizeof(uint8_t)*(p_sw->num_ports));

	for (p=0; p<num_ports_in_group; p++) {
		if (!p_vendor_data->new_tier1[port_group[p]]) {
			if (!current_group) {
				current_group = ++p_vendor_data->last_tier1_pg;
			}
			p_vendor_data->new_tier1[port_group[p]] = current_group;
		} else if (p_vendor_data->new_tier1[port_group[p]] != 0xff) {
			tier1_bitmap[port_group[p]] = 1;
			bits++;
		} 
	}

	if (!bits) {
		// done
		return;
	} else if (bits == 1) {
		for (current_port=0; current_port < p_sw->num_ports; current_port++) {
			if (!tier1_bitmap[current_port]) continue;
			p_vendor_data->new_tier1[current_port] = 0xff;
			break;
		}
		return;
	}

	for (current_port=0; current_port < p_sw->num_ports; current_port++) {
		if (!tier1_bitmap[current_port]) continue;
		pgInUse = p_vendor_data->new_tier1[current_port];
		current_group = 0;

		for (p=1; p<p_sw->num_ports; p++) {
			if (p_vendor_data->new_tier1[p] == pgInUse) {
				if (!tier1_bitmap[p]) {
					if (!current_group) {
						current_group = ++p_vendor_data->last_tier1_pg;
					}
					p_vendor_data->new_tier1[p] = current_group;
				}
				tier1_bitmap[p] = 0;
			}
		}
	}
}

void
qlogic_path_setup(IN const osm_switch_t * p_sw,
				  IN const osm_port_t * p_port,
				  IN uint16_t base_lid,
				  IN uint8_t least_hops) 
{
	uint8_t port_num;
    uint8_t port_group[256];
    unsigned port_group_num = 0;
	osm_physp_t *p_physp;
	qlogic_vendor_data_t *p_vendor_data = NULL;
	
	if (!(p_port->p_node->sw &&
		  p_port->p_node->sw->endport_links)) 
		// not path to leaf
		return;

	if (!p_sw->vendor_data) return;

	p_vendor_data = (qlogic_vendor_data_t *)p_sw->vendor_data;

	if (!p_vendor_data->tier1) return;

	for (port_num = 1; port_num < p_sw->num_ports; port_num++) {
		if (osm_switch_get_hop_count(p_sw, base_lid, port_num) != least_hops) continue;

		/* let us make sure it is not down or unhealthy */
		p_physp = osm_node_get_physp_ptr(p_sw->p_node, port_num);
		if (!p_physp || !osm_physp_is_healthy(p_physp) ||
	   	/*
	   	we require all - non sma ports to be linked
	   	to be routed through
	   	*/
	   	!osm_physp_get_remote(p_physp))
		continue;
				
		/*
	   	We located a least-hop port, possibly one of many.
		*/
		port_group[port_group_num++] = port_num;
	}
	qlogic_tier1_ports(p_sw, port_group, port_group_num);
}

void
qlogic_set_ar_switches_pause(IN const osm_ucast_mgr_t * p_mgr,
							 IN uint8_t set)
{
	osm_opensm_t *p_osm;
	cl_qmap_t *p_sw_guid_tbl;
	osm_switch_t *p_sw;
	int setError = 0;
	qlogic_vendor_data_t *p_vendor_data = NULL;

	OSM_LOG_ENTER(p_mgr->p_log);

	p_sw_guid_tbl = &p_mgr->p_subn->sw_guid_tbl;
	p_osm = p_mgr->p_subn->p_osm;

	CL_PLOCK_EXCL_ACQUIRE(p_mgr->p_lock);

	/*
	   If there are no switches in the subnet, we are done.
	 */
	if (cl_qmap_count(p_sw_guid_tbl) == 0)
		goto Exit;

	for (p_sw = (osm_switch_t *) cl_qmap_head(&p_mgr->sm->p_subn->sw_guid_tbl);
		 p_sw != (osm_switch_t *) cl_qmap_end(&p_mgr->sm->p_subn->sw_guid_tbl);
		 p_sw = (osm_switch_t *) cl_qmap_next(&p_sw->map_item)) {

		if (!p_sw->vendor_data) continue;
		if (!qlogic_switch_ar_support(p_mgr->p_subn, p_sw)) continue;

		p_vendor_data = (qlogic_vendor_data_t *)p_sw->vendor_data;

		if (!set) {
			setError = 0;
			// Don't clear pause if error occurred on set of AR data
			if (p_vendor_data->tier1 && p_vendor_data->new_tier1 &&
				memcmp(p_vendor_data->tier1, p_vendor_data->new_tier1, sizeof(uint8_t)*p_sw->num_ports)) {
				setError = 1;
			}

			if (p_vendor_data->lidmask && p_vendor_data->new_lidmask) {
				if (memcmp(p_vendor_data->lidmask, p_vendor_data->new_lidmask, p_vendor_data->lidmask_size)) {
					setError = 1;
				}
				free(p_vendor_data->new_lidmask);
				p_vendor_data->new_lidmask = NULL;
			}
			if (setError) {
        		osm_log(p_mgr->p_log, OSM_LOG_ERROR, "ERR AE09: "
					"Adaptive routing info of switch 0x%016" PRIx64 " (%s) is not up to date\n",
					cl_ntoh64(p_sw->p_node->node_info.node_guid),
					p_sw->p_node->print_desc);
				continue;
			}
		} 

		qlogic_set_vswitch_info(p_mgr->sm, p_sw, set);
	}

Exit:
	CL_PLOCK_RELEASE(p_mgr->p_lock);
	OSM_LOG_EXIT(p_mgr->p_log);
}

void qlogic_switch_vendor_delete(IN osm_switch_t * p_sw)
{
	qlogic_vendor_data_t *p_vendor_data = NULL;

	if (!p_sw) return;
	if (!p_sw->vendor_data) return;

	p_vendor_data = (qlogic_vendor_data_t *)p_sw->vendor_data;

    if (p_vendor_data->tier0)
        free(p_vendor_data->tier0);
    if (p_vendor_data->tier1)
        free(p_vendor_data->tier1);
    if (p_vendor_data->new_tier1)
        free(p_vendor_data->new_tier1);
    if (p_vendor_data->lidmask)
        free(p_vendor_data->lidmask);
    if (p_vendor_data->new_lidmask)
        free(p_vendor_data->new_lidmask);

	free(p_sw->vendor_data);
	p_sw->vendor_data = NULL;
}

int qlogic_switch_path_prepare(IN osm_switch_t * p_sw,
					 	       IN uint16_t lids)
{
	uint16_t lidmask_size;
	qlogic_vendor_data_t *p_vendor_data = NULL;

	if (!p_sw) return 0;
	if (!p_sw->vendor_data) return 0;

	p_vendor_data = (qlogic_vendor_data_t *)p_sw->vendor_data;

	if (p_vendor_data->tier1) {
		lidmask_size = ((lids/QLOGIC_LIDS_PER_BYTE/IB_SMP_DATA_SIZE) + 1) * IB_SMP_DATA_SIZE;
		if (lidmask_size > p_vendor_data->lidmask_size) {
			uint8_t *new_lidmask = realloc(p_vendor_data->lidmask, lidmask_size);
			if (!new_lidmask)
				return -1;
			memset(new_lidmask + p_vendor_data->lidmask_size, 0,
				lidmask_size - p_vendor_data->lidmask_size);
			p_vendor_data->lidmask = new_lidmask;
			p_vendor_data->lidmask_size = lidmask_size;
		}

		if (!(p_vendor_data->new_lidmask = realloc(p_vendor_data->new_lidmask, p_vendor_data->lidmask_size))) {
			return -1; 
		}
		memset(p_vendor_data->new_lidmask, 0, p_vendor_data->lidmask_size);
		if (p_vendor_data->new_tier1)
			memset(p_vendor_data->new_tier1, 0, sizeof(uint8_t)*p_sw->num_ports);
		p_vendor_data->last_tier1_pg = 0;
	}

    return 0;
}
