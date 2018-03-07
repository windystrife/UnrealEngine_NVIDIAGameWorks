/*------------------------------------------------------------------------------
	From LocalVertexFactory.usf
------------------------------------------------------------------------------*/

#define NUM_MATERIAL_TEXCOORDS 3
#define FCOLOR_COMPONENT_SWIZZLE .bgra

struct FVertexFactoryInput
{
	float4	Position	: ATTRIBUTE0;
	half3	TangentX	: ATTRIBUTE1;
	// TangentZ.w contains sign of tangent basis determinant
	half4	TangentZ	: ATTRIBUTE2;
	half4	Color		: ATTRIBUTE3;

#if NUM_MATERIAL_TEXCOORDS
	float2	TexCoords[NUM_MATERIAL_TEXCOORDS] : ATTRIBUTE4;
#endif
};

struct FVertexFactoryInterpolantsVSToPS
{
	float4 TangentToWorld0 : TEXCOORD10_centroid;
	float4 TangentToWorld2	: TEXCOORD11_centroid;
	float4 Color : COLOR0;

#if NUM_MATERIAL_TEXCOORDS
	float4	TexCoords[(NUM_MATERIAL_TEXCOORDS+1)/2]	: TEXCOORD0;
#endif
};

struct FVertexFactoryIntermediates
{
	float4 Color;
};

FVertexFactoryIntermediates GetVertexFactoryIntermediates(FVertexFactoryInput Input)
{
	FVertexFactoryIntermediates Intermediates;
	Intermediates.Color = Input.Color FCOLOR_COMPONENT_SWIZZLE;
	return Intermediates;
}

float4 VertexFactoryGetWorldPosition(FVertexFactoryInput Input, FVertexFactoryIntermediates Intermediates)
{
	return Input.Position;
}

FVertexFactoryInterpolantsVSToPS VertexFactoryGetInterpolantsVSToPS(FVertexFactoryInput Input, FVertexFactoryIntermediates Intermediates)
{
	FVertexFactoryInterpolantsVSToPS Interpolants;
#if NUM_MATERIAL_TEXCOORDS
	// Ensure the unused components of the last packed texture coordinate are initialized.
	Interpolants.TexCoords[(NUM_MATERIAL_TEXCOORDS + 1) / 2 - 1] = 0;

	for(int CoordinateIndex = 0;CoordinateIndex < NUM_MATERIAL_TEXCOORDS;CoordinateIndex += 2)
	{
		Interpolants.TexCoords[CoordinateIndex / 2].xy = Input.TexCoords[CoordinateIndex];
		if(CoordinateIndex + 1 < NUM_MATERIAL_TEXCOORDS)
		{
			Interpolants.TexCoords[CoordinateIndex / 2].wz = Input.TexCoords[CoordinateIndex + 1];
		}
	}

	for(int CoordinateIndex = 0;CoordinateIndex < NUM_MATERIAL_TEXCOORDS;CoordinateIndex += 2)
	{
		Interpolants.TexCoords[CoordinateIndex / 2].xy += Input.TexCoords[CoordinateIndex];
		if(CoordinateIndex + 1 < NUM_MATERIAL_TEXCOORDS)
		{
			Interpolants.TexCoords[CoordinateIndex / 2].wz += Input.TexCoords[CoordinateIndex + 1];
		}
	}
#endif

	Interpolants.TangentToWorld0.xyz = Input.TangentX;
	Interpolants.TangentToWorld0.w = 0;
	Interpolants.TangentToWorld2 = Input.TangentZ;
	Interpolants.Color = Intermediates.Color;

	return Interpolants;
}

struct FInputVS
{
	FVertexFactoryInput VFInput;
	float4 ExtraStuff : ATTRIBUTE10;
};

struct FOutputVS
{
	FVertexFactoryInterpolantsVSToPS VFInterpolants;
	float4 ExtraStuff : INTERP0;
	float4 Position : SV_Position;
};

void TestMain(
	in FInputVS In,
	out FOutputVS Out
	)
{
	Out = (FOutputVS)0;
	FVertexFactoryIntermediates VFIntermediates = GetVertexFactoryIntermediates(In.VFInput);
	Out.VFInterpolants = VertexFactoryGetInterpolantsVSToPS(In.VFInput, VFIntermediates);
	Out.ExtraStuff = In.ExtraStuff;
	Out.Position = VertexFactoryGetWorldPosition(In.VFInput, VFIntermediates);
}
