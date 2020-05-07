#include "RTShaders.hlsli"
#include "Utils.hlsli"

RaytracingAccelerationStructure gRtScene : register(t0);
StructuredBuffer<float3> verts : register(t1);
StructuredBuffer<FaceAttributes> faceAttributes : register(t2);
StructuredBuffer<Material> materials : register(t3);
StructuredBuffer<float2> texVerts : register(t4);
StructuredBuffer<float4x3> matrices : register(t5);
Texture2D gTextures[]: register(t6);
SamplerState gSampler : register(s0);

// Output texture
RWTexture2D<float4> gOutput : register(u0);
RWTexture2D<float4> gRadiance : register(u1);

cbuffer CB1 : register(b0) 
{
	ConstBuff cBuffer;
}

struct RayPayload
{
	float3 color;
};

struct IndirectPayload
{
	uint instanceIndex;
	uint primitiveId;
	float tHit;
	float2 bary;
};

float3 getUnitNormal(float3 a0, float3 a1, float3 a2, uint instanceIndex) {
	//if (instanceIndex == 5) return float3(0, 1, 0);
	return normalize(mul(float4(cross(a1 - a0, a2 - a0), 0.f), matrices[instanceIndex]));
}

uint getPrimitiveIndex() {
	return InstanceID() + PrimitiveIndex();
}

float3 getDiffuseValue(uint primitiveId, uint materialId, float2 bary) {
	if (materials[materialId].diffuseTextureId == -1) {
		return (float3)materials[materialId].diffuse;
	}

	const uint index = primitiveId * 3;
	float2 a0 = texVerts.Load(index);
	float2 a1 = texVerts.Load(index + 1);
	float2 a2 = texVerts.Load(index + 2);
	float2 pTex = a0 + bary.x * (a1 - a0) + bary.y * (a2 - a0);
	return (float3)gTextures[materials[materialId].diffuseTextureId].SampleLevel(gSampler, pTex, 0);
}

float3 explicitLighting(inout uint seed, uint primitiveId, float3 interPoint, float3 unitNormal, uint materialId, float2 bary) {
	float3 radiance = float3(0.f, 0.f, 0.f);

	const uint lightIndex = chooseInRange(seed, 0, cBuffer.numLights - 1);

	// If this is a light, make sure it does not contribute its light to itself
	if (cBuffer.areaLights[lightIndex].primitiveId == primitiveId) {
		return radiance;
	}

	AreaLight areaLight = cBuffer.areaLights[lightIndex];

	const uint areaLightIndex = areaLight.primitiveId * 3;
	float3 a[3];
	a[0] = mul(float4(verts.Load(areaLightIndex    ), 1.f), matrices[areaLight.instanceIndex]);
	a[1] = mul(float4(verts.Load(areaLightIndex + 1), 1.f), matrices[areaLight.instanceIndex]);
	a[2] = mul(float4(verts.Load(areaLightIndex + 2), 1.f), matrices[areaLight.instanceIndex]);
	
	const float3 pointOnLightSource = samplePointOnTriangle(seed, (float3[3])a);
	const float3 lightDirLarge = pointOnLightSource - interPoint;
	const float3 lightDir = normalize(lightDirLarge);

	// Check if light is behind the primitive (back face)
	const float primitiveShadowDot = dot(unitNormal, lightDir);
	if (primitiveShadowDot <= 0.f) {
		return radiance;
	}

	// Check if primitive is behind the light (back face)
	const float lightShadowDot = dot(getUnitNormal(a[0], a[1], a[2]), -lightDir);
	if (lightShadowDot <= 0.f) {
		return radiance;
	}

	// Setup Shadow Ray
	RayDesc shadowRay;
	shadowRay.Origin = interPoint;
	shadowRay.Direction = lightDirLarge;
	shadowRay.TMin = 0.001f;
	shadowRay.TMax = 0.99f;

	RayPayload payload;
	TraceRay(
		gRtScene,	// Acceleration Structure
		0,			// Ray flags
		0xFF,		// Instance inclusion Mask (0xFF includes everything)
		1,			// RayContributionToHitGroupIndex
		3,			// MultiplierForGeometryContributionToShaderIndex
		0,			// Miss shader index (within the shader table)
		shadowRay,
		payload);

	// We're occluded, return
	if (payload.color[0] == 1.f) {
		return radiance;
	}

	// Get light radiance
	float3 lightRadiance = (float3)(areaLight.intensity * materials[areaLight.materialId].emission);

	// Get projected area
	float lightDistance = length(lightDirLarge);
	float projectedArea = getTriangleArea((float3[3]) a) * lightShadowDot / (lightDistance * lightDistance);

	// Get diffuse of intersected material
	float3 diffuse = getDiffuseValue(primitiveId, materialId, bary);

	radiance = lightRadiance * diffuse;
	radiance *= cBuffer.numLights * primitiveShadowDot * projectedArea  / PI;

	return radiance;
}

float3 indirectLighting(inout uint seed, uint primitiveId, float3 interPoint, float3 unitNormal, uint materialId, float2 bary) {
	float3 rad = float3(0.f, 0.f, 0.f);
	
	// Get cosine-weighted ray
	RayDesc indirectRay;
	indirectRay.Origin = interPoint;
	indirectRay.Direction = randomRayLobe(seed, unitNormal, 1);
	indirectRay.TMin = 0.001f;
	indirectRay.TMax = 3.402823e+38;

	IndirectPayload payload;
	float3 localCoefficients = float3(1.f, 1.f, 1.f);

	// For use with russian roulette
	float probabilityOfContinuing;

	// Russian roulette
	while (rand_next(seed) <= (probabilityOfContinuing = dot(unitNormal, indirectRay.Direction))) 
	{
		TraceRay(
			gRtScene,	// Acceleration Structure
			0,			// Ray flags
			0xFF,		// Instance inclusion Mask (0xFF includes everything)
			2,			// RayContributionToHitGroupIndex
			3,			// MultiplierForGeometryContributionToShaderIndex
			1,			// Miss shader index (within the shader table)
			indirectRay,
			payload);

		// Traceray - get primitiveId & intersection point  (from t value)
		if (payload.tHit == -1.f) { break; }

		// Get diffuse of prev intersected material
		float3 diffuse = getDiffuseValue(primitiveId, materialId, bary);

		// Compute coefficients for this iteration
		localCoefficients *= diffuse / probabilityOfContinuing;

		// Get intersected face unit normal
		const uint pIndex = payload.primitiveId;
		const uint index = pIndex * 3;
		unitNormal = getUnitNormal(verts.Load(index), verts.Load(index + 1), verts.Load(index + 2), payload.instanceIndex);
		if (dot(indirectRay.Direction, unitNormal) >= 0.f) {
			break;
		}

		// Get intersected face material
		FaceAttributes f = faceAttributes.Load(pIndex);
		materialId = f.materialId;
		primitiveId = payload.primitiveId;
		bary = payload.bary;
		indirectRay.Origin += payload.tHit * indirectRay.Direction;

		// explicit lighing
		rad += localCoefficients * explicitLighting(seed, primitiveId, indirectRay.Origin, unitNormal, materialId, bary);

		// Generate next ray
		indirectRay.Direction = randomRayLobe(seed, unitNormal, 1);
	}

	return rad;
}

[shader("raygeneration")]
void rayGen()
{
	// Work item index - current x,y point
	const uint2 launchIndex = DispatchRaysIndex();

	// dimensions - the previous x,y point is contained within these dimensions
	const uint2 launchDim = DispatchRaysDimensions();

	if (cBuffer.numLights == 0) {
		gOutput[launchIndex] = float4(0.f, 0.f, 0.f, 0.f);
		return;
	}

	// Clear buffer if stuff changed
	if (cBuffer.clear) {
		gRadiance[launchIndex] = float4(0.f, 0.f, 0.f, 0.f);
	}

	// Calculate camera's u,v,w
	const float3 w = normalize(cBuffer.camera.direction);
	const float3 u = -normalize(cross(cBuffer.camera.up, cBuffer.camera.direction));
	const float3 v = -normalize(cross(w, u));

	// Setup Ray
	RayDesc ray;
	ray.Origin = cBuffer.camera.position;
	ray.TMin = 0.f;
	ray.TMax = 3.402823e+38;

	// Let's ray trace
	float3 radiance = float3(0.f, 0.f, 0.f);
	const int iterCount = 1;
	for (int i = 0; i < iterCount; ++i) {

		// Note: Multiplying by iterCount not required if seed is changing on the Host side (not assuming i)
		uint seed = rand_init(
			cBuffer.seed1 + launchDim.x * ((uint)gRadiance[launchIndex].w + i) + launchIndex.x,
			cBuffer.seed2 + launchDim.y * ((uint)gRadiance[launchIndex].w + i) + launchIndex.y);

		// Generate ray direction using camera
		const float2 r = (launchIndex + float2(rand_next(seed), rand_next(seed))) / launchDim;
		const float2 filmPlanePosition = float2(cBuffer.camera.filmPlane.width * (r.x - 0.5f), cBuffer.camera.filmPlane.height * (0.5f - r.y));
		ray.Direction = (w * cBuffer.camera.filmPlane.distance + u * filmPlanePosition.x + v * filmPlanePosition.y);

		RayPayload payload;
		payload.color[0] = asfloat(seed); // Interpret the bits of seed as if it was a float

		TraceRay(
			gRtScene,	// Acceleration Structure
			0,			// Ray flags
			0xFF,		// Instance inclusion Mask (0xFF includes everything)
			0,			// RayContributionToHitGroupIndex (calls chs)
			3,			// MultiplierForGeometryContributionToShaderIndex
			0,			// Miss shader index (within the shader table) (calls miss)
			ray,
			payload);
		radiance += payload.color;
	}

	// Accumulate local radiance to global radiance (need to divide by N)
	gRadiance[launchIndex] += float4(radiance, iterCount);

	// Tonemap and convert radiance (which is gRadiance / N)
	gOutput[launchIndex] = float4(linearToSrgb(toneMap(gRadiance[launchIndex].xyz / gRadiance[launchIndex].w)), 1.f);
}

[shader("closesthit")]
void chs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	const uint pIndex = getPrimitiveIndex();
	
	const uint vIndex = pIndex * 3; 
	const float3 unitNormal = getUnitNormal(verts.Load(vIndex), verts.Load(vIndex + 1), verts.Load(vIndex + 2), InstanceIndex());
	const float3 unitRayDir = normalize(WorldRayDirection());
	uint seed = asuint(payload.color[0]);
	payload.color = float3(0.f, 0.f, 0.f);

	// We're hitting the behind of this geometry, exit
	if (dot(unitRayDir, unitNormal) >= 0.f) {
		return;
	}
	
	const FaceAttributes fAttr = faceAttributes.Load(pIndex);

	//explicitLighting(inout uint seed, float3 interPoint, float3 unitNormal, uint materialId)
	const float3 interPoint = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
	//const float3 interPoint = verts.Load(vIndex) + (verts.Load(vIndex + 1) - verts.Load(vIndex)) *
	//					  attribs.barycentrics.x + (verts.Load(vIndex + 2) - verts.Load(vIndex)) * attribs.barycentrics.y;

	// We're directly hitting an emmisive primite, return its emission - might want to correct this later, as lights have diffuse props too
	if (any(materials[fAttr.materialId].emission)) {
		payload.color = (float3)(cBuffer.areaLights[fAttr.areaLightId].intensity * materials[fAttr.materialId].emission);
	}
	payload.color +=
		explicitLighting(seed, pIndex, interPoint, unitNormal, fAttr.materialId, attribs.barycentrics)
		+ 
		indirectLighting(seed, pIndex, interPoint, unitNormal, fAttr.materialId, attribs.barycentrics)
		;
}

[shader("closesthit")]
void shadowChs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.color = float3(1.f, 1.f, 1.f);
}

[shader("closesthit")]
void indirectChs(inout IndirectPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.instanceIndex = InstanceIndex();
	payload.primitiveId = getPrimitiveIndex();
	payload.tHit = RayTCurrent();
	payload.bary = attribs.barycentrics;
}

[shader("miss")]
void miss(inout RayPayload payload)
{
	payload.color = float3(0.f, 0.f, 0.f);
}

[shader("miss")]
void indirectMiss(inout IndirectPayload payload)
{
	payload.tHit = -1.f;
}