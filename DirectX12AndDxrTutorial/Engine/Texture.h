#pragma once

#include <cstdint>
#include <string>
#include <memory>

namespace Engine {
	class Texture {
	public:
		Texture(const std::string& fileName);

		static void stbImageDeleter(unsigned char* image);

		int width, height, channels, bytesPerPixel;

		using StbImagePtr = std::unique_ptr<unsigned char, decltype(&stbImageDeleter)>;
		StbImagePtr data;
	};
}