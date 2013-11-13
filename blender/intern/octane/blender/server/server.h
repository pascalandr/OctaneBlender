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

#ifndef __SERVER_H__
#define __SERVER_H__

#undef htonl
#undef ntohl

#ifndef WIN32
#  include <unistd.h> // for read close
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netinet/ip.h>
#  include <netdb.h>
#else
#  include "winsock2.h"
#  include <io.h> // for open close read
#endif

#undef max
#undef min

#pragma push_macro("htonl")
#define htonl 1
#pragma push_macro("ntohl")
#define ntohl 1

#include "camera.h"
#include "nodes.h"
#include "kernel.h"
#include "server.h"
#include "buffers.h"
#include "environment.h"

#include <stdlib.h>

#include "util_string.h"
#include "util_thread.h"
#include "util_types.h"
#include "util_lists.h"
#include "util_progress.h"
#include <OpenImageIO/ustring.h>

#include "nodes.h"
#include "blender_session.h"

#pragma pop_macro("htonl")
#pragma pop_macro("ntohl")

#include "BLI_math_color.h"

OCT_NAMESPACE_BEGIN

using std::cout;
using std::exception;

static const int SERVER_PORT = 5130;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
enum PacketType {
    NONE = 0,

    GET_LDB_CAT,
    GET_LDB_MAT_PREVIEW,
    GET_LDB_MAT,

    ERROR_PACKET,
    DESCRIPTION,
    SET_LIC_DATA,
    RESET,
    START,
    UPDATE,
    PAUSE,
    GET_IMAGE,
    GET_SAMPLES,

    LOAD_GPU,
    LOAD_KERNEL,
    LOAD_THIN_LENS_CAMERA,
    LOAD_PANORAMIC_CAMERA,
    //LOAD_IMAGER,
    //LOAD_POSTPROCESSOR,
    LOAD_SUNSKY,

    FIRST_NAMED_PACKET,
    LOAD_GLOBAL_MESH = FIRST_NAMED_PACKET,
    DEL_GLOBAL_MESH,
    LOAD_LOCAL_MESH,
    DEL_LOCAL_MESH,
    LOAD_GEO_MAT,
    DEL_GEO_MAT,
    LOAD_GEO_SCATTER,
    DEL_GEO_SCATTER,

    LOAD_MATERIAL_FIRST,
    LOAD_DIFFUSE_MATERIAL = LOAD_MATERIAL_FIRST,
    LOAD_GLOSSY_MATERIAL,
    LOAD_SPECULAR_MATERIAL,
    LOAD_MIX_MATERIAL,
    LOAD_PORTAL_MATERIAL,
    LOAD_MATERIAL_LAST = LOAD_PORTAL_MATERIAL,
    DEL_MATERIAL,

    LOAD_TEXTURE_FIRST,
    LOAD_FLOAT_TEXTURE = LOAD_TEXTURE_FIRST,
    LOAD_RGB_SPECTRUM_TEXTURE,
    LOAD_GAUSSIAN_SPECTRUM_TEXTURE,
    LOAD_CHECKS_TEXTURE,
    LOAD_MARBLE_TEXTURE,
    LOAD_RIDGED_FRACTAL_TEXTURE,
    LOAD_SAW_WAVE_TEXTURE,
    LOAD_SINE_WAVE_TEXTURE,
    LOAD_TRIANGLE_WAVE_TEXTURE,
    LOAD_TURBULENCE_TEXTURE,
    LOAD_CLAMP_TEXTURE,
    LOAD_COSINE_MIX_TEXTURE,
    LOAD_INVERT_TEXTURE,
    LOAD_MIX_TEXTURE,
    LOAD_MULTIPLY_TEXTURE,
    LOAD_IMAGE_TEXTURE,
    LOAD_ALPHA_IMAGE_TEXTURE,
    LOAD_FLOAT_IMAGE_TEXTURE,
    LOAD_FALLOFF_TEXTURE,
    LOAD_COLOR_CORRECT_TEXTURE,
    LOAD_DIRT_TEXTURE,
    LOAD_TEXTURE_LAST = LOAD_DIRT_TEXTURE,
    DEL_TEXTURE,

    LOAD_EMISSION_FIRST,
    LOAD_BLACKBODY_EMISSION = LOAD_EMISSION_FIRST,
    LOAD_TEXTURE_EMISSION,
    LOAD_EMISSION_LAST = LOAD_TEXTURE_EMISSION,
    DEL_EMISSION,

    LOAD_TRANSFORM_FIRST,
    LOAD_SCALE_TRANSFORM = LOAD_TRANSFORM_FIRST,
    LOAD_ROTATION_TRANSFORM,
    LOAD_FULL_TRANSFORM,
    LOAD_TRANSFORM_LAST = LOAD_FULL_TRANSFORM,
    DEL_TRANSFORM,

    LOAD_MEDIUM_FIRST,
    LOAD_ABSORPTION_MEDIUM = LOAD_MEDIUM_FIRST,
    LOAD_SCATTERING_MEDIUM,
    LOAD_MEDIUM_LAST = LOAD_SCATTERING_MEDIUM,
    DEL_MEDIUM,

    LAST_NAMED_PACKET = DEL_MEDIUM,

    LAST_PACKET_TYPE = LAST_NAMED_PACKET
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Send data class
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct RPCSend {
    RPCSend(int socket_, uint64_t buf_size_, PacketType type_, const char* szName = 0) : type(type_), socket(socket_), buf_pointer(0) {
        buf_size = buf_size_ + sizeof(uint64_t)*2;
        size_t name_len, name_buf_len;

        if(szName) {
            name_len = strlen(szName);
            if(name_len > 245) name_len = 245;
            name_buf_len = name_len+1+1;
            name_buf_len += sizeof(uint64_t) - name_buf_len%sizeof(uint64_t);
            buf_size += name_buf_len;
        }
        else {
            name_len = 0;
            name_buf_len = 0;
        }
        buf_pointer = buffer = new uint8_t[static_cast<unsigned int>(buf_size)];

        *reinterpret_cast<uint64_t*>(buf_pointer) = static_cast<uint64_t>(type_);
        buf_pointer += sizeof(uint64_t);
        *reinterpret_cast<uint64_t*>(buf_pointer) = buf_size - sizeof(uint64_t)*2;
        buf_pointer += sizeof(uint64_t);
        if(szName) {
            *reinterpret_cast<uint8_t*>(buf_pointer) = static_cast<uint8_t>(name_buf_len);
#ifndef WIN32
            strncpy(reinterpret_cast<char*>(buf_pointer+1), szName, name_len+1);
#else
            strncpy_s(reinterpret_cast<char*>(buf_pointer+1), name_len+1, szName, name_len);
#endif
            buf_pointer += name_buf_len;
        }
    }
    ~RPCSend() {
        if(buffer) delete[] buffer;
    }

    inline RPCSend& operator<<(float& val) {
        if(buffer && (buf_pointer+sizeof(float) <= buffer+buf_size)) {
            *reinterpret_cast<float*>(buf_pointer) = val;
            buf_pointer += sizeof(float);
        }
        return *this;
    }

    inline RPCSend& operator<<(float3& val) {
        if(buffer && (buf_pointer+sizeof(float3) <= buffer+buf_size)) {
            *reinterpret_cast<float3*>(buf_pointer) = val;
            buf_pointer += sizeof(float3);
        }
        return *this;
    }

    inline RPCSend& operator<<(double& val) {
        if(buffer && (buf_pointer+sizeof(double) <= buffer+buf_size)) {
            *reinterpret_cast<double*>(buf_pointer) = val;
            buf_pointer += sizeof(double);
        }
        return *this;
    }

    inline RPCSend& operator<<(uint64_t& val) {
        if(buffer && (buf_pointer+sizeof(uint64_t) <= buffer+buf_size)) {
            *reinterpret_cast<uint64_t*>(buf_pointer) = val;
            buf_pointer += sizeof(uint64_t);
        }
        return *this;
    }

    inline RPCSend& operator<<(uint32_t& val) {
        if(buffer && (buf_pointer+sizeof(uint32_t) <= buffer+buf_size)) {
            *reinterpret_cast<uint32_t*>(buf_pointer) = val;
            buf_pointer += sizeof(uint32_t);
        }
        return *this;
    }

    inline RPCSend& operator<<(int32_t& val) {
        if(buffer && (buf_pointer+sizeof(int32_t) <= buffer+buf_size)) {
            *reinterpret_cast<int32_t*>(buf_pointer) = val;
            buf_pointer += sizeof(int32_t);
        }
        return *this;
    }

    inline RPCSend& operator<<(bool& val) {
        if(buffer && (buf_pointer+sizeof(int32_t) <= buffer+buf_size)) {
            *reinterpret_cast<uint32_t*>(buf_pointer) = val;
            buf_pointer += sizeof(uint32_t);
        }
        return *this;
    }

    inline RPCSend& operator<<(char* val) {
        if(buffer) {
            size_t len = strlen(val);
            if(buf_pointer+len+2 > buffer+buf_size) return *this;
            if(len > 254) len = 254;
            *reinterpret_cast<uint8_t*>(buf_pointer) = static_cast<uint8_t>(len);
            buf_pointer += sizeof(uint8_t);
#ifndef WIN32
            strncpy(reinterpret_cast<char*>(buf_pointer), val, len+1);
#else
            strncpy_s(reinterpret_cast<char*>(buf_pointer), len+1, val, len);
#endif
            buf_pointer += len+1;
        }
        return *this;
    }

    inline RPCSend& operator<<(const char* val) {
        if(!val) return this->operator<<("");
        else return this->operator<<(const_cast<char*>(val));
    }

    inline RPCSend& operator<<(string& val) {
        return this->operator<<(val.c_str());
    }

    bool write_buffer(void* buf, uint64_t len) {
        if(buffer && (buf_pointer+len <= buffer+buf_size)) {
            memcpy(buf_pointer, buf, static_cast<size_t>(len));
            buf_pointer += len;
            return true;
        }
        else return false;
    }

    bool write_float3_buffer(float3* buf, uint64_t len) {
        if(buffer && (buf_pointer + len*sizeof(float)*3 <= buffer+buf_size)) {
            register float* ptr = (float*)buf_pointer;
            for(register uint64_t i=0; i<len; ++i) {
                *(ptr++) = buf[i].x;
                *(ptr++) = buf[i].y;
                *(ptr++) = buf[i].z;
            }
            buf_pointer = (uint8_t*)ptr;
            return true;
        }
        else return false;
    }

    bool write() {
        if(!buffer) return false;

        if(::send(socket, (const char*)buffer, static_cast<unsigned int>(buf_size), 0) < 0) {
            cout << "Octane: Network send error\n";
            return false;
        }
        else {
            delete[] buffer;
            buffer      = 0;
            buf_size    = 0;
            buf_pointer = 0;
        }
        return true;
    }

private:
    RPCSend() {}

    PacketType  type;
    int         socket;
    uint8_t     *buffer;
    uint64_t    buf_size;
    uint8_t     *buf_pointer;
} RPCSend;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Receive data class
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct RPCReceive {
    RPCReceive(int socket_) : socket(socket_), buffer(0), buf_pointer(0) {
        uint64_t uiTmp[2];

        if(::recv(socket_, (char*)uiTmp, sizeof(uint64_t)*2, MSG_WAITALL) != sizeof(uint64_t)*2) {
            type = NONE;
            buf_size = 0;
            return;
        }
        else {
            type = static_cast<PacketType>(uiTmp[0]);
            buf_size = uiTmp[1];
        }

        if(buf_size > 0) {
            buf_pointer = buffer = new uint8_t[static_cast<unsigned int>(buf_size)];

            if(::recv(socket_, (char*)buffer, static_cast<unsigned int>(buf_size), MSG_WAITALL) != buf_size) {
                delete[] buffer;
                buf_pointer = buffer = 0;
                type = NONE;
                buf_size = 0;
                return;
            }
        }
        if(buffer && type >= FIRST_NAMED_PACKET && type <= LAST_NAMED_PACKET) {
            uint8_t len = *reinterpret_cast<uint8_t*>(buf_pointer);

            name = reinterpret_cast<char*>(buf_pointer+1);
            buf_pointer += len;
        }
    }

    ~RPCReceive() {
        if(buffer) delete[] buffer;
    }

    inline RPCReceive& operator>>(float& val) {
        if(buffer && (buf_pointer+sizeof(float) <= buffer+buf_size)) {
            val = *reinterpret_cast<float*>(buf_pointer);
            buf_pointer += sizeof(float);
        }
        else val = 0;
        return *this;
    }

    inline RPCReceive& operator>>(double& val) {
        if(buffer && (buf_pointer+sizeof(double) <= buffer+buf_size)) {
            val = *reinterpret_cast<double*>(buf_pointer);
            buf_pointer += sizeof(double);
        }
        else val = 0;
        return *this;
    }

    inline RPCReceive& operator>>(bool& val) {
        if(buffer && (buf_pointer+sizeof(uint32_t) <= buffer+buf_size)) {
            val = (*reinterpret_cast<uint32_t*>(buf_pointer) != 0);
            buf_pointer += sizeof(uint32_t);
        }
        else val = 0;
        return *this;
    }

    inline RPCReceive& operator>>(uint64_t& val) {
        if(buffer && (buf_pointer+sizeof(uint64_t) <= buffer+buf_size)) {
            val = *reinterpret_cast<uint64_t*>(buf_pointer);
            buf_pointer += sizeof(uint64_t);
        }
        else val = 0;
        return *this;
    }

    inline RPCReceive& operator>>(uint32_t& val) {
        if(buffer && (buf_pointer+sizeof(uint32_t) <= buffer+buf_size)) {
            val = *reinterpret_cast<uint32_t*>(buf_pointer);
            buf_pointer += sizeof(uint32_t);
        }
        else val = 0;
        return *this;
    }

    inline RPCReceive& operator>>(int32_t& val) {
        if(buffer && (buf_pointer+sizeof(int32_t) <= buffer+buf_size)) {
            val = *reinterpret_cast<int32_t*>(buf_pointer);
            buf_pointer += sizeof(int32_t);
        }
        else val = 0;
        return *this;
    }

    inline RPCReceive& operator>>(char*& val) {
        if(buffer && (buf_pointer+1 <= buffer+buf_size)) {
            uint8_t len = *reinterpret_cast<uint8_t*>(buf_pointer);
            buf_pointer += sizeof(uint8_t);
            if(buf_pointer+len+1 > buffer+buf_size) {
                val = "";
                return *this;
            }
            val = reinterpret_cast<char*>(buf_pointer);
            buf_pointer += len+1;
        }
        else val = "";
        return *this;
    }

    inline RPCReceive& operator>>(string& val) {
        if(buffer && (buf_pointer+1 <= buffer+buf_size)) {
            uint8_t len = *reinterpret_cast<uint8_t*>(buf_pointer);
            buf_pointer += sizeof(uint8_t);
            if(buf_pointer+len+1 > buffer+buf_size) {
                val = "";
                return *this;
            }
            val = reinterpret_cast<char*>(buf_pointer);
            buf_pointer += len+1;
        }
        else val = "";
        return *this;
    }

    void* read_buffer(uint64_t len) {
        if(buffer && (buf_pointer+len <= buffer+buf_size)) {
            void* ret = reinterpret_cast<void*>(buf_pointer);
            buf_pointer += len;
            return ret;
        }
        else return 0;
    }

    int         socket;
    PacketType  type;
    char*       name;

private:
    RPCReceive() {}

    uint8_t  *buffer;
    uint64_t buf_size;
    uint8_t  *buf_pointer;
} RPCReceive;



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Render Server info structures
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class RenderServerInfo {
public:
	string  description;
	bool    pack_images;
    char    net_address[256];

	RenderServerInfo() {
		pack_images = false;
        ::strcpy(net_address, "127.0.0.1");
	}
}; //RenderServerInfo

class ImageStatistics {
public:
    uint32_t cur_samples;
    uint64_t used_vram, free_vram, total_vram;
    float spp;
    uint32_t tri_cnt, meshes_cnt;
    uint32_t rgb32_cnt, rgb32_max, rgb64_cnt, rgb64_max, grey8_cnt, grey8_max, grey16_cnt, grey16_max;
}; //ImageStatistics

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Render Server
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class RenderServer {
    uint8_t*    image_buf;
    float*      float_img_buf;
    int32_t     cur_w, cur_h;

protected:
	RenderServer() {}

	string error_msg;

public:
    static char         address[256];
    thread_mutex        socket_mutex;
    thread_mutex        img_buf_mutex;
	RenderServerInfo    info;

    int socket;

    // Create the render-server object, connected to the server
    //  addr - server address
    RenderServer(const char *addr) : image_buf(0), float_img_buf(0), cur_w(0), cur_h(0), socket(-1) {
        struct  hostent *host;
        struct  sockaddr_in sa;

        strcpy(address, addr);
        host = gethostbyname(addr);
        if(!host || host->h_length != sizeof(struct in_addr)) {
            throw exception();
        }
        socket = ::socket(AF_INET, SOCK_STREAM, 0);
        if(socket < 0) throw exception();

        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        sa.sin_port = htons(0);
        sa.sin_addr.s_addr = htonl(INADDR_ANY);
        if(::bind(socket, (struct sockaddr*) &sa, sizeof(sa)) < 0) {
#ifndef WIN32
            close(socket);
#else
            closesocket(socket);
#endif
            throw exception();
        }
        sa.sin_port = htons(SERVER_PORT);
        sa.sin_addr = *(struct in_addr*) host->h_addr;
        if(::connect(socket, (struct sockaddr*) &sa, sizeof(sa)) < 0) {
#ifndef WIN32
            close(socket);
#else
            closesocket(socket);
#endif
            socket = -1;
        }
    } //RenderServer()

    ~RenderServer() {
        reset(0);
        if(socket >= 0)
#ifndef WIN32
            close(socket);
#else
            closesocket(socket);
#endif
        if(image_buf) delete[] image_buf;
        if(float_img_buf) delete[] float_img_buf;
    } //~RenderServer()

    // Get the last error message
	inline const string& error_message() {
        return error_msg;
    } //error_message()

	inline void clear_error_message() {
        error_msg.clear();
    } //clear_error_message()



    // Activate the Octane license on server
    inline int activate(string& sLogin, string& sPass) {
        if(socket < 0) return 0;

        thread_scoped_lock socket_lock(socket_mutex);

        RPCSend snd(socket, sLogin.length()+2 + sPass.length()+2, SET_LIC_DATA);
        snd << sLogin << sPass;
        snd.write();

        RPCReceive rcv(socket);
        if(rcv.type != SET_LIC_DATA) {
            rcv >> error_msg;
            fprintf(stderr, "Octane: ERROR activate render-server: %s\n", error_msg.c_str());
            return 0;
        }
        else return 1;
    } //activate()

    // Reset the server project (SOFT reset).
    // GPUs - the GPUs which should be used by server, as bit map
    inline void reset(uint32_t GPUs) {
        if(socket < 0) return;

        thread_scoped_lock socket_lock(socket_mutex);

        RPCSend snd(socket, sizeof(uint32_t), RESET);
        snd << GPUs;
        snd.write();

        RPCReceive rcv(socket);
        if(rcv.type != RESET) {
            rcv >> error_msg;
            fprintf(stderr, "Octane: ERROR resetting render server: %s\n", error_msg.c_str());
        }
    } //reset()

    // GPUs - the GPUs which should be used by server, as bit map
    inline void load_GPUs(uint32_t GPUs) {
        if(socket < 0) return;

        thread_scoped_lock socket_lock(socket_mutex);

        RPCSend snd(socket, sizeof(uint32_t), LOAD_GPU);
        snd << GPUs;
        snd.write();

        RPCReceive rcv(socket);
        if(rcv.type != LOAD_GPU) {
            rcv >> error_msg;
            fprintf(stderr, "Octane: ERROR setting GPUs on render server: %s\n", error_msg.c_str());
        }
    } //load_GPUs()

    // Update current render project on the server.
    inline bool update() {
        if(socket < 0) return false;

        thread_scoped_lock socket_lock(socket_mutex);

        RPCSend snd(socket, 0, UPDATE);
        snd.write();

        RPCReceive rcv(socket);
        if(rcv.type != UPDATE) {
            return false;
        }
        else return true;
    } //update()

    // Start the render process on the server.
    // w - the width of the the rendered image needed 
    // h - the height of the the rendered image needed
    inline void start_render(int32_t w, int32_t h, uint32_t img_type) {
        if(socket < 0) return;

        thread_scoped_lock socket_lock(socket_mutex);

        RPCSend snd(socket, sizeof(int32_t) * 2 + sizeof(uint32_t), START);
        snd << w << h << img_type;
        snd.write();

        RPCReceive rcv(socket);
        if(rcv.type != START) {
            rcv >> error_msg;
            fprintf(stderr, "Octane: ERROR starting render: %s\n", error_msg.c_str());
        }
    } //start_render()

    inline void pause_render(int32_t pause) {
        if(socket < 0) return;

        thread_scoped_lock socket_lock(socket_mutex);

        RPCSend snd(socket, sizeof(int32_t), PAUSE);
        snd << pause;
        snd.write();

        RPCReceive rcv(socket);
        if(rcv.type != PAUSE) {
            rcv >> error_msg;
            fprintf(stderr, "Octane: ERROR pause render: %s\n", error_msg.c_str());
        }
    } //pause_render()

    // Load camera to render server.
    // cam - camera data structure
    inline void load_camera(Camera* cam) {
        if(socket < 0) return;

        thread_scoped_lock socket_lock(socket_mutex);

        int32_t response_type       = static_cast<int32_t>(cam->response_type);
        int32_t min_display_samples = static_cast<int32_t>(cam->min_display_samples);
        int32_t glare_ray_count     = static_cast<int32_t>(cam->glare_ray_count);

        if(cam->type == CAMERA_PANORAMA) {
            {
                RPCSend snd(socket, sizeof(float3)*4 + sizeof(float)*17 + sizeof(int32_t)*7, LOAD_PANORAMIC_CAMERA);
                snd << cam->eye_point << cam->look_at << cam->up << cam->white_balance 

                    << cam->fov_x << cam->fov_y << cam->near_clip_depth << cam->exposure << cam->fstop << cam->ISO
                    << cam->gamma << cam->vignetting << cam->saturation << cam->hot_pix << cam->white_saturation
                    << cam->bloom_power << cam->glare_power << cam->glare_angle << cam->glare_blur << cam->spectral_shift << cam->spectral_intencity

                    << cam->pan_type << response_type << min_display_samples << glare_ray_count
                    
                    << cam->premultiplied_alpha << cam->dithering << cam->postprocess;
                snd.write();
            }

            RPCReceive rcv(socket);
            if(rcv.type != LOAD_PANORAMIC_CAMERA) {
                rcv >> error_msg;
                fprintf(stderr, "Octane: ERROR loading camera: %s\n", error_msg.c_str());
            }
        }
        else {
            {
                RPCSend snd(socket, sizeof(float3)*6 + sizeof(float)*23 + sizeof(int32_t)*9, LOAD_THIN_LENS_CAMERA);
                snd << cam->eye_point << cam->look_at << cam->up << cam->left_filter << cam->right_filter << cam->white_balance 

                    << cam->aperture << cam->aperture_edge << cam->distortion << cam->focal_depth << cam->near_clip_depth << cam->lens_shift_x << cam->lens_shift_y
                    << cam->stereo_dist << cam->fov << cam->exposure << cam->fstop << cam->ISO << cam->gamma << cam->vignetting
                    << cam->saturation << cam->hot_pix << cam->white_saturation
                    << cam->bloom_power << cam->glare_power << cam->glare_angle << cam->glare_blur << cam->spectral_shift << cam->spectral_intencity

                    << response_type << min_display_samples << glare_ray_count

                    << cam->ortho << cam->autofocus << cam->stereo
                    << cam->premultiplied_alpha << cam->dithering
                    << cam->postprocess;
                snd.write();
            }

            RPCReceive rcv(socket);
            if(rcv.type != LOAD_THIN_LENS_CAMERA) {
                rcv >> error_msg;
                fprintf(stderr, "Octane: ERROR loading camera: %s\n", error_msg.c_str());
            }
        }
    } //load_camera()

    // Load kernel to render server.
    // kernel - kernel data structure
    // interactive - Max. Samples will be set according to mode choosen
    void load_kernel(Kernel* kernel, bool interactive) {
        if(socket < 0) return;

        thread_scoped_lock socket_lock(socket_mutex);

        int32_t kernel_type = static_cast<int32_t>(kernel->kernel_type);
        switch(kernel->kernel_type) {
            case Kernel::DIRECT_LIGHT:
                {
                    RPCSend snd(socket, sizeof(float)*4 + sizeof(int32_t)*9, LOAD_KERNEL);
                    int32_t gi_mode = static_cast<int32_t>(kernel->gi_mode);
                    snd << kernel_type << (interactive ? kernel->max_preview_samples : kernel->max_samples) << kernel->filter_size << kernel->ray_epsilon << kernel->rrprob << kernel->ao_dist << kernel->alpha_channel << kernel->keep_environment << kernel->alpha_shadows
                        << kernel->specular_depth << kernel->glossy_depth << gi_mode << kernel->diffuse_depth;
                    snd.write();
                }
                break;
            case Kernel::PATH_TRACE:
                {
                    RPCSend snd(socket, sizeof(float)*4 + sizeof(int32_t)*6, LOAD_KERNEL);
                    snd << kernel_type << (interactive ? kernel->max_preview_samples : kernel->max_samples) << kernel->filter_size << kernel->ray_epsilon << kernel->rrprob << kernel->caustic_blur << kernel->alpha_channel << kernel->keep_environment << kernel->alpha_shadows
                        << kernel->max_depth;
                    snd.write();
                }
                break;
            case Kernel::PMC:
                {
                    RPCSend snd(socket, sizeof(float)*6 + sizeof(int32_t)*8, LOAD_KERNEL);
                    snd << kernel_type << (interactive ? kernel->max_preview_samples : kernel->max_samples) << kernel->filter_size << kernel->ray_epsilon << kernel->rrprob << kernel->exploration << kernel->direct_light_importance << kernel->caustic_blur
                        << kernel->alpha_channel << kernel->keep_environment << kernel->alpha_shadows
                        << kernel->max_depth << kernel->max_rejects << kernel->parallelism;
                    snd.write();
                }
                break;
            case Kernel::INFO_CHANNEL:
                {
                    RPCSend snd(socket, sizeof(float)*3 + sizeof(int32_t)*4, LOAD_KERNEL);
                    int32_t info_channel_type = static_cast<int32_t>(kernel->info_channel_type);
                    snd << kernel_type << info_channel_type << kernel->filter_size << kernel->zdepth_max << kernel->uv_max
                        << kernel->alpha_channel << (interactive ? kernel->max_preview_samples : kernel->max_samples);
                    snd.write();
                }
                break;
            default:
                {
                    RPCSend snd(socket, sizeof(int32_t), LOAD_KERNEL);
                    int32_t def_kernel_type = static_cast<int32_t>(Kernel::DEFAULT);
                    snd << def_kernel_type;
                    snd.write();
                }
                break;
        }

        RPCReceive rcv(socket);
        if(rcv.type != LOAD_KERNEL) {
            rcv >> error_msg;
            fprintf(stderr, "Octane: ERROR loading kernel: %s\n", error_msg.c_str());
        }
    } //load_kernel()

    // Load environment to render server.
    // env - environment data structure
    void load_environment(Environment* env) {
        if(socket < 0) return;

        thread_scoped_lock socket_lock(socket_mutex);

        if(env->type == 0) {
            RPCSend snd(socket, sizeof(uint32_t) * 3 + sizeof(float) * 3 + env->texture.length() + 2, LOAD_SUNSKY);
            snd << env->type << env->type << env->rotation.x << env->rotation.y << env->power << env->importance_sampling << env->texture.c_str();
            snd.write();
        }
        else {
            if(env->daylight_type == 0) {
                RPCSend snd(socket, sizeof(uint32_t) * 2 + sizeof(float) * 13 + sizeof(int32_t), LOAD_SUNSKY);
                snd << env->type << env->daylight_type << env->sun_vector.x << env->sun_vector.y << env->sun_vector.z << env->power << env->turbidity << env->northoffset
                    << env->sun_size << env->sky_color.x << env->sky_color.y << env->sky_color.z << env->sunset_color.x << env->sunset_color.y << env->sunset_color.z
                    << env->model;
                snd.write();
            }
            else {
                RPCSend snd(socket, sizeof(uint32_t) * 2 + sizeof(float) * 13 + sizeof(int32_t) * 4, LOAD_SUNSKY);
                snd << env->type << env->daylight_type << env->longitude << env->latitude << env->hour << env->power << env->turbidity << env->northoffset
                    << env->sun_size << env->sky_color.x << env->sky_color.y << env->sky_color.z << env->sunset_color.x << env->sunset_color.y << env->sunset_color.z
                    << env->model << env->month << env->day << env->gmtoffset;
                snd.write();
            }
        }
        RPCReceive rcv(socket);
        if(rcv.type != LOAD_SUNSKY) {
            rcv >> error_msg;
            fprintf(stderr, "Octane: ERROR loading environment: %s\n", error_msg.c_str());
        }
    } //load_environment()

    // Load scatter node to server. Subject to change at the time. Should be more complicated. Multiple matrices in one scatter will be implemented later.
    // scatter_name - transforms name
    // mesh_name    - name of mesh scattered
    // matrices     - matrices array
    // mat_cnt      - count of matrices
    // shader_names - names of shaders assigned to transforms
    inline void load_scatter(string& scatter_name, string& mesh_name, float* matrices, uint64_t mat_cnt, vector<string>& shader_names) {
        if(socket < 0) return;
        thread_scoped_lock socket_lock(socket_mutex);

        uint64_t shaders_cnt = shader_names.size();
        
        {
            uint64_t size = sizeof(uint64_t) + mesh_name.length()+2;
            for(uint64_t n=0; n<shaders_cnt; ++n)
                size += shader_names[n].length()+2;

            RPCSend snd(socket, size, LOAD_GEO_MAT, (scatter_name+"_m__").c_str());
            snd << shaders_cnt << mesh_name;
            for(uint64_t n=0; n<shaders_cnt; ++n)
                snd << shader_names[n];
            snd.write();
        }
        {
            RPCReceive rcv(socket);
            if(rcv.type != LOAD_GEO_MAT) {
                rcv >> error_msg;
                fprintf(stderr, "Octane: ERROR loading materials of transform: %s\n", error_msg.c_str());
            }
        }

        {
            uint64_t size = sizeof(uint64_t) + scatter_name.length()+4+2 + sizeof(float)*12*mat_cnt;

            RPCSend snd(socket, size, LOAD_GEO_SCATTER, (scatter_name+"_s__").c_str());
            std::string tmp = scatter_name+"_m__";
            snd << mat_cnt << tmp;
            snd.write_buffer(matrices, sizeof(float)*12*mat_cnt);
            snd.write();
        }
        RPCReceive rcv(socket);
        if(rcv.type != LOAD_GEO_SCATTER) {
            rcv >> error_msg;
            fprintf(stderr, "Octane: ERROR loading transform: %s\n", error_msg.c_str());
        }
    } //load_scatter()
    inline void delete_scatter(string& name) {
        if(socket < 0) return;
        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, 0, DEL_GEO_SCATTER, name.c_str());
            snd.write();
        }
        {
            RPCReceive rcv(socket);
            if(rcv.type != DEL_GEO_SCATTER) {
                rcv >> error_msg;
                fprintf(stderr, "Octane: ERROR deleting transform: %s\n", error_msg.c_str());
            }
        }
    } //delete_scatter()

    // Load mesh to render server.
    inline void load_mesh(          bool            global,
                                    uint64_t        mesh_cnt,
                                    char            **names,
                                    uint64_t        *shaders_count,
                                    vector<string>  *shader_names,
                                    float3          **points,
                                    uint64_t        *points_size,
                                    float3          **normals,
                                    uint64_t        *normals_size,
                                    int             **points_indices,
                                    int             **normals_indices,
                                    uint64_t        *points_indices_size,
                                    uint64_t        *normals_indices_size,
                                    int             **vert_per_poly,
                                    uint64_t        *vert_per_poly_size,
                                    int             **poly_mat_index,
                                    float3          **uvs,
                                    uint64_t        *uvs_size,
                                    int             **uv_indices,
                                    uint64_t        *uv_indices_size,
                                    bool            *subdivide,
                                    float           *subdiv_divider) {
            if(socket < 0) return;

            thread_scoped_lock socket_lock(socket_mutex);

            {
                uint64_t size = sizeof(uint64_t) //Meshes count;
                    + sizeof(int32_t) * mesh_cnt + sizeof(float) * mesh_cnt //Subdivision addributes
                    + sizeof(uint64_t) * 8 * mesh_cnt;
                for(unsigned long i=0; i<mesh_cnt; ++i) {
                    size +=
                    + points_size[i]*sizeof(float)*3
                    + normals_size[i]*sizeof(float)*3
                    + points_indices_size[i]*sizeof(int32_t)
                    + normals_indices_size[i]*sizeof(int32_t)
                    + + 2*vert_per_poly_size[i]*sizeof(int32_t)
                    + uvs_size[i]*sizeof(float)*3
                    + uv_indices_size[i]*sizeof(int32_t);

                    if(!global) size += strlen(names[i])+2;
                    for(unsigned int n=0; n<shaders_count[i]; ++n)
                        size += shader_names[i][n].length()+2;
                }

                RPCSend snd(socket, size, global ? LOAD_GLOBAL_MESH : LOAD_LOCAL_MESH, names[0]);

                snd << mesh_cnt;

                for(unsigned long i=0; i<mesh_cnt; ++i) {
                    if(!points_size[i] || (!subdivide[i] && !normals_size[i]) || !points_indices_size[i] || !vert_per_poly_size[i] || (!subdivide[i] && !uvs_size[i]) || (!subdivide[i] && !uv_indices_size[i])) return;

                    snd << points_size[i]
                        << normals_size[i]
                        << points_indices_size[i]
                        << normals_indices_size[i]
                        << vert_per_poly_size[i]
                        << uvs_size[i]
                        << uv_indices_size[i]
                        << shaders_count[i];
                }

                for(unsigned long i=0; i<mesh_cnt; ++i) {
                    snd.write_float3_buffer(points[i], points_size[i]);
                    if(normals_size[i]) snd.write_float3_buffer(normals[i], normals_size[i]);
                    if(uvs_size[i]) snd.write_float3_buffer(uvs[i], uvs_size[i]);
                    snd << subdiv_divider[i];
                }

                for(unsigned long i=0; i<mesh_cnt; ++i) {
                    snd.write_buffer(points_indices[i], points_indices_size[i] * sizeof(int32_t));
                    snd.write_buffer(vert_per_poly[i], vert_per_poly_size[i] * sizeof(int32_t));
                    snd.write_buffer(poly_mat_index[i], vert_per_poly_size[i] * sizeof(int32_t));
                    if(normals_indices_size[i]) snd.write_buffer(normals_indices[i], normals_indices_size[i] * sizeof(int32_t));
                    if(uv_indices_size[i]) snd.write_buffer(uv_indices[i], uv_indices_size[i] * sizeof(int32_t));
                    snd << subdivide[i];
                }
                for(unsigned long i=0; i<mesh_cnt; ++i) {
                    if(!global) snd << names[i];
                    for(unsigned int n=0; n<shaders_count[i]; ++n)
                        snd << shader_names[i][n];
                }

                snd.write();
            }

            RPCReceive rcv(socket);
            if(rcv.type != (global ? LOAD_GLOBAL_MESH : LOAD_LOCAL_MESH)) {
                rcv >> error_msg;
                fprintf(stderr, "Octane: ERROR loading mesh: %s\n", error_msg.c_str());
            }
    } //load_mesh()
    inline void delete_mesh(bool global, string& name) {
        if(socket < 0) return;
        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, 0, (global ? DEL_GLOBAL_MESH : DEL_LOCAL_MESH), name.c_str());
            snd.write();
        }
        {
            RPCReceive rcv(socket);
            if(rcv.type != (global ? DEL_GLOBAL_MESH : DEL_LOCAL_MESH)) {
                rcv >> error_msg;
                fprintf(stderr, "Octane: ERROR deleting mesh: %s\n", error_msg.c_str());
            }
        }
    } //delete_mesh()

    inline bool wait_error(PacketType type) {
        if(socket < 0) return false;

        RPCReceive rcv(socket);
        if(rcv.type != type) {
            rcv >> error_msg;
            fprintf(stderr, "Octane: ERROR loading node: %s\n", error_msg.c_str());
            return false;
        }
        else return true;
    } //wait_error()

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // MATERIALS
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    inline void load_diffuse_mat(OctaneDiffuseMaterial* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) * 7 + sizeof(int32_t) * 2
            + node->diffuse.length() + 2
            + node->transmission.length() + 2
            + node->bump.length() + 2
            + node->normal.length() + 2
            + node->opacity.length() + 2
            + node->emission.length() + 2
            + node->medium.length() + 2;

        const char* mat_name = node->name.c_str();

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_DIFFUSE_MATERIAL, mat_name);
            snd << node->diffuse_default_val.x << node->diffuse_default_val.y << node->diffuse_default_val.z
                 << node->transmission_default_val << node->bump_default_val << node->normal_default_val << node->opacity_default_val
                 << node->smooth << node->matte << node->diffuse.c_str() << node->transmission.c_str() << node->bump.c_str()
                 << node->normal.c_str() << node->opacity.c_str() << node->emission.c_str() << node->medium.c_str();
            snd.write();
        }
        wait_error(LOAD_DIFFUSE_MATERIAL);
    } //load_diffuse_mat()

    inline void load_glossy_mat(OctaneGlossyMaterial* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) * 9 + sizeof(float) * 2 + sizeof(int32_t) * 1
            + node->diffuse.length() + 2
            + node->specular.length() + 2
            + node->roughness.length() + 2
            + node->filmwidth.length() + 2
            + node->bump.length() + 2
            + node->normal.length() + 2
            + node->opacity.length() + 2;

        const char* mat_name = node->name.c_str();

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_GLOSSY_MATERIAL, mat_name);
            snd << node->diffuse_default_val.x << node->diffuse_default_val.y << node->diffuse_default_val.z
                << node->specular_default_val << node->roughness_default_val << node->filmwidth_default_val << node->bump_default_val
                << node->normal_default_val << node->opacity_default_val
                << node->filmindex << node->index << node->smooth << node->diffuse.c_str() << node->specular.c_str() << node->roughness.c_str()
                << node->filmwidth.c_str() << node->bump.c_str() << node->normal.c_str() << node->opacity.c_str();
            snd.write();
        }
        wait_error(LOAD_GLOSSY_MATERIAL);
    } //load_glossy_mat()

    inline void load_specular_mat(OctaneSpecularMaterial* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) * 9 + sizeof(float) * 3 + sizeof(int32_t) * 2
            + node->reflection.length() + 2
            + node->transmission.length() + 2
            + node->filmwidth.length() + 2
            + node->bump.length() + 2
            + node->normal.length() + 2
            + node->opacity.length() + 2
            + node->roughness.length() + 2
            + node->medium.length() + 2;

        const char* mat_name = node->name.c_str();

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_SPECULAR_MATERIAL, mat_name);
            snd << node->reflection_default_val.x << node->reflection_default_val.y << node->reflection_default_val.z
                << node->transmission_default_val << node->filmwidth_default_val << node->bump_default_val << node->normal_default_val
                << node->opacity_default_val << node->roughness_default_val
                << node->index << node->filmindex << node->dispersion_coef_B << node->smooth << node->fake_shadows
                << node->reflection.c_str() << node->transmission.c_str() << node->filmwidth.c_str() << node->bump.c_str()
                << node->normal.c_str() << node->opacity.c_str() << node->roughness.c_str() << node->medium.c_str();
            snd.write();
        }
        wait_error(LOAD_SPECULAR_MATERIAL);
    } //load_specular_mat()

    inline void load_mix_mat(OctaneMixMaterial* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float)
            + node->amount.length() + 2
            + node->material1.length() + 2
            + node->material2.length() + 2;

        const char* mat_name = node->name.c_str();

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_MIX_MATERIAL, mat_name);
            snd << node->amount_default_val << node->amount.c_str() << node->material2.c_str() << node->material1.c_str();
            snd.write();
        }
        wait_error(LOAD_MIX_MATERIAL);
    } //load_mix_mat()

    inline void load_portal_mat(OctanePortalMaterial* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(int32_t);

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_PORTAL_MATERIAL, node->name.c_str());
            snd << node->enabled;
            snd.write();
        }
        wait_error(LOAD_PORTAL_MATERIAL);
    } //load_portal_mat()

    inline void delete_material(string& name) {
        if(socket < 0) return;
        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, 0, DEL_MATERIAL, name.c_str());
            snd.write();
        }
        {
            RPCReceive rcv(socket);
            if(rcv.type != DEL_MATERIAL) {
                rcv >> error_msg;
                fprintf(stderr, "Octane: ERROR deleting material: %s\n", error_msg.c_str());
            }
        }
    } //delete_material()



    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // TEXTURES
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    inline void load_float_tex(OctaneFloatTexture* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float);

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_FLOAT_TEXTURE, node->name.c_str());
            snd << node->value;
            snd.write();
        }
        wait_error(LOAD_FLOAT_TEXTURE);
    } //load_float_tex()

    inline void load_rgb_spectrum_tex(OctaneRGBSpectrumTexture* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) * 3;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_RGB_SPECTRUM_TEXTURE, node->name.c_str());
            snd << node->value[0] << node->value[1] << node->value[2];
            snd.write();
        }
        wait_error(LOAD_RGB_SPECTRUM_TEXTURE);
    } //load_rgb_spectrum_tex()

    inline void load_gaussian_spectrum_tex(OctaneGaussianSpectrumTexture* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) * 3
            + node->WaveLength.length() + 2
            + node->Width.length() + 2
            + node->Power.length() + 2;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_GAUSSIAN_SPECTRUM_TEXTURE, node->name.c_str());
            snd << node->WaveLength_default_val << node->Width_default_val << node->Power_default_val
                << node->WaveLength.c_str() << node->Width.c_str() << node->Power.c_str();
            snd.write();
        }
        wait_error(LOAD_GAUSSIAN_SPECTRUM_TEXTURE);
    } //load_gaussian_spectrum_tex()

    inline void load_checks_tex(OctaneChecksTexture* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) + node->Transform.length() + 2;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_CHECKS_TEXTURE, node->name.c_str());
            snd << node->Transform_default_val << node->Transform.c_str();
            snd.write();
        }
        wait_error(LOAD_CHECKS_TEXTURE);
    } //load_checks_tex()

    inline void load_marble_tex(OctaneMarbleTexture* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) * 4 + sizeof(int32_t)
            + node->Power.length() + 2
            + node->Offset.length() + 2
            + node->Omega.length() + 2
            + node->Variance.length() + 2
            + node->Transform.length() + 2;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_MARBLE_TEXTURE, node->name.c_str());
            snd << node->Power_default_val << node->Offset_default_val << node->Omega_default_val << node->Variance_default_val
                << node->Octaves
                << node->Power.c_str() << node->Offset.c_str() << node->Omega.c_str() << node->Variance.c_str() << node->Transform.c_str();
            snd.write();
        }
        wait_error(LOAD_MARBLE_TEXTURE);
    } //load_marble_tex()

    inline void load_ridged_fractal_tex(OctaneRidgedFractalTexture* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) * 3 + sizeof(int32_t)
            + node->Power.length() + 2
            + node->Offset.length() + 2
            + node->Lacunarity.length() + 2
            + node->Transform.length() + 2;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_RIDGED_FRACTAL_TEXTURE, node->name.c_str());
            snd << node->Power_default_val << node->Offset_default_val << node->Lacunarity_default_val
                << node->Octaves
                << node->Power.c_str() << node->Offset.c_str() << node->Lacunarity.c_str() << node->Transform.c_str();
            snd.write();
        }
        wait_error(LOAD_RIDGED_FRACTAL_TEXTURE);
    } //load_ridged_fractal_tex()

    inline void load_saw_wave_tex(OctaneSawWaveTexture* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) + sizeof(int32_t)
            + node->Offset.length() + 2
            + node->Transform.length() + 2;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_SAW_WAVE_TEXTURE, node->name.c_str());
            snd << node->Offset_default_val
                << node->Circular
                << node->Offset.c_str() << node->Transform.c_str();
            snd.write();
        }
        wait_error(LOAD_SAW_WAVE_TEXTURE);
    } //load_saw_wave_tex()

    inline void load_sine_wave_tex(OctaneSineWaveTexture* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) + sizeof(int32_t)
            + node->Offset.length() + 2
            + node->Transform.length() + 2;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_SINE_WAVE_TEXTURE, node->name.c_str());
            snd << node->Offset_default_val
                << node->Circular
                << node->Offset.c_str() << node->Transform.c_str();
            snd.write();
        }
        wait_error(LOAD_SINE_WAVE_TEXTURE);
    } //load_sine_wave_tex()

    inline void load_triangle_wave_tex(OctaneTriangleWaveTexture* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) + sizeof(int32_t)
            + node->Offset.length() + 2
            + node->Transform.length() + 2;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_TRIANGLE_WAVE_TEXTURE, node->name.c_str());
            snd << node->Offset_default_val
                << node->Circular
                << node->Offset.c_str() << node->Transform.c_str();
            snd.write();
        }
        wait_error(LOAD_TRIANGLE_WAVE_TEXTURE);
    } //load_triangle_wave_tex()

    inline void load_turbulence_tex(OctaneTurbulenceTexture* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) * 3 + sizeof(int32_t)
            + node->Power.length() + 2
            + node->Offset.length() + 2
            + node->Omega.length() + 2
            + node->Transform.length() + 2;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_TURBULENCE_TEXTURE, node->name.c_str());
            snd << node->Power_default_val << node->Offset_default_val << node->Omega_default_val
                << node->Octaves
                << node->Power.c_str() << node->Offset.c_str() << node->Omega.c_str() << node->Transform.c_str();
            snd.write();
        }
        wait_error(LOAD_TURBULENCE_TEXTURE);
    } //load_turbulence_tex()

    inline void load_clamp_tex(OctaneClampTexture* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) * 3
            + node->Input.length() + 2
            + node->Min.length() + 2
            + node->Max.length() + 2;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_CLAMP_TEXTURE, node->name.c_str());
            snd << node->Input_default_val << node->Min_default_val << node->Max_default_val
                << node->Input.c_str() << node->Min.c_str() << node->Max.c_str();
            snd.write();
        }
        wait_error(LOAD_CLAMP_TEXTURE);
    } //load_clamp_tex()

    inline void load_cosine_mix_tex(OctaneCosineMixTexture* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float)
            + node->Amount.length() + 2
            + node->Texture1.length() + 2
            + node->Texture2.length() + 2;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_COSINE_MIX_TEXTURE, node->name.c_str());
            snd << node->Amount_default_val
                << node->Amount.c_str() << node->Texture1.c_str() << node->Texture2.c_str();
            snd.write();
        }
        wait_error(LOAD_COSINE_MIX_TEXTURE);
    } //load_cosine_mix_tex()

    inline void load_invert_tex(OctaneInvertTexture* node) {
        if(socket < 0) return;

        uint64_t size = node->Texture.length() + 2;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_INVERT_TEXTURE, node->name.c_str());
            snd << node->Texture.c_str();
            snd.write();
        }
        wait_error(LOAD_INVERT_TEXTURE);
    } //load_invert_tex()

    inline void load_mix_tex(OctaneMixTexture* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float)
            + node->Amount.length() + 2
            + node->Texture1.length() + 2
            + node->Texture2.length() + 2;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_MIX_TEXTURE, node->name.c_str());
            snd << node->Amount_default_val
                << node->Amount.c_str() << node->Texture1.c_str() << node->Texture2.c_str();
            snd.write();
        }
        wait_error(LOAD_MIX_TEXTURE);
    } //load_mix_tex()

    inline void load_multiply_tex(OctaneMultiplyTexture* node) {
        if(socket < 0) return;

        uint64_t size = node->Texture1.length() + 2
            + node->Texture2.length() + 2;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_MULTIPLY_TEXTURE, node->name.c_str());
            snd << node->Texture1.c_str() << node->Texture2.c_str();
            snd.write();
        }
        wait_error(LOAD_MULTIPLY_TEXTURE);
    } //load_multiply_tex()

    inline void load_falloff_tex(OctaneFalloffTexture* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) * 3;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_FALLOFF_TEXTURE, node->name.c_str());
            snd << node->Normal << node->Grazing << node->Index;
            snd.write();
        }
        wait_error(LOAD_FALLOFF_TEXTURE);
    } //load_falloff_tex()

    inline void load_colorcorrect_tex(OctaneColorCorrectTexture* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) * 4 + sizeof(int32_t)
            + node->Texture.length() + 2
            + node->Brightness.length() + 2;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_COLOR_CORRECT_TEXTURE, node->name.c_str());
            snd << node->Brightness_default_val << node->BrightnessScale << node->BlackLevel << node->Gamma
                << node->Invert
                << node->Texture.c_str() << node->Brightness.c_str();
            snd.write();
        }
        wait_error(LOAD_COLOR_CORRECT_TEXTURE);
    } //load_colorcorrect_tex()

    inline void load_image_tex(OctaneImageTexture* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) * 4 + sizeof(int32_t)
            + node->FileName.length() + 2
            + node->Power.length() + 2;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_IMAGE_TEXTURE, node->name.c_str());
            snd << node->Power_default_val << node->Gamma << node->Scale.x << node->Scale.y
                << node->Invert
                << node->FileName.c_str() << node->Power.c_str();
            snd.write();
        }
        wait_error(LOAD_IMAGE_TEXTURE);
    } //load_image_tex()

    inline void load_float_image_tex(OctaneFloatImageTexture* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) * 4 + sizeof(int32_t)
            + node->FileName.length() + 2
            + node->Power.length() + 2;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_FLOAT_IMAGE_TEXTURE, node->name.c_str());
            snd << node->Power_default_val << node->Gamma << node->Scale.x << node->Scale.y
                << node->Invert
                << node->FileName.c_str() << node->Power.c_str();
            snd.write();
        }
        wait_error(LOAD_FLOAT_IMAGE_TEXTURE);
    } //load_float_image_tex()

    inline void load_alpha_image_tex(OctaneAlphaImageTexture* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) * 4 + sizeof(int32_t)
            + node->FileName.length() + 2
            + node->Power.length() + 2;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_ALPHA_IMAGE_TEXTURE, node->name.c_str());
            snd << node->Power_default_val << node->Gamma << node->Scale.x << node->Scale.y
                << node->Invert
                << node->FileName.c_str() << node->Power.c_str();
            snd.write();
        }
        wait_error(LOAD_ALPHA_IMAGE_TEXTURE);
    } //load_alpha_image_tex()

    inline void load_dirt_tex(OctaneDirtTexture* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) * 3 + sizeof(int32_t);

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_DIRT_TEXTURE, node->name.c_str());
            snd << node->Strength << node->Details << node->Radius
                << node->InvertNormal;
            snd.write();
        }
        wait_error(LOAD_DIRT_TEXTURE);
    } //load_dirt_tex()

    inline void delete_texture(string& name) {
        if(socket < 0) return;
        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, 0, DEL_TEXTURE, name.c_str());
            snd.write();
        }
        {
            RPCReceive rcv(socket);
            if(rcv.type != DEL_TEXTURE) {
                rcv >> error_msg;
                fprintf(stderr, "Octane: ERROR deleting texture: %s\n", error_msg.c_str());
            }
        }
    } //delete_texture()



    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // EMISSIONS
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    inline void load_bbody_emission(OctaneBlackBodyEmission* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) * 8 + sizeof(int32_t)
            + node->Efficiency.length() + 2
            + node->Distribution.length() + 2;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_BLACKBODY_EMISSION, node->name.c_str());
            snd << node->Efficiency_default_val << node->Distribution_default_val << node->Temperature << node->Power
                << node->Orientation.x << node->Orientation.y << node->Orientation.z << node->SamplingRate
                << node->Normalize
                << node->Efficiency.c_str() << node->Distribution.c_str();
            snd.write();
        }
        wait_error(LOAD_BLACKBODY_EMISSION);
    } //load_bbody_emission()

    inline void load_texture_emission(OctaneTextureEmission* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) * 7
            + node->Efficiency.length() + 2
            + node->Distribution.length() + 2;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_TEXTURE_EMISSION, node->name.c_str());
            snd << node->Efficiency_default_val << node->Distribution_default_val << node->Power << node->Orientation.x
                << node->Orientation.y << node->Orientation.z << node->SamplingRate
                << node->Efficiency.c_str() << node->Distribution.c_str();
            snd.write();
        }
        wait_error(LOAD_TEXTURE_EMISSION);
    } //load_texture_emission()

    inline void delete_emission(string& name) {
        if(socket < 0) return;
        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, 0, DEL_EMISSION, name.c_str());
            snd.write();
        }
        {
            RPCReceive rcv(socket);
            if(rcv.type != DEL_EMISSION) {
                rcv >> error_msg;
                fprintf(stderr, "Octane: ERROR deleting emission: %s\n", error_msg.c_str());
            }
        }
    } //delete_emission()



    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // MEDIUMS
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    inline void load_absorption_medium(OctaneAbsorptionMedium* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) * 2
            + node->Absorption.length() + 2;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_ABSORPTION_MEDIUM, node->name.c_str());
            snd << node->Absorption_default_val << node->Scale
                << node->Absorption.c_str();
            snd.write();
        }
        wait_error(LOAD_ABSORPTION_MEDIUM);
    } //load_absorption_medium()

    inline void load_scattering_medium(OctaneScatteringMedium* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) * 4
            + node->Absorption.length() + 2
            + node->Scattering.length() + 2
            + node->Phase.length() + 2
            + node->Emission.length() + 2;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_SCATTERING_MEDIUM, node->name.c_str());
            snd << node->Absorption_default_val << node->Scattering_default_val << node->Phase_default_val << node->Scale
                << node->Absorption.c_str() << node->Scattering.c_str() << node->Phase.c_str() << node->Emission.c_str();
            snd.write();
        }
        wait_error(LOAD_SCATTERING_MEDIUM);
    } //load_scattering_medium()

    inline void delete_medium(string& name) {
        if(socket < 0) return;
        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, 0, DEL_MEDIUM, name.c_str());
            snd.write();
        }
        {
            RPCReceive rcv(socket);
            if(rcv.type != DEL_MEDIUM) {
                rcv >> error_msg;
                fprintf(stderr, "Octane: ERROR deleting medium: %s\n", error_msg.c_str());
            }
        }
    } //delete_medium()



    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // TRANSFORMS
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    inline void load_rotation_transform(OctaneRotationTransform* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) * 3;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_ROTATION_TRANSFORM, node->name.c_str());
            snd << node->Rotation.x << node->Rotation.y << node->Rotation.z;
            snd.write();
        }
        wait_error(LOAD_ROTATION_TRANSFORM);
    } //load_rotation_transform()

    inline void load_scale_transform(OctaneScaleTransform* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) * 3;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_SCALE_TRANSFORM, node->name.c_str());
            snd << node->Scale.x << node->Scale.y << node->Scale.z;
            snd.write();
        }
        wait_error(LOAD_SCALE_TRANSFORM);
    } //load_scale_transform()

    inline void load_full_transform(OctaneFullTransform* node) {
        if(socket < 0) return;

        uint64_t size = sizeof(float) * 9;

        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, size, LOAD_FULL_TRANSFORM, node->name.c_str());
            snd << node->Rotation.x << node->Rotation.y << node->Rotation.z
                << node->Scale.x << node->Scale.y << node->Scale.z
                << node->Translation.x << node->Translation.y << node->Translation.z;
            snd.write();
        }
        wait_error(LOAD_FULL_TRANSFORM);
    } //load_full_transform()

    inline void delete_transform(string& name) {
        if(socket < 0) return;
        thread_scoped_lock socket_lock(socket_mutex);
        {
            RPCSend snd(socket, 0, DEL_TRANSFORM, name.c_str());
            snd.write();
        }
        {
            RPCReceive rcv(socket);
            if(rcv.type != DEL_TRANSFORM) {
                rcv >> error_msg;
                fprintf(stderr, "Octane: ERROR deleting medium: %s\n", error_msg.c_str());
            }
        }
    } //delete_transform()



    //FIXME: The partial buffer is not needed here ("y" parameter)...
    inline bool get_8bit_pixels(uchar4* mem, int y, int w, int h) {
        if(w <= 0) w = 4;
        if(h <= 0) h = 4;
        thread_scoped_lock img_buf_lock(img_buf_mutex);

        if(!image_buf || cur_w != w || cur_h != (h-y)) {
            if(image_buf) delete[] image_buf;
            size_t len  = w * (h-y) * 4;
            image_buf   = new unsigned char[len];

            cur_w = w;
            cur_h = h-y;
            memset(image_buf, 1, len);
            memcpy(mem + y*w, image_buf, cur_w * cur_h * 4);
            return false;
        }
        memcpy(mem + y*w, image_buf, cur_w * cur_h * 4);
        return true;
    } //get_8bit_pixels()

    inline bool get_image_buffer(ImageStatistics &image_stat, bool interactive, PassType type, Progress &progress) {
        if(socket < 0) return false;

        thread_scoped_lock socket_lock(socket_mutex);

        while(true) {
            if(progress.get_cancel()) return false;
            {
                RPCSend snd(socket, sizeof(int32_t)*2 + sizeof(uint32_t), GET_IMAGE);
                uint32_t uiType = (interactive ? 0 : 2);
                snd << uiType << cur_w << cur_h;
                snd.write();
            }

            RPCReceive rcv(socket);
            if(rcv.type == GET_IMAGE) {
                uint64_t ullUsedMem = 0, ullFreeMem = 0, ullTotalMem = 0;
                uint32_t ui32TriCnt = 0, ui32MeshCnt = 0;
                uint32_t uiRgb32Cnt = 0, uiRgb32Max = 0, uiRgb64Cnt = 0, uiRgb64Max = 0, uiGrey8Cnt = 0, uiGrey8Max = 0, uiGrey16Cnt = 0, uiGrey16Max = 0;
                uint32_t uiW, uiH, uiSamples;
                rcv >> image_stat.used_vram >> image_stat.free_vram >> image_stat.total_vram >> image_stat.spp >> image_stat.tri_cnt >> image_stat.meshes_cnt >> image_stat.rgb32_cnt >> image_stat.rgb32_max
                    >> image_stat.rgb64_cnt >> image_stat.rgb64_max >> image_stat.grey8_cnt >> image_stat.grey8_max >> image_stat.grey16_cnt >> image_stat.grey16_max >> uiSamples >> uiW >> uiH;

                if(uiSamples) image_stat.cur_samples = uiSamples;

                thread_scoped_lock img_buf_lock(img_buf_mutex);
                if(interactive) {
                    if(!image_buf || static_cast<uint32_t>(cur_w) != uiW || static_cast<uint32_t>(cur_h) != uiH) return false;

                    size_t  len     = uiW * uiH * 4;
                    uint8_t *ucBuf  = (uint8_t*)rcv.read_buffer(len);

                    memcpy(image_buf, ucBuf, len);
                }
                else {
                    if(!float_img_buf || static_cast<uint32_t>(cur_w) != uiW || static_cast<uint32_t>(cur_h) != uiH) return false;

                    size_t len  = uiW * uiH * 4;
                    float* fBuf = (float*)rcv.read_buffer(len * sizeof(float));

                    if(type == PASS_COMBINED) {
                        for(register size_t i=0; i<len; i+=4)
                            srgb_to_linearrgb_v4(&float_img_buf[i], &fBuf[i]);
                    }
                    else memcpy(float_img_buf, fBuf, len * sizeof(float));
                }
                break;
            }
            else return false;
        }
        return true;
    } //get_image_buffer()

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Get Image-buffer of given pass
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    inline bool get_pass_rect(PassType type, int components, float *pixels, int w, int h) {
        if(w <= 0) w = 4;
        if(h <= 0) h = 4;
        thread_scoped_lock img_buf_lock(img_buf_mutex);
        if(!float_img_buf || cur_w != w || cur_h != h) {
            if(float_img_buf) delete[] float_img_buf;
            size_t len = w * h * 4;
            float_img_buf = new float[len];

            cur_w = w;
            cur_h = h;
            memset(float_img_buf, 0, len * sizeof(float));
            for(int i=0; i<len; ++i) float_img_buf[i] = 0.0f;
            memset(pixels, 0, w * h * components * sizeof(float));
            return false;
        }

        size_t pixel_size = cur_w * cur_h;
        float* in = float_img_buf;

        if(components == 1) {
	        for(int i = 0; i < pixel_size; i++, in += 4, pixels++)
		        *pixels = *in;
	    } //if(components == 1)
	    else if(components == 3) {
		    for(int i = 0; i < pixel_size; i++, in += 4, pixels += 3) {
			    pixels[0] = in[0];
			    pixels[1] = in[1];
			    pixels[2] = in[2];
		    }
	    } //else if(components == 3)
	    else if(components == 4) {
            memcpy(pixels, float_img_buf, pixel_size * sizeof(float) * 4);
	    } //else if(components == 4)
	    return true;
    } //get_pass_rect()


	static RenderServer*        create(RenderServerInfo& info, bool interactive = false);
    static RenderServerInfo&    get_info(void);
}; //RenderServer

OCT_NAMESPACE_END

#endif /* __SERVER_H__ */

