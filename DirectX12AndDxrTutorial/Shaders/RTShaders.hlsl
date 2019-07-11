#include "RTShaders.hlsli"

RaytracingAccelerationStructure gRtScene : register(t0);
StructuredBuffer<float3> verts : register(t1);
StructuredBuffer<FaceAttributes> faceAttributes : register(t2);
StructuredBuffer<Material> materials : register(t3);

// Output texture
RWTexture2D<float4> gOutput : register(u0);

cbuffer CB1 : register(b0) 
{
	ConstBuff cBuffer;
}

float3 linearToSrgb(float3 c)
{
	// Based on http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
	float3 sq1 = sqrt(c);
	float3 sq2 = sqrt(sq1);
	float3 sq3 = sqrt(sq2);
	float3 srgb = 0.662002687 * sq1 + 0.684122060 * sq2 - 0.323583601 * sq3 - 0.0225411470 * c;
	return srgb;
}

float3 getUnitNormal(float3 a0, float3 a1, float3 a2) {
	return normalize(cross(a1 - a0, a2 - a0));
}

float3 getUnitNormal(float3 a[3]) {
	return getUnitNormal(a[0], a[1], a[2]);
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
	RayPayload payload;
	TraceRay(
		gRtScene,	// Acceleration Structure
		0,			// Ray flags
		0xFF,		// Instance inclusion Mask (0xFF includes everything)
		0,			// RayContributionToHitGroupIndex
		2,			// MultiplierForGeometryContributionToShaderIndex
		0,			// Miss shader index (within the shader table)
		ray,
		payload);

	float3 col = linearToSrgb(payload.color);
	gOutput[launchIndex.xy] = float4(col, 1);
}

[shader("miss")]
void miss(inout RayPayload payload)
{
	payload.color = float3(0.f, 0.f, 0.f);
}

[shader("closesthit")]
void chs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	uint pIndex = PrimitiveIndex();

	float3 interPoint = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();

	// assume point light
	const float3 lightPos = (float3) cBuffer.areaLights[0].a[0];
	float3 lightDirLarge = lightPos - interPoint;

	// Setup Shadow Ray
	RayDesc ray;
	ray.Origin = interPoint;
	ray.Direction = lightDirLarge;
	ray.TMin = 0.001f;
	ray.TMax = 0.999999f;

	TraceRay(
		gRtScene,	// Acceleration Structure
		0,			// Ray flags
		0xFF,		// Instance inclusion Mask (0xFF includes everything)
		1,			// RayContributionToHitGroupIndex
		2,			// MultiplierForGeometryContributionToShaderIndex
		0,			// Miss shader index (within the shader table)
		ray,
		payload);

	if (payload.color[0] == 1.f) {
		payload.color = float3(0.f, 0.f, 0.f);
		return;
	}

	float3 lightDir = normalize(lightDirLarge);

	// Face normal
	const uint index = pIndex * 3;
	float3 normal = getUnitNormal(verts.Load(index), verts.Load(index + 1), verts.Load(index + 2));

	float coeff = saturate(dot(lightDir, normal));

	FaceAttributes f = faceAttributes.Load(pIndex);
	Material m = materials[f.materialId];
	Material lightMaterial = materials[cBuffer.areaLights[0].materialId];

	payload.color = (m.emission.x == 0 ? coeff : 1.f) *  ((float3)lightMaterial.emission * (float3)m.diffuse);
}

[shader("closesthit")]
void shadowChs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.color = float3(1.f, 0.f, 0.f);
}