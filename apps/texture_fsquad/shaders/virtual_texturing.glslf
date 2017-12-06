#version 430 core

in vec2 texture_coord;
flat in uint max_level;
flat in uint toggle_view;
flat in uvec2 physical_texture_dim;
flat in uvec2 index_texture_dim;

layout(binding  = 0)        uniform sampler2D  physical_texture;
layout(binding  = 1)        uniform usampler2D index_texture;
layout(binding  = 2, r32ui) coherent uniform uimage2D   feedback_image;

layout(location = 0) out     vec4       out_color;

void main() {
    // swap y axis
    vec2 swapped_y_texture_coordinates = texture_coord;
    swapped_y_texture_coordinates.y = 1.0 - swapped_y_texture_coordinates.y;

    vec4 c;
    if(toggle_view == 0) { // Show the physical texture
        c = texture(physical_texture, swapped_y_texture_coordinates);
    } else { // Show the image viewer

        //access on index texture, reading x,y,LoD into a uvec3 -> efficient
        uvec3 index_triplet = texture(index_texture, swapped_y_texture_coordinates).xyz;

        //extracting LoD from index texture
        uint current_level = index_triplet.z;

        //exponent for calculating the occupied pixels in our index texture, based on which level the tile is in
        uint tile_occupation_exponent = max_level - current_level;

        //2^tile_occupation_exponent defines how many pixel (of the index texture) are used by the given tile
        uint occupied_index_pixel_per_dimension = uint(pow(2, tile_occupation_exponent));

        // offset represented as tiles is divided by total num tiles per axis
        // (replace max_width_tiles later by correct uniform)
        //extracting x,y from index texture
        vec2 base_xy_offset = index_triplet.xy;

        // just to be conformant to the modf interface (integer parts are ignored)
        vec2 dummy;

        //base x,y coordinates * number of tiles / number of used index texture pixel
        //taking the factional part by modf

        vec2 physical_tile_ratio_xy = modf((swapped_y_texture_coordinates.xy * index_texture_dim / vec2(occupied_index_pixel_per_dimension)), dummy);

        //adding the ratio for every texel to our base offset to get the right pixel in our tile
        //and dividing it by the dimension of the phy. tex.

        vec2 physical_texture_coordinates = (base_xy_offset.xy + physical_tile_ratio_xy) / physical_texture_dim;

        //c = vec4(physical_tile_ratio_xy, 0.0, 1.0);

        //outputting the calculated coordinate from our physical texture
        c = texture(physical_texture, physical_texture_coordinates);

        // simple feedback
        // TODO SOMETHING IS FISHY HERE
        imageAtomicAdd(feedback_image, ivec2(base_xy_offset.xy ), 1);
//        c = imageLoad(feedback_image, ivec2(swapped_y_texture_coordinates.xy * physical_texture_dim));

//        if(ivec2(swapped_y_texture_coordinates * physical_texture_dim) == ivec2(2,0)) {
//            c = vec4(0, 1, 0, 1);
//        }
    }

    out_color = c;
}

