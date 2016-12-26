#define VBASE_IMPLEMENTATION
#define DEFERED_RENDERER_IMPLEMENTATION
#include "deferred_renderer.h"

int main()
{
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