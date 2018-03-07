// Arithmetic type checks.
int atype(float a) { return 0; }
int atype(float2 a) { return 1; }
int atype(float3 a) { return 2; }
int atype(float4 a) { return 3; }
int atype(int a) { return 4; }
int atype(int2 a) { return 5; }
int atype(int3 a) { return 6; }
int atype(int4 a) { return 7; }
int atype(uint a) { return 8; }
int atype(uint2 a) { return 9; }
int atype(uint3 a) { return 10; }
int atype(uint4 a) { return 11; }
int atype(bool a) { return 1; }
int atype(bool2 a) { return 2; }
int atype(bool3 a) { return 3; }
int atype(bool4 a) { return 4; }
int atype(float3x3 a) { return 5; }
int atype(float4x4 a) { return 6; }

bool bTest;
static const bool bConst = true;

void TestMain(
	float2 InPosition : ATTRIBUTE0,
	float2 InUV : ATTRIBUTE1,
	out float2 OutUV : TEXCOORD0,
	out float4 OutPosition : SV_POSITION,
	out int TypeIndex[30] : TYPEINDEX0
	)
{
	OutPosition = float4( InPosition, 0, 1 );
    OutUV = InUV;

	float4 fVector = float4(1,2,3,4);
	int4 iVector = fVector;
	uint4 uVector = fVector;
	bool4 bVector = fVector;
	float4x4 fMatrix = float4x4(fVector,fVector,fVector,fVector);

	TypeIndex[0] = atype(fVector.xy * fVector);						// 1 float2
	TypeIndex[1] = atype(iVector.xyz * fVector);					// 2 float3
	TypeIndex[2] = atype(uVector.x * fVector);						// 3 float4
	TypeIndex[3] = atype(uVector.x * iVector.xy);					// 9 uint2
	TypeIndex[4] = atype(iVector.x * uVector.xy);					// 9 uint2
	TypeIndex[5] = atype(bVector.xy * iVector.x);					// 5 int2
	TypeIndex[6] = atype(fMatrix * iVector.x);						// 6 float4x4
	TypeIndex[7] = atype(fVector.xy >= iVector.xy);					// 2 bool2
	TypeIndex[8] = atype(uVector.xyz != iVector);					// 3 bool3
	TypeIndex[9] = atype(bVector.x << 2);							// 4 int
	TypeIndex[10] = atype(uVector.xy >> 3);							// 9 uint2
	TypeIndex[11] = atype(iVector << bVector.xyz);					// 6 int3
	TypeIndex[12] = atype(iVector >> uVector.xyz);					// 6 int3
	TypeIndex[13] = atype(iVector.x << uVector.xy);					// 5 int2
	TypeIndex[14] = atype(-bVector.xyz);							// 6 int3
	TypeIndex[15] = atype(uVector.xyz | iVector);					// 10 uint3
	TypeIndex[16] = atype(bVector.xy | iVector);					// 5 int2
	TypeIndex[17] = atype(bVector.xy | uVector);					// 9 uint2
	TypeIndex[18] = atype(fVector % fVector.xyz);					// 2 float3
	TypeIndex[19] = atype(fVector.xy % fVector);					// 1 float2
	TypeIndex[20] = atype(fVector.x % fVector);						// 3 float4
	TypeIndex[21] = atype(fVector % iVector.xyz);					// 2 float3
	TypeIndex[22] = atype(iVector % fVector.xy);					// 1 float2
	TypeIndex[23] = atype(iVector.xy % uVector);					// 9 uint2
	TypeIndex[24] = atype(iVector.xy % iVector);					// 5 int2
	TypeIndex[25] = atype(bTest ? iVector.xyz : fVector);			// 2 float3
	TypeIndex[26] = atype(bConst ? iVector.xyz : fVector);			// 2 float3
	TypeIndex[27] = atype(bTest.xx ? iVector.xy : uVector.xy);		// 9 uint2
	TypeIndex[28] = atype(fVector.xx ? iVector.xy : uVector.xyz);	// 9 uint2
	TypeIndex[29] = atype(fVector && iVector.xyz);					// 3 bool3
}
