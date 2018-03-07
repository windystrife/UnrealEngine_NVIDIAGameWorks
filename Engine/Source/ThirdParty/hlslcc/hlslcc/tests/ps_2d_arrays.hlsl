#define NUM_MATERIAL_TEXCOORDS 2
#define NUM_TEXCOORD_SAMPLES 4

float EvaluateAttributeAtSample(float Attribute,uint SampleIndex) { return Attribute; }
float2 EvaluateAttributeAtSample(float2 Attribute,uint SampleIndex) { return Attribute; }
float3 EvaluateAttributeAtSample(float3 Attribute,uint SampleIndex) { return Attribute; }
float4 EvaluateAttributeAtSample(float4 Attribute,uint SampleIndex) { return Attribute; }

struct FTestStruct
{
	float2 TexCoords[NUM_MATERIAL_TEXCOORDS];
	float2 SampleTexCoords[NUM_TEXCOORD_SAMPLES][NUM_MATERIAL_TEXCOORDS];
};

struct FInterpolants
{
	float4 TexCoords[(NUM_MATERIAL_TEXCOORDS+1)/2] : TEXCOORD0;
};

FTestStruct GetTestStruct(FInterpolants Interpolants)
{
	FTestStruct Result = (FTestStruct)0;

	for(int CoordinateIndex = 0;CoordinateIndex <  NUM_MATERIAL_TEXCOORDS;CoordinateIndex += 2)
	{
		Result.TexCoords[CoordinateIndex] = Interpolants.TexCoords[CoordinateIndex/2].xy;

		for(uint SampleIndex = 0;SampleIndex <  NUM_TEXCOORD_SAMPLES;++SampleIndex)
		{
			Result.SampleTexCoords[SampleIndex][CoordinateIndex] = EvaluateAttributeAtSample(Interpolants.TexCoords[CoordinateIndex/2].xy,SampleIndex);
		}
		if(CoordinateIndex + 1 < NUM_MATERIAL_TEXCOORDS)
		{
			Result.TexCoords[CoordinateIndex + 1] = Interpolants.TexCoords[CoordinateIndex/2].wz;

			for(uint SampleIndex = 0;SampleIndex < NUM_TEXCOORD_SAMPLES;++SampleIndex)
			{
				Result.SampleTexCoords[SampleIndex][CoordinateIndex + 1] = EvaluateAttributeAtSample(Interpolants.TexCoords[CoordinateIndex/2].wz,SampleIndex);
			}
		}
	}

	return Result;
}

void TestMain(
	in FInterpolants Interps,
	out float4 OutColor: SV_Target0
	)
{
	FTestStruct Test = GetTestStruct(Interps);
	OutColor = float4(Test.SampleTexCoords[0][1].xy, Test.SampleTexCoords[3][0].xy);
}