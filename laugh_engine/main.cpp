#define TINYGLTF_LOADER_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define VBASE_IMPLEMENTATION
#define DEFERED_RENDERER_IMPLEMENTATION
#include "deferred_renderer.h"

#ifdef USE_GLTF
std::string GLTF_NAME;
#endif

int main(int argc, char *argv[])
{
#ifdef USE_GLTF
	if (argc < 2) throw std::runtime_error("glTF file name required");
	GLTF_NAME = argv[1];
#endif

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