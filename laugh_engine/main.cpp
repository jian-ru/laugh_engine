#define TINYGLTF_LOADER_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define VBASE_IMPLEMENTATION
#define DEFERED_RENDERER_IMPLEMENTATION
#include "deferred_renderer.h"


int main()
{
	//tinygltf::Scene scene;
	//tinygltf::TinyGLTFLoader loader;
	//std::string err;

	//bool ret = loader.LoadASCIIFromFile(&scene, &err, "../assets/microphone/Microphone.gltf");

	//if (!err.empty())
	//{
	//	printf("Err: %s\n", err.c_str());
	//}

	//if (!ret)
	//{
	//	printf("Failed to parse glTF\n");
	//	return -1;
	//}

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