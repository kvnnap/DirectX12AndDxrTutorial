#include "RTShaders.hlsli"
#include "Utils.hlsli"

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
	uint3 d = DispatchRaysIndex();
	uint seed = rand_init(d.x, d.y);

	float3 interPoint = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();

	AreaLight a = cBuffer.areaLights[chooseInRange(seed,0,0)];

	// assume point light
	const float3 lightPos = getCentroid((float3[3]) a.a);
	float3 lightDirLarge = lightPos - interPoint;
	float3 lightDir = normalize(lightDirLarge);

	// compute area light normal
	float3 lightNormal = getUnitNormal((float3[3]) a.a);

	if (dot(lightNormal, lightDir) >= 0.f) {
		payload.color = float3(0.f, 0.f, 0.f);
		return;
	}

	// Setup Shadow Ray
	RayDesc ray;
	ray.Origin = interPoint;
	ray.Direction = lightDirLarge;
	ray.TMin = 0.001f;
	ray.TMax = 0.999f;

	TraceRay(
		gRtScene,	// Acceleration Structure
		0,			// Ray flags
		0xFF,		// Instance inclusion Mask (0xFF includes everything)
		1,			// RayContributionToHitGroupIndex
		2,			// MultiplierForGeometryContributionToShaderIndex
		0,			// Miss shader index (within the shader table)
		ray,
		payload);

	// Use instead: payload.color = -(payload.color - float3(1.f, 1.f, 1.f));
	if (payload.color[0] == 1.f) {
		payload.color = float3(0.f, 0.f, 0.f);
		return;
	}

	// Face normal
	const uint index = pIndex * 3;
	float3 normal = getUnitNormal(verts.Load(index), verts.Load(index + 1), verts.Load(index + 2));

	float coeff = saturate(dot(lightDir, normal));

	FaceAttributes f = faceAttributes.Load(pIndex);
	Material m = materials[f.materialId];
	Material lightMaterial = materials[a.materialId];

	//payload.color = (m.emission.x == 0 ? coeff : 1.f) * ((float3)a.intensity * (float3)lightMaterial.emission * (float3)m.diffuse);
	payload.color = coeff * ((float3)a.intensity * (float3)lightMaterial.emission * (float3)m.diffuse);
}

[shader("closesthit")]
void shadowChs(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.color = float3(1.f, 1.f, 1.f);
}