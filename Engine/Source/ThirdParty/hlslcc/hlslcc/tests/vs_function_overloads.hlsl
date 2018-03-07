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

	OneParam[0] = foo(fVector);								// 1
	OneParam[1] = foo(fVector.xyz);							// 0
	OneParam[2] = foo(iVector);								// 1
	OneParam[3] = foo(iVector.xy);							// 4
	//OneParam[4] = bar(fVector);							// ambiguous
	OneParam[5] = bar(uVector);								// 2
	//OneParam[6] = baz(fVector);							// ambiguous

	TwoParam[0] = foo(iVector, fVector.xyz);				// 0
	//TwoParam[1] = foo(fVector.xyz, iVector);				// ambiguous
	TwoParam[2] = foo(fVector.xyz, iVector.xyz);			// 3

	ThreeParam[0] = foo(fVector.xyz, fVector, fVector);		// 4
	//ThreeParam[1] = foo(fVector, fVector.xyz, fVector);	// ambiguous
	ThreeParam[2] = foo(fVector.xy, fVector, fVector);		// 0
	ThreeParam[3] = foo(fVector, fVector, fVector.xy);		// 2
	ThreeParam[4] = foo(fVector, fVector.xy, fVector.x);	// 3
	ThreeParam[5] = foo(iVector, fVector, fVector.x);		// 2
	ThreeParam[6] = foo(iVector.xyz, fVector, fVector.x);	// 4
	//ThreeParam[7] = foo(iVector, fVector.xyz, fVector.x);	// ambiguous
}
