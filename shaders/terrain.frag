#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragPos;

layout(location = 0) out vec4 outColour;

layout(binding = 0) uniform sampler2DArray texSampler;

void main()
{
    //https://gamedevelopment.tutsplus.com/articles/use-tri-planar-texture-mapping-for-better-terrain--gamedev-13821
    vec3 blend = normalize(abs(fragNormal));
     //Make sure all parts add to make 1
     //to prevent bright/dark spots
    blend /= blend.x + blend.y + blend.z;

    vec3 tex0X = texture(texSampler, vec3(fragPos.yz/5,0)).rgb;
    vec3 tex0Y = texture(texSampler, vec3(fragPos.xz/5,0)).rgb;
    vec3 tex0Z = texture(texSampler, vec3(fragPos.xy/5,0)).rgb;
    vec3 tex0 = tex0X*blend.x + tex0Y*blend.y + tex0Z*blend.z;

    vec3 tex1X = texture(texSampler, vec3(fragPos.yz/5,1)).rgb;
    vec3 tex1Y = texture(texSampler, vec3(fragPos.xz/5,1)).rgb;
    vec3 tex1Z = texture(texSampler, vec3(fragPos.xy/5,1)).rgb;
    vec3 tex1 = tex1X*blend.x + tex1Y*blend.y + tex1Z*blend.z;

    vec3 col = mix(tex0, tex1, blend.y);

    float intensity = max(0,dot(normalize(vec3(1,1,0)), fragNormal));
    intensity += 0.2; //ambient
    intensity = clamp(intensity, 0, 1);
    outColour = vec4(col*intensity, 1);
}