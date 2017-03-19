#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"


class DirectionalLight
{
public:
	DirectionalLight(
		const glm::vec3 &pos, const glm::vec3 &dir,
		const glm::vec3 &clr, bool castShadow = false);

	// bboxMin and bboxMax are in the light's view space
	void computeProjMatrix(const glm::vec3 &bboxMin, const glm::vec3 &bboxMax);

	// bboxMin and bboxMax define a bounding box of a view frustum segment in the
	// light's clip space (normalized)
	void computeCropMatrix(const glm::vec3 &bboxMin, const glm::vec3 &bboxMax, glm::mat4 *cropMat) const;

	const glm::mat4 &getViewMatrix() const { return V; }
	const glm::mat4 &getProjMatrix() const { return P; }
	const glm::vec3 &getColor() const { return color; }
	bool castShadow() const { return bCastShadow; }

protected:
	glm::vec3 position;
	glm::vec3 direction;
	glm::mat4 V, P;

	glm::vec3 color;

	bool bCastShadow;
};