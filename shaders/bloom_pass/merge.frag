#version 450

#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0) uniform sampler2D blurredMask;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;


void main() 
{
	vec3 bloomColor = texture(blurredMask, inUV).rgb;
	
	outColor = vec4(bloomColor, 1.0);
}

