#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragPos;

layout(location = 0) out vec4 outColour;

layout(binding = 0) uniform sampler2DArray texSampler;

void main()
{
    //outColour = vec4(vec3(fragPos.y/10),1);//vec4((fragNormal+vec3(1))/2, 1);
    vec3 tex0 = texture(texSampler, vec3(fragPos.xz,0)).rgb;
    vec3 tex1 = texture(texSampler, vec3(fragPos.xz,1)).rgb;
    vec3 col = mix(tex0, tex1, fragNormal.y);
    outColour = vec4(col, 1);
}