#define TINYGLTF_LOADER_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "deferred_renderer.h"

#ifdef USE_GLTF
std::string GLTF_VERSION;
std::string GLTF_NAME;
#endif

int main(int argc, char *argv[])
{
#ifdef USE_GLTF
	if (argc < 2 || (argc > 2 && strcmp(argv[1], "--gltf_version") != 0))
	{
		throw std::runtime_error("glTF file name required");
	}
	
	GLTF_VERSION = argc > 2 ? argv[2] : "2.0";
	GLTF_NAME = argv[argc - 1];
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