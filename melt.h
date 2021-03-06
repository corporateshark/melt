//
// The MIT License (MIT)
//
// Copyright (c) 2019 Karim Naaji, karim.naaji@gmail.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// This library generates conservative occluders from triangle meshes.
//
// How to use:
//  In a single compilation unit define the following and include the library:
//  #define MELT_IMPLEMENTATION
//  #include melt.h
//
// A full description of the algorithm is available at:
//  http://karim.naaji.fr/blog/2019/15.11.19.html
//
// Triangle box intersection algorithm by Tomas Akenine-Möller[1].
//
// References:
//  [1] https://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/code/tribox_tam.pdf
//  [2] https://research.nvidia.com/publication/occluder-simplification-using-planar-sections
//  [3] http://fileadmin.cs.lth.se/graphics/research/papers/2005/cr/_conservative.pdf
//

#ifndef MELT_H
#define MELT_H

#include <stdint.h>

typedef struct
{
    float x;
    float y;
    float z;
} melt_vec3_t;

typedef struct
{
    melt_vec3_t* vertices;
    uint16_t* indices;
    uint32_t vertex_count;
    uint32_t index_count;
}  melt_mesh_t;

typedef enum melt_occluder_box_type_t
{
    MELT_OCCLUDER_BOX_TYPE_NONE      = 0,
    MELT_OCCLUDER_BOX_TYPE_DIAGONALS = 1 << 0,
    MELT_OCCLUDER_BOX_TYPE_TOP       = 1 << 1,
    MELT_OCCLUDER_BOX_TYPE_BOTTOM    = 1 << 2,
    MELT_OCCLUDER_BOX_TYPE_SIDES     = 1 << 3,
    MELT_OCCLUDER_BOX_TYPE_REGULAR   = MELT_OCCLUDER_BOX_TYPE_SIDES | MELT_OCCLUDER_BOX_TYPE_TOP | MELT_OCCLUDER_BOX_TYPE_BOTTOM
} melt_occluder_box_type_t;

typedef int32_t melt_occluder_box_type_flags_t;

typedef enum melt_debug_type_t
{
    MELT_DEBUG_TYPE_SHOW_INNER           = 1 << 0,
    MELT_DEBUG_TYPE_SHOW_EXTENT          = 1 << 1,
    MELT_DEBUG_TYPE_SHOW_RESULT          = 1 << 2,
    MELT_DEBUG_TYPE_SHOW_OUTER           = 1 << 3,
    MELT_DEBUG_TYPE_SHOW_MIN_DISTANCE    = 1 << 4,
    MELT_DEBUG_TYPE_SHOW_SLICE_SELECTION = 1 << 5
} melt_debug_type_t;

typedef int32_t melt_debug_type_flags_t;

typedef struct
{
    melt_debug_type_flags_t flags;
    int32_t voxel_x;
    int32_t voxel_y;
    int32_t voxel_z;
    int32_t extent_index;
    float voxelScale;
} melt_debug_params_t;

typedef struct
{
    uint32_t _start_canary;
    melt_mesh_t mesh;
    melt_occluder_box_type_flags_t box_type_flags;
    melt_debug_params_t debug;
    float voxel_size;
    float fill_pct;
    uint32_t _end_canary;
} melt_params_t;

typedef struct
{
    melt_mesh_t mesh;
    melt_mesh_t debug_mesh;
} melt_result_t;

int melt_generate_occluder(melt_params_t params, melt_result_t* result);

void melt_free_result(melt_result_t result);

#ifndef MELT_ASSERT
#define MELT_ASSERT(stmt) (void)(stmt)
#endif
#ifndef MELT_PROFILE_BEGIN
#define MELT_PROFILE_BEGIN()
#endif
#ifndef MELT_PROFILE_END
#define MELT_PROFILE_END()
#endif
#ifndef MELT_MALLOC
#include <stdlib.h>
#define MELT_MALLOC(T, N) (T*)malloc(N * sizeof(T))
#define MELT_FREE(T) free(T)
#endif

#endif // MELT_H

#ifdef MELT_IMPLEMENTATION

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4244) // 'unsigned int' to 'float', possible loss of data
#pragma warning(disable:4201) // nonstandard extension used: nameless struct/union
#endif // _MSC_VER

#ifndef _MSC_VER
#include <alloca.h>
#define MELT_ALLOCA(T, N) (T*)alloca(N * sizeof(T))
#else // _MSC_VER
#include <malloc.h>
#pragma warning(disable:6255)
#define MELT_ALLOCA(T, N) (T*)_alloca(N * sizeof(T))
#endif // !_MSC_VER

#include <math.h>    // fabsf
#include <float.h>   // FLT_MAX
#include <limits.h>  // INT_MAX
#include <string.h>  // memset
#include <stdbool.h> // bool

#define MELT_ARRAY_LENGTH(array) ((int)(sizeof(array) / sizeof(*array)))
#define MELT_UNUSED(value) (void)value

typedef melt_vec3_t vec3_t;
typedef struct
{
    float x, y;
} vec2_t;

typedef struct
{
    int32_t x, y, z;
} svec3_t;

typedef struct
{
    uint32_t x, y;
} uvec2_t;

typedef struct
{
    uint32_t x, y, z;
} uvec3_t;

typedef uvec3_t color_3u8_t;

typedef struct
{
    vec3_t min;
    vec3_t max;
} _aabb_t;

typedef struct
{
    _aabb_t aabb;
    uvec3_t position;
} _voxel_t;

typedef struct
{
    vec3_t v0;
    vec3_t v1;
    vec3_t v2;
} _triangle_t;

typedef struct
{
    vec3_t normal;
    float distance;
} _plane_t;

typedef struct
{
    svec3_t dist;
    union
    {
        struct
        {
            uint32_t x;
            uint32_t y;
            uint32_t z;
        };
        uvec3_t position;
    };
} _min_distance_t;

typedef struct
{
    uint8_t visibility : 6;
    uint8_t clipped : 1;
    uint8_t inner : 1;
} _voxel_status_t;

typedef enum _visibility_t
{
    MELT_AXIS_VISIBILITY_NULL    =      0,
    MELT_AXIS_VISIBILITY_PLUS_X  = 1 << 0,
    MELT_AXIS_VISIBILITY_MINUS_X = 1 << 1,
    MELT_AXIS_VISIBILITY_PLUS_Y  = 1 << 2,
    MELT_AXIS_VISIBILITY_MINUS_Y = 1 << 3,
    MELT_AXIS_VISIBILITY_PLUS_Z  = 1 << 4,
    MELT_AXIS_VISIBILITY_MINUS_Z = 1 << 5,
    MELT_AXIS_VISIBILITY_ALL     =   0x3f
} _visibility_t;

typedef struct
{
    uvec3_t position;
    uvec3_t extent;
    uint32_t volume;
} _max_extent_t;

typedef struct
{
    uint32_t element_count;
    uint32_t element_byte_size;
    void* data;
} _array_t;

typedef struct
{
    _voxel_t* voxels;
    uint32_t voxel_count;
} _voxel_set_plane_t;

typedef struct
{
    _voxel_set_plane_t* x;
    _voxel_set_plane_t* y;
    _voxel_set_plane_t* z;

    uint32_t x_count;
    uint32_t y_count;
    uint32_t z_count;
} _voxel_set_planes_t;

typedef struct
{
    uvec3_t dimension;
    uint32_t size;

    int32_t* voxel_indices;
    _voxel_status_t* voxel_field;
    _min_distance_t* min_distance_field;

    _voxel_t* voxel_set;
    uint32_t voxel_set_count;

    _voxel_set_planes_t voxel_set_planes;

    _max_extent_t* max_extents;
    uint32_t max_extents_count;
} _context_t;

static const color_3u8_t _color_null = { 0, 0, 0 };

#ifdef MELT_DEBUG
static const color_3u8_t _color_steel_blue = { 70, 130, 180 };
static const color_3u8_t _colors[] =
{
    { 245, 245, 245 },
    {  70, 130, 180 },
    {   0, 255, 127 },
    {   0, 128, 128 },
    { 255, 182, 193 },
    { 176, 224, 230 },
    { 119, 136, 153 },
    { 143, 188, 143 },
    { 255, 250, 240 },
};
#endif

static const uint16_t _voxel_cube_indices[36] =
{
    0, 1, 2,
    0, 2, 3,
    3, 2, 6,
    3, 6, 7,
    0, 7, 4,
    0, 3, 7,
    4, 7, 5,
    7, 6, 5,
    0, 4, 5,
    0, 5, 1,
    1, 5, 6,
    1, 6, 2,
};

static const uint16_t _voxel_cube_indices_sides[24] =
{
    0, 1, 2,
    0, 2, 3,
    3, 2, 6,
    3, 6, 7,
    4, 7, 5,
    7, 6, 5,
    0, 4, 5,
    0, 5, 1,
};

static const uint16_t _voxel_cube_indices_diagonals[12] =
{
    0, 1, 6,
    0, 6, 7,
    4, 5, 2,
    4, 2, 3,
};

static const uint16_t _voxel_cube_indices_bottom[6] =
{
    1, 5, 6,
    1, 6, 2,
};

static const uint16_t _voxel_cube_indices_top[6] =
{
    0, 7, 4,
    0, 3, 7,
};

static const vec3_t _voxel_cube_vertices[8] =
{
    {-1.0f,  1.0f,  1.0f},
    {-1.0f, -1.0f,  1.0f},
    { 1.0f, -1.0f,  1.0f},
    { 1.0f,  1.0f,  1.0f},
    {-1.0f,  1.0f, -1.0f},
    {-1.0f, -1.0f, -1.0f},
    { 1.0f, -1.0f, -1.0f},
    { 1.0f,  1.0f, -1.0f},
};

static vec3_t _vec3_init(float x, float y, float z)
{
    vec3_t v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

static vec3_t _uvec3_to_vec3(uvec3_t v)
{
    vec3_t out;
    out.x = (float)v.x;
    out.y = (float)v.y;
    out.z = (float)v.z;
    return out;
}

static uvec3_t _vec3_to_uvev3(vec3_t v)
{
    uvec3_t out;
    out.x = (uint32_t)v.x;
    out.y = (uint32_t)v.y;
    out.z = (uint32_t)v.z;
    return out;
}

static uvec3_t _uvec3_init(float x, float y, float z)
{
    uvec3_t v;
    v.x = (uint32_t)x;
    v.y = (uint32_t)y;
    v.z = (uint32_t)z;
    return v;
}

static vec3_t _vec3_mulf(vec3_t v, float factor)
{
    return _vec3_init(v.x * factor, v.y * factor, v.z * factor);
}

static vec3_t _vec3_mul(vec3_t a, vec3_t b)
{
    return _vec3_init(a.x * b.x, a.y * b.y, a.z * b.z);
}

static vec3_t _vec3_div(vec3_t v, float divisor)
{
    return _vec3_mulf(v, 1.0f / divisor);
}

static vec3_t _vec3_sub(vec3_t a, vec3_t b)
{
    return _vec3_init(a.x - b.x, a.y - b.y, a.z - b.z);
}

static vec3_t _vec3_add(vec3_t a, vec3_t b)
{
    return _vec3_init(a.x + b.x, a.y + b.y, a.z + b.z);
}

static vec3_t _vec3_abs(vec3_t v)
{
    return _vec3_init(fabsf(v.x), fabsf(v.y), fabsf(v.z));
}

static vec3_t _vec3_cross(vec3_t a, vec3_t b)
{
    float x = a.y * b.z - a.z * b.y;
    float y = a.z * b.x - a.x * b.z;
    float z = a.x * b.y - a.y * b.x;
    return _vec3_init(x, y, z);
}

static float _vec3_dot(vec3_t a, vec3_t b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static uvec2_t _uvec2_init(uint32_t x, uint32_t y)
{
    uvec2_t v;
    v.x = x;
    v.y = y;
    return v;
}

static svec3_t _svec3_init(int32_t x, int32_t y, int32_t z)
{
    svec3_t v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

static int _svec3_equals(svec3_t a, svec3_t b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

static int _uvec3_equals(uvec3_t a, uvec3_t b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

static float _float_min(float a, float b) {
    return a < b ? a : b;
}

static float _float_max(float a, float b) {
    return a > b ? a : b;
}

static uint32_t _uint32_t_min(uint32_t a, uint32_t b)
{
    return a < b ? a : b;
}

static vec3_t _vec3_min(vec3_t a, vec3_t b)
{
    float x = _float_min(a.x, b.x);
    float y = _float_min(a.y, b.y);
    float z = _float_min(a.z, b.z);
    return _vec3_init(x, y, z);
}

static vec3_t _vec3_max(vec3_t a, vec3_t b)
{
    float x = _float_max(a.x, b.x);
    float y = _float_max(a.y, b.y);
    float z = _float_max(a.z, b.z);
    return _vec3_init(x, y, z);
}

static vec3_t _aabb_center(_aabb_t aabb)
{
    return _vec3_mulf(_vec3_add(aabb.min, aabb.max), 0.5f);
}

static bool _aabb_intersects_plane(const _plane_t plane, vec3_t half_aabb_dim)
{
    vec3_t vmin;
    vec3_t vmax;

    if (plane.normal.x > 0.0f)
    {
        vmin.x = -half_aabb_dim.x;
        vmax.x =  half_aabb_dim.x;
    }
    else
    {
        vmin.x =  half_aabb_dim.x;
        vmax.x = -half_aabb_dim.x;
    }

    if (plane.normal.y > 0.0f)
    {
        vmin.y = -half_aabb_dim.y;
        vmax.y =  half_aabb_dim.y;
    }
    else
    {
        vmin.y =  half_aabb_dim.y;
        vmax.y = -half_aabb_dim.y;
    }

    if (plane.normal.z > 0.0f)
    {
        vmin.z = -half_aabb_dim.z;
        vmax.z =  half_aabb_dim.z;
    }
    else
    {
        vmin.z =  half_aabb_dim.z;
        vmax.z = -half_aabb_dim.z;
    }

    if (_vec3_dot(plane.normal, vmin) + plane.distance > 0.0f)
        return false;

    if (_vec3_dot(plane.normal, vmax) + plane.distance >= 0.0f)
        return true;

    return false;
}

#define AXISTEST_X01(a, b, fa, fb)                 \
p0 = a * v0.y - b * v0.z;                          \
p2 = a * v2.y - b * v2.z;                          \
if (p0 < p2) {                                     \
    min = p0; max = p2;                            \
} else {                                           \
    min = p2; max = p0;                            \
}                                                  \
rad = fa * half_aabb_dim.y + fb * half_aabb_dim.z; \
if (min > rad || max < -rad) {                     \
    return false;                                  \
}                                                  \

#define AXISTEST_X2(a, b, fa, fb)                  \
p0 = a * v0.y - b * v0.z;                          \
p1 = a * v1.y - b * v1.z;                          \
if (p0 < p1) {                                     \
    min = p0; max = p1;                            \
} else {                                           \
    min = p1; max = p0;                            \
}                                                  \
rad = fa * half_aabb_dim.y + fb * half_aabb_dim.z; \
if (min > rad || max < -rad) {                     \
    return false;                                  \
}                                                  \

#define AXISTEST_Y02(a, b, fa, fb)                 \
p0 = -a * v0.x + b * v0.z;                         \
p2 = -a * v2.x + b * v2.z;                         \
if (p0 < p2) {                                     \
    min = p0; max = p2;                            \
} else {                                           \
    min = p2; max = p0;                            \
}                                                  \
rad = fa * half_aabb_dim.x + fb * half_aabb_dim.z; \
if (min > rad || max < -rad) {                     \
    return false;                                  \
}                                                  \

#define AXISTEST_Y1(a, b, fa, fb)                  \
p0 = -a * v0.x + b * v0.z;                         \
p1 = -a * v1.x + b * v1.z;                         \
if (p0 < p1) {                                     \
    min = p0; max = p1;                            \
} else {                                           \
    min = p1; max = p0;                            \
}                                                  \
rad = fa * half_aabb_dim.x + fb * half_aabb_dim.z; \
if (min > rad || max < -rad) {                     \
    return false;                                  \
}

#define AXISTEST_Z12(a, b, fa, fb)                 \
p1 = a * v1.x - b * v1.y;                          \
p2 = a * v2.x - b * v2.y;                          \
if (p2 < p1) {                                     \
    min = p2; max = p1;                            \
} else {                                           \
    min = p1; max = p2;                            \
}                                                  \
rad = fa * half_aabb_dim.x + fb * half_aabb_dim.y; \
if (min > rad || max < -rad) {                     \
    return false;                                  \
}

#define AXISTEST_Z0(a, b, fa, fb)                  \
p0 = a * v0.x - b * v0.y;                          \
p1 = a * v1.x - b * v1.y;                          \
if (p0 < p1) {                                     \
    min = p0; max = p1;                            \
} else {                                           \
    min = p1; max = p0;                            \
}                                                  \
rad = fa * half_aabb_dim.x + fb * half_aabb_dim.y; \
if (min > rad || max < -rad) {                     \
    return false;                                  \
}

#define FINDMINMAX(x0, x1, x2, min, max)           \
min = max = x0;                                    \
if (x1 < min) min = x1;                            \
if (x1 > max) max = x1;                            \
if (x2 < min) min = x2;                            \
if (x2 > max) max = x2;

static bool _aabb_intersects_triangle(const _triangle_t* triangle, vec3_t aabb_center, vec3_t half_aabb_dim)
{
    vec3_t v0, v1, v2;
    vec3_t e0, e1, e2;
    vec3_t edge_abs;
    float min, max;
    float p0, p1, p2;
    float rad;

    v0 = _vec3_sub(triangle->v0, aabb_center);
    v1 = _vec3_sub(triangle->v1, aabb_center);
    v2 = _vec3_sub(triangle->v2, aabb_center);

    e0 = _vec3_sub(v1, v0);
    e1 = _vec3_sub(v2, v1);
    e2 = _vec3_sub(v0, v2);

    edge_abs = _vec3_abs(e0);
    AXISTEST_X01(e0.z, e0.y, edge_abs.z, edge_abs.y);
    AXISTEST_Y02(e0.z, e0.x, edge_abs.z, edge_abs.x);
    AXISTEST_Z12(e0.y, e0.x, edge_abs.y, edge_abs.x);

    edge_abs = _vec3_abs(e1);
    AXISTEST_X01(e1.z, e1.y, edge_abs.z, edge_abs.y);
    AXISTEST_Y02(e1.z, e1.x, edge_abs.z, edge_abs.x);
    AXISTEST_Z0 (e1.y, e1.x, edge_abs.y, edge_abs.x);

    edge_abs = _vec3_abs(e2);
    AXISTEST_X2 (e2.z, e2.y, edge_abs.z, edge_abs.y);
    AXISTEST_Y1 (e2.z, e2.x, edge_abs.z, edge_abs.x);
    AXISTEST_Z12(e2.y, e2.x, edge_abs.y, edge_abs.x);

    FINDMINMAX(v0.x, v1.x, v2.x, min, max);
    if (min > half_aabb_dim.x || max < -half_aabb_dim.x)
        return false;

    FINDMINMAX(v0.y, v1.y, v2.y, min, max);
    if (min > half_aabb_dim.y || max < -half_aabb_dim.y)
        return false;

    FINDMINMAX(v0.z, v1.z, v2.z, min, max);
    if (min > half_aabb_dim.z || max < -half_aabb_dim.z)
        return false;

    _plane_t plane;
    plane.normal = _vec3_cross(e0, e1);
    plane.distance = -_vec3_dot(plane.normal, v0);

    if (!_aabb_intersects_plane(plane, half_aabb_dim))
        return false;

    return true;
}

static inline uint32_t _flatten_3d(uvec3_t index, uvec3_t dimension)
{
    uint32_t out_index = index.x + dimension.x * index.y + dimension.x * dimension.y * index.z;
    MELT_ASSERT(out_index < dimension.x * dimension.y * dimension.z);
    return out_index;
}

static inline uint32_t _flatten_2d(uvec2_t index, uvec2_t dimension)
{
    uint32_t out_index = index.x + dimension.x * index.y;
    MELT_ASSERT(out_index < dimension.x * dimension.y);
    return out_index;
}

static inline uvec3_t _unflatten_3d(uint32_t position, uvec3_t dimension)
{
    uvec3_t out_index;

    uint32_t dim_xy = dimension.x * dimension.y;
    out_index.z = position / dim_xy;
    position -= out_index.z * dim_xy;
    out_index.y = position / dimension.x;
    out_index.x = position % dimension.x;

    MELT_ASSERT(out_index.x < dimension.x);
    MELT_ASSERT(out_index.y < dimension.y);
    MELT_ASSERT(out_index.z < dimension.z);

    return out_index;
}

static float _map_to_voxel_max_func(float value, float voxel_size)
{
    float sign = value < 0.0f ? -1.0f : 1.0f;
    float result = value + sign * voxel_size * 0.5f;
    return ceilf(result / voxel_size) * voxel_size;
}

static vec3_t _map_to_voxel_max_bound(vec3_t position, float voxel_size)
{
    float x = _map_to_voxel_max_func(position.x, voxel_size);
    float y = _map_to_voxel_max_func(position.y, voxel_size);
    float z = _map_to_voxel_max_func(position.z, voxel_size);

    return _vec3_init(x, y, z);
}

static float _map_to_voxel_min_func(float value, float voxel_size)
{
    float sign = value < 0.0f ? -1.0f : 1.0f;
    float result = value + sign * voxel_size * 0.5f;
    return floorf(result / voxel_size) * voxel_size;
}

static vec3_t _map_to_voxel_min_bound(vec3_t position, float voxel_size)
{
    float x = _map_to_voxel_min_func(position.x, voxel_size);
    float y = _map_to_voxel_min_func(position.y, voxel_size);
    float z = _map_to_voxel_min_func(position.z, voxel_size);

    return _vec3_init(x, y, z);
}

static _aabb_t _generate_aabb_from_triangle(const _triangle_t* triangle)
{
    _aabb_t aabb;

    aabb.min = _vec3_init( FLT_MAX,  FLT_MAX,  FLT_MAX);
    aabb.max = _vec3_init(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    aabb.min = _vec3_min(aabb.min, triangle->v0);
    aabb.max = _vec3_max(aabb.max, triangle->v0);

    aabb.min = _vec3_min(aabb.min, triangle->v1);
    aabb.max = _vec3_max(aabb.max, triangle->v1);

    aabb.min = _vec3_min(aabb.min, triangle->v2);
    aabb.max = _vec3_max(aabb.max, triangle->v2);

    return aabb;
}

static _aabb_t _generate_aabb_from_mesh(const melt_mesh_t mesh)
{
    _aabb_t aabb;

    aabb.min = _vec3_init( FLT_MAX,  FLT_MAX,  FLT_MAX);
    aabb.max = _vec3_init(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (uint32_t i = 0; i < mesh.index_count; ++i)
    {
        aabb.min = _vec3_min(aabb.min, mesh.vertices[mesh.indices[i]]);
        aabb.max = _vec3_max(aabb.max, mesh.vertices[mesh.indices[i]]);
    }

    return aabb;
}

static void _free_per_plane_voxel_set(_context_t* context)
{
    for (uint32_t i = 0; i < context->voxel_set_planes.x_count; ++i)
        MELT_FREE(context->voxel_set_planes.x[i].voxels);
    for (uint32_t i = 0; i < context->voxel_set_planes.y_count; ++i)
        MELT_FREE(context->voxel_set_planes.y[i].voxels);
    for (uint32_t i = 0; i < context->voxel_set_planes.z_count; ++i)
        MELT_FREE(context->voxel_set_planes.z[i].voxels);

    MELT_FREE(context->voxel_set_planes.x);
    MELT_FREE(context->voxel_set_planes.y);
    MELT_FREE(context->voxel_set_planes.z);
}

static void _generate_per_plane_voxel_set(_context_t* context)
{
    MELT_PROFILE_BEGIN();

    context->voxel_set_planes.x_count = context->dimension.y * context->dimension.z;
    context->voxel_set_planes.y_count = context->dimension.x * context->dimension.z;
    context->voxel_set_planes.z_count = context->dimension.x * context->dimension.y;

    context->voxel_set_planes.x = MELT_MALLOC(_voxel_set_plane_t, context->voxel_set_planes.x_count);
    context->voxel_set_planes.y = MELT_MALLOC(_voxel_set_plane_t, context->voxel_set_planes.y_count);
    context->voxel_set_planes.z = MELT_MALLOC(_voxel_set_plane_t, context->voxel_set_planes.z_count);

    for (uint32_t i = 0; i < context->voxel_set_planes.x_count; ++i)
    {
        context->voxel_set_planes.x[i].voxels = MELT_MALLOC(_voxel_t, context->dimension.x);
        context->voxel_set_planes.x[i].voxel_count = 0;
    }
    for (uint32_t i = 0; i < context->voxel_set_planes.y_count; ++i)
    {
        context->voxel_set_planes.y[i].voxels = MELT_MALLOC(_voxel_t, context->dimension.y);
        context->voxel_set_planes.y[i].voxel_count = 0;
    }
    for (uint32_t i = 0; i < context->voxel_set_planes.z_count; ++i)
    {
        context->voxel_set_planes.z[i].voxels = MELT_MALLOC(_voxel_t, context->dimension.z);
        context->voxel_set_planes.z[i].voxel_count = 0;
    }

    uvec2_t dim_yz = _uvec2_init(context->dimension.y, context->dimension.z);
    uvec2_t dim_xz = _uvec2_init(context->dimension.x, context->dimension.z);
    uvec2_t dim_xy = _uvec2_init(context->dimension.x, context->dimension.y);

    for (uint32_t x = 0; x < context->dimension.x; ++x)
    {
        for (uint32_t y = 0; y < context->dimension.y; ++y)
        {
            for (uint32_t z = 0; z < context->dimension.z; ++z)
            {
                uvec3_t position = _uvec3_init(x, y, z);
                int32_t voxel_index = context->voxel_indices[_flatten_3d(position, context->dimension)];
                if (voxel_index != -1)
                {
                    uint32_t index_yz = _flatten_2d(_uvec2_init(y, z), dim_yz);
                    uint32_t index_xz = _flatten_2d(_uvec2_init(x, z), dim_xz);
                    uint32_t index_xy = _flatten_2d(_uvec2_init(x, y), dim_xy);

                    _voxel_set_plane_t* voxels_x_planes = &context->voxel_set_planes.x[index_yz];
                    _voxel_set_plane_t* voxels_y_planes = &context->voxel_set_planes.y[index_xz];
                    _voxel_set_plane_t* voxels_z_planes = &context->voxel_set_planes.z[index_xy];

                    voxels_x_planes->voxels[voxels_x_planes->voxel_count++] = context->voxel_set[voxel_index];
                    voxels_y_planes->voxels[voxels_y_planes->voxel_count++] = context->voxel_set[voxel_index];
                    voxels_z_planes->voxels[voxels_z_planes->voxel_count++] = context->voxel_set[voxel_index];
                }
            }
        }
    }

    MELT_PROFILE_END();
}

static void _get_field(_context_t* context, uint32_t x, uint32_t y, uint32_t z, _min_distance_t* out_min_distance, _voxel_status_t* out_status)
{
    const svec3_t InfiniteDistance = _svec3_init(INT_MAX, INT_MAX, INT_MAX);
    const svec3_t NullDistance = _svec3_init(0, 0, 0);

    out_min_distance->dist = InfiniteDistance;
    out_min_distance->x = x;
    out_min_distance->y = y;
    out_min_distance->z = z;

    out_status->visibility = MELT_AXIS_VISIBILITY_NULL;
    out_status->clipped = false;
    out_status->inner = false;

    uvec2_t dim_yz = _uvec2_init(context->dimension.y, context->dimension.z);
    uint32_t index_yz = _flatten_2d(_uvec2_init(y, z), dim_yz);
    const _voxel_set_plane_t* voxels_x_plane = &context->voxel_set_planes.x[index_yz];
    for (uint32_t i = 0; i < voxels_x_plane->voxel_count; ++i)
    {
        const _voxel_t* voxel = &voxels_x_plane->voxels[i];
        int32_t distance = voxel->position.x - x;
        if (distance > 0)
        {
            out_status->visibility |= MELT_AXIS_VISIBILITY_PLUS_X;
            out_min_distance->dist.x = _uint32_t_min(out_min_distance->dist.x, distance);
        }
        else if (distance < 0)
            out_status->visibility |= MELT_AXIS_VISIBILITY_MINUS_X;
        else
            out_min_distance->dist.x = 0;
    }
    uvec2_t dim_xz = _uvec2_init(context->dimension.x, context->dimension.z);
    uint32_t index_xz = _flatten_2d(_uvec2_init(x, z), dim_xz);
    const _voxel_set_plane_t* voxels_y_plane = &context->voxel_set_planes.y[index_xz];
    for (uint32_t i = 0; i < voxels_y_plane->voxel_count; ++i)
    {
        const _voxel_t* voxel = &voxels_y_plane->voxels[i];
        int32_t distance = voxel->position.y - y;
        if (distance > 0)
        {
            out_status->visibility |= MELT_AXIS_VISIBILITY_PLUS_Y;
            out_min_distance->dist.y = _uint32_t_min(out_min_distance->dist.y, distance);
        }
        else if (distance < 0)
            out_status->visibility |= MELT_AXIS_VISIBILITY_MINUS_Y;
        else
            out_min_distance->dist.y = 0;
    }
    uvec2_t dim_xy = _uvec2_init(context->dimension.x, context->dimension.y);
    uint32_t index_xy = _flatten_2d(_uvec2_init(x, y), dim_xy);
    const _voxel_set_plane_t* voxels_z_plane = &context->voxel_set_planes.z[index_xy];
    for (uint32_t i = 0; i < voxels_z_plane->voxel_count; ++i)
    {
        const _voxel_t* voxel = &voxels_z_plane->voxels[i];
        int32_t distance = voxel->position.z - z;
        if (distance > 0)
        {
            out_status->visibility |= MELT_AXIS_VISIBILITY_PLUS_Z;
            out_min_distance->dist.z = _uint32_t_min(out_min_distance->dist.z, distance);
        }
        else if (distance < 0)
            out_status->visibility |= MELT_AXIS_VISIBILITY_MINUS_Z;
        else
            out_min_distance->dist.z = 0;
    }
    if (out_status->visibility == MELT_AXIS_VISIBILITY_ALL)
    {
        if (!_svec3_equals(out_min_distance->dist, InfiniteDistance) &&
            !_svec3_equals(out_min_distance->dist, NullDistance))
        {
            out_status->inner = true;
        }
    }
}

static void _generate_fields(_context_t* context)
{
    MELT_PROFILE_BEGIN();

    for (uint32_t i = 0; i < context->size; ++i)
    {
        _min_distance_t* min_distance = &context->min_distance_field[i];
        _voxel_status_t* voxel_status = &context->voxel_field[i];
        const uvec3_t position = _unflatten_3d(i, context->dimension);
        _get_field(context, position.x, position.y, position.z, min_distance, voxel_status);
    }

    MELT_PROFILE_END();
}

static inline bool _inner_voxel(_voxel_status_t voxel_status)
{
    return voxel_status.inner && !voxel_status.clipped;
}

static uvec3_t _get_max_aabb_extent(const _context_t* context, const _min_distance_t* min_distance)
{
    MELT_PROFILE_BEGIN();

    uvec2_t* max_aabb_extents = MELT_ALLOCA(uvec2_t, min_distance->z + min_distance->dist.z);
    uint32_t max_aabb_extents_count = 0;

    for (uint32_t z = min_distance->z; z < min_distance->z + min_distance->dist.z; ++z)
    {
        uvec3_t z_slice_position = _uvec3_init(min_distance->x, min_distance->y, z);
        uint32_t z_slice_index = _flatten_3d(z_slice_position, context->dimension);

        MELT_ASSERT(context->voxel_field[z_slice_index].inner);

        if (context->voxel_field[z_slice_index].clipped)
            continue;

        const _min_distance_t* sample_min_distance = &context->min_distance_field[z_slice_index];

        uvec2_t max_extent = _uvec2_init(sample_min_distance->dist.x, sample_min_distance->dist.y);

        uint32_t x = sample_min_distance->x + 1;
        uint32_t y = sample_min_distance->y + 1;
        uint32_t i = 1;
        while (x < sample_min_distance->x + sample_min_distance->dist.x &&
               y < sample_min_distance->y + sample_min_distance->dist.y)
        {
            const uint32_t index = _flatten_3d(_uvec3_init(x, y, z), context->dimension);
            if (_inner_voxel(context->voxel_field[index]))
            {
                const _min_distance_t* distance = &context->min_distance_field[index];
                max_extent.x = _uint32_t_min(distance->dist.x + i, max_extent.x);
                max_extent.y = _uint32_t_min(distance->dist.y + i, max_extent.y);
            }
            else
            {
                max_extent.x = i;
                max_extent.y = i;
                break;
            }
            ++x;
            ++y;
            ++i;
        }

        max_aabb_extents[max_aabb_extents_count++] = max_extent;
    }

    uvec2_t min_extent = _uvec2_init(UINT_MAX, UINT_MAX);

    uint32_t z_slice = 1;
    uint32_t max_volume = 0;

    MELT_ASSERT(max_aabb_extents_count > 0);

    for (uint32_t i = 0; i < max_aabb_extents_count; ++i)
    {
        const uvec2_t* extent = &max_aabb_extents[i];
        min_extent.x = _uint32_t_min(extent->x, min_extent.x);
        min_extent.y = _uint32_t_min(extent->y, min_extent.y);

        const uint32_t volume = min_extent.x * min_extent.y * z_slice;
        if (volume > max_volume)
            max_volume = volume;

        ++z_slice;
    }

    MELT_ASSERT(max_volume > 0);
    MELT_ASSERT(min_extent.x > 0);
    MELT_ASSERT(min_extent.y > 0);
    MELT_ASSERT(z_slice > 1);
    MELT_PROFILE_END();

    return _uvec3_init(min_extent.x, min_extent.y, z_slice - 1);
}

static void _clip_voxel_field(const _context_t* context, const uvec3_t start_position, const uvec3_t extent)
{
    MELT_PROFILE_BEGIN();

    for (uint32_t x = start_position.x; x < start_position.x + extent.x; ++x)
    {
        for (uint32_t y = start_position.y; y < start_position.y + extent.y; ++y)
        {
            for (uint32_t z = start_position.z; z < start_position.z + extent.z; ++z)
            {
                uint32_t index = _flatten_3d(_uvec3_init(x, y, z), context->dimension);
                MELT_ASSERT(!context->voxel_field[index].clipped && "Clipping already clipped voxel field index");
                context->voxel_field[index].clipped = true;
            }
        }
    }

    MELT_PROFILE_END();
}

static bool _water_tight_mesh(const _context_t* context)
{
    for (uint32_t i = 0; i < context->size; ++i)
    {
        const _min_distance_t* min_distance = &context->min_distance_field[i];
        if (!_inner_voxel(context->voxel_field[_flatten_3d(min_distance->position, context->dimension)]))
            continue;

        for (uint32_t x = min_distance->x; x < min_distance->x + min_distance->dist.x; ++x)
        {
            const uint32_t y = min_distance->y;
            const uint32_t z = min_distance->z;
            const uint32_t index = _flatten_3d(_uvec3_init(x, y, z), context->dimension);
            if (!_inner_voxel(context->voxel_field[index]))
            {
                return false;
            }
        }
        for (uint32_t y = min_distance->y; y < min_distance->y + min_distance->dist.y; ++y)
        {
            const uint32_t x = min_distance->x;
            const uint32_t z = min_distance->z;
            const uint32_t index = _flatten_3d(_uvec3_init(x, y, z), context->dimension);
            if (!_inner_voxel(context->voxel_field[index]))
            {
                return false;
            }
        }
        for (uint32_t z = min_distance->z; z < min_distance->z + min_distance->dist.z; ++z)
        {
            const uint32_t x = min_distance->x;
            const uint32_t y = min_distance->y;
            const uint32_t index = _flatten_3d(_uvec3_init(x, y, z), context->dimension);
            if (!_inner_voxel(context->voxel_field[index]))
            {
                return false;
            }
        }
    }

    return true;
}

static void _debug_validate_min_distance_field(const _context_t* context)
{
#if defined(MELT_DEBUG) && defined(MELT_ASSERT)
    for (uint32_t i = 0; i < context->size; ++i)
    {
        const _min_distance_t* min_distance = &context->min_distance_field[i];
        if (!_inner_voxel(context->voxel_field[_flatten_3d(min_distance->position, context->dimension)]))
            continue;

        for (uint32_t x = min_distance->x; x < min_distance->x + min_distance->dist.x; ++x)
        {
            const uint32_t y = min_distance->y;
            const uint32_t z = min_distance->z;
            const uint32_t index = _flatten_3d(_uvec3_init(x, y, z), context->dimension);
            for (uint32_t i = 0; i < context->voxel_set_count; ++i)
                MELT_ASSERT(!_uvec3_equals(context->voxel_set[i].position, _uvec3_init(x, y, z)));
            MELT_ASSERT(_inner_voxel(context->voxel_field[index]));
        }
        for (uint32_t y = min_distance->y; y < min_distance->y + min_distance->dist.y; ++y)
        {
            const uint32_t x = min_distance->x;
            const uint32_t z = min_distance->z;
            const uint32_t index = _flatten_3d(_uvec3_init(x, y, z), context->dimension);
            for (uint32_t i = 0; i < context->voxel_set_count; ++i)
                MELT_ASSERT(!_uvec3_equals(context->voxel_set[i].position, _uvec3_init(x, y, z)));
            MELT_ASSERT(_inner_voxel(context->voxel_field[index]));
        }
        for (uint32_t z = min_distance->z; z < min_distance->z + min_distance->dist.z; ++z)
        {
            const uint32_t x = min_distance->x;
            const uint32_t y = min_distance->y;
            const uint32_t index = _flatten_3d(_uvec3_init(x, y, z), context->dimension);
            for (uint32_t i = 0; i < context->voxel_set_count; ++i)
                MELT_ASSERT(!_uvec3_equals(context->voxel_set[i].position, _uvec3_init(x, y, z)));
            MELT_ASSERT(_inner_voxel(context->voxel_field[index]));
        }
    }
#else
    MELT_UNUSED(context);
#endif
}

static void _debug_validate_max_extents(const _context_t* context, const _max_extent_t* max_extents, uint32_t max_extent_count)
{
#if defined(MELT_DEBUG) && defined(MELT_ASSERT)
    for (uint32_t i = 0; i < max_extent_count; ++i)
    {
        const _max_extent_t* extent = &max_extents[i];
        for (uint32_t x = extent->position.x; x < extent->position.x + extent->extent.x; ++x)
        {
            for (uint32_t y = extent->position.y; y < extent->position.y + extent->extent.y; ++y)
            {
                for (uint32_t z = extent->position.z; z < extent->position.z + extent->extent.z; ++z)
                {
                    for (uint32_t i = 0; i < context->voxel_set_count; ++i)
                    {
                        const _voxel_t* voxel = &context->voxel_set[i];
                        MELT_ASSERT(!_uvec3_equals(voxel->position, _uvec3_init(x, y, z)));
                    }
                }
            }
        }
    }
#else
    MELT_UNUSED(context);
    MELT_UNUSED(max_extents);
    MELT_UNUSED(max_extent_count);
#endif
}

static void _update_min_distance_field(const _context_t* context, uvec3_t start_position, uvec3_t extent)
{
    MELT_PROFILE_BEGIN();
    MELT_ASSERT(start_position.x - 1 != ~0U);
    MELT_ASSERT(start_position.y - 1 != ~0U);
    MELT_ASSERT(start_position.z - 1 != ~0U);

    for (uint32_t x = start_position.x - 1; x != ~0U; --x)
    {
        for (uint32_t y = start_position.y; y < start_position.y + extent.y; ++y)
        {
            for (uint32_t z = start_position.z; z < start_position.z + extent.z; ++z)
            {
                const uint32_t index = _flatten_3d(_uvec3_init(x, y, z), context->dimension);
                if (_inner_voxel(context->voxel_field[index]))
                {
                    _min_distance_t* min_distance = &context->min_distance_field[index];
                    const uint32_t updated_distance_x = start_position.x - min_distance->x;
                    min_distance->dist.x = _uint32_t_min(updated_distance_x, min_distance->dist.x);
                }
            }
        }
    }
    for (uint32_t x = start_position.x; x < start_position.x + extent.x; ++x)
    {
        for (uint32_t y = start_position.y - 1; y != ~0U; --y)
        {
            for (uint32_t z = start_position.z; z < start_position.z + extent.z; ++z)
            {
                const uint32_t index = _flatten_3d(_uvec3_init(x, y, z), context->dimension);
                if (_inner_voxel(context->voxel_field[index]))
                {
                    _min_distance_t* min_distance = &context->min_distance_field[index];
                    const uint32_t updated_distance_y = start_position.y - min_distance->y;
                    min_distance->dist.y = _uint32_t_min(updated_distance_y, min_distance->dist.y);
                }
            }
        }
    }
    for (uint32_t x = start_position.x; x < start_position.x + extent.x; ++x)
    {
        for (uint32_t y = start_position.y; y < start_position.y + extent.y; ++y)
        {
            for (uint32_t z = start_position.z - 1; z != ~0U; --z)
            {
                const uint32_t index = _flatten_3d(_uvec3_init(x, y, z), context->dimension);
                if (_inner_voxel(context->voxel_field[index]))
                {
                    _min_distance_t* min_distance = &context->min_distance_field[index];
                    const uint32_t updated_distance_z = start_position.z - min_distance->z;
                    min_distance->dist.z = _uint32_t_min(updated_distance_z, min_distance->dist.z);
                }
            }
        }
    }

    MELT_PROFILE_END();
}

static melt_occluder_box_type_t _select_voxel_indices(melt_occluder_box_type_flags_t box_type_flags, const uint16_t** out_indices, uint32_t* out_index_length)
{
#define CHECK_FLAG(flag) (box_type_flags & flag) == flag

    if (CHECK_FLAG(MELT_OCCLUDER_BOX_TYPE_REGULAR))
    {
        *out_indices = (const uint16_t*)_voxel_cube_indices;
        *out_index_length = MELT_ARRAY_LENGTH(_voxel_cube_indices);
        return MELT_OCCLUDER_BOX_TYPE_REGULAR;
    }
    else if (CHECK_FLAG(MELT_OCCLUDER_BOX_TYPE_SIDES))
    {
        *out_indices = (const uint16_t*)_voxel_cube_indices_sides;
        *out_index_length = MELT_ARRAY_LENGTH(_voxel_cube_indices_sides);
        return MELT_OCCLUDER_BOX_TYPE_SIDES;
    }
    else if (CHECK_FLAG(MELT_OCCLUDER_BOX_TYPE_BOTTOM))
    {
        *out_indices = (const uint16_t*)_voxel_cube_indices_bottom;
        *out_index_length = MELT_ARRAY_LENGTH(_voxel_cube_indices_bottom);
        return MELT_OCCLUDER_BOX_TYPE_BOTTOM;
    }
    else if (CHECK_FLAG(MELT_OCCLUDER_BOX_TYPE_TOP))
    {
        *out_indices = (const uint16_t*)_voxel_cube_indices_top;
        *out_index_length = MELT_ARRAY_LENGTH(_voxel_cube_indices_top);
        return MELT_OCCLUDER_BOX_TYPE_TOP;
    }
    else if (CHECK_FLAG(MELT_OCCLUDER_BOX_TYPE_DIAGONALS))
    {
        *out_indices = (const uint16_t*)_voxel_cube_indices_diagonals;
        *out_index_length = MELT_ARRAY_LENGTH(_voxel_cube_indices_diagonals);
        return MELT_OCCLUDER_BOX_TYPE_DIAGONALS;
    }

#undef CHECK_FLAG

    return MELT_OCCLUDER_BOX_TYPE_NONE;
}

static uint16_t _index_count_per_aabb(melt_occluder_box_type_flags_t box_type_flags)
{
    uint16_t index_count = 0;
    while (box_type_flags != MELT_OCCLUDER_BOX_TYPE_NONE)
    {
        const uint16_t* indices = NULL;
        uint32_t indices_length = 0;
        melt_occluder_box_type_t selected_type = _select_voxel_indices(box_type_flags, &indices, &indices_length);
        MELT_ASSERT(indices && indices_length > 0);
        index_count += indices_length;
        box_type_flags &= ~selected_type;
    }
    return index_count;
}

static uint32_t _vertex_count_per_aabb()
{
    return MELT_ARRAY_LENGTH(_voxel_cube_vertices);
}

static void _add_voxel_to_mesh_with_color(vec3_t voxel_center, vec3_t half_voxel_size, melt_mesh_t* mesh, melt_occluder_box_type_flags_t box_type_flags, const color_3u8_t color)
{
    bool has_color = !_uvec3_equals(color, _color_null);
    uint16_t index_offset = (uint16_t)(has_color ? mesh->vertex_count / 2 : mesh->vertex_count);

    for (uint32_t i = 0; i < MELT_ARRAY_LENGTH(_voxel_cube_vertices); ++i)
    {
        vec3_t vertex = _vec3_add(_vec3_mul(half_voxel_size, _voxel_cube_vertices[i]), voxel_center);
        mesh->vertices[mesh->vertex_count++] = vertex;
        if (has_color) mesh->vertices[mesh->vertex_count++] = _vec3_div(_uvec3_to_vec3(color), 255.0f);
    }

    while (box_type_flags != MELT_OCCLUDER_BOX_TYPE_NONE)
    {
        const uint16_t* indices = NULL;
        uint32_t indices_length = 0;

        melt_occluder_box_type_t selected_type = _select_voxel_indices(box_type_flags, &indices, &indices_length);

        MELT_ASSERT(indices && indices_length > 0);
        for (uint32_t i = 0; i < indices_length; ++i)
        {
            mesh->indices[mesh->index_count++] = indices[i] + index_offset;
        }

        box_type_flags &= ~selected_type;
    }
}

static void _add_voxel_to_mesh(vec3_t voxel_center, vec3_t half_voxel_size, melt_mesh_t* mesh, melt_occluder_box_type_flags_t box_type_flags)
{
    _add_voxel_to_mesh_with_color(voxel_center, half_voxel_size, mesh, box_type_flags, _color_null);
}

#if defined(MELT_DEBUG)
static void _add_voxel_set_to_mesh(const _voxel_t* voxel_set, const uint32_t voxel_set_count, vec3_t half_voxel_extent, melt_mesh_t* mesh)
{
    for (uint32_t i = 0; i < voxel_set_count; ++i)
    {
        _add_voxel_to_mesh_with_color(_aabb_center(voxel_set[i].aabb), half_voxel_extent, mesh, MELT_OCCLUDER_BOX_TYPE_REGULAR, _color_steel_blue);
    }
}
#endif

static _max_extent_t _get_max_extent(const _context_t* context)
{
    MELT_PROFILE_BEGIN();

    _max_extent_t max_extent;
    max_extent.extent = _uvec3_init(0, 0, 0);
    max_extent.position = _uvec3_init(0, 0, 0);
    max_extent.volume = 0;

    for (uint32_t i = 0; i < context->size; ++i)
    {
        const _min_distance_t* min_distance = &context->min_distance_field[i];
        _voxel_status_t voxel_status = context->voxel_field[_flatten_3d(min_distance->position, context->dimension)];
        if (_inner_voxel(voxel_status))
        {
            uvec3_t extent = _get_max_aabb_extent(context, min_distance);
            uint32_t volume = extent.x * extent.y * extent.z;
            if (volume > max_extent.volume)
            {
                max_extent.extent = extent;
                max_extent.position = min_distance->position;
                max_extent.volume = max_extent.extent.x * max_extent.extent.y * max_extent.extent.z;
            }
        }
    }

    MELT_PROFILE_END();

    return max_extent;
}

void _init_context(_context_t* context, vec3_t voxel_count)
{
    memset(context, 0, sizeof(_context_t));
    context->dimension = _vec3_to_uvev3(voxel_count);
    context->size = (uint32_t)voxel_count.x * (uint32_t)voxel_count.y * (uint32_t)voxel_count.z;
    context->voxel_field = MELT_MALLOC(_voxel_status_t, context->size);
    context->min_distance_field = MELT_MALLOC(_min_distance_t, context->size);
    context->voxel_indices = MELT_MALLOC(int32_t, context->size);
    context->voxel_set = MELT_MALLOC(_voxel_t, context->size);
    for (uint32_t i = 0; i < context->size; ++i)
        context->voxel_indices[i] = -1;
}

void _free_context(_context_t* context)
{
    _free_per_plane_voxel_set(context);
    MELT_FREE(context->voxel_indices);
    MELT_FREE(context->voxel_field);
    MELT_FREE(context->min_distance_field);
    MELT_FREE(context->voxel_set);
}

void melt_free_result(melt_result_t result)
{
    MELT_FREE(result.mesh.vertices);
    MELT_FREE(result.mesh.indices);
    MELT_FREE(result.debug_mesh.vertices);
    MELT_FREE(result.debug_mesh.indices);
}

int melt_generate_occluder(melt_params_t params, melt_result_t* out_result)
{
    MELT_ASSERT(params._start_canary == 0 && params._end_canary == 0 && "Make sure to memset params to 0 before use");

    vec3_t voxel_extent = _vec3_init(params.voxel_size, params.voxel_size, params.voxel_size);
    vec3_t half_voxel_extent = _vec3_mulf(voxel_extent, 0.5f);

    _aabb_t mesh_aabb = _generate_aabb_from_mesh(params.mesh);

    mesh_aabb.min = _vec3_sub(_map_to_voxel_min_bound(mesh_aabb.min, params.voxel_size), voxel_extent);
    mesh_aabb.max = _vec3_add(_map_to_voxel_max_bound(mesh_aabb.max, params.voxel_size), voxel_extent);

    vec3_t mesh_extent = _vec3_sub(mesh_aabb.max, mesh_aabb.min);
    vec3_t inv_mesh_extent = _vec3_init(1.0f / mesh_extent.x, 1.0f / mesh_extent.y, 1.0f / mesh_extent.z);
    vec3_t voxel_count = _vec3_div(mesh_extent, params.voxel_size);
    vec3_t voxel_resolution = _vec3_mul(voxel_count, inv_mesh_extent);

    _context_t context;
    _init_context(&context, voxel_count);

    // Perform shell voxelization
    for (uint32_t i = 0; i < params.mesh.index_count; i += 3)
    {
        MELT_PROFILE_BEGIN();

        _triangle_t triangle;

        triangle.v0 = params.mesh.vertices[params.mesh.indices[i + 0]];
        triangle.v1 = params.mesh.vertices[params.mesh.indices[i + 1]];
        triangle.v2 = params.mesh.vertices[params.mesh.indices[i + 2]];

        _aabb_t triangle_aabb = _generate_aabb_from_triangle(&triangle);

        // Voxel snapping, snap the triangle extent to find the 3d grid to iterate on.
        triangle_aabb.min = _vec3_sub(_map_to_voxel_min_bound(triangle_aabb.min, params.voxel_size), voxel_extent);
        triangle_aabb.max = _vec3_add(_map_to_voxel_max_bound(triangle_aabb.max, params.voxel_size), voxel_extent);

        for (float x = triangle_aabb.min.x; x <= triangle_aabb.max.x; x += params.voxel_size)
        {
            for (float y = triangle_aabb.min.y; y <= triangle_aabb.max.y; y += params.voxel_size)
            {
                for (float z = triangle_aabb.min.z; z <= triangle_aabb.max.z; z += params.voxel_size)
                {
                    _voxel_t voxel;

                    voxel.aabb.min = _vec3_sub(_vec3_init(x, y, z), half_voxel_extent);
                    voxel.aabb.max = _vec3_add(_vec3_init(x, y, z), half_voxel_extent);

                    MELT_ASSERT(voxel.aabb.min.x >= mesh_aabb.min.x - half_voxel_extent.x);
                    MELT_ASSERT(voxel.aabb.min.y >= mesh_aabb.min.y - half_voxel_extent.y);
                    MELT_ASSERT(voxel.aabb.min.z >= mesh_aabb.min.z - half_voxel_extent.z);

                    MELT_ASSERT(voxel.aabb.max.x <= mesh_aabb.max.x + half_voxel_extent.x);
                    MELT_ASSERT(voxel.aabb.max.y <= mesh_aabb.max.y + half_voxel_extent.y);
                    MELT_ASSERT(voxel.aabb.max.z <= mesh_aabb.max.z + half_voxel_extent.z);

                    vec3_t voxel_center = _aabb_center(voxel.aabb);
                    vec3_t relative_to_origin = _vec3_sub(_vec3_sub(voxel_center, mesh_aabb.min), half_voxel_extent);

                    if (!_aabb_intersects_triangle(&triangle, voxel_center, half_voxel_extent))
                        continue;
                    voxel.position = _vec3_to_uvev3(_vec3_mul(relative_to_origin, voxel_resolution));

                    const uint32_t index = _flatten_3d(voxel.position, context.dimension);
                    if (context.voxel_indices[index] != -1)
                        continue;

                    context.voxel_indices[index] = (int32_t)context.voxel_set_count;
                    context.voxel_set[context.voxel_set_count] = voxel;
                    ++context.voxel_set_count;
                }
            }
        }

        MELT_PROFILE_END();
    }

    // Generate a flat voxel list per plane (x,y), (x,z), (y,z)
    _generate_per_plane_voxel_set(&context);

    // The minimum distance field is a data structure representing, for each voxel,
    // the minimum distance that we can go in each of the positive directions x, y,
    // z until we collide with a shell voxel. The voxel field is a data structure
    // representing the state of the voxels. The clip status is a representation of
    // whether a voxel is in the clip state, whether a voxel can 'see' a voxel in
    // each of the directions +x,-x,+y,-y,+z,-z, and whether the voxel is an 'inner'
    // voxel (contained within the shell voxels).

    // Generate the minimum distance field, and voxel status from the initial shell.

    _generate_fields(&context);

    if (!_water_tight_mesh(&context))
    {
        _free_context(&context);
        return 0;
    }

    _debug_validate_min_distance_field(&context);

    uint32_t volume = 0;
    uint32_t total_volume = 0;
    float fill_pct = 0.0f;

    // Approximate the volume of the mesh by the number of voxels that can fit within.
    for (uint32_t i = 0; i < context.size; ++i)
    {
        const _min_distance_t* min_distance = &context.min_distance_field[i];
        _voxel_status_t voxel_status = context.voxel_field[_flatten_3d(min_distance->position, context.dimension)];

        // Each inner voxel adds one unit to the volume.
        if (_inner_voxel(voxel_status))
            ++total_volume;
    }

    _max_extent_t* max_extents = MELT_MALLOC(_max_extent_t, total_volume);
    uint32_t max_extent_count = 0;

    // One iteration to find an extent does the following:
    // . Get the extent that maximizes the volume considering the minimum distance
    //    field
    // . Clip the max extent found to the set of inner voxels
    // . Update the minimum distance field by adjusting the distances on the set
    //    of inner voxels. This is done by extending the extent cube to infinity
    //    on each of the axes +x, +y, +z
    while (fill_pct < params.fill_pct && volume != total_volume)
    {
        _max_extent_t max_extent = _get_max_extent(&context);

        _clip_voxel_field(&context, max_extent.position, max_extent.extent);

        _update_min_distance_field(&context, max_extent.position, max_extent.extent);

        _debug_validate_min_distance_field(&context);

        max_extents[max_extent_count++] = max_extent;

        fill_pct += (float)max_extent.volume / total_volume;
        volume += max_extent.volume;
    }

    memset(out_result, 0, sizeof(melt_result_t));

    out_result->mesh.vertices = MELT_MALLOC(vec3_t, _vertex_count_per_aabb() * max_extent_count);
    out_result->mesh.indices = MELT_MALLOC(uint16_t, _index_count_per_aabb(params.box_type_flags) * max_extent_count);

    for (uint32_t i = 0; i < max_extent_count; ++i)
    {
        const _max_extent_t* extent = &max_extents[i];

        vec3_t half_extent = _vec3_mul(_uvec3_to_vec3(extent->extent), half_voxel_extent);
        vec3_t voxel_position = _vec3_mul(_uvec3_to_vec3(extent->position), voxel_extent);
        vec3_t voxel_position_biased_to_center = _vec3_add(voxel_position, half_extent);
        vec3_t aabb_center = _vec3_add(mesh_aabb.min, voxel_position_biased_to_center);

        _add_voxel_to_mesh(_vec3_add(aabb_center, half_voxel_extent), half_extent, &out_result->mesh, params.box_type_flags);
    }

    _debug_validate_max_extents(&context, max_extents, max_extent_count);

#if defined(MELT_DEBUG)
    if (params.debug.flags > 0)
    {
        // _add_voxel_to_mesh(aabb_center(mesh_aabb), (mesh_aabb.max - mesh_aabb.min) * 0.5f, out_result->debug_mesh, _colors[0]);

        if (params.debug.flags & MELT_DEBUG_TYPE_SHOW_OUTER)
        {
            _add_voxel_set_to_mesh(context.voxel_set, context.voxel_set_count, _vec3_mulf(half_voxel_extent, params.debug.voxelScale), &out_result->debug_mesh);
        }
        if (params.debug.flags & MELT_DEBUG_TYPE_SHOW_SLICE_SELECTION)
        {
            if (params.debug.voxel_y > 0 && params.debug.voxel_z > 0)
            {
                uint32_t index = _flatten_2d(_uvec2_init(params.debug.voxel_y, params.debug.voxel_z), _uvec2_init(context.dimension.y, context.dimension.z));
                const _voxel_set_plane_t* voxels_x = &context.voxel_set_planes.x[index];
                _add_voxel_set_to_mesh(voxels_x->voxels, voxels_x->voxel_count, _vec3_mulf(half_voxel_extent, params.debug.voxelScale), &out_result->debug_mesh);
            }
            if (params.debug.voxel_x > 0 && params.debug.voxel_z > 0)
            {
                uint32_t index = _flatten_2d(_uvec2_init(params.debug.voxel_x, params.debug.voxel_z), _uvec2_init(context.dimension.x, context.dimension.z));
                const _voxel_set_plane_t* voxels_y = &context.voxel_set_planes.y[index];
                _add_voxel_set_to_mesh(voxels_y->voxels, voxels_y->voxel_count, _vec3_mulf(half_voxel_extent, params.debug.voxelScale), &out_result->debug_mesh);
            }
            if (params.debug.voxel_x > 0 && params.debug.voxel_y > 0)
            {
                uint32_t index = _flatten_2d(_uvec2_init(params.debug.voxel_x, params.debug.voxel_y), _uvec2_init(context.dimension.x, context.dimension.y));
                const _voxel_set_plane_t* voxels_z = &context.voxel_set_planes.z[index];
                _add_voxel_set_to_mesh(voxels_z->voxels, voxels_z->voxel_count, _vec3_mulf(half_voxel_extent, params.debug.voxelScale), &out_result->debug_mesh);
            }
        }
        if (params.debug.flags & MELT_DEBUG_TYPE_SHOW_INNER)
        {
            for (uint32_t i = 0; i < context.size; ++i)
            {
                const _min_distance_t* min_distance = &context.min_distance_field[i];
                const uint32_t index = _flatten_3d(min_distance->position, context.dimension);
                if (!context.voxel_field[index].inner)
                    continue;

                vec3_t voxel_position = _vec3_mul(_uvec3_to_vec3(min_distance->position), voxel_extent);
                vec3_t voxel_center = _vec3_add(mesh_aabb.min, voxel_position);
                if (params.debug.voxel_x < 0 ||
                    params.debug.voxel_y < 0 ||
                    params.debug.voxel_z < 0)
                {
                    _add_voxel_to_mesh_with_color(_vec3_add(voxel_center, voxel_extent), half_voxel_extent,
                        &out_result->debug_mesh, MELT_OCCLUDER_BOX_TYPE_REGULAR, _color_steel_blue);
                }
            }
        }
        if (params.debug.flags & MELT_DEBUG_TYPE_SHOW_MIN_DISTANCE)
        {
            for (uint32_t i = 0; i < context.size; ++i)
            {
                const _min_distance_t* min_distance = &context.min_distance_field[i];
                vec3_t voxel_center = _vec3_add(mesh_aabb.min, _vec3_mul(_uvec3_to_vec3(min_distance->position), voxel_extent));
                if ((uint32_t)params.debug.voxel_x == min_distance->x &&
                    (uint32_t)params.debug.voxel_y == min_distance->y &&
                    (uint32_t)params.debug.voxel_z == min_distance->z)
                {
                    _add_voxel_to_mesh_with_color(_vec3_add(voxel_center, voxel_extent), half_voxel_extent, &out_result->debug_mesh,
                        MELT_OCCLUDER_BOX_TYPE_REGULAR, _color_steel_blue);

                    for (uint32_t x = min_distance->x; x < min_distance->x + min_distance->dist.x; ++x)
                    {
                        vec3_t voxel_center_x = _vec3_add(mesh_aabb.min, _vec3_mul(_vec3_init(x, min_distance->y, min_distance->z), voxel_extent));
                        _add_voxel_to_mesh_with_color(_vec3_add(voxel_center_x, voxel_extent), half_voxel_extent,
                            &out_result->debug_mesh, MELT_OCCLUDER_BOX_TYPE_REGULAR, _color_steel_blue);
                    }
                    for (uint32_t y = min_distance->y; y < min_distance->y + min_distance->dist.y; ++y)
                    {
                        vec3_t voxel_center_y = _vec3_add(mesh_aabb.min, _vec3_mul(_vec3_init(min_distance->x, y, min_distance->z), voxel_extent));
                        _add_voxel_to_mesh_with_color(_vec3_add(voxel_center_y, voxel_extent), half_voxel_extent,
                            &out_result->debug_mesh, MELT_OCCLUDER_BOX_TYPE_REGULAR, _color_steel_blue);
                    }
                    for (uint32_t z = min_distance->z; z < min_distance->z + min_distance->dist.z; ++z)
                    {
                        vec3_t voxel_center_z = _vec3_add(mesh_aabb.min, _vec3_mul(_vec3_init(min_distance->x, min_distance->y, z), voxel_extent));
                        _add_voxel_to_mesh_with_color(_vec3_add(voxel_center_z, voxel_extent), half_voxel_extent,
                            &out_result->debug_mesh, MELT_OCCLUDER_BOX_TYPE_REGULAR, _color_steel_blue);
                    }
                }
            }
        }
        if (params.debug.flags & MELT_DEBUG_TYPE_SHOW_EXTENT)
        {
            for (uint32_t i = 0; i < context.size; ++i)
            {
                const _min_distance_t* min_distance = &context.min_distance_field[i];
                uvec3_t max_extent = _get_max_aabb_extent(&context, min_distance);
                for (uint32_t x = min_distance->x; x < min_distance->x + max_extent.x; ++x)
                {
                    for (uint32_t y = min_distance->y; y < min_distance->y + max_extent.y; ++y)
                    {
                        for (uint32_t z = min_distance->z; z < min_distance->z + max_extent.z; ++z)
                        {
                            vec3_t voxel_center = _vec3_add(mesh_aabb.min, _vec3_mul(_vec3_init(x, y, z), voxel_extent));
                            _add_voxel_to_mesh_with_color(_vec3_add(voxel_center, voxel_extent), half_voxel_extent, &out_result->debug_mesh,
                                MELT_OCCLUDER_BOX_TYPE_REGULAR, _color_steel_blue);
                        }
                    }
                }
            }
        }
        if (params.debug.flags & MELT_DEBUG_TYPE_SHOW_RESULT)
        {
            out_result->debug_mesh.vertices = MELT_MALLOC(vec3_t, _vertex_count_per_aabb() * max_extent_count * 2);
            out_result->debug_mesh.indices = MELT_MALLOC(uint16_t, _index_count_per_aabb(params.box_type_flags) * max_extent_count);

            for (size_t i = 0; i < max_extent_count; ++i)
            {
                const _max_extent_t* extent = &max_extents[i];
                if ((int32_t)i == params.debug.extent_index || params.debug.extent_index < 0)
                {
                    vec3_t half_extent = _vec3_mul(_uvec3_to_vec3(extent->extent), half_voxel_extent);
                    vec3_t voxel_position = _vec3_mul(_uvec3_to_vec3(extent->position), voxel_extent);
                    vec3_t voxel_position_biased_to_center = _vec3_add(voxel_position, half_extent);
                    vec3_t aabb_center = _vec3_add(mesh_aabb.min, voxel_position_biased_to_center);
                    color_3u8_t color = _colors[i % MELT_ARRAY_LENGTH(_colors)];
                    _add_voxel_to_mesh_with_color(_vec3_add(aabb_center, half_voxel_extent), half_extent, &out_result->debug_mesh, params.box_type_flags, color);
                }
            }

            MELT_ASSERT(out_result->debug_mesh.vertex_count == _vertex_count_per_aabb() * max_extent_count * 2);
            MELT_ASSERT(out_result->debug_mesh.index_count ==  _index_count_per_aabb(params.box_type_flags) * max_extent_count);
        }
    }
#endif

    _free_context(&context);
    MELT_FREE(max_extents);
    return 1;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // MELT_IMPLEMENTATION
