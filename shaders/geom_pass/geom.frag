#version 450

#extension GL_ARB_separate_shader_objects : enable

layout (binding = 1) uniform sampler2D samplerAlbedo;
layout (binding = 2) uniform sampler2D samplerNormal;

layout (location = 0) in vec3 inEyePos;
layout (location = 1) in vec3 inEyeNormal;
layout (location = 2) in vec3 inEyeTangent;
layout (location = 3) in vec2 inTexcoord;

layout (location = 0) out vec4 outGbuffer1;


float packRGBA(vec4 color)
{
	uvec4 rgba = uvec4(color * 255.0 + 0.49);
	return float((rgba.r << 24) | (rgba.g << 16) | (rgba.b << 8) | rgba.a);
}


vec3 applyNormalMap(vec3 surfnrm, vec3 surftan, vec3 nrmmap)
{
    vec3 surfbinrm = cross(surfnrm, surftan);
	nrmmap = nrmmap * 2.0 - 1.0;
    return normalize(nrmmap.x * surftan + nrmmap.y * surfbinrm + nrmmap.z * surfnrm);
}


void main() 
{
	vec4 albedo = texture(samplerAlbedo, inTexcoord);
	vec3 nrmmap = texture(samplerNormal, inTexcoord).rgb;

	vec3 surfnrm = normalize(inEyeNormal);
	vec3 surftan = normalize(inEyeTangent - surfnrm * dot(surfnrm, inEyeTangent));
	
	float packedAlbedo = packRGBA(albedo);
	vec3 nrm = applyNormalMap(surfnrm, surftan, nrmmap);
	
	outGbuffer1 = vec4(nrm.xy, packedAlbedo, 0.0);
	// outGbuffer1 = vec4(albedo.rgb, 1.0);
	// outGbuffer1 = vec4(nrm, 1.0);
	// outGbuffer1 = vec4(inEyePos, 1.0);
}