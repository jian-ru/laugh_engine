#version 450

#extension GL_ARB_separate_shader_objects : enable

#define MAT_ID_HDR_PROBE 0
#define MAT_ID_FSCHLICK_DGGX_GSMITH 1

layout (input_attachment_index = 0, set = 0, binding = 1) uniform subpassInput siGbuffer1;
layout (input_attachment_index = 1, set = 0, binding = 2) uniform subpassInput siGbuffer2;
layout (input_attachment_index = 2, set = 0, binding = 3) uniform subpassInput siDepthImage;

layout (set = 0, binding = 4) uniform samplerCube diffuseIrradianceMap;
layout (set = 0, binding = 5) uniform samplerCube specularIrradianceMap;
layout (set = 0, binding = 6) uniform sampler2D brdfLuts[1];

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

layout (constant_id = 0) const int NUM_LIGHTS = 32;

struct Light
{
	vec4 position;
	vec3 color;
	float radius;
};

layout (std140, set = 0, binding = 0) uniform UBO 
{
	vec4 eyePos;
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

vec3 recoverWorldNrm(vec2 xy)
{
	return vec3(xy, sqrt(1.0 - max(0.0, dot(xy, xy))));
}


void main() 
{
	vec4 gb1 = subpassLoad(siGbuffer1);
	vec4 gb2 = subpassLoad(siGbuffer2);
	float depth = subpassLoad(siDepthImage).r;
	
	if (depth >= 1.0 || depth <= 0.0) return;
	
	vec3 pos = gb2.xyz;
	vec3 nrm = recoverWorldNrm(gb1.xy);
	vec3 albedo = unpackRGBA(gb1.z).rgb;
	albedo = pow(albedo, vec3(2.2)); // to linear space
	
	vec4 RMI = unpackRGBA(gb1.w); // (roughness, metalness, material id, unused)
	float roughness = clamp(RMI.x, 0.0, 1.0);
	float metalness = clamp(RMI.y, 0.0, 1.0);
	uint matId = uint(round(RMI.z * 255.0));
	
	vec3 finalColor = vec3(0.0);

	if (matId == MAT_ID_HDR_PROBE)
	{
		finalColor = gb1.rgb;
	}
	else if (matId == MAT_ID_FSCHLICK_DGGX_GSMITH)
	{
		// for (int i = 0; i < NUM_LIGHTS; ++i)
		// {
			// vec3 lightPos = pointLights[i].position.xyz;
			
			// float dist = distance(lightPos, pos);
			
			// if (dist < pointLights[i].radius)
			// {
				// float scale = (pointLights[i].radius - dist) / pointLights[i].radius;
				// vec3 v = -pos;
				// vec3 l = lightPos - pos;
				// vec3 h = normalize(v + l);
				
				// // assume specular color is white here
				// finalColor +=
					// (0.5 * albedo * clamp(dot(nrm, l), 0.0, 1.0) +
					// 0.5 * pow(clamp(dot(nrm, h), 0.0, 1.0), 20.0)) *
					// pointLights[i].color * scale;
			// }
		// }
		
		vec3 v = normalize(eyePos.xyz - pos);
		vec3 r = normalize(reflect(-v, nrm));
		
		vec3 diffIr = textureLod(diffuseIrradianceMap, nrm, 0).rgb / 3.1415926;
		
		float specMipLevel = roughness * float(pcs.totalMipLevels - 1);
		vec3 specIr = textureLod(specularIrradianceMap, r, specMipLevel).rgb;
		
		float NoV = clamp(dot(nrm, v), 0.0, 1.0);
		vec2 brdfTerm = textureLod(brdfLuts[0], vec2(NoV, clamp(1.0 - roughness, 0.0, 1.0)), 0).rg;
		
		const vec3 dielectricF0 = vec3(0.04);
		vec3 diffColor = albedo * (1.0 - metalness); // if it is metal, no diffuse color
		vec3 specColor = mix(dielectricF0, albedo, metalness); // since metal has no albedo, we use the space to store its F0
		
		// finalColor = diffColor * diffIr + specIr * (specColor * brdfTerm.x + brdfTerm.y);
		// finalColor = nrm;
		// finalColor = abs(pos);
		// finalColor = eyePos.xyz;
		finalColor = vec3(brdfTerm, 0.0);
	}
	
	outColor = vec4(finalColor, 1.0);
}