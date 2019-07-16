#include "Texture.h"

#include "Libraries/stb/stb_image.h"

Engine::Texture::Texture(const std::string& fileName)
	: width(), height(), channels(), bytesPerPixel(), data(nullptr, stbImageDeleter)
{
	int imageChannels;
	data = StbImagePtr(stbi_load(fileName.c_str(), &width, &height, &imageChannels, channels = STBI_rgb_alpha), stbImageDeleter);
	bytesPerPixel = channels * 8;
}

void Engine::Texture::stbImageDeleter(unsigned char* image)
{
	if (image != nullptr) {
		stbi_image_free(image);
	}
}
