#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/hash.hpp"

#define PI 3.14159265358979323846f

class Camera
{
public:
	const float thetaLimit = .1f * PI;
	const float minDistance = .1f;

	float fovy;
	float aspectRatio;
	float zNear, zFar;

	glm::vec3 position;
	glm::vec3 lookAtPos;
	glm::vec2 phiTheta; // azimuth and zenith angles

	Camera(
		const glm::vec3 position,
		const glm::vec3 lookAtPos,
		float fovy, float aspect,
		float zNear, float zFar)
		:
		position(position), lookAtPos(lookAtPos),
		fovy(fovy), aspectRatio(aspect),
		zNear(zNear), zFar(zFar)
	{
		glm::vec3 dir = glm::normalize(position - lookAtPos);
		float theta = acosf(fmax(-1.f, fmin(1.f, dir.y)));
		float phi = atan2f(dir.x, dir.z);
		theta = fmax(thetaLimit, fmin(PI - thetaLimit, theta));
		phiTheta = glm::vec2(phi, theta);

		float dist = glm::distance(position, lookAtPos);
		glm::vec3 newDir = glm::vec3(sinf(phi) * sinf(theta), cosf(theta), cosf(phi) * sinf(theta));
		this->position = this->lookAtPos + newDir * dist;
	}

	void getViewProjMatrix(glm::mat4 &V, glm::mat4 &P)
	{
		V = glm::lookAt(position, lookAtPos, glm::vec3(0.f, 1.f, 0.f));
		P = glm::perspective(fovy, aspectRatio, zNear, zFar);
		P[1][1] *= -1.f; // the y-axis of clip space in Vulkan is pointing down in contrary to OpenGL
	}

	void addRotation(float phi, float theta)
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

	void addPan(float x, float y)
	{
		glm::vec3 view = glm::normalize(glm::vec3(lookAtPos - position));
		glm::vec3 up(0.f, 1.f, 0.f);
		up = glm::normalize(up - view * glm::dot(view, up));
		glm::vec3 right = glm::normalize(glm::cross(view, up));

		position += x * right + y * up;
		lookAtPos += x * right + y * up;
	}

	void addZoom(float d)
	{
		glm::vec3 view = glm::normalize(glm::vec3(lookAtPos - position));
		float dist = glm::distance(lookAtPos, position);
		d = fmin(d, dist - minDistance);
		position += d * view;
	}
};

#undef PI