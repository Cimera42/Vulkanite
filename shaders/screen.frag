#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 fragUV;

layout(location = 0) out vec4 outColour;

layout(binding = 0) uniform sampler2D texSampler;

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
        outColour = texture(texSampler, fragUV);
}