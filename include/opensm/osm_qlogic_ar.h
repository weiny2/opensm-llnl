/*
 * Copyright (c) 2010 QLogic, Inc. All rights reserved.
 * Copyright (c) 2004-2009 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2002-2009 Mellanox Technologies LTD. All rights reserved.
 * Copyright (c) 1996-2003 Intel Corporation. All rights reserved.
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
 *   QLogic Adaptive Routing.
 */

#ifndef _OSM_QLOGIC_AR_H
#define _OSM_QLOGIC_AR_H

#include <complib/cl_packon.h>
#include <osm_qlogic_ar_config.h>

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

/****h* QLogic/Adaptive Routing
* NAME
*	Adaptive Routing Manager
*
* DESCRIPTION
*	The Adaptive Routing Manager object encapsulates the information
*	needed to control programming of adaptive routing tables on the subnet.
*
* AUTHOR
*	QLogic
*
*********/

/****s* QLogicAR: vsi/QLOGIC_VSI_CM_IS_AR_SUPPORTED
* NAME
*	QLOGIC_VSI_CM_IS_AR_SUPPORTED
*
* DESCRIPTION
*	AR vendor info component mask setting for feature enable.
*
* SYNOPSIS
*/
#define QLOGIC_VSI_CM_IS_AR_SUPPORTED 1
/********/

/****s* QLogicAR: vsi/QLOGIC_VSI_CM_IS_AR_TIER1_SUPPORTED
* NAME
*	QLOGIC_VSI_CM_IS_AR_TIER1_SUPPORTED
*
* DESCRIPTION
*	AR vendor info component mask setting for tier1 support.
*
* SYNOPSIS
*/
#define QLOGIC_VSI_CM_IS_AR_TIER1_SUPPORTED 2
/********/

/****s* QLogicAR: vsi/qlogic_vendor_info_t
* NAME
*   qlogic_vendor_info_t
*
* DESCRIPTION
*   Adaptive routing structure.  Vendor info related
*	to adaptive routing associates with a switch.
*
*   This object should be treated as opaque and should
*   be manipulated only through the provided functions.
*
*/

typedef struct qlogic_vendor_info {
	ib_net32_t		rsvd1;
	ib_net32_t		rsvd2;
	uint8_t			version;
	uint8_t			flags;	// bit 0 - lost_routes_only
							// bit 1 - adaptive_routing_pause
							// bit 2 - adaptive_routing_enable
							// bit 3 - tier1_enable
	ib_net16_t     capability_mask;
} PACK_SUFFIX qlogic_vendor_info_t; 
/********/

/****s* QLogicAR: vsi/qlogic_vendor_swinfo_t
* NAME
*   qlogic_vendor_swinfo_t
*
* DESCRIPTION
*   Adaptive routing structure.  Augments switch info
*	with vendor info.
*
*   This object should be treated as opaque and should
*   be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct qlogic_vendor_swinfo {
	ib_switch_info_t		switch_info;
	qlogic_vendor_info_t	vendor_info;
} qlogic_vendor_swinfo_t;
/********/

/****f* QLogicAR: vsi/qlogic_vsi_get_ar_capable
* NAME
*   qlogic_vsi_get_ar_capable
*
* DESCRIPTION
*   Returns adaptive routing capability from capability mask.
*
* SYNOPSIS
*/
static inline uint8_t
qlogic_vsi_get_ar_capable(IN const qlogic_vendor_info_t * p_vi)
{
    return (cl_ntoh16(p_vi->capability_mask) & QLOGIC_VSI_CM_IS_AR_SUPPORTED);
}
/********/

/****f* QLogicAR: vsi/qlogic_vsi_get_lost_routes_only
* NAME
*   qlogic_vsi_get_lost_routes_only
*
* DESCRIPTION
*   Gets the lost routes only flag from vendor switch info.
*
* SYNOPSIS
*/
static inline uint8_t
qlogic_vsi_get_lost_routes_only(IN const qlogic_vendor_info_t * p_vi)
{
    return ((uint8_t) ((p_vi->flags >> 7)));
}
/********/

/****f* QLogicAR: vsi/qlogic_vsi_set_lost_routes_only
* NAME
*   qlogic_vsi_set_lost_routes_only
*
* DESCRIPTION
*   Sets the lost routes only flag into vendor switch info.
*
* SYNOPSIS
*/
static inline void
qlogic_vsi_set_lost_routes_only(IN qlogic_vendor_info_t * p_vi, 
								IN uint8_t set)
{
    p_vi->flags = ((p_vi->flags & 0x7F) | (set << 7));
}
/********/

/****f* QLogicAR: vsi/qlogic_vsi_get_adaptive_routing_pause
* NAME
*   qlogic_vsi_get_adaptive_routing_pause
*
* DESCRIPTION
*   Gets the adaptive routing pause indicator from vendor switch info.
*
* SYNOPSIS
*/
static inline uint8_t
qlogic_vsi_get_adaptive_routing_pause(IN const qlogic_vendor_info_t * const p_vi)
{
    return ((uint8_t) ((p_vi->flags >> 6) & 1));
}
/********/

/****f* QLogicAR: vsi/qlogic_vsi_set_adaptive_routing_pause
* NAME
*   qlogic_vsi_set_adaptive_routing_pause
*
* DESCRIPTION
*   Sets the adaptive routing pause indicator into vendor switch info.
*
* SYNOPSIS
*/
static inline void
qlogic_vsi_set_adaptive_routing_pause(IN qlogic_vendor_info_t * const p_vi, 
									  IN uint8_t set)
{
    p_vi->flags = ((p_vi->flags & 0xBF) | (set << 6));
}
/********/

/****f* QLogicAR: vsi/qlogic_vsi_get_adaptive_routing_enable
* NAME
*   qlogic_vsi_get_adaptive_routing_enable
*
* DESCRIPTION
*   Gets the adaptive routing enabled indicator from vendor switch info.
*
* SYNOPSIS
*/
static inline uint8_t
qlogic_vsi_get_adaptive_routing_enable(IN const qlogic_vendor_info_t * const p_vi)
{
    return ((uint8_t) ((p_vi->flags >> 5) & 1));
}
/********/

/****f* QLogicAR: vsi/qlogic_vsi_set_adaptive_routing_enable
* NAME
*   qlogic_vsi_set_adaptive_routing_enable
*
* DESCRIPTION
*   Sets the adaptive routing enabled indicator into vendor switch info.
*
* SYNOPSIS
*/
static inline void
qlogic_vsi_set_adaptive_routing_enable(IN qlogic_vendor_info_t * const p_vi, 
									   IN uint8_t set)
{
    p_vi->flags = ((p_vi->flags & 0xDF) | (set << 5));
}
/********/

/****f* QLogicAR: vsi/qlogic_vsi_get_tier1_enable
* NAME
*   qlogic_vsi_get_tier1_enable
*
* DESCRIPTION
*   Gets the tier1 enabled flag from vendor switch info.
*
* SYNOPSIS
*/
static inline uint8_t
qlogic_vsi_get_tier1_enable(IN const qlogic_vendor_info_t * const p_vi)
{
    return ((uint8_t) ((p_vi->flags >> 4) & 1));
}
/********/

/****f* QLogicAR: vsi/qlogic_vsi_set_tier1_enable
* NAME
*   qlogic_vsi_set_tier1_enable
*
* DESCRIPTION
*   Sets the tier1 enabled flag into vendor switch info.
*
* SYNOPSIS
*/
static inline void
qlogic_vsi_set_tier1_enable(IN qlogic_vendor_info_t * const p_vi, 
							IN uint8_t set)
{
    p_vi->flags = ((p_vi->flags & 0xEF) | (set << 4));
}
/********/

/****s* QLogicAR: vpg/QLOGIC_PG_LIST_COUNT
* NAME
*   QLOGIC_PG_LIST_COUNT
*
* DESCRIPTION
*   Adaptive routing structure.  Size of port group table.
*
*   This object should be treated as opaque and should
*   be manipulated only through the provided functions.
*
* SYNOPSIS
*/
#define QLOGIC_PG_LIST_COUNT 64
/********/

/****s* QLogicAR: vpg/qlogic_port_group_block_t
* NAME
*   qlogic_port_group_block_t
*
* DESCRIPTION
*   Adaptive routing structure.  A port group block.
*
*   This object should be treated as opaque and should
*   be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct qlogic_port_group_block {
	uint8_t			port_group;       // portGroup port is a member of
} qlogic_port_group_block_t;
/********/

/****s* QLogicAR: vpg/qlogic_port_group_table_t
* NAME
*   qlogic_port_group_table_t
*
* DESCRIPTION
*   Adaptive routing structure.  The port group table.
*
*   This object should be treated as opaque and should
*   be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct qlogic_port_group_table {
	qlogic_port_group_block_t	list[QLOGIC_PG_LIST_COUNT]; // list of PG Block elements
} qlogic_port_group_table_t;
/********/

/****s* QLogicAR: vlidm/QLOGIC_LIDS_PER_BYTE
* NAME
*   QLOGIC_LIDS_PER_BYTE
*
* DESCRIPTION
*   Adaptive routing structure. The number of lids
*   per byte.
*
* SYNOPSIS
*/
#define QLOGIC_LIDS_PER_BYTE 8
/********/

/****s* QLogicAR: vlidm/QLOGIC_LIDMASK_LEN
* NAME
*   QLOGIC_LIDMASK_LEN
*
* DESCRIPTION
*   Adaptive routing structure.  Size lidmask block.
*
*   This object should be treated as opaque and should
*   be manipulated only through the provided functions.
*
* SYNOPSIS
*/
#define QLOGIC_LIDMASK_LEN 64
/********/

/****s* QLogicAR: vlidm/qlogic_lidmask_block_t
* NAME
*   qlogic_lidmask_block_t
*
* DESCRIPTION
*   Adaptive routing structure.  A lidmask block.
*
*   This object should be treated as opaque and should
*   be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct qlogic_lidmask_block {
	uint8_t			lidmask[QLOGIC_LIDMASK_LEN];	// lidmask block of 512 lids
} qlogic_lidmask_block_t;
/********/

/****s* QLogicAR: Adaptive Routing/qlogic_vendor_data_t
* NAME
*   qlogic_vendor_data_t
*
* DESCRIPTION
*   Adaptive routing structure.  Vendor data associated
*	with a switch object.
*
*   This object should be treated as opaque and should
*   be manipulated only through the provided functions.
*
* SYNOPSIS
*/
typedef struct qlogic_vendor_data_t {
	qlogic_vendor_info_t vendor_info;
	uint8_t	ar_support;
	uint8_t last_tier1_pg;
	uint8_t* tier0;
	uint8_t* tier1;
	uint8_t* new_tier1;
	uint8_t* lidmask;
	uint8_t* new_lidmask;
	uint16_t lidmask_size;
} qlogic_vendor_data_t;

/*
* FIELDS
*	vendor_info
*		Additional switch info associated with adaptive routing.
*
*	ar_support
*		Adaptive routing feature is enabled on this switch.
*
*	last_tier1_pg
*		Port group number assigned to last tier1 group
*
*	tier0
*		Tier0 port group (adjacent links)
*
*	tier1
*		Tier1 port group (all possible routes)
*
*	new_tier1
*		Switches tier1 calculated on last routing engine
*		execution.
*
*	lidmask
*		Switches adaptive routing lidmask data
*
*	new_lidmask
*		Switches adaptive routing lidmask data calculated
*		during routing/ar engine execution.
*
*	lidmask_size
*		Size of lidmask
*
*********/

/****f* QLogicAR: vsi/qlogic_switch_set_vendor_info
* NAME
*	qlogic_switch_set_vendor_info
*
* DESCRIPTION
*	Updates the vendor info associated with this switch.
*
* SYNOPSIS
*/
static inline void qlogic_switch_set_vendor_info(IN osm_switch_t * p_sw,
												 IN const qlogic_vendor_info_t * p_vi)
{
	qlogic_vendor_data_t *vendor_data = NULL;

	CL_ASSERT(p_sw);
	CL_ASSERT(p_sw->vendor_data);
	CL_ASSERT(p_vi);

	vendor_data = (qlogic_vendor_data_t *)p_sw->vendor_data;
	vendor_data->vendor_info = *p_vi;
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch object.
*	p_vi
*		[in] Pointer to the switch vendor info.
*
* RETURN VALUE
*	None.
*
*********/

/****f* QLogicAR: vpg/osm_switch_set_vpg
* NAME
*	osm_switch_set_vpg
*
* DESCRIPTION
*	Updates the portgroup associated with this switch.
*
* SYNOPSIS
*/
static inline void qlogic_switch_set_vpg(IN osm_switch_t * p_sw,
					      			     IN const uint8_t * p_vpg,
					      			     IN const uint32_t  tier)
{
	qlogic_vendor_data_t *vendor_data = NULL;

	CL_ASSERT(p_sw);
	CL_ASSERT(p_sw->vendor_data);
	CL_ASSERT(p_vpg);

	vendor_data = (qlogic_vendor_data_t *)p_sw->vendor_data;

	if ((tier == 0) && vendor_data->tier0) {
		memcpy(vendor_data->tier0, p_vpg, sizeof(uint8_t)*p_sw->num_ports);
	} else if ((tier == 1) && vendor_data->tier1) {
		memcpy(vendor_data->tier1, p_vpg, sizeof(uint8_t)*p_sw->num_ports);
	}
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch object.
*	p_vpg
*		[in] Pointer to the vendor portGroup info.
*	tier
*		[in] Tier associated with this portGroup.
*
* RETURN VALUE
*	None.
*
*********/

/****f* QLogicAR: vlidm/osm_switch_set_vlidm
* NAME
*	osm_switch_set_vlidm
*
* DESCRIPTION
*	Updates the lidmask associated with this switch.
*
* SYNOPSIS
*/
static inline ib_api_status_t
qlogic_switch_set_vlidm(IN osm_switch_t * p_sw,
						IN const uint8_t * p_block,
						IN const uint32_t  block_num)
{
	qlogic_vendor_data_t *vendor_data = NULL;
	uint16_t lidmask_start = 
		(uint16_t) (block_num * IB_SMP_DATA_SIZE);

	CL_ASSERT(p_sw);
	CL_ASSERT(p_sw->vendor_data);
	CL_ASSERT(p_block);
	
	vendor_data = (qlogic_vendor_data_t *)p_sw->vendor_data;

	if (lidmask_start + IB_SMP_DATA_SIZE > vendor_data->lidmask_size)
		return IB_INVALID_PARAMETER;

	if (vendor_data->lidmask)
		memcpy(&vendor_data->lidmask[lidmask_start], p_block, IB_SMP_DATA_SIZE);

    return IB_SUCCESS;

}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch object.
*	p_block
*		[in] Pointer to the lidmask block to be copied.
*	block_num
*		[in] Block number for this block to be copied.
*
* RETURN VALUE
*	status of set
*
*********/

/****f* QLogicAR: vlidm/osm_switch_purge_vlidm
* NAME
*	osm_switch_purge_vlidm
*
* DESCRIPTION
*	Purges the lidmask data associated with this switch.
*
* SYNOPSIS
*/
static inline void qlogic_switch_purge_vlidm(IN osm_switch_t * p_sw)
{

	qlogic_vendor_data_t *vendor_data = NULL;

	CL_ASSERT(p_sw);
	CL_ASSERT(p_sw->vendor_data);

	vendor_data = (qlogic_vendor_data_t *)p_sw->vendor_data;
	
	if (vendor_data->lidmask)
		memset(vendor_data->lidmask, 0, vendor_data->lidmask_size);
}
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch object.
*
* RETURN VALUE
*	None.
*
*********/

/****f* QLogicAR: switch/is_qlogic_switch
* NAME
*   is_qlogic_switch
*
* DESCRIPTION
*   Identifies qlogic switch which is capable of adaptive routing.
*
* SYNOPSIS
*/
static inline int is_qlogic_switch(IN const osm_node_t *p_node)
{
    return ((osm_node_get_type(p_node) == IB_NODE_TYPE_SWITCH) &&
			(osm_node_get_num_physp(p_node) >= 36) &&
            (cl_ntoh32(ib_node_info_get_vendor_id(&p_node->node_info)) == OSM_VENDOR_ID_SILVERSTORM) &&
			(cl_ntoh32(p_node->node_info.revision) >= 2));
}
/*
* PARAMETERS
*	p_node
*		[in] Pointer to the node object.
*
* RETURN VALUE
*	Whether it is a QLogic switch capable of AR.
********/

/****f* QLogicAR: switch/qlogic_switch_ar_support
* NAME
*   qlogic_switch_ar_support
*
* DESCRIPTION
*   Indicates whether switch supports adaptive routing.
*
* SYNOPSIS
*/
static inline boolean_t
qlogic_switch_ar_support(IN osm_subn_t * p_subn, IN osm_switch_t * p_sw)
{
	qlogic_vendor_data_t *vendor_data = NULL;

	if (!p_sw->vendor_data) FALSE;

	vendor_data = (qlogic_vendor_data_t *)p_sw->vendor_data;

    return (qlogic_adaptive_routing_enabled(p_subn) &&
			vendor_data && vendor_data->ar_support);
}
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to the subnet object.
*	p_sw
*		[in] Pointer to the switch object.
*
* RETURN VALUE
*	Whether it is a QLogic switch using AR.
********/

/****f* QLogicAR: switch/qlogic_switch_path_prepare
* NAME
*	qlogic_switch_path_prepare
*
* DESCRIPTION
*	Prepares the vendor data associated with this switch
*	during path rebuild
*
* SYNOPSIS
*/
int
qlogic_switch_path_prepare(IN osm_switch_t * p_sw,
						   IN uint16_t lids);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch object.
*	lids
*		[in] Number of fabric lids
*
* RETURN VALUE
*	Whether prepare was successful.
*
*********/

/****f* QLogicAR: switch/qlogic_path_setup
* NAME
*   qlogic_path_setup
*
* DESCRIPTION
*   Sets up port groups based on ucast routing data.
*
* SYNOPSIS
*/
void
qlogic_path_setup(IN const osm_switch_t * p_sw,
				  IN const osm_port_t * p_port,
				  IN uint16_t base_lid,
				  IN uint8_t least_hops);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch object.
*	p_node
*		[in] Pointer to the node object.
*	base_lid
*		[in] Base lid of the port
*	least_hops
*		[in] Hops to port
*
* RETURN VALUE
*	None.
*
* SEE ALSO
*	ucast mgr
*
*********/

/****f* QLogicAR: switch/qlogic_switch_vendor_delete
* NAME
*	qlogic_switch_vendor_delete
*
* DESCRIPTION
*	Deletes the vendor data associated with this switch.
*
* SYNOPSIS
*/
void
qlogic_switch_vendor_delete(IN osm_switch_t * p_sw);
/*
* PARAMETERS
*	p_sw
*		[in] Pointer to the switch object.
*
* RETURN VALUE
*	None.
*
*********/

/****f* QLogicAR: switch/qlogic_vswinfo_update_required
* NAME
*	qlogic_vswinfo_update_required
*
* DESCRIPTION
*	Returns true if vendor switch info needs updating.
*	Used when AR is indicated on switch but not configured.
*
* SYNOPSIS
*/
uint8_t
qlogic_vswinfo_update_required(IN osm_subn_t *p_subn, IN osm_switch_t *p_sw);
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to the subnet object.
*	p_sw
*		[in] Pointer to the switch object.
*
* RETURN VALUE
*	Update required.
*
*********/

END_C_DECLS
#endif				/* _QLOGIC_AR_H */
