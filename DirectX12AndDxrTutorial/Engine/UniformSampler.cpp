#include "UniformSampler.h"

#include <chrono>
#include <limits>
#include "UniformSampler.h"

using namespace std;
using namespace Engine;

UniformSampler::UniformSampler()
	: mt(default_random_engine(chrono::system_clock::now().time_since_epoch().count())()),
	dist(0.f, 1.f)
{ }

float UniformSampler::nextSample() {
	return dist(mt);
}

float UniformSampler::nextSample(float min, float max) {
	float f = uniform_real_distribution<float>(min, max)(mt);
	return f;
}

vector<float> UniformSampler::nextSamples(size_t p_numSamples) {
	vector<float> samples;
	for (size_t i = 0; i < p_numSamples; ++i) {
		samples.push_back(nextSample());
	}
	return samples;
}

size_t UniformSampler::chooseInRange(size_t a, size_t b) {
	return uniform_int_distribution<size_t>(a, b)(mt);
}

uint32_t UniformSampler::nextUInt32()
{
	return static_cast<std::uint32_t>(chooseInRange(0, numeric_limits<std::uint32_t>::max()));
}
