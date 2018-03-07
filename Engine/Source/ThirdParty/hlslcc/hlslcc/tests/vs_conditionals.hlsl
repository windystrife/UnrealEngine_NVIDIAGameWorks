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

static const bool bConst = true;

void TestMain(
	float2 InPosition : ATTRIBUTE0,
	float2 InUV : ATTRIBUTE1,
	out float2 OutUV : TEXCOORD0,
	out float4 OutPosition : SV_POSITION,
	out int TypeIndex[3] : TYPEINDEX0
	)
{
	OutPosition = float4( InPosition, 0, 1 );
    OutUV = InUV;

	float4 fVector = float4(1,2,3,4);
	int4 iVector = fVector;

	// bConst == true && atype(int) != 0, so this will evaluate to 5.
	TypeIndex[0] = 0;
	if (bConst && (TypeIndex[0] = atype(iVector.x)))
	{
		TypeIndex[0]++;
	}
	else
	{
		TypeIndex[0] += 2;
	}

	// !bConst == false, but HLSL doesn't short-circuit expressions!
	// This will evaluate to 6.
	TypeIndex[1] = 0;
	if (!bConst && (TypeIndex[1] = atype(iVector.x)))
	{
		TypeIndex[1]++;
	}
	else
	{
		TypeIndex[1] += 2;
	}

	// Again, HLSL doesn't perform ANY short-circuiting. Therefore ColorIndex
	// will be 0 even though bConst is true.
	TypeIndex[2] = 1;
	int i = bConst ? (1) : (TypeIndex[2] = atype(fVector.x));
}
