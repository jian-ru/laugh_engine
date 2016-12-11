#version 450

#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 1) uniform samplerCube hdrProbe; // unfiltered map with mips

layout (location = 0) in vec3 inNormal;

layout (location = 0) out vec4 outColor;

layout (push_constant) uniform PushConst
{
	uint curLevel;
} pcs;


void main() 
{
	vec3 hdrColor = textureLod(hdrProbe, inNormal, float(pcs.curLevel)).rgb;
	outColor = vec4(hdrColor, 1.0);
}