#define VBASE_IMPLEMENTATION
#define DEFERED_RENDERER_IMPLEMENTATION
#include "deferred_renderer.h"

//class Test
//{
//public:
//	Test(int _x = 0) : x(_x)
//	{
//		std::cout << "c" << x << std::endl;
//	}
//
//	Test(Test &&other)
//	{
//		x = other.x;
//		std::cout << "mc" << x << std::endl;
//	}
//
//	Test &operator=(Test &&other)
//	{
//		x = other.x;
//		std::cout << "ma" << x << std::endl;
//		return *this;
//	}
//
//	Test(Test &other)
//	{
//		x = other.x;
//		other.x = -99;
//		std::cout << "cc" << x << std::endl;
//	}
//
//	Test &operator=(const Test &other)
//	{
//		x = other.x;
//		std::cout << "ca" << x << std::endl;
//		return *this;
//	}
//
//	~Test()
//	{
//		std::cout << "d" << x << std::endl;
//	}
//
//	int x;
//};

int main()
{
	//std::vector<Test> tests;
	//tests.emplace_back(1);
	//tests.emplace_back(2);
	//tests.emplace_back(3);

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