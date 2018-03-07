// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GammaCorrectionCommon.hlsl"

cbuffer PerElementVSConstants
{
	matrix WorldViewProjection;
}

struct VertexOut
{
	float4 Position : SV_POSITION;
	float4 Color : COLOR0;
	float4 TextureCoordinates : TEXCOORD0;
};

VertexOut Main(
	in float2 InPosition : POSITION,
	in float4 InTextureCoordinates : TEXCOORD0,
	in float2 MaterialTexCoords : TEXCOORD1,
	in float4 InColor : COLOR0
	)
{
	VertexOut Out;

	Out.Position = mul( WorldViewProjection, float4( InPosition.xy, 0, 1 ) );

	Out.TextureCoordinates = InTextureCoordinates;
	
	Out.Color.rgb = sRGBToLinear(InColor.rgb);
	Out.Color.a = InColor.a;

	return Out;
}
