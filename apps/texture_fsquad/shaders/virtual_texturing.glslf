// Copyright (c) 2012 Christopher Lux <christopherlux@gmail.com>
// Distributed under the Modified BSD License, see license.txt.

#version 330 core

in vec2 texture_coord;

uniform sampler2D color_texture_linear;
uniform sampler2D color_texture_nearest;

layout(location = 0) out vec4        out_color;

void main()
{
    vec4 res;

    vec4 c;

    vec2 swapped_y_texture_coordinates = texture_coord;
    swapped_y_texture_coordinates.y = 1.0 - swapped_y_texture_coordinates.y;

    if (texture_coord.x > 0.5) {
        c = texture(color_texture_linear, swapped_y_texture_coordinates);
    }
    else {
        c = texture(color_texture_nearest, swapped_y_texture_coordinates);
    }

    res = c;

    out_color = res;
}

