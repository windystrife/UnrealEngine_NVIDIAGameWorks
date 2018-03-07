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

void TestMain(
	float2 InPosition : ATTRIBUTE0,
	float2 InUV : ATTRIBUTE1,
	out float2 OutUV : TEXCOORD0,
	out float4 OutPosition : SV_POSITION,
	out int TypeIndex[6] : TYPEINDEX0
	)
{
	OutPosition = float4( InPosition, 0, 1 );
    OutUV = InUV;

	float4 fVector = float4(1,2,3,4);
	int4 iVector = fVector;
	uint4 uVector = fVector;
	bool4 bVector = fVector;
	float4x4 fMatrix = float4x4(fVector,fVector,fVector,fVector);

	TypeIndex[0] = atype(fMatrix * uVector.xy);					// error
	TypeIndex[1] = atype(fVector.x << 2);						// error
	TypeIndex[2] = atype(uVector << fVector);					// error
	TypeIndex[3] = atype(bVector++);							// error
	TypeIndex[4] = atype(fVector.xy | iVector);					// error
	TypeIndex[5] = atype(bTest.xx ? iVector : uVector);			// error
}
