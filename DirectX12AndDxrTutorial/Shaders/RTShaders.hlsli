#pragma once

namespace Shaders {
	enum CameraType {
		Pinhole = 0,
		ThinLens = 1
	};
}

#ifdef __cplusplus
#include <cstdint>
#include "DirectXMath.h"
namespace Shaders {
	struct Material {
		DirectX::XMFLOAT4 diffuse;
		DirectX::XMFLOAT4 emission;
		std::int32_t diffuseTextureId;
		std::uint32_t padding[3];
	};

	struct FaceAttributes {
		std::uint32_t materialId;
		std::uint32_t areaLightId;
		std::uint32_t padding[2];
	};

	struct CameraPlane {
		float width;
		float height;
		std::uint32_t padding[2];
	};

	struct Camera {
		DirectX::XMVECTOR position;
		DirectX::XMVECTOR direction;
		DirectX::XMVECTOR up;
		CameraPlane filmPlane;
		float focalLength;
		float apertureRadius;
		Shaders::CameraType cameraType;
		std::uint32_t padding[1];
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

		// Anvil
		std::uint32_t debugPixelX;
		std::uint32_t debugPixelY;
		std::uint32_t debugPixel;
		std::uint32_t padding;
	};

	// Anvil
	struct PathTracingIntersectionContext {
		// Ray - XMVECTOR's are pods
		DirectX::XMVECTOR origin;
		DirectX::XMVECTOR direction;
		float tMin;
		float tMax;
		float tHit;
		float rayProbability;

		DirectX::XMVECTOR radiance;
		DirectX::XMVECTOR unitNormal;

		uint32_t rayDepth;
		uint32_t rayType;
		uint32_t primitiveId;
		uint32_t materialId;
	};

	struct PathTracingPath {
		uint32_t debugId;
		uint32_t numRays;
		uint32_t pixelX;
		uint32_t pixelY;
		uint32_t seed1;
		uint32_t seed2;
		uint32_t padding[2];
		DirectX::XMVECTOR totalRadiance;
		PathTracingIntersectionContext pathTracingIntersectionContext[16];
	};
}
#else
struct Material {
	float4 diffuse;
	float4 emission;
	int diffuseTextureId;
	int3 padding;
};

struct FaceAttributes {
	uint materialId;
	uint areaLightId;
	uint2 padding;
};

struct CameraPlane {
	float width;
	float height;
	float2 padding;
};

struct Camera {
	// Float 3 will be padded automatically
	float3 position;
	float3 direction;
	float3 up;
	CameraPlane filmPlane;
	float focalLength;
	float apertureRadius;
	Shaders::CameraType cameraType;
	float padding;
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
	uint4 debugPixel; // Anvil
};

// Anvil
struct PathTracingIntersectionContext {
	// Ray - XMVECTOR's are pods
	float4 origin;
	float4 direction;

	float tMin;
	float tMax;
	float tHit;
	float rayProbability;

	float4 radiance;
	float4 unitNormal;

	uint rayDepth;
	uint rayType;
	uint primitiveId;
	uint materialId;
};

struct PathTracingPath {
	uint debugId;
	uint numRays;
	uint2 pixel;
	uint seed1;
	uint seed2;
	uint2 padding;

	float4 totalRadiance;
	PathTracingIntersectionContext pathTracingIntersectionContext[16];
};
#endif

