#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColour;

layout(binding = 0) uniform sampler2D positionSampler;
layout(binding = 1) uniform sampler2D colourSampler;
layout(binding = 2) uniform sampler2D normalSampler;

float sq(float a) { return a*a; }
float zSphere(vec2 point)
{
    return sqrt(sq(0.25) - sq(point.x - 0.5) - sq(point.y - 0.5));
}
void main()
{
//    vec2 dir = fragUV - vec2(0.5);
//    float dist = length(dir);
//    if(dist < 0.25)
//    {
//        float z = zSphere(fragUV);
//        vec3 sphereDir = vec3(fragUV,z) - vec3(0.5,0.5,0);
//        outColour = texture(texSampler, fragUV - sphereDir.xy*(sphereDir.z*2));
//    }
//    else
        vec4 normal = texture(normalSampler, fragUV);
        float intensity = 1;
        if(normal.a != 0)
        {
            intensity = clamp(dot(normalize(vec3(-1,1,-1)), vec3(normal))+0.2, 0,1);
        }
        outColour = texture(colourSampler, fragUV)*vec4(vec3(intensity),1);
}