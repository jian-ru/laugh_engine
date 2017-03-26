#include "directional_light.h"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/norm.hpp"


DirectionalLight::DirectionalLight()
	: DirectionalLight(glm::vec3(0.f), glm::vec3(-1.f), glm::vec3(1.f))
{
}

DirectionalLight::DirectionalLight(
	const glm::vec3 &pos, const glm::vec3 &dir,
	const glm::vec3 &clr, bool castShadow, uint32_t pcfSize)
	:
	position(pos), direction(glm::normalize(dir)),
	color(clr), bCastShadow(castShadow), pcfKernelSize(pcfSize)
{
	recomputeViewMatrix();
}

void DirectionalLight::computeCascadeScalesAndOffsets(
	const std::vector<glm::vec3>& frustumCorners, const std::vector<float> &cascadeDepths,
	const glm::vec3 &sceneAABBMin, const glm::vec3 &sceneAABBMax, uint32_t shadowMapDim)
{
	assert(frustumCorners.size() >= 8 && (frustumCorners.size() - 4) % 4 == 0);
	
	const float fShadowMapDim = static_cast<float>(shadowMapDim);
	const uint32_t cascadeCount = (frustumCorners.size() - 4) >> 2;
	cascadeScales.resize(cascadeCount);
	cascadeOffsets.resize(cascadeCount);
	assert(cascadeDepths.size() == cascadeCount);

	glm::vec4 worldSpaceSceneAABBCorners[8] =
	{
		glm::vec4(sceneAABBMax.x, sceneAABBMax.y, sceneAABBMin.z, 1.f),
		glm::vec4(sceneAABBMin.x, sceneAABBMax.y, sceneAABBMin.z, 1.f),
		glm::vec4(sceneAABBMin.x, sceneAABBMin.y, sceneAABBMin.z, 1.f),
		glm::vec4(sceneAABBMax.x, sceneAABBMin.y, sceneAABBMin.z, 1.f),
		glm::vec4(sceneAABBMax.x, sceneAABBMax.y, sceneAABBMax.z, 1.f),
		glm::vec4(sceneAABBMin.x, sceneAABBMax.y, sceneAABBMax.z, 1.f),
		glm::vec4(sceneAABBMin.x, sceneAABBMin.y, sceneAABBMax.z, 1.f),
		glm::vec4(sceneAABBMax.x, sceneAABBMin.y, sceneAABBMax.z, 1.f)
	};

	// Make sure near plane doesn't clip shadow casters
	float fZNear = -std::numeric_limits<float>::max();
	for (uint32_t i = 0; i < 8; ++i)
	{
		auto sceneAABBCornerLVS = V * worldSpaceSceneAABBCorners[i];
		fZNear = fmax(fZNear, sceneAABBCornerLVS.z);
	}

	for (uint32_t cascadeIdx = 0; cascadeIdx < cascadeCount; ++cascadeIdx)
	{
		// Define a AABB of the current cascade in light's view space
		glm::vec3 lightViewSpaceMin(std::numeric_limits<float>::max());
		glm::vec3 lightViewSpaceMax(-std::numeric_limits<float>::max());

		for (uint32_t cornerIdx = 0; cornerIdx < 8; ++cornerIdx)
		{
			auto lightViewSpaceCorner = glm::vec3(V * glm::vec4(frustumCorners[4 * cascadeIdx + cornerIdx], 1.f));
			lightViewSpaceMin = glm::min(lightViewSpaceMin, lightViewSpaceCorner);
			lightViewSpaceMax = glm::max(lightViewSpaceMax, lightViewSpaceCorner);
		}

		// View transform is only rotational and translational so it doesn't change distances.
		float cascadeDepth = cascadeDepths[cascadeIdx];
		float diagLen = glm::distance2(frustumCorners[4 * cascadeIdx + 4], frustumCorners[4 * cascadeIdx + 6]);
		diagLen = std::sqrt(diagLen + cascadeDepth * cascadeDepth);

		// Pad X and Y so that the size of the cascade orthographic frustum is fixed when
		// camera moves
		float paddingX = (diagLen - (lightViewSpaceMax.x - lightViewSpaceMin.x)) * 0.5f;
		float paddingY = (diagLen - (lightViewSpaceMax.y - lightViewSpaceMin.y)) * 0.5f;
		assert(paddingX >= 0.f && paddingY >= 0.f);
		glm::vec3 padding(paddingX, paddingY, 0.f);
		lightViewSpaceMin -= padding;
		lightViewSpaceMax += padding;

		// Pad X and Y so that PCF kernel will not sample outside of shadow maps
		float pcfPaddingTexelCount = static_cast<float>(pcfKernelSize >> 1);
		glm::vec2 worldUnitsPerTexel = glm::vec2(lightViewSpaceMax - lightViewSpaceMin);
		worldUnitsPerTexel /= fShadowMapDim - 2.f * pcfPaddingTexelCount;

		padding = glm::vec3(worldUnitsPerTexel * pcfPaddingTexelCount, 0.f);
		lightViewSpaceMin -= padding;
		lightViewSpaceMax += padding;

		// Snap the orthographic frustum to pixel boundaries so that shadow edges will not shimmer
		lightViewSpaceMin = glm::vec3(
			glm::floor(glm::vec2(lightViewSpaceMin) / worldUnitsPerTexel) * worldUnitsPerTexel,
			lightViewSpaceMin.z);
		lightViewSpaceMax = glm::vec3(
			glm::floor(glm::vec2(lightViewSpaceMax) / worldUnitsPerTexel) * worldUnitsPerTexel,
			fZNear);

		auto &scale = cascadeScales[cascadeIdx];
		auto &offset = cascadeOffsets[cascadeIdx];
		scale.x = 2.f / (lightViewSpaceMax.x - lightViewSpaceMin.x);
		scale.y = -2.f / (lightViewSpaceMax.y - lightViewSpaceMin.y);
		scale.z = -1.f / (lightViewSpaceMax.z - lightViewSpaceMin.z);
		offset.x = -0.5f * (lightViewSpaceMax.x + lightViewSpaceMin.x) * scale.x;
		offset.y = -0.5f * (lightViewSpaceMax.y + lightViewSpaceMin.y) * scale.y;
		offset.z = -lightViewSpaceMax.z * scale.z;
	}
}

void DirectionalLight::setPosition(const glm::vec3 &newPos)
{
	setPositionAndDirection(newPos, direction);
}

void DirectionalLight::setDirection(const glm::vec3 &newDir)
{
	setPositionAndDirection(position, newDir);
}

void DirectionalLight::setPositionAndDirection(const glm::vec3 &newPos, const glm::vec3 &newDir)
{
	position = newPos;
	direction = newDir;
	recomputeViewMatrix();
}

void DirectionalLight::setColor(const glm::vec3 &newClr)
{
	color = newClr;
}

void DirectionalLight::setCastShadow(bool newState)
{
	bCastShadow = newState;
}

void DirectionalLight::setPCFKernelSize(uint32_t newSize)
{
	pcfKernelSize = newSize;
}

void DirectionalLight::getCascadeViewProjMatrix(uint32_t cascadeIdx, glm::mat4 *lightVP) const
{
	assert(cascadeIdx < cascadeScales.size());

	const auto &scale = cascadeScales[cascadeIdx];
	const auto &offset = cascadeOffsets[cascadeIdx];
	glm::mat4 P;
	P[0][0] = scale.x;
	P[1][1] = scale.y;
	P[2][2] = scale.z;
	P[3][0] = offset.x;
	P[3][1] = offset.y;
	P[3][2] = offset.z;
	*lightVP = P * V;
}

void DirectionalLight::recomputeViewMatrix()
{
	auto lookAtPos = position + direction;
	auto viewDir = glm::normalize(lookAtPos - position);
	glm::vec3 up = 1.f - std::abs(viewDir.y) < 1e-6f ? glm::vec3(1.f, 0.f, 0.f) : glm::vec3(0.f, 1.f, 0.f);
	V = glm::lookAt(position, lookAtPos, up);
}
