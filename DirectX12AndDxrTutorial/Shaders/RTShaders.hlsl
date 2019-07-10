RaytracingAccelerationStructure gRtScene : register(t0);
StructuredBuffer<float3> gVerts : register(t1);
RWTexture2D<float4> gOutput : register(u0);

cbuffer TriCol : register(b0) 
{
	float3 cols;
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

	float2 pt = float2(launchIndex.xy);
	float2 dims = float2(launchDim.xy);

	// Point is now in range -1,1
	float2 ndc = (pt / dims) * 2.f - 1.f;

	float aspectRatio = dims.x / dims.y;

	// Setup Ray
	RayDesc ray;
	ray.Origin = float3(0.f, 1.f, 2.0f);
	ray.Direction = normalize(float3(ndc.x * aspectRatio, -ndc.y, -1.f));
	ray.TMin = 0.f;
	ray.TMax = 1000.f;

	// Let's ray trace
	RayPayload payload;
	TraceRay(
		gRtScene,	// Acceleration Structure
		0,			// Ray flags
		0xFF,		// Instance inclusion Mask (0xFF includes everything)
		0,			// RayContributionToHitGroupIndex
		1,			// MultiplierForGeometryContributionToShaderIndex
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
	const float3 lightPos = float3(0.f, 1.96f, 0.f);
	float3 lightDir = normalize(lightPos - interPoint);

	// Face normal
	uint index = pIndex * 3;
	float3 a0 = gVerts.Load(index);
	float3 a1 = gVerts.Load(index + 1);
	float3 a2 = gVerts.Load(index + 2);

	float3 normal = normalize(cross(a1 - a0, a2 - a0));

	float coeff = saturate(dot(lightDir, normal));

	//payload.color = float3(1.f, 0.f, 0.f);
	//float3 barycentrics = float3(1.f - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);

	//payload.color = cols[0] * barycentrics.x + cols[1] * barycentrics.y + cols[2] * barycentrics.z;
	//payload.color = cols[PrimitiveIndex() % 3];
	//payload.color = float3(1, 1, 1);
	payload.color = coeff * cols;
	//payload.color = normal;

	/*const float3 A = float3(1, 0, 0);
	const float3 B = float3(0, 1, 0);
	const float3 C = float3(0, 0, 1);

	payload.color = A * barycentrics.x + B * barycentrics.y + C * barycentrics.z;*/
}
