#pragma once

#ifdef __cplusplus
#include <cstdint>
#include "DirectXMath.h"
namespace Shaders {
	struct Material {
		DirectX::XMFLOAT4 diffuse;
		DirectX::XMFLOAT4 emission;
	};

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

	struct AreaLight {
		DirectX::XMFLOAT4 a[3];

		std::uint32_t materialId;
		float intensity;
		float padding[2];
	};

	struct ConstBuff {
		Camera camera;
		AreaLight areaLights[8];
		std::uint32_t numLights;
		float padding[3];
	};
}
#else
struct Material {
	float4 diffuse;
	float4 emission;
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

struct AreaLight {
	float4 a[3];

	uint materialId;
	float intensity;
};

struct ConstBuff {
	Camera camera;
	AreaLight areaLights[8];
	uint numLights;
};
#endif

