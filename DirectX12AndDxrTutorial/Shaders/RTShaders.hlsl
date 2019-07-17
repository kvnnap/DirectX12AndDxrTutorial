#include "RTShaders.hlsli"
#include "Utils.hlsli"

RaytracingAccelerationStructure gRtScene : register(t0);
StructuredBuffer<float3> verts : register(t1);
StructuredBuffer<FaceAttributes> faceAttributes : register(t2);
StructuredBuffer<Material> materials : register(t3);
StructuredBuffer<float2> texVerts : register(t4);
Texture2D gTextures[]: register(t5);
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
	uint primitiveId;
	float tHit;
	float2 bary;
};

[shader("raygeneration")]
void rayGen()
{
	// Work item index - current x,y point
	uint3 launchIndex = DispatchRaysIndex();

	// dimensions - the previous x,y point is contained within these dimensions
	uint3 launchDim = DispatchRaysDimensions();

	const float2 pt = float2(launchIndex.xy);
	const float2 dims = float2(launchDim.xy);

	// calculate u,v,w
	float3 u, v, w;
	w = normalize(cBuffer.camera.direction);
	u = -normalize(cross(cBuffer.camera.up, cBuffer.camera.direction));
	v = -normalize(cross(w, u));

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
			cBuffer.seed1 + launchDim.x * ((uint)gRadiance[launchIndex.xy].w + i) + launchIndex.x,
			cBuffer.seed2 + launchDim.y * ((uint)gRadiance[launchIndex.xy].w + i) + launchIndex.y);

		// point on film plane
		float2 r = (pt + float2(rand_next(seed), rand_next(seed))) / dims;
		float2 filmPlanePosition = float2(
			cBuffer.camera.filmPlane.width * (r.x - 0.5f),
			cBuffer.camera.filmPlane.height * (0.5f - r.y));
		ray.Direction = (w * cBuffer.camera.filmPlane.distance + u * filmPlanePosition.x + v * filmPlanePosition.y);

		RayPayload payload;
		payload.color[0] = asfloat(seed);

		TraceRay(
			gRtScene,	// Acceleration Structure
			0,			// Ray flags
			0xFF,		// Instance inclusion Mask (0xFF includes everything)
			0,			// RayContributionToHitGroupIndex
			3,			// MultiplierForGeometryContributionToShaderIndex
			0,			// Miss shader index (within the shader table)
			ray,
			payload);
		radiance += payload.color;
	}

	if (cBuffer.clear) {
		gRadiance[launchIndex.xy] *= float4(0.f, 0.f, 0.f, 0.f);
	}

	gRadiance[launchIndex.xy] += float4(radiance, iterCount);

	const float3 rad = (float3)gRadiance[launchIndex.xy] / gRadiance[launchIndex.xy].w;
	const float3 col = linearToSrgb(toneMap(rad));
	gOutput[launchIndex.xy] = float4(col, 1.f);
}

[shader("miss")]
void miss(inout RayPayload payload)
{
	payload.color = float3(0.f, 0.f, 0.f);
}

float3 getDiffuseValue(uint primitiveId, uint materialId, float2 bary) {
	const uint index = primitiveId * 3;
	float2 a0 = texVerts.Load(index);
	float2 a1 = texVerts.Load(index + 1);
	float2 a2 = texVerts.Load(index + 2);
	float2 pTex = a0 + bary.x * (a1 - a0) + bary.y * (a2 - a0);
	return materials[materialId].diffuseTextureId == -1 ?
		(float3)materials[materialId].diffuse :
		(float3)gTextures[materials[materialId].diffuseTextureId].SampleLevel(gSampler, pTex, 0);
}

float3 explicitLighting(inout uint seed, uint primitiveId, float3 interPoint, float3 unitNormal, uint materialId, float2 bary) {
	float3 radiance = float3(0.f, 0.f, 0.f);
	float lightPdf = 1.f / cBuffer.numLights;
	
	AreaLight areaLight = cBuffer.areaLights[chooseInRange(seed, 0, cBuffer.numLights - 1)];

	float3 pointOnLightSource = samplePointOnTriangle(seed, (float3[3])areaLight.a);
	float3 lightDirLarge = pointOnLightSource - interPoint;
	float3 lightDir = normalize(lightDirLarge);

	// Check if light is behind the primitive (back face)
	float primitiveShadowDot = dot(unitNormal, lightDir);
	if (primitiveShadowDot <= 0.f) {
		return radiance;
	}

	// Check if primitive is behind the light (back face)
	float3 lightUnitNormal = getUnitNormal((float3[3]) areaLight.a);
	float lightShadowDot = dot(lightUnitNormal, -lightDir);
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
	float projectedArea = getTriangleArea((float3[3]) areaLight.a) * lightShadowDot / (lightDistance * lightDistance);

	// Get diffuse of intersected material
	float3 diffuse = getDiffuseValue(primitiveId, materialId, bary);

	radiance = lightRadiance * diffuse;
	radiance *= primitiveShadowDot * projectedArea  / (PI * lightPdf);

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
	while (rand_next(seed) <= (probabilityOfContinuing = dot(unitNormal, indirectRay.Direction))) {
		
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
		unitNormal = getUnitNormal(verts.Load(index), verts.Load(index + 1), verts.Load(index + 2));
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

[shader("closesthit")]
void chs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	const uint pIndex = PrimitiveIndex();
	const uint index = pIndex * 3; 
	
	const float3 unitNormal = getUnitNormal(verts.Load(index), verts.Load(index + 1), verts.Load(index + 2));
	const float3 dir = normalize(WorldRayDirection());

	if (dot(dir, unitNormal) >= 0.f) {
		payload.color = float3(0.f, 0.f, 0.f);
		return;
	}
	
	FaceAttributes f = faceAttributes.Load(pIndex);

	//explicitLighting(inout uint seed, float3 interPoint, float3 unitNormal, uint materialId)
	if (materials[f.materialId].emission.x == 0.f) {
		uint seed = asuint(payload.color[0]);
		const float3 interPoint = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
		//float3 interPoint = verts.Load(index) + (verts.Load(index + 1) - verts.Load(index)) * attribs.barycentrics.x + (verts.Load(index + 2) - verts.Load(index)) * attribs.barycentrics.y;
		payload.color = 
			explicitLighting(seed, pIndex, interPoint, unitNormal, f.materialId, attribs.barycentrics) + 
			indirectLighting(seed, pIndex, interPoint, unitNormal, f.materialId, attribs.barycentrics);
	}
	else {
		payload.color = (float3)materials[f.materialId].emission;
	}
}

[shader("closesthit")]
void shadowChs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.color = float3(1.f, 1.f, 1.f);
}

[shader("closesthit")]
void indirectChs(inout IndirectPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.primitiveId = PrimitiveIndex();
	payload.tHit = RayTCurrent();
	payload.bary = attribs.barycentrics;
}

[shader("miss")]
void indirectMiss(inout IndirectPayload payload)
{
	payload.tHit = -1.f;
}