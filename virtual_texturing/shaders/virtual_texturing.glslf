#version 450 core
#extension GL_EXT_texture_array : enable

in vec2 texture_coord;
flat in uint max_level;
flat in uint toggle_view;
flat in uvec2 index_texture_dim;
flat in uvec2 physical_texture_dim;

flat in vec2 tile_size;
flat in vec2 tile_padding;

layout(std430, binding = 0) buffer out_feedback_ssbo { uint[] out_feedback_values; };

layout(binding = 0) uniform sampler2DArray physical_texture_array;
layout(binding = 1) uniform usampler2D index_texture;

layout(location = 0) out vec4 out_color;

void main()
{
    // swap y axis
    vec3 texture_coordinates = vec3(texture_coord, 0.0);
    texture_coordinates.y = 1.0 - texture_coordinates.y;

    uvec4 index_quadruple = texture(index_texture, texture_coord).rgba;
    texture_coordinates.z = float(index_quadruple.a);
    vec4 c;

    uint reference_count = 0;
    if(toggle_view == 0)
    { // Show the physical texture
        c = texture2DArray(physical_texture_array, texture_coordinates);
        out_color = c;
    }
    else
    { // Show the image viewer

        uint current_level = index_quadruple.z;

        // exponent for calculating the occupied pixels in our index texture, based on which level the tile is in
        uint tile_occupation_exponent = max_level - current_level;

        // 2^tile_occupation_exponent defines how many pixel (of the index texture) are used by the given tile
        uint occupied_index_pixel_per_dimension = uint(1 << tile_occupation_exponent);

        // offset represented as tiles is divided by total num tiles per axis
        // (replace max_width_tiles later by correct uniform)
        // extracting x,y from index texture
        uvec2 base_xy_offset = index_quadruple.xy;

        // just to be conformant to the modf interface (integer parts are ignored)
        vec2 dummy;

        // base x,y coordinates * number of tiles / number of used index texture pixel
        // taking the factional part by modf
        vec2 physical_tile_ratio_xy = modf((texture_coordinates.xy * index_texture_dim / vec2(occupied_index_pixel_per_dimension)), dummy);

        // Use only tile_size - 2*tile_padding pixels to render scene
        // Therefore, scale reduced tile size to full size and translate it
        vec2 padding_scale = 1 - 2 * tile_padding / tile_size;
        vec2 padding_offset = tile_padding / tile_size;

        // adding the ratio for every texel to our base offset to get the right pixel in our tile
        // and dividing it by the dimension of the phy. tex.
        vec2 physical_texture_coordinates = (base_xy_offset.xy + physical_tile_ratio_xy * padding_scale + padding_offset) / physical_texture_dim;

        // c = vec4(physical_tile_ratio_xy, 0.0, 1.0);

        // outputting the calculated coordinate from our physical texture
        c = texture2DArray(physical_texture_array, vec3(physical_texture_coordinates, texture_coordinates.z));

        // feedback calculation based on accumulated use of each rendered tile
        // TODO: maybe a bug here: take level of physical texture into account
        uint one_d_feedback_ssbo_index = base_xy_offset.x + base_xy_offset.y * physical_texture_dim.x;
        reference_count = atomicAdd(out_feedback_values[one_d_feedback_ssbo_index], 1);
        // reference_count += 1;

        // c = vec4( float(reference_count) / (10*375542.857 / float( (current_level+1) * (current_level+1) )), (float(reference_count) / (10*375542.857 / float(current_level+1) * (current_level+1) ))
        // * 0.3, 0.0, 1.0 );

        /*if(texture_coordinates.z == 0) {
            c = vec4(1.0, 0.0, 0.0, 1.0);
        }*/
    }

    out_color = c;
}