/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "mesh.h"
#include "server.h"
#include "shader.h"
#include "light.h"
#include "object.h"
#include "scene.h"

#include "util_progress.h"
#include "util_lists.h"

OCT_NAMESPACE_BEGIN

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CONSTRUCTOR
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Mesh::Mesh() {
    empty           = false;
	mesh_type       = GLOBAL;

	need_update     = true;
    use_subdivision = false;
    subdiv_divider  = 1.0f;
} //Mesh()

Mesh::~Mesh() {
} //~Mesh()

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Clear all mesh data
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Mesh::clear() {
	// Clear all verts and triangles
	points.clear();
	normals.clear();
	uvs.clear();
	vert_per_poly.clear();
	points_indices.clear();
	uv_indices.clear();
	poly_mat_index.clear();

	used_shaders.clear();
} //clear()

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Tag the mesh as "need update" (and "rebuild" if needed).
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Mesh::tag_update(Scene *scene) {
    if(empty) {
        if(need_update) need_update = false;
        return;
    }

	need_update = true;

	scene->mesh_manager->need_update    = true;
	scene->object_manager->need_update  = true;
    scene->light_manager->need_update   = true;
} //tag_update()



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MeshManager CONSTRUCTOR
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
MeshManager::MeshManager() {
	need_update         = true;
    need_global_update  = false;
} //MeshManager()

MeshManager::~MeshManager() {
} //~MeshManager()

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Update all (already compiled) scene meshes on render-server (finally sends one LOAD_MESH packet for global mesh and one for each "Movable proxy" mesh)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MeshManager::server_update_mesh(RenderServer *server, Scene *scene, Progress& progress) {
	if(!need_update) return;
    need_update = false;
	progress.set_status("Loading Meshes to render-server", "");

    uint64_t ulGlobalCnt    = 0;
    uint64_t ulLocalCnt     = 0;
    bool global_update      = false;

    if(need_global_update) {
        global_update       = need_global_update;
        need_global_update  = false;
    }
    vector<Mesh*>::iterator it;
    for(it = scene->meshes.begin(); it != scene->meshes.end(); ++it) {
        Mesh *mesh = *it;
        if(mesh->empty) {
        	if(mesh->need_update) mesh->need_update = false;
            continue;
        }
        else if(scene->meshes_type == Mesh::GLOBAL || (scene->meshes_type == Mesh::AS_IS && mesh->mesh_type == oct::Mesh::GLOBAL)) {
            ++ulGlobalCnt;
            if(mesh->need_update && !global_update) global_update = true;
            continue;
        }
        if(!mesh->need_update) continue;
        ++ulLocalCnt;
		if(progress.get_cancel()) return;
	}

    if(ulLocalCnt) {
        char            **mesh_names            = new char*[ulLocalCnt];
        size_t          *used_shaders_size      = new size_t[ulLocalCnt];
        vector<string>  *shader_names           = new vector<string>[ulLocalCnt];
        float3          **points                = new float3*[ulLocalCnt];
        size_t          *points_size            = new size_t[ulLocalCnt];
        float3          **normals               = new float3*[ulLocalCnt];
        size_t          *normals_size           = new size_t[ulLocalCnt];
        int             **points_indices        = new int*[ulLocalCnt];
        int             **normals_indices       = new int*[ulLocalCnt];
        size_t          *points_indices_size    = new size_t[ulLocalCnt];
        size_t          *normals_indices_size   = new size_t[ulLocalCnt];
        int             **vert_per_poly         = new int*[ulLocalCnt];
        size_t          *vert_per_poly_size     = new size_t[ulLocalCnt];
        int             **poly_mat_index        = new int*[ulLocalCnt];
        float3          **uvs                   = new float3*[ulLocalCnt];
        size_t          *uvs_size               = new size_t[ulLocalCnt];
        int             **uv_indices            = new int*[ulLocalCnt];
        size_t          *uv_indices_size        = new size_t[ulLocalCnt];
        bool            *use_subdivision        = new bool[ulLocalCnt];
        float           *subdiv_divider         = new float[ulLocalCnt];

        uint64_t i = 0;
        vector<Mesh*>::iterator it;
        for(it = scene->meshes.begin(); it != scene->meshes.end(); ++it) {
            Mesh *mesh = *it;
            if(scene->meshes_type == Mesh::GLOBAL || (scene->meshes_type == Mesh::AS_IS && mesh->mesh_type == oct::Mesh::GLOBAL) || mesh->empty || !mesh->need_update) continue;

            if(scene->meshes_type == Mesh::SCATTER || (scene->meshes_type == Mesh::AS_IS && mesh->mesh_type == Mesh::SCATTER))
                progress.set_status("Loading Meshes to render-server", string("Scatter: ") + mesh->name.c_str());
            else if(scene->meshes_type == Mesh::MOVABLE_PROXY || (scene->meshes_type == Mesh::AS_IS && mesh->mesh_type == Mesh::MOVABLE_PROXY))
                progress.set_status("Loading Meshes to render-server", string("Movable: ") + mesh->name.c_str());

            used_shaders_size[i] = mesh->used_shaders.size();
            for(size_t n=0; n<used_shaders_size[i]; ++n) {
                shader_names[i].push_back(scene->shaders[mesh->used_shaders[n]]->name);
            }
            mesh_names[i]           = (char*) mesh->name.c_str();
            points[i]               = &mesh->points[0];
            points_size[i]          = mesh->points.size();
            normals[i]              = &mesh->normals[0];
            normals_size[i]         = mesh->normals.size();
            points_indices[i]       = &mesh->points_indices[0];
            normals_indices[i]      = &mesh->points_indices[0];
            points_indices_size[i]  = mesh->points_indices.size();
            normals_indices_size[i] = 0;//mesh->points_indices.size();
            vert_per_poly[i]        = &mesh->vert_per_poly[0];
            vert_per_poly_size[i]   = mesh->vert_per_poly.size();
            poly_mat_index[i]       = &mesh->poly_mat_index[0];
            uvs[i]                  = &mesh->uvs[0];
            uvs_size[i]             = mesh->uvs.size();
            uv_indices[i]           = &mesh->uv_indices[0];
            uv_indices_size[i]      = mesh->uv_indices.size();
            use_subdivision[i]      = mesh->use_subdivision;
            subdiv_divider[i]       = mesh->subdiv_divider;

        	if(mesh->need_update) mesh->need_update = false;
    		if(progress.get_cancel()) return;
            ++i;
	    }
        progress.set_status("Loading Meshes to render-server", "Transferring...");
	    server->load_mesh(false, ulLocalCnt, mesh_names,
                                    used_shaders_size,
                                    shader_names,
                                    points,
                                    points_size,
                                    normals,
                                    normals_size,
                                    points_indices,
                                    normals_indices,
                                    points_indices_size,
                                    normals_indices_size,
                                    vert_per_poly,
                                    vert_per_poly_size,
                                    poly_mat_index,
                                    uvs,
                                    uvs_size,
                                    uv_indices,
                                    uv_indices_size,
                                    use_subdivision,
                                    subdiv_divider);
        delete[] mesh_names;
        delete[] used_shaders_size;
        delete[] shader_names;
        delete[] points;
        delete[] points_size;
        delete[] normals;
        delete[] normals_size;
        delete[] points_indices;
        delete[] normals_indices;
        delete[] points_indices_size;
        delete[] normals_indices_size;
        delete[] vert_per_poly;
        delete[] vert_per_poly_size;
        delete[] poly_mat_index;
        delete[] uvs;
        delete[] uvs_size;
        delete[] uv_indices;
        delete[] uv_indices_size;
        delete[] use_subdivision;
        delete[] subdiv_divider;
    }
    if(global_update) {
        progress.set_status("Loading global Mesh to render-server", "");
        uint64_t obj_cnt = 0;
        for(map<Mesh*, vector<Object*> >::const_iterator obj_it = scene->objects.begin(); obj_it != scene->objects.end(); ++obj_it) {
            Mesh* mesh = obj_it->first;
            if((scene->meshes_type == Mesh::SCATTER || scene->meshes_type == Mesh::MOVABLE_PROXY) || (scene->meshes_type == Mesh::AS_IS && mesh->mesh_type != oct::Mesh::GLOBAL) || mesh->empty) continue;
            obj_cnt += obj_it->second.size();
        }
        if(obj_cnt > 0) {
            size_t          *used_shaders_size      = new size_t[obj_cnt];
            vector<string>  *shader_names           = new vector<string>[obj_cnt];
            float3          **points                = new float3*[obj_cnt];
            size_t          *points_size            = new size_t[obj_cnt];
            float3          **normals               = new float3*[obj_cnt];
            size_t          *normals_size           = new size_t[obj_cnt];
            int             **points_indices        = new int*[obj_cnt];
            int             **normals_indices       = new int*[obj_cnt];
            size_t          *points_indices_size    = new size_t[obj_cnt];
            size_t          *normals_indices_size   = new size_t[obj_cnt];
            int             **vert_per_poly         = new int*[obj_cnt];
            size_t          *vert_per_poly_size     = new size_t[obj_cnt];
            int             **poly_mat_index        = new int*[obj_cnt];
            float3          **uvs                   = new float3*[obj_cnt];
            size_t          *uvs_size               = new size_t[obj_cnt];
            int             **uv_indices            = new int*[obj_cnt];
            size_t          *uv_indices_size        = new size_t[obj_cnt];
            bool            *use_subdivision        = new bool[obj_cnt];
            float           *subdiv_divider         = new float[obj_cnt];

            obj_cnt = 0;
            for(map<Mesh*, vector<Object*> >::const_iterator obj_it = scene->objects.begin(); obj_it != scene->objects.end(); ++obj_it) {
                Mesh* mesh = obj_it->first;
                if((scene->meshes_type == Mesh::SCATTER || scene->meshes_type == Mesh::MOVABLE_PROXY) || (scene->meshes_type == Mesh::AS_IS && mesh->mesh_type != oct::Mesh::GLOBAL) || mesh->empty) continue;

                for(vector<Object*>::const_iterator it = obj_it->second.begin(); it != obj_it->second.end(); ++it) {
    		        Object *mesh_object = *it;
                    Transform &tfm = mesh_object->tfm;

                    used_shaders_size[obj_cnt] = mesh_object->used_shaders.size();
                    for(size_t n=0; n<used_shaders_size[obj_cnt]; ++n) {
                        shader_names[obj_cnt].push_back(scene->shaders[mesh_object->used_shaders[n]]->name);
                    }
                    size_t points_cnt             = mesh->points.size();
                    points[obj_cnt]               = new float3[points_cnt];
                    float3 *p                     = &mesh->points[0];
                    for(size_t k=0; k<points_cnt; ++k) points[obj_cnt][k] = transform_point(&tfm, p[k]);
                    points_size[obj_cnt]          = points_cnt;

                    size_t norm_cnt               = mesh->normals.size();
                    normals[obj_cnt]              = new float3[norm_cnt];
                    float3 *n                     = &mesh->normals[0];
                    for(size_t k=0; k<norm_cnt; ++k) normals[obj_cnt][k] = transform_direction(&tfm, n[k]);
                    normals_size[obj_cnt]         = norm_cnt;

                    points_indices[obj_cnt]       = &mesh->points_indices[0];
                    normals_indices[obj_cnt]      = &mesh->points_indices[0];
                    points_indices_size[obj_cnt]  = mesh->points_indices.size();
                    normals_indices_size[obj_cnt] = 0;
                    vert_per_poly[obj_cnt]        = &mesh->vert_per_poly[0];
                    vert_per_poly_size[obj_cnt]   = mesh->vert_per_poly.size();
                    poly_mat_index[obj_cnt]       = &mesh->poly_mat_index[0];
                    uvs[obj_cnt]                  = &mesh->uvs[0];
                    uvs_size[obj_cnt]             = mesh->uvs.size();
                    uv_indices[obj_cnt]           = &mesh->uv_indices[0];
                    uv_indices_size[obj_cnt]      = mesh->uv_indices.size();
                    use_subdivision[obj_cnt]      = mesh->use_subdivision;
                    subdiv_divider[obj_cnt]       = mesh->subdiv_divider;

        	        if(mesh->need_update) mesh->need_update = false;
    		        if(progress.get_cancel()) return;
                    ++obj_cnt;
                }
            }
            progress.set_status("Loading global Mesh to render-server", string("Transferring..."));
            char* name = "__global";
	        server->load_mesh(true, obj_cnt, &name,
                                        used_shaders_size,
                                        shader_names,
                                        points,
                                        points_size,
                                        normals,
                                        normals_size,
                                        points_indices,
                                        normals_indices,
                                        points_indices_size,
                                        normals_indices_size,
                                        vert_per_poly,
                                        vert_per_poly_size,
                                        poly_mat_index,
                                        uvs,
                                        uvs_size,
                                        uv_indices,
                                        uv_indices_size,
                                        use_subdivision,
                                        subdiv_divider);
            delete[] used_shaders_size;
            delete[] shader_names;
            delete[] points;
            delete[] points_size;
            delete[] normals;
            delete[] normals_size;
            delete[] points_indices;
            delete[] normals_indices;
            delete[] points_indices_size;
            delete[] normals_indices_size;
            delete[] vert_per_poly;
            delete[] vert_per_poly_size;
            delete[] poly_mat_index;
            delete[] uvs;
            delete[] uvs_size;
            delete[] uv_indices;
            delete[] uv_indices_size;
            delete[] use_subdivision;
            delete[] subdiv_divider;
        }
    }
    std::string cur_name("__global");
    if(!ulGlobalCnt) server->delete_mesh(true, cur_name);
	//need_update = false;
} //server_update_mesh()

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Update render-server (sends "LOAD_MESH" packets finally)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MeshManager::server_update(RenderServer *server, Scene *scene, Progress& progress) {
	if(!need_update) return;

    server_update_mesh(server, scene, progress);

	need_update = false;
} //server_update()

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Tag all meshes to update
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MeshManager::tag_update(Scene *scene) {
	need_update = true;
	scene->object_manager->need_update = true;
} //tag_update()

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Tag global mesh to update
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MeshManager::tag_global_update() {
	need_update         = true;
	need_global_update  = true;
} //tag_global_update()

OCT_NAMESPACE_END
