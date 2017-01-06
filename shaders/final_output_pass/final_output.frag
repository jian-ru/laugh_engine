#version 450

#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0) uniform sampler2D finalOutput;
layout (set = 0, binding = 1) uniform sampler2DMS gbuffer1;
layout (set = 0, binding = 2) uniform sampler2DMS gbuffer2;
layout (set = 0, binding = 3) uniform sampler2DMS gbuffer3;
layout (set = 0, binding = 4) uniform sampler2DMS depthImage;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

layout(std140, set = 0, binding = 5) uniform UBO
{
	int displayMode;
};


vec4 unpackRGBA(float packedColor)
{
	uint tmp = uint(packedColor);	
	return vec4((tmp >> 24) & 0xff, (tmp >> 16) & 0xff, (tmp >> 8) & 0xff, tmp & 0xff) 
		* 0.0039215686274509803921568627451;
}

vec3 filmicTonemap(vec3 x)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    float W = 11.2;
    return ((x*(A*x+C*B)+D*E) / (x*(A*x+B)+D*F))- E / F;
}

vec3 applyFilmicToneMap(vec3 color) 
{
    color = 2.0 * filmicTonemap(color);
    vec3 whiteScale = 1.0 / filmicTonemap(vec3(11.2));
    color *= whiteScale;
    return color;
}


void main() 
{
	ivec2 extent = textureSize(gbuffer1);
	ivec2 pixelPos = ivec2(extent * inUV);

	vec3 finalColor = texture(finalOutput, inUV).rgb;
	vec4 gb1 = texelFetch(gbuffer1, pixelPos, 0);
	vec4 gb2 = texelFetch(gbuffer2, pixelPos, 0);
	vec4 RMAI = texelFetch(gbuffer3, pixelPos, 0);
	float depth = texelFetch(depthImage, pixelPos, 0).r;
	
	vec3 albedo = unpackRGBA(gb1.w).rgb;
	vec3 pos = gb2.xyz;
	vec3 nrm = gb1.xyz;
	uint matId = uint(round(RMAI.w * 255.0));
	
	if (displayMode == 4)
	{
		outColor = vec4(vec3(depth), 1.0);
		return;
	}
	
	if (depth >= 1.0 || depth <= 0.0) return;
	
	if (displayMode == 0)
	{
		finalColor = applyFilmicToneMap(finalColor);
		outColor = vec4(pow(finalColor, vec3(1.0 / 2.2)), 1.0);
		
		// outColor = vec4(finalColor, 1.0);
	}
	else if (displayMode == 1) // albedo
	{
		outColor = vec4(albedo, 1.0);
	}
	else if (displayMode == 2) // eye normal
	{
		if (matId != 0) outColor = vec4(nrm, 1.0);
	}
	else if (displayMode == 3) // eye position
	{
		outColor = vec4(abs(pos), 1.0);
	}
	else if (displayMode == 5) // roughness
	{
		outColor = vec4(RMAI.xxx, 1.0);
	}
	else if (displayMode == 6) // metalness
	{
		outColor = vec4(RMAI.yyy, 1.0);
	}
	else if (displayMode == 7)
	{
		outColor = vec4(RMAI.zzz, 1.0);
	}
}

