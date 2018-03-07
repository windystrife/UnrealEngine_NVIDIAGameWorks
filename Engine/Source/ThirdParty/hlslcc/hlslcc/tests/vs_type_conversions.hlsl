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

void Other(
	float4x4 InMatrix, float4 InVector, float3 InVector3, float InScalar,
	int4 InInt4, float3x3 InMatrix3, float3x4 InMatrix34, FTestStructA InA,
	FTestStructB InB
	)
{

	float4x4 TempA = InScalar;
	float4x4 TempB = InMatrix * InScalar;
	float TempD = InMatrix;
	float3x3 TempF = InMatrix;

	float TempG = InScalar * InMatrix;
	FTestStructB TempH = (FTestStructB)2;
	FTestStructA TempI = (FTestStructA)InVector;
	FTestStructB TempJ = (FTestStructB)InMatrix3;
	float TempL = (float)InB;
	float4 TempM = (float4)InB;
	float3x3 TempN = (float3x3)InB;
	FTestStructA TempO = (FTestStructA)InB;
	FTestStructC TempQ = (FTestStructC)InVector;
	float2x2 TempR = InVector;

	float4x4 Temp3 = InScalar * InMatrix;
	float3 Temp5 = InVector * InVector3;
	float2 Temp6 = InVector * InVector3;
	float3 Temp7 = InVector * InInt4;
	int3 Temp8 = InVector * InInt4;
	uint3 Temp9 = InVector3 * InInt4;
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

	float f; int i; uint u; bool b;

	// Scalar conversions.
	f = 1.0f;
	i = f;
	u = f;
	b = f;

	i = -3;
	f = i;
	u = i;
	b = i;

	u = 4;
	f = u;
	i = u;
	b = u;

	b = true;
	f = b;
	i = b;
	u = b;

	// Vector conversions, same size.
	float2 f2; int2 i2; uint2 u2; bool2 b2;

	f2 = 1.0f;
	i2 = f2;
	u2 = f2;
	b2 = f2;

	i2 = -3;
	f2 = i2;
	u2 = i2;
	b2 = i2;
	u2 = 4;
	f2 = u2;
	i2 = u2;
	b2 = u2;

	b2 = true;
	f2 = b2;
	i2 = b2;
	u2 = b2;

	// Vector conversions, same size.
	float3 f3; int3 i3; uint3 u3; bool3 b3;

	f3 = 1.0f;
	i3 = f3;
	u3 = f3;
	b3 = f3;

	i3 = -3;
	f3 = i3;
	u3 = i3;
	b3 = i3;

	u3 = 4;
	f3 = u3;
	i3 = u3;
	b3 = u3;

	b3 = true;
	f3 = b3;
	i3 = b3;
	u3 = b3;

	// Vector conversions, larger to smaller.
	f2 = f3;
	i2 = f3;
	u2 = f3;
	b2 = f3;

	f2 = i3;
	i2 = i3;
	u2 = i3;
	b2 = i3;

	f2 = u3;
	i2 = u3;
	u2 = u3;
	b2 = u3;

	f2 = b3;
	i2 = b3;
	u2 = b3;
	b2 = b3;

	// Vector conversions, larger to smaller. Explicit.
	f2 = (float2)f3;
	i2 = (int2)f3;
	u2 = (uint2)f3;
	b2 = (bool2)f3;

	f2 = (float2)i3;
	i2 = (int2)i3;
	u2 = (uint2)i3;
	b2 = (bool2)i3;

	f2 = (float2)u3;
	i2 = (int2)u3;
	u2 = (uint2)u3;
	b2 = (bool2)u3;

	f2 = (float2)b3;
	i2 = (int2)b3;
	u2 = (uint2)b3;
	b2 = (bool2)b3;

	// Vector -> scalar.
	f = f3;
	i = f3;
	u = f3;
	b = f3;

	f = i3;
	i = i3;
	u = i3;
	b = i3;

	f = u3;
	i = u3;
	u = u3;
	b = u3;

	f = b3;
	i = b3;
	u = b3;
	b = b3;

	TempOut.Position.zw = f2;

	Out = TempOut;
}
