#pragma once

#ifdef __cplusplus
#include <cstdint>
#include "DirectXMath.h"
namespace Shaders {
	struct FaceAttributes {
		std::uint32_t materialId;
	};

	struct CameraPlane {
		float width;
		float height;
		float distance;
		float apertureSize; // Diamater - unused for now
	};

	struct Camera {
		DirectX::XMVECTOR position;
		DirectX::XMVECTOR direction;
		DirectX::XMVECTOR up;
		CameraPlane objectPlane;
	};
}
#else
struct Material {
	float4 diffuse;
};

struct FaceAttributes {
	uint materialId;
};

struct CameraPlane {
	float width;
	float height;
	float distance;
	float apertureSize; // Diamater - unused for now
};

struct Camera {
	// Float 3 will be padded automatically
	float3 position;
	float3 direction;
	float3 up;
	CameraPlane objectPlane;
};
#endif

