#include "vscene.h"


VScene::VScene(rj::VManager *pManager)
	: skybox(pManager)
{
}

void VScene::computeAABBWorldSpace()
{
	aabbWorldSpace = BBox();
	for (const auto &mesh : meshes)
	{
		auto meshAABB = mesh.getAABBWorldSpace();
		aabbWorldSpace.min = glm::min(aabbWorldSpace.min, meshAABB.min);
		aabbWorldSpace.max = glm::max(aabbWorldSpace.max, meshAABB.max);
	}
}
