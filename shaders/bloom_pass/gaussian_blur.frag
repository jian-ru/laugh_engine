#version 450

#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0) uniform sampler2D inputImage;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

layout (push_constant) uniform PushConst
{
	bool isHorizontal;
} pcs;


void main() 
{
	const int stepCount = 4;
	const float weights[stepCount] = { 0.249615, 0.192463, 0.051476, 0.006446 };
	const float offsets[stepCount] = { 0.644342, 2.378848, 4.291111, 6.216607 };
	
	vec2 deltaUV = (1.0 / textureSize(inputImage, 0)) * vec2(float(pcs.isHorizontal), float(!pcs.isHorizontal));
	vec3 result = vec3(0.0);
	
	for (int i = 0; i < stepCount; ++i)
	{
		vec2 offset = offsets[i] * deltaUV;
		vec3 color = texture(inputImage, inUV + offset).rgb + texture(inputImage, inUV - offset).rgb;
		result += color * weights[i];
	}
	
	outColor = vec4(result, 1.0);
}

