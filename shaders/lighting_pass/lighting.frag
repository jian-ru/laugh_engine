#version 450

#extension GL_ARB_separate_shader_objects : enable

layout (input_attachment_index = 0, binding = 2) uniform subpassInput siGbuffer1;
layout (input_attachment_index = 1, binding = 3) uniform subpassInput siDepthImage;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

layout (constant_id = 0) const int NUM_LIGHTS = 32;

struct Light
{
	vec4 position;
	vec3 color;
	float radius;
};

layout (std140, binding = 0) uniform UBO 
{
	Light lights[NUM_LIGHTS];
} ubo;

layout (std140, binding = 1) uniform UBO2
{
	int displayMode; // not used
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
	vec4 gb1 = subpassLoad(siGbuffer1);
	float depth = subpassLoad(siDepthImage).r;
	
	vec3 pos = recoverEyePos(depth);
	vec3 nrm = recoverEyeNrm(gb1.xy);
	vec3 albedo = unpackRGBA(gb1.z).rgb;
	
	// float depth = (100.0 / -99.9 * gb1.w - 10.0 / 99.9) / -gb1.w;
	// outColor = vec4(depth, depth, depth, 1.0);
	// return;
	
	if (-gb1.w > 0.1 && -gb1.w < 100.0)
	{
		vec3 finalColor = vec3(0.0);
	
		for (int i = 0; i < NUM_LIGHTS; ++i)
		{
			vec3 lightPos = ubo.lights[i].position.xyz;
			
			float dist = distance(lightPos, pos);
			
			if (dist < ubo.lights[i].radius)
			{
				float scale = (ubo.lights[i].radius - dist) / ubo.lights[i].radius;
				vec3 v = -pos;
				vec3 l = lightPos - pos;
				vec3 h = normalize(v + l);
				
				finalColor +=
					0.5 * albedo * clamp(dot(nrm, l), 0.0, 1.0) +
					0.5 * ubo.lights[i].color * pow(clamp(dot(nrm, h), 0.0, 1.0), 20.0);
			}
		}
		
		outColor = vec4(finalColor, 1.0);
		// outColor = vec4(nrm, 1.0);
		// outColor = vec4(albedo, 1.0);
		// outColor = gb1;
		// outColor = vec4(abs(pos), 1.0);
		// outColor = vec4(ubo.eyePos.xyz / 3.0, 1.0);
	}
}