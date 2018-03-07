#define TEST_FLOAT(intrin) \
	TestMatrix = intrin(fMatrix); \
	TestVector = intrin(fVector); \
	TestVector3 = intrin(fVector3); \
	TestScalar = intrin(fScalar);
#define TEST_INT(intrin) \
	TestIVector = intrin(iVector); \
	TestIVector3 = intrin(iVector3); \
	TestIScalar = intrin(iScalar);
#define TEST_UINT(intrin) \
	TestUVector = intrin(uVector); \
	TestUVector3 = intrin(uVector3); \
	TestUScalar = intrin(uScalar);
#define TEST_ALL(intrin) \
	TEST_FLOAT(intrin) \
	TEST_INT(intrin) \
	TEST_UINT(intrin)

#define TEST_FLOAT2(intrin) \
	TestMatrix = intrin(fMatrix,fMatrix); \
	TestVector = intrin(fVector,fVector); \
	TestVector3 = intrin(fVector3,fVector3); \
	TestScalar = intrin(fScalar,fScalar);
#define TEST_INT2(intrin) \
	TestIVector = intrin(iVector,iVector); \
	TestIVector3 = intrin(iVector3,iVector3); \
	TestIScalar = intrin(iScalar,iScalar);
#define TEST_UINT2(intrin) \
	TestUVector = intrin(uVector,uVector); \
	TestUVector3 = intrin(uVector3,uVector3); \
	TestUScalar = intrin(uScalar,uScalar);
#define TEST_ALL2(intrin) \
	TEST_FLOAT2(intrin) \
	TEST_INT2(intrin) \
	TEST_UINT2(intrin)

#define TEST_FLOAT3(intrin) \
	TestMatrix = intrin(fMatrix,fMatrix,fMatrix); \
	TestVector = intrin(fVector,fVector,fVector); \
	TestVector3 = intrin(fVector3,fVector3,fVector3); \
	TestScalar = intrin(fScalar,fScalar,fScalar);
#define TEST_INT3(intrin) \
	TestIVector = intrin(iVector,iVector,iVector); \
	TestIVector3 = intrin(iVector3,iVector3,iVector3); \
	TestIScalar = intrin(iScalar,iScalar,iScalar);
#define TEST_UINT3(intrin) \
	TestUVector = intrin(uVector,uVector,uVector); \
	TestUVector3 = intrin(uVector3,uVector3,uVector3); \
	TestUScalar = intrin(uScalar,uScalar,uScalar);
#define TEST_ALL3(intrin) \
	TEST_FLOAT3(intrin) \
	TEST_INT3(intrin) \
	TEST_UINT3(intrin)

#define TEST_FLOAT_V(intrin) \
	TestVector = intrin(fVector); \
	TestVector3 = intrin(fVector3); \
	TestScalar = intrin(fScalar);
#define TEST_FLOAT2_V(intrin) \
	TestVector = intrin(fVector,fVector); \
	TestVector3 = intrin(fVector3,fVector3); \
	TestScalar = intrin(fScalar,fScalar);
#define TEST_FLOAT3_V(intrin) \
	TestVector = intrin(fVector,fVector,fVector); \
	TestVector3 = intrin(fVector3,fVector3,fVector3); \
	TestScalar = intrin(fScalar,fScalar,fScalar);

#define TEST_ALL_V(intrin) \
	TestVector = intrin(fVector); \
	TestVector3 = intrin(fVector3); \
	TestIVector = intrin(iVector); \
	TestIVector3 = intrin(iVector3); \
	TestUVector = intrin(uVector); \
	TestUVector3 = intrin(uVector3);

#define TEST_ALL2_V(intrin) \
	TestVector = intrin(fVector,fVector); \
	TestVector3 = intrin(fVector3,fVector3); \
	TestIVector = intrin(iVector,iVector); \
	TestIVector3 = intrin(iVector3,iVector3); \
	TestUVector = intrin(uVector,uVector); \
	TestUVector3 = intrin(uVector3,uVector3);

#define TEST_ALL3_V(intrin) \
	TestVector = intrin(fVector,fVector,fVector); \
	TestVector3 = intrin(fVector3,fVector3,fVector3); \
	TestIVector = intrin(iVector,iVector,iVector); \
	TestIVector3 = intrin(iVector3,iVector3,iVector3); \
	TestUVector = intrin(uVector,uVector,uVector); \
	TestUVector3 = intrin(uVector3,uVector3,uVector3);

// Function signatures can hide intrinsics.
float sin(float a, float b, float c)
{
	return sin(a) + sin(b) * sin(c);
}

void TestIntrinsics(
	float4x4 fMatrix, float4 fVector, float3 fVector3, float fScalar,
	int4 iVector, int3 iVector3, int iScalar,
	uint4 uVector, uint3 uVector3, uint uScalar
	)
{
	float4x4 TestMatrix;
	float4 TestVector;
	float3 TestVector3;
	float TestScalar;
	int4 TestIVector;
	int3 TestIVector3;
	int TestIScalar;
	uint4 TestUVector;
	uint3 TestUVector3;
	uint TestUScalar;

	TestVector3 = cross(fVector,fVector3);
	TestIVector3 = cross(iVector,iVector3);
	TestUVector3 = cross(uVector,uVector3);

	TEST_ALL(radians);
	TEST_ALL(degrees);
	TEST_ALL(sin);
	TEST_ALL(cos);
	TEST_ALL(tan);
	TEST_ALL(asin);
	TEST_ALL(acos);
	TEST_ALL(atan);
	TEST_ALL(sinh);
	TEST_ALL(cosh);
	TEST_ALL(tanh);
	TEST_ALL2(atan2);

	TEST_ALL2(pow);
	TEST_ALL(exp);
	TEST_ALL(log);
	TEST_ALL(exp2);
	TEST_ALL(log2);
	TEST_ALL(sqrt);
	TEST_ALL(rsqrt);

	TEST_ALL(abs);
	TEST_ALL(sign);
	TEST_ALL(floor);
	TEST_ALL(trunc);
	TEST_ALL(round);
	TEST_ALL(ceil);
	TEST_ALL(frac);
	TEST_ALL2(fmod);
	TEST_ALL2_V(modf);
	TEST_ALL2(min);
	TEST_ALL2(max);
	TEST_ALL3(clamp);
	TEST_ALL(saturate);
	TEST_ALL3(lerp);
	TEST_ALL2(step);
	TEST_ALL3(smoothstep);
	TEST_ALL_V(isnan);
	TEST_ALL_V(isinf);
	TEST_ALL_V(isfinite);

	TEST_ALL_V(length);
	TEST_ALL2_V(distance);
	TEST_ALL2_V(dot);
	TEST_ALL_V(normalize);
	TEST_ALL3_V(faceforward);
	TEST_ALL2_V(reflect);
	TEST_ALL3_V(refract);

	TEST_ALL_V(ddx);
	TEST_ALL_V(ddy);
	TEST_ALL_V(fwidth);

	TEST_ALL_V(all);
	TEST_ALL_V(any);
	TEST_ALL(rcp);

	TestScalar = sin(fScalar,fScalar,fScalar);
}

void TestMain(
	in float4 InPosition : ATTRIBUTE0,
	out float4 OutPosition : SV_Position
	)
{
	OutPosition = InPosition;
}
