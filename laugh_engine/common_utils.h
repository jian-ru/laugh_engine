#pragma once

#include <vector>
#include <fstream>
#include <stdexcept>
#include <chrono>


class Timer
{
public:
	void start()
	{
		tStart = std::chrono::high_resolution_clock::now();
	}

	void stop()
	{
		tEnd = std::chrono::high_resolution_clock::now();
	}

	template<typename Precision = float, typename Unit = std::milli>
	Precision timeElapsed()
	{
		return std::chrono::duration<Precision, Unit>(tEnd - tStart).count();
	}

private:
	std::chrono::time_point<std::chrono::high_resolution_clock> tStart;
	decltype(tStart) tEnd;
};

static std::vector<char> readFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		throw std::runtime_error("failed to open file!");
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

template <class T>
inline void hash_combine(std::size_t& seed, const T& v)
{
	std::hash<T> hasher;
	seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}