/*******************************************************************
	Random numbers based on Mersenne Twister
*******************************************************************/
uint rand_init(uint val0, uint val1, uint backoff = 16)
{
	uint v0 = val0;
	uint v1 = val1;
	uint s0 = 0;

	for (uint n = 0; n < backoff; n++)
	{
		s0 += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
		v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
	}

	return v0;
}

float rand_next(inout uint s)
{
	uint LCG_A = 1664525u;
	uint LCG_C = 1013904223u;
	s = (LCG_A * s + LCG_C);
	return float(s & 0x00FFFFFF) / float(0x01000000);
}

// Range is [a-b] (inclusive), a <= b
// Gens a num from 0 to 1, scales it, and returns uint
uint chooseInRange(inout uint s, uint a, uint b) {
	return a + uint(rand_next(s) * (b - a + 1));
}

static const float PI = 3.14159265f;

float3 samplePointOnTriangle(inout uint s, float3 verts[3]) {
	float r1 = rand_next(s);
	float r2 = rand_next(s);

	// Avoiding conditional - not needed for statements without else?
	/*bool cond = (r1 + r2 > 1.f);
	r1 = r1 * !cond + (1.f - r1) * cond;
	r2 = r2 * !cond + (1.f - r2) * cond;*/
	if (r1 + r2 > 1.f) {
		r1 = 1.f - r1;
		r2 = 1.f - r2;
	}

	float3 Q1 = verts[1] - verts[0];
	float3 Q2 = verts[2] - verts[0];

	return verts[0] + r1 * Q1 + r2 * Q2;
}

float getTriangleArea(float3 verts[3]) {
	const float3 Q1 = verts[1] - verts[0];
	const float3 Q2 = verts[2] - verts[0];
	const float Q1Q2 = dot(Q1, Q2);

	return 0.5f * length(Q1) * length(Q2) * sqrt(1.f - (Q1Q2 * Q1Q2 / (dot(Q1, Q1) * dot(Q2, Q2))));
}

float3 getUnitNormal(float3 a0, float3 a1, float3 a2) {
	return normalize(cross(a1 - a0, a2 - a0));
}

float3 getUnitNormal(float3 a[3]) {
	return getUnitNormal(a[0], a[1], a[2]);
}

float3 getCentroid(float3 a[3]) {
	return (a[0] + a[1] + a[2]) / 3.f;
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

float3 transformPointToBasis(float3 unitNormal, float3 pt) {
	// Create first vector perpendicular to the normal
	const float3 u = unitNormal.y != 0.f || unitNormal.x != 0.f ?
					normalize(float3(unitNormal.y, -unitNormal.x, 0.f)) :
					float3(unitNormal.z, 0.f, 0.f);

	// Create second vector perpendicular to the normal
	const float3 w = normalize(cross(u, unitNormal));
	const float3 v = unitNormal;

	return pt.x * u + pt.y * v + pt.z * w;
}

float3 randomRayLobe(inout uint s, float3 unitNormal, float n) {
	// The pdf is (n + 1) cos^n(phi) / (2*pi)
	const float nPlusOne = n + 1.f;
	const float cosPhiToTheNPlusOne = rand_next(s);
	// TODO: uncomment if used
	//probability = nPlusOne / (2.f * Mathematics::Constants::Pi) * Functions::pow(cosPhiToTheNPlusOne, n / nPlusOne);
	const float cosPhi = pow(cosPhiToTheNPlusOne, 1.f / nPlusOne);
	const float sinPhi = sqrt(1.f - cosPhi * cosPhi);
	const float theta = 2.f * PI * rand_next(s);

	float sinTheta;
	float cosTheta;
	sincos(theta, sinTheta, cosTheta);

	return transformPointToBasis(unitNormal, float3(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta));
}

float3 randomRayHemisphere(inout uint s, float3 unitNormal) {
	return randomRayLobe(s, unitNormal, 0);
}