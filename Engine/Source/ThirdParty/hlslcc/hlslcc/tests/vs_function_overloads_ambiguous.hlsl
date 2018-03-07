float4x4 UniformMatrix;

// Single Argument.
int foo(int3 a) { return 0; }
int foo(float4 a) { return 1; }
int foo(float2 a) { return 2; }
int foo(float a) { return 3; }
int foo(uint2 a) { return 4; }

int bar(float a) { return 0; }
int bar(float2 a) { return 1; }
int bar(int2 a) { return 2; }

int baz(int2 a) { return 0; }
int baz(uint2 a) { return 1; }

// Two Arguments.
int foo(float4 a, int3 b) { return 0; }
int foo(int2 a, float2 b) { return 1; }
int foo(float2 b, float3 a) { return 2; }
int foo(float b, uint3 a) { return 3; }
int foo(int2 a, int4 b) { return 4; }
int foo(float a, int4 b) { return 5; }
int foo(float2 a, int4 b) { return 6; }

// Three Arguments.
int foo(float2 a, float2 b, float c) { return 0; }
int foo(float3 a, float3 b, float c) { return 1; }
int foo(float4 a, float4 b, float c) { return 2; }
int foo(float4 a, float2 b, float c) { return 3; }
int foo(int3 a, float4 b, float c) { return 4; }

void TestMain(
	float2 InPosition : ATTRIBUTE0,
	float2 InUV : ATTRIBUTE1,
	out float4 OutUV : TEXCOORD0,
	out float4 OutMisc : TEXCOORD1,
	out int OneParam[7] : ONEPARAM0,
	out int TwoParam[3] : TWOPARAM0,
	out int ThreeParam[8] : THREEPARAM0,
	out float4 OutPosition : SV_POSITION
	)
{
	OutPosition = float4( InPosition, sin(InUV.x), dot(InPosition,InPosition) );

	float2 IntUV, FracUV;
	FracUV = modf(InUV, IntUV);
    OutUV = float4(IntUV, FracUV);

	OutMisc = saturate(abs(UniformMatrix))[2];

	float4 fVector = float4(0,1,2,3);
	int4 iVector = int4(0,1,2,3);
	uint4 uVector = uint4(0,1,2,3);

	OneParam[4] = bar(fVector);							// ambiguous
	OneParam[6] = baz(fVector);							// ambiguous

	TwoParam[1] = foo(fVector.xyz, iVector);			// ambiguous

	ThreeParam[1] = foo(fVector, fVector.xyz, fVector);	// ambiguous
	ThreeParam[7] = foo(iVector, fVector.xyz, fVector.x);	// ambiguous
}
