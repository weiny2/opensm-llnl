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
 *   QLogic Adaptive Routing config.
 */

#ifndef _OSM_QLOGIC_AR_CONFIG_H
#define _OSM_QLOGIC_AR_CONFIG_H

#ifdef __cplusplus
#  define BEGIN_C_DECLS extern "C" {
#  define END_C_DECLS   }
#else				/* !__cplusplus */
#  define BEGIN_C_DECLS
#  define END_C_DECLS
#endif				/* __cplusplus */

BEGIN_C_DECLS

/****s* QLogicAR: opt/QLOGIC_ADAPTIVE_ROUTING_NONE
* NAME
*	QLOGIC_ADAPTIVE_ROUTING_NONE
*
* DESCRIPTION
*	Indicates adaptive routing not supported.
*
* SYNOPSIS
*/
#define QLOGIC_ADAPTIVE_ROUTING_NONE 0
/********/

/****s* QLogicAR: opt/QLOGIC_ADAPTIVE_ROUTING_ENABLED
* NAME
*	QLOGIC_ADAPTIVE_ROUTING_ENABLED
*
* DESCRIPTION
*	Indicates adaptive routing enabled.
*
* SYNOPSIS
*/
#define QLOGIC_ADAPTIVE_ROUTING_ENABLED 1
/********/

/****s* QLogicAR: opt/QLOGIC_ADAPTIVE_ROUTING_LOST_ROUTES
* NAME
*	QLOGIC_ADAPTIVE_ROUTING_LOST_ROUTES
*
* DESCRIPTION
*	Indicates adaptive routing on lost routes only.
*
* SYNOPSIS
*/
#define QLOGIC_ADAPTIVE_ROUTING_LOST_ROUTES 2
/********/

/****s* QLogicAR: opt/QLOGIC_ADAPTIVE_ROUTING_TIER1_FAT_TREE
* NAME
*	QLOGIC_ADAPTIVE_ROUTING_TIER1_FAT_TREE
*
* DESCRIPTION
*	Indicates adaptive routing for tier1 fat tree configuration.
*
* SYNOPSIS
*/
#define QLOGIC_ADAPTIVE_ROUTING_TIER1_FAT_TREE 4
/********/

/****f* QLogicAR: opt/qlogic_ar_set_config_option
* NAME
*   qlogic_ar_set_config_option
*
* DESCRIPTION
*   Setup qlogic AR config option
*
* SYNOPSIS
*/
static inline void
qlogic_ar_set_config_option(IN uint8_t * p_ar_opt,
				IN char * p_option,
				IN char * p_opt_val)
{
	if (p_ar_opt && p_option && p_opt_val) {
		if (strcmp(p_opt_val, "1") == 0) {
			if (strcmp(p_option, "qlogic_ar_enable") == 0)
				*p_ar_opt |= QLOGIC_ADAPTIVE_ROUTING_ENABLED;
			else if (strcmp(p_option, "qlogic_tier1_fat_tree") == 0)
				*p_ar_opt |= QLOGIC_ADAPTIVE_ROUTING_TIER1_FAT_TREE;
			else if (strcmp(p_option, "qlogic_lost_routes_only") == 0)
				*p_ar_opt |= QLOGIC_ADAPTIVE_ROUTING_LOST_ROUTES;
		}
	}
}
/*
* PARAMETERS
*	p_ar_opt
*		[in] Pointer to the qlogic adaptive routing option field.
*	p_option
*		[in] String containing configured qlogic AR option.
*	p_opt_val
*		[in] String containing configured qlogic AR option value.
*
* RETURN VALUE
*	None.
********/

/****f* QLogicAR: opt/qlogic_adaptive_routing_enabled
* NAME
*   qlogic_adaptive_routing_enabled
*
* DESCRIPTION
*   Indicates whether subnet config supports adaptive routing.
*
* SYNOPSIS
*/
static inline boolean_t
qlogic_adaptive_routing_enabled(IN osm_subn_t * p_subn)
{
    return (p_subn->opt.qlogic_adaptive_routing & QLOGIC_ADAPTIVE_ROUTING_ENABLED);
}
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to the subnet object.
*
* RETURN VALUE
*	Config option.
********/

/****f* QLogicAR: opt/qlogic_ar_tier1_fat_tree_enabled
* NAME
*   qlogic_ar_tier1_fat_tree_enabled
*
* DESCRIPTION
*   Indicates whether adaptive routing tier1 fat tree is enabled.
*
* SYNOPSIS
*/
static inline boolean_t
qlogic_ar_tier1_fat_tree_enabled(IN osm_subn_t * p_subn)
{
    return (qlogic_adaptive_routing_enabled(p_subn) &&
			(p_subn->opt.qlogic_adaptive_routing & QLOGIC_ADAPTIVE_ROUTING_TIER1_FAT_TREE));
}
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to the subnet object.
*
* RETURN VALUE
*	Config option.
********/

/****f* QLogicAR: opt/qlogic_ar_lost_routes_only
* NAME
*   qlogic_ar_lost_routes_only
*
* DESCRIPTION
*   Indicates whether adaptive routing lost routes only is enabled.
*
* SYNOPSIS
*/
static inline boolean_t
qlogic_ar_lost_routes_only(IN osm_subn_t * p_subn)
{
    return (qlogic_adaptive_routing_enabled(p_subn) &&
    		(p_subn->opt.qlogic_adaptive_routing & QLOGIC_ADAPTIVE_ROUTING_LOST_ROUTES));
}
/*
* PARAMETERS
*	p_subn
*		[in] Pointer to the subnet object.
*
* RETURN VALUE
*	Config option.
********/

/****f* QLogicAR: opt/qlogic_ar_log_options
* NAME
*   qlogic_ar_log_options
*
* DESCRIPTION
*   Logs currently configured AR options 
*
* SYNOPSIS
*/
static inline void
qlogic_ar_log_options(IN uint8_t opt)
{
	if (!(opt & QLOGIC_ADAPTIVE_ROUTING_ENABLED)) 
		printf(" Adaptive Routing: Not Enabled\n");
	else
		printf(" Adaptive Routing: Enabled%s%s\n",
			(opt & QLOGIC_ADAPTIVE_ROUTING_LOST_ROUTES)?",LostRoutesOnly":"",
 			(opt & QLOGIC_ADAPTIVE_ROUTING_TIER1_FAT_TREE)?",PureTier1FatTree":"");
}
/*
* PARAMETERS
*	opt
*		[in] qlogic adaptive routing option field.
*
* RETURN VALUE
*	None.
********/

END_C_DECLS
#endif				/* _QLOGIC_AR_CONFIG_H */
