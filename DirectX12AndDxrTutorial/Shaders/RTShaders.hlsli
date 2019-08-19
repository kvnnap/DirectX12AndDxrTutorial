#pragma once

#ifdef __cplusplus
#include <cstdint>
#include "DirectXMath.h"
namespace Shaders {
	struct Material {
		DirectX::XMFLOAT4 diffuse;
		DirectX::XMFLOAT4 emission;
		std::int32_t diffuseTextureId;
		std::uint32_t other[3];
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
		CameraPlane filmPlane;
	};

	struct AreaLight {
		DirectX::XMVECTOR intensity;
		std::uint32_t instanceIndex;
		std::uint32_t primitiveId;
		std::uint32_t materialId;
		std::uint32_t padding[1];
	};

	struct ConstBuff {
		Camera camera;
		AreaLight areaLights[8];
		std::uint32_t numLights;
		std::uint32_t seed1;
		std::uint32_t seed2;
		std::uint32_t clear;
	};
}
#else
struct Material {
	float4 diffuse;
	float4 emission;
	int diffuseTextureId;
	int other[3];
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
	CameraPlane filmPlane;
};

struct AreaLight {
	float4 intensity;
	uint instanceIndex;
	uint primitiveId;
	uint materialId;
	uint padding;
};

struct ConstBuff {
	Camera camera;
	AreaLight areaLights[8];
	uint numLights;
	uint seed1;
	uint seed2;
	uint clear;
};
#endif

