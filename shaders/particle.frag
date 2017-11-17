#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 worldPos;

layout(location = 0) out vec4 outColour;

layout(binding = 0) uniform sampler2D texSampler;

void main()
{
    vec3 dir = normalize(vec3(5,5,5) - worldPos);
    float intensity = dot(dir, fragNormal);
    vec4 colour = texture(texSampler, fragUV);
    colour.rgb *= intensity;
    outColour = colour;
}