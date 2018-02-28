#version 420 core
#extension GL_ARB_bindless_texture : require

in vec2 texture_coord;
flat in uint max_level;
flat in uint toggle_view;
flat in uvec2 physical_texture_dim;
flat in uvec2 index_texture_dim;

uniform uvec2 physical_resident_near;
uniform uvec2 index_resident_near;
/*alternative:
layout(bindless_sampler) uniform sampler2D physical_resident_near;
layout(bindless_sampler) uniform sampler2D index_resident_near;*/

//layout(binding = 0) uniform sampler2DArray physical_texture_array;
//layout(binding = 1) uniform usampler2D index_texture;
layout(location = 0) out vec4 out_color;

void main()
{
    sampler2DArray physical_tex_sample = sampler2DArray(physical_resident_near);
    usampler2D index_tex_sample = usampler2D(index_resident_near);

    // swap y axis
    vec2 swapped_y_texture_coordinates = texture_coord;
    swapped_y_texture_coordinates.y = 1.0 - swapped_y_texture_coordinates.y;
    // access on index texture, reading x,y,LoD into a uvec3 -> efficient
    uvec4 index_quadruple = texture(index_tex_sample, swapped_y_texture_coordinates).xyzw;
    uint layer_id = index_quadruple.w;
    vec4 c;
    if(toggle_view == 0)
    {
        // Show the physical texture
        //TODO get layer from index.w and display layers dynamically
        c = texture(physical_tex_sample, vec3(swapped_y_texture_coordinates, layer_id) );
    }
    else
    {
        // Show the image viewer

        // 4-channel texture on the cpu (RGBA, => XXXXXXXX YYYYYYYY ZZZZZZZZ WWWWWWWW)
        // X = X_index offset
        // Y = Y_index_offset
        // Z = LoD
        // W = Layer_index (as int, is cast to float when used)



        // extracting LoD from index texture into a new var
        uint current_level = index_quadruple.z;

        // exponent for calculating the occupied pixel in our index texture, based on which level the tile is in
        uint tile_occupation_exponent = max_level - current_level;

        // 2^tile_occupation_exponent defines how many pixel (of the index texture) are used by the given tile
        uint occupied_index_pixel_per_dimension = uint(pow(2, tile_occupation_exponent));

        // offset represented as tiles is divided by total num tiles per axis
        // (replace max_width_tiles later by correct uniform)
        // extracting x,y from index texture
        vec2 base_xy_offset = index_quadruple.xy;

        // just to be conformant to the modf interface (integer parts are ignored)
        vec2 dummy;

        // base x,y coordinates * number of tiles / number of used index texture pixel
        // taking the factional part by modf
        vec2 physical_tile_ratio_xy = modf((swapped_y_texture_coordinates.xy * index_texture_dim / vec2(occupied_index_pixel_per_dimension)), dummy);

        // adding the ratio for every texel to our base offset to get the right pixel in our tile and dividing it by the dimension of the phy. tex.
        vec2 physical_texture_coordinates = (base_xy_offset.xy + physical_tile_ratio_xy) / physical_texture_dim;

        // c = vec4(physical_tile_ratio_xy, 0.0, 1.0);

        // outputting the calculated coordinate from our physical texture
        //TODO layer dym
        c = texture(physical_tex_sample, vec3(physical_texture_coordinates, layer_id) );
    }
    out_color = c;
}
