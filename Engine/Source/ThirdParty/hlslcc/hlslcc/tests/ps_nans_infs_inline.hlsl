float4 TexCoordScaleBias;
Texture2D ColorTexture;
SamplerState ColorTextureSampler;

float GetTexCoordDivisor()
{
	return 0.0f;
}

void TestMain(
	in float2 TexCoord : TEXCOORD0,
	out float4 OutColor : SV_Target0
	)
{
	float2 ActualTexCoord = TexCoord * TexCoordScaleBias.xy + TexCoordScaleBias.zw;
	ActualTexCoord /= GetTexCoordDivisor();
	OutColor = ColorTexture.Sample(ColorTextureSampler, ActualTexCoord);
}
