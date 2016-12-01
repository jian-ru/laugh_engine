#pragma once

#include <vector>
#include <fstream>
#include <stdexcept>
#include <chrono>


class Timer
{
public:
	Timer(uint32_t ac = 100) :
		tStart(std::chrono::high_resolution_clock::now()),
		tEnd(tStart),
		averageCount{ac},
		timeIntervals(ac, 0.f)
	{}

	void start()
	{
		tStart = std::chrono::high_resolution_clock::now();
	}

	void stop()
	{
		tEnd = std::chrono::high_resolution_clock::now();
		float te = timeElapsed();
		total -= timeIntervals[nextIdx];
		timeIntervals[nextIdx] = te;
		total += te;
		nextIdx = (nextIdx + 1) % averageCount;
	}

	float timeElapsed() const
	{
		return std::chrono::duration<float, std::milli>(tEnd - tStart).count();
	}

	float getAverageTime() const
	{
		return total / averageCount;
	}

private:
	std::chrono::time_point<std::chrono::high_resolution_clock> tStart;
	decltype(tStart) tEnd;

	uint32_t averageCount;
	uint32_t nextIdx = 0;
	float total = 0.f;
	std::vector<float> timeIntervals;
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