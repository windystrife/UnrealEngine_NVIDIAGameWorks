struct FVertexOutputInner
{
	float2 Interp0 : INTERP0;
	uint3 Interp1 : INTERP1;
};

struct FVertexOutput
{
	FVertexOutputInner Inner;
	float2 UV : TEXCOORD0;
	float4 Position : SV_Position;
};

struct FTestStructA
{
	float2 Field1;
	float2 Field2;
};

struct FTestStructB
{
	float3 Field1;
	float3 Field2;
	float3 Field3;
};

struct FMyFloat2
{
	float X;
	float Y;
};

struct FTestStructC
{
	FMyFloat2 Field1;
	FMyFloat2 Field2;
};

void Test(
	float4x4 InMatrix, float4 InVector, float3 InVector3, float InScalar,
	int4 InInt4, float3x3 InMatrix3, float3x4 InMatrix34, FTestStructA InA,
	FTestStructB InB
	)
{

	float4x4 TempC = InVector; // Not allowed.
	float4 TempE = InMatrix; // Not allowed.
	FTestStructB TempK = (FTestStructB)InMatrix; // Not allowed.
	FTestStructB TempP = (FTestStructB)InA; // Not allowed.
	float4x3 TempU = (float4x3)InMatrix34; // Not allowed.
	float4 Temp2 = InMatrix * InVector;
	float4x3 Temp10 = InMatrix34;
}

void TestMain(
	float2 InUV : ATTRIBUTE0,
	out FVertexOutput Out
	)
{
	float3 TempVec3 = 3.0f;
	FVertexOutput TempOut = (FVertexOutput)0;
	TempOut.UV = InUV;
	TempOut.Position.xy = InUV.xy;
	TempOut.Position.zw = TempVec3.zw;
	Out = TempOut;
}
