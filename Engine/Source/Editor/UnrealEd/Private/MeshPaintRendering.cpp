// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	MeshPaintRendering.cpp: Mesh texture paint brush rendering
================================================================================*/

#include "MeshPaintRendering.h"
#include "ShaderParameters.h"
#include "RenderResource.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "RHIStaticStates.h"
#include "BatchedElements.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "TextureResource.h"
#include "Engine/TextureRenderTarget2D.h"
#include "PipelineStateCache.h"

namespace MeshPaintRendering
{

	/** Mesh paint vertex shader */
	class TMeshPaintVertexShader : public FGlobalShader
	{
		DECLARE_SHADER_TYPE( TMeshPaintVertexShader, Global );

	public:

		static bool ShouldCache( EShaderPlatform Platform )
		{
			return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && !IsConsolePlatform(Platform);
		}

		/** Default constructor. */
		TMeshPaintVertexShader() {}

		/** Initialization constructor. */
		TMeshPaintVertexShader( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
			: FGlobalShader( Initializer )
		{
			TransformParameter.Bind( Initializer.ParameterMap, TEXT( "c_Transform" ) );
		}

		virtual bool Serialize( FArchive& Ar ) override
		{
			bool bShaderHasOutdatedParameters = FGlobalShader::Serialize( Ar );
			Ar << TransformParameter;
			return bShaderHasOutdatedParameters;
		}

		void SetParameters(FRHICommandList& RHICmdList, const FMatrix& InTransform )
		{
			SetShaderValue(RHICmdList, GetVertexShader(), TransformParameter, InTransform );
		}

	private:

		FShaderParameter TransformParameter;
	};


	IMPLEMENT_SHADER_TYPE( , TMeshPaintVertexShader, TEXT( "/Engine/Private/MeshPaintVertexShader.usf" ), TEXT( "Main" ), SF_Vertex);



	/** Mesh paint pixel shader */
	class TMeshPaintPixelShader : public FGlobalShader
	{
		DECLARE_SHADER_TYPE( TMeshPaintPixelShader, Global );

	public:

		static bool ShouldCache(EShaderPlatform Platform)
		{
			return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && !IsConsolePlatform(Platform);
		}

		/** Default constructor. */
		TMeshPaintPixelShader() {}

		/** Initialization constructor. */
		TMeshPaintPixelShader( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
			: FGlobalShader( Initializer )
		{
			CloneTextureParameter.Bind( Initializer.ParameterMap, TEXT( "s_CloneTexture" ) );
			CloneTextureParameterSampler.Bind( Initializer.ParameterMap, TEXT( "s_CloneTextureSampler" ));
			WorldToBrushMatrixParameter.Bind( Initializer.ParameterMap, TEXT( "c_WorldToBrushMatrix" ) );
			BrushMetricsParameter.Bind( Initializer.ParameterMap, TEXT( "c_BrushMetrics" ) );
			BrushStrengthParameter.Bind( Initializer.ParameterMap, TEXT( "c_BrushStrength" ) );
			BrushColorParameter.Bind( Initializer.ParameterMap, TEXT( "c_BrushColor" ) );
			ChannelFlagsParameter.Bind( Initializer.ParameterMap, TEXT( "c_ChannelFlags") );
			GenerateMaskFlagParameter.Bind( Initializer.ParameterMap, TEXT( "c_GenerateMaskFlag") );
			GammaParameter.Bind( Initializer.ParameterMap, TEXT( "c_Gamma" ) );
		}

		virtual bool Serialize(FArchive& Ar) override
		{
			bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
			Ar << CloneTextureParameter;
			Ar << CloneTextureParameterSampler;
			Ar << WorldToBrushMatrixParameter;
			Ar << BrushMetricsParameter;
			Ar << BrushStrengthParameter;
			Ar << BrushColorParameter;
			Ar << ChannelFlagsParameter;
			Ar << GenerateMaskFlagParameter;
			Ar << GammaParameter;
			return bShaderHasOutdatedParameters;
		}

		void SetParameters(FRHICommandList& RHICmdList, const float InGamma, const FMeshPaintShaderParameters& InShaderParams )
		{
			const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				CloneTextureParameter,
				CloneTextureParameterSampler,
				TStaticSamplerState< SF_Point, AM_Clamp, AM_Clamp, AM_Clamp >::GetRHI(),
				InShaderParams.CloneTexture->GetRenderTargetResource()->TextureRHI );

			SetShaderValue(RHICmdList, ShaderRHI, WorldToBrushMatrixParameter, InShaderParams.WorldToBrushMatrix );

			FVector4 BrushMetrics;
			BrushMetrics.X = InShaderParams.BrushRadius;
			BrushMetrics.Y = InShaderParams.BrushRadialFalloffRange;
			BrushMetrics.Z = InShaderParams.BrushDepth;
			BrushMetrics.W = InShaderParams.BrushDepthFalloffRange;
			SetShaderValue(RHICmdList, ShaderRHI, BrushMetricsParameter, BrushMetrics );

			FVector4 BrushStrength4( InShaderParams.BrushStrength, 0.0f, 0.0f, 0.0f );
			SetShaderValue(RHICmdList, ShaderRHI, BrushStrengthParameter, BrushStrength4 );

			SetShaderValue(RHICmdList, ShaderRHI, BrushColorParameter, InShaderParams.BrushColor );

			FVector4 ChannelFlags;
			ChannelFlags.X = InShaderParams.RedChannelFlag;
			ChannelFlags.Y = InShaderParams.GreenChannelFlag;
			ChannelFlags.Z = InShaderParams.BlueChannelFlag;
			ChannelFlags.W = InShaderParams.AlphaChannelFlag;
			SetShaderValue(RHICmdList, ShaderRHI, ChannelFlagsParameter, ChannelFlags );
			
			float MaskVal = InShaderParams.GenerateMaskFlag ? 1.0f : 0.0f;
			SetShaderValue(RHICmdList, ShaderRHI, GenerateMaskFlagParameter, MaskVal );

			// @todo MeshPaint
			SetShaderValue(RHICmdList, ShaderRHI, GammaParameter, InGamma );
		}


	private:

		/** Texture that is a clone of the destination render target before we start drawing */
		FShaderResourceParameter CloneTextureParameter;
		FShaderResourceParameter CloneTextureParameterSampler;

		/** Brush -> World matrix */
		FShaderParameter WorldToBrushMatrixParameter;

		/** Brush metrics: x = radius, y = falloff range, z = depth, w = depth falloff range */
		FShaderParameter BrushMetricsParameter;

		/** Brush strength */
		FShaderParameter BrushStrengthParameter;

		/** Brush color */
		FShaderParameter BrushColorParameter;

		/** Flags that control paining individual channels: x = Red, y = Green, z = Blue, w = Alpha */
		FShaderParameter ChannelFlagsParameter;
		
		/** Flag to control brush mask generation or paint blending */
		FShaderParameter GenerateMaskFlagParameter;

		/** Gamma */
		// @todo MeshPaint: Remove this?
		FShaderParameter GammaParameter;
	};


	IMPLEMENT_SHADER_TYPE( , TMeshPaintPixelShader, TEXT( "/Engine/Private/MeshPaintPixelShader.usf" ), TEXT( "Main" ), SF_Pixel );


	/** Mesh paint dilate vertex shader */
	class TMeshPaintDilateVertexShader : public FGlobalShader
	{
		DECLARE_SHADER_TYPE( TMeshPaintDilateVertexShader, Global );

	public:

		static bool ShouldCache( EShaderPlatform Platform )
		{
			return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && !IsConsolePlatform(Platform);
		}

		/** Default constructor. */
		TMeshPaintDilateVertexShader() {}

		/** Initialization constructor. */
		TMeshPaintDilateVertexShader( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
			: FGlobalShader( Initializer )
		{
			TransformParameter.Bind( Initializer.ParameterMap, TEXT( "c_Transform" ) );
		}

		virtual bool Serialize( FArchive& Ar ) override
		{
			bool bShaderHasOutdatedParameters = FShader::Serialize( Ar );
			Ar << TransformParameter;
			return bShaderHasOutdatedParameters;
		}

		void SetParameters(FRHICommandList& RHICmdList, const FMatrix& InTransform )
		{
			SetShaderValue(RHICmdList, GetVertexShader(), TransformParameter, InTransform );
		}

	private:

		FShaderParameter TransformParameter;
	};


	IMPLEMENT_SHADER_TYPE( , TMeshPaintDilateVertexShader, TEXT( "/Engine/Private/meshpaintdilatevertexshader.usf" ), TEXT( "Main" ), SF_Vertex );



	/** Mesh paint pixel shader */
	class TMeshPaintDilatePixelShader : public FGlobalShader
	{
		DECLARE_SHADER_TYPE( TMeshPaintDilatePixelShader, Global );

	public:

		static bool ShouldCache(EShaderPlatform Platform)
		{
			return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4) && !IsConsolePlatform(Platform);
		}

		/** Default constructor. */
		TMeshPaintDilatePixelShader() {}

		/** Initialization constructor. */
		TMeshPaintDilatePixelShader( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
			: FGlobalShader( Initializer )
		{
			Texture0Parameter.Bind( Initializer.ParameterMap, TEXT( "Texture0" ) );
			Texture0ParameterSampler.Bind( Initializer.ParameterMap, TEXT( "Texture0Sampler" ) );
			Texture1Parameter.Bind( Initializer.ParameterMap, TEXT( "Texture1") );
			Texture1ParameterSampler.Bind( Initializer.ParameterMap, TEXT( "Texture1Sampler") );
			Texture2Parameter.Bind( Initializer.ParameterMap, TEXT( "Texture2") );
			Texture2ParameterSampler.Bind( Initializer.ParameterMap, TEXT( "Texture2Sampler") );
			WidthPixelOffsetParameter.Bind( Initializer.ParameterMap, TEXT( "WidthPixelOffset") );
			HeightPixelOffsetParameter.Bind( Initializer.ParameterMap, TEXT( "HeightPixelOffset") );
			GammaParameter.Bind( Initializer.ParameterMap, TEXT( "Gamma" ) );
		}

		virtual bool Serialize(FArchive& Ar) override
		{
			bool bShaderHasOutdatedParameters = FShader::Serialize(Ar);
			Ar << Texture0Parameter;
			Ar << Texture0ParameterSampler;
			Ar << Texture1Parameter;
			Ar << Texture1ParameterSampler;
			Ar << Texture2Parameter;
			Ar << Texture2ParameterSampler;
			Ar << WidthPixelOffsetParameter;
			Ar << HeightPixelOffsetParameter;
			Ar << GammaParameter;
			return bShaderHasOutdatedParameters;
		}

		void SetParameters(FRHICommandList& RHICmdList, const float InGamma, const FMeshPaintDilateShaderParameters& InShaderParams )
		{
			const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				Texture0Parameter,
				Texture0ParameterSampler,
				TStaticSamplerState< SF_Point, AM_Clamp, AM_Clamp, AM_Clamp >::GetRHI(),
				InShaderParams.Texture0->GetRenderTargetResource()->TextureRHI );

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				Texture1Parameter,
				Texture1ParameterSampler,
				TStaticSamplerState< SF_Point, AM_Clamp, AM_Clamp, AM_Clamp >::GetRHI(),
				InShaderParams.Texture1->GetRenderTargetResource()->TextureRHI );

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				Texture2Parameter,
				Texture2ParameterSampler,
				TStaticSamplerState< SF_Point, AM_Clamp, AM_Clamp, AM_Clamp >::GetRHI(),
				InShaderParams.Texture2->GetRenderTargetResource()->TextureRHI );

			SetShaderValue(RHICmdList, ShaderRHI, WidthPixelOffsetParameter, InShaderParams.WidthPixelOffset );
			SetShaderValue(RHICmdList, ShaderRHI, HeightPixelOffsetParameter, InShaderParams.HeightPixelOffset );
			SetShaderValue(RHICmdList, ShaderRHI, GammaParameter, InGamma );
		}


	private:

		/** Texture0 */
		FShaderResourceParameter Texture0Parameter;
		FShaderResourceParameter Texture0ParameterSampler;

		/** Texture1 */
		FShaderResourceParameter Texture1Parameter;
		FShaderResourceParameter Texture1ParameterSampler;

		/** Texture2 */
		FShaderResourceParameter Texture2Parameter;
		FShaderResourceParameter Texture2ParameterSampler;

		/** Pixel size width */
		FShaderParameter WidthPixelOffsetParameter;
		
		/** Pixel size height */
		FShaderParameter HeightPixelOffsetParameter;

		/** Gamma */
		// @todo MeshPaint: Remove this?
		FShaderParameter GammaParameter;
	};


	IMPLEMENT_SHADER_TYPE( , TMeshPaintDilatePixelShader, TEXT( "/Engine/Private/meshpaintdilatepixelshader.usf" ), TEXT( "Main" ), SF_Pixel );


	/** Mesh paint vertex format */
	typedef FSimpleElementVertex FMeshPaintVertex;


	/** Mesh paint vertex declaration resource */
	typedef FSimpleElementVertexDeclaration FMeshPaintVertexDeclaration;


	/** Global mesh paint vertex declaration resource */
	TGlobalResource< FMeshPaintVertexDeclaration > GMeshPaintVertexDeclaration;



	typedef FSimpleElementVertex FMeshPaintDilateVertex;
	typedef FSimpleElementVertexDeclaration FMeshPaintDilateVertexDeclaration;
	TGlobalResource< FMeshPaintDilateVertexDeclaration > GMeshPaintDilateVertexDeclaration;


	/** Binds the mesh paint vertex and pixel shaders to the graphics device */
	void SetMeshPaintShaders(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, ERHIFeatureLevel::Type InFeatureLevel, const FMatrix& InTransform,
										   const float InGamma,
										   const FMeshPaintShaderParameters& InShaderParams )
	{
		TShaderMapRef< TMeshPaintVertexShader > VertexShader(GetGlobalShaderMap(InFeatureLevel));
		TShaderMapRef< TMeshPaintPixelShader > PixelShader(GetGlobalShaderMap(InFeatureLevel));

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GMeshPaintDilateVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, EApplyRendertargetOption::ForceApply);

		// Set vertex shader parameters
		VertexShader->SetParameters(RHICmdList, InTransform );
		
		// Set pixel shader parameters
		PixelShader->SetParameters(RHICmdList, InGamma, InShaderParams );

		// @todo MeshPaint: Make sure blending/color writes are setup so we can write to ALPHA if needed!
	}

	/** Binds the mesh paint vertex and pixel shaders to the graphics device */
	void SetMeshPaintDilateShaders(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, ERHIFeatureLevel::Type InFeatureLevel, const FMatrix& InTransform,
												 const float InGamma,
												 const FMeshPaintDilateShaderParameters& InShaderParams )
	{
		TShaderMapRef< TMeshPaintDilateVertexShader > VertexShader(GetGlobalShaderMap(InFeatureLevel));
		TShaderMapRef< TMeshPaintDilatePixelShader > PixelShader(GetGlobalShaderMap(InFeatureLevel));

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GMeshPaintDilateVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, EApplyRendertargetOption::ForceApply);

		// Set vertex shader parameters
		VertexShader->SetParameters(RHICmdList, InTransform );

		// Set pixel shader parameters
		PixelShader->SetParameters(RHICmdList, InGamma, InShaderParams );
	}

}

