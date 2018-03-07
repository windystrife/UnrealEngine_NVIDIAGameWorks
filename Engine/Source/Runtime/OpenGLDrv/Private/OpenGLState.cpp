// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLState.cpp: OpenGL state implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "Serialization/MemoryWriter.h"
#include "RHI.h"
#include "ShaderCache.h"
#include "OpenGLDrv.h"

GLint GMaxOpenGLTextureFilterAnisotropic = 1;

// Similar to sizeof(FSamplerStateInitializerRHI), but without any padding added by the compiler
static uint32 SizeOfSamplerStateInitializer = 0;

static FORCEINLINE void CalculateSizeOfSamplerStateInitializer()
{
	if (SizeOfSamplerStateInitializer == 0)
	{
		TArray<uint8> Data;
		FMemoryWriter Writer(Data);
		FSamplerStateInitializerRHI State;
		Writer << State;
		SizeOfSamplerStateInitializer = Data.Num();
	}
}

static bool operator==(const FSamplerStateInitializerRHI& A, const FSamplerStateInitializerRHI& B)
{
	return FMemory::Memcmp(&A, &B, SizeOfSamplerStateInitializer) == 0;
}

static FORCEINLINE uint32 GetTypeHash(const FSamplerStateInitializerRHI& SamplerState)
{
	return FCrc::MemCrc_DEPRECATED(&SamplerState, SizeOfSamplerStateInitializer);
}

// Hash of sampler states, used for caching sampler states and texture objects
static TMap<FSamplerStateInitializerRHI, FOpenGLSamplerState*> GSamplerStateCache;

void EmptyGLSamplerStateCache()
{
	for (auto Iter = GSamplerStateCache.CreateIterator(); Iter; ++Iter )
	{
		auto* State = Iter.Value();
		// Manually release
		State->Release();
	}

	GSamplerStateCache.Empty();
}

static GLenum TranslateAddressMode(ESamplerAddressMode AddressMode)
{
	switch(AddressMode)
	{
	case AM_Clamp: return GL_CLAMP_TO_EDGE;
	case AM_Mirror: return GL_MIRRORED_REPEAT;
	case AM_Border: return UGL_CLAMP_TO_BORDER;
	default: return GL_REPEAT;
	};
}

static GLenum TranslateCullMode(ERasterizerCullMode CullMode)
{
	switch(CullMode)
	{
	case CM_CW: return FOpenGL::SupportsClipControl() ? GL_BACK : GL_FRONT;
	case CM_CCW: return FOpenGL::SupportsClipControl() ? GL_FRONT : GL_BACK;
	default: return GL_NONE;
	};
}

static GLenum TranslateFillMode(ERasterizerFillMode FillMode)
{
	if ( FOpenGL::SupportsPolygonMode() )
	{
		switch(FillMode)
		{
			case FM_Point: return GL_POINT;
			case FM_Wireframe: return GL_LINE;
			default: break;
		};
	}
	return GL_FILL;
}

static GLenum TranslateCompareFunction(ECompareFunction CompareFunction)
{
	switch(CompareFunction)
	{
	case CF_Less: return GL_LESS;
	case CF_LessEqual: return GL_LEQUAL;
	case CF_Greater: return GL_GREATER;
	case CF_GreaterEqual: return GL_GEQUAL;
	case CF_Equal: return GL_EQUAL;
	case CF_NotEqual: return GL_NOTEQUAL;
	case CF_Never: return GL_NEVER;
	default: return GL_ALWAYS;
	};
}

static GLenum TranslateStencilOp(EStencilOp StencilOp)
{
	switch(StencilOp)
	{
	case SO_Zero: return GL_ZERO;
	case SO_Replace: return GL_REPLACE;
	case SO_SaturatedIncrement: return GL_INCR;
	case SO_SaturatedDecrement: return GL_DECR;
	case SO_Invert: return GL_INVERT;
	case SO_Increment: return GL_INCR_WRAP;
	case SO_Decrement: return GL_DECR_WRAP;
	default: return GL_KEEP;
	};
}

static GLenum TranslateBlendOp(EBlendOperation BlendOp)
{
	switch(BlendOp)
	{
	case BO_Subtract: return GL_FUNC_SUBTRACT;
	case BO_Min: return GL_MIN;
	case BO_Max: return GL_MAX;
	case BO_ReverseSubtract: return GL_FUNC_REVERSE_SUBTRACT;
	default: return GL_FUNC_ADD;
	};
}

static GLenum TranslateBlendFactor(EBlendFactor BlendFactor)
{
	switch(BlendFactor)
	{
	case BF_One: return GL_ONE;
	case BF_SourceColor: return GL_SRC_COLOR;
	case BF_InverseSourceColor: return GL_ONE_MINUS_SRC_COLOR;
	case BF_SourceAlpha: return GL_SRC_ALPHA;
	case BF_InverseSourceAlpha: return GL_ONE_MINUS_SRC_ALPHA;
	case BF_DestAlpha: return GL_DST_ALPHA;
	case BF_InverseDestAlpha: return GL_ONE_MINUS_DST_ALPHA;
	case BF_DestColor: return GL_DST_COLOR;
	case BF_InverseDestColor: return GL_ONE_MINUS_DST_COLOR;
	case BF_ConstantBlendFactor: return GL_CONSTANT_COLOR;
	case BF_InverseConstantBlendFactor: return GL_ONE_MINUS_CONSTANT_COLOR;
	default: return GL_ZERO;
	};
}

FOpenGLSamplerState::~FOpenGLSamplerState()
{
	VERIFY_GL_SCOPE();
	FOpenGL::DeleteSamplers(1,&Resource);
}

FSamplerStateRHIRef FOpenGLDynamicRHI::RHICreateSamplerState(const FSamplerStateInitializerRHI& Initializer)
{
	// Try to find an existing cached state
	FOpenGLSamplerState** Found = GSamplerStateCache.Find(Initializer);
	if (Found)
	{
		return *Found;
	}

	// Create a new one
	CalculateSizeOfSamplerStateInitializer();

	VERIFY_GL_SCOPE();

	FOpenGLSamplerState* SamplerState = new FOpenGLSamplerState;

	SamplerState->Data.WrapS = TranslateAddressMode( Initializer.AddressU );
	SamplerState->Data.WrapT = TranslateAddressMode( Initializer.AddressV );
	SamplerState->Data.WrapR = TranslateAddressMode( Initializer.AddressW );
	SamplerState->Data.LODBias = Initializer.MipBias;

	SamplerState->Data.MaxAnisotropy = 1;
	const bool bComparisonEnabled = (Initializer.SamplerComparisonFunction != SCF_Never);

	switch(Initializer.Filter)
	{
	case SF_AnisotropicPoint:
		// This is set up like this in D3D11, so following suit.
		// Otherwise we're getting QA reports about weird artifacting, because QA scenes are set up in
		// D3D11 and AnisotropicPoint when Linear would be proper goes unnoticed there.

		// Once someone decides to fix things in D3D11, I assume they'll look here to fix things up too. The code below is waiting.

		// MagFilter	= GL_NEAREST;
		// MinFilter	= bComparisonEnabled ? GL_NEAREST : GL_NEAREST_MIPMAP_NEAREST;
		// break;

		// PASS-THROUGH to AnisotropicLinear!

	case SF_AnisotropicLinear:
		SamplerState->Data.MagFilter	= GL_LINEAR;
		SamplerState->Data.MinFilter	= bComparisonEnabled ? GL_LINEAR : GL_LINEAR_MIPMAP_LINEAR;
		SamplerState->Data.MaxAnisotropy = FMath::Min<uint32>(ComputeAnisotropyRT(Initializer.MaxAnisotropy), GMaxOpenGLTextureFilterAnisotropic);
		break;
	case SF_Trilinear:
		SamplerState->Data.MagFilter	= GL_LINEAR;
		SamplerState->Data.MinFilter	= bComparisonEnabled ? GL_LINEAR : GL_LINEAR_MIPMAP_LINEAR;
		break;
	case SF_Bilinear:
		SamplerState->Data.MagFilter	= GL_LINEAR;
		SamplerState->Data.MinFilter	= GL_LINEAR_MIPMAP_NEAREST;
		break;
	default:
	case SF_Point:
		SamplerState->Data.MagFilter	= GL_NEAREST;
		SamplerState->Data.MinFilter	= GL_NEAREST_MIPMAP_NEAREST;
		break;
	}

	if( bComparisonEnabled )
	{
		check(Initializer.SamplerComparisonFunction == SCF_Less);
		SamplerState->Data.CompareMode = GL_COMPARE_REF_TO_TEXTURE;
		SamplerState->Data.CompareFunc = GL_LESS;
	}
	else
	{
		SamplerState->Data.CompareMode = GL_NONE;
	}

	if( FOpenGL::SupportsSamplerObjects() )
	{
		FOpenGL::GenSamplers( 1, &SamplerState->Resource );

		FOpenGL::SetSamplerParameter( SamplerState->Resource, GL_TEXTURE_WRAP_S, SamplerState->Data.WrapS );
		FOpenGL::SetSamplerParameter( SamplerState->Resource, GL_TEXTURE_WRAP_T, SamplerState->Data.WrapT );
		if( FOpenGL::SupportsTexture3D() )
		{
			FOpenGL::SetSamplerParameter( SamplerState->Resource, GL_TEXTURE_WRAP_R, SamplerState->Data.WrapR );
		}
		if( FOpenGL::SupportsTextureLODBias() )
		{
			FOpenGL::SetSamplerParameter( SamplerState->Resource, GL_TEXTURE_LOD_BIAS, SamplerState->Data.LODBias );
		}

		FOpenGL::SetSamplerParameter( SamplerState->Resource, GL_TEXTURE_MIN_FILTER, SamplerState->Data.MinFilter );
		FOpenGL::SetSamplerParameter( SamplerState->Resource, GL_TEXTURE_MAG_FILTER, SamplerState->Data.MagFilter );
		if( FOpenGL::SupportsTextureFilterAnisotropic() )
		{
			FOpenGL::SetSamplerParameter( SamplerState->Resource, GL_TEXTURE_MAX_ANISOTROPY_EXT, SamplerState->Data.MaxAnisotropy );
		}

		if( FOpenGL::SupportsTextureCompare() )
		{
			FOpenGL::SetSamplerParameter( SamplerState->Resource, GL_TEXTURE_COMPARE_MODE, SamplerState->Data.CompareMode );
			FOpenGL::SetSamplerParameter( SamplerState->Resource, GL_TEXTURE_COMPARE_FUNC, SamplerState->Data.CompareFunc );
		}
	}
	else
	{
		// Resource is used to check for state changes so set to something unique
		// 0 reserved for default
		static GLuint SamplerCount = 1;
		SamplerState->Resource = SamplerCount++;
	}

	// Manually add reference as we control the creation/destructions
	SamplerState->AddRef();
	GSamplerStateCache.Add(Initializer, SamplerState);

	FShaderCache::LogSamplerState(FShaderCache::GetDefaultCacheState(), Initializer, SamplerState);
	return SamplerState;
}

FRasterizerStateRHIRef FOpenGLDynamicRHI::RHICreateRasterizerState(const FRasterizerStateInitializerRHI& Initializer)
{
	FOpenGLRasterizerState* RasterizerState = new FOpenGLRasterizerState;
	RasterizerState->Data.CullMode = TranslateCullMode(Initializer.CullMode);
	RasterizerState->Data.FillMode = TranslateFillMode(Initializer.FillMode);
	RasterizerState->Data.DepthBias = Initializer.DepthBias;
	RasterizerState->Data.SlopeScaleDepthBias = Initializer.SlopeScaleDepthBias;
	
	FShaderCache::LogRasterizerState(FShaderCache::GetDefaultCacheState(), Initializer, RasterizerState);
	return RasterizerState;
}

FDepthStencilStateRHIRef FOpenGLDynamicRHI::RHICreateDepthStencilState(const FDepthStencilStateInitializerRHI& Initializer)
{
	FOpenGLDepthStencilState* DepthStencilState = new FOpenGLDepthStencilState;
	DepthStencilState->Data.bZEnable = Initializer.DepthTest != CF_Always || Initializer.bEnableDepthWrite;
	DepthStencilState->Data.bZWriteEnable = Initializer.bEnableDepthWrite;
	DepthStencilState->Data.ZFunc = TranslateCompareFunction(Initializer.DepthTest);
	DepthStencilState->Data.bStencilEnable = Initializer.bEnableFrontFaceStencil || Initializer.bEnableBackFaceStencil;
	DepthStencilState->Data.bTwoSidedStencilMode = Initializer.bEnableBackFaceStencil;
	DepthStencilState->Data.StencilFunc = TranslateCompareFunction(Initializer.FrontFaceStencilTest);
	DepthStencilState->Data.StencilFail = TranslateStencilOp(Initializer.FrontFaceStencilFailStencilOp);
	DepthStencilState->Data.StencilZFail = TranslateStencilOp(Initializer.FrontFaceDepthFailStencilOp);
	DepthStencilState->Data.StencilPass = TranslateStencilOp(Initializer.FrontFacePassStencilOp);
	DepthStencilState->Data.CCWStencilFunc = TranslateCompareFunction(Initializer.BackFaceStencilTest);
	DepthStencilState->Data.CCWStencilFail = TranslateStencilOp(Initializer.BackFaceStencilFailStencilOp);
	DepthStencilState->Data.CCWStencilZFail = TranslateStencilOp(Initializer.BackFaceDepthFailStencilOp);
	DepthStencilState->Data.CCWStencilPass = TranslateStencilOp(Initializer.BackFacePassStencilOp);
	DepthStencilState->Data.StencilReadMask = Initializer.StencilReadMask;
	DepthStencilState->Data.StencilWriteMask = Initializer.StencilWriteMask;

	FShaderCache::LogDepthStencilState(FShaderCache::GetDefaultCacheState(), Initializer, DepthStencilState);
	return DepthStencilState;
}

FBlendStateRHIRef FOpenGLDynamicRHI::RHICreateBlendState(const FBlendStateInitializerRHI& Initializer)
{
	FOpenGLBlendState* BlendState = new FOpenGLBlendState;
	for(uint32 RenderTargetIndex = 0;RenderTargetIndex < MaxSimultaneousRenderTargets;++RenderTargetIndex)
	{
		const FBlendStateInitializerRHI::FRenderTarget& RenderTargetInitializer =
			Initializer.bUseIndependentRenderTargetBlendStates
			? Initializer.RenderTargets[RenderTargetIndex]
			: Initializer.RenderTargets[0];
		FOpenGLBlendStateData::FRenderTarget& RenderTarget = BlendState->Data.RenderTargets[RenderTargetIndex];
		RenderTarget.bAlphaBlendEnable = 
			RenderTargetInitializer.ColorBlendOp != BO_Add || RenderTargetInitializer.ColorDestBlend != BF_Zero || RenderTargetInitializer.ColorSrcBlend != BF_One ||
			RenderTargetInitializer.AlphaBlendOp != BO_Add || RenderTargetInitializer.AlphaDestBlend != BF_Zero || RenderTargetInitializer.AlphaSrcBlend != BF_One;
		RenderTarget.ColorBlendOperation = TranslateBlendOp(RenderTargetInitializer.ColorBlendOp);
		RenderTarget.ColorSourceBlendFactor = TranslateBlendFactor(RenderTargetInitializer.ColorSrcBlend);
		RenderTarget.ColorDestBlendFactor = TranslateBlendFactor(RenderTargetInitializer.ColorDestBlend);
		RenderTarget.bSeparateAlphaBlendEnable =
			RenderTargetInitializer.AlphaDestBlend != RenderTargetInitializer.ColorDestBlend ||
			RenderTargetInitializer.AlphaSrcBlend != RenderTargetInitializer.ColorSrcBlend;
		RenderTarget.AlphaBlendOperation = TranslateBlendOp(RenderTargetInitializer.AlphaBlendOp);
		RenderTarget.AlphaSourceBlendFactor = TranslateBlendFactor(RenderTargetInitializer.AlphaSrcBlend);
		RenderTarget.AlphaDestBlendFactor = TranslateBlendFactor(RenderTargetInitializer.AlphaDestBlend);
		RenderTarget.ColorWriteMaskR = (RenderTargetInitializer.ColorWriteMask & CW_RED) != 0;
		RenderTarget.ColorWriteMaskG = (RenderTargetInitializer.ColorWriteMask & CW_GREEN) != 0;
		RenderTarget.ColorWriteMaskB = (RenderTargetInitializer.ColorWriteMask & CW_BLUE) != 0;
		RenderTarget.ColorWriteMaskA = (RenderTargetInitializer.ColorWriteMask & CW_ALPHA) != 0;
	}
	
	FShaderCache::LogBlendState(FShaderCache::GetDefaultCacheState(), Initializer, BlendState);
	return BlendState;
}

//!AB: moved from the header, since it was causing linker error when the header is included externally
void FOpenGLRHIState::InitializeResources(int32 NumCombinedTextures, int32 NumComputeUAVUnits)
{
	check(!ShaderParameters);
	FOpenGLCommonState::InitializeResources(NumCombinedTextures, NumComputeUAVUnits);
	ShaderParameters = new FOpenGLShaderParameterCache[CrossCompiler::NUM_SHADER_STAGES];
	ShaderParameters[CrossCompiler::SHADER_STAGE_VERTEX].InitializeResources(FOpenGL::GetMaxVertexUniformComponents() * 4 * sizeof(float));
	ShaderParameters[CrossCompiler::SHADER_STAGE_PIXEL].InitializeResources(FOpenGL::GetMaxPixelUniformComponents() * 4 * sizeof(float));
	ShaderParameters[CrossCompiler::SHADER_STAGE_GEOMETRY].InitializeResources(FOpenGL::GetMaxGeometryUniformComponents() * 4 * sizeof(float));
		
	if ( FOpenGL::SupportsTessellation() )
	{
		ShaderParameters[CrossCompiler::SHADER_STAGE_HULL].InitializeResources(FOpenGL::GetMaxHullUniformComponents() * 4 * sizeof(float));
		ShaderParameters[CrossCompiler::SHADER_STAGE_DOMAIN].InitializeResources(FOpenGL::GetMaxDomainUniformComponents() * 4 * sizeof(float));
	}

	if ( FOpenGL::SupportsComputeShaders() )
	{
		ShaderParameters[CrossCompiler::SHADER_STAGE_COMPUTE].InitializeResources(FOpenGL::GetMaxComputeUniformComponents() * 4 * sizeof(float));
	}

	for (int32 Frequency = 0; Frequency < SF_NumFrequencies; ++Frequency)
	{
		DirtyUniformBuffers[Frequency] = MAX_uint16;
	}
}
