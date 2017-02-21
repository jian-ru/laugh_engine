#define TINYGLTF_LOADER_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define VBASE_IMPLEMENTATION
#define DEFERED_RENDERER_IMPLEMENTATION
#include "deferred_renderer.h"

#include "gltf_loader.h"

int main()
{
	rj::GLTFLoader loader;
	rj::GLTFScene scene;
	loader.load(&scene, "../assets/avocado/Avocado.gltf");

	DeferredRenderer renderer;

	try
	{
		renderer.run();
	}
	catch (const std::runtime_error& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}