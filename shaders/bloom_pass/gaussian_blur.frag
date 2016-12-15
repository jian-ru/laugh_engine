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
	const float weight[5] = {0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216};
	
	vec2 deltaUV = 1.0 / textureSize(inputImage, 0);
	vec3 result = texture(inputImage, inUV).rgb * weight[0];
	
	if (pcs.isHorizontal)
	{
		for (int i = 1; i < 5; ++i)
		{
			result += texture(inputImage, inUV + vec2(deltaUV.s * i, 0.0)).rgb * weight[i];
			result += texture(inputImage, inUV - vec2(deltaUV.s * i, 0.0)).rgb * weight[i];
		}
	}
	else
	{
		for (int i = 1; i < 5; ++i)
		{
			result += texture(inputImage, inUV + vec2(0.0, deltaUV.t * i)).rgb * weight[i];
			result += texture(inputImage, inUV - vec2(0.0, deltaUV.t * i)).rgb * weight[i];
		}
	}
	
	outColor = vec4(result, 1.0);
}

