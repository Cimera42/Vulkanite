#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;

layout(location = 3) in mat4 instanceMatrix;

layout(location = 0) out vec2 fragUV;

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(push_constant) uniform ParticlePushConstantBufferObject {
    mat4 view;
    mat4 proj;
} pcbo;

void main()
{
    vec4 p = vec4(inPosition, 1.0);
//    float extra = 0;
//    for(int i = 0; i < 4; i++)
//    {
//        for(int j = 0; j < 4; j++)
//        {
//            extra += abs(pcbo.view[i][j]);
//        }
//    }
//    p.x += extra;
//    gl_Position = p;

//    p.x += gl_InstanceIndex*0.25;
    gl_Position = pcbo.proj * pcbo.view * instanceMatrix * p;
    fragUV = inUV;
}