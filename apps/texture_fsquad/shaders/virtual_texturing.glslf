// Copyright (c) 2012 Christopher Lux <christopherlux@gmail.com>
// Distributed under the Modified BSD License, see license.txt.

#version 330 core

in vec2 texture_coord;

uniform sampler2D color_texture_linear;
uniform sampler2D color_texture_nearest;

//uniform usampler2D index_texture;
//uniform sampler2D physical_texture;

layout(location = 0) out vec4        out_color;

void main()
{
    vec4 res;
    vec4 c;


    vec2 swapped_y_texture_coordinates = texture_coord;
    swapped_y_texture_coordinates.y = 1.0 - swapped_y_texture_coordinates.y;


    //float r_channel_sampler2D_test = texture(physical_texture, swapped_y_texture_coordinates).r;
    //vec2 bg_channel_sampler2D_test = texture(physical_texture, swapped_y_texture_coordinates).bg;

    //uint r_channel_usampler2D_test = texture(index_texture, swapped_y_texture_coordinates).r;
    //uvec2 bg_channel_usampler2D_test = texture(index_texture, swapped_y_texture_coordinates).bg;


    //out_color = vec4(r_channel_usampler2D_test / 255.0);


    //if(r_channel_usampler2D_test == 203) {
    //    out_color = vec4(1.0, 0.0, 0.0, 1.0);
    //} else {
    //    out_color = vec4(0.0, 0.0, 1.0, 1.0);
    //}


    if (texture_coord.x > 0.5) {
        c = texture(color_texture_linear, swapped_y_texture_coordinates);
    }
    else {
        c = texture(color_texture_nearest, swapped_y_texture_coordinates);
    }

    res = c;

    out_color = res;
}

