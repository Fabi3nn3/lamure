#version 420 core

in vec2 texture_coord;

layout(binding  = 0) uniform sampler2D  physical_texture;
layout(binding  = 1) uniform usampler2D index_texture;
layout(location = 0) out     vec4       out_color;

void main() {
    vec2 swapped_y_texture_coordinates = texture_coord;
    swapped_y_texture_coordinates.y = 1.0 - swapped_y_texture_coordinates.y;

    int maxLevel = 2;

    uvec3 index_triplet = texture(index_texture, swapped_y_texture_coordinates).xyz;
    uint currentLevel = index_triplet.z;

    uint tile_occupation_exponent = maxLevel - currentLevel;

    uint occupied_index_pixel_per_dimension = uint(pow(2, tile_occupation_exponent));

    //will be uploaded as uniform
    uvec2 physical_texture_dim = uvec2(7,3);
    //will be uploaded as uniform
    uvec2 index_texture_dim = uvec2(4, 4);
    vec4 c;

    // offset represented as tiles is divided by total num tiles per axis
    // (replace max_width_tiles later by correct uniform)
    vec2 base_xy_offset = index_triplet.xy / vec2(physical_texture_dim);

    // just to be conformant to the modf interface (integer parts are ignored)
    vec2 dummy;
    vec2 physical_tile_ratio_xy = modf((swapped_y_texture_coordinates.xy * index_texture_dim / vec2(occupied_index_pixel_per_dimension)), dummy);

    vec2 physical_texture_coordinates = base_xy_offset.xy + (physical_tile_ratio_xy / physical_texture_dim);

    c = vec4(physical_tile_ratio_xy, 0.0, 1.0);

    c = texture(physical_texture, physical_texture_coordinates);
    out_color = c;
}

