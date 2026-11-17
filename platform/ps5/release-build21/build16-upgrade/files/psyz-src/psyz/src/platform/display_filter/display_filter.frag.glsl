// SPDX-License-Identifier: MIT
#version 450
layout(set=2,binding=0) uniform sampler2D image;
layout(set=3,binding=0,std140) uniform Parameters {
    vec4 source;       // selected visible source rectangle in texture pixels
    vec4 destination;  // unchanged physical output viewport
    vec4 effect;       // logical image rows, mode; remaining components reserved
};
layout(location=0) out vec4 colour;
void main() {
    vec2 p = (gl_FragCoord.xy-destination.xy)/destination.zw;
    // Clamp to the selected rectangle, not the whole VRAM page. Linear
    // interpolation must never gather neighboring pages or letterbox pixels.
    vec2 pixel = clamp(source.xy+p*source.zw,
                       source.xy+vec2(0.5),source.xy+source.zw-vec2(0.5));
    vec3 c = texture(image,pixel/vec2(textureSize(image,0))).rgb;
    float rows = max(1.0,effect.x);
    // Fade below two output pixels per original row; no single-pixel row
    // removal or source-resolution-dependent stripes on internally scaled art.
    float strength = smoothstep(1.0,2.0,destination.w/rows);
    float phase = 6.28318530718*p.y*rows;
    c *= 1.0 - 0.08*strength*(0.5-0.5*cos(phase));
    if(effect.y>1.5) {
        // Restrained soft RGB phosphor mask, fixed in physical output pixels.
        // No warp, overscan, blur, bloom, temporal history or invented detail.
        float x = 6.28318530718*(gl_FragCoord.x-destination.x)/3.0;
        vec3 mask = 0.5+0.5*cos(vec3(x,x-2.09439510239,x-4.18879020479));
        c *= vec3(1.0)-0.055*strength*(vec3(1.0)-mask);
    }
    colour=vec4(c,1.0);
}
