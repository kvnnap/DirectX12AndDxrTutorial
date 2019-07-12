#include "RTShaders.hlsli"
#include "Utils.hlsli"

RaytracingAccelerationStructure gRtScene : register(t0);
StructuredBuffer<float3> verts : register(t1);
StructuredBuffer<FaceAttributes> faceAttributes : register(t2);
StructuredBuffer<Material> materials : register(t3);

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

	// point on film plane
	float2 r = pt / dims;
	float3 objectPlanePosition = float3(
		cBuffer.camera.objectPlane.width * (r.x - 0.5f), 
		cBuffer.camera.objectPlane.height * (0.5f - r.y), 
		0.f);

	// Setup Ray
	RayDesc ray;
	ray.Origin = cBuffer.camera.position;
	ray.Direction = (w * cBuffer.camera.objectPlane.distance + u * objectPlanePosition.x + v * objectPlanePosition.y);
	ray.TMin = 0.f;
	ray.TMax = 3.402823e+38;

	// Let's ray trace
	float3 radiance = float3(0.f, 0.f, 0.f);
	const int iterCount = 1;
	for (int i = 0; i < iterCount; ++i) {
		RayPayload payload;
		payload.color[0] = i + 1;
		TraceRay(
			gRtScene,	// Acceleration Structure
			0,			// Ray flags
			0xFF,		// Instance inclusion Mask (0xFF includes everything)
			0,			// RayContributionToHitGroupIndex
			2,			// MultiplierForGeometryContributionToShaderIndex
			0,			// Miss shader index (within the shader table)
			ray,
			payload);
		radiance += payload.color;
	}

	float3 col = linearToSrgb(radiance);
	gOutput[launchIndex.xy] = float4(col, 1.f);
}

[shader("miss")]
void miss(inout RayPayload payload)
{
	payload.color = float3(0.f, 0.f, 0.f);
}

float3 explicitLighting(inout uint seed, float3 interPoint, float3 unitNormal, uint materialId) {
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
	shadowRay.TMax = 0.999f;

	RayPayload payload;
	TraceRay(
		gRtScene,	// Acceleration Structure
		0,			// Ray flags
		0xFF,		// Instance inclusion Mask (0xFF includes everything)
		1,			// RayContributionToHitGroupIndex
		2,			// MultiplierForGeometryContributionToShaderIndex
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
	float3 diffuse = (float3)materials[materialId].diffuse;

	radiance = lightRadiance * diffuse;
	radiance *= primitiveShadowDot * projectedArea  / (PI * lightPdf);

	return radiance;
}

[shader("closesthit")]
void chs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	const uint pIndex = PrimitiveIndex();
	const uint index = pIndex * 3;
	FaceAttributes f = faceAttributes.Load(pIndex);

	const uint3 d = DispatchRaysIndex();
	uint seed = rand_init(d.x * (uint)payload.color[0], d.y * (uint)payload.color[0]);
	
	float3 interPoint = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
	float3 unitNormal = getUnitNormal(verts.Load(index), verts.Load(index + 1), verts.Load(index + 2));

	//explicitLighting(inout uint seed, float3 interPoint, float3 unitNormal, uint materialId)
	payload.color = explicitLighting(seed, interPoint, unitNormal, f.materialId);
}

[shader("closesthit")]
void shadowChs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.color = float3(1.f, 1.f, 1.f);
}