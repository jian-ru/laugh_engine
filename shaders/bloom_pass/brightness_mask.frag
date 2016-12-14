#version 450

#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0) uniform sampler2D lightingResult;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;


void main() 
{
	vec3 hdrColor = texture(lightingResult, inUV).rgb;
	float brightness = dot(hdrColor, vec3(0.2126, 0.7152, 0.0722));
	
	if (brightness > 1.0)
	{
		outColor = vec4(hdrColor, 1.0);
	}
}

