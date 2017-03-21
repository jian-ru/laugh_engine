#pragma once

#include <vector>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"

#define CSM_MAX_SEG_COUNT 4

class Camera
{
public:
	Camera(
		const glm::vec3 position,
		const glm::vec3 lookAtPos,
		float fovy, float aspect,
		float zNear, float zFar,
		uint32_t segCount = 3);

	void getViewProjMatrix(glm::mat4 &V, glm::mat4 &P) const;
	
	const glm::vec3 &getPosition() const { return position; }
	uint32_t getSegmentCount() const { return segmentCount; }
	float getNormFarPlaneZ(uint32_t segIdx) const { return normFarPlaneZs[segIdx]; }

	void computeFrustumBBox(const glm::mat4 &worldToX, glm::vec3 *pMin, glm::vec3 *pMax) const;
	void computeSegmentBBox(uint32_t segIdx, const glm::mat4 &worldToX, glm::vec3 *pMin, glm::vec3 *pMax) const;

	void addRotation(float phi, float theta);
	void addPan(float x, float y);
	void addZoom(float d);

	void setAspectRatio(float aspect) { aspectRatio = aspect; }

protected:
	const float thetaLimit;
	const float minDistance;

	float fovy;
	float aspectRatio;
	float zNear, zFar;

	glm::vec3 position;
	glm::vec3 lookAtPos;
	glm::vec2 phiTheta; // azimuth and zenith angles

	uint32_t segmentCount;
	float farPlaneZs[CSM_MAX_SEG_COUNT];
	float normFarPlaneZs[CSM_MAX_SEG_COUNT];

	void getCornersWorldSpace(float zNear, float zFar, std::vector<glm::vec4> *corners) const;
};
