// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLVertexBuffer.cpp: OpenGL texture RHI implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "Containers/ResourceArray.h"
#include "Stats/Stats.h"
#include "RHI.h"
#include "RenderUtils.h"
#include "ShaderCache.h"
#include "OpenGLDrv.h"
#include "OpenGLDrvPrivate.h"

/*-----------------------------------------------------------------------------
	Texture allocator support.
-----------------------------------------------------------------------------*/

/** Caching it here, to avoid getting it every time we create a texture. 0 is no multisampling. */
GLint GMaxOpenGLColorSamples = 0;
GLint GMaxOpenGLDepthSamples = 0;
GLint GMaxOpenGLIntegerSamples = 0;

// in bytes, never change after RHI, needed to scale game features
int64 GOpenGLDedicatedVideoMemory = 0;
// In bytes. Never changed after RHI init. Our estimate of the amount of memory that we can use for graphics resources in total.
int64 GOpenGLTotalGraphicsMemory = 0;

static bool ShouldCountAsTextureMemory(uint32 Flags)
{
	return (Flags & (TexCreate_RenderTargetable | TexCreate_ResolveTargetable | TexCreate_DepthStencilTargetable)) == 0;
}

void OpenGLTextureAllocated(FRHITexture* Texture, uint32 Flags)
{
	int32 TextureSize = 0;
	FOpenGLTextureCube* TextureCube = 0;
	FOpenGLTexture2D* Texture2D = 0;
	FOpenGLTexture2DArray* Texture2DArray = 0;
	FOpenGLTexture3D* Texture3D = 0;
	bool bRenderTarget = !ShouldCountAsTextureMemory(Flags);

	if (( TextureCube = (FOpenGLTextureCube*)Texture->GetTextureCube()) != NULL)
	{
		TextureSize = CalcTextureSize( TextureCube->GetSize(), TextureCube->GetSize(), TextureCube->GetFormat(), TextureCube->GetNumMips() );
		TextureSize *= TextureCube->GetArraySize() * (TextureCube->GetArraySize() == 1 ? 6 : 1);
		TextureCube->SetMemorySize( TextureSize );
		TextureCube->SetIsPowerOfTwo(FMath::IsPowerOfTwo(TextureCube->GetSizeX()) && FMath::IsPowerOfTwo(TextureCube->GetSizeY()));
		if (bRenderTarget)
		{
			INC_MEMORY_STAT_BY(STAT_RenderTargetMemoryCube,TextureSize);
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_TextureMemoryCube,TextureSize);
		}
	}
	else if ((Texture2D = (FOpenGLTexture2D*)Texture->GetTexture2D()) != NULL)
	{
		TextureSize = CalcTextureSize( Texture2D->GetSizeX(), Texture2D->GetSizeY(), Texture2D->GetFormat(), Texture2D->GetNumMips() )*Texture2D->GetNumSamples();
		Texture2D->SetMemorySize( TextureSize );
		Texture2D->SetIsPowerOfTwo(FMath::IsPowerOfTwo(Texture2D->GetSizeX()) && FMath::IsPowerOfTwo(Texture2D->GetSizeY()));
		if (bRenderTarget)
		{
			INC_MEMORY_STAT_BY(STAT_RenderTargetMemory2D,TextureSize);
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_TextureMemory2D,TextureSize);
		}
	}
	else if ((Texture3D = (FOpenGLTexture3D*)Texture->GetTexture3D()) != NULL)
	{
		TextureSize = CalcTextureSize3D( Texture3D->GetSizeX(), Texture3D->GetSizeY(), Texture3D->GetSizeZ(), Texture3D->GetFormat(), Texture3D->GetNumMips() );
		Texture3D->SetMemorySize( TextureSize );
		Texture3D->SetIsPowerOfTwo(FMath::IsPowerOfTwo(Texture3D->GetSizeX()) && FMath::IsPowerOfTwo(Texture3D->GetSizeY()) && FMath::IsPowerOfTwo(Texture3D->GetSizeZ()));
		if (bRenderTarget)
		{
			INC_MEMORY_STAT_BY(STAT_RenderTargetMemory3D,TextureSize);
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_TextureMemory3D,TextureSize);
		}
	}
	else if ((Texture2DArray = (FOpenGLTexture2DArray*)Texture->GetTexture2DArray()) != NULL)
	{
		TextureSize = Texture2DArray->GetSizeZ() * CalcTextureSize( Texture2DArray->GetSizeX(), Texture2DArray->GetSizeY(), Texture2DArray->GetFormat(), Texture2DArray->GetNumMips() );
		Texture2DArray->SetMemorySize( TextureSize );
		Texture2DArray->SetIsPowerOfTwo(FMath::IsPowerOfTwo(Texture2DArray->GetSizeX()) && FMath::IsPowerOfTwo(Texture2DArray->GetSizeY()));
		if (bRenderTarget)
		{
			INC_MEMORY_STAT_BY(STAT_RenderTargetMemory2D,TextureSize);
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_TextureMemory2D,TextureSize);
		}
	}
	else
	{
		check(0);	// Add handling of other texture types
	}

	if( bRenderTarget )
	{
		GCurrentRendertargetMemorySize += Align(TextureSize, 1024) / 1024;
	}
	else
	{
		GCurrentTextureMemorySize += Align(TextureSize, 1024) / 1024;
	}
}

void OpenGLTextureDeleted( FRHITexture* Texture )
{
	FShaderCache::RemoveTexture(Texture);
	
	bool bRenderTarget = !ShouldCountAsTextureMemory(Texture->GetFlags());
	int32 TextureSize = 0;
	if (Texture->GetTextureCube())
	{
		TextureSize = ((FOpenGLTextureCube*)Texture->GetTextureCube())->GetMemorySize();
		if (bRenderTarget)
		{
			DEC_MEMORY_STAT_BY(STAT_RenderTargetMemoryCube,TextureSize);
		}
		else
		{
			DEC_MEMORY_STAT_BY(STAT_TextureMemoryCube,TextureSize);
		}
	}
	else if (Texture->GetTexture2D())
	{
		TextureSize = ((FOpenGLTexture2D*)Texture->GetTexture2D())->GetMemorySize();
		if (bRenderTarget)
		{
			DEC_MEMORY_STAT_BY(STAT_RenderTargetMemory2D,TextureSize);
		}
		else
		{
			DEC_MEMORY_STAT_BY(STAT_TextureMemory2D,TextureSize);
		}
	}
	else if (Texture->GetTexture3D())
	{
		TextureSize = ((FOpenGLTexture3D*)Texture->GetTexture3D())->GetMemorySize();
		if (bRenderTarget)
		{
			DEC_MEMORY_STAT_BY(STAT_RenderTargetMemory3D,TextureSize);
		}
		else
		{
			DEC_MEMORY_STAT_BY(STAT_TextureMemory3D,TextureSize);
		}
	}
	else if (Texture->GetTexture2DArray())
	{
		TextureSize = ((FOpenGLTexture2DArray*)Texture->GetTexture2DArray())->GetMemorySize();
		if (bRenderTarget)
		{
			DEC_MEMORY_STAT_BY(STAT_RenderTargetMemory2D,TextureSize);
		}
		else
		{
			DEC_MEMORY_STAT_BY(STAT_TextureMemory2D,TextureSize);
		}
	}
	else
	{
		check(0);	// Add handling of other texture types
	}

	if( bRenderTarget )
	{
		GCurrentRendertargetMemorySize -= Align(TextureSize, 1024) / 1024;
	}
	else
	{
		GCurrentTextureMemorySize -= Align(TextureSize, 1024) / 1024;
	}
}

uint64 FOpenGLDynamicRHI::RHICalcTexture2DPlatformSize(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, uint32& OutAlign)
{
	OutAlign = 0;
	return CalcTextureSize(SizeX, SizeY, (EPixelFormat)Format, NumMips);
}

uint64 FOpenGLDynamicRHI::RHICalcTexture3DPlatformSize(uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint8 Format, uint32 NumMips, uint32 Flags, uint32& OutAlign)
{
	OutAlign = 0;
	return CalcTextureSize3D(SizeX, SizeY, SizeZ, (EPixelFormat)Format, NumMips);
}

uint64 FOpenGLDynamicRHI::RHICalcTextureCubePlatformSize(uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags,	uint32& OutAlign)
{
	OutAlign = 0;
	return CalcTextureSize(Size, Size, (EPixelFormat)Format, NumMips) * 6;
}

/**
 * Retrieves texture memory stats. Unsupported with this allocator.
 *
 * @return false, indicating that out variables were left unchanged.
 */
void FOpenGLDynamicRHI::RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats)
{
	OutStats.DedicatedVideoMemory = GOpenGLDedicatedVideoMemory;
    OutStats.DedicatedSystemMemory = 0;
    OutStats.SharedSystemMemory = 0;
	OutStats.TotalGraphicsMemory = GOpenGLTotalGraphicsMemory ? GOpenGLTotalGraphicsMemory : -1;

	OutStats.AllocatedMemorySize = int64(GCurrentTextureMemorySize) * 1024;
	OutStats.LargestContiguousAllocation = OutStats.AllocatedMemorySize;
	OutStats.TexturePoolSize = GTexturePoolSize;
	OutStats.PendingMemoryAdjustment = 0;
}


/**
 * Fills a texture with to visualize the texture pool memory.
 *
 * @param	TextureData		Start address
 * @param	SizeX			Number of pixels along X
 * @param	SizeY			Number of pixels along Y
 * @param	Pitch			Number of bytes between each row
 * @param	PixelSize		Number of bytes each pixel represents
 *
 * @return true if successful, false otherwise
 */
bool FOpenGLDynamicRHI::RHIGetTextureMemoryVisualizeData( FColor* /*TextureData*/, int32 /*SizeX*/, int32 /*SizeY*/, int32 /*Pitch*/, int32 /*PixelSize*/ )
{
	return false;
}



FRHITexture* FOpenGLDynamicRHI::CreateOpenGLTexture(uint32 SizeX, uint32 SizeY, bool bCubeTexture, bool bArrayTexture, bool bIsExternal, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 ArraySize, uint32 Flags, const FClearValueBinding& InClearValue, FResourceBulkDataInterface* BulkData)
{
	VERIFY_GL_SCOPE();

	SCOPE_CYCLE_COUNTER(STAT_OpenGLCreateTextureTime);

	bool bAllocatedStorage = false;

	if (NumMips == 0)
	{
		if (NumSamples <= 1)
		{
			NumMips = FindMaxMipmapLevel(SizeX, SizeY);
		}
		else
		{
			NumMips = 1;
		}
	}

#if UE_BUILD_DEBUG
	check(!(NumSamples > 1 && bCubeTexture));
	check(bArrayTexture != (ArraySize == 1));
#endif

	// Move NumSamples to on-chip MSAA if supported
	uint32 NumSamplesTileMem = 1;
	GLint MaxSamplesTileMem = FOpenGL::GetMaxMSAASamplesTileMem(); /* RHIs which do not support tiled GPU MSAA return 0 */
	if (MaxSamplesTileMem > 0)
	{
		NumSamplesTileMem = FMath::Min<uint32>(NumSamples, MaxSamplesTileMem);
		NumSamples = 1;
	}

	bool bNoSRGBSupport = (GMaxRHIFeatureLevel == ERHIFeatureLevel::ES2);

	if ((Flags & TexCreate_RenderTargetable) && Format == PF_B8G8R8A8 && !FOpenGL::SupportsBGRA8888RenderTarget())
	{
		// Some android devices does not support BGRA as a color attachment
		Format = PF_R8G8B8A8;
	}

	if (bNoSRGBSupport)
	{
		// Remove sRGB read flag when not supported
		Flags &= ~TexCreate_SRGB;
	}

	GLuint TextureID = 0;
	FOpenGL::GenTextures(1, &TextureID);

	GLenum Target = GL_NONE;
	if (bCubeTexture)
	{
		if (FOpenGL::SupportsTexture3D())
		{
			Target = bArrayTexture ? GL_TEXTURE_CUBE_MAP_ARRAY : GL_TEXTURE_CUBE_MAP;
		}
		else
		{
			check(!bArrayTexture);
			Target = GL_TEXTURE_CUBE_MAP;
		}
		check(SizeX == SizeY);
	}
#if PLATFORM_ANDROID
	else if (bIsExternal)
	{
		if (FOpenGL::SupportsImageExternal())
		{
			Target = GL_TEXTURE_EXTERNAL_OES;
		}
		else
		{
			// Fall back to a regular 2d texture if we don't have support. Texture samplers in the shader will also fall back to a regular sampler2D.
			Target = GL_TEXTURE_2D;
		}
	}
#endif
	else
	{
		Target =  (NumSamples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

		// @todo: refactor 2d texture array support here?
		check(!bArrayTexture);
	}

	check(Target != GL_NONE);

	const bool bSRGB = (Flags&TexCreate_SRGB) != 0;
	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[Format];
	if (GLFormat.InternalFormat[bSRGB] == GL_NONE)
	{
		UE_LOG(LogRHI, Fatal,TEXT("Texture format '%s' not supported (sRGB=%d)."), GPixelFormats[Format].Name, bSRGB);
	}

	FOpenGLContextState& ContextState = GetContextStateForCurrentContext();

	// Make sure PBO is disabled
	CachedBindPixelUnpackBuffer(ContextState,0);

	// Use a texture stage that's not likely to be used for draws, to avoid waiting
	CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Target, TextureID, 0, NumMips);

	// For client storage textures we allocate a single backing store buffer.
	uint8* TextureRange = nullptr;
	
	if (NumSamples == 1)
	{
		if (!FMath::IsPowerOfTwo(SizeX) || !FMath::IsPowerOfTwo(SizeY))
		{
			glTexParameteri(Target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(Target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			if ( FOpenGL::SupportsTexture3D() )
			{
				glTexParameteri(Target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
			}
		}
		else
		{
			glTexParameteri(Target, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(Target, GL_TEXTURE_WRAP_T, GL_REPEAT);
			if ( FOpenGL::SupportsTexture3D() )
			{
				glTexParameteri(Target, GL_TEXTURE_WRAP_R, GL_REPEAT);
			}
		}
		glTexParameteri(Target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(Target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		if( FOpenGL::SupportsTextureFilterAnisotropic() )
		{
			glTexParameteri(Target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
		}
		if ( FOpenGL::SupportsTextureBaseLevel() )
		{
			glTexParameteri(Target, GL_TEXTURE_BASE_LEVEL, 0);
		}
		if ( FOpenGL::SupportsTextureMaxLevel() )
		{
#if PLATFORM_ANDROID
			// Do not use GL_TEXTURE_MAX_LEVEL if external texture on Android
			if (Target != GL_TEXTURE_EXTERNAL_OES)
#endif
			{
				glTexParameteri(Target, GL_TEXTURE_MAX_LEVEL, NumMips - 1);
			}
		}
		
		TextureMipLimits.Add(TextureID, TPair<GLenum, GLenum>(0, NumMips - 1));
		
		if (FOpenGL::SupportsTextureSwizzle() && GLFormat.bBGRA && !(Flags & TexCreate_RenderTargetable))
		{
			glTexParameteri(Target, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
			glTexParameteri(Target, GL_TEXTURE_SWIZZLE_B, GL_RED);
		}

		if (bArrayTexture)
		{
			FOpenGL::TexStorage3D( Target, NumMips, GLFormat.InternalFormat[bSRGB], SizeX, SizeY, ArraySize, GLFormat.Format, GLFormat.Type);
		}
		else
		{
			// Should we use client-storage to improve update time on platforms that require it
			bool const bRenderable = (Flags & (TexCreate_RenderTargetable|TexCreate_ResolveTargetable|TexCreate_DepthStencilTargetable|TexCreate_CPUReadback)) != 0;
			bool const bUseClientStorage = (FOpenGL::SupportsClientStorage() && !FOpenGL::SupportsTextureView() && !bRenderable && !GLFormat.bCompressed);
			if(bUseClientStorage)
			{
				const bool bIsCubeTexture = Target == GL_TEXTURE_CUBE_MAP;
				const uint32 TextureSize = CalcTextureSize(SizeX, SizeY, (EPixelFormat)Format, NumMips) * (bIsCubeTexture ? 6 : 1);
				const GLenum FirstTarget = bIsCubeTexture ? GL_TEXTURE_CUBE_MAP_POSITIVE_X : Target;
				const uint32 NumTargets = bIsCubeTexture ? 6 : 1;
				
				TextureRange = new uint8[TextureSize];
				check(TextureRange);
				
				if(FOpenGL::SupportsTextureRange())
				{
					FOpenGL::TextureRange(Target, TextureSize, TextureRange);
					glTexParameteri(Target, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_CACHED_APPLE);
				}
				
				glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				
				uint8* MipPointer = TextureRange;
				for(uint32 MipIndex = 0; MipIndex < uint32(NumMips); MipIndex++)
				{
					const uint32 MipSize = CalcTextureMipMapSize(SizeX, SizeY, (EPixelFormat)Format, MipIndex);
					for(uint32 TargetIndex = 0; TargetIndex < NumTargets; TargetIndex++)
					{
						glTexImage2D(
									 FirstTarget + TargetIndex,
									 MipIndex,
									 GLFormat.InternalFormat[bSRGB],
									 FMath::Max<uint32>(1,(SizeX >> MipIndex)),
									 FMath::Max<uint32>(1,(SizeY >> MipIndex)),
									 0,
									 GLFormat.Format,
									 GLFormat.Type,
									 MipPointer
									 );
						MipPointer += MipSize;
					}
				}
				
				glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
				glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE);
				
				if(FOpenGL::SupportsTextureRange())
				{
					FOpenGL::TextureRange(Target, 0, 0);
					glTexParameteri(Target, GL_TEXTURE_STORAGE_HINT_APPLE, GL_STORAGE_PRIVATE_APPLE);
				}
				
				// Leave bAllocatedStorage as false, so that the client storage buffers are setup only when the texture is locked
			}
			// Try to allocate using TexStorage2D
			else if( FOpenGL::TexStorage2D( Target, NumMips, GLFormat.SizedInternalFormat[bSRGB], SizeX, SizeY, GLFormat.Format, GLFormat.Type, Flags) )
			{
				bAllocatedStorage = true;
			}
			else if (!GLFormat.bCompressed)
			{
				// Otherwise, allocate storage for each mip using TexImage2D
				// We can't do so for compressed textures because we can't pass NULL in to CompressedTexImage2D!
				bAllocatedStorage = true;

				const bool bIsCubeTexture = Target == GL_TEXTURE_CUBE_MAP;
				const GLenum FirstTarget = bIsCubeTexture ? GL_TEXTURE_CUBE_MAP_POSITIVE_X : Target;
				const uint32 NumTargets = bIsCubeTexture ? 6 : 1;

				for(uint32 MipIndex = 0; MipIndex < uint32(NumMips); MipIndex++)
				{
					for(uint32 TargetIndex = 0; TargetIndex < NumTargets; TargetIndex++)
					{
						glTexImage2D(
							FirstTarget + TargetIndex,
							MipIndex,
							GLFormat.InternalFormat[bSRGB],
							FMath::Max<uint32>(1,(SizeX >> MipIndex)),
							FMath::Max<uint32>(1,(SizeY >> MipIndex)),
							0,
							GLFormat.Format,
							GLFormat.Type,
							NULL
							);
					}
				}
			}
		}

		if (BulkData != NULL)
		{
			uint8* Data = (uint8*)BulkData->GetResourceBulkData();
			uint32 MipOffset = 0;

			const uint32 BlockSizeX = GPixelFormats[Format].BlockSizeX;
			const uint32 BlockSizeY = GPixelFormats[Format].BlockSizeY;
			for(uint32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
			{
				uint32 NumBlocksX = AlignArbitrary(FMath::Max<uint32>(1,(SizeX >> MipIndex)), BlockSizeX) / BlockSizeX;
				uint32 NumBlocksY = AlignArbitrary(FMath::Max<uint32>(1,(SizeY >> MipIndex)), BlockSizeY) / BlockSizeY;
				uint32 NumLayers = FMath::Max<uint32>(1,ArraySize);
				
				if(bArrayTexture )
				{
					if(bCubeTexture)
					{
						check(FOpenGL::SupportsTexture3D());
						FOpenGL::TexSubImage3D(
							/*Target=*/ Target,
							/*Level=*/ MipIndex,
							/* XOffset */ 0,
							/* YOffset */ 0,
							/* ZOffset */ 0,
							/*SizeX=*/ FMath::Max<uint32>(1,(SizeX >> MipIndex)),
							/*SizeY=*/ FMath::Max<uint32>(1,(SizeY >> MipIndex)),
							/*SizeZ=*/ ArraySize,
							/*Format=*/ GLFormat.Format,
							/*Type=*/ GLFormat.Type,
							/*Data=*/ &Data[MipOffset]
							);
					}
					else
					{
						// @todo: refactor 2d texture arrays here?
						check(!bCubeTexture);
					}
					
					MipOffset += NumBlocksX * NumBlocksY * NumLayers * GPixelFormats[Format].BlockBytes;
				}
				else
				{
					GLenum FirstTarget = bCubeTexture ? GL_TEXTURE_CUBE_MAP_POSITIVE_X : Target;
					uint32 NumTargets = bCubeTexture ? 6 : 1;

					for(uint32 TargetIndex = 0; TargetIndex < NumTargets; TargetIndex++)
					{
						glTexSubImage2D(
							/*Target=*/ FirstTarget + TargetIndex,
							/*Level=*/ MipIndex,
							/*XOffset*/ 0,
							/*YOffset*/ 0,
							/*SizeX=*/ FMath::Max<uint32>(1,(SizeX >> MipIndex)),
							/*SizeY=*/ FMath::Max<uint32>(1,(SizeY >> MipIndex)),
							/*Format=*/ GLFormat.Format,
							/*Type=*/ GLFormat.Type,
							/*Data=*/ &Data[MipOffset]
							);
						
						MipOffset += NumBlocksX * NumBlocksY * NumLayers * GPixelFormats[Format].BlockBytes;
					}
				}
			}

			BulkData->Discard();
		}
	}
	else
	{
		check( FOpenGL::SupportsMultisampledTextures() );
		check( BulkData == NULL);

		// Try to create an immutable texture and fallback if it fails
		if (!FOpenGL::TexStorage2DMultisample( Target, NumSamples, GLFormat.InternalFormat[bSRGB], SizeX, SizeY, true))
		{
			FOpenGL::TexImage2DMultisample(
				Target,
				NumSamples,
				GLFormat.InternalFormat[bSRGB],
				SizeX,
				SizeY,
				true
				);
		}
	}
	
	// Determine the attachment point for the texture.	
	GLenum Attachment = GL_NONE;
	if((Flags & TexCreate_RenderTargetable) || (Flags & TexCreate_CPUReadback))
	{
		Attachment = GL_COLOR_ATTACHMENT0;
	}
	else if(Flags & TexCreate_DepthStencilTargetable)
	{
		Attachment = (Format == PF_DepthStencil && FOpenGL::SupportsPackedDepthStencil()) ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
	}
	else if(Flags & TexCreate_ResolveTargetable)
	{
		Attachment = (Format == PF_DepthStencil && FOpenGL::SupportsPackedDepthStencil())
						? GL_DEPTH_STENCIL_ATTACHMENT
						: ((Format == PF_ShadowDepth || Format == PF_D24)
							? GL_DEPTH_ATTACHMENT
							: GL_COLOR_ATTACHMENT0);
	}

	switch(Attachment)
	{
		case GL_COLOR_ATTACHMENT0:
			check(GMaxOpenGLColorSamples>=(GLint)NumSamples);
			break;
		case GL_DEPTH_ATTACHMENT:
		case GL_DEPTH_STENCIL_ATTACHMENT:
			check(GMaxOpenGLDepthSamples>=(GLint)NumSamples);
			break;
		default:
			break;
	}
	// @todo: If integer pixel format
	//check(GMaxOpenGLIntegerSamples>=NumSamples);

	FRHITexture* Texture;
	if (bCubeTexture)
	{
		FOpenGLTextureCube* TextureCube = new FOpenGLTextureCube(this, TextureID, Target, Attachment, SizeX, SizeY, 0, NumMips, 1, 1, ArraySize, (EPixelFormat)Format, true, bAllocatedStorage, Flags, TextureRange, InClearValue);
		Texture = TextureCube;
	}
	else
	{
		FOpenGLTexture2D* Texture2D = new FOpenGLTexture2D(this,TextureID,Target,Attachment,SizeX,SizeY,0,NumMips,NumSamples, NumSamplesTileMem, 1, (EPixelFormat)Format,false,bAllocatedStorage,Flags,TextureRange, InClearValue);
		Texture = Texture2D;
	}
	OpenGLTextureAllocated( Texture, Flags );

	// No need to restore texture stage; leave it like this,
	// and the next draw will take care of cleaning it up; or
	// next operation that needs the stage will switch something else in on it.

	return Texture;
}

#if PLATFORM_ANDROIDESDEFERRED // Flithy hack to workaround radr://16011763
GLuint FOpenGLTextureBase::GetOpenGLFramebuffer(uint32 ArrayIndices, uint32 MipmapLevels)
{
	GLuint FBO = 0;
	switch(Attachment)
	{
		case GL_COLOR_ATTACHMENT0:
		{
			FOpenGLTextureBase* RenderTarget[] = {this};
			FBO = OpenGLRHI->GetOpenGLFramebuffer(1, RenderTarget, &ArrayIndices, &MipmapLevels, NULL);
			break;
		}
		case GL_DEPTH_ATTACHMENT:
		case GL_DEPTH_STENCIL_ATTACHMENT:
		{
			FBO = OpenGLRHI->GetOpenGLFramebuffer(1, NULL, &ArrayIndices, &MipmapLevels, this);
			break;
		}
		default:
			break;
	}
	return FBO;
}
#endif

void FOpenGLTextureBase::InvalidateTextureResourceInCache()
{
	OpenGLRHI->InvalidateTextureResourceInCache(Resource);
	if (SRVResource)
	{
		OpenGLRHI->InvalidateTextureResourceInCache(SRVResource);
	}
}

template<typename RHIResourceType>
void TOpenGLTexture<RHIResourceType>::Resolve(uint32 MipIndex,uint32 ArrayIndex)
{
	VERIFY_GL_SCOPE();
	
#if UE_BUILD_DEBUG
	if((FOpenGLTexture2D*)this->GetTexture2D())
	{
		check( ((FOpenGLTexture2D*)this->GetTexture2D())->GetNumSamples() == 1 );
	}
#endif
	
	// Calculate the dimensions of the mip-map.
	EPixelFormat PixelFormat = this->GetFormat();
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	const uint32 MipSizeX = FMath::Max(this->GetSizeX() >> MipIndex,BlockSizeX);
	const uint32 MipSizeY = FMath::Max(this->GetSizeY() >> MipIndex,BlockSizeY);
	uint32 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;
	uint32 NumBlocksY = (MipSizeY + BlockSizeY - 1) / BlockSizeY;
	if ( PixelFormat == PF_PVRTC2 || PixelFormat == PF_PVRTC4 )
	{
		// PVRTC has minimum 2 blocks width and height
		NumBlocksX = FMath::Max<uint32>(NumBlocksX, 2);
		NumBlocksY = FMath::Max<uint32>(NumBlocksY, 2);
	}
	const uint32 MipBytes = NumBlocksX * NumBlocksY * BlockBytes;
	
	const int32 BufferIndex = MipIndex * (bCubemap ? 6 : 1) * this->GetEffectiveSizeZ() + ArrayIndex;
	
	// Standard path with a PBO mirroring ever slice of a texture to allow multiple simulataneous maps
	if (!IsValidRef(PixelBuffers[BufferIndex]))
	{
		PixelBuffers[BufferIndex] = new FOpenGLPixelBuffer(0, MipBytes, BUF_Dynamic);
	}
	
	TRefCountPtr<FOpenGLPixelBuffer> PixelBuffer = PixelBuffers[BufferIndex];
	check(PixelBuffer->GetSize() == MipBytes);
	check(!PixelBuffer->IsLocked());
	
	check( FOpenGL::SupportsPixelBuffers() );
	
	// Transfer data from texture to pixel buffer.
	// This may be further optimized by caching information if surface content was changed since last lock.
	
	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[PixelFormat];
	const bool bSRGB = (this->GetFlags() & TexCreate_SRGB) != 0;
	
	// Use a texture stage that's not likely to be used for draws, to avoid waiting
	FOpenGLContextState& ContextState = OpenGLRHI->GetContextStateForCurrentContext();
	OpenGLRHI->CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Target, Resource, -1, this->GetNumMips());
	
	glBindBuffer( GL_PIXEL_PACK_BUFFER, PixelBuffer->Resource );

#if PLATFORM_ANDROIDESDEFERRED // glReadPixels is async with PBOs - glGetTexImage is not: radr://16011763
	if(Attachment == GL_COLOR_ATTACHMENT0 && !GLFormat.bCompressed)
	{
		GLuint SourceFBO = GetOpenGLFramebuffer(ArrayIndex, MipIndex);
		check(SourceFBO > 0);
		glBindFramebuffer(UGL_READ_FRAMEBUFFER, SourceFBO);
		FOpenGL::ReadBuffer(Attachment);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, MipSizeX, MipSizeY, GLFormat.Format, GLFormat.Type, 0 );
		glPixelStorei(GL_PACK_ALIGNMENT, 4);
		ContextState.Framebuffer = (GLuint)-1;
	}
	else
#endif
	{
		if( this->GetSizeZ() )
		{
			// apparently it's not possible to retrieve compressed image from GL_TEXTURE_2D_ARRAY in OpenGL for compressed images
			// and for uncompressed ones it's not possible to specify the image index
			check(0);
		}
		else
		{
			if (GLFormat.bCompressed)
			{
				FOpenGL::GetCompressedTexImage(
											   bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
											   MipIndex,
											   0);	// offset into PBO
			}
			else
			{
				glPixelStorei(GL_PACK_ALIGNMENT, 1);
				FOpenGL::GetTexImage(
									 bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
									 MipIndex,
									 GLFormat.Format,
									 GLFormat.Type,
									 0);	// offset into PBO
				glPixelStorei(GL_PACK_ALIGNMENT, 4);
			}
		}
	}
	
	glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
	
	// No need to restore texture stage; leave it like this,
	// and the next draw will take care of cleaning it up; or
	// next operation that needs the stage will switch something else in on it.
}

template<typename RHIResourceType>
void* TOpenGLTexture<RHIResourceType>::Lock(uint32 InMipIndex,uint32 ArrayIndex,EResourceLockMode LockMode,uint32& DestStride)
{
	VERIFY_GL_SCOPE();

#if UE_BUILD_DEBUG
	if((FOpenGLTexture2D*)this->GetTexture2D())
	{
		check( ((FOpenGLTexture2D*)this->GetTexture2D())->GetNumSamples() == 1 );
	}
#endif

	SCOPE_CYCLE_COUNTER(STAT_OpenGLLockTextureTime);

	void* result = NULL;

	// Calculate the dimensions of the mip-map.
	EPixelFormat PixelFormat = this->GetFormat();
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	const uint32 MipSizeX = FMath::Max(this->GetSizeX() >> InMipIndex,BlockSizeX);
	const uint32 MipSizeY = FMath::Max(this->GetSizeY() >> InMipIndex,BlockSizeY);
	uint32 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;
	uint32 NumBlocksY = (MipSizeY + BlockSizeY - 1) / BlockSizeY;
	if ( PixelFormat == PF_PVRTC2 || PixelFormat == PF_PVRTC4 )
	{
		// PVRTC has minimum 2 blocks width and height
		NumBlocksX = FMath::Max<uint32>(NumBlocksX, 2);
		NumBlocksY = FMath::Max<uint32>(NumBlocksY, 2);
	}
	const uint32 MipBytes = NumBlocksX * NumBlocksY * BlockBytes;

	DestStride = NumBlocksX * BlockBytes;

	const int32 BufferIndex = InMipIndex * (bCubemap ? 6 : 1) * this->GetEffectiveSizeZ() + ArrayIndex;

	// Should we use client-storage to improve update time on platforms that require it
	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[PixelFormat];
	bool const bRenderable = (this->GetFlags() & (TexCreate_RenderTargetable|TexCreate_ResolveTargetable|TexCreate_DepthStencilTargetable|TexCreate_CPUReadback)) != 0;
	bool const bUseClientStorage = FOpenGL::SupportsClientStorage() && !FOpenGL::SupportsTextureView() && !bRenderable && !this->GetSizeZ() && !GLFormat.bCompressed;
	if(!bUseClientStorage)
	{
		// Standard path with a PBO mirroring ever slice of a texture to allow multiple simulataneous maps
		bool bBufferExists = true;
		if (!IsValidRef(PixelBuffers[BufferIndex]))
		{
			bBufferExists = false;
			PixelBuffers[BufferIndex] = new FOpenGLPixelBuffer(0, MipBytes, BUF_Dynamic);
		}

		TRefCountPtr<FOpenGLPixelBuffer> PixelBuffer = PixelBuffers[BufferIndex];
		check(PixelBuffer->GetSize() == MipBytes);
		check(!PixelBuffer->IsLocked());
		
		// If the buffer already exists & the flags are such that the texture cannot be rendered to & is CPU accessible then we can skip the internal resolve for read locks. This makes HZB occlusion faster.
		const bool bCPUTexResolved = bBufferExists && (this->GetFlags() & TexCreate_CPUReadback) && !(this->GetFlags() & (TexCreate_RenderTargetable|TexCreate_DepthStencilTargetable));

		if( LockMode != RLM_WriteOnly && !bCPUTexResolved && FOpenGL::SupportsPixelBuffers() )
		{
			Resolve(InMipIndex, ArrayIndex);
		}

		result = PixelBuffer->Lock(0, PixelBuffer->GetSize(), LockMode == RLM_ReadOnly, LockMode != RLM_ReadOnly);
	}
	else
	{
		// Use APPLE_client_storage to reduce memory usage and improve performance
		// GL's which support this extension only need copy a pointer, not the memory contents
		check( FOpenGL::SupportsClientStorage() && !FOpenGL::SupportsTextureView() );
		if(GetAllocatedStorageForMip(InMipIndex,ArrayIndex))
		{
			result = ClientStorageBuffers[BufferIndex].Data;
		}
		else
		{
			// The assumption at present is that this only applies to 2D & cubemap textures
			// Array, 3D and variants thereof aren't supported.
			const bool bIsCubeTexture = Target == GL_TEXTURE_CUBE_MAP;
			const GLenum FirstTarget = bIsCubeTexture ? GL_TEXTURE_CUBE_MAP_POSITIVE_X : Target;
			const uint32 NumTargets = bIsCubeTexture ? 6 : 1;
			
			uint8* MipPointer = TextureRange;
			for(uint32 MipIndex = 0; MipIndex < FOpenGLTextureBase::NumMips; MipIndex++)
			{
				const uint32 MipSize = CalcTextureMipMapSize(this->GetSizeX(), this->GetSizeY(), PixelFormat, MipIndex);
				for(uint32 TargetIndex = 0; TargetIndex < NumTargets; TargetIndex++)
				{
					const int32 ClientIndex = (MipIndex * NumTargets) + TargetIndex;
					ClientStorageBuffers[ClientIndex].Data = MipPointer;
					ClientStorageBuffers[ClientIndex].Size = MipSize;
					ClientStorageBuffers[ClientIndex].bReadOnly = false;
					MipPointer += MipSize;
					SetAllocatedStorageForMip(MipIndex,TargetIndex);
				}
				
			}
			
			result = ClientStorageBuffers[BufferIndex].Data;
		}
		ClientStorageBuffers[BufferIndex].bReadOnly = (LockMode == RLM_ReadOnly);
	}
	
	return result;
}

// Copied from OpenGLDebugFrameDump.
inline uint32 HalfFloatToFloatInteger(uint16 HalfFloat)
{
	uint32 Sign = (HalfFloat >> 15) & 0x00000001;
	uint32 Exponent = (HalfFloat >> 10) & 0x0000001f;
	uint32 Mantiss = HalfFloat & 0x000003ff;

	if (Exponent == 0)
	{
		if (Mantiss == 0) // Plus or minus zero
		{
			return Sign << 31;
		}
		else // Denormalized number -- renormalize it
		{
			while ((Mantiss & 0x00000400) == 0)
			{
				Mantiss <<= 1;
				Exponent -= 1;
			}

			Exponent += 1;
			Mantiss &= ~0x00000400;
		}
	}
	else if (Exponent == 31)
	{
		if (Mantiss == 0) // Inf
			return (Sign << 31) | 0x7f800000;
		else // NaN
			return (Sign << 31) | 0x7f800000 | (Mantiss << 13);
	}

	Exponent = Exponent + (127 - 15);
	Mantiss = Mantiss << 13;

	return (Sign << 31) | (Exponent << 23) | Mantiss;
}

inline float HalfFloatToFloat(uint16 HalfFloat)
{
	union
	{
		float F;
		uint32 I;
	} Convert;

	Convert.I = HalfFloatToFloatInteger(HalfFloat);
	return Convert.F;
}

template<typename RHIResourceType>
void TOpenGLTexture<RHIResourceType>::Unlock(uint32 MipIndex,uint32 ArrayIndex)
{
	VERIFY_GL_SCOPE();

	SCOPE_CYCLE_COUNTER(STAT_OpenGLUnlockTextureTime);

	const int32 BufferIndex = MipIndex * (bCubemap ? 6 : 1) * this->GetEffectiveSizeZ() + ArrayIndex;
	TRefCountPtr<FOpenGLPixelBuffer> PixelBuffer = PixelBuffers[BufferIndex];
	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[this->GetFormat()];
	const bool bSRGB = (this->GetFlags() & TexCreate_SRGB) != 0;

	// Should we use client-storage to improve update time on platforms that require it
	bool const bRenderable = (this->GetFlags() & (TexCreate_RenderTargetable|TexCreate_ResolveTargetable|TexCreate_DepthStencilTargetable|TexCreate_CPUReadback)) != 0;
	bool const bUseClientStorage = FOpenGL::SupportsClientStorage() && !FOpenGL::SupportsTextureView() && !bRenderable && !this->GetSizeZ() && !GLFormat.bCompressed;
	check(bUseClientStorage || IsValidRef(PixelBuffers[BufferIndex]));
	
#if PLATFORM_ANDROID
	// check for FloatRGBA to RGBA8 conversion needed
	if (this->GetFormat() == PF_FloatRGBA && GLFormat.Type == GL_UNSIGNED_BYTE)
	{
		UE_LOG(LogRHI, Warning, TEXT("Converting texture from PF_FloatRGBA to RGBA8!  Only supported for limited cases of 0.0 to 1.0 values (clamped)"));

		// Code path for non-PBO: and always uncompressed!
		// Volume/array textures are currently only supported if PixelBufferObjects are also supported.
		check(this->GetSizeZ() == 0);

		// Use a texture stage that's not likely to be used for draws, to avoid waiting
		FOpenGLContextState& ContextState = OpenGLRHI->GetContextStateForCurrentContext();
		OpenGLRHI->CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Target, Resource, -1, this->GetNumMips());

		CachedBindPixelUnpackBuffer(0);

		// get the source data and size
		uint16* floatData = (uint16*)PixelBuffer->GetLockedBuffer();
		int32 texWidth = FMath::Max<uint32>(1, (this->GetSizeX() >> MipIndex));
		int32 texHeight = FMath::Max<uint32>(1, (this->GetSizeY() >> MipIndex));

		// always RGBA8 so 4 bytes / pixel
		int nValues = texWidth * texHeight * 4;
		uint8* rgbaData = (uint8*)FMemory::Malloc(nValues);

		// convert to GL_BYTE (saturate)
		uint8* outPtr = rgbaData;
		while (nValues--)
		{
			int32 pixelValue = (int32)(HalfFloatToFloat(*floatData++) * 255.0f);
			*outPtr++ = (uint8)(pixelValue < 0 ? 0 : (pixelValue < 256 ? pixelValue : 255));
		}

		// All construction paths should have called TexStorage2D or TexImage2D. So we will
		// always call TexSubImage2D.
		check(GetAllocatedStorageForMip(MipIndex, ArrayIndex) == true);
		glTexSubImage2D(
			bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
			MipIndex,
			0,
			0,
			texWidth,
			texHeight,
			GLFormat.Format,
			GLFormat.Type,
			rgbaData);

		// free temporary conversion buffer
		FMemory::Free(rgbaData);

		// Unlock "PixelBuffer" and free the temp memory after the texture upload.
		PixelBuffer->Unlock();

		// No need to restore texture stage; leave it like this,
		// and the next draw will take care of cleaning it up; or
		// next operation that needs the stage will switch something else in on it.

		CachedBindPixelUnpackBuffer(0);

		return;
	}
#endif
	
	if ( !bUseClientStorage && FOpenGL::SupportsPixelBuffers() )
	{
		// Code path for PBO per slice
		check(IsValidRef(PixelBuffers[BufferIndex]));
			
		PixelBuffer->Unlock();

		// Modify permission?
		if (!PixelBuffer->IsLockReadOnly())
		{
			// Use a texture stage that's not likely to be used for draws, to avoid waiting
			FOpenGLContextState& ContextState = OpenGLRHI->GetContextStateForCurrentContext();
			OpenGLRHI->CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Target, Resource, -1, this->GetNumMips());

			if( this->GetSizeZ() )
			{
				// texture 2D array
				if (GLFormat.bCompressed)
				{
					FOpenGL::CompressedTexSubImage3D(
						Target,
						MipIndex,
						0,
						0,
						ArrayIndex,
						FMath::Max<uint32>(1,(this->GetSizeX() >> MipIndex)),
						FMath::Max<uint32>(1,(this->GetSizeY() >> MipIndex)),
						1,
						GLFormat.InternalFormat[bSRGB],
						PixelBuffer->GetSize(),
						0);
				}
				else
				{
					glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
					check( FOpenGL::SupportsTexture3D() );
					FOpenGL::TexSubImage3D(
						Target,
						MipIndex,
						0,
						0,
						ArrayIndex,
						FMath::Max<uint32>(1,(this->GetSizeX() >> MipIndex)),
						FMath::Max<uint32>(1,(this->GetSizeY() >> MipIndex)),
						1,
						GLFormat.Format,
						GLFormat.Type,
						0);	// offset into PBO
					glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
				}
			}
			else
			{
				if (GLFormat.bCompressed)
				{
					if (GetAllocatedStorageForMip(MipIndex,ArrayIndex))
					{
						glCompressedTexSubImage2D(
							bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
							MipIndex,
							0,
							0,
							FMath::Max<uint32>(1,(this->GetSizeX() >> MipIndex)),
							FMath::Max<uint32>(1,(this->GetSizeY() >> MipIndex)),
							GLFormat.InternalFormat[bSRGB],
							PixelBuffer->GetSize(),
							0);	// offset into PBO
					}
					else
					{
						glCompressedTexImage2D(
							bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
							MipIndex,
							GLFormat.InternalFormat[bSRGB],
							FMath::Max<uint32>(1,(this->GetSizeX() >> MipIndex)),
							FMath::Max<uint32>(1,(this->GetSizeY() >> MipIndex)),
							0,
							PixelBuffer->GetSize(),
							0);	// offset into PBO
						SetAllocatedStorageForMip(MipIndex,ArrayIndex);
					}
				}
				else
				{
					// All construction paths should have called TexStorage2D or TexImage2D. So we will
					// always call TexSubImage2D.
					check(GetAllocatedStorageForMip(MipIndex,ArrayIndex) == true);
					glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
					glTexSubImage2D(
						bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
						MipIndex,
						0,
						0,
						FMath::Max<uint32>(1,(this->GetSizeX() >> MipIndex)),
						FMath::Max<uint32>(1,(this->GetSizeY() >> MipIndex)),
						GLFormat.Format,
						GLFormat.Type,
						0);	// offset into PBO
					glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
				}
			}
		}

		//need to free PBO if we aren't keeping shadow copies
		PixelBuffers[BufferIndex] = NULL;
	}
	else if(!bUseClientStorage || !ClientStorageBuffers[BufferIndex].bReadOnly)
	{
		// Code path for non-PBO:
		// Volume/array textures are currently only supported if PixelBufferObjects are also supported.
		check(this->GetSizeZ() == 0);

		// Use a texture stage that's not likely to be used for draws, to avoid waiting
		FOpenGLContextState& ContextState = OpenGLRHI->GetContextStateForCurrentContext();
		OpenGLRHI->CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Target, Resource, -1, this->GetNumMips());

		CachedBindPixelUnpackBuffer( 0 );
		
		uint32 LockedSize = 0;
		void* LockedBuffer = nullptr;
		
		if(bUseClientStorage)
		{
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);
			LockedSize = ClientStorageBuffers[BufferIndex].Size;
			LockedBuffer = ClientStorageBuffers[BufferIndex].Data;
		}
		else
		{
			LockedSize = PixelBuffer->GetSize();
			LockedBuffer = PixelBuffer->GetLockedBuffer();
		}
		if (GLFormat.bCompressed)
		{
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			if (GetAllocatedStorageForMip(MipIndex,ArrayIndex))
			{
				glCompressedTexSubImage2D(
					bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
					MipIndex,
					0,
					0,
					FMath::Max<uint32>(1,(this->GetSizeX() >> MipIndex)),
					FMath::Max<uint32>(1,(this->GetSizeY() >> MipIndex)),
					GLFormat.InternalFormat[bSRGB],
					LockedSize,
					LockedBuffer);
			}
			else
			{
				glCompressedTexImage2D(
					bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
					MipIndex,
					GLFormat.InternalFormat[bSRGB],
					FMath::Max<uint32>(1,(this->GetSizeX() >> MipIndex)),
					FMath::Max<uint32>(1,(this->GetSizeY() >> MipIndex)),
					0,
					LockedSize,
					LockedBuffer);
					SetAllocatedStorageForMip(MipIndex,ArrayIndex);
			}
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		}
		else
		{
			if (GetAllocatedStorageForMip(MipIndex,ArrayIndex))
			{
				glTexSubImage2D(
					bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
					MipIndex,
					0,
					0,
					FMath::Max<uint32>(1,(this->GetSizeX() >> MipIndex)),
					FMath::Max<uint32>(1,(this->GetSizeY() >> MipIndex)),
					GLFormat.Format,
					GLFormat.Type,
					LockedBuffer);
			}
			else
			{
				glTexImage2D(
					bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
					MipIndex,
					GLFormat.InternalFormat[bSRGB],
					FMath::Max<uint32>(1,(this->GetSizeX() >> MipIndex)),
					FMath::Max<uint32>(1,(this->GetSizeY() >> MipIndex)),
					0,
					GLFormat.Format,
					GLFormat.Type,
					LockedBuffer);
				SetAllocatedStorageForMip(MipIndex,ArrayIndex);
			}
		}
		if(bUseClientStorage)
		{
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
			glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_FALSE);
		}
		else
		{
			// Unlock "PixelBuffer" and free the temp memory after the texture upload.
			PixelBuffer->Unlock();
		}
	}

	// No need to restore texture stage; leave it like this,
	// and the next draw will take care of cleaning it up; or
	// next operation that needs the stage will switch something else in on it.

	CachedBindPixelUnpackBuffer(0);
}

template<typename RHIResourceType>
void TOpenGLTexture<RHIResourceType>::CloneViaCopyImage( TOpenGLTexture<RHIResourceType>* Src, uint32 InNumMips, int32 SrcOffset, int32 DstOffset)
{
	VERIFY_GL_SCOPE();
	
	check(FOpenGL::SupportsCopyImage());
	
	for (uint32 ArrayIndex = 0; ArrayIndex < this->GetEffectiveSizeZ(); ArrayIndex++)
	{
		// use the Copy Image functionality to copy mip level by mip level
		for(uint32 MipIndex = 0;MipIndex < InNumMips;++MipIndex)
		{
			// Calculate the dimensions of the mip-map.
			const uint32 DstMipIndex = MipIndex + DstOffset;
			const uint32 SrcMipIndex = MipIndex + SrcOffset;
			const uint32 MipSizeX = FMath::Max(this->GetSizeX() >> DstMipIndex,uint32(1));
			const uint32 MipSizeY = FMath::Max(this->GetSizeY() >> DstMipIndex,uint32(1));
			
			if(FOpenGL::AmdWorkaround() && ((MipSizeX < 4) || (MipSizeY < 4))) break;

			// copy the texture data
			FOpenGL::CopyImageSubData( Src->Resource, Src->Target, SrcMipIndex, 0, 0, ArrayIndex,
									  Resource, Target, DstMipIndex, 0, 0, ArrayIndex, MipSizeX, MipSizeY, 1);
		}
	}
	
}

template<typename RHIResourceType>
void TOpenGLTexture<RHIResourceType>::CloneViaPBO( TOpenGLTexture<RHIResourceType>* Src, uint32 InNumMips, int32 SrcOffset, int32 DstOffset)
{
	VERIFY_GL_SCOPE();
	
	// apparently it's not possible to retrieve compressed image from GL_TEXTURE_2D_ARRAY in OpenGL for compressed images
	// and for uncompressed ones it's not possible to specify the image index
	check(this->GetSizeZ() == 0);
	
	// only PBO path is supported here
	check( FOpenGL::SupportsPixelBuffers() );
	
	EPixelFormat PixelFormat = this->GetFormat();
	check(PixelFormat == Src->GetFormat());

	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[PixelFormat];
	const bool bSRGB = (this->GetFlags() & TexCreate_SRGB) != 0;
	check(bSRGB == ((Src->GetFlags() & TexCreate_SRGB) != 0));
	
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	
	FOpenGLContextState& ContextState = OpenGLRHI->GetContextStateForCurrentContext();
	
	for (uint32 ArrayIndex = 0; ArrayIndex < this->GetEffectiveSizeZ(); ArrayIndex++)
	{
		// use PBO functionality to copy mip level by mip level
		for(uint32 MipIndex = 0;MipIndex < InNumMips;++MipIndex)
		{
			// Actual mip levels
			const uint32 DstMipIndex = MipIndex + DstOffset;
			const uint32 SrcMipIndex = MipIndex + SrcOffset;
			
			// Calculate the dimensions of the mip-map.
			const uint32 MipSizeX = FMath::Max(this->GetSizeX() >> DstMipIndex,1u);
			const uint32 MipSizeY = FMath::Max(this->GetSizeY() >> DstMipIndex,1u);
			
			// Then the rounded PBO size required to capture this mip
			const uint32 DataSizeX = FMath::Max(MipSizeX,BlockSizeX);
			const uint32 DataSizeY = FMath::Max(MipSizeY,BlockSizeY);
			uint32 NumBlocksX = (DataSizeX + BlockSizeX - 1) / BlockSizeX;
			uint32 NumBlocksY = (DataSizeY + BlockSizeY - 1) / BlockSizeY;
			if ( PixelFormat == PF_PVRTC2 || PixelFormat == PF_PVRTC4 )
			{
				// PVRTC has minimum 2 blocks width and height
				NumBlocksX = FMath::Max<uint32>(NumBlocksX, 2);
				NumBlocksY = FMath::Max<uint32>(NumBlocksY, 2);
			}

			const uint32 MipBytes = NumBlocksX * NumBlocksY * BlockBytes;
			const int32 BufferIndex = DstMipIndex * (bCubemap ? 6 : 1) * this->GetEffectiveSizeZ() + ArrayIndex;
			const int32 SrcBufferIndex = SrcMipIndex * (Src->bCubemap ? 6 : 1) * Src->GetEffectiveSizeZ() + ArrayIndex;
			
			// Standard path with a PBO mirroring ever slice of a texture to allow multiple simulataneous maps
			if (!IsValidRef(PixelBuffers[BufferIndex]))
			{
				PixelBuffers[BufferIndex] = new FOpenGLPixelBuffer(0, MipBytes, BUF_Dynamic);
			}
			
			TRefCountPtr<FOpenGLPixelBuffer> PixelBuffer = PixelBuffers[BufferIndex];
			check(PixelBuffer->GetSize() == MipBytes);
			check(!PixelBuffer->IsLocked());
			
			// Transfer data from texture to pixel buffer.
			// This may be further optimized by caching information if surface content was changed since last lock.
			// Use a texture stage that's not likely to be used for draws, to avoid waiting
			OpenGLRHI->CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Src->Target, Src->Resource, -1, this->GetNumMips());
			
			glBindBuffer( GL_PIXEL_PACK_BUFFER, PixelBuffer->Resource );
			
#if PLATFORM_ANDROIDESDEFERRED // glReadPixels is async with PBOs - glGetTexImage is not: radr://16011763
			if(Attachment == GL_COLOR_ATTACHMENT0 && !GLFormat.bCompressed)
			{
				GLuint SourceFBO = Src->GetOpenGLFramebuffer(ArrayIndex, SrcMipIndex);
				check(SourceFBO > 0);
				glBindFramebuffer(UGL_READ_FRAMEBUFFER, SourceFBO);
				FOpenGL::ReadBuffer(Attachment);
				glPixelStorei(GL_PACK_ALIGNMENT, 1);
				glReadPixels(0, 0, MipSizeX, MipSizeY, GLFormat.Format, GLFormat.Type, 0 );
				glPixelStorei(GL_PACK_ALIGNMENT, 4);
				ContextState.Framebuffer = (GLuint)-1;
			}
			else
#endif
			if (GLFormat.bCompressed)
			{
				FOpenGL::GetCompressedTexImage(Src->bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Src->Target,
											   SrcMipIndex,
											   0);	// offset into PBO
			}
			else
			{
				glPixelStorei(GL_PACK_ALIGNMENT, 1);
				FOpenGL::GetTexImage(Src->bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Src->Target,
									 SrcMipIndex,
									 GLFormat.Format,
									 GLFormat.Type,
									 0);	// offset into PBO
				glPixelStorei(GL_PACK_ALIGNMENT, 4);
			}
			
			// copy the texture data
			// Upload directly into Dst to avoid out-of-band synchronization caused by glMapBuffer!
			{
				CachedBindPixelUnpackBuffer( PixelBuffer->Resource );
				
				// Use a texture stage that's not likely to be used for draws, to avoid waiting
				OpenGLRHI->CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Target, Resource, -1, this->GetNumMips());
				
				if( this->GetSizeZ() )
				{
					// texture 2D array
					if (GLFormat.bCompressed)
					{
						FOpenGL::CompressedTexSubImage3D(Target,
														 DstMipIndex,
														 0,
														 0,
														 ArrayIndex,
														 MipSizeX,
														 MipSizeY,
														 1,
														 GLFormat.InternalFormat[bSRGB],
														 PixelBuffer->GetSize(),
														 0);
					}
					else
					{
						glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
						check( FOpenGL::SupportsTexture3D() );
						FOpenGL::TexSubImage3D(Target,
											   DstMipIndex,
											   0,
											   0,
											   ArrayIndex,
											   MipSizeX,
											   MipSizeY,
											   1,
											   GLFormat.Format,
											   GLFormat.Type,
											   0);	// offset into PBO
						glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
					}
				}
				else
				{
					if (GLFormat.bCompressed)
					{
						if (GetAllocatedStorageForMip(DstMipIndex,ArrayIndex))
						{
							glCompressedTexSubImage2D(bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
													  DstMipIndex,
													  0,
													  0,
													  MipSizeX,
													  MipSizeY,
													  GLFormat.InternalFormat[bSRGB],
													  PixelBuffer->GetSize(),
													  0);	// offset into PBO
						}
						else
						{
							glCompressedTexImage2D(bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
												   DstMipIndex,
												   GLFormat.InternalFormat[bSRGB],
												   MipSizeX,
												   MipSizeY,
												   0,
												   PixelBuffer->GetSize(),
												   0);	// offset into PBO
							SetAllocatedStorageForMip(DstMipIndex,ArrayIndex);
						}
					}
					else
					{
						// All construction paths should have called TexStorage2D or TexImage2D. So we will
						// always call TexSubImage2D.
						check(GetAllocatedStorageForMip(DstMipIndex,ArrayIndex) == true);
						glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
						glTexSubImage2D(bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
										DstMipIndex,
										0,
										0,
										MipSizeX,
										MipSizeY,
										GLFormat.Format,
										GLFormat.Type,
										0);	// offset into PBO
						glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
					}
				}
			}
			
			// need to free PBO if we aren't keeping shadow copies
			PixelBuffers[BufferIndex] = NULL;
			
			// No need to restore texture stage; leave it like this,
			// and the next draw will take care of cleaning it up; or
			// next operation that needs the stage will switch something else in on it.
		}
	}
	
	// Reset the buffer bindings on exit only
	glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
	CachedBindPixelUnpackBuffer(0);
}


/*-----------------------------------------------------------------------------
	2D texture support.
-----------------------------------------------------------------------------*/

/**
* Creates a 2D RHI texture resource
* @param SizeX - width of the texture to create
* @param SizeY - height of the texture to create
* @param Format - EPixelFormat texture format
* @param NumMips - number of mips to generate or 0 for full mip pyramid
* @param Flags - ETextureCreateFlags creation flags
*/
FTexture2DRHIRef FOpenGLDynamicRHI::RHICreateTexture2D(uint32 SizeX,uint32 SizeY,uint8 Format,uint32 NumMips,uint32 NumSamples,uint32 Flags,FRHIResourceCreateInfo& Info)
{
	return (FRHITexture2D*)CreateOpenGLTexture(SizeX,SizeY,false,false,false,Format,NumMips,NumSamples,1,Flags,Info.ClearValueBinding,Info.BulkData);
}

/**
* Creates a 2D RHI texture external resource
* @param SizeX - width of the texture to create
* @param SizeY - height of the texture to create
* @param Format - EPixelFormat texture format
* @param NumMips - number of mips to generate or 0 for full mip pyramid
* @param Flags - ETextureCreateFlags creation flags
*/
FTexture2DRHIRef FOpenGLDynamicRHI::RHICreateTextureExternal2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 NumSamples, uint32 Flags, FRHIResourceCreateInfo& Info)
{
	return (FRHITexture2D*)CreateOpenGLTexture(SizeX, SizeY, false, false, true, Format, NumMips, NumSamples, 1, Flags, Info.ClearValueBinding, Info.BulkData);
}

FTexture2DRHIRef FOpenGLDynamicRHI::RHIAsyncCreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, uint32 Flags, void** InitialMipData, uint32 NumInitialMips)
{
	check(0);
	return FTexture2DRHIRef();
}

void FOpenGLDynamicRHI::RHICopySharedMips(FTexture2DRHIParamRef DestTexture2D,FTexture2DRHIParamRef SrcTexture2D)
{
	check(0);
}

FTexture2DArrayRHIRef FOpenGLDynamicRHI::RHICreateTexture2DArray(uint32 SizeX,uint32 SizeY,uint32 SizeZ,uint8 Format,uint32 NumMips,uint32 Flags, FRHIResourceCreateInfo& Info)
{
	VERIFY_GL_SCOPE();

	SCOPE_CYCLE_COUNTER(STAT_OpenGLCreateTextureTime);

	check( FOpenGL::SupportsTexture3D() );

	if(NumMips == 0)
	{
		NumMips = FindMaxMipmapLevel(SizeX, SizeY);
	}

	if (GMaxRHIFeatureLevel == ERHIFeatureLevel::ES2)
	{
		// Remove sRGB read flag when not supported
		Flags &= ~TexCreate_SRGB;
	}

	GLuint TextureID = 0;
	FOpenGL::GenTextures(1, &TextureID);

	const GLenum Target = GL_TEXTURE_2D_ARRAY;

	// Use a texture stage that's not likely to be used for draws, to avoid waiting
	FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
	CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Target, TextureID, 0, NumMips);

	glTexParameteri(Target, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(Target, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(Target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(Target, GL_TEXTURE_MIN_FILTER, NumMips > 1 ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST);
	if( FOpenGL::SupportsTextureFilterAnisotropic() )
	{
		glTexParameteri(Target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
	}
	glTexParameteri(Target, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(Target, GL_TEXTURE_MAX_LEVEL, NumMips - 1);
	
	TextureMipLimits.Add(TextureID, TPair<GLenum, GLenum>(0, NumMips - 1));

	const bool bSRGB = (Flags&TexCreate_SRGB) != 0;
	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[Format];
	if (GLFormat.InternalFormat[bSRGB] == GL_NONE)
	{
		UE_LOG(LogRHI, Fatal,TEXT("Texture format '%s' not supported."), GPixelFormats[Format].Name);
	}

	checkf(!GLFormat.bCompressed, TEXT("%s compressed 2D texture arrays not currently supported by the OpenGL RHI"), GPixelFormats[Format].Name);

	// Make sure PBO is disabled
	CachedBindPixelUnpackBuffer(ContextState, 0);

	uint8* Data = Info.BulkData ? (uint8*)Info.BulkData->GetResourceBulkData() : NULL;
	uint32 MipOffset = 0;

	FOpenGL::TexStorage3D( Target, NumMips, GLFormat.InternalFormat[bSRGB], SizeX, SizeY, SizeZ, GLFormat.Format, GLFormat.Type );

	if (Data)
	{
		for(uint32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
		{
			FOpenGL::TexSubImage3D(
				/*Target=*/ Target,
				/*Level=*/ MipIndex,
				0,
				0,
				0,
				/*SizeX=*/ FMath::Max<uint32>(1,(SizeX >> MipIndex)),
				/*SizeY=*/ FMath::Max<uint32>(1,(SizeY >> MipIndex)),
				/*SizeZ=*/ SizeZ,
				/*Format=*/ GLFormat.Format,
				/*Type=*/ GLFormat.Type,
				/*Data=*/ &Data[MipOffset]
				);

			uint32 SysMemPitch      =  FMath::Max<uint32>(1,SizeX >> MipIndex) * GPixelFormats[Format].BlockBytes;
			uint32 SysMemSlicePitch =  FMath::Max<uint32>(1,SizeY >> MipIndex) * SysMemPitch;
			MipOffset               += SizeZ * SysMemSlicePitch;
		}

		Info.BulkData->Discard();
	}
	
	// Determine the attachment point for the texture.	
	GLenum Attachment = GL_NONE;
	if(Flags & TexCreate_RenderTargetable)
	{
		Attachment = GL_COLOR_ATTACHMENT0;
	}
	else if(Flags & TexCreate_DepthStencilTargetable)
	{
		Attachment = (FOpenGL::SupportsPackedDepthStencil() && Format == PF_DepthStencil) ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
	}
	else if(Flags & TexCreate_ResolveTargetable)
	{
		Attachment = (Format == PF_DepthStencil && FOpenGL::SupportsPackedDepthStencil())
			? GL_DEPTH_STENCIL_ATTACHMENT
			: ((Format == PF_ShadowDepth || Format == PF_D24)
			? GL_DEPTH_ATTACHMENT
			: GL_COLOR_ATTACHMENT0);
	}

	FOpenGLTexture2DArray* Texture = new FOpenGLTexture2DArray(this,TextureID,Target,Attachment,SizeX,SizeY,SizeZ,NumMips,1, 1, SizeZ, (EPixelFormat)Format,false,true,Flags,nullptr,Info.ClearValueBinding);
	OpenGLTextureAllocated( Texture, Flags );

	// No need to restore texture stage; leave it like this,
	// and the next draw will take care of cleaning it up; or
	// next operation that needs the stage will switch something else in on it.

	return Texture;
}

FTexture3DRHIRef FOpenGLDynamicRHI::RHICreateTexture3D(uint32 SizeX,uint32 SizeY,uint32 SizeZ,uint8 Format,uint32 NumMips,uint32 Flags,FRHIResourceCreateInfo& CreateInfo)
{
	VERIFY_GL_SCOPE();

	SCOPE_CYCLE_COUNTER(STAT_OpenGLCreateTextureTime);

	check( FOpenGL::SupportsTexture3D() );

	if(NumMips == 0)
	{
		NumMips = FindMaxMipmapLevel(SizeX, SizeY, SizeZ);
	}

	if (GMaxRHIFeatureLevel == ERHIFeatureLevel::ES2)
	{
		// Remove sRGB read flag when not supported
		Flags &= ~TexCreate_SRGB;
	}

	GLuint TextureID = 0;
	FOpenGL::GenTextures(1, &TextureID);

	const GLenum Target = GL_TEXTURE_3D;

	// Use a texture stage that's not likely to be used for draws, to avoid waiting
	FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
	CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Target, TextureID, 0, NumMips);

	glTexParameteri(Target, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(Target, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(Target, GL_TEXTURE_WRAP_R, GL_REPEAT);
	glTexParameteri(Target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(Target, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	if( FOpenGL::SupportsTextureFilterAnisotropic() )
	{
		glTexParameteri(Target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
	}
	glTexParameteri(Target, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(Target, GL_TEXTURE_MAX_LEVEL, NumMips - 1);
	
	TextureMipLimits.Add(TextureID, TPair<GLenum, GLenum>(0, NumMips - 1));

	const bool bSRGB = (Flags&TexCreate_SRGB) != 0;
	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[Format];
	if (GLFormat.InternalFormat[bSRGB] == GL_NONE)
	{
		UE_LOG(LogRHI, Fatal,TEXT("Texture format '%s' not supported."), GPixelFormats[Format].Name);
	}

	// Make sure PBO is disabled
	CachedBindPixelUnpackBuffer(ContextState,0);

	uint8* Data = CreateInfo.BulkData ? (uint8*)CreateInfo.BulkData->GetResourceBulkData() : NULL;
	uint32 MipOffset = 0;

	FOpenGL::TexStorage3D( Target, NumMips, GLFormat.InternalFormat[bSRGB], SizeX, SizeY, SizeZ, GLFormat.Format, GLFormat.Type );

	if (Data)
	{
		for(uint32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
		{
			FOpenGL::TexSubImage3D(
				/*Target=*/ Target,
				/*Level=*/ MipIndex,
				0,
				0,
				0,
				/*SizeX=*/ FMath::Max<uint32>(1,(SizeX >> MipIndex)),
				/*SizeY=*/ FMath::Max<uint32>(1,(SizeY >> MipIndex)),
				/*SizeZ=*/ FMath::Max<uint32>(1,(SizeZ >> MipIndex)),
				/*Format=*/ GLFormat.Format,
				/*Type=*/ GLFormat.Type,
				/*Data=*/ &Data[MipOffset]
				);

			uint32 SysMemPitch      =  FMath::Max<uint32>(1,SizeX >> MipIndex) * GPixelFormats[Format].BlockBytes;
			uint32 SysMemSlicePitch =  FMath::Max<uint32>(1,SizeY >> MipIndex) * SysMemPitch;
			MipOffset               += FMath::Max<uint32>(1,SizeZ >> MipIndex) * SysMemSlicePitch;
		}

		CreateInfo.BulkData->Discard();
	}
	
	// Determine the attachment point for the texture.	
	GLenum Attachment = GL_NONE;
	if(Flags & TexCreate_RenderTargetable)
	{
		Attachment = GL_COLOR_ATTACHMENT0;
	}
	else if(Flags & TexCreate_DepthStencilTargetable)
	{
		Attachment = Format == PF_DepthStencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
	}
	else if(Flags & TexCreate_ResolveTargetable)
	{
		Attachment = (Format == PF_DepthStencil && FOpenGL::SupportsCombinedDepthStencilAttachment())
			? GL_DEPTH_STENCIL_ATTACHMENT
			: ((Format == PF_ShadowDepth || Format == PF_D24)
			? GL_DEPTH_ATTACHMENT
			: GL_COLOR_ATTACHMENT0);
	}

	FOpenGLTexture3D* Texture = new FOpenGLTexture3D(this,TextureID,Target,Attachment,SizeX,SizeY,SizeZ,NumMips,1,1,1, (EPixelFormat)Format,false,true,Flags,nullptr,CreateInfo.ClearValueBinding);
	OpenGLTextureAllocated( Texture, Flags );

	// No need to restore texture stage; leave it like this,
	// and the next draw will take care of cleaning it up; or
	// next operation that needs the stage will switch something else in on it.

	return Texture;
}

void FOpenGLDynamicRHI::RHIGetResourceInfo(FTextureRHIParamRef Ref, FRHIResourceInfo& OutInfo)
{
}

FShaderResourceViewRHIRef FOpenGLDynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel)
{
	FOpenGLTexture2D* Texture2D = ResourceCast(Texture2DRHI);

	FOpenGLShaderResourceView *View = 0;

	if (FOpenGL::SupportsTextureView())
	{
		VERIFY_GL_SCOPE();

		GLuint Resource = 0;

		FOpenGL::GenTextures( 1, &Resource);
		const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[Texture2D->GetFormat()];
		const bool bSRGB = (Texture2D->GetFlags()&TexCreate_SRGB) != 0;
		
		FOpenGL::TextureView( Resource, Texture2D->Target, Texture2D->Resource, GLFormat.InternalFormat[bSRGB], MipLevel, 1, 0, 1);
		
		View = new FOpenGLShaderResourceView(this, Resource, Texture2D->Target, MipLevel, true);
	}
	else
	{
		View = new FOpenGLShaderResourceView(this, Texture2D->Resource, Texture2D->Target, MipLevel, false);
	}
	
	FShaderCache::LogSRV(View, Texture2DRHI, MipLevel, 1, PF_Unknown);

	return View;
}

FShaderResourceViewRHIRef FOpenGLDynamicRHI::RHICreateShaderResourceView(FTexture2DRHIParamRef Texture2DRHI, uint8 MipLevel, uint8 NumMipLevels, uint8 Format)
{
	FOpenGLTexture2D* Texture2D = ResourceCast(Texture2DRHI);

	FOpenGLShaderResourceView *View = 0;

	if (FOpenGL::SupportsTextureView())
	{
		VERIFY_GL_SCOPE();

		GLuint Resource = 0;

		FOpenGL::GenTextures( 1, &Resource);

		if (Format != PF_X24_G8)
		{
			const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[Format];
			const bool bSRGB = (Texture2D->GetFlags()&TexCreate_SRGB) != 0;
		
			FOpenGL::TextureView( Resource, Texture2D->Target, Texture2D->Resource, GLFormat.InternalFormat[bSRGB], MipLevel, NumMipLevels, 0, 1);
		}
		else
		{
			// PF_X24_G8 doesn't correspond to a real format under OpenGL
			// The solution is to create a view with the original format, and convert it to return the stencil index
			// To match component locations, texture swizzle needs to be setup too
			const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[Texture2D->GetFormat()];

			// create a second depth/stencil view
			FOpenGL::TextureView( Resource, Texture2D->Target, Texture2D->Resource, GLFormat.InternalFormat[0], MipLevel, NumMipLevels, 0, 1);

			// Use a texture stage that's not likely to be used for draws, to avoid waiting
			FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
			CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Texture2D->Target, Resource, 0, NumMipLevels);

			//set the texture to return the stencil index, and then force the components to match D3D
			glTexParameteri( Texture2D->Target, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX);
			glTexParameteri( Texture2D->Target, GL_TEXTURE_SWIZZLE_R, GL_ZERO);
			glTexParameteri( Texture2D->Target, GL_TEXTURE_SWIZZLE_G, GL_RED);
			glTexParameteri( Texture2D->Target, GL_TEXTURE_SWIZZLE_B, GL_ZERO);
			glTexParameteri( Texture2D->Target, GL_TEXTURE_SWIZZLE_A, GL_ZERO);
		}
		
		View = new FOpenGLShaderResourceView(this, Resource, Texture2D->Target, MipLevel, true);
	}
	else
	{
		uint32 const Target = Texture2D->Target;
		GLuint Resource = Texture2D->Resource;
		
		FTexture2DRHIParamRef DepthStencilTex = nullptr;
		
		// For stencil sampling we have to use a separate single channel texture to blit stencil data into
#if PLATFORM_DESKTOP || PLATFORM_ANDROIDESDEFERRED
		if (FOpenGL::GetFeatureLevel() >= ERHIFeatureLevel::SM4 && Format == PF_X24_G8 && FOpenGL::SupportsPixelBuffers())
		{
			check(NumMipLevels == 1 && MipLevel == 0);
			
			if (!Texture2D->SRVResource)
			{
				FOpenGL::GenTextures(1, &Texture2D->SRVResource);
				
				GLenum const InternalFormat = GL_R8UI;
				GLenum const ChannelFormat = GL_RED_INTEGER;
				uint32 const SizeX = Texture2D->GetSizeX();
				uint32 const SizeY = Texture2D->GetSizeY();
				GLenum const Type = GL_UNSIGNED_BYTE;
				uint32 const Flags = 0;
				
				FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
				CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Target, Texture2D->SRVResource, MipLevel, NumMipLevels);
				
				if (!FOpenGL::TexStorage2D(Target, NumMipLevels, InternalFormat, SizeX, SizeY, ChannelFormat, Type, Flags))
				{
					glTexImage2D(Target, 0, InternalFormat, SizeX, SizeY, 0, ChannelFormat, Type, nullptr);
				}
				
				TArray<uint8> ZeroData;
				ZeroData.AddZeroed(SizeX * SizeY);
				
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				glTexSubImage2D(
								Target,
								0,
								0,
								0,
								SizeX,
								SizeY,
								ChannelFormat,
								Type,
								ZeroData.GetData());
				glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
				
				//set the texture to return the stencil index, and then force the components to match D3D
				glTexParameteri( Target, GL_TEXTURE_SWIZZLE_R, GL_ZERO);
				glTexParameteri( Target, GL_TEXTURE_SWIZZLE_G, GL_RED);
				glTexParameteri( Target, GL_TEXTURE_SWIZZLE_B, GL_ZERO);
				glTexParameteri( Target, GL_TEXTURE_SWIZZLE_A, GL_ZERO);
			}
			check(Texture2D->SRVResource);
			
			Resource = Texture2D->SRVResource;
			DepthStencilTex = Texture2DRHI;
		}
#endif
		
		View = new FOpenGLShaderResourceView(this, Resource, Target, MipLevel, false);
		View->Texture2D = DepthStencilTex;
	}
	
	FShaderCache::LogSRV(View, Texture2DRHI, MipLevel, NumMipLevels, Format);

	return View;
}

FShaderResourceViewRHIRef FOpenGLDynamicRHI::RHICreateShaderResourceView(FTexture3DRHIParamRef Texture3DRHI, uint8 MipLevel)
{
	FOpenGLTexture3D* Texture3D = ResourceCast(Texture3DRHI);

	FOpenGLShaderResourceView *View = 0;

	if (FOpenGL::SupportsTextureView())
	{
		VERIFY_GL_SCOPE();

		GLuint Resource = 0;

		FOpenGL::GenTextures( 1, &Resource);
		const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[Texture3D->GetFormat()];
		const bool bSRGB = (Texture3D->GetFlags()&TexCreate_SRGB) != 0;
		
		FOpenGL::TextureView( Resource, Texture3D->Target, Texture3D->Resource, GLFormat.InternalFormat[bSRGB], MipLevel, 1, 0, 1);
		
		View = new FOpenGLShaderResourceView(this, Resource, Texture3D->Target, MipLevel, true);
	}
	else
	{
		View = new FOpenGLShaderResourceView(this, Texture3D->Resource, Texture3D->Target, MipLevel, false);
	}
	
	FShaderCache::LogSRV(View, Texture3DRHI, MipLevel, Texture3DRHI->GetNumMips(), Texture3DRHI->GetFormat());

	return View;
}

FShaderResourceViewRHIRef FOpenGLDynamicRHI::RHICreateShaderResourceView(FTexture2DArrayRHIParamRef Texture2DArrayRHI, uint8 MipLevel)
{
	FOpenGLTexture2DArray* Texture2DArray = ResourceCast(Texture2DArrayRHI);

	FOpenGLShaderResourceView *View = 0;

	if (FOpenGL::SupportsTextureView())
	{
		VERIFY_GL_SCOPE();

		GLuint Resource = 0;

		FOpenGL::GenTextures( 1, &Resource);
		const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[Texture2DArray->GetFormat()];
		const bool bSRGB = (Texture2DArray->GetFlags()&TexCreate_SRGB) != 0;
		
		FOpenGL::TextureView( Resource, Texture2DArray->Target, Texture2DArray->Resource, GLFormat.InternalFormat[bSRGB], MipLevel, 1, 0, 1);
		
		View = new FOpenGLShaderResourceView(this, Resource, Texture2DArray->Target, MipLevel, true);
	}
	else
	{
		View = new FOpenGLShaderResourceView(this, Texture2DArray->Resource, Texture2DArray->Target, MipLevel, false);
	}
	
	FShaderCache::LogSRV(View, Texture2DArrayRHI, MipLevel, Texture2DArrayRHI->GetNumMips(), Texture2DArrayRHI->GetFormat());

	return View;
}

FShaderResourceViewRHIRef FOpenGLDynamicRHI::RHICreateShaderResourceView(FTextureCubeRHIParamRef TextureCubeRHI, uint8 MipLevel)
{
	FOpenGLTextureCube* TextureCube = ResourceCast(TextureCubeRHI);

	FOpenGLShaderResourceView *View = 0;

	if (FOpenGL::SupportsTextureView())
	{
		VERIFY_GL_SCOPE();

		GLuint Resource = 0;

		FOpenGL::GenTextures( 1, &Resource);
		const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[TextureCube->GetFormat()];
		const bool bSRGB = (TextureCube->GetFlags()&TexCreate_SRGB) != 0;
		
		FOpenGL::TextureView( Resource, TextureCube->Target, TextureCube->Resource, GLFormat.InternalFormat[bSRGB], MipLevel, 1, 0, 6);
		
		View = new FOpenGLShaderResourceView(this, Resource, TextureCube->Target, MipLevel, true);
	}
	else
	{
		View = new FOpenGLShaderResourceView(this, TextureCube->Resource, TextureCube->Target, MipLevel, false);
	}
	
	FShaderCache::LogSRV(View, TextureCube, MipLevel, TextureCubeRHI->GetNumMips(), TextureCubeRHI->GetFormat());

	return View;
}


/** Generates mip maps for the surface. */
void FOpenGLDynamicRHI::RHIGenerateMips(FTextureRHIParamRef SurfaceRHI)
{
	VERIFY_GL_SCOPE();

	FOpenGLTextureBase* Texture = GetOpenGLTextureFromRHITexture(SurfaceRHI);

	if ( FOpenGL::SupportsGenerateMipmap())
	{
		GPUProfilingData.RegisterGPUWork(0);

		FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
		// Setup the texture on a disused unit
		// need to figure out how to setup mips properly in no views case
		CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Texture->Target, Texture->Resource, -1, 1);

		FOpenGL::GenerateMipmap( Texture->Target);
	}
	else
	{
		UE_LOG( LogRHI, Fatal, TEXT("Generate Mipmaps unsupported on this OpenGL version"));
	}
}



/**
 * Computes the size in memory required by a given texture.
 *
 * @param	TextureRHI		- Texture we want to know the size of
 * @return					- Size in Bytes
 */
uint32 FOpenGLDynamicRHI::RHIComputeMemorySize(FTextureRHIParamRef TextureRHI)
{
	if(!TextureRHI)
	{
		return 0;
	}

	FOpenGLTextureBase* Texture = GetOpenGLTextureFromRHITexture(TextureRHI);
	return Texture->GetMemorySize();
}

/**
 * Starts an asynchronous texture reallocation. It may complete immediately if the reallocation
 * could be performed without any reshuffling of texture memory, or if there isn't enough memory.
 * The specified status counter will be decremented by 1 when the reallocation is complete (success or failure).
 *
 * Returns a new reference to the texture, which will represent the new mip count when the reallocation is complete.
 * RHIGetAsyncReallocateTexture2DStatus() can be used to check the status of an ongoing or completed reallocation.
 *
 * @param Texture2D		- Texture to reallocate
 * @param NewMipCount	- New number of mip-levels
 * @param RequestStatus	- Will be decremented by 1 when the reallocation is complete (success or failure).
 * @return				- New reference to the texture, or an invalid reference upon failure
 */
FTexture2DRHIRef FOpenGLDynamicRHI::RHIAsyncReallocateTexture2D(FTexture2DRHIParamRef Texture2DRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	VERIFY_GL_SCOPE();

	FOpenGLTexture2D* Texture2D = ResourceCast(Texture2DRHI);

	// Allocate a new texture.
	FOpenGLTexture2D* NewTexture2D = (FOpenGLTexture2D*)CreateOpenGLTexture(NewSizeX,NewSizeY,false,false,false, Texture2D->GetFormat(),NewMipCount,1,1, Texture2D->GetFlags(), Texture2DRHI->GetClearBinding());
	
	const uint32 BlockSizeX = GPixelFormats[Texture2D->GetFormat()].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[Texture2D->GetFormat()].BlockSizeY;
	const uint32 NumBytesPerBlock = GPixelFormats[Texture2D->GetFormat()].BlockBytes;
	
	// Should we use client-storage to improve update time on platforms that require it
	const bool bCompressed = GOpenGLTextureFormats[Texture2D->GetFormat()].bCompressed;
	bool const bRenderable = (Texture2D->GetFlags() & (TexCreate_RenderTargetable|TexCreate_ResolveTargetable|TexCreate_DepthStencilTargetable|TexCreate_CPUReadback)) != 0;
	const bool bUseClientStorage = (FOpenGL::SupportsClientStorage() && !FOpenGL::SupportsTextureView() && !bRenderable && !bCompressed);
	
	// Use the GPU to asynchronously copy the old mip-maps into the new texture.
	const uint32 NumSharedMips = FMath::Min(Texture2D->GetNumMips(),NewTexture2D->GetNumMips());
	const uint32 SourceMipOffset = Texture2D->GetNumMips()    - NumSharedMips;
	const uint32 DestMipOffset   = NewTexture2D->GetNumMips() - NumSharedMips;

	if (FOpenGL::SupportsCopyImage())
	{
		FOpenGLTexture2D *NewOGLTexture2D = (FOpenGLTexture2D*)NewTexture2D;
		FOpenGLTexture2D *OGLTexture2D = (FOpenGLTexture2D*)Texture2D;
		NewOGLTexture2D->CloneViaCopyImage( OGLTexture2D, NumSharedMips, SourceMipOffset, DestMipOffset);
	}
	else if (FOpenGL::SupportsCopyTextureLevels())
	{
		FOpenGL::CopyTextureLevels( NewTexture2D->Resource, Texture2D->Resource, SourceMipOffset, NumSharedMips);
	}
	else if (FOpenGL::SupportsPixelBuffers() && !bUseClientStorage)
	{
		FOpenGLTexture2D *NewOGLTexture2D = (FOpenGLTexture2D*)NewTexture2D;
		FOpenGLTexture2D *OGLTexture2D = (FOpenGLTexture2D*)Texture2D;
		NewOGLTexture2D->CloneViaPBO( OGLTexture2D, NumSharedMips, SourceMipOffset, DestMipOffset);
	}
	else
	{
		for(uint32 MipIndex = 0;MipIndex < NumSharedMips;++MipIndex)
		{
			const uint32 MipSizeX = FMath::Max<uint32>(1,NewSizeX >> (MipIndex+DestMipOffset));
			const uint32 MipSizeY = FMath::Max<uint32>(1,NewSizeY >> (MipIndex+DestMipOffset));
			const uint32 NumBlocksX = AlignArbitrary(MipSizeX, BlockSizeX) / BlockSizeX;
			const uint32 NumBlocksY = AlignArbitrary(MipSizeY, BlockSizeY) / BlockSizeY;
			const uint32 NumMipBlocks = NumBlocksX * NumBlocksY;

			// Lock old and new texture.
			uint32 SrcStride;
			uint32 DestStride;

			void* Src = RHILockTexture2D( Texture2D, MipIndex + SourceMipOffset, RLM_ReadOnly, SrcStride, false );
			void* Dst = RHILockTexture2D( NewTexture2D, MipIndex + DestMipOffset, RLM_WriteOnly, DestStride, false );
			check(SrcStride == DestStride);
			FMemory::Memcpy( Dst, Src, NumMipBlocks * NumBytesPerBlock );
			RHIUnlockTexture2D( Texture2D, MipIndex + SourceMipOffset, false );
			RHIUnlockTexture2D( NewTexture2D, MipIndex + DestMipOffset, false );
		}
	}

	// Decrement the thread-safe counter used to track the completion of the reallocation, since D3D handles sequencing the
	// async mip copies with other D3D calls.
	RequestStatus->Decrement();

	return NewTexture2D;
}

/**
 * Returns the status of an ongoing or completed texture reallocation:
 *	TexRealloc_Succeeded	- The texture is ok, reallocation is not in progress.
 *	TexRealloc_Failed		- The texture is bad, reallocation is not in progress.
 *	TexRealloc_InProgress	- The texture is currently being reallocated async.
 *
 * @param Texture2D		- Texture to check the reallocation status for
 * @return				- Current reallocation status
 */
ETextureReallocationStatus FOpenGLDynamicRHI::RHIFinalizeAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted )
{
	return TexRealloc_Succeeded;
}

/**
 * Cancels an async reallocation for the specified texture.
 * This should be called for the new texture, not the original.
 *
 * @param Texture				Texture to cancel
 * @param bBlockUntilCompleted	If true, blocks until the cancellation is fully completed
 * @return						Reallocation status
 */
ETextureReallocationStatus FOpenGLDynamicRHI::RHICancelAsyncReallocateTexture2D( FTexture2DRHIParamRef Texture2D, bool bBlockUntilCompleted )
{
	return TexRealloc_Succeeded;
}

void* FOpenGLDynamicRHI::RHILockTexture2D(FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	FOpenGLTexture2D* Texture = ResourceCast(TextureRHI);
	return Texture->Lock(MipIndex,0,LockMode,DestStride);
}

void FOpenGLDynamicRHI::RHIUnlockTexture2D(FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,bool bLockWithinMiptail)
{
	FOpenGLTexture2D* Texture = ResourceCast(TextureRHI);
	Texture->Unlock(MipIndex, 0);
}

void* FOpenGLDynamicRHI::RHILockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,uint32 TextureIndex,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	FOpenGLTexture2DArray* Texture = ResourceCast(TextureRHI);
	return Texture->Lock(MipIndex,TextureIndex,LockMode,DestStride);
}

void FOpenGLDynamicRHI::RHIUnlockTexture2DArray(FTexture2DArrayRHIParamRef TextureRHI,uint32 TextureIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	FOpenGLTexture2DArray* Texture = ResourceCast(TextureRHI);
	Texture->Unlock(MipIndex, TextureIndex);
}

void FOpenGLDynamicRHI::RHIUpdateTexture2D(FTexture2DRHIParamRef TextureRHI,uint32 MipIndex,const FUpdateTextureRegion2D& UpdateRegion,uint32 SourcePitch,const uint8* SourceData)
{
	VERIFY_GL_SCOPE();

	FOpenGLTexture2D* Texture = ResourceCast(TextureRHI);

	// Use a texture stage that's not likely to be used for draws, to avoid waiting
	FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
	CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Texture->Target, Texture->Resource, 0, Texture->GetNumMips());
	CachedBindPixelUnpackBuffer(ContextState, 0);

	EPixelFormat PixelFormat = Texture->GetFormat();
	check(GPixelFormats[PixelFormat].BlockSizeX == 1);
	check(GPixelFormats[PixelFormat].BlockSizeY == 1);
	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[PixelFormat];
	const uint32 FormatBPP = GPixelFormats[PixelFormat].BlockBytes;
	checkf(!GLFormat.bCompressed, TEXT("RHIUpdateTexture2D not currently supported for compressed (%s) textures by the OpenGL RHI"), GPixelFormats[PixelFormat].Name);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, SourcePitch / FormatBPP);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexSubImage2D(Texture->Target, MipIndex, UpdateRegion.DestX, UpdateRegion.DestY, UpdateRegion.Width, UpdateRegion.Height,
		GLFormat.Format, GLFormat.Type, SourceData);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	// No need to restore texture stage; leave it like this,
	// and the next draw will take care of cleaning it up; or
	// next operation that needs the stage will switch something else in on it.
}

void FOpenGLDynamicRHI::RHIUpdateTexture3D(FTexture3DRHIParamRef TextureRHI,uint32 MipIndex,const FUpdateTextureRegion3D& UpdateRegion,uint32 SourceRowPitch,uint32 SourceDepthPitch,const uint8* SourceData)
{
	VERIFY_GL_SCOPE();
	check( FOpenGL::SupportsTexture3D() );
	FOpenGLTexture3D* Texture = ResourceCast(TextureRHI);

	// Use a texture stage that's not likely to be used for draws, to avoid waiting
	FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
	CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Texture->Target, Texture->Resource, 0, Texture->GetNumMips());
	CachedBindPixelUnpackBuffer(ContextState, 0);

	EPixelFormat PixelFormat = Texture->GetFormat();
	check(GPixelFormats[PixelFormat].BlockSizeX == 1);
	check(GPixelFormats[PixelFormat].BlockSizeY == 1);

	// TO DO - add appropriate offsets to source data when necessary
	check(UpdateRegion.SrcX == 0);
	check(UpdateRegion.SrcY == 0);
	check(UpdateRegion.SrcZ == 0);

	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[PixelFormat];
	const uint32 FormatBPP = GPixelFormats[PixelFormat].BlockBytes;
	checkf(!GLFormat.bCompressed, TEXT("RHIUpdateTexture3D not currently supported for compressed (%s) textures by the OpenGL RHI"), GPixelFormats[PixelFormat].Name);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, SourceRowPitch / FormatBPP);

	check( SourceDepthPitch % ( FormatBPP * UpdateRegion.Width ) == 0 );
	glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, SourceDepthPitch / UpdateRegion.Width / FormatBPP);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	FOpenGL::TexSubImage3D(Texture->Target, MipIndex, UpdateRegion.DestX, UpdateRegion.DestY, UpdateRegion.DestZ, UpdateRegion.Width, UpdateRegion.Height, UpdateRegion.Depth,
		GLFormat.Format, GLFormat.Type, SourceData);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	// No need to restore texture stage; leave it like this,
	// and the next draw will take care of cleaning it up; or
	// next operation that needs the stage will switch something else in on it.
}

void FOpenGLDynamicRHI::InvalidateTextureResourceInCache(GLuint Resource)
{
	for (int32 SamplerIndex = 0; SamplerIndex < FOpenGL::GetMaxCombinedTextureImageUnits(); ++SamplerIndex)
	{
		if (SharedContextState.Textures[SamplerIndex].Resource == Resource)
		{
			SharedContextState.Textures[SamplerIndex].Target = GL_NONE;
			SharedContextState.Textures[SamplerIndex].Resource = 0;
		}

		if (RenderingContextState.Textures[SamplerIndex].Resource == Resource)
		{
			RenderingContextState.Textures[SamplerIndex].Target = GL_NONE;
			RenderingContextState.Textures[SamplerIndex].Resource = 0;
		}
	}
	
	TextureMipLimits.Remove(Resource);
	
	if (PendingState.DepthStencil && PendingState.DepthStencil->Resource == Resource)
	{
		PendingState.DepthStencil = nullptr;
	}
}

void FOpenGLDynamicRHI::InvalidateUAVResourceInCache(GLuint Resource)
{
	for (int32 UAVIndex = 0; UAVIndex < OGL_MAX_COMPUTE_STAGE_UAV_UNITS; ++UAVIndex)
	{
		if (SharedContextState.UAVs[UAVIndex].Resource == Resource)
		{
			SharedContextState.UAVs[UAVIndex].Format = GL_NONE;
			SharedContextState.UAVs[UAVIndex].Resource = 0;
		}

		if (RenderingContextState.UAVs[UAVIndex].Resource == Resource)
		{
			RenderingContextState.UAVs[UAVIndex].Format = GL_NONE;
			RenderingContextState.UAVs[UAVIndex].Resource = 0;
		}
	}
}

/*-----------------------------------------------------------------------------
	Cubemap texture support.
-----------------------------------------------------------------------------*/
FTextureCubeRHIRef FOpenGLDynamicRHI::RHICreateTextureCube( uint32 Size, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo )
{
	// not yet supported
	check(!CreateInfo.BulkData);

	return (FRHITextureCube*)CreateOpenGLTexture(Size,Size,true, false, false, Format, NumMips, 1, 1, Flags, CreateInfo.ClearValueBinding);
}

FTextureCubeRHIRef FOpenGLDynamicRHI::RHICreateTextureCubeArray( uint32 Size, uint32 ArraySize, uint8 Format, uint32 NumMips, uint32 Flags, FRHIResourceCreateInfo& CreateInfo )
{
	// not yet supported
	check(!CreateInfo.BulkData);

	return (FRHITextureCube*)CreateOpenGLTexture(Size, Size, true, true, false, Format, NumMips, 1, 6 * ArraySize, Flags, CreateInfo.ClearValueBinding);
}

void* FOpenGLDynamicRHI::RHILockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,uint32 FaceIndex,uint32 ArrayIndex,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	FOpenGLTextureCube* TextureCube = ResourceCast(TextureCubeRHI);
	return TextureCube->Lock(MipIndex,FaceIndex + 6 * ArrayIndex,LockMode,DestStride);
}

void FOpenGLDynamicRHI::RHIUnlockTextureCubeFace(FTextureCubeRHIParamRef TextureCubeRHI,uint32 FaceIndex,uint32 ArrayIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	FOpenGLTextureCube* TextureCube = ResourceCast(TextureCubeRHI);
	TextureCube->Unlock(MipIndex,FaceIndex + ArrayIndex * 6);
}

void FOpenGLDynamicRHI::RHIBindDebugLabelName(FTextureRHIParamRef TextureRHI, const TCHAR* Name)
{
	FOpenGLTextureBase* Texture = GetOpenGLTextureFromRHITexture(TextureRHI);
	FOpenGL::LabelObject(GL_TEXTURE, Texture->Resource, TCHAR_TO_ANSI(Name));
}

void FOpenGLDynamicRHI::RHIVirtualTextureSetFirstMipInMemory(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
}

void FOpenGLDynamicRHI::RHIVirtualTextureSetFirstMipVisible(FTexture2DRHIParamRef TextureRHI, uint32 FirstMip)
{
}

FTextureReferenceRHIRef FOpenGLDynamicRHI::RHICreateTextureReference(FLastRenderTimeContainer* InLastRenderTime)
{
	return new FOpenGLTextureReference(InLastRenderTime);
}

void FOpenGLTextureReference::SetReferencedTexture(FRHITexture* InTexture)
{
	FRHITextureReference::SetReferencedTexture(InTexture);
	TexturePtr = GetOpenGLTextureFromRHITexture(InTexture);
}

void FOpenGLDynamicRHI::RHIUpdateTextureReference(FTextureReferenceRHIParamRef TextureRefRHI, FTextureRHIParamRef NewTextureRHI)
{
	auto* TextureRef = (FOpenGLTextureReference*)TextureRefRHI;
	if (TextureRef)
	{
		TextureRef->SetReferencedTexture(NewTextureRHI);
	}
}

FTexture2DRHIRef FOpenGLDynamicRHI::RHICreateTexture2DFromResource(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, uint32 NumSamplesTileMem, const FClearValueBinding& ClearValueBinding, GLuint Resource, uint32 TexCreateFlags)
{
	FOpenGLTexture2D* Texture2D = new FOpenGLTexture2D(
		this,
		Resource,
		(NumSamples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D,
		GL_NONE,
		SizeX,
		SizeY,
		0,
		NumMips,
		NumSamples,
		NumSamplesTileMem,
		1,
		Format,
		false,
		false,
		TexCreateFlags,
		nullptr,
		ClearValueBinding);

	OpenGLTextureAllocated(Texture2D, TexCreateFlags);
	return Texture2D;
}

FTexture2DRHIRef FOpenGLDynamicRHI::RHICreateTexture2DArrayFromResource(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, uint32 NumSamplesTileMem, const FClearValueBinding& ClearValueBinding, GLuint Resource, uint32 TexCreateFlags)
{
	FOpenGLTexture2D* Texture2DArray = new FOpenGLTexture2D(
		this,
		Resource,
		GL_TEXTURE_2D_ARRAY,
		GL_NONE,
		SizeX,
		SizeY,
		0,
		NumMips,
		NumSamples,
		NumSamplesTileMem,
		ArraySize,
		Format,
		false,
		false,
		TexCreateFlags,
		nullptr,
		ClearValueBinding);

	OpenGLTextureAllocated(Texture2DArray, TexCreateFlags);
	return Texture2DArray;
}

FTextureCubeRHIRef FOpenGLDynamicRHI::RHICreateTextureCubeFromResource(EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, uint32 NumSamplesTileMem, const FClearValueBinding& ClearValueBinding, GLuint Resource, uint32 TexCreateFlags)
{
	FOpenGLTextureCube* TextureCube = new FOpenGLTextureCube(
		this,
		Resource,
		GL_TEXTURE_CUBE_MAP,
		GL_NONE,
		Size,
		Size,
		0,
		NumMips,
		NumSamples,
		NumSamplesTileMem,
		1,
		Format,
		false,
		false,
		TexCreateFlags,
		nullptr,
		ClearValueBinding);

	OpenGLTextureAllocated(TextureCube, TexCreateFlags);
	return TextureCube;
}

void FOpenGLDynamicRHI::RHIAliasTextureResources(FTextureRHIParamRef DestRHITexture, FTextureRHIParamRef SrcRHITexture)
{
	FOpenGLTextureBase* DestTexture = GetOpenGLTextureFromRHITexture(DestRHITexture);
	FOpenGLTextureBase* SrcTexture = GetOpenGLTextureFromRHITexture(SrcRHITexture);

	if (DestTexture && SrcTexture)
	{
		DestTexture->AliasResources(SrcTexture);
	}
}
