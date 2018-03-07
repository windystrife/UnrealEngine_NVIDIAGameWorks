cbuffer View
{
	struct
	{
	float4x4 TranslatedWorldToClip;
	float4x4 TranslatedWorldToView;
	float4x4 ViewToTranslatedWorld;
	float4x4 ViewToClip;
	float4x4 ClipToTranslatedWorld;
	float3 ViewForward;
	float1 _PrePadding332;
	float3 ViewUp;
	float1 _PrePadding348;
	float3 ViewRight;
	float1 _PrePadding364;
	float4 InvDeviceZToWorldZTransform;
	float4 ScreenPositionScaleBias;
	float4 ViewSizeAndSceneTexelSize;
	float4 ViewOrigin;
	float4 TranslatedViewOrigin;
	float4 DiffuseOverrideParameter;
	float4 SpecularOverrideParameter;
	float3 PreViewTranslation;
	float1 _PrePadding492;
	float3 ViewOriginDelta;
	float CullingSign;
	float NearPlane;
	float AdaptiveTessellationFactor;
	float GameTime;
	float RealTime;
	float4 UpperSkyColor;
	float4 LowerSkyColor;
	float3 AmbientColor;
	float SkyFactor;
	float4 TranslucencyLightingVolumeMin[2];
	float4 TranslucencyLightingVolumeInvSize[2];
	float DepthOfFieldFocalDistance;
	float DepthOfFieldScale;
	float DepthOfFieldFocalLength;
	float DepthOfFieldFocalRegion;
	} View;
}

cbuffer Primitive
{
	struct
	{
	float4x4 LocalToWorld;
	float4x4 WorldToLocal;
	float4 ObjectWorldPositionAndRadius;
	float3 ActorWorldPosition;
	float1 _PrePadding156;
	float4 ObjectOrientation;
	float4 DisplacementNonUniformScale;
	float LocalToWorldDeterminantSign;
	float DecalReceiverMask;
	} Primitive;
}

float4x4 GetLocalToTranslatedWorld()
{
	float4x4 Result = Primitive.LocalToWorld;
	Result._m03_m13_m23 += View.PreViewTranslation;
	//Result._14_24_34_44 += View.PreViewTranslation;
	return Result;
}

struct FVertexFactoryInput
{
	float4 Position : ATTRIBUTE0;
	centroid noperspective half3 TangentX : ATTRIBUTE1;

	half4 TangentZ : ATTRIBUTE2;
	linear half4 Color : ATTRIBUTE3;


	float2 TexCoords[ 1 ] : ATTRIBUTE4;
};

struct FVertexFactoryIntermediates
{
	float4 Color;
};

float4 CalcWorldPosition(float4 Position)
{
	return mul(Position, GetLocalToTranslatedWorld());
}

FVertexFactoryIntermediates GetVertexFactoryIntermediates(FVertexFactoryInput Input)
{
	FVertexFactoryIntermediates Intermediates;
	Intermediates.Color = Input.Color .bgra;
	return Intermediates;
}

float4 VertexFactoryGetWorldPosition(FVertexFactoryInput Input, FVertexFactoryIntermediates Intermediates)
{
	return CalcWorldPosition(Input.Position);
}

void TestMain(
	in FVertexFactoryInput Input,
	centroid out noperspective float4 OutColor0 : COLOR0,
	out nointerpolation float4 OutColor1 : COLOR1,
	out float4 OutPosition : SV_Position
	)
{
	FVertexFactoryIntermediates VFIntermediates = GetVertexFactoryIntermediates(Input);
	OutPosition = VertexFactoryGetWorldPosition(Input, VFIntermediates);
	OutColor0 = 1;
	OutColor1 = 2;
}