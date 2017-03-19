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

void Camera::getCornersWorldSpace(float zNear, float zFar, std::vector<glm::vec4> *corners) const
{
	auto f = glm::normalize(lookAtPos - position);
	auto r = glm::normalize(glm::cross(f, glm::vec3(0.f, 1.f, 0.f)));
	auto u = glm::cross(r, f);

	const float tanHalfFovy = tanf(fovy * 0.5f);
	auto fNear = zNear * f;
	auto uNear = zNear * tanHalfFovy * u;
	auto rNear = zNear * tanHalfFovy * aspectRatio * r;
	auto fFar = zFar * f;
	auto uFar = zFar * tanHalfFovy * u;
	auto rFar = zFar * tanHalfFovy * aspectRatio * r;

	*corners =
	{
		glm::vec4(position + fNear + uNear + rNear, 1.f),
		glm::vec4(position + fNear + uNear - rNear, 1.f),
		glm::vec4(position + fNear - uNear - rNear, 1.f),
		glm::vec4(position + fNear - uNear + rNear, 1.f),
		glm::vec4(position + fFar + uFar + rFar, 1.f),
		glm::vec4(position + fFar + uFar - rFar, 1.f),
		glm::vec4(position + fFar - uFar - rFar, 1.f),
		glm::vec4(position + fFar - uFar + rFar, 1.f)
	};
}

void Camera::computeFrustumBBox(const glm::mat4 &worldToX, glm::vec3 *pMin, glm::vec3 *pMax) const
{
	assert(pMin && pMax);

	std::vector<glm::vec4> corners;
	getCornersWorldSpace(zNear, zFar, &corners);

	*pMin = glm::vec3(std::numeric_limits<float>::max());
	*pMax = glm::vec3(-std::numeric_limits<float>::max());

	for (const auto &c : corners)
	{
		auto corner = worldToX * c;
		corner /= corner.w;
		bboxUnion(glm::vec3(corner), pMin, pMax);
	}
}

void Camera::computeSegmentBBox(uint32_t segIdx, const glm::mat4 &worldToX, glm::vec3 *pMin, glm::vec3 *pMax) const
{
	assert(pMin && pMax);
	assert(segIdx < segmentCount);

	const float n = segIdx == 0 ? 0.f : -farPlaneZs[segIdx - 1];
	const float f = -farPlaneZs[segIdx];
	std::vector<glm::vec4> corners;
	getCornersWorldSpace(n, f, &corners);

	*pMin = glm::vec3(std::numeric_limits<float>::max());
	*pMax = glm::vec3(-std::numeric_limits<float>::max());

	for (const auto &c : corners)
	{
		auto corner = worldToX * c;
		corner /= corner.w;
		bboxUnion(glm::vec3(corner), pMin, pMax);
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
