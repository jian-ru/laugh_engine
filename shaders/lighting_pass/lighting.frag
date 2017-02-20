#version 450

#extension GL_ARB_separate_shader_objects : enable

#define MAT_ID_HDR_PROBE 0
#define MAT_ID_FSCHLICK_DGGX_GSMITH 1

layout (set = 0, binding = 1) uniform sampler2DMS gbuffer1;
layout (set = 0, binding = 2) uniform sampler2DMS gbuffer2;
layout (set = 0, binding = 3) uniform sampler2DMS gbuffer3;
layout (set = 0, binding = 4) uniform sampler2DMS depthImage;

layout (set = 0, binding = 5) uniform samplerCube diffuseIrradianceMap;
layout (set = 0, binding = 6) uniform samplerCube specularIrradianceMap;
layout (set = 0, binding = 7) uniform sampler2D brdfLuts[1];

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

layout (constant_id = 0) const int NUM_LIGHTS = 32;
layout (constant_id = 1) const int NUM_SAMPLES = 4;

struct Light
{
	vec4 position;
	vec3 color;
	float radius;
};

layout (std140, set = 0, binding = 0) uniform UBO 
{
	vec3 eyePos;
	float emissiveStrength;
	Light pointLights[NUM_LIGHTS];
};

layout (push_constant) uniform pushConstants
{
	uint totalMipLevels;
} pcs;


vec4 unpackRGBA(float packedColor)
{
	uint tmp = uint(packedColor);	
	return vec4((tmp >> 24) & 0xff, (tmp >> 16) & 0xff, (tmp >> 8) & 0xff, tmp & 0xff) 
		* 0.0039215686274509803921568627451;
}


void main() 
{
	const ivec2 extent = textureSize(gbuffer1);
	const ivec2 pixelPos = ivec2(extent * inUV);
	
	vec3 finalColor = vec3(0.0);
	
	for (int i = 0; i < NUM_SAMPLES; ++i)
	{	
		float depth = texelFetch(depthImage, pixelPos, i).r;
		if (depth >= 1.0 || depth <= 0.0) continue;
		
		vec4 gb1 = texelFetch(gbuffer1, pixelPos, i);
		vec4 gb2 = texelFetch(gbuffer2, pixelPos, i);
		vec4 RMAI = texelFetch(gbuffer3, pixelPos, i); // (roughness, metalness, AO, material id)
		
		vec3 pos = gb2.xyz;
		float emissiveness = gb2.w;
		vec3 nrm = gb1.xyz;
		vec3 albedo = unpackRGBA(gb1.w).rgb;
		albedo = pow(albedo, vec3(2.2)); // to linear space
		
		float roughness = clamp(RMAI.x, 0.0, 1.0);
		float metalness = clamp(RMAI.y, 0.0, 1.0);
		float aoVal = RMAI.z;
		uint matId = uint(round(RMAI.w * 255.0));
		
		if (matId == MAT_ID_HDR_PROBE)
		{
			finalColor += gb1.rgb;
		}
		else if (matId == MAT_ID_FSCHLICK_DGGX_GSMITH)
		{
			// PBR
			vec3 v = normalize(eyePos.xyz - pos);
			vec3 r = normalize(reflect(-v, nrm));
			
			vec3 diffIr = textureLod(diffuseIrradianceMap, nrm, 0).rgb / 3.1415926;
			
			float specMipLevel = roughness * float(pcs.totalMipLevels - 1);
			vec3 specIr = textureLod(specularIrradianceMap, r, specMipLevel).rgb;
			
			float NoV = clamp(dot(nrm, v), 0.0, 1.0);
			vec2 brdfTerm = textureLod(brdfLuts[0], vec2(NoV, clamp(roughness, 0.0, 1.0)), 0).rg;
			
			const vec3 dielectricF0 = vec3(0.04);
			vec3 diffColor = albedo * (1.0 - metalness); // if it is metal, no diffuse color
			vec3 specColor = mix(dielectricF0, albedo, metalness); // since metal has no albedo, we use the space to store its F0
			
			vec3 result = diffColor * diffIr + specIr * (specColor * brdfTerm.x + brdfTerm.y);
			result *= aoVal;
			result += albedo * emissiveness * emissiveStrength;
			finalColor += result;
		}
	}
	
	finalColor /= float(NUM_SAMPLES);
	outColor = vec4(finalColor, 1.0);
}