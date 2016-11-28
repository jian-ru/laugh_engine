#version 450

#extension GL_ARB_separate_shader_objects : enable

layout (binding = 0) uniform sampler2D finalOutput;
layout (binding = 1) uniform sampler2D gbuffer1;
layout (binding = 2) uniform sampler2D depthImage;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

layout(std140, binding = 3) uniform UBO
{
	int displayMode;
	float imageWidth;
	float imageHeight;
	float twoTimesTanHalfFovy;
	float zNear;
	float zFar;
};


vec4 unpackRGBA(float packedColor)
{
	uint tmp = uint(packedColor);	
	return vec4((tmp >> 24) & 0xff, (tmp >> 16) & 0xff, (tmp >> 8) & 0xff, tmp & 0xff) 
		* 0.0039215686274509803921568627451;
}

vec3 recoverEyePos(float depth)
{
	float oneOverHeight = 1.0 / imageHeight;
	float aspect = imageWidth * oneOverHeight;
	float ph = twoTimesTanHalfFovy * oneOverHeight;
	float pw = ph * aspect;
	vec2 extent = vec2(imageWidth, imageHeight);
	vec3 viewVec = vec3(
		vec2(pw, ph) * (vec2(inUV.x, 1.0 - inUV.y) * extent - extent * 0.5), -1.0);
	float eyeDepth = zFar * zNear / (zFar + depth * (zNear - zFar));
	return viewVec * eyeDepth;
}

vec3 recoverEyeNrm(vec2 xy)
{
	return vec3(xy, sqrt(1.0 - max(0.0, dot(xy, xy))));
}


void main() 
{
	vec4 finalColor = texture(finalOutput, inUV);
	vec4 gb1 = texture(gbuffer1, inUV);
	float depth = texture(depthImage, inUV).r;
	
	vec3 albedo = unpackRGBA(gb1.z).rgb;
	vec3 pos = recoverEyePos(depth);
	vec3 nrm = recoverEyeNrm(gb1.xy);
	
	// if (-gb1.w < zNear || -gb1.w > zFar) return;
	
	if (displayMode == 0)
	{
		outColor = finalColor;
	}
	else if (displayMode == 1)
	{
		outColor = vec4(albedo, 1.0);
	}
	else if (displayMode == 2)
	{
		outColor = vec4(nrm, 1.0);
	}
	else if (displayMode == 3)
	{
		outColor = vec4(abs(pos), 1.0);
	}
	else if (displayMode == 4)
	{
		outColor = vec4(vec3(depth), 1.0);
	}
}

