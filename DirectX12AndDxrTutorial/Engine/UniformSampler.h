#pragma once

#include <stddef.h>
#include <vector>
#include <random>
#include <cstdint>

namespace Engine
{
	class UniformSampler
	{
	public:
		UniformSampler();
		// Virtuals
		float nextSample();
		float nextSample(float min, float max);
		std::vector<float> nextSamples(size_t p_numSamples);

		// Our
		size_t chooseInRange(size_t a, size_t b);

		// random uint32
		std::uint32_t nextUInt32();

	private:
		std::mt19937 mt;
		std::uniform_real_distribution<float> dist;
	};
}