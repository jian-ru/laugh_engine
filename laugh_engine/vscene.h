#pragma once

#include "vmesh.h"
#include "directional_light.h"


class VScene
{
public:
	Skybox skybox;
	DirectionalLight shadowLight;
	std::vector<VMesh> meshes;

	BBox aabbWorldSpace;

	VScene(rj::VManager *pManager);

	void computeAABBWorldSpace();
};