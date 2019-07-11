#pragma once

#ifdef __cplusplus
#include <cstdint>
namespace Shaders {
	struct FaceAttributes {
		std::uint32_t materialId;
	};
}
#else
struct Material {
	float4 diffuse;
};

struct FaceAttributes {
	uint materialId;
};
#endif

