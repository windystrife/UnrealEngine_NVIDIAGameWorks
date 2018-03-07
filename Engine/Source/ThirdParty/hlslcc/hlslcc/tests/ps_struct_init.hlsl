struct FTest
{
	float3 Vector;
	float Scalar;
	float3x3 Matrix;
	int IntArray[3];
};

void TestMain(out float4 OutColor[2] : SV_Target0)
{
	FTest TestA =
	{
		{ 1.5f, 1.5f, 2.5f },
		3.4f,
		{
			{ 1, 0, 0 },
			{ 0, 2, 0 },
			{ 0, 0, 3 }
		},
		{ 4, 5, 6}
	};

	FTest TestB =
	{
		 1.5f, float2(1.5f, 2.5f),
		3.4f,
		{
			{ 1, 0, 0, 0 },
			{ 2, 0 },
			{ 0, 0, 3 }
		},
		{ 4, 5 }, 6 
	};

	OutColor[0] = float4(TestA.Vector.z, TestA.Scalar, TestA.Matrix[2][2], TestA.IntArray[2]);
	OutColor[1] = float4(TestB.Vector.z, TestB.Scalar, TestB.Matrix[2][2], TestB.IntArray[2]);
}