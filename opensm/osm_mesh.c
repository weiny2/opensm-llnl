/*
 * Copyright (c) 2008      System Fabric Works, Inc.
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
 *      routines to analyze certain meshes
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <opensm/osm_switch.h>
#include <opensm/osm_opensm.h>
#include <opensm/osm_log.h>
#include <opensm/osm_mesh.h>
#include <opensm/osm_ucast_lash.h>

/*
 * per fabric mesh info
 */
typedef struct _mesh {
	int num_class;			/* number of switch classes */
	int *class_type;		/* index of first switch found for each class */
	int *class_count;		/* population of each class */
	int dimension;			/* mesh dimension */
	int *size;			/* an array to hold size of mesh */
} mesh_t;

/*
 * osm_mesh_delete - free per mesh resources
 */
static void mesh_delete(mesh_t *mesh)
{
	if (mesh) {
		if (mesh->class_type)
			free(mesh->class_type);

		if (mesh->class_count)
			free(mesh->class_count);

		free(mesh);
	}
}

/*
 * osm_mesh_create - allocate per mesh resources
 */
static mesh_t *mesh_create(lash_t *p_lash)
{
	osm_log_t *p_log = &p_lash->p_osm->log;
	mesh_t *mesh;

	if(!(mesh = calloc(1, sizeof(mesh_t))))
		goto err;

	if (!(mesh->class_type = calloc(p_lash->num_switches, sizeof(int))))
		goto err;

	if (!(mesh->class_count = calloc(p_lash->num_switches, sizeof(int))))
		goto err;

	return mesh;

err:
	mesh_delete(mesh);
	OSM_LOG(p_log, OSM_LOG_ERROR, "Failed allocating mesh - out of memory\n");
	return NULL;
}

/*
 * osm_mesh_node_delete - cleanup per switch resources
 */
void osm_mesh_node_delete(lash_t *p_lash, switch_t *sw)
{
	osm_log_t *p_log = &p_lash->p_osm->log;
	int i;
	mesh_node_t *node = sw->node;
	unsigned num_ports = sw->p_sw->num_ports;

	OSM_LOG_ENTER(p_log);

	if (node) {
		if (node->links) {
			for (i = 0; i < num_ports; i++) {
				if (node->links[i]) {
					if (node->links[i]->ports)
						free(node->links[i]->ports);
					free(node->links[i]);
				}
			}
			free(node->links);
		}

		if (node->poly)
			free(node->poly);

		if (node->matrix) {
			for (i = 0; i < node->num_links; i++) {
				if (node->matrix[i])
					free(node->matrix[i]);
			}
			free(node->matrix);
		}

		if (node->axes)
			free(node->axes);

		free(node);

		sw->node = NULL;
	}

	OSM_LOG_EXIT(p_log);
}

/*
 * osm_mesh_node_create - allocate per switch resources
 */
int osm_mesh_node_create(lash_t *p_lash, switch_t *sw)
{
	osm_log_t *p_log = &p_lash->p_osm->log;
	int i;
	mesh_node_t *node;
	unsigned num_ports = sw->p_sw->num_ports;

	OSM_LOG_ENTER(p_log);

	if (!(node = sw->node = calloc(1, sizeof(mesh_node_t))))
		goto err;

	if (!(node->links = calloc(num_ports, sizeof(link_t *))))
		goto err;

	for (i = 0; i < num_ports; i++) {
		if (!(node->links[i] = calloc(1, sizeof(link_t))) ||
		    !(node->links[i]->ports = calloc(num_ports, sizeof(int))))
			goto err;
	}

	if (!(node->axes = calloc(num_ports, sizeof(int))))
		goto err;

	for (i = 0; i < num_ports; i++) {
		node->links[i]->switch_id = NONE;
	}

	OSM_LOG_EXIT(p_log);
	return 0;

err:
	osm_mesh_node_delete(p_lash, sw);
	OSM_LOG(p_log, OSM_LOG_ERROR, "Failed allocating mesh node - out of memory\n");
	OSM_LOG_EXIT(p_log);
	return -1;
}

/*
 * osm_do_mesh_analysis
 */
int osm_do_mesh_analysis(lash_t *p_lash)
{
	osm_log_t *p_log = &p_lash->p_osm->log;
	mesh_t *mesh;

	OSM_LOG_ENTER(p_log);

	mesh = mesh_create(p_lash);
	if (!mesh)
		return -1;

	mesh_delete(mesh);

	OSM_LOG_EXIT(p_log);
	return 0;
}