#include "directional_light.h"
#include "glm/gtc/matrix_transform.hpp"


DirectionalLight::DirectionalLight(
	const glm::vec3 &pos, const glm::vec3 &dir,
	const glm::vec3 &clr, bool castShadow)
	:
	position(pos), direction(glm::normalize(dir)),
	color(clr), bCastShadow(castShadow)
{
	auto lookAtPos = position + direction;
	V = glm::lookAt(position, lookAtPos, glm::vec3(0.f, 1.f, 0.f));
}

void DirectionalLight::computeProjMatrix(const glm::vec3 &bboxMin, const glm::vec3 &bboxMax)
{
	P = glm::mat4();
	P[0][0] = 2.f / (bboxMax.x - bboxMin.x);
	P[1][1] = 2.f / (bboxMin.y - bboxMax.y);
	P[2][2] = 1.f / bboxMin.z;
	P[3][0] = -(bboxMax.x + bboxMin.x) / (bboxMax.x - bboxMin.x);
	P[3][1] = (bboxMax.y + bboxMin.y) / (bboxMin.y - bboxMax.y);
}

void DirectionalLight::computeCropMatrix(const glm::vec3 &bboxMin, const glm::vec3 &bboxMax, glm::mat4 *cropMat) const
{
	assert(cropMat);
	float segmentSizeX = bboxMax.x - bboxMin.x;
	float segmentSizeY = bboxMax.y - bboxMin.y;
	float segmentSize = fmax(segmentSizeX, segmentSizeY);
	
	auto &M = *cropMat;
	M = glm::mat4();
	M[0][0] = 2.f / segmentSize;
	M[1][1] = 2.f / segmentSize;
	M[2][2] = 1.f / bboxMax.z;
	M[3][0] = -(bboxMin.x + segmentSize + bboxMin.x) / segmentSize;
	M[3][1] = -(bboxMin.y + segmentSize + bboxMin.y) / segmentSize;
}
