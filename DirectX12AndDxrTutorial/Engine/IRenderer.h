#pragma once

#include <cstdint>

#include "Camera.h"

namespace Engine {
	class IRenderer {
	public:
		virtual ~IRenderer() = default;
		
		virtual void clearBuffer(float red, float green, float blue) = 0;
		virtual void init() = 0;
		virtual void draw(std::uint64_t timeMs, bool& clear) = 0;
		virtual void endFrame() = 0;

		virtual Camera& getCamera() = 0;
	};
}