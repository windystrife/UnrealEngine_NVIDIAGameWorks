// Tests all texture sampling functionality.

// Passing a texture object around.
float4 Texture2DSample(Texture2D Tex, SamplerState Sampler, float2 UV)
{
	return Tex.Sample(Sampler, UV);
}

struct FScreenVertexOutput
{
	float2 UV : TEXCOORD0;
	float4 Position : SV_POSITION;
};

Texture2D<float4> InTexture;
Texture2DMS InTextureMS;
Texture3D<float4> InVolumeTexture;
Texture2D<float> InShadowTexture;
SamplerState InTextureSampler;
SamplerComparisonState InShadowCompState;

void TestMain(
	FScreenVertexOutput Input,
	out float4 OutColor : SV_Target0,
	out float4 OutMisc : SV_Target1
	)
{
	vec4 Color = Texture2DSample(InTexture, InTextureSampler, Input.UV);
	Color += InTextureMS.Load(int2(Input.UV), 0);
	Color += InVolumeTexture.Sample(InTextureSampler, Input.Position.xyz);
	uint ShadowFactor = InShadowTexture.SampleCmpLevelZero(InShadowCompState, Input.Position.xy, Input.Position.z);

	const uint MipLevel = 3u;
	uint TextureWidth, TextureHeight, NumLevels;
	InTexture.GetDimensions(MipLevel, TextureWidth, TextureHeight, NumLevels);

	OutColor = float4(Color.rgb * float(ShadowFactor), Color.a);
	OutMisc = float4(TextureWidth, TextureHeight, 0, 0);
}
