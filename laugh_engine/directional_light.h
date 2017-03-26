#pragma once

#include <vector>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"


class DirectionalLight
{
public:
	DirectionalLight();

	DirectionalLight(
		const glm::vec3 &pos, const glm::vec3 &dir,
		const glm::vec3 &clr, bool castShadow = false, uint32_t pcfSize = 3);

	// @frustumCorners, @sceneAABBMin, and @sceneAABBMax are in world space
	// In @frustumCorners, corners of a far plane are reused as corners of the near
	// plane of the next cascade.
	void computeCascadeScalesAndOffsets(
		const std::vector<glm::vec3> &frustumCorners, const std::vector<float> &cascadeDepths,
		const glm::vec3 &sceneAABBMin, const glm::vec3 &sceneAABBMax, uint32_t shadowMapDim);

	void setPosition(const glm::vec3 &newPos);
	void setDirection(const glm::vec3 &newDir);
	void setPositionAndDirection(const glm::vec3 &newPos, const glm::vec3 &newDir);
	void setColor(const glm::vec3 &newClr);
	void setCastShadow(bool newState);
	void setPCFKernelSize(uint32_t newSize);

	const glm::mat4 &getViewMatrix() const { return V; }
	void getCascadeViewProjMatrix(uint32_t cascadeIdx, glm::mat4 *lightVP) const;
	const glm::vec3 &getColor() const { return color; }
	const glm::vec3 &getDirection() const { return direction; }
	bool castShadow() const { return bCastShadow; }
	uint32_t getPCFKernlSize() const { return pcfKernelSize; }

protected:
	glm::vec3 position;
	glm::vec3 direction;
	glm::mat4 V;
	glm::vec3 color;
	bool bCastShadow;
	uint32_t pcfKernelSize;

	std::vector<glm::vec3> cascadeScales;
	std::vector<glm::vec3> cascadeOffsets;

	void recomputeViewMatrix();
};