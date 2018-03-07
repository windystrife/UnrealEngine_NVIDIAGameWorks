// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLCommands.cpp: OpenGL RHI commands implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "RHIDefinitions.h"
#include "RHI.h"
#include "EngineGlobals.h"
#include "RenderResource.h"
#include "ShaderCache.h"
#include "OpenGLDrv.h"
#include "OpenGLDrvPrivate.h"
#include "RenderUtils.h"

#define DECLARE_ISBOUNDSHADER(ShaderType) inline void ValidateBoundShader(TRefCountPtr<FOpenGLBoundShaderState> InBoundShaderState, F##ShaderType##RHIParamRef ShaderType##RHI) \
{ \
	FOpenGL##ShaderType* ShaderType = FOpenGLDynamicRHI::ResourceCast(ShaderType##RHI); \
	ensureMsgf(InBoundShaderState && ShaderType == InBoundShaderState->ShaderType, TEXT("Parameters are being set for a %s which is not currently bound"), TEXT(#ShaderType)); \
}

DECLARE_ISBOUNDSHADER(VertexShader)
DECLARE_ISBOUNDSHADER(PixelShader)
DECLARE_ISBOUNDSHADER(GeometryShader)
DECLARE_ISBOUNDSHADER(HullShader)
DECLARE_ISBOUNDSHADER(DomainShader)

#if DO_CHECK
	#define VALIDATE_BOUND_SHADER(s) ValidateBoundShader(PendingState.BoundShaderState, s)
#else
	#define VALIDATE_BOUND_SHADER(s)
#endif

namespace OpenGLConsoleVariables
{
#if PLATFORM_WINDOWS || PLATFORM_LINUX
	int32 bUseMapBuffer = 0;
#else
	int32 bUseMapBuffer = 1;
#endif
	static FAutoConsoleVariableRef CVarUseMapBuffer(
		TEXT("OpenGL.UseMapBuffer"),
		bUseMapBuffer,
		TEXT("If true, use glMapBuffer otherwise use glBufferSubdata.")
		);

	static FAutoConsoleVariable CVarUseEmulatedUBs(
		TEXT("OpenGL.UseEmulatedUBs"),
		0,
		TEXT("If true, enable using emulated uniform buffers on ES2 mode."),
		ECVF_ReadOnly
		);

	int32 bSkipCompute = 0;
	static FAutoConsoleVariableRef CVarSkipCompute(
		TEXT("OpenGL.SkipCompute"),
		bSkipCompute,
		TEXT("If true, don't issue dispatch work.")
		);

	int32 bUseVAB = 1;
	static FAutoConsoleVariableRef CVarUseVAB(
		TEXT("OpenGL.UseVAB"),
		bUseVAB,
		TEXT("If true, use GL_VERTEX_ATTRIB_BINDING instead of traditional vertex array setup."),
		ECVF_ReadOnly
		);

#if PLATFORM_WINDOWS || PLATFORM_LINUX
	int32 MaxSubDataSize = 256*1024;
#else
	int32 MaxSubDataSize = 0;
#endif
	static FAutoConsoleVariableRef CVarMaxSubDataSize(
		TEXT("OpenGL.MaxSubDataSize"),
		MaxSubDataSize,
		TEXT("Maximum amount of data to send to glBufferSubData in one call"),
		ECVF_ReadOnly
		);

	int32 bBindlessTexture = 0;
	static FAutoConsoleVariableRef CVarBindlessTexture(
		TEXT("OpenGL.BindlessTexture"),
		bBindlessTexture,
		TEXT("If true, use GL_ARB_bindless_texture over traditional glBindTexture/glBindSampler."),
		ECVF_ReadOnly
		);
	
	int32 bRebindTextureBuffers = 0;
	static FAutoConsoleVariableRef CVarRebindTextureBuffers(
		TEXT("OpenGL.RebindTextureBuffers"),
		bRebindTextureBuffers,
		TEXT("If true, rebind GL_TEXTURE_BUFFER's to their GL_TEXTURE name whenever the buffer is modified.")
		);

	int32 bUseBufferDiscard = 1;
	static FAutoConsoleVariableRef CVarUseBufferDiscard(
		TEXT("OpenGL.UseBufferDiscard"),
		bUseBufferDiscard,
		TEXT("If true, use dynamic buffer orphaning hint.")
		);
	
	static TAutoConsoleVariable<int32> CVarUseSeparateShaderObjects(
		TEXT("OpenGL.UseSeparateShaderObjects"),
		0,
		TEXT("If set to 1, use OpenGL's separate shader objects to eliminate expensive program linking"),
		ECVF_ReadOnly|ECVF_RenderThreadSafe);
};

#if PLATFORM_64BITS
#define INDEX_TO_VOID(Index) (void*)((uint64)(Index))
#else
#define INDEX_TO_VOID(Index) (void*)((uint32)(Index))
#endif

enum EClearType
{
	CT_None				= 0x0,
	CT_Depth			= 0x1,
	CT_Stencil			= 0x2,
	CT_Color			= 0x4,
	CT_DepthStencil		= CT_Depth | CT_Stencil,
};

struct FPendingSamplerDataValue
{
	GLenum	Enum;
	GLint	Value;
};

struct FVertexBufferPair
{
	FOpenGLVertexBuffer*				Source;
	TRefCountPtr<FOpenGLVertexBuffer>	Dest;
};
static TArray<FVertexBufferPair> ZeroStrideExpandedBuffersList;


static int FindVertexBuffer(FOpenGLVertexBuffer* Source)
{
	for (int32 Index = 0; Index < ZeroStrideExpandedBuffersList.Num(); ++Index)
	{
		if (ZeroStrideExpandedBuffersList[Index].Source == Source)
		{
			return Index;
		}
	}
	return -1;
}

static FOpenGLVertexBuffer* FindExpandedZeroStrideBuffer(FOpenGLVertexBuffer* ZeroStrideVertexBuffer, uint32 Stride, uint32 NumVertices, const FOpenGLVertexElement& VertexElement)
{
	uint32 Size = NumVertices * Stride;
	int32 FoundExpandedVBIndex = FindVertexBuffer(ZeroStrideVertexBuffer);
	if (FoundExpandedVBIndex != -1)
	{
		// Check if the current size is big enough
		FOpenGLVertexBuffer* ExpandedVB = ZeroStrideExpandedBuffersList[FoundExpandedVBIndex].Dest;
		if (Size <= ExpandedVB->GetSize())
		{
			return ExpandedVB;
		}
	}
	else
	{
		FVertexBufferPair NewPair;
		NewPair.Source = ZeroStrideVertexBuffer;
		NewPair.Dest = NULL;
		FoundExpandedVBIndex = ZeroStrideExpandedBuffersList.Num();
		ZeroStrideExpandedBuffersList.Add(NewPair);
	}

	int32 VertexTypeSize = 0;
	switch( VertexElement.Type )
	{
	case GL_FLOAT:
	case GL_UNSIGNED_INT:
	case GL_INT:
		VertexTypeSize = 4;
		break;
	case GL_SHORT:
	case GL_UNSIGNED_SHORT:
	case GL_HALF_FLOAT:
		VertexTypeSize = 2;
		break;
	case GL_BYTE:
	case GL_UNSIGNED_BYTE:
		VertexTypeSize = 1;
		break;
	case GL_DOUBLE:
		VertexTypeSize = 8;
		break;
	default:
		check(0);
		break;
	}

	const int32 VertexElementSize = ( VertexElement.Size == GL_BGRA ) ? 4 : VertexElement.Size;
	const int32 SizeToFill = VertexElementSize * VertexTypeSize;
	void* RESTRICT SourceData = ZeroStrideVertexBuffer->GetZeroStrideBuffer();
	check(SourceData);
	TRefCountPtr<FOpenGLVertexBuffer> ExpandedVB = new FOpenGLVertexBuffer(0, Size, BUF_Static, NULL);
	uint8* RESTRICT Data = ExpandedVB->Lock(0, Size, false, true);

	switch (SizeToFill)
	{
	case 4:
		{
			uint32 Source = *(uint32*)SourceData;
			uint32* RESTRICT Dest = (uint32*)Data;
			for (uint32 Index = 0; Index < Size / sizeof(uint32); ++Index)
			{
				*Dest++ = Source;
			}
		}
		break;
	case 8:
		{
			uint64 Source = *(uint64*)SourceData;
			uint64* RESTRICT Dest = (uint64*)Data;
			for (uint32 Index = 0; Index < Size / sizeof(uint64); ++Index)
			{
				*Dest++ = Source;
			}
		}
		break;
	case 16:
		{
			uint64 SourceA = *(uint64*)SourceData;
			uint64 SourceB = *((uint64*)SourceData + 1);
			uint64* RESTRICT Dest = (uint64*)Data;
			for (uint32 Index = 0; Index < Size / (2 * sizeof(uint64)); ++Index)
			{
				*Dest++ = SourceA;
				*Dest++ = SourceB;
			}
		}
		break;
	default:
		check(0);
	}

	ExpandedVB->Unlock();

	ZeroStrideExpandedBuffersList[FoundExpandedVBIndex].Dest = ExpandedVB;

	return ExpandedVB;
}

static FORCEINLINE GLint ModifyFilterByMips(GLint Filter, bool bHasMips)
{
	if (!bHasMips)
	{
		switch (Filter)
		{
			case GL_LINEAR_MIPMAP_NEAREST:
			case GL_LINEAR_MIPMAP_LINEAR:
				return GL_LINEAR;

			case GL_NEAREST_MIPMAP_NEAREST:
			case GL_NEAREST_MIPMAP_LINEAR:
				return GL_NEAREST;

			default:
				break;
		}
	}

	return Filter;
}

// Vertex state.
void FOpenGLDynamicRHI::RHISetStreamSource(uint32 StreamIndex,FVertexBufferRHIParamRef VertexBufferRHI,uint32 Stride,uint32 Offset)
{
	ensure(PendingState.BoundShaderState->StreamStrides[StreamIndex] == Stride);
	FOpenGLVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	PendingState.Streams[StreamIndex].VertexBuffer = VertexBuffer;
	PendingState.Streams[StreamIndex].Stride = PendingState.BoundShaderState ? PendingState.BoundShaderState->StreamStrides[StreamIndex] : 0;
	PendingState.Streams[StreamIndex].Offset = Offset;
}

void FOpenGLDynamicRHI::RHISetStreamSource(uint32 StreamIndex, FVertexBufferRHIParamRef VertexBufferRHI, uint32 Offset)
{
	FOpenGLVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);
	PendingState.Streams[StreamIndex].VertexBuffer = VertexBuffer;
	PendingState.Streams[StreamIndex].Stride = PendingState.BoundShaderState ? PendingState.BoundShaderState->StreamStrides[StreamIndex] : 0;
	PendingState.Streams[StreamIndex].Offset = Offset;
}

void FOpenGLDynamicRHI::RHISetStreamOutTargets(uint32 NumTargets, const FVertexBufferRHIParamRef* VertexBuffers, const uint32* Offsets)
{
	check(0);
}

// Rasterizer state.
void FOpenGLDynamicRHI::RHISetRasterizerState(FRasterizerStateRHIParamRef NewStateRHI)
{
	VERIFY_GL_SCOPE();
	FOpenGLRasterizerState* NewState = ResourceCast(NewStateRHI);
	PendingState.RasterizerState = NewState->Data;
	
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FShaderCache::SetRasterizerState(FShaderCache::GetDefaultCacheState(), NewStateRHI);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FOpenGLDynamicRHI::UpdateRasterizerStateInOpenGLContext( FOpenGLContextState& ContextState )
{
	if (FOpenGL::SupportsPolygonMode() && ContextState.RasterizerState.FillMode != PendingState.RasterizerState.FillMode)
	{
		FOpenGL::PolygonMode(GL_FRONT_AND_BACK, PendingState.RasterizerState.FillMode);
		ContextState.RasterizerState.FillMode = PendingState.RasterizerState.FillMode;
	}

	if (ContextState.RasterizerState.CullMode != PendingState.RasterizerState.CullMode)
	{
		if (PendingState.RasterizerState.CullMode != GL_NONE)
		{
			// Only call glEnable if needed
			if (ContextState.RasterizerState.CullMode == GL_NONE)
			{
				glEnable(GL_CULL_FACE);
			}
			glCullFace(PendingState.RasterizerState.CullMode);
		}
		else
		{
			glDisable(GL_CULL_FACE);
		}
		ContextState.RasterizerState.CullMode = PendingState.RasterizerState.CullMode;
	}

	// Convert our platform independent depth bias into an OpenGL depth bias.
	const float BiasScale = float((1<<24)-1);	// Warning: this assumes depth bits == 24, and won't be correct with 32.
	float DepthBias = PendingState.RasterizerState.DepthBias * BiasScale;
	if (ContextState.RasterizerState.DepthBias != PendingState.RasterizerState.DepthBias
		|| ContextState.RasterizerState.SlopeScaleDepthBias != PendingState.RasterizerState.SlopeScaleDepthBias)
	{
		if ((DepthBias == 0.0f) && (PendingState.RasterizerState.SlopeScaleDepthBias == 0.0f))
		{
			// If we're here, both previous 2 'if' conditions are true, and this implies that cached state was not all zeroes, so we need to glDisable.
			glDisable(GL_POLYGON_OFFSET_FILL);
			if ( FOpenGL::SupportsPolygonMode() )
			{
				glDisable(GL_POLYGON_OFFSET_LINE);
				glDisable(GL_POLYGON_OFFSET_POINT);
			}
		}
		else
		{
			if (ContextState.RasterizerState.DepthBias == 0.0f && ContextState.RasterizerState.SlopeScaleDepthBias == 0.0f)
			{
				glEnable(GL_POLYGON_OFFSET_FILL);
				if ( FOpenGL::SupportsPolygonMode() )
				{
					glEnable(GL_POLYGON_OFFSET_LINE);
					glEnable(GL_POLYGON_OFFSET_POINT);
				}
			}
			glPolygonOffset(PendingState.RasterizerState.SlopeScaleDepthBias, DepthBias);
		}

		ContextState.RasterizerState.DepthBias = PendingState.RasterizerState.DepthBias;
		ContextState.RasterizerState.SlopeScaleDepthBias = PendingState.RasterizerState.SlopeScaleDepthBias;
	}
}

void FOpenGLDynamicRHI::UpdateViewportInOpenGLContext( FOpenGLContextState& ContextState )
{
	if (ContextState.Viewport != PendingState.Viewport)
	{
		//@todo the viewport defined by glViewport does not clip, unlike the viewport in d3d
		// Set the scissor rect to the viewport unless it is explicitly set smaller to emulate d3d.
		glViewport(
			PendingState.Viewport.Min.X,
			PendingState.Viewport.Min.Y,
			PendingState.Viewport.Max.X - PendingState.Viewport.Min.X,
			PendingState.Viewport.Max.Y - PendingState.Viewport.Min.Y);

		ContextState.Viewport = PendingState.Viewport;
	}

	if (ContextState.DepthMinZ != PendingState.DepthMinZ || ContextState.DepthMaxZ != PendingState.DepthMaxZ)
	{
		FOpenGL::DepthRange(PendingState.DepthMinZ, PendingState.DepthMaxZ);
		ContextState.DepthMinZ = PendingState.DepthMinZ;
		ContextState.DepthMaxZ = PendingState.DepthMaxZ;
	}
}

void FOpenGLDynamicRHI::RHISetViewport(uint32 MinX,uint32 MinY,float MinZ,uint32 MaxX,uint32 MaxY,float MaxZ)
{
	PendingState.Viewport.Min.X = MinX;
	PendingState.Viewport.Min.Y = MinY;
	PendingState.Viewport.Max.X = MaxX;
	PendingState.Viewport.Max.Y = MaxY;
	PendingState.DepthMinZ = MinZ;
	PendingState.DepthMaxZ = MaxZ;

	RHISetScissorRect(false, 0, 0, 0, 0);

	FShaderCache::SetViewport(FShaderCache::GetDefaultCacheState(), MinX, MinY, MinZ, MaxX, MaxY, MaxZ);
}

void FOpenGLDynamicRHI::RHISetScissorRect(bool bEnable,uint32 MinX,uint32 MinY,uint32 MaxX,uint32 MaxY)
{
	PendingState.bScissorEnabled = bEnable;
	PendingState.Scissor.Min.X = MinX;
	PendingState.Scissor.Min.Y = MinY;
	PendingState.Scissor.Max.X = MaxX;
	PendingState.Scissor.Max.Y = MaxY;
}

inline void FOpenGLDynamicRHI::UpdateScissorRectInOpenGLContext( FOpenGLContextState& ContextState )
{
	VERIFY_GL_SCOPE();
	if (ContextState.bScissorEnabled != PendingState.bScissorEnabled)
	{
		if (PendingState.bScissorEnabled)
		{
			glEnable(GL_SCISSOR_TEST);
		}
		else
		{
			glDisable(GL_SCISSOR_TEST);
		}
		ContextState.bScissorEnabled = PendingState.bScissorEnabled;
	}

	if( PendingState.bScissorEnabled &&
		ContextState.Scissor != PendingState.Scissor )
	{
		check(PendingState.Scissor.Min.X <= PendingState.Scissor.Max.X);
		check(PendingState.Scissor.Min.Y <= PendingState.Scissor.Max.Y);
		glScissor(PendingState.Scissor.Min.X, PendingState.Scissor.Min.Y, PendingState.Scissor.Max.X - PendingState.Scissor.Min.X, PendingState.Scissor.Max.Y - PendingState.Scissor.Min.Y);
		ContextState.Scissor = PendingState.Scissor;
	}
}

/**
* Set bound shader state. This will set the vertex decl/shader, and pixel shader
* @param BoundShaderState - state resource
*/
void FOpenGLDynamicRHI::RHISetBoundShaderState( FBoundShaderStateRHIParamRef BoundShaderStateRHI)
{
	VERIFY_GL_SCOPE();
	FOpenGLBoundShaderState* BoundShaderState = ResourceCast(BoundShaderStateRHI);
	PendingState.BoundShaderState = BoundShaderState;

	// Prevent transient bound shader states from being recreated for each use by keeping a history of the most recently used bound shader states.
	// The history keeps them alive, and the bound shader state cache allows them to be reused if needed.
	BoundShaderStateHistory.Add(BoundShaderState);
	
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FShaderCache::SetBoundShaderState(FShaderCache::GetDefaultCacheState(), BoundShaderStateRHI);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FOpenGLDynamicRHI::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 UAVIndex,FUnorderedAccessViewRHIParamRef UnorderedAccessViewRHI)
{
	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);

	VERIFY_GL_SCOPE();
	if(UnorderedAccessViewRHI)
	{
		FOpenGLUnorderedAccessView* UnorderedAccessView = ResourceCast(UnorderedAccessViewRHI);
		InternalSetShaderUAV(FOpenGL::GetFirstComputeUAVUnit() + UAVIndex, UnorderedAccessView->Format , UnorderedAccessView->Resource);
	}
	else
	{
		InternalSetShaderUAV(FOpenGL::GetFirstComputeUAVUnit() + UAVIndex, GL_R32F, 0);
	}
}

void FOpenGLDynamicRHI::RHISetUAVParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 UAVIndex,FUnorderedAccessViewRHIParamRef UAVRHI, uint32 InitialCount )
{
	// TODO: Implement for OpenGL
	check(0);
}

void FOpenGLDynamicRHI::InternalSetShaderTexture(FOpenGLTextureBase* Texture, FOpenGLShaderResourceView* SRV, GLint TextureIndex, GLenum Target, GLuint Resource, int NumMips, int LimitMip)
{
	auto& PendingTextureState = PendingState.Textures[TextureIndex];
	PendingTextureState.Texture = Texture;
	PendingTextureState.SRV = SRV;
	PendingTextureState.Target = Target;
	PendingTextureState.Resource = Resource;
	PendingTextureState.LimitMip = LimitMip;
	PendingTextureState.bHasMips = (NumMips == 0 || NumMips > 1);
	PendingTextureState.NumMips = NumMips;
}

void FOpenGLDynamicRHI::InternalSetSamplerStates(GLint TextureIndex, FOpenGLSamplerState* SamplerState)
{
	PendingState.SamplerStates[TextureIndex] = SamplerState;
}

void FOpenGLDynamicRHI::CachedSetupTextureStage(FOpenGLContextState& ContextState, GLint TextureIndex, GLenum Target, GLuint Resource, GLint LimitMip, GLint NumMips)
{
	VERIFY_GL_SCOPE();
	auto& TextureState = ContextState.Textures[TextureIndex];
	const bool bSameTarget = (TextureState.Target == Target);
	const bool bSameResource = (TextureState.Resource == Resource);

	if( bSameTarget && bSameResource )
	{
		// Nothing changed, no need to update
		return;
	}

	// Something will have to be changed. Switch to the stage in question.
	if( ContextState.ActiveTexture != TextureIndex )
	{
		glActiveTexture( GL_TEXTURE0 + TextureIndex );
		ContextState.ActiveTexture = TextureIndex;
	}

	if (bSameTarget)
	{
		glBindTexture(Target, Resource);
	}
	else
	{
		if(TextureState.Target != GL_NONE)
		{
			// Unbind different texture target on the same stage, to avoid OpenGL keeping its data, and potential driver problems.
			glBindTexture(TextureState.Target, 0);
		}

		if(Target != GL_NONE)
		{
			glBindTexture(Target, Resource);
		}
	}
	
	// Use the texture SRV's LimitMip value to specify the mip available for sampling
	// This requires SupportsTextureBaseLevel & is a fallback for SupportsTextureView
	// which should be preferred.
	if(Target != GL_NONE && Target != GL_TEXTURE_BUFFER && !FOpenGL::SupportsTextureView())
	{
		TPair<GLenum, GLenum>* MipLimits = TextureMipLimits.Find(Resource);
		
		GLint BaseMip = LimitMip == -1 ? 0 : LimitMip;
		GLint MaxMip = LimitMip == -1 ? NumMips - 1 : LimitMip;
		
		const bool bSameLimitMip = MipLimits && MipLimits->Key == BaseMip;
		const bool bSameNumMips = MipLimits && MipLimits->Value == MaxMip;
		
		if(FOpenGL::SupportsTextureBaseLevel() && !bSameLimitMip)
		{
			FOpenGL::TexParameter(Target, GL_TEXTURE_BASE_LEVEL, BaseMip);
		}
		TextureState.LimitMip = LimitMip;
		
#if PLATFORM_ANDROID
		if (FOpenGL::SupportsTextureMaxLevel())
		{
			// Always set if last target was external texture, or new target is not external and number of mips doesn't match
			if ((!bSameTarget && TextureState.Target == GL_TEXTURE_EXTERNAL_OES) || ((Target != GL_TEXTURE_EXTERNAL_OES) && !bSameNumMips))
			{
				FOpenGL::TexParameter(Target, GL_TEXTURE_MAX_LEVEL, MaxMip);
			}
		}
#else
		if (FOpenGL::SupportsTextureMaxLevel() && !bSameNumMips)
		{
			FOpenGL::TexParameter(Target, GL_TEXTURE_MAX_LEVEL, MaxMip);
		}
#endif
		TextureState.NumMips = NumMips;
		
		TextureMipLimits.Add(Resource, TPair<GLenum, GLenum>(BaseMip, MaxMip));
	}
	else
	{
		TextureState.LimitMip = 0;
		TextureState.NumMips = 0;
	}

	TextureState.Target = Target;
	TextureState.Resource = Resource;
}

inline void FOpenGLDynamicRHI::ApplyTextureStage(FOpenGLContextState& ContextState, GLint TextureIndex, const FTextureStage& TextureStage, FOpenGLSamplerState* SamplerState)
{
	GLenum Target = TextureStage.Target;
	VERIFY_GL_SCOPE();
	const bool bHasTexture = (TextureStage.Texture != NULL);
	if (!bHasTexture || TextureStage.Texture->SamplerState != SamplerState)
	{
		// Texture must be bound first
		if( ContextState.ActiveTexture != TextureIndex )
		{
			glActiveTexture(GL_TEXTURE0 + TextureIndex);
			ContextState.ActiveTexture = TextureIndex;
		}

		GLint WrapS = SamplerState->Data.WrapS;
		GLint WrapT = SamplerState->Data.WrapT;
		if (!FOpenGL::SupportsTextureNPOT() && bHasTexture)
		{
			if (!TextureStage.Texture->IsPowerOfTwo())
			{
				bool bChanged = false;
				if (WrapS != GL_CLAMP_TO_EDGE)
				{
					WrapS = GL_CLAMP_TO_EDGE;
					bChanged = true;
				}
				if (WrapT != GL_CLAMP_TO_EDGE)
				{
					WrapT = GL_CLAMP_TO_EDGE;
					bChanged = true;
				}
				if (bChanged)
				{
					ANSICHAR DebugName[128] = "";
					if (FOpenGL::GetLabelObject(GL_TEXTURE, TextureStage.Resource, sizeof(DebugName), DebugName) != 0)
					{
						UE_LOG(LogRHI, Warning, TEXT("Texture %s (Index %d, Resource %d) has a non-clamp mode; switching to clamp to avoid driver problems"), ANSI_TO_TCHAR(DebugName), TextureIndex, TextureStage.Resource);
					}
					else
					{
						UE_LOG(LogRHI, Warning, TEXT("Texture %d (Resource %d) has a non-clamp mode; switching to clamp to avoid driver problems"), TextureIndex, TextureStage.Resource);
					}
				}
			}
		}

		// Sets parameters of currently bound texture
		FOpenGL::TexParameter(Target, GL_TEXTURE_WRAP_S, WrapS);
		FOpenGL::TexParameter(Target, GL_TEXTURE_WRAP_T, WrapT);
		if( FOpenGL::SupportsTexture3D() )
		{
			FOpenGL::TexParameter(Target, GL_TEXTURE_WRAP_R, SamplerState->Data.WrapR);
		}

		if( FOpenGL::SupportsTextureLODBias() )
		{
			FOpenGL::TexParameter(Target, GL_TEXTURE_LOD_BIAS, SamplerState->Data.LODBias);
		}
		// Make sure we don't set mip filtering on if the texture has no mip levels, as that will cause a crash/black render on ES2.
		FOpenGL::TexParameter(Target, GL_TEXTURE_MIN_FILTER, ModifyFilterByMips(SamplerState->Data.MinFilter, TextureStage.bHasMips));
		FOpenGL::TexParameter(Target, GL_TEXTURE_MAG_FILTER, SamplerState->Data.MagFilter);
		if( FOpenGL::SupportsTextureFilterAnisotropic() )
		{
			// GL_EXT_texture_filter_anisotropic requires value to be at least 1
			GLint MaxAnisotropy = FMath::Max(1, SamplerState->Data.MaxAnisotropy);
			FOpenGL::TexParameter(Target, GL_TEXTURE_MAX_ANISOTROPY_EXT, MaxAnisotropy);
		}

		if( FOpenGL::SupportsTextureCompare() )
		{
			FOpenGL::TexParameter(Target, GL_TEXTURE_COMPARE_MODE, SamplerState->Data.CompareMode);
			FOpenGL::TexParameter(Target, GL_TEXTURE_COMPARE_FUNC, SamplerState->Data.CompareFunc);
		}

		if (bHasTexture)
		{
			TextureStage.Texture->SamplerState = SamplerState;
		}
	}
}

template <typename StateType>
void FOpenGLDynamicRHI::SetupTexturesForDraw( FOpenGLContextState& ContextState, const StateType ShaderState, int32 MaxTexturesNeeded )
{
	SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLTextureBindTime);
	// Texture must be bound first
	const bool bNeedsSetupSamplerStage = !FOpenGL::SupportsSamplerObjects();
	
	// Skip texture setup when running bindless texture, it is done with program setup
	if (OpenGLConsoleVariables::bBindlessTexture && FOpenGL::SupportsBindlessTexture())
	{
		return;
	}

	const int32 MaxProgramTexture = ShaderState->MaxTextureStageUsed();

	for( int32 TextureStageIndex = 0; TextureStageIndex <= MaxProgramTexture; ++TextureStageIndex )
	{
		if (!ShaderState->NeedsTextureStage(TextureStageIndex))
		{
			// Current program doesn't make use of this texture stage. No matter what UE4 wants to have on in,
			// it won't be useful for this draw, so telling OpenGL we don't really need it to give the driver
			// more leeway in memory management, and avoid false alarms about same texture being set on
			// texture stage and in framebuffer.
			CachedSetupTextureStage( ContextState, TextureStageIndex, GL_NONE, 0, -1, 1 );
		}
		else
		{
			const FTextureStage& TextureStage = PendingState.Textures[TextureStageIndex];
			
#if UE_BUILD_DEBUG
			// Use the texture SRV's LimitMip value to specify the mip available for sampling
			// This requires SupportsTextureBaseLevel & is a fallback for SupportsTextureView
			// which should be preferred.
			if(!FOpenGL::SupportsTextureView())
			{
				// When trying to limit the mip available for sampling (as part of texture SRV)
				// ensure that the texture is bound to only one sampler, or that all samplers
				// share the same restriction.
				if(TextureStage.LimitMip != -1)
				{
					for( int32 TexIndex = 0; TexIndex <= MaxProgramTexture; ++TexIndex )
					{
						if(TexIndex != TextureStageIndex && ShaderState->NeedsTextureStage(TexIndex))
						{
							const FTextureStage& OtherStage = PendingState.Textures[TexIndex];
							const bool bSameResource = OtherStage.Resource == TextureStage.Resource;
							const bool bSameTarget = OtherStage.Target == TextureStage.Target;
							const GLint TextureStageBaseMip = TextureStage.LimitMip == -1 ? 0 : TextureStage.LimitMip;
							const GLint OtherStageBaseMip = OtherStage.LimitMip == -1 ? 0 : OtherStage.LimitMip;
							const bool bSameLimitMip = TextureStageBaseMip == OtherStageBaseMip;
							const GLint TextureStageMaxMip = TextureStage.LimitMip == -1 ? TextureStage.NumMips - 1 : TextureStage.LimitMip;
							const GLint OtherStageMaxMip = OtherStage.LimitMip == -1 ? OtherStage.NumMips - 1 : OtherStage.LimitMip;
							const bool bSameMaxMip = TextureStageMaxMip == OtherStageMaxMip;
							if( bSameTarget && bSameResource && !bSameLimitMip && !bSameMaxMip )
							{
								UE_LOG(LogRHI, Warning, TEXT("Texture SRV fallback requires that each texture SRV be bound with the same mip-range restrictions. Expect rendering errors."));
							}
						}
					}
				}
			}
#endif
			CachedSetupTextureStage( ContextState, TextureStageIndex, TextureStage.Target, TextureStage.Resource, TextureStage.LimitMip, TextureStage.NumMips );
			
			if (bNeedsSetupSamplerStage && TextureStage.Target != GL_TEXTURE_BUFFER)
			{
				ApplyTextureStage( ContextState, TextureStageIndex, TextureStage, PendingState.SamplerStates[TextureStageIndex] );
			}
		}
	}

	// For now, continue to clear unused stages
	for( int32 TextureStageIndex = MaxProgramTexture + 1; TextureStageIndex < MaxTexturesNeeded; ++TextureStageIndex )
	{
		CachedSetupTextureStage( ContextState, TextureStageIndex, GL_NONE, 0, -1, 1 );
	}
}

void FOpenGLDynamicRHI::SetupTexturesForDraw( FOpenGLContextState& ContextState )
{
	SetupTexturesForDraw(ContextState, PendingState.BoundShaderState, FOpenGL::GetMaxCombinedTextureImageUnits());
}

void FOpenGLDynamicRHI::InternalSetShaderUAV(GLint UAVIndex, GLenum Format, GLuint Resource)
{
	PendingState.UAVs[UAVIndex].Format = Format;
	PendingState.UAVs[UAVIndex].Resource = Resource;
}

void FOpenGLDynamicRHI::SetupUAVsForDraw( FOpenGLContextState& ContextState, const TRefCountPtr<FOpenGLComputeShader> &ComputeShader, int32 MaxUAVsNeeded )
{
	for( int32 UAVStageIndex = 0; UAVStageIndex < MaxUAVsNeeded; ++UAVStageIndex )
	{
		if (!ComputeShader->NeedsUAVStage(UAVStageIndex))
		{
			CachedSetupUAVStage(ContextState, UAVStageIndex, GL_R32F, 0 );
		}
		else
		{
			CachedSetupUAVStage(ContextState, UAVStageIndex, PendingState.UAVs[UAVStageIndex].Format, PendingState.UAVs[UAVStageIndex].Resource );
		}
	}
	
}


void FOpenGLDynamicRHI::CachedSetupUAVStage( FOpenGLContextState& ContextState, GLint UAVIndex, GLenum Format, GLuint Resource)
{
	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);

	if( ContextState.UAVs[UAVIndex].Format == Format && ContextState.Textures[UAVIndex].Resource == Resource)
	{
		// Nothing's changed, no need to update
		return;
	}

	FOpenGL::BindImageTexture(UAVIndex, Resource, 0, GL_FALSE, 0, GL_READ_WRITE, Format);
	
	ContextState.UAVs[UAVIndex].Format = Format;
	ContextState.UAVs[UAVIndex].Resource = Resource;
}

void FOpenGLDynamicRHI::UpdateSRV(FOpenGLShaderResourceView* SRV)
{
	check(SRV);
	// For Depth/Stencil textures whose Stencil component we wish to sample we must blit the stencil component out to an intermediate texture when we 'Store' the texture.
#if PLATFORM_DESKTOP || PLATFORM_ANDROIDESDEFERRED
	if (FOpenGL::GetFeatureLevel() >= ERHIFeatureLevel::SM4 && FOpenGL::SupportsPixelBuffers() && IsValidRef(SRV->Texture2D))
	{
		FOpenGLTexture2D* Texture2D = ResourceCast(SRV->Texture2D.GetReference());
		
		uint32 ArrayIndices = 0;
		uint32 MipmapLevels = 0;
		
		GLuint SourceFBO = GetOpenGLFramebuffer(0, nullptr, &ArrayIndices, &MipmapLevels, (FOpenGLTextureBase*)Texture2D);
		
		glBindFramebuffer(GL_FRAMEBUFFER, SourceFBO);
		
		uint32 SizeX = Texture2D->GetSizeX();
		uint32 SizeY = Texture2D->GetSizeY();
		
		uint32 MipBytes = SizeX * SizeY;
		TRefCountPtr<FOpenGLPixelBuffer> PixelBuffer = new FOpenGLPixelBuffer(0, MipBytes, BUF_Dynamic);
		
		glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
		glBindBuffer( GL_PIXEL_PACK_BUFFER, PixelBuffer->Resource );
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, SizeX, SizeY, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, nullptr );
		glPixelStorei(GL_PACK_ALIGNMENT, 4);
		glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
		
		FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
		
		GLenum Target = SRV->Target;
		
		CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Target, SRV->Resource, -1, 1);
		
		CachedBindPixelUnpackBuffer(ContextState, PixelBuffer->Resource);
		
		glPixelStorei(GL_UNPACK_ROW_LENGTH, SizeX);
		
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexSubImage2D(Target, 0, 0, 0, SizeX, SizeY, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		
		CachedBindPixelUnpackBuffer(ContextState, 0);
		
		glBindFramebuffer(GL_FRAMEBUFFER, ContextState.Framebuffer);
		ContextState.Framebuffer = -1;
	}
#endif
}

void FOpenGLDynamicRHI::RHISetShaderResourceViewParameter(FPixelShaderRHIParamRef PixelShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	VERIFY_GL_SCOPE();
	VALIDATE_BOUND_SHADER(PixelShaderRHI);
#ifndef __EMSCRIPTEN__
	// TODO: On WebGL1/GLES2 builds, the following assert() always comes out false, however when simply ignoring this check,
	// everything seems to be working fine. That said, I don't know what should change here, shader resource views are a D3D abstraction,
	// but I think InternalSetShaderTexture() and RHISetShaderSampler() calls below need to occur even on GLES2.
	check(FOpenGL::SupportsResourceView());
#endif
	FOpenGLShaderResourceView* SRV = ResourceCast(SRVRHI);
	GLuint Resource = 0;
	GLenum Target = GL_TEXTURE_BUFFER;
	int32 LimitMip = -1;
	if( SRV )
	{
		Resource = SRV->Resource;
		Target = SRV->Target;
		LimitMip = SRV->LimitMip;
		UpdateSRV(SRV);
	}
	InternalSetShaderTexture(NULL, SRV, FOpenGL::GetFirstPixelTextureUnit() + TextureIndex, Target, Resource, 0, LimitMip);
	RHISetShaderSampler(PixelShaderRHI,TextureIndex,PointSamplerState);
	
	FShaderCache::SetSRV(FShaderCache::GetDefaultCacheState(), SF_Pixel, TextureIndex, SRVRHI);
}

void FOpenGLDynamicRHI::RHISetShaderResourceViewParameter(FVertexShaderRHIParamRef VertexShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	VALIDATE_BOUND_SHADER(VertexShaderRHI);

	VERIFY_GL_SCOPE();
	check(FOpenGL::SupportsResourceView());
	FOpenGLShaderResourceView* SRV = ResourceCast(SRVRHI);
	GLuint Resource = 0;
	GLenum Target = GL_TEXTURE_BUFFER;
	int32 LimitMip = -1;
	if( SRV )
	{
		Resource = SRV->Resource;
		Target = SRV->Target;
		LimitMip = SRV->LimitMip;
		UpdateSRV(SRV);
	}
	InternalSetShaderTexture(NULL, SRV, FOpenGL::GetFirstVertexTextureUnit() + TextureIndex, Target, Resource, 0, LimitMip);
	RHISetShaderSampler(VertexShaderRHI,TextureIndex,PointSamplerState);
	
	FShaderCache::SetSRV(FShaderCache::GetDefaultCacheState(), SF_Vertex, TextureIndex, SRVRHI);
}

void FOpenGLDynamicRHI::RHISetShaderResourceViewParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	VERIFY_GL_SCOPE();
	check(FOpenGL::SupportsResourceView());
	FOpenGLShaderResourceView* SRV = ResourceCast(SRVRHI);
	GLuint Resource = 0;
	GLenum Target = GL_TEXTURE_BUFFER;
	int32 LimitMip = -1;
	if( SRV )
	{
		Resource = SRV->Resource;
		Target = SRV->Target;
		LimitMip = SRV->LimitMip;
		UpdateSRV(SRV);
	}
	InternalSetShaderTexture(NULL, SRV, FOpenGL::GetFirstComputeTextureUnit() + TextureIndex, Target, Resource, 0, LimitMip);
	RHISetShaderSampler(ComputeShaderRHI,TextureIndex,PointSamplerState);
	
	FShaderCache::SetSRV(FShaderCache::GetDefaultCacheState(), SF_Compute, TextureIndex, SRVRHI);
}

void FOpenGLDynamicRHI::RHISetShaderResourceViewParameter(FHullShaderRHIParamRef HullShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	VALIDATE_BOUND_SHADER(HullShaderRHI);

	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	VERIFY_GL_SCOPE();
	check(FOpenGL::SupportsResourceView());
	FOpenGLShaderResourceView* SRV = ResourceCast(SRVRHI);
	GLuint Resource = 0;
	GLenum Target = GL_TEXTURE_BUFFER;
	int32 LimitMip = -1;
	if( SRV )
	{
		Resource = SRV->Resource;
		Target = SRV->Target;
		LimitMip = SRV->LimitMip;
		UpdateSRV(SRV);
	}
	InternalSetShaderTexture(NULL, SRV, FOpenGL::GetFirstHullTextureUnit() + TextureIndex, Target, Resource, 0, LimitMip);
	
	FShaderCache::SetSRV(FShaderCache::GetDefaultCacheState(), SF_Hull, TextureIndex, SRVRHI);
}

void FOpenGLDynamicRHI::RHISetShaderResourceViewParameter(FDomainShaderRHIParamRef DomainShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	VALIDATE_BOUND_SHADER(DomainShaderRHI);

	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	VERIFY_GL_SCOPE();
	check(FOpenGL::SupportsResourceView());
	FOpenGLShaderResourceView* SRV = ResourceCast(SRVRHI);
	GLuint Resource = 0;
	GLenum Target = GL_TEXTURE_BUFFER;
	int32 LimitMip = -1;
	if( SRV )
	{
		Resource = SRV->Resource;
		Target = SRV->Target;
		LimitMip = SRV->LimitMip;
		UpdateSRV(SRV);
	}
	InternalSetShaderTexture(NULL, SRV, FOpenGL::GetFirstDomainTextureUnit() + TextureIndex, Target, Resource, 0, LimitMip);
	
	FShaderCache::SetSRV(FShaderCache::GetDefaultCacheState(), SF_Domain, TextureIndex, SRVRHI);
}

void FOpenGLDynamicRHI::RHISetShaderResourceViewParameter(FGeometryShaderRHIParamRef GeometryShaderRHI,uint32 TextureIndex,FShaderResourceViewRHIParamRef SRVRHI)
{
	VALIDATE_BOUND_SHADER(GeometryShaderRHI);

	VERIFY_GL_SCOPE();
	check(FOpenGL::SupportsResourceView());
	FOpenGLShaderResourceView* SRV = ResourceCast(SRVRHI);
	GLuint Resource = 0;
	GLenum Target = GL_TEXTURE_BUFFER;
	int32 LimitMip = -1;
	if( SRV )
	{
		Resource = SRV->Resource;
		Target = SRV->Target;
		LimitMip = SRV->LimitMip;
		UpdateSRV(SRV);
	}
	InternalSetShaderTexture(NULL, SRV, FOpenGL::GetFirstGeometryTextureUnit() + TextureIndex, Target, Resource, 0, LimitMip);
	RHISetShaderSampler(GeometryShaderRHI,TextureIndex,PointSamplerState);
	
	FShaderCache::SetSRV(FShaderCache::GetDefaultCacheState(), SF_Geometry, TextureIndex, SRVRHI);
}

void FOpenGLDynamicRHI::RHISetShaderTexture(FVertexShaderRHIParamRef VertexShaderRHI,uint32 TextureIndex,FTextureRHIParamRef NewTextureRHI)
{
	VALIDATE_BOUND_SHADER(VertexShaderRHI);

	VERIFY_GL_SCOPE();
	FOpenGLTextureBase* NewTexture = GetOpenGLTextureFromRHITexture(NewTextureRHI);
	if (NewTexture)
	{
		InternalSetShaderTexture(NewTexture, nullptr, FOpenGL::GetFirstVertexTextureUnit() + TextureIndex, NewTexture->Target, NewTexture->Resource, NewTextureRHI->GetNumMips(), -1);
	}
	else
	{
		InternalSetShaderTexture(nullptr, nullptr, FOpenGL::GetFirstVertexTextureUnit() + TextureIndex, 0, 0, 0, -1);
	}
	
	FShaderCache::SetTexture(FShaderCache::GetDefaultCacheState(), SF_Vertex, TextureIndex, NewTextureRHI);
}

void FOpenGLDynamicRHI::RHISetShaderTexture(FHullShaderRHIParamRef HullShaderRHI,uint32 TextureIndex,FTextureRHIParamRef NewTextureRHI)
{
	VALIDATE_BOUND_SHADER(HullShaderRHI);
	
	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	VERIFY_GL_SCOPE();
	FOpenGLTextureBase* NewTexture = GetOpenGLTextureFromRHITexture(NewTextureRHI);
	if (NewTexture)
	{
		InternalSetShaderTexture(NewTexture, nullptr, FOpenGL::GetFirstHullTextureUnit() + TextureIndex, NewTexture->Target, NewTexture->Resource, NewTextureRHI->GetNumMips(), -1);
	}
	else
	{
		InternalSetShaderTexture(nullptr, nullptr, FOpenGL::GetFirstHullTextureUnit() + TextureIndex, 0, 0, 0, -1);
	}
	
	FShaderCache::SetTexture(FShaderCache::GetDefaultCacheState(), SF_Hull, TextureIndex, NewTextureRHI);
}

void FOpenGLDynamicRHI::RHISetShaderTexture(FDomainShaderRHIParamRef DomainShaderRHI,uint32 TextureIndex,FTextureRHIParamRef NewTextureRHI)
{
	VALIDATE_BOUND_SHADER(DomainShaderRHI);

	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	VERIFY_GL_SCOPE();
	FOpenGLTextureBase* NewTexture = GetOpenGLTextureFromRHITexture(NewTextureRHI);
	if (NewTexture)
	{
		InternalSetShaderTexture(NewTexture, nullptr, FOpenGL::GetFirstDomainTextureUnit() + TextureIndex, NewTexture->Target, NewTexture->Resource, NewTextureRHI->GetNumMips(), -1);
	}
	else
	{
		InternalSetShaderTexture(nullptr, nullptr, FOpenGL::GetFirstDomainTextureUnit() + TextureIndex, 0, 0, 0, -1);
	}
	
	FShaderCache::SetTexture(FShaderCache::GetDefaultCacheState(), SF_Domain, TextureIndex, NewTextureRHI);
}

void FOpenGLDynamicRHI::RHISetShaderTexture(FGeometryShaderRHIParamRef GeometryShaderRHI,uint32 TextureIndex,FTextureRHIParamRef NewTextureRHI)
{
	VALIDATE_BOUND_SHADER(GeometryShaderRHI);

	VERIFY_GL_SCOPE();
	FOpenGLTextureBase* NewTexture = GetOpenGLTextureFromRHITexture(NewTextureRHI);
	if (NewTexture)
	{
		InternalSetShaderTexture(NewTexture, nullptr, FOpenGL::GetFirstGeometryTextureUnit() + TextureIndex, NewTexture->Target, NewTexture->Resource, NewTextureRHI->GetNumMips(), -1);
	}
	else
	{
		InternalSetShaderTexture(nullptr, nullptr, FOpenGL::GetFirstGeometryTextureUnit() + TextureIndex, 0, 0, 0, -1);
	}
	
	FShaderCache::SetTexture(FShaderCache::GetDefaultCacheState(), SF_Geometry, TextureIndex, NewTextureRHI);
}

void FOpenGLDynamicRHI::RHISetShaderTexture(FPixelShaderRHIParamRef PixelShaderRHI,uint32 TextureIndex,FTextureRHIParamRef NewTextureRHI)
{
	VALIDATE_BOUND_SHADER(PixelShaderRHI);

	VERIFY_GL_SCOPE();
	FOpenGLTextureBase* NewTexture = GetOpenGLTextureFromRHITexture(NewTextureRHI);
	if (NewTexture)
	{
		InternalSetShaderTexture(NewTexture, nullptr, FOpenGL::GetFirstPixelTextureUnit() + TextureIndex, NewTexture->Target, NewTexture->Resource, NewTextureRHI->GetNumMips(), -1);
	}
	else
	{
		InternalSetShaderTexture(nullptr, nullptr, FOpenGL::GetFirstPixelTextureUnit() + TextureIndex, 0, 0, 0, -1);
	}
	
	FShaderCache::SetTexture(FShaderCache::GetDefaultCacheState(), SF_Pixel, TextureIndex, NewTextureRHI);
}

void FOpenGLDynamicRHI::RHISetShaderSampler(FVertexShaderRHIParamRef VertexShaderRHI,uint32 SamplerIndex,FSamplerStateRHIParamRef NewStateRHI)
{
	VALIDATE_BOUND_SHADER(VertexShaderRHI);

	VERIFY_GL_SCOPE();
	FOpenGLSamplerState* NewState = ResourceCast(NewStateRHI);
	if (FOpenGL::SupportsSamplerObjects())
	{
		if ( OpenGLConsoleVariables::bBindlessTexture == 0 || !FOpenGL::SupportsBindlessTexture())
		{
			FOpenGL::BindSampler(FOpenGL::GetFirstVertexTextureUnit() + SamplerIndex, NewState->Resource);
		}
		else
		{
			PendingState.SamplerStates[FOpenGL::GetFirstVertexTextureUnit() + SamplerIndex] = NewState;
		}
	}
	else
	{
		InternalSetSamplerStates(FOpenGL::GetFirstVertexTextureUnit() + SamplerIndex, NewState);
	}
	
	FShaderCache::SetSamplerState(FShaderCache::GetDefaultCacheState(), SF_Vertex, SamplerIndex, NewStateRHI);
}

void FOpenGLDynamicRHI::RHISetShaderSampler(FHullShaderRHIParamRef HullShaderRHI,uint32 SamplerIndex,FSamplerStateRHIParamRef NewStateRHI)
{
	VALIDATE_BOUND_SHADER(HullShaderRHI);

	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	VERIFY_GL_SCOPE();
	FOpenGLSamplerState* NewState = ResourceCast(NewStateRHI);
	if (FOpenGL::SupportsSamplerObjects())
	{
		if ( OpenGLConsoleVariables::bBindlessTexture == 0 || !FOpenGL::SupportsBindlessTexture())
		{
			FOpenGL::BindSampler(FOpenGL::GetFirstHullTextureUnit() + SamplerIndex, NewState->Resource);
		}
		else
		{
			PendingState.SamplerStates[FOpenGL::GetFirstHullTextureUnit() + SamplerIndex] = NewState;
		}
	}
	else
	{
		InternalSetSamplerStates(FOpenGL::GetFirstHullTextureUnit() + SamplerIndex, NewState);
	}
	
	FShaderCache::SetSamplerState(FShaderCache::GetDefaultCacheState(), SF_Hull, SamplerIndex, NewStateRHI);
}
void FOpenGLDynamicRHI::RHISetShaderSampler(FDomainShaderRHIParamRef DomainShaderRHI,uint32 SamplerIndex,FSamplerStateRHIParamRef NewStateRHI)
{
	VALIDATE_BOUND_SHADER(DomainShaderRHI);

	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	VERIFY_GL_SCOPE();
	FOpenGLSamplerState* NewState = ResourceCast(NewStateRHI);
	if (FOpenGL::SupportsSamplerObjects())
	{
		if ( OpenGLConsoleVariables::bBindlessTexture == 0 || !FOpenGL::SupportsBindlessTexture())
		{
			FOpenGL::BindSampler(FOpenGL::GetFirstDomainTextureUnit() + SamplerIndex, NewState->Resource);
		}
		else
		{
			PendingState.SamplerStates[FOpenGL::GetFirstDomainTextureUnit() + SamplerIndex] = NewState;
		}
	}
	else
	{
		InternalSetSamplerStates(FOpenGL::GetFirstDomainTextureUnit() + SamplerIndex, NewState);
	}
	
	FShaderCache::SetSamplerState(FShaderCache::GetDefaultCacheState(), SF_Domain, SamplerIndex, NewStateRHI);
}

void FOpenGLDynamicRHI::RHISetShaderSampler(FGeometryShaderRHIParamRef GeometryShaderRHI,uint32 SamplerIndex,FSamplerStateRHIParamRef NewStateRHI)
{
	VALIDATE_BOUND_SHADER(GeometryShaderRHI);

	VERIFY_GL_SCOPE();
	FOpenGLSamplerState* NewState = ResourceCast(NewStateRHI);
	if (FOpenGL::SupportsSamplerObjects())
	{
		if ( OpenGLConsoleVariables::bBindlessTexture == 0 || !FOpenGL::SupportsBindlessTexture())
		{
			FOpenGL::BindSampler(FOpenGL::GetFirstGeometryTextureUnit() + SamplerIndex, NewState->Resource);
		}
		else
		{
			PendingState.SamplerStates[FOpenGL::GetFirstGeometryTextureUnit() + SamplerIndex] = NewState;
		}
	}
	else
	{
		InternalSetSamplerStates(FOpenGL::GetFirstGeometryTextureUnit() + SamplerIndex, NewState);
	}
	
	FShaderCache::SetSamplerState(FShaderCache::GetDefaultCacheState(), SF_Geometry, SamplerIndex, NewStateRHI);
}

void FOpenGLDynamicRHI::RHISetShaderTexture(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 TextureIndex,FTextureRHIParamRef NewTextureRHI)
{
	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	VERIFY_GL_SCOPE();
	FOpenGLTextureBase* NewTexture = GetOpenGLTextureFromRHITexture(NewTextureRHI);
	if (NewTexture)
	{
		InternalSetShaderTexture(NewTexture, nullptr, FOpenGL::GetFirstComputeTextureUnit() + TextureIndex, NewTexture->Target, NewTexture->Resource, NewTextureRHI->GetNumMips(), -1);
	}
	else
	{
		InternalSetShaderTexture(nullptr, nullptr, FOpenGL::GetFirstComputeTextureUnit() + TextureIndex, 0, 0, 0, -1);
	}
	
	FShaderCache::SetTexture(FShaderCache::GetDefaultCacheState(), SF_Compute, TextureIndex, NewTextureRHI);
}

void FOpenGLDynamicRHI::RHISetShaderSampler(FPixelShaderRHIParamRef PixelShaderRHI,uint32 SamplerIndex,FSamplerStateRHIParamRef NewStateRHI)
{
	VALIDATE_BOUND_SHADER(PixelShaderRHI);

	VERIFY_GL_SCOPE();
	FOpenGLSamplerState* NewState = ResourceCast(NewStateRHI);
	if ( FOpenGL::SupportsSamplerObjects() )
	{
		if ( OpenGLConsoleVariables::bBindlessTexture == 0 || !FOpenGL::SupportsBindlessTexture())
		{
			FOpenGL::BindSampler(FOpenGL::GetFirstPixelTextureUnit() + SamplerIndex, NewState->Resource);
		}
		else
		{
			PendingState.SamplerStates[FOpenGL::GetFirstPixelTextureUnit() + SamplerIndex] = NewState;
		}
	}
	else
	{
		InternalSetSamplerStates(FOpenGL::GetFirstPixelTextureUnit() + SamplerIndex, NewState);
	}
	
	FShaderCache::SetSamplerState(FShaderCache::GetDefaultCacheState(), SF_Pixel, SamplerIndex, NewStateRHI);
}

void FOpenGLDynamicRHI::RHISetShaderUniformBuffer(FVertexShaderRHIParamRef VertexShaderRHI,uint32 BufferIndex,FUniformBufferRHIParamRef BufferRHI)
{
	VALIDATE_BOUND_SHADER(VertexShaderRHI);
	PendingState.BoundUniformBuffers[SF_Vertex][BufferIndex] = BufferRHI;
	PendingState.DirtyUniformBuffers[SF_Vertex] |= 1 << BufferIndex;
}

void FOpenGLDynamicRHI::RHISetShaderUniformBuffer(FHullShaderRHIParamRef HullShaderRHI,uint32 BufferIndex,FUniformBufferRHIParamRef BufferRHI)
{
	VALIDATE_BOUND_SHADER(HullShaderRHI);
	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	PendingState.BoundUniformBuffers[SF_Hull][BufferIndex] = BufferRHI;
	PendingState.DirtyUniformBuffers[SF_Hull] |= 1 << BufferIndex;
}

void FOpenGLDynamicRHI::RHISetShaderUniformBuffer(FDomainShaderRHIParamRef DomainShaderRHI,uint32 BufferIndex,FUniformBufferRHIParamRef BufferRHI)
{
	VALIDATE_BOUND_SHADER(DomainShaderRHI);
	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	PendingState.BoundUniformBuffers[SF_Domain][BufferIndex] = BufferRHI;
	PendingState.DirtyUniformBuffers[SF_Domain] |= 1 << BufferIndex;
}

void FOpenGLDynamicRHI::RHISetShaderUniformBuffer(FGeometryShaderRHIParamRef GeometryShaderRHI,uint32 BufferIndex,FUniformBufferRHIParamRef BufferRHI)
{
	VALIDATE_BOUND_SHADER(GeometryShaderRHI);
	PendingState.BoundUniformBuffers[SF_Geometry][BufferIndex] = BufferRHI;
	PendingState.DirtyUniformBuffers[SF_Geometry] |= 1 << BufferIndex;
}

void FOpenGLDynamicRHI::RHISetShaderSampler(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 SamplerIndex,FSamplerStateRHIParamRef NewStateRHI)
{
	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	VERIFY_GL_SCOPE();
	FOpenGLSamplerState* NewState = ResourceCast(NewStateRHI);
	if ( OpenGLConsoleVariables::bBindlessTexture == 0 || !FOpenGL::SupportsBindlessTexture())
	{
		FOpenGL::BindSampler(FOpenGL::GetFirstComputeTextureUnit() + SamplerIndex, NewState->Resource);
	}
	else
	{
		PendingState.SamplerStates[FOpenGL::GetFirstComputeTextureUnit() + SamplerIndex] = NewState;
	}
}

void FOpenGLDynamicRHI::RHISetShaderUniformBuffer(FPixelShaderRHIParamRef PixelShaderRHI,uint32 BufferIndex,FUniformBufferRHIParamRef BufferRHI)
{
	VALIDATE_BOUND_SHADER(PixelShaderRHI);
	PendingState.BoundUniformBuffers[SF_Pixel][BufferIndex] = BufferRHI;
	PendingState.DirtyUniformBuffers[SF_Pixel] |= 1 << BufferIndex;
}

void FOpenGLDynamicRHI::RHISetShaderUniformBuffer(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 BufferIndex,FUniformBufferRHIParamRef BufferRHI)
{
	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	PendingState.BoundUniformBuffers[SF_Compute][BufferIndex] = BufferRHI;
	PendingState.DirtyUniformBuffers[SF_Compute] |= 1 << BufferIndex;
}

void FOpenGLDynamicRHI::RHISetShaderParameter(FVertexShaderRHIParamRef VertexShaderRHI,uint32 BufferIndex,uint32 BaseIndex,uint32 NumBytes,const void* NewValue)
{
	VALIDATE_BOUND_SHADER(VertexShaderRHI);

	VERIFY_GL_SCOPE();
	PendingState.ShaderParameters[CrossCompiler::SHADER_STAGE_VERTEX].Set(BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FOpenGLDynamicRHI::RHISetShaderParameter(FPixelShaderRHIParamRef PixelShaderRHI,uint32 BufferIndex,uint32 BaseIndex,uint32 NumBytes,const void* NewValue)
{
	VALIDATE_BOUND_SHADER(PixelShaderRHI);

	VERIFY_GL_SCOPE();
	PendingState.ShaderParameters[CrossCompiler::SHADER_STAGE_PIXEL].Set(BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FOpenGLDynamicRHI::RHISetShaderParameter(FHullShaderRHIParamRef HullShaderRHI,uint32 BufferIndex,uint32 BaseIndex,uint32 NumBytes,const void* NewValue)
{
	VALIDATE_BOUND_SHADER(HullShaderRHI);

	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	VERIFY_GL_SCOPE();
	PendingState.ShaderParameters[CrossCompiler::SHADER_STAGE_HULL].Set(BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FOpenGLDynamicRHI::RHISetShaderParameter(FDomainShaderRHIParamRef DomainShaderRHI,uint32 BufferIndex,uint32 BaseIndex,uint32 NumBytes,const void* NewValue)
{
	VALIDATE_BOUND_SHADER(DomainShaderRHI);

	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	VERIFY_GL_SCOPE();
	PendingState.ShaderParameters[CrossCompiler::SHADER_STAGE_DOMAIN].Set(BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FOpenGLDynamicRHI::RHISetShaderParameter(FGeometryShaderRHIParamRef GeometryShaderRHI,uint32 BufferIndex,uint32 BaseIndex,uint32 NumBytes,const void* NewValue)
{
	VALIDATE_BOUND_SHADER(GeometryShaderRHI);

	VERIFY_GL_SCOPE();
	PendingState.ShaderParameters[CrossCompiler::SHADER_STAGE_GEOMETRY].Set(BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FOpenGLDynamicRHI::RHISetShaderParameter(FComputeShaderRHIParamRef ComputeShaderRHI,uint32 BufferIndex,uint32 BaseIndex,uint32 NumBytes,const void* NewValue)
{ 
	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	PendingState.ShaderParameters[CrossCompiler::SHADER_STAGE_COMPUTE].Set(BufferIndex, BaseIndex, NumBytes, NewValue);
}

void FOpenGLDynamicRHI::RHISetDepthStencilState(FDepthStencilStateRHIParamRef NewStateRHI,uint32 StencilRef)
{
	VERIFY_GL_SCOPE();
	FOpenGLDepthStencilState* NewState = ResourceCast(NewStateRHI);
	PendingState.DepthStencilState = NewState->Data;
	PendingState.StencilRef = StencilRef;
	
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FShaderCache::SetDepthStencilState(FShaderCache::GetDefaultCacheState(), NewStateRHI);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FOpenGLDynamicRHI::RHISetStencilRef(uint32 StencilRef)
{
	VERIFY_GL_SCOPE();
	PendingState.StencilRef = StencilRef;
}

void FOpenGLDynamicRHI::UpdateDepthStencilStateInOpenGLContext( FOpenGLContextState& ContextState )
{
	if (ContextState.DepthStencilState.bZEnable != PendingState.DepthStencilState.bZEnable)
	{
		if (PendingState.DepthStencilState.bZEnable)
		{
			glEnable(GL_DEPTH_TEST);
		}
		else
		{
			glDisable(GL_DEPTH_TEST);
		}
		ContextState.DepthStencilState.bZEnable = PendingState.DepthStencilState.bZEnable;
	}

	if (ContextState.DepthStencilState.bZWriteEnable != PendingState.DepthStencilState.bZWriteEnable)
	{
		glDepthMask(PendingState.DepthStencilState.bZWriteEnable);
		ContextState.DepthStencilState.bZWriteEnable = PendingState.DepthStencilState.bZWriteEnable;
	}

	if (PendingState.DepthStencilState.bZEnable)
	{
		if (ContextState.DepthStencilState.ZFunc != PendingState.DepthStencilState.ZFunc)
		{
			glDepthFunc(PendingState.DepthStencilState.ZFunc);
			ContextState.DepthStencilState.ZFunc = PendingState.DepthStencilState.ZFunc;
		}
	}

	if (ContextState.DepthStencilState.bStencilEnable != PendingState.DepthStencilState.bStencilEnable)
	{
		if (PendingState.DepthStencilState.bStencilEnable)
		{
			glEnable(GL_STENCIL_TEST);
		}
		else
		{
			glDisable(GL_STENCIL_TEST);
		}
		ContextState.DepthStencilState.bStencilEnable = PendingState.DepthStencilState.bStencilEnable;
	}

	// If only two-sided <-> one-sided stencil mode changes, and nothing else, we need to call full set of functions
	// to ensure all drivers handle this correctly - some of them might keep those states in different variables.
	if (ContextState.DepthStencilState.bTwoSidedStencilMode != PendingState.DepthStencilState.bTwoSidedStencilMode)
	{
		// Invalidate cache to enforce update of part of stencil state that needs to be set with different functions, when needed next
		// Values below are all invalid, but they'll never be used, only compared to new values to be set.
		ContextState.DepthStencilState.StencilFunc = 0xFFFF;
		ContextState.DepthStencilState.StencilFail = 0xFFFF;
		ContextState.DepthStencilState.StencilZFail = 0xFFFF;
		ContextState.DepthStencilState.StencilPass = 0xFFFF;
		ContextState.DepthStencilState.CCWStencilFunc = 0xFFFF;
		ContextState.DepthStencilState.CCWStencilFail = 0xFFFF;
		ContextState.DepthStencilState.CCWStencilZFail = 0xFFFF;
		ContextState.DepthStencilState.CCWStencilPass = 0xFFFF;
		ContextState.DepthStencilState.StencilReadMask = 0xFFFF;

		ContextState.DepthStencilState.bTwoSidedStencilMode = PendingState.DepthStencilState.bTwoSidedStencilMode;
	}

	if (PendingState.DepthStencilState.bStencilEnable)
	{
		if (PendingState.DepthStencilState.bTwoSidedStencilMode)
		{
			if (ContextState.DepthStencilState.StencilFunc != PendingState.DepthStencilState.StencilFunc
				|| ContextState.StencilRef != PendingState.StencilRef
				|| ContextState.DepthStencilState.StencilReadMask != PendingState.DepthStencilState.StencilReadMask)
			{
				glStencilFuncSeparate(GL_BACK, PendingState.DepthStencilState.StencilFunc, PendingState.StencilRef, PendingState.DepthStencilState.StencilReadMask);
				ContextState.DepthStencilState.StencilFunc = PendingState.DepthStencilState.StencilFunc;
			}

			if (ContextState.DepthStencilState.StencilFail != PendingState.DepthStencilState.StencilFail
				|| ContextState.DepthStencilState.StencilZFail != PendingState.DepthStencilState.StencilZFail
				|| ContextState.DepthStencilState.StencilPass != PendingState.DepthStencilState.StencilPass)
			{
				glStencilOpSeparate(GL_BACK, PendingState.DepthStencilState.StencilFail, PendingState.DepthStencilState.StencilZFail, PendingState.DepthStencilState.StencilPass);
				ContextState.DepthStencilState.StencilFail = PendingState.DepthStencilState.StencilFail;
				ContextState.DepthStencilState.StencilZFail = PendingState.DepthStencilState.StencilZFail;
				ContextState.DepthStencilState.StencilPass = PendingState.DepthStencilState.StencilPass;
			}

			if (ContextState.DepthStencilState.CCWStencilFunc != PendingState.DepthStencilState.CCWStencilFunc
				|| ContextState.StencilRef != PendingState.StencilRef
				|| ContextState.DepthStencilState.StencilReadMask != PendingState.DepthStencilState.StencilReadMask)
			{
				glStencilFuncSeparate(GL_FRONT, PendingState.DepthStencilState.CCWStencilFunc, PendingState.StencilRef, PendingState.DepthStencilState.StencilReadMask);
				ContextState.DepthStencilState.CCWStencilFunc = PendingState.DepthStencilState.CCWStencilFunc;
			}

			if (ContextState.DepthStencilState.CCWStencilFail != PendingState.DepthStencilState.CCWStencilFail
				|| ContextState.DepthStencilState.CCWStencilZFail != PendingState.DepthStencilState.CCWStencilZFail
				|| ContextState.DepthStencilState.CCWStencilPass != PendingState.DepthStencilState.CCWStencilPass)
			{
				glStencilOpSeparate(GL_FRONT, PendingState.DepthStencilState.CCWStencilFail, PendingState.DepthStencilState.CCWStencilZFail, PendingState.DepthStencilState.CCWStencilPass);
				ContextState.DepthStencilState.CCWStencilFail = PendingState.DepthStencilState.CCWStencilFail;
				ContextState.DepthStencilState.CCWStencilZFail = PendingState.DepthStencilState.CCWStencilZFail;
				ContextState.DepthStencilState.CCWStencilPass = PendingState.DepthStencilState.CCWStencilPass;
			}

			ContextState.DepthStencilState.StencilReadMask = PendingState.DepthStencilState.StencilReadMask;
			ContextState.StencilRef = PendingState.StencilRef;
		}
		else
		{
			if (ContextState.DepthStencilState.StencilFunc != PendingState.DepthStencilState.StencilFunc
				|| ContextState.StencilRef != PendingState.StencilRef
				|| ContextState.DepthStencilState.StencilReadMask != PendingState.DepthStencilState.StencilReadMask)
			{
				glStencilFunc(PendingState.DepthStencilState.StencilFunc, PendingState.StencilRef, PendingState.DepthStencilState.StencilReadMask);
				ContextState.DepthStencilState.StencilFunc = PendingState.DepthStencilState.StencilFunc;
				ContextState.DepthStencilState.StencilReadMask = PendingState.DepthStencilState.StencilReadMask;
				ContextState.StencilRef = PendingState.StencilRef;
			}

			if (ContextState.DepthStencilState.StencilFail != PendingState.DepthStencilState.StencilFail
				|| ContextState.DepthStencilState.StencilZFail != PendingState.DepthStencilState.StencilZFail
				|| ContextState.DepthStencilState.StencilPass != PendingState.DepthStencilState.StencilPass)
			{
				glStencilOp(PendingState.DepthStencilState.StencilFail, PendingState.DepthStencilState.StencilZFail, PendingState.DepthStencilState.StencilPass);
				ContextState.DepthStencilState.StencilFail = PendingState.DepthStencilState.StencilFail;
				ContextState.DepthStencilState.StencilZFail = PendingState.DepthStencilState.StencilZFail;
				ContextState.DepthStencilState.StencilPass = PendingState.DepthStencilState.StencilPass;
			}
		}

		if (ContextState.DepthStencilState.StencilWriteMask != PendingState.DepthStencilState.StencilWriteMask)
		{
			glStencilMask(PendingState.DepthStencilState.StencilWriteMask);
			ContextState.DepthStencilState.StencilWriteMask = PendingState.DepthStencilState.StencilWriteMask;
		}
	}
}

void FOpenGLDynamicRHI::SetPendingBlendStateForActiveRenderTargets( FOpenGLContextState& ContextState )
{
	VERIFY_GL_SCOPE();

	bool bABlendWasSet = false;

	//
	// Need to expand setting for glBlendFunction and glBlendEquation

	const uint32 NumRenderTargets = FOpenGL::SupportsMultipleRenderTargets() ? MaxSimultaneousRenderTargets : 1;

	for (uint32 RenderTargetIndex = 0;RenderTargetIndex < NumRenderTargets; ++RenderTargetIndex)
	{
		if(PendingState.RenderTargets[RenderTargetIndex] == 0)
		{
			// Even if on this stage blend states are incompatible with other stages, we can disregard it, as no render target is assigned to it.
			continue;
		}

		const FOpenGLBlendStateData::FRenderTarget& RenderTargetBlendState = PendingState.BlendState.RenderTargets[RenderTargetIndex];
		FOpenGLBlendStateData::FRenderTarget& CachedRenderTargetBlendState = ContextState.BlendState.RenderTargets[RenderTargetIndex];

		if (CachedRenderTargetBlendState.bAlphaBlendEnable != RenderTargetBlendState.bAlphaBlendEnable)
		{
			if (RenderTargetBlendState.bAlphaBlendEnable)
			{
				FOpenGL::EnableIndexed(GL_BLEND,RenderTargetIndex);
			}
			else
			{
				FOpenGL::DisableIndexed(GL_BLEND,RenderTargetIndex);
			}
			CachedRenderTargetBlendState.bAlphaBlendEnable = RenderTargetBlendState.bAlphaBlendEnable;
		}

		if (RenderTargetBlendState.bAlphaBlendEnable)
		{
			if ( FOpenGL::SupportsSeparateAlphaBlend() )
			{
				// Set current blend per stage
				if (RenderTargetBlendState.bSeparateAlphaBlendEnable)
				{
					if (CachedRenderTargetBlendState.ColorSourceBlendFactor != RenderTargetBlendState.ColorSourceBlendFactor
						|| CachedRenderTargetBlendState.ColorDestBlendFactor != RenderTargetBlendState.ColorDestBlendFactor
						|| CachedRenderTargetBlendState.AlphaSourceBlendFactor != RenderTargetBlendState.AlphaSourceBlendFactor
						|| CachedRenderTargetBlendState.AlphaDestBlendFactor != RenderTargetBlendState.AlphaDestBlendFactor)
					{
						FOpenGL::BlendFuncSeparatei(
							RenderTargetIndex, 
							RenderTargetBlendState.ColorSourceBlendFactor, RenderTargetBlendState.ColorDestBlendFactor,
							RenderTargetBlendState.AlphaSourceBlendFactor, RenderTargetBlendState.AlphaDestBlendFactor
							);
					}

					if (CachedRenderTargetBlendState.ColorBlendOperation != RenderTargetBlendState.ColorBlendOperation
						|| CachedRenderTargetBlendState.AlphaBlendOperation != RenderTargetBlendState.AlphaBlendOperation)
					{
						FOpenGL::BlendEquationSeparatei(
							RenderTargetIndex, 
							RenderTargetBlendState.ColorBlendOperation,
							RenderTargetBlendState.AlphaBlendOperation
							);
					}
				}
				else
				{
					if (CachedRenderTargetBlendState.ColorSourceBlendFactor != RenderTargetBlendState.ColorSourceBlendFactor
						|| CachedRenderTargetBlendState.ColorDestBlendFactor != RenderTargetBlendState.ColorDestBlendFactor
						|| CachedRenderTargetBlendState.AlphaSourceBlendFactor != RenderTargetBlendState.ColorSourceBlendFactor
						|| CachedRenderTargetBlendState.AlphaDestBlendFactor != RenderTargetBlendState.ColorDestBlendFactor)
					{
						FOpenGL::BlendFunci(RenderTargetIndex, RenderTargetBlendState.ColorSourceBlendFactor, RenderTargetBlendState.ColorDestBlendFactor);
					}

					if (CachedRenderTargetBlendState.ColorBlendOperation != RenderTargetBlendState.ColorBlendOperation)
					{
						FOpenGL::BlendEquationi(RenderTargetIndex, RenderTargetBlendState.ColorBlendOperation);
					}
				}

				CachedRenderTargetBlendState.bSeparateAlphaBlendEnable = RenderTargetBlendState.bSeparateAlphaBlendEnable;
				CachedRenderTargetBlendState.ColorBlendOperation = RenderTargetBlendState.ColorBlendOperation;
				CachedRenderTargetBlendState.ColorSourceBlendFactor = RenderTargetBlendState.ColorSourceBlendFactor;
				CachedRenderTargetBlendState.ColorDestBlendFactor = RenderTargetBlendState.ColorDestBlendFactor;
				if( RenderTargetBlendState.bSeparateAlphaBlendEnable )
				{ 
					CachedRenderTargetBlendState.AlphaSourceBlendFactor = RenderTargetBlendState.AlphaSourceBlendFactor;
					CachedRenderTargetBlendState.AlphaDestBlendFactor = RenderTargetBlendState.AlphaDestBlendFactor;
				}
				else
				{
					CachedRenderTargetBlendState.AlphaSourceBlendFactor = RenderTargetBlendState.ColorSourceBlendFactor;
					CachedRenderTargetBlendState.AlphaDestBlendFactor = RenderTargetBlendState.ColorDestBlendFactor;
				}
			}
			else
			{
				if( bABlendWasSet )
				{
					// Detect the case of subsequent render target needing different blend setup than one already set in this call.
					if( CachedRenderTargetBlendState.bSeparateAlphaBlendEnable != RenderTargetBlendState.bSeparateAlphaBlendEnable
						|| CachedRenderTargetBlendState.ColorBlendOperation != RenderTargetBlendState.ColorBlendOperation
						|| CachedRenderTargetBlendState.ColorSourceBlendFactor != RenderTargetBlendState.ColorSourceBlendFactor
						|| CachedRenderTargetBlendState.ColorDestBlendFactor != RenderTargetBlendState.ColorDestBlendFactor
						|| ( RenderTargetBlendState.bSeparateAlphaBlendEnable && 
							( CachedRenderTargetBlendState.AlphaSourceBlendFactor != RenderTargetBlendState.AlphaSourceBlendFactor
							|| CachedRenderTargetBlendState.AlphaDestBlendFactor != RenderTargetBlendState.AlphaDestBlendFactor
							)
							)
						)
						UE_LOG(LogRHI, Fatal, TEXT("OpenGL state on draw requires setting different blend operation or factors to different render targets. This is not supported on Mac OS X!"));
				}
				else
				{
					// Set current blend to all stages
					if (RenderTargetBlendState.bSeparateAlphaBlendEnable)
					{
						if (CachedRenderTargetBlendState.ColorSourceBlendFactor != RenderTargetBlendState.ColorSourceBlendFactor
							|| CachedRenderTargetBlendState.ColorDestBlendFactor != RenderTargetBlendState.ColorDestBlendFactor
							|| CachedRenderTargetBlendState.AlphaSourceBlendFactor != RenderTargetBlendState.AlphaSourceBlendFactor
							|| CachedRenderTargetBlendState.AlphaDestBlendFactor != RenderTargetBlendState.AlphaDestBlendFactor)
						{
							glBlendFuncSeparate(
								RenderTargetBlendState.ColorSourceBlendFactor, RenderTargetBlendState.ColorDestBlendFactor,
								RenderTargetBlendState.AlphaSourceBlendFactor, RenderTargetBlendState.AlphaDestBlendFactor
								);
						}

						if (CachedRenderTargetBlendState.ColorBlendOperation != RenderTargetBlendState.ColorBlendOperation
							|| CachedRenderTargetBlendState.AlphaBlendOperation != RenderTargetBlendState.AlphaBlendOperation)
						{
							glBlendEquationSeparate(
								RenderTargetBlendState.ColorBlendOperation,
								RenderTargetBlendState.AlphaBlendOperation
								);
						}
					}
					else
					{
						if (CachedRenderTargetBlendState.ColorSourceBlendFactor != RenderTargetBlendState.ColorSourceBlendFactor
							|| CachedRenderTargetBlendState.ColorDestBlendFactor != RenderTargetBlendState.ColorDestBlendFactor
							|| CachedRenderTargetBlendState.AlphaSourceBlendFactor != RenderTargetBlendState.ColorSourceBlendFactor
							|| CachedRenderTargetBlendState.AlphaDestBlendFactor != RenderTargetBlendState.ColorDestBlendFactor)
						{
							glBlendFunc(RenderTargetBlendState.ColorSourceBlendFactor, RenderTargetBlendState.ColorDestBlendFactor);
						}

						if (CachedRenderTargetBlendState.ColorBlendOperation != RenderTargetBlendState.ColorBlendOperation
							|| CachedRenderTargetBlendState.AlphaBlendOperation != RenderTargetBlendState.ColorBlendOperation)
						{
							glBlendEquation(RenderTargetBlendState.ColorBlendOperation);
						}
					}

					// Set cached values of all stages to what they were set by global calls, common to all stages
					for(uint32 RenderTargetIndex2 = 0; RenderTargetIndex2 < MaxSimultaneousRenderTargets; ++RenderTargetIndex2 )
					{
						FOpenGLBlendStateData::FRenderTarget& CachedRenderTargetBlendState2 = ContextState.BlendState.RenderTargets[RenderTargetIndex2];
						CachedRenderTargetBlendState2.bSeparateAlphaBlendEnable = RenderTargetBlendState.bSeparateAlphaBlendEnable;
						CachedRenderTargetBlendState2.ColorBlendOperation = RenderTargetBlendState.ColorBlendOperation;
						CachedRenderTargetBlendState2.ColorSourceBlendFactor = RenderTargetBlendState.ColorSourceBlendFactor;
						CachedRenderTargetBlendState2.ColorDestBlendFactor = RenderTargetBlendState.ColorDestBlendFactor;
						if( RenderTargetBlendState.bSeparateAlphaBlendEnable )
						{
							CachedRenderTargetBlendState2.AlphaBlendOperation = RenderTargetBlendState.AlphaBlendOperation;
							CachedRenderTargetBlendState2.AlphaSourceBlendFactor = RenderTargetBlendState.AlphaSourceBlendFactor;
							CachedRenderTargetBlendState2.AlphaDestBlendFactor = RenderTargetBlendState.AlphaDestBlendFactor;
							CachedRenderTargetBlendState2.AlphaBlendOperation = RenderTargetBlendState.AlphaBlendOperation;
						}
						else
						{
							CachedRenderTargetBlendState2.AlphaBlendOperation = RenderTargetBlendState.ColorBlendOperation;
							CachedRenderTargetBlendState2.AlphaSourceBlendFactor = RenderTargetBlendState.ColorSourceBlendFactor;
							CachedRenderTargetBlendState2.AlphaDestBlendFactor = RenderTargetBlendState.ColorDestBlendFactor;
							CachedRenderTargetBlendState2.AlphaBlendOperation = RenderTargetBlendState.ColorBlendOperation;
						}
					}

					bABlendWasSet = true;
				}
			}
		}

		CachedRenderTargetBlendState.bSeparateAlphaBlendEnable = RenderTargetBlendState.bSeparateAlphaBlendEnable;

		if(CachedRenderTargetBlendState.ColorWriteMaskR != RenderTargetBlendState.ColorWriteMaskR
			|| CachedRenderTargetBlendState.ColorWriteMaskG != RenderTargetBlendState.ColorWriteMaskG
			|| CachedRenderTargetBlendState.ColorWriteMaskB != RenderTargetBlendState.ColorWriteMaskB
			|| CachedRenderTargetBlendState.ColorWriteMaskA != RenderTargetBlendState.ColorWriteMaskA)
		{
			FOpenGL::ColorMaskIndexed(
				RenderTargetIndex,
				RenderTargetBlendState.ColorWriteMaskR,
				RenderTargetBlendState.ColorWriteMaskG,
				RenderTargetBlendState.ColorWriteMaskB,
				RenderTargetBlendState.ColorWriteMaskA
				);

			CachedRenderTargetBlendState.ColorWriteMaskR = RenderTargetBlendState.ColorWriteMaskR;
			CachedRenderTargetBlendState.ColorWriteMaskG = RenderTargetBlendState.ColorWriteMaskG;
			CachedRenderTargetBlendState.ColorWriteMaskB = RenderTargetBlendState.ColorWriteMaskB;
			CachedRenderTargetBlendState.ColorWriteMaskA = RenderTargetBlendState.ColorWriteMaskA;
		}
	}
}

void FOpenGLDynamicRHI::RHISetBlendState(FBlendStateRHIParamRef NewStateRHI,const FLinearColor& BlendFactor)
{
	FOpenGLBlendState* NewState = ResourceCast(NewStateRHI);
	FMemory::Memcpy(&PendingState.BlendState,&(NewState->Data),sizeof(FOpenGLBlendStateData));
	
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FShaderCache::SetBlendState(FShaderCache::GetDefaultCacheState(), NewStateRHI);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FOpenGLDynamicRHI::RHISetRenderTargets(
	uint32 NumSimultaneousRenderTargets,
	const FRHIRenderTargetView* NewRenderTargetsRHI,
	const FRHIDepthRenderTargetView* NewDepthStencilTargetRHI,
	uint32 NumUAVs,
	const FUnorderedAccessViewRHIParamRef* UAVs
	)
{
	VERIFY_GL_SCOPE();
	check(NumSimultaneousRenderTargets <= MaxSimultaneousRenderTargets);
	check(NumUAVs == 0);

	FMemory::Memset(PendingState.RenderTargets,0,sizeof(PendingState.RenderTargets));
	FMemory::Memset(PendingState.RenderTargetMipmapLevels,0,sizeof(PendingState.RenderTargetMipmapLevels));
	FMemory::Memset(PendingState.RenderTargetArrayIndex,0,sizeof(PendingState.RenderTargetArrayIndex));
	PendingState.FirstNonzeroRenderTarget = -1;
	
	FShaderCache::SetRenderTargets(FShaderCache::GetDefaultCacheState(), NumSimultaneousRenderTargets, NewRenderTargetsRHI, NewDepthStencilTargetRHI);

	for( int32 RenderTargetIndex = NumSimultaneousRenderTargets - 1; RenderTargetIndex >= 0; --RenderTargetIndex )
	{
		PendingState.RenderTargets[RenderTargetIndex] = GetOpenGLTextureFromRHITexture(NewRenderTargetsRHI[RenderTargetIndex].Texture);
		PendingState.RenderTargetMipmapLevels[RenderTargetIndex] = NewRenderTargetsRHI[RenderTargetIndex].MipIndex;
		PendingState.RenderTargetArrayIndex[RenderTargetIndex] = NewRenderTargetsRHI[RenderTargetIndex].ArraySliceIndex;

		if( PendingState.RenderTargets[RenderTargetIndex] )
		{
			PendingState.FirstNonzeroRenderTarget = (int32)RenderTargetIndex;
		}
	}

	FOpenGLTextureBase* NewDepthStencilRT = GetOpenGLTextureFromRHITexture(NewDepthStencilTargetRHI ? NewDepthStencilTargetRHI->Texture : nullptr);

	if (IsES2Platform(GMaxRHIShaderPlatform) && !IsPCPlatform(GMaxRHIShaderPlatform))
	{
		// @todo-mobile

		FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
		GLuint NewColorRTResource = PendingState.RenderTargets[0] ? PendingState.RenderTargets[0]->Resource : 0;
		GLenum NewColorTargetType = PendingState.RenderTargets[0] ? PendingState.RenderTargets[0]->Target : 0;
		// If the color buffer did not change and we are disabling depth, do not switch depth and assume
		// the high level will disable depth test/write (so we can avoid a logical buffer store);
		// if both are set to nothing, then it's an endframe so we don't want to switch either...
		if (NewDepthStencilRT == NULL && PendingState.DepthStencil != NULL)
		{
			const bool bColorBufferUnchanged = ContextState.LastES2ColorRTResource == NewColorRTResource && ContextState.LastES2ColorTargetType == NewColorTargetType;
#if PLATFORM_ANDROID
			//color RT being 0 means backbuffer is being used. Hence taking only comparison with previous RT into consideration. Fixes black screen issue.
			if (bColorBufferUnchanged)
#else
			if (NewColorRTResource == 0 || bColorBufferUnchanged)
#endif
			{
				return;
			}
			else
			{
				ContextState.LastES2ColorRTResource = NewColorRTResource;
				ContextState.LastES2ColorTargetType = NewColorTargetType;
			}
		}
		else
		{
				ContextState.LastES2ColorRTResource = NewColorRTResource;
				ContextState.LastES2ColorTargetType = NewColorTargetType;
		}
	}
	
	PendingState.DepthStencil = NewDepthStencilRT;
	PendingState.StencilStoreAction = NewDepthStencilTargetRHI ? NewDepthStencilTargetRHI->GetStencilStoreAction() : ERenderTargetStoreAction::ENoAction;
	PendingState.DepthTargetWidth = NewDepthStencilTargetRHI ? GetOpenGLTextureSizeXFromRHITexture(NewDepthStencilTargetRHI->Texture) : 0u;
	PendingState.DepthTargetHeight = NewDepthStencilTargetRHI ? GetOpenGLTextureSizeYFromRHITexture(NewDepthStencilTargetRHI->Texture) : 0u;
	
	if (PendingState.FirstNonzeroRenderTarget == -1 && !PendingState.DepthStencil)
	{
		// Special case - invalid setup, but sometimes performed by the engine

		PendingState.Framebuffer = 0;
		PendingState.bFramebufferSetupInvalid = true;
		return;
	}

	PendingState.Framebuffer = GetOpenGLFramebuffer(NumSimultaneousRenderTargets, PendingState.RenderTargets, PendingState.RenderTargetArrayIndex, PendingState.RenderTargetMipmapLevels, PendingState.DepthStencil);
	PendingState.bFramebufferSetupInvalid = false;

	if (PendingState.FirstNonzeroRenderTarget != -1)
	{
		// Set viewport size to new render target size.
		PendingState.Viewport.Min.X = 0;
		PendingState.Viewport.Min.Y = 0;

		uint32 Width = 0;
		uint32 Height = 0;

		FOpenGLTexture2D* NewRenderTarget2D = (FOpenGLTexture2D*)NewRenderTargetsRHI[PendingState.FirstNonzeroRenderTarget].Texture->GetTexture2D();
		if(NewRenderTarget2D)
		{
			Width = NewRenderTarget2D->GetSizeX();
			Height = NewRenderTarget2D->GetSizeY();
		}
		else
		{
			FOpenGLTextureCube* NewRenderTargetCube = (FOpenGLTextureCube*)NewRenderTargetsRHI[PendingState.FirstNonzeroRenderTarget].Texture->GetTextureCube();
			if(NewRenderTargetCube)
			{
				Width = NewRenderTargetCube->GetSize();
				Height = NewRenderTargetCube->GetSize();
			}
			else
			{
				FOpenGLTexture3D* NewRenderTarget3D = (FOpenGLTexture3D*)NewRenderTargetsRHI[PendingState.FirstNonzeroRenderTarget].Texture->GetTexture3D();
				if(NewRenderTarget3D)
				{
					Width = NewRenderTarget3D->GetSizeX();
					Height = NewRenderTarget3D->GetSizeY();
				}
				else
				{
					FOpenGLTexture2DArray* NewRenderTarget2DArray = (FOpenGLTexture2DArray*)NewRenderTargetsRHI[PendingState.FirstNonzeroRenderTarget].Texture->GetTexture2DArray();
					if(NewRenderTarget2DArray)
					{
						Width = NewRenderTarget2DArray->GetSizeX();
						Height = NewRenderTarget2DArray->GetSizeY();
					}
					else
					{
						check(0);
					}
				}
			}
		}

		{
			uint32 MipIndex = NewRenderTargetsRHI[PendingState.FirstNonzeroRenderTarget].MipIndex;
			Width = FMath::Max<uint32>(1,(Width >> MipIndex));
			Height = FMath::Max<uint32>(1,(Height >> MipIndex));
		}

		PendingState.Viewport.Max.X = PendingState.RenderTargetWidth = Width;
		PendingState.Viewport.Max.Y = PendingState.RenderTargetHeight = Height;
	}
	else if( NewDepthStencilTargetRHI )
	{
		// Set viewport size to new depth target size.
		PendingState.Viewport.Min.X = 0;
		PendingState.Viewport.Min.Y = 0;
		PendingState.Viewport.Max.X = GetOpenGLTextureSizeXFromRHITexture(NewDepthStencilTargetRHI->Texture);
		PendingState.Viewport.Max.Y = GetOpenGLTextureSizeYFromRHITexture(NewDepthStencilTargetRHI->Texture);
	}
}

void FOpenGLDynamicRHI::RHIDiscardRenderTargets(bool Depth, bool Stencil, uint32 ColorBitMask)
{
	if (FOpenGL::SupportsDiscardFrameBuffer())
	{
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_DiscardRenderTargets_Flush);
			FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		}

		// 8 Color + Depth + Stencil = 10
		GLenum Attachments[MaxSimultaneousRenderTargets + 2];
		uint32 I=0;
		if(Depth) 
		{
			Attachments[I] = GL_DEPTH_ATTACHMENT;
			I++;
		}
		if(Stencil) 
		{
			Attachments[I] = GL_STENCIL_ATTACHMENT;
			I++;
		}

		ColorBitMask &= (1 << MaxSimultaneousRenderTargets) - 1;
		uint32 J = 0;
		while (ColorBitMask)
		{
			if(ColorBitMask & 1)
			{
				Attachments[I] = GL_COLOR_ATTACHMENT0 + J;
				I++;
			}

			ColorBitMask >>= 1;
			++J;
		}
		FOpenGL::DiscardFramebufferEXT(GL_FRAMEBUFFER, I, Attachments);
	}
}

void FOpenGLDynamicRHI::RHISetRenderTargetsAndClear(const FRHISetRenderTargetsInfo& RenderTargetsInfo)
{
	this->RHISetRenderTargets(RenderTargetsInfo.NumColorRenderTargets,
		RenderTargetsInfo.ColorRenderTarget,
		&RenderTargetsInfo.DepthStencilRenderTarget,
		0,
		nullptr);
	if (RenderTargetsInfo.bClearColor || RenderTargetsInfo.bClearStencil || RenderTargetsInfo.bClearDepth)
	{
		FLinearColor ClearColors[MaxSimultaneousRenderTargets];
		float DepthClear = 0.0;
		uint32 StencilClear = 0;

		if (RenderTargetsInfo.bClearColor)
		{
			for (int32 i = 0; i < RenderTargetsInfo.NumColorRenderTargets; ++i)
			{
				if (RenderTargetsInfo.ColorRenderTarget[i].Texture != nullptr)
				{
					const FClearValueBinding& ClearValue = RenderTargetsInfo.ColorRenderTarget[i].Texture->GetClearBinding();
					checkf(ClearValue.ColorBinding == EClearBinding::EColorBound, TEXT("Texture: %s does not have a color bound for fast clears"), *RenderTargetsInfo.ColorRenderTarget[i].Texture->GetName().GetPlainNameString());
					ClearColors[i] = ClearValue.GetClearColor();
				}
			}
		}
		if (RenderTargetsInfo.bClearDepth || RenderTargetsInfo.bClearStencil)
		{
			const FClearValueBinding& ClearValue = RenderTargetsInfo.DepthStencilRenderTarget.Texture->GetClearBinding();
			checkf(ClearValue.ColorBinding == EClearBinding::EDepthStencilBound, TEXT("Texture: %s does not have a DS value bound for fast clears"), *RenderTargetsInfo.DepthStencilRenderTarget.Texture->GetName().GetPlainNameString());
			ClearValue.GetDepthStencil(DepthClear, StencilClear);
		}

		this->RHIClearMRT(RenderTargetsInfo.bClearColor, RenderTargetsInfo.NumColorRenderTargets, ClearColors, RenderTargetsInfo.bClearDepth, DepthClear, RenderTargetsInfo.bClearStencil, StencilClear);
	}
}

// Primitive drawing.

void FOpenGLDynamicRHI::EnableVertexElementCached(
	FOpenGLContextState& ContextState,
	const FOpenGLVertexElement &VertexElement,
	GLsizei Stride,
	void *Pointer,
	GLuint Buffer)
{
	VERIFY_GL_SCOPE();

	check( !(FOpenGL::SupportsVertexAttribBinding() && OpenGLConsoleVariables::bUseVAB));

	GLuint AttributeIndex = VertexElement.AttributeIndex;
	AttributeIndex = RemapVertexAttrib(AttributeIndex);
	FOpenGLCachedAttr &Attr = ContextState.VertexAttrs[AttributeIndex];

	if (!Attr.bEnabled)
	{
		glEnableVertexAttribArray(AttributeIndex);
		Attr.bEnabled = true;
	}

	if (
		(Attr.Pointer != Pointer) ||
		(Attr.Buffer != Buffer) ||
		(Attr.Size != VertexElement.Size) ||
		(Attr.Divisor != VertexElement.Divisor) ||
		(Attr.Type != VertexElement.Type) ||
		(Attr.bNormalized != VertexElement.bNormalized) ||
		(Attr.Stride != Stride))
	{
		CachedBindArrayBuffer(ContextState, Buffer);
		if( !VertexElement.bShouldConvertToFloat )
		{
			FOpenGL::VertexAttribIPointer(
				AttributeIndex,
				VertexElement.Size,
				VertexElement.Type,
				Stride,
				Pointer
				);
		}
		else
		{
			FOpenGL::VertexAttribPointer(
				AttributeIndex,
				VertexElement.Size,
				VertexElement.Type,
				VertexElement.bNormalized,
				Stride,
				Pointer
				);
		}
		FOpenGL::VertexAttribDivisor(AttributeIndex, VertexElement.Divisor);

		Attr.Pointer = Pointer;
		Attr.Buffer = Buffer;
		Attr.Size = VertexElement.Size;
		Attr.Divisor = VertexElement.Divisor;
		Attr.Type = VertexElement.Type;
		Attr.bNormalized = VertexElement.bNormalized;
		Attr.Stride = Stride;
	}
}

void FOpenGLDynamicRHI::EnableVertexElementCachedZeroStride(FOpenGLContextState& ContextState, const FOpenGLVertexElement& VertexElement, uint32 NumVertices, FOpenGLVertexBuffer* ZeroStrideVertexBuffer)
{
	VERIFY_GL_SCOPE();

	GLuint AttributeIndex = VertexElement.AttributeIndex;
	AttributeIndex = RemapVertexAttrib(AttributeIndex);

	FOpenGLCachedAttr &Attr = ContextState.VertexAttrs[AttributeIndex];
	uint32 Stride = ZeroStrideVertexBuffer->GetSize();

	FOpenGLVertexBuffer* ExpandedVertexBuffer = FindExpandedZeroStrideBuffer(ZeroStrideVertexBuffer, Stride, NumVertices, VertexElement);
	EnableVertexElementCached(ContextState, VertexElement, Stride, 0, ExpandedVertexBuffer->Resource);
}

void FOpenGLDynamicRHI::FreeZeroStrideBuffers()
{
	// Forces releasing references to expanded zero stride vertex buffers
	ZeroStrideExpandedBuffersList.Empty();
}

void FOpenGLDynamicRHI::SetupVertexArrays(FOpenGLContextState& ContextState, uint32 BaseVertexIndex, FOpenGLStream* Streams, uint32 NumStreams, uint32 MaxVertices)
{
	SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLVBOSetupTime);
	if (FOpenGL::SupportsVertexAttribBinding() && OpenGLConsoleVariables::bUseVAB)
	{
		SetupVertexArraysVAB(ContextState, BaseVertexIndex, Streams, NumStreams, MaxVertices);
		return;
	}
	VERIFY_GL_SCOPE();
	bool UsedAttributes[NUM_OPENGL_VERTEX_STREAMS] = { 0 };

	check(IsValidRef(PendingState.BoundShaderState));
	check(IsValidRef(PendingState.BoundShaderState->VertexShader));
	FOpenGLVertexDeclaration* VertexDeclaration = PendingState.BoundShaderState->VertexDeclaration;
	for (int32 ElementIndex = 0; ElementIndex < VertexDeclaration->VertexElements.Num(); ElementIndex++)
	{
		FOpenGLVertexElement& VertexElement = VertexDeclaration->VertexElements[ElementIndex];
		uint32 AttributeIndex = VertexElement.AttributeIndex;
		const bool bAttribInUse = (PendingState.BoundShaderState->VertexShader->Bindings.InOutMask & (0x1 << AttributeIndex)) != 0;
		if (!bAttribInUse)
		{
			continue; // skip unused attributes.
		}

		AttributeIndex = RemapVertexAttrib(AttributeIndex);

		if ( VertexElement.StreamIndex < NumStreams)
		{
			FOpenGLStream* Stream = &Streams[VertexElement.StreamIndex];
			uint32 Stride = Stream->Stride;

			if( Stream->VertexBuffer->GetUsage() & BUF_ZeroStride )
			{
				check(Stride == 0);
				check(Stream->Offset == 0);
				check(VertexElement.Offset == 0);
				check(Stream->VertexBuffer->GetZeroStrideBuffer());
				EnableVertexElementCachedZeroStride(
					ContextState,
					VertexElement,
					MaxVertices,
					Stream->VertexBuffer
					);
			}
			else
			{
				check( Stride > 0 );
				EnableVertexElementCached(
					ContextState,
					VertexElement,
					Stride,
					INDEX_TO_VOID(BaseVertexIndex * Stride + Stream->Offset + VertexElement.Offset),
					Stream->VertexBuffer->Resource
					);
			}
			UsedAttributes[AttributeIndex] = true;
		}
		else
		{
			//workaround attributes with no streams
			VERIFY_GL_SCOPE();

			FOpenGLCachedAttr &Attr = ContextState.VertexAttrs[AttributeIndex];

			if (Attr.bEnabled)
			{
				glDisableVertexAttribArray(AttributeIndex);
				Attr.bEnabled = false;
			}

			float data[4] = { 0.0f};

			glVertexAttrib4fv(AttributeIndex, data);
		}
	}

	// Disable remaining vertex arrays
	for (GLuint AttribIndex = 0; AttribIndex < NUM_OPENGL_VERTEX_STREAMS; AttribIndex++)
	{
		if (UsedAttributes[AttribIndex] == false && ContextState.VertexAttrs[AttribIndex].bEnabled)
		{
			glDisableVertexAttribArray(AttribIndex);
			ContextState.VertexAttrs[AttribIndex].bEnabled = false;
		}
	}
}

void FOpenGLDynamicRHI::SetupVertexArraysVAB(FOpenGLContextState& ContextState, uint32 BaseVertexIndex, FOpenGLStream* Streams, uint32 NumStreams, uint32 MaxVertices)
{
	VERIFY_GL_SCOPE();
	bool KnowsDivisor[NUM_OPENGL_VERTEX_STREAMS] = { 0 };
	uint32 Divisor[NUM_OPENGL_VERTEX_STREAMS] = { 0 };
	uint32 LastMaxAttrib = ContextState.MaxActiveAttrib;
	bool UpdateDivisors = false;
	uint32 StreamMask = ContextState.ActiveStreamMask;

	check(IsValidRef(PendingState.BoundShaderState));
	check(IsValidRef(PendingState.BoundShaderState->VertexShader));
	FOpenGLVertexDeclaration* VertexDeclaration = PendingState.BoundShaderState->VertexDeclaration;
	uint32 AttributeMask = PendingState.BoundShaderState->VertexShader->Bindings.InOutMask;
	if (FOpenGL::NeedsVertexAttribRemapTable())
	{
		AttributeMask = PendingState.BoundShaderState->VertexShader->Bindings.VertexRemappedMask;
	}

	if (ContextState.VertexDecl != VertexDeclaration || AttributeMask != ContextState.ActiveAttribMask)
	{
		ContextState.MaxActiveAttrib = 0;
		StreamMask = 0;
		UpdateDivisors = true;

		check(VertexDeclaration->VertexElements.Num() <= 32);

		for (int32 ElementIndex = 0; ElementIndex < VertexDeclaration->VertexElements.Num(); ElementIndex++)
		{
			FOpenGLVertexElement& VertexElement = VertexDeclaration->VertexElements[ElementIndex];
			uint32 AttributeIndex = VertexElement.AttributeIndex;
			const bool bAttribInUse = (PendingState.BoundShaderState->VertexShader->Bindings.InOutMask & (0x1 << AttributeIndex)) != 0;
			if (bAttribInUse)
			{
				AttributeIndex = RemapVertexAttrib(AttributeIndex);
			}

			const uint32 StreamIndex = VertexElement.StreamIndex;

			ContextState.MaxActiveAttrib = FMath::Max( ContextState.MaxActiveAttrib, AttributeIndex);

			//only setup/track attributes actually in use
			FOpenGLCachedAttr &Attr = ContextState.VertexAttrs[AttributeIndex];
			if (bAttribInUse)
			{
				if (VertexElement.StreamIndex < NumStreams)
				{

					// Track the actively used streams, to limit the updates to those in use
					StreamMask |= 0x1 << VertexElement.StreamIndex;

					// Verify that the Divisor is consistent across the stream
					check(!KnowsDivisor[StreamIndex] || Divisor[StreamIndex] == VertexElement.Divisor);
					KnowsDivisor[StreamIndex] = true;
					Divisor[StreamIndex] = VertexElement.Divisor;

					if (
						(Attr.StreamOffset != VertexElement.Offset) ||
						(Attr.Size != VertexElement.Size) ||
						(Attr.Type != VertexElement.Type) ||
						(Attr.bNormalized != VertexElement.bNormalized))
					{
						if (!VertexElement.bShouldConvertToFloat)
						{
							FOpenGL::VertexAttribIFormat(AttributeIndex, VertexElement.Size, VertexElement.Type, VertexElement.Offset);
						}
						else
						{
							FOpenGL::VertexAttribFormat(AttributeIndex, VertexElement.Size, VertexElement.Type, VertexElement.bNormalized, VertexElement.Offset);
						}

						Attr.StreamOffset = VertexElement.Offset;
						Attr.Size = VertexElement.Size;
						Attr.Type = VertexElement.Type;
						Attr.bNormalized = VertexElement.bNormalized;
					}

					if (Attr.StreamIndex != StreamIndex)
					{
						FOpenGL::VertexAttribBinding(AttributeIndex, VertexElement.StreamIndex);
						Attr.StreamIndex = StreamIndex;
					}
				}
				else
				{
					// bogus stream, make sure current value is zero to match D3D
					static float data[4] = { 0.0f };

					glVertexAttrib4fv(AttributeIndex, data);

					//Kill this attribute to make sure it isn't enabled
					AttributeMask &= ~(1 << AttributeIndex);
				}
			}
			else
			{
				if (Attr.StreamIndex != StreamIndex)
				{
					FOpenGL::VertexAttribBinding(AttributeIndex, VertexElement.StreamIndex);
					Attr.StreamIndex = StreamIndex;
				}
			}
		}
		ContextState.VertexDecl = VertexDeclaration;

		//Update the stream mask
		ContextState.ActiveStreamMask = StreamMask;
	}

	//setup streams
	for (uint32 StreamIndex = 0; StreamIndex < NumStreams; StreamIndex++, StreamMask >>= 1)
	{
		FOpenGLStream &CachedStream = ContextState.VertexStreams[StreamIndex];
		FOpenGLStream &Stream = Streams[StreamIndex];
		uint32 Offset = BaseVertexIndex * Stream.Stride + Stream.Offset;
		if ((StreamMask & 0x1) != 0 && Stream.VertexBuffer)
		{
			if ( CachedStream.VertexBuffer != Stream.VertexBuffer || CachedStream.Offset != Offset || CachedStream.Stride != Stream.Stride)
			{
				check(Stream.VertexBuffer->Resource != 0);
				FOpenGL::BindVertexBuffer( StreamIndex, Stream.VertexBuffer->Resource, Offset, Stream.Stride);
				CachedStream.VertexBuffer = Stream.VertexBuffer;
				CachedStream.Offset = Offset;
				CachedStream.Stride = Stream.Stride;
			}
			if (UpdateDivisors && CachedStream.Divisor != Divisor[StreamIndex])
			{
				FOpenGL::VertexBindingDivisor( StreamIndex, Divisor[StreamIndex]);
				CachedStream.Divisor = Divisor[StreamIndex];
			}
		}
		else
		{
			if (((StreamMask & 0x1) != 0) && (Stream.VertexBuffer == nullptr))
			{
				UE_LOG(LogRHI, Error, TEXT("Stream %d marked as in use, but vertex buffer provided is NULL (Mask = %x)"), StreamIndex, StreamMask);
			}
			if (CachedStream.VertexBuffer != Stream.VertexBuffer || CachedStream.Offset != Offset || CachedStream.Stride != Stream.Stride)
			{
				FOpenGL::BindVertexBuffer(StreamIndex, 0, 0, 0);
				CachedStream.VertexBuffer = nullptr;
				CachedStream.Offset = 0;
				CachedStream.Stride = 0;
			}
		}
	}

	//Ensure that all requested streams were set
	check(StreamMask == 0);

	// Set the enable/disable state on the arrays
	uint32 MaskDif = ContextState.ActiveAttribMask ^ AttributeMask;
	if (MaskDif)
	{
		ContextState.ActiveAttribMask = AttributeMask;
		uint32 MaxAttrib = FMath::Max( ContextState.MaxActiveAttrib, LastMaxAttrib);
	
		for (GLuint AttribIndex = 0; AttribIndex < NUM_OPENGL_VERTEX_STREAMS && AttribIndex <= MaxAttrib && MaskDif; AttribIndex++)
		{
			if ( MaskDif & 0x1)
			{
				if ( AttributeMask & 0x1)
				{
					glEnableVertexAttribArray(AttribIndex);
				}
				else
				{
					glDisableVertexAttribArray(AttribIndex);
				}
			}
			AttributeMask >>= 1;
			MaskDif >>= 1;
		}
		check( MaskDif == 0);
	}
}

// Used by default on ES2 for immediate mode rendering.
void FOpenGLDynamicRHI::SetupVertexArraysUP(FOpenGLContextState& ContextState, void* Buffer, uint32 Stride)
{
	VERIFY_GL_SCOPE();
	bool UsedAttributes[NUM_OPENGL_VERTEX_STREAMS] = { 0 };

	check(IsValidRef(PendingState.BoundShaderState));
	check(IsValidRef(PendingState.BoundShaderState->VertexShader));
	FOpenGLVertexDeclaration* VertexDeclaration = PendingState.BoundShaderState->VertexDeclaration;

	for (int32 ElementIndex = 0; ElementIndex < VertexDeclaration->VertexElements.Num(); ElementIndex++)
	{
		FOpenGLVertexElement &VertexElement = VertexDeclaration->VertexElements[ElementIndex];
		check(VertexElement.StreamIndex < 1);

		uint32 AttributeIndex = VertexElement.AttributeIndex;
		const bool bAttribInUse = (PendingState.BoundShaderState->VertexShader->Bindings.InOutMask & (0x1 << AttributeIndex)) != 0;
		if (bAttribInUse)
		{
			AttributeIndex = RemapVertexAttrib(AttributeIndex);
			check(Stride > 0);
			EnableVertexElementCached(
				ContextState,
				VertexElement,
				Stride,
				(void*)(((char*)Buffer) + VertexElement.Offset),
				0
				);
			UsedAttributes[AttributeIndex] = true;
		}
	}

	// Disable remaining vertex arrays
	for (GLuint AttribIndex = 0; AttribIndex < NUM_OPENGL_VERTEX_STREAMS; AttribIndex++)
	{
		if (UsedAttributes[AttribIndex] == false && ContextState.VertexAttrs[AttribIndex].bEnabled)
		{
			glDisableVertexAttribArray(AttribIndex);
			ContextState.VertexAttrs[AttribIndex].bEnabled = false;
		}
	}
}

void FOpenGLDynamicRHI::OnProgramDeletion( GLint ProgramResource )
{
	if( SharedContextState.Program == ProgramResource )
	{
		SharedContextState.Program = -1;
	}

	if( RenderingContextState.Program == ProgramResource )
	{
		RenderingContextState.Program = -1;
	}
}

void FOpenGLDynamicRHI::OnVertexBufferDeletion( GLuint VertexBufferResource )
{
	if (SharedContextState.ArrayBufferBound == VertexBufferResource)
	{
		SharedContextState.ArrayBufferBound = -1;	// will force refresh
	}

	if (RenderingContextState.ArrayBufferBound == VertexBufferResource)
	{
		RenderingContextState.ArrayBufferBound = -1;	// will force refresh
	}

	for (GLuint AttribIndex = 0; AttribIndex < NUM_OPENGL_VERTEX_STREAMS; AttribIndex++)
	{
		if( SharedContextState.VertexAttrs[AttribIndex].Buffer == VertexBufferResource )
		{
			SharedContextState.VertexAttrs[AttribIndex].Pointer = FOpenGLCachedAttr_Invalid;	// that'll enforce state update on next cache test
		}

		if( RenderingContextState.VertexAttrs[AttribIndex].Buffer == VertexBufferResource )
		{
			RenderingContextState.VertexAttrs[AttribIndex].Pointer = FOpenGLCachedAttr_Invalid;	// that'll enforce state update on next cache test
		}
	}

	for (GLuint StreamIndex = 0; StreamIndex < NUM_OPENGL_VERTEX_STREAMS; StreamIndex++)
	{
		if (SharedContextState.VertexStreams[StreamIndex].VertexBuffer != nullptr && SharedContextState.VertexStreams[StreamIndex].VertexBuffer->Resource == VertexBufferResource)
		{
			FOpenGL::BindVertexBuffer(StreamIndex, 0, 0, 0); // brianh@nvidia: work around driver bug 1809000
			SharedContextState.VertexStreams[StreamIndex].VertexBuffer = nullptr;
		}

		if (RenderingContextState.VertexStreams[StreamIndex].VertexBuffer != nullptr && RenderingContextState.VertexStreams[StreamIndex].VertexBuffer->Resource == VertexBufferResource)
		{
			FOpenGL::BindVertexBuffer(StreamIndex, 0, 0, 0); // brianh@nvidia: work around driver bug 1809000
			RenderingContextState.VertexStreams[StreamIndex].VertexBuffer = nullptr;
		}
	}
}

void FOpenGLDynamicRHI::OnIndexBufferDeletion( GLuint IndexBufferResource )
{
	if (SharedContextState.ElementArrayBufferBound == IndexBufferResource)
	{
		SharedContextState.ElementArrayBufferBound = -1;	// will force refresh
	}

	if (RenderingContextState.ElementArrayBufferBound == IndexBufferResource)
	{
		RenderingContextState.ElementArrayBufferBound = -1;	// will force refresh
	}
}

void FOpenGLDynamicRHI::OnPixelBufferDeletion( GLuint PixelBufferResource )
{
	if (SharedContextState.PixelUnpackBufferBound == PixelBufferResource)
	{
		SharedContextState.PixelUnpackBufferBound = -1;	// will force refresh
	}

	if (RenderingContextState.PixelUnpackBufferBound == PixelBufferResource)
	{
		RenderingContextState.PixelUnpackBufferBound = -1;	// will force refresh
	}
}

void FOpenGLDynamicRHI::OnUniformBufferDeletion( GLuint UniformBufferResource, uint32 AllocatedSize, bool bStreamDraw )
{
	if (SharedContextState.UniformBufferBound == UniformBufferResource)
	{
		SharedContextState.UniformBufferBound = -1;	// will force refresh
	}

	if (RenderingContextState.UniformBufferBound == UniformBufferResource)
	{
		RenderingContextState.UniformBufferBound = -1;	// will force refresh
	}

	for (GLuint UniformBufferIndex = 0; UniformBufferIndex < CrossCompiler::NUM_SHADER_STAGES*OGL_MAX_UNIFORM_BUFFER_BINDINGS; UniformBufferIndex++)
	{
		if( SharedContextState.UniformBuffers[UniformBufferIndex] == UniformBufferResource )
		{
			SharedContextState.UniformBuffers[UniformBufferIndex] = FOpenGLCachedUniformBuffer_Invalid;	// that'll enforce state update on next cache test
		}

		if( RenderingContextState.UniformBuffers[UniformBufferIndex] == UniformBufferResource )
		{
			RenderingContextState.UniformBuffers[UniformBufferIndex] = FOpenGLCachedUniformBuffer_Invalid;	// that'll enforce state update on next cache test
		}
	}
}

void FOpenGLDynamicRHI::CommitNonComputeShaderConstants()
{
	VERIFY_GL_SCOPE();

	FOpenGLLinkedProgram* LinkedProgram = PendingState.BoundShaderState->LinkedProgram;
	if (GUseEmulatedUniformBuffers)
	{
		PendingState.ShaderParameters[CrossCompiler::SHADER_STAGE_VERTEX].CommitPackedUniformBuffers(LinkedProgram, CrossCompiler::SHADER_STAGE_VERTEX, PendingState.BoundUniformBuffers[SF_Vertex], PendingState.BoundShaderState->VertexShader->UniformBuffersCopyInfo);
	}
	PendingState.ShaderParameters[CrossCompiler::SHADER_STAGE_VERTEX].CommitPackedGlobals(LinkedProgram, CrossCompiler::SHADER_STAGE_VERTEX);

	if (GUseEmulatedUniformBuffers)
	{
		PendingState.ShaderParameters[CrossCompiler::SHADER_STAGE_PIXEL].CommitPackedUniformBuffers(LinkedProgram, CrossCompiler::SHADER_STAGE_PIXEL, PendingState.BoundUniformBuffers[SF_Pixel], PendingState.BoundShaderState->PixelShader->UniformBuffersCopyInfo);
	}
	PendingState.ShaderParameters[CrossCompiler::SHADER_STAGE_PIXEL].CommitPackedGlobals(LinkedProgram, CrossCompiler::SHADER_STAGE_PIXEL);

	if (PendingState.BoundShaderState->GeometryShader)
	{
		if (GUseEmulatedUniformBuffers)
		{
			PendingState.ShaderParameters[CrossCompiler::SHADER_STAGE_GEOMETRY].CommitPackedUniformBuffers(LinkedProgram, CrossCompiler::SHADER_STAGE_GEOMETRY, PendingState.BoundUniformBuffers[SF_Geometry], PendingState.BoundShaderState->GeometryShader->UniformBuffersCopyInfo);
		}
		PendingState.ShaderParameters[CrossCompiler::SHADER_STAGE_GEOMETRY].CommitPackedGlobals(LinkedProgram, CrossCompiler::SHADER_STAGE_GEOMETRY);
	}
}

void FOpenGLDynamicRHI::CommitComputeShaderConstants(FComputeShaderRHIParamRef ComputeShaderRHI)
{
	VERIFY_GL_SCOPE();
	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);

	FOpenGLComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);
	const int32 Stage = CrossCompiler::SHADER_STAGE_COMPUTE;

	FOpenGLShaderParameterCache& StageShaderParameters = PendingState.ShaderParameters[CrossCompiler::SHADER_STAGE_COMPUTE];
	StageShaderParameters.CommitPackedGlobals(ComputeShader->LinkedProgram, CrossCompiler::SHADER_STAGE_COMPUTE);
}

template <EShaderFrequency Frequency>
uint32 GetFirstTextureUnit();

template <> FORCEINLINE uint32 GetFirstTextureUnit<SF_Vertex>() { return FOpenGL::GetFirstVertexTextureUnit(); }
template <> FORCEINLINE uint32 GetFirstTextureUnit<SF_Hull>() { return FOpenGL::GetFirstHullTextureUnit(); }
template <> FORCEINLINE uint32 GetFirstTextureUnit<SF_Domain>() { return FOpenGL::GetFirstDomainTextureUnit(); }
template <> FORCEINLINE uint32 GetFirstTextureUnit<SF_Pixel>() { return FOpenGL::GetFirstPixelTextureUnit(); }
template <> FORCEINLINE uint32 GetFirstTextureUnit<SF_Geometry>() { return FOpenGL::GetFirstGeometryTextureUnit(); }
template <> FORCEINLINE uint32 GetFirstTextureUnit<SF_Compute>() { return FOpenGL::GetFirstComputeTextureUnit(); }

template <EShaderFrequency Frequency>
FORCEINLINE void SetResource(FOpenGLDynamicRHI* RESTRICT OpenGLRHI, uint32 BindIndex, FRHITexture* RESTRICT TextureRHI, float CurrentTime)
{
	FOpenGLTextureBase* Texture = GetOpenGLTextureFromRHITexture(TextureRHI);
	if (Texture)
	{
		TextureRHI->SetLastRenderTime(CurrentTime);
		OpenGLRHI->InternalSetShaderTexture(Texture, nullptr, GetFirstTextureUnit<Frequency>() + BindIndex, Texture->Target, Texture->Resource, Texture->NumMips, -1);
	}
	else
	{
		OpenGLRHI->InternalSetShaderTexture(Texture, nullptr, GetFirstTextureUnit<Frequency>() + BindIndex, 0, 0, 0, -1);
	}
	
	FShaderCache::SetTexture(FShaderCache::GetDefaultCacheState(), Frequency, BindIndex, (FTextureRHIParamRef)TextureRHI);
}

template <EShaderFrequency Frequency>
FORCEINLINE void SetResource(FOpenGLDynamicRHI* RESTRICT OpenGLRHI, uint32 BindIndex, FOpenGLSamplerState* RESTRICT SamplerState, float CurrentTime)
{
	if (FOpenGL::SupportsSamplerObjects())
	{
		PTRINT SamplerStateAsInt = (PTRINT)SamplerState->Resource;
		FOpenGL::BindSampler(GetFirstTextureUnit<Frequency>() + BindIndex, (GLuint)SamplerStateAsInt);
	}
	else
	{
		OpenGLRHI->InternalSetSamplerStates(GetFirstTextureUnit<Frequency>() + BindIndex, SamplerState);
	}
	
	FShaderCache::SetSamplerState(FShaderCache::GetDefaultCacheState(), Frequency, BindIndex, (FSamplerStateRHIParamRef)SamplerState);
}

template <EShaderFrequency Frequency>
FORCEINLINE void SetResource(FOpenGLDynamicRHI* RESTRICT OpenGLRHI, uint32 BindIndex, FOpenGLShaderResourceView* RESTRICT SRV, float CurrentTime)
{
	OpenGLRHI->InternalSetShaderTexture(NULL, SRV, GetFirstTextureUnit<Frequency>() + BindIndex, SRV->Target, SRV->Resource, 0, SRV->LimitMip);
	SetResource<Frequency>(OpenGLRHI,BindIndex,OpenGLRHI->GetPointSamplerState(), CurrentTime);
	
	FShaderCache::SetSRV(FShaderCache::GetDefaultCacheState(), Frequency, BindIndex, SRV);
}

template <class GLResourceType, EShaderFrequency ShaderFrequency>
inline int32 SetShaderResourcesFromBuffer(FOpenGLDynamicRHI* RESTRICT OpenGLRHI, FOpenGLUniformBuffer* RESTRICT Buffer, const uint32* RESTRICT ResourceMap, int32 BufferIndex)
{
	const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
	float CurrentTime = FApp::GetCurrentTime();
	int32 NumSetCalls = 0;
	uint32 BufferOffset = ResourceMap[BufferIndex];
	if (BufferOffset > 0)
	{
		const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
		uint32 ResourceInfo = *ResourceInfos++;
		do 
		{
			checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
			const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

			GLResourceType* ResourcePtr = (GLResourceType*)Resources[ResourceIndex].GetReference();
			SetResource<ShaderFrequency>(OpenGLRHI, BindIndex, ResourcePtr, CurrentTime);

			NumSetCalls++;
			ResourceInfo = *ResourceInfos++;
		} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
	}
	return NumSetCalls;
}

template <class ShaderType>
void FOpenGLDynamicRHI::SetResourcesFromTables(const ShaderType* RESTRICT Shader)
{
	checkSlow(Shader);
	const FOpenGLShaderResourceTable* RESTRICT SRT = &Shader->Bindings.ShaderResourceTable;

	// Mask the dirty bits by those buffers from which the shader has bound resources.
	uint32 DirtyBits = SRT->ResourceTableBits & PendingState.DirtyUniformBuffers[ShaderType::StaticFrequency];
	uint32 NumSetCalls = 0;
	while (DirtyBits)
	{
		// Scan for the lowest set bit, compute its index, clear it in the set of dirty bits.
		const uint32 LowestBitMask = (DirtyBits) & (-(int32)DirtyBits);
		const int32 BufferIndex = FMath::FloorLog2(LowestBitMask); // todo: This has a branch on zero, we know it could never be zero...
		DirtyBits ^= LowestBitMask;

		FOpenGLUniformBuffer* Buffer = (FOpenGLUniformBuffer*)PendingState.BoundUniformBuffers[ShaderType::StaticFrequency][BufferIndex].GetReference();
		if(!FShaderCache::IsPredrawCall(FShaderCache::GetDefaultCacheState()))
		{
			check(Buffer);
			check(BufferIndex < SRT->ResourceTableLayoutHashes.Num());
			check(Buffer->GetLayout().GetHash() == SRT->ResourceTableLayoutHashes[BufferIndex]);

			// todo: could make this two pass: gather then set
			SetShaderResourcesFromBuffer<FRHITexture,(EShaderFrequency)ShaderType::StaticFrequency>(this,Buffer,SRT->TextureMap.GetData(),BufferIndex);
			SetShaderResourcesFromBuffer<FOpenGLShaderResourceView,(EShaderFrequency)ShaderType::StaticFrequency>(this,Buffer,SRT->ShaderResourceViewMap.GetData(),BufferIndex);
			SetShaderResourcesFromBuffer<FOpenGLSamplerState,(EShaderFrequency)ShaderType::StaticFrequency>(this,Buffer,SRT->SamplerMap.GetData(),BufferIndex);
		}
	}
	PendingState.DirtyUniformBuffers[ShaderType::StaticFrequency] = 0;
	//SetTextureInTableCalls += NumSetCalls;
}

void FOpenGLDynamicRHI::CommitGraphicsResourceTables()
{
	if (auto* Shader = PendingState.BoundShaderState->VertexShader.GetReference())
	{
		SetResourcesFromTables(Shader);
	}
	if (auto* Shader = PendingState.BoundShaderState->PixelShader.GetReference())
	{
		SetResourcesFromTables(Shader);
	}
	if (auto* Shader = PendingState.BoundShaderState->HullShader.GetReference())
	{
		SetResourcesFromTables(Shader);
	}
	if (auto* Shader = PendingState.BoundShaderState->DomainShader.GetReference())
	{
		SetResourcesFromTables(Shader);
	}
	if (auto* Shader = PendingState.BoundShaderState->GeometryShader.GetReference())
	{
		SetResourcesFromTables(Shader);
	}
}

void FOpenGLDynamicRHI::CommitComputeResourceTables(FOpenGLComputeShader* ComputeShader)
{
	check(ComputeShader);
	SetResourcesFromTables(ComputeShader);
}

#if DEBUG_GL_SHADERS
static void VerifyProgramPipeline()
{
	if (FOpenGL::SupportsSeparateShaderObjects())
	{
		GLint ProgramPipeline = 0;
		glGetIntegerv(GL_PROGRAM_PIPELINE_BINDING, &ProgramPipeline);
		if(ProgramPipeline)
		{
			FOpenGL::ValidateProgramPipeline(ProgramPipeline);
			GLint LinkStatus = GL_FALSE;
			FOpenGL::GetProgramPipelineiv(ProgramPipeline, GL_VALIDATE_STATUS, &LinkStatus);
			if(LinkStatus == GL_FALSE)
			{
				GLint LogLength = 0;
				FOpenGL::GetProgramPipelineiv(ProgramPipeline, GL_INFO_LOG_LENGTH, &LogLength);
				ANSICHAR DefaultLog[] = "No log";
				ANSICHAR *CompileLog = DefaultLog;
				if (LogLength > 1)
				{
					CompileLog = (ANSICHAR *)FMemory::Malloc(LogLength);
					FOpenGL::GetProgramPipelineInfoLog(ProgramPipeline, LogLength, NULL, CompileLog);
				}
				
				UE_LOG(LogRHI,Error,TEXT("Failed to validate pipeline %d. Compile log:\n%s"), ProgramPipeline,
					   ANSI_TO_TCHAR(CompileLog));
				
				if (LogLength > 1)
				{
					FMemory::Free(CompileLog);
				}
			}
		}
	}
}
#endif

void FOpenGLDynamicRHI::RHIDrawPrimitive(uint32 PrimitiveType,uint32 BaseVertexIndex,uint32 NumPrimitives,uint32 NumInstances)
{
	SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLDrawPrimitiveTime);
	VERIFY_GL_SCOPE();
	RHI_DRAW_CALL_STATS(PrimitiveType,NumPrimitives*NumInstances);

	FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
	BindPendingFramebuffer(ContextState);
	SetPendingBlendStateForActiveRenderTargets(ContextState);
	UpdateViewportInOpenGLContext(ContextState);
	UpdateScissorRectInOpenGLContext(ContextState);
	UpdateRasterizerStateInOpenGLContext(ContextState);
	UpdateDepthStencilStateInOpenGLContext(ContextState);
	BindPendingShaderState(ContextState);
	CommitGraphicsResourceTables();
	SetupTexturesForDraw(ContextState);
	CommitNonComputeShaderConstants();
	CachedBindElementArrayBuffer(ContextState,0);
	uint32 VertexCount = GetVertexCountForPrimitiveCount(NumPrimitives,PrimitiveType);
	SetupVertexArrays(ContextState, BaseVertexIndex, PendingState.Streams, NUM_OPENGL_VERTEX_STREAMS, VertexCount);

	GLenum DrawMode = GL_TRIANGLES;
	GLsizei NumElements = 0;
	GLint PatchSize = 0;
	FindPrimitiveType(PrimitiveType, ContextState.bUsingTessellation, NumPrimitives, DrawMode, NumElements, PatchSize);

	if (FOpenGL::SupportsTessellation() && DrawMode == GL_PATCHES )
	{
		FOpenGL::PatchParameteri(GL_PATCH_VERTICES, PatchSize);
	}
	
#if DEBUG_GL_SHADERS
	VerifyProgramPipeline();
#endif

	GPUProfilingData.RegisterGPUWork(NumPrimitives * NumInstances, VertexCount * NumInstances);
	if (NumInstances == 1)
	{
		SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLDrawPrimitiveDriverTime);
		CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderFirstDrawTime, PendingState.BoundShaderState->RequiresDriverInstantiation());
		glDrawArrays(DrawMode, 0, NumElements);
		REPORT_GL_DRAW_ARRAYS_EVENT_FOR_FRAME_DUMP( DrawMode, 0, NumElements );
	}
	else
	{
		SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLDrawPrimitiveDriverTime);
		CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderFirstDrawTime, PendingState.BoundShaderState->RequiresDriverInstantiation());
		check( FOpenGL::SupportsInstancing() );
		FOpenGL::DrawArraysInstanced(DrawMode, 0, NumElements, NumInstances);
		REPORT_GL_DRAW_ARRAYS_INSTANCED_EVENT_FOR_FRAME_DUMP( DrawMode, 0, NumElements, NumInstances );
	}

	FShaderCache::LogDraw(FShaderCache::GetDefaultCacheState(), PrimitiveType, 0);
}

void FOpenGLDynamicRHI::RHIDrawPrimitiveIndirect(uint32 PrimitiveType,FVertexBufferRHIParamRef ArgumentBufferRHI,uint32 ArgumentOffset)
{
	if (FOpenGL::SupportsDrawIndirect())
	{
		VERIFY_GL_SCOPE();

		check(ArgumentBufferRHI);
	GPUProfilingData.RegisterGPUWork(0);

		FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
		BindPendingFramebuffer(ContextState);
		SetPendingBlendStateForActiveRenderTargets(ContextState);
		UpdateViewportInOpenGLContext(ContextState);
		UpdateScissorRectInOpenGLContext(ContextState);
		UpdateRasterizerStateInOpenGLContext(ContextState);
		UpdateDepthStencilStateInOpenGLContext(ContextState);
		BindPendingShaderState(ContextState);
		SetupTexturesForDraw(ContextState);
		CommitNonComputeShaderConstants();
		CachedBindElementArrayBuffer(ContextState,0);

		// Zero-stride buffer emulation won't work here, need to use VAB with proper zero strides
		SetupVertexArrays(ContextState, 0, PendingState.Streams, NUM_OPENGL_VERTEX_STREAMS, 1);

		GLenum DrawMode = GL_TRIANGLES;
		GLsizei NumElements = 0;
		GLint PatchSize = 0;
		FindPrimitiveType(PrimitiveType, ContextState.bUsingTessellation, 0, DrawMode, NumElements, PatchSize);

		if (FOpenGL::SupportsTessellation() && DrawMode == GL_PATCHES )
		{
			FOpenGL::PatchParameteri(GL_PATCH_VERTICES, PatchSize);
		} 

		FOpenGLVertexBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);


		glBindBuffer( GL_DRAW_INDIRECT_BUFFER, ArgumentBuffer->Resource);
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderFirstDrawTime, PendingState.BoundShaderState->RequiresDriverInstantiation());
			FOpenGL::DrawArraysIndirect( DrawMode, INDEX_TO_VOID(ArgumentOffset));
		}
		glBindBuffer( GL_DRAW_INDIRECT_BUFFER, 0);
		
		FShaderCache::LogDraw(FShaderCache::GetDefaultCacheState(), PrimitiveType, 0);
	}
	else
	{
		UE_LOG(LogRHI, Fatal,TEXT("OpenGL RHI does not yet support indirect draw calls."));
	}

}

void FOpenGLDynamicRHI::RHIDrawIndexedIndirect(FIndexBufferRHIParamRef IndexBufferRHI, uint32 PrimitiveType, FStructuredBufferRHIParamRef ArgumentsBufferRHI, int32 DrawArgumentsIndex, uint32 NumInstances)
{
	if (FOpenGL::SupportsDrawIndirect())
	{
		VERIFY_GL_SCOPE();

		FOpenGLIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
	GPUProfilingData.RegisterGPUWork(1);

		check(ArgumentsBufferRHI);

		//Draw indiect has to have a number of instances
		check(NumInstances > 1);

		FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
		BindPendingFramebuffer(ContextState);
		SetPendingBlendStateForActiveRenderTargets(ContextState);
		UpdateViewportInOpenGLContext(ContextState);
		UpdateScissorRectInOpenGLContext(ContextState);
		UpdateRasterizerStateInOpenGLContext(ContextState);
		UpdateDepthStencilStateInOpenGLContext(ContextState);
		BindPendingShaderState(ContextState);
		SetupTexturesForDraw(ContextState);
		CommitNonComputeShaderConstants();
		CachedBindElementArrayBuffer(ContextState,IndexBuffer->Resource);

		// Zero-stride buffer emulation won't work here, need to use VAB with proper zero strides
		SetupVertexArrays(ContextState, 0, PendingState.Streams, NUM_OPENGL_VERTEX_STREAMS, 1);

		GLenum DrawMode = GL_TRIANGLES;
		GLsizei NumElements = 0;
		GLint PatchSize = 0;
		FindPrimitiveType(PrimitiveType, ContextState.bUsingTessellation, 0, DrawMode, NumElements, PatchSize);

		if (FOpenGL::SupportsTessellation() && DrawMode == GL_PATCHES )
		{
			FOpenGL::PatchParameteri(GL_PATCH_VERTICES, PatchSize);
		} 

		GLenum IndexType = IndexBuffer->GetStride() == sizeof(uint32) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;

		FOpenGLStructuredBuffer* ArgumentsBuffer = ResourceCast(ArgumentsBufferRHI);


		glBindBuffer( GL_DRAW_INDIRECT_BUFFER, ArgumentsBuffer->Resource);
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderFirstDrawTime, PendingState.BoundShaderState->RequiresDriverInstantiation());
	
			// Offset is based on an index into the list of structures
			FOpenGL::DrawElementsIndirect( DrawMode, IndexType, INDEX_TO_VOID(DrawArgumentsIndex * 5 *sizeof(uint32)));
		}
		glBindBuffer( GL_DRAW_INDIRECT_BUFFER, 0);
		
		FShaderCache::LogDraw(FShaderCache::GetDefaultCacheState(), PrimitiveType, IndexBuffer->GetStride());
	}
	else
	{
		UE_LOG(LogRHI, Fatal,TEXT("OpenGL RHI does not yet support indirect draw calls."));
	}
}

void FOpenGLDynamicRHI::RHIDrawIndexedPrimitive(FIndexBufferRHIParamRef IndexBufferRHI,uint32 PrimitiveType,int32 BaseVertexIndex,uint32 FirstInstance,uint32 NumVertices,uint32 StartIndex,uint32 NumPrimitives,uint32 NumInstances)
{
	SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLDrawPrimitiveTime);
	VERIFY_GL_SCOPE();

	FOpenGLIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);

	RHI_DRAW_CALL_STATS(PrimitiveType,NumPrimitives*NumInstances);

	FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
	BindPendingFramebuffer(ContextState);
	SetPendingBlendStateForActiveRenderTargets(ContextState);
	UpdateViewportInOpenGLContext(ContextState);
	UpdateScissorRectInOpenGLContext(ContextState);
	UpdateRasterizerStateInOpenGLContext(ContextState);
	UpdateDepthStencilStateInOpenGLContext(ContextState);
	BindPendingShaderState(ContextState);
	CommitGraphicsResourceTables();
	SetupTexturesForDraw(ContextState);
	CommitNonComputeShaderConstants();
	CachedBindElementArrayBuffer(ContextState,IndexBuffer->Resource);
	SetupVertexArrays(ContextState, BaseVertexIndex, PendingState.Streams, NUM_OPENGL_VERTEX_STREAMS, NumVertices + StartIndex);

	GLenum DrawMode = GL_TRIANGLES;
	GLsizei NumElements = 0;
	GLint PatchSize = 0;
	FindPrimitiveType(PrimitiveType, ContextState.bUsingTessellation, NumPrimitives, DrawMode, NumElements, PatchSize);

	if (FOpenGL::SupportsTessellation() && DrawMode == GL_PATCHES )
	{
		FOpenGL::PatchParameteri(GL_PATCH_VERTICES, PatchSize);
	}

	GLenum IndexType = IndexBuffer->GetStride() == sizeof(uint32) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
	StartIndex *= IndexBuffer->GetStride() == sizeof(uint32) ? sizeof(uint32) : sizeof(uint16);

#if DEBUG_GL_SHADERS
	VerifyProgramPipeline();
#endif

	GPUProfilingData.RegisterGPUWork(NumPrimitives * NumInstances, NumElements * NumInstances);
	if (NumInstances > 1)
	{
		SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLDrawPrimitiveDriverTime);
		CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderFirstDrawTime, PendingState.BoundShaderState->RequiresDriverInstantiation());
		check( FOpenGL::SupportsInstancing() );
		checkf(FirstInstance  == 0, TEXT("FirstInstance is currently unsupported on this RHI"));
		FOpenGL::DrawElementsInstanced(DrawMode, NumElements, IndexType, INDEX_TO_VOID(StartIndex), NumInstances);
		REPORT_GL_DRAW_ELEMENTS_INSTANCED_EVENT_FOR_FRAME_DUMP(DrawMode, NumElements, IndexType, (void *)StartIndex, NumInstances);
	}
	else
	{
		SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLDrawPrimitiveDriverTime);
		CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderFirstDrawTime, PendingState.BoundShaderState->RequiresDriverInstantiation());
		if ( FOpenGL::SupportsDrawIndexOffset() )
		{
			FOpenGL::DrawRangeElements(DrawMode, 0, NumVertices, NumElements, IndexType, INDEX_TO_VOID(StartIndex));
		}
		else
		{
			glDrawElements(DrawMode, NumElements, IndexType, INDEX_TO_VOID(StartIndex));
		}
		REPORT_GL_DRAW_RANGE_ELEMENTS_EVENT_FOR_FRAME_DUMP(DrawMode, MinIndex, MinIndex + NumVertices, NumElements, IndexType, (void *)StartIndex);
	}

	FShaderCache::LogDraw(FShaderCache::GetDefaultCacheState(), PrimitiveType, IndexBuffer->GetStride());
}

void FOpenGLDynamicRHI::RHIDrawIndexedPrimitiveIndirect(uint32 PrimitiveType,FIndexBufferRHIParamRef IndexBufferRHI,FVertexBufferRHIParamRef ArgumentBufferRHI,uint32 ArgumentOffset)
{
	if (FOpenGL::SupportsDrawIndirect())
	{
		VERIFY_GL_SCOPE();

		FOpenGLIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);
		GPUProfilingData.RegisterGPUWork(1);

		check(ArgumentBufferRHI);

		FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
		BindPendingFramebuffer(ContextState);
		SetPendingBlendStateForActiveRenderTargets(ContextState);
		UpdateViewportInOpenGLContext(ContextState);
		UpdateScissorRectInOpenGLContext(ContextState);
		UpdateRasterizerStateInOpenGLContext(ContextState);
		UpdateDepthStencilStateInOpenGLContext(ContextState);
		BindPendingShaderState(ContextState);
		SetupTexturesForDraw(ContextState);
		CommitNonComputeShaderConstants();
		CachedBindElementArrayBuffer(ContextState,IndexBuffer->Resource);

		// @ToDo Zero-stride buffer emulation won't work here, need to use VAB with proper zero strides
		SetupVertexArrays(ContextState, 0, PendingState.Streams, NUM_OPENGL_VERTEX_STREAMS, 1);

		GLenum DrawMode = GL_TRIANGLES;
		GLsizei NumElements = 0;
		GLint PatchSize = 0;
		FindPrimitiveType(PrimitiveType, ContextState.bUsingTessellation, 0, DrawMode, NumElements, PatchSize);

		if (FOpenGL::SupportsTessellation() && DrawMode == GL_PATCHES )
		{
			FOpenGL::PatchParameteri(GL_PATCH_VERTICES, PatchSize);
		} 

		GLenum IndexType = IndexBuffer->GetStride() == sizeof(uint32) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;

		FOpenGLVertexBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);


		glBindBuffer( GL_DRAW_INDIRECT_BUFFER, ArgumentBuffer->Resource);
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderFirstDrawTime, PendingState.BoundShaderState->RequiresDriverInstantiation());
		
			// Offset is based on an index into the list of structures
			FOpenGL::DrawElementsIndirect( DrawMode, IndexType, INDEX_TO_VOID(ArgumentOffset));
		}
		glBindBuffer( GL_DRAW_INDIRECT_BUFFER, 0);
		
		FShaderCache::LogDraw(FShaderCache::GetDefaultCacheState(), PrimitiveType, IndexBuffer->GetStride());
	}
	else
	{
		UE_LOG(LogRHI, Fatal,TEXT("OpenGL RHI does not yet support indirect draw calls."));
	}
}

/**
 * Preallocate memory or get a direct command stream pointer to fill up for immediate rendering . This avoids memcpys below in DrawPrimitiveUP
 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
 * @param NumPrimitives The number of primitives in the VertexData buffer
 * @param NumVertices The number of vertices to be written
 * @param VertexDataStride Size of each vertex 
 * @param OutVertexData Reference to the allocated vertex memory
 */
void FOpenGLDynamicRHI::RHIBeginDrawPrimitiveUP( uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData)
{
	SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLDrawPrimitiveUPTime);
	VERIFY_GL_SCOPE();
	check(PendingState.NumPrimitives == 0);

	if(FOpenGL::SupportsFastBufferData()) 
	{
		OutVertexData = DynamicVertexBuffers.Lock(NumVertices * VertexDataStride);
	}
	else 
	{
		uint32 BytesVertex = NumVertices * VertexDataStride;
		if(BytesVertex > PendingState.UpVertexBufferBytes) 
		{
			if(PendingState.UpVertexBuffer) 
			{
				FMemory::Free(PendingState.UpVertexBuffer);
			}
			PendingState.UpVertexBuffer = FMemory::Malloc(BytesVertex);
			PendingState.UpVertexBufferBytes = BytesVertex;
		}
		OutVertexData = PendingState.UpVertexBuffer;
		PendingState.UpStride = VertexDataStride;
	}

	PendingState.PrimitiveType = PrimitiveType;
	PendingState.NumPrimitives = NumPrimitives;
	PendingState.NumVertices = NumVertices;
	if(FOpenGL::SupportsFastBufferData())
	{
		PendingState.DynamicVertexStream.VertexBuffer = DynamicVertexBuffers.GetPendingBuffer();
		PendingState.DynamicVertexStream.Offset = DynamicVertexBuffers.GetPendingOffset();
		PendingState.DynamicVertexStream.Stride = VertexDataStride;
	}
	else
	{
		PendingState.DynamicVertexStream.VertexBuffer = 0;
		PendingState.DynamicVertexStream.Offset = 0;
		PendingState.DynamicVertexStream.Stride = VertexDataStride;
	}
}

/**
 * Draw a primitive using the vertex data populated since RHIBeginDrawPrimitiveUP and clean up any memory as needed
 */
void FOpenGLDynamicRHI::RHIEndDrawPrimitiveUP()
{
	SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLDrawPrimitiveUPTime);
	VERIFY_GL_SCOPE();
	check(PendingState.NumPrimitives != 0);

	RHI_DRAW_CALL_STATS(PendingState.PrimitiveType,PendingState.NumPrimitives);


	if(FOpenGL::SupportsFastBufferData()) 
	{
		DynamicVertexBuffers.Unlock();
	}

	FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
	BindPendingFramebuffer(ContextState);
	SetPendingBlendStateForActiveRenderTargets(ContextState);
	UpdateViewportInOpenGLContext(ContextState);
	UpdateScissorRectInOpenGLContext(ContextState);
	UpdateRasterizerStateInOpenGLContext(ContextState);
	UpdateDepthStencilStateInOpenGLContext(ContextState);
	BindPendingShaderState(ContextState);
	CommitGraphicsResourceTables();
	SetupTexturesForDraw(ContextState);
	CommitNonComputeShaderConstants();
	CachedBindElementArrayBuffer(ContextState,0);

	GLenum DrawMode = GL_TRIANGLES;
	GLsizei NumElements = 0;
	GLint PatchSize = 0;
	FindPrimitiveType(PendingState.PrimitiveType, ContextState.bUsingTessellation, PendingState.NumPrimitives, DrawMode, NumElements, PatchSize);

	if (FOpenGL::SupportsTessellation() && DrawMode == GL_PATCHES)
	{
		FOpenGL::PatchParameteri(GL_PATCH_VERTICES, PatchSize);
	}

	if(FOpenGL::SupportsFastBufferData()) 
	{
		SetupVertexArrays(ContextState, 0, &PendingState.DynamicVertexStream, 1, PendingState.NumVertices);
	}
	else
	{
		SetupVertexArraysUP(ContextState, PendingState.UpVertexBuffer, PendingState.UpStride);
	}
	
#if DEBUG_GL_SHADERS
	VerifyProgramPipeline();
#endif

	GPUProfilingData.RegisterGPUWork(PendingState.NumPrimitives,PendingState.NumVertices);
	{
		CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderFirstDrawTime, PendingState.BoundShaderState->RequiresDriverInstantiation());
		glDrawArrays(DrawMode, 0, NumElements);
	}
	PendingState.NumPrimitives = 0;

	REPORT_GL_DRAW_ARRAYS_EVENT_FOR_FRAME_DUMP( DrawMode, 0, NumElements );

	FShaderCache::LogDraw(FShaderCache::GetDefaultCacheState(), PendingState.PrimitiveType, 0);
}

/**
 * Preallocate memory or get a direct command stream pointer to fill up for immediate rendering . This avoids memcpys below in DrawIndexedPrimitiveUP
 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
 * @param NumPrimitives The number of primitives in the VertexData buffer
 * @param NumVertices The number of vertices to be written
 * @param VertexDataStride Size of each vertex
 * @param OutVertexData Reference to the allocated vertex memory
 * @param MinVertexIndex The lowest vertex index used by the index buffer
 * @param NumIndices Number of indices to be written
 * @param IndexDataStride Size of each index (either 2 or 4 bytes)
 * @param OutIndexData Reference to the allocated index memory
 */
void FOpenGLDynamicRHI::RHIBeginDrawIndexedPrimitiveUP( uint32 PrimitiveType, uint32 NumPrimitives, uint32 NumVertices, uint32 VertexDataStride, void*& OutVertexData, uint32 MinVertexIndex, uint32 NumIndices, uint32 IndexDataStride, void*& OutIndexData)
{
	SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLDrawPrimitiveUPTime);
	check(PendingState.NumPrimitives == 0);
	check((sizeof(uint16) == IndexDataStride) || (sizeof(uint32) == IndexDataStride));

	if(FOpenGL::SupportsFastBufferData()) 
	{
		OutVertexData = DynamicVertexBuffers.Lock(NumVertices * VertexDataStride);
		OutIndexData = DynamicIndexBuffers.Lock(NumIndices * IndexDataStride);
	}
	else
	{
		uint32 BytesVertex = NumVertices * VertexDataStride;
		if(BytesVertex > PendingState.UpVertexBufferBytes) 
		{
			if(PendingState.UpVertexBuffer) 
			{
				FMemory::Free(PendingState.UpVertexBuffer);
			}
			PendingState.UpVertexBuffer = FMemory::Malloc(BytesVertex);
			PendingState.UpVertexBufferBytes = BytesVertex;
		}
		OutVertexData = PendingState.UpVertexBuffer;
		PendingState.UpStride = VertexDataStride;
		uint32 BytesIndex = NumIndices * IndexDataStride;
		if(BytesIndex > PendingState.UpIndexBufferBytes)
		{
			if(PendingState.UpIndexBuffer) 
			{
				FMemory::Free(PendingState.UpIndexBuffer);
			}
			PendingState.UpIndexBuffer = FMemory::Malloc(BytesIndex);
			PendingState.UpIndexBufferBytes = BytesIndex;
		}
		OutIndexData = PendingState.UpIndexBuffer;
	}

	PendingState.PrimitiveType = PrimitiveType;
	PendingState.NumPrimitives = NumPrimitives;
	PendingState.MinVertexIndex = MinVertexIndex;
	PendingState.IndexDataStride = IndexDataStride;
	PendingState.NumVertices = NumVertices;
	if(FOpenGL::SupportsFastBufferData()) 
	{
		PendingState.DynamicVertexStream.VertexBuffer = DynamicVertexBuffers.GetPendingBuffer();
		PendingState.DynamicVertexStream.Offset = DynamicVertexBuffers.GetPendingOffset();
		PendingState.DynamicVertexStream.Stride = VertexDataStride;
	}
	else
	{
		PendingState.DynamicVertexStream.VertexBuffer = 0;
		PendingState.DynamicVertexStream.Offset = 0;
		PendingState.DynamicVertexStream.Stride = VertexDataStride;
	}
}

/**
 * Draw a primitive using the vertex and index data populated since RHIBeginDrawIndexedPrimitiveUP and clean up any memory as needed
 */
void FOpenGLDynamicRHI::RHIEndDrawIndexedPrimitiveUP()
{
	SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLDrawPrimitiveUPTime);
	VERIFY_GL_SCOPE();
	check(PendingState.NumPrimitives != 0);

	RHI_DRAW_CALL_STATS(PendingState.PrimitiveType,PendingState.NumPrimitives);

	if(FOpenGL::SupportsFastBufferData()) 
	{
		DynamicVertexBuffers.Unlock();
		DynamicIndexBuffers.Unlock();
	}

	FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
	BindPendingFramebuffer(ContextState);
	SetPendingBlendStateForActiveRenderTargets(ContextState);
	UpdateViewportInOpenGLContext(ContextState);
	UpdateScissorRectInOpenGLContext(ContextState);
	UpdateRasterizerStateInOpenGLContext(ContextState);
	UpdateDepthStencilStateInOpenGLContext(ContextState);
	BindPendingShaderState(ContextState);
	CommitGraphicsResourceTables();
	SetupTexturesForDraw(ContextState);
	CommitNonComputeShaderConstants();
	if(FOpenGL::SupportsFastBufferData()) 
	{
		CachedBindElementArrayBuffer(ContextState,DynamicIndexBuffers.GetPendingBuffer()->Resource);
		SetupVertexArrays(ContextState, 0, &PendingState.DynamicVertexStream, 1, PendingState.NumVertices);
	}
	else
	{
		CachedBindElementArrayBuffer(ContextState,0);
		SetupVertexArraysUP(ContextState, PendingState.UpVertexBuffer, PendingState.UpStride);
	}

	GLenum DrawMode = GL_TRIANGLES;
	GLsizei NumElements = 0;
	GLint PatchSize = 0;
	FindPrimitiveType(PendingState.PrimitiveType, ContextState.bUsingTessellation, PendingState.NumPrimitives, DrawMode, NumElements, PatchSize);
	GLenum IndexType = (PendingState.IndexDataStride == sizeof(uint32)) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;

	if (FOpenGL::SupportsTessellation() && DrawMode == GL_PATCHES )
	{
		FOpenGL::PatchParameteri(GL_PATCH_VERTICES, PatchSize);
	}
	
#if DEBUG_GL_SHADERS
	VerifyProgramPipeline();
#endif

	GPUProfilingData.RegisterGPUWork(PendingState.NumPrimitives,PendingState.NumVertices);
	if(FOpenGL::SupportsFastBufferData())
	{
		CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderFirstDrawTime, PendingState.BoundShaderState->RequiresDriverInstantiation());
		if ( FOpenGL::SupportsDrawIndexOffset() )
		{
			FOpenGL::DrawRangeElements(DrawMode, PendingState.MinVertexIndex, PendingState.MinVertexIndex + PendingState.NumVertices, NumElements, IndexType, INDEX_TO_VOID(DynamicIndexBuffers.GetPendingOffset()));
		}
		else
		{
			check(PendingState.MinVertexIndex == 0);
			glDrawElements(DrawMode, NumElements, IndexType, INDEX_TO_VOID(DynamicIndexBuffers.GetPendingOffset()));
		}
	}
	else
	{
		CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderFirstDrawTime, PendingState.BoundShaderState->RequiresDriverInstantiation());
		glDrawElements(DrawMode, NumElements, IndexType, PendingState.UpIndexBuffer);
	}

	PendingState.NumPrimitives = 0;

	REPORT_GL_DRAW_RANGE_ELEMENTS_EVENT_FOR_FRAME_DUMP( DrawMode, PendingState.MinVertexIndex, PendingState.MinVertexIndex + PendingState.NumVertices, NumElements, IndexType, 0 );

	FShaderCache::LogDraw(FShaderCache::GetDefaultCacheState(), PendingState.PrimitiveType, PendingState.IndexDataStride);
}


// Raster operations.
static inline void ClearCurrentDepthStencilWithCurrentScissor( int8 ClearType, float Depth, uint32 Stencil )
{
	switch (ClearType)
	{
	case CT_DepthStencil:	// Clear depth and stencil
		FOpenGL::ClearBufferfi(GL_DEPTH_STENCIL, 0, Depth, Stencil);
		break;

	case CT_Stencil:	// Clear stencil only
		FOpenGL::ClearBufferiv(GL_STENCIL, 0, (const GLint*)&Stencil);
		break;

	case CT_Depth:	// Clear depth only
		FOpenGL::ClearBufferfv(GL_DEPTH, 0, &Depth);
		break;

	default:
		break;	// impossible anyway
	}
}

void FOpenGLDynamicRHI::ClearCurrentFramebufferWithCurrentScissor(FOpenGLContextState& ContextState, int8 ClearType, int32 NumClearColors, const FLinearColor* ClearColorArray, float Depth, uint32 Stencil)
{
	if ( FOpenGL::SupportsMultipleRenderTargets() )
	{
		// Clear color buffers
		if (ClearType & CT_Color)
		{
			for(int32 ColorIndex = 0; ColorIndex < NumClearColors; ++ColorIndex)
			{
				FOpenGL::ClearBufferfv( GL_COLOR, ColorIndex, (const GLfloat*)&ClearColorArray[ColorIndex] );
			}
		}

		if (ClearType & CT_DepthStencil)
		{
			ClearCurrentDepthStencilWithCurrentScissor(ClearType & CT_DepthStencil, Depth, Stencil);
		}
	}
	else
	{
		GLuint Mask = 0;
		if( ClearType & CT_Color && NumClearColors > 0 )
		{
			if (!ContextState.BlendState.RenderTargets[0].ColorWriteMaskR ||
				!ContextState.BlendState.RenderTargets[0].ColorWriteMaskG ||
				!ContextState.BlendState.RenderTargets[0].ColorWriteMaskB ||
				!ContextState.BlendState.RenderTargets[0].ColorWriteMaskA)
			{
				FOpenGL::ColorMaskIndexed(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				ContextState.BlendState.RenderTargets[0].ColorWriteMaskR = 1;
				ContextState.BlendState.RenderTargets[0].ColorWriteMaskG = 1;
				ContextState.BlendState.RenderTargets[0].ColorWriteMaskB = 1;
				ContextState.BlendState.RenderTargets[0].ColorWriteMaskA = 1;
			}

			if (ContextState.ClearColor != ClearColorArray[0])
			{
				glClearColor( ClearColorArray[0].R, ClearColorArray[0].G, ClearColorArray[0].B, ClearColorArray[0].A );
				ContextState.ClearColor = ClearColorArray[0];
			}
			Mask |= GL_COLOR_BUFFER_BIT;
		}
		if ( ClearType & CT_Depth )
		{
			if (!ContextState.DepthStencilState.bZWriteEnable)
			{
				glDepthMask(GL_TRUE);
				ContextState.DepthStencilState.bZWriteEnable = true;
			}
			if (ContextState.ClearDepth != Depth)
			{
				FOpenGL::ClearDepth( Depth );
				ContextState.ClearDepth = Depth;
			}
			Mask |= GL_DEPTH_BUFFER_BIT;
		}
		if ( ClearType & CT_Stencil )
		{
			if (ContextState.DepthStencilState.StencilWriteMask != 0xFFFFFFFF)
			{
				glStencilMask(0xFFFFFFFF);
				ContextState.DepthStencilState.StencilWriteMask = 0xFFFFFFFF;
			}

			if (ContextState.ClearStencil != Stencil)
			{
				glClearStencil( Stencil );
				ContextState.ClearStencil = Stencil;
			}
			Mask |= GL_STENCIL_BUFFER_BIT;
		}

		// do the clear
		glClear( Mask );
	}

	REPORT_GL_CLEAR_EVENT_FOR_FRAME_DUMP( ClearType, NumClearColors, (const float*)ClearColorArray, Depth, Stencil );
}

void FOpenGLDynamicRHI::RHIClearMRT(bool bClearColor,int32 NumClearColors,const FLinearColor* ClearColorArray,bool bClearDepth,float Depth,bool bClearStencil,uint32 Stencil)
{
	FIntRect ExcludeRect;
	VERIFY_GL_SCOPE();

	check((GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5) || !PendingState.bFramebufferSetupInvalid);

	if (bClearColor)
	{
		// This is copied from DirectX11 code - apparently there's a silent assumption that there can be no valid render target set at index higher than an invalid one.
		int32 NumActiveRenderTargets = 0;
		for (int32 TargetIndex = 0; TargetIndex < MaxSimultaneousRenderTargets; TargetIndex++)
		{
			if (PendingState.RenderTargets[TargetIndex] != 0)
			{
				NumActiveRenderTargets++;
			}
			else
			{
				break;
			}
		}
		
		// Must specify enough clear colors for all active RTs
		check(NumClearColors >= NumActiveRenderTargets);
	}

	// Remember cached scissor state, and set one to cover viewport
	FIntRect PrevScissor = PendingState.Scissor;
	bool bPrevScissorEnabled = PendingState.bScissorEnabled;

	bool bScissorChanged = false;
	GPUProfilingData.RegisterGPUWork(0);
	FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
	BindPendingFramebuffer(ContextState);

	if (bPrevScissorEnabled || PendingState.Viewport.Min.X != 0 || PendingState.Viewport.Min.Y != 0 || PendingState.Viewport.Max.X != PendingState.RenderTargetWidth || PendingState.Viewport.Max.Y != PendingState.RenderTargetHeight)
	{
		RHISetScissorRect(false, 0, 0, 0, 0);
		bScissorChanged = true;
	}

	// Always update in case there are uncommitted changes to disable scissor
	UpdateScissorRectInOpenGLContext(ContextState);

	int8 ClearType = CT_None;

	// Prepare color buffer masks, if applicable
	if (bClearColor)
	{
		ClearType |= CT_Color;

		for(int32 ColorIndex = 0; ColorIndex < NumClearColors; ++ColorIndex)
		{
			if( !ContextState.BlendState.RenderTargets[ColorIndex].ColorWriteMaskR ||
				!ContextState.BlendState.RenderTargets[ColorIndex].ColorWriteMaskG ||
				!ContextState.BlendState.RenderTargets[ColorIndex].ColorWriteMaskB ||
				!ContextState.BlendState.RenderTargets[ColorIndex].ColorWriteMaskA)
			{
				FOpenGL::ColorMaskIndexed(ColorIndex, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				ContextState.BlendState.RenderTargets[ColorIndex].ColorWriteMaskR = 1;
				ContextState.BlendState.RenderTargets[ColorIndex].ColorWriteMaskG = 1;
				ContextState.BlendState.RenderTargets[ColorIndex].ColorWriteMaskB = 1;
				ContextState.BlendState.RenderTargets[ColorIndex].ColorWriteMaskA = 1;
			}
		}
	}

	// Prepare depth mask, if applicable
	if (bClearDepth && PendingState.DepthStencil)
	{
		ClearType |= CT_Depth;

		if (!ContextState.DepthStencilState.bZWriteEnable)
		{
			glDepthMask(GL_TRUE);
			ContextState.DepthStencilState.bZWriteEnable = true;
		}
	}

	// Prepare stencil mask, if applicable
	if (bClearStencil && PendingState.DepthStencil)
	{
		ClearType |= CT_Stencil;

		if (ContextState.DepthStencilState.StencilWriteMask != 0xFFFFFFFF)
		{
			glStencilMask(0xFFFFFFFF);
			ContextState.DepthStencilState.StencilWriteMask = 0xFFFFFFFF;
		}
	}

	// Just one clear
	ClearCurrentFramebufferWithCurrentScissor(ContextState, ClearType, NumClearColors, ClearColorArray, Depth, Stencil);

	if (bScissorChanged)
	{
		// Change it back
		RHISetScissorRect(bPrevScissorEnabled,PrevScissor.Min.X, PrevScissor.Min.Y, PrevScissor.Max.X, PrevScissor.Max.Y);
	}
}

// Blocks the CPU until the GPU catches up and goes idle.
void FOpenGLDynamicRHI::RHIBlockUntilGPUIdle()
{
	// Not really supported
}

void FOpenGLDynamicRHI::RHISubmitCommandsAndFlushGPU()
{
	FOpenGL::Flush();
}

/**
 * Returns the total GPU time taken to render the last frame. Same metric as FPlatformTime::Cycles().
 */
uint32 FOpenGLDynamicRHI::RHIGetGPUFrameCycles()
{
	return GGPUFrameTime;
}

void FOpenGLDynamicRHI::RHISetComputeShader(FComputeShaderRHIParamRef ComputeShaderRHI)
{
	if (OpenGLConsoleVariables::bSkipCompute)
	{
		return;
	}

	if ( FOpenGL::SupportsComputeShaders() )
	{
		VERIFY_GL_SCOPE();
		check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);

		PendingState.CurrentComputeShader = ComputeShaderRHI;
	}
	else
	{
		UE_LOG(LogRHI,Fatal,TEXT("Platform doesn't support SM5 for OpenGL but set feature level to SM5"));
	}
}

void FOpenGLDynamicRHI::RHIDispatchComputeShader(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{ 
	if (OpenGLConsoleVariables::bSkipCompute)
	{
		return;
	}

	if ( FOpenGL::SupportsComputeShaders() )
	{
		VERIFY_GL_SCOPE();

		check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);

		FComputeShaderRHIParamRef ComputeShaderRHI = PendingState.CurrentComputeShader;
		check(ComputeShaderRHI);

		FOpenGLComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);

		FOpenGLContextState& ContextState = GetContextStateForCurrentContext();

		GPUProfilingData.RegisterGPUWork(1);

		BindPendingComputeShaderState(ContextState, ComputeShader);
		CommitComputeResourceTables(ComputeShader);
		SetupTexturesForDraw(ContextState, ComputeShader, FOpenGL::GetMaxComputeTextureImageUnits());
		SetupUAVsForDraw(ContextState, ComputeShader, OGL_MAX_COMPUTE_STAGE_UAV_UNITS);
		CommitComputeShaderConstants(ComputeShader);
	
		FOpenGL::MemoryBarrier(GL_ALL_BARRIER_BITS);
		FOpenGL::DispatchCompute(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
		FOpenGL::MemoryBarrier(GL_ALL_BARRIER_BITS);
	}
	else
	{
		UE_LOG(LogRHI,Fatal,TEXT("Platform doesn't support SM5 for OpenGL but set feature level to SM5"));
	}
}

void FOpenGLDynamicRHI::RHIDispatchIndirectComputeShader(FVertexBufferRHIParamRef ArgumentBufferRHI,uint32 ArgumentOffset)
{
	if ( FOpenGL::SupportsComputeShaders() )
	{
		VERIFY_GL_SCOPE();
		check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5)

		FComputeShaderRHIParamRef ComputeShaderRHI = PendingState.CurrentComputeShader;
		check(ComputeShaderRHI);

		FOpenGLComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);
		FOpenGLVertexBuffer* ArgumentBuffer = ResourceCast(ArgumentBufferRHI);

		FOpenGLContextState& ContextState = GetContextStateForCurrentContext();

		GPUProfilingData.RegisterGPUWork(1);

		BindPendingComputeShaderState(ContextState, ComputeShader);

		SetupTexturesForDraw(ContextState, ComputeShader, FOpenGL::GetMaxComputeTextureImageUnits());

		SetupUAVsForDraw(ContextState, ComputeShader, OGL_MAX_COMPUTE_STAGE_UAV_UNITS);
	
		CommitComputeShaderConstants(ComputeShader);
	
		FOpenGL::MemoryBarrier(GL_ALL_BARRIER_BITS);

		glBindBuffer( GL_DISPATCH_INDIRECT_BUFFER, ArgumentBuffer->Resource);
	
		FOpenGL::DispatchComputeIndirect(ArgumentOffset);

		glBindBuffer( GL_DISPATCH_INDIRECT_BUFFER, 0);
	
		FOpenGL::MemoryBarrier(GL_ALL_BARRIER_BITS);

	}
	else
	{
		UE_LOG(LogRHI,Fatal,TEXT("Platform doesn't support SM5 for OpenGL but set feature level to SM5"));
	}
}

void FOpenGLDynamicRHI::RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data)
{
	UE_LOG(LogRHI, Fatal,TEXT("OpenGL Render path does not support multiple Viewports!"));
}

void FOpenGLDynamicRHI::RHIExecuteCommandList(FRHICommandList*)
{
	check(0);
}

void FOpenGLDynamicRHI::RHIEnableDepthBoundsTest(bool bEnable,float MinDepth,float MaxDepth)
{
	if (FOpenGL::SupportsDepthBoundsTest())
	{
		if(bEnable)
		{
			glEnable(GL_DEPTH_BOUNDS_TEST_EXT);
		}
		else
		{
			glDisable(GL_DEPTH_BOUNDS_TEST_EXT);
		}
		FOpenGL::DepthBounds(MinDepth,MaxDepth);
	}
}

void FOpenGLDynamicRHI::RHISubmitCommandsHint()
{
}
IRHICommandContext* FOpenGLDynamicRHI::RHIGetDefaultContext()
{
	return this;
}

IRHICommandContextContainer* FOpenGLDynamicRHI::RHIGetCommandContextContainer(int32 Index, int32 Num)
{
	return nullptr;
}

void FOpenGLDynamicRHI::RHIInvalidateCachedState()
{
	RenderingContextState = FOpenGLContextState();
	SharedContextState = FOpenGLContextState();

	RenderingContextState.InitializeResources(FOpenGL::GetMaxCombinedTextureImageUnits(), OGL_MAX_COMPUTE_STAGE_UAV_UNITS);
	SharedContextState.InitializeResources(FOpenGL::GetMaxCombinedTextureImageUnits(), OGL_MAX_COMPUTE_STAGE_UAV_UNITS);
}



