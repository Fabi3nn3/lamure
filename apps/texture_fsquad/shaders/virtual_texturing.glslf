// Copyright (c) 2012 Christopher Lux <christopherlux@gmail.com>
// Distributed under the Modified BSD License, see license.txt.

#version 330 core

in vec2 texture_coord;

uniform sampler2D color_texture_linear;
uniform sampler2D color_texture_nearest;
uniform sampler2D physical_texture;
uniform usampler2D index_texture;

layout(location = 0) out vec4        out_color;

void main()
{
    vec2 swapped_y_texture_coordinates = texture_coord;
    swapped_y_texture_coordinates.y = 1.0 - swapped_y_texture_coordinates.y;

    int maxLevel = 2;
    float currentLevel = texture(index_texture, swapped_y_texture_coordinates).b;
    float blockwidth = 4;
    float max_width_tiles = 4;
    vec4 c;

    //float r_channel_sampler2D_test = texture(physical_texture, swapped_y_texture_coordinates).r;
    //float g_channel_sampler2D_test = texture(physical_texture, swapped_y_texture_coordinates).g;
    //float b_channel_sampler2D_test = texture(physical_texture, swapped_y_texture_coordinates).b;
    //vec2 bg_channel_sampler2D_test = texture(physical_texture, swapped_y_texture_coordinates).bg;

    //uint r_channel_usampler2D_test = texture(index_texture, swapped_y_texture_coordinates).r;
    //uvec2 bg_channel_usampler2D_test = texture(index_texture, swapped_y_texture_coordinates).bg;

    vec2 offset_xy_usampler2D = texture(index_texture, swapped_y_texture_coordinates).xy;

    float physical_tile_ratio_x = mod((swapped_y_texture_coordinates.x * max_width_tiles) , blockwidth) / blockwidth;

    float physical_tile_ratio_y = mod((swapped_y_texture_coordinates.y * max_width_tiles) , blockwidth) / blockwidth;

    offset_xy_usampler2D.x += physical_tile_ratio_x;
    offset_xy_usampler2D.y += physical_tile_ratio_y;

    c = texture(physical_texture, offset_xy_usampler2D);
    c *= vec4(1,0,0,1);




    //out_color = vec4(r_channel_usampler2D_test / 255.0);


    //if(r_channel_usampler2D_test == 203) {
    //    out_color = vec4(1.0, 0.0, 0.0, 1.0);
    //} else {
    //    out_color = vec4(0.0, 0.0, 1.0, 1.0);
    //}

    //c = texture(physical_texture, swapped_y_texture_coordinates);
    c = texture(index_texture, swapped_y_texture_coordinates);

//    if (texture_coord.x > 0.5) {
//        c = texture(color_texture_linear, swapped_y_texture_coordinates);
//    }
//    else {
//        c = texture(color_texture_nearest, swapped_y_texture_coordinates);
//    }

   out_color =c;
}

