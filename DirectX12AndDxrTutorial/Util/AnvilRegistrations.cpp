#include "AnvilRegistrations.h"
#include "Shaders/RTShaders.hlsli"
#include "Engine/Camera.h"
#include "reflection/reflect.h"

DirectXVectorWrapper::DirectXVectorWrapper(const DirectX::XMVECTOR& p_vector)
	: vector (&p_vector)
{}

float DirectXVectorWrapper::getX() const { return vector->m128_f32[0]; }
float DirectXVectorWrapper::getY() const { return vector->m128_f32[1]; }
float DirectXVectorWrapper::getZ() const { return vector->m128_f32[2]; }
float DirectXVectorWrapper::getW() const { return vector->m128_f32[3]; }

// Reflect..
REFLECT_BEGIN(DirectXVectorWrapper)
auto s = sizeof(DirectXVectorWrapper);
addMember("X", &BaseType::getX);
addMember("Y", &BaseType::getY);
addMember("Z", &BaseType::getZ);
addMember("W", &BaseType::getW);
REFLECT_END()

DirectXVectorWrapper origin(const Shaders::PathTracingIntersectionContext& ptic) {  return ptic.origin; }
DirectXVectorWrapper direction(const Shaders::PathTracingIntersectionContext& ptic) { return ptic.direction; }
DirectXVectorWrapper radiance(const Shaders::PathTracingIntersectionContext& ptic) { return ptic.radiance; }
DirectXVectorWrapper unitNormal(const Shaders::PathTracingIntersectionContext& ptic) { return ptic.unitNormal; }

REFLECT_BEGIN(Shaders::PathTracingIntersectionContext)
addMember("Position", origin);
addMember("Direction", direction);
REFLECT_MEMBER(tMin)
REFLECT_MEMBER(tMax)
REFLECT_MEMBER(tHit)
REFLECT_MEMBER(rayProbability)
addMember("radiance", radiance);
addMember("unitNormal", unitNormal);
REFLECT_MEMBER(rayDepth)
REFLECT_MEMBER(rayType)
REFLECT_MEMBER(primitiveId)
REFLECT_MEMBER(materialId)
REFLECT_END()

DirectXVectorWrapper totalRadiance(const Shaders::PathTracingPath& ptp) { return ptp.totalRadiance; }

REFLECT_BEGIN(Shaders::PathTracingPath)
REFLECT_MEMBER(debugId)
REFLECT_MEMBER(numRays)
REFLECT_MEMBER(pixelX)
REFLECT_MEMBER(pixelY)
REFLECT_MEMBER(seed1)
REFLECT_MEMBER(seed2)
addMember("totalRadiance", totalRadiance);
REFLECT_MEMBER(pathTracingIntersectionContext)
REFLECT_END()

DirectXVectorWrapper cameraPosition(const Engine::Camera& ptp) { return ptp.getPosition(); }
DirectXVectorWrapper cameraDirection(const Engine::Camera& ptp) { return ptp.getDirection(); }
REFLECT_BEGIN(Engine::Camera)
addMember("Position", cameraPosition);
addMember("Direction", cameraDirection);
REFLECT_END()