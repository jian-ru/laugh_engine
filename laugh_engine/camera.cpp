#include "camera.h"
#include "glm/gtc/matrix_transform.hpp"

#define PI 3.14159265358979323846f


Camera::Camera(
	const glm::vec3 position,
	const glm::vec3 lookAtPos,
	float fovy, float aspect,
	float zNear, float zFar,
	uint32_t segCount)
	:
	thetaLimit(.1f * PI), minDistance(.1f),
	fovy(fovy), aspectRatio(aspect),
	zNear(zNear), zFar(zFar),
	position(position), lookAtPos(lookAtPos),
	segmentCount(segCount)
{
	glm::vec3 dir = glm::normalize(position - lookAtPos);
	float theta = acosf(fmax(-1.f, fmin(1.f, dir.y)));
	float phi = atan2f(dir.x, dir.z);
	theta = fmax(thetaLimit, fmin(PI - thetaLimit, theta));
	phiTheta = glm::vec2(phi, theta);

	float dist = glm::distance(position, lookAtPos);
	glm::vec3 newDir = glm::vec3(sinf(phi) * sinf(theta), cosf(theta), cosf(phi) * sinf(theta));
	this->position = this->lookAtPos + newDir * dist;

	// CSM related
	memset(farPlaneZs, 0, sizeof(farPlaneZs));
	memset(normFarPlaneZs, 0, sizeof(normFarPlaneZs));

	const float lambda = 0.5f;
	glm::mat4 P = glm::perspective(fovy, aspectRatio, zNear, zFar);
	
	for (uint32_t i = 1; i <= segmentCount; ++i)
	{
		float frac = float(i) / segmentCount;
		float logSplit = zNear * std::pow(zFar / zNear, frac);
		float uniSplit = zNear + (zFar - zNear) * frac;
		float splitDepth = (1.f - lambda) * uniSplit + lambda * logSplit;
		splitDepth = fmax(zNear, fmin(zFar, splitDepth));
		
		farPlaneZs[i - 1] = -splitDepth;
		float projectedDepth = (P[2][2] * -splitDepth + P[3][2]) / (P[2][3] * -splitDepth);
		projectedDepth = fmax(0.f, fmin(1.f, projectedDepth));
		normFarPlaneZs[i - 1] = projectedDepth;
	}
}

void Camera::getViewProjMatrix(glm::mat4 &V, glm::mat4 &P) const
{
	V = glm::lookAt(position, lookAtPos, glm::vec3(0.f, 1.f, 0.f));
	P = glm::perspective(fovy, aspectRatio, zNear, zFar);
	P[1][1] *= -1.f; // the y-axis of clip space in Vulkan is pointing down in contrary to OpenGL
}

inline void bboxUnion(const glm::vec3 &p, glm::vec3 *pMin, glm::vec3 *pMax)
{
	*pMin = glm::min(*pMin, p);
	*pMax = glm::max(*pMax, p);
}

void Camera::getSegmentDepths(std::vector<float> *segDepths) const
{
	assert(segDepths);
	segDepths->resize(segmentCount);
	for (int i = 0; i < segmentCount; ++i)
	{
		float nearClip = i == 0 ? zNear : -farPlaneZs[i - 1];
		float farClip = -farPlaneZs[i];
		(*segDepths)[i] = farClip - nearClip;
	}
}

void Camera::getCornersWorldSpace(std::vector<glm::vec3> *corners) const
{
	assert(corners);
	const auto f = glm::normalize(lookAtPos - position);
	const auto r = glm::normalize(glm::cross(f, glm::vec3(0.f, 1.f, 0.f)));
	const auto u = glm::cross(r, f);
	const float tanHalfFovy = tanf(fovy * 0.5f);

	corners->resize(segmentCount * 4 + 4);

	for (uint32_t i = 0; i <= segmentCount; ++i)
	{
		float depth = i == 0 ? zNear : -farPlaneZs[i - 1];
		auto df = depth * f;
		auto du = depth * tanHalfFovy * u;
		auto dr = depth * tanHalfFovy * aspectRatio * r;

		(*corners)[4 * i] = position + df + du + dr;
		(*corners)[4 * i + 1] = position + df + du - dr;
		(*corners)[4 * i + 2] = position + df - du - dr;
		(*corners)[4 * i + 3] = position + df - du + dr;
	}
}

void Camera::addRotation(float phi, float theta)
{
	float newPhi = phiTheta.x + phi;
	float newTheta = phiTheta.y + theta;

	while (newPhi > PI) newPhi -= 2.f * PI;
	while (newPhi < -PI) newPhi += 2.f * PI;
	newTheta = fmax(thetaLimit, fmin(PI - thetaLimit, newTheta));

	phiTheta = glm::vec2(newPhi, newTheta);

	glm::vec3 newDir = glm::vec3(sinf(newPhi) * sinf(newTheta), cosf(newTheta), cosf(newPhi) * sinf(newTheta));
	float dist = glm::distance(position, lookAtPos);
	position = lookAtPos + newDir * dist;
}

void Camera::addPan(float x, float y)
{
	glm::vec3 view = glm::normalize(glm::vec3(lookAtPos - position));
	glm::vec3 up(0.f, 1.f, 0.f);
	up = glm::normalize(up - view * glm::dot(view, up));
	glm::vec3 right = glm::normalize(glm::cross(view, up));

	position += x * right + y * up;
	lookAtPos += x * right + y * up;
}

void Camera::addZoom(float d)
{
	glm::vec3 view = glm::normalize(glm::vec3(lookAtPos - position));
	float dist = glm::distance(lookAtPos, position);
	d = fmin(d, dist - minDistance);
	position += d * view;
}
