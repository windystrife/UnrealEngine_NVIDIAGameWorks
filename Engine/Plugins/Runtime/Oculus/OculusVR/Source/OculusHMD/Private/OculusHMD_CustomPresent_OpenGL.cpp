// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_CustomPresent.h"
#include "OculusHMDPrivateRHI.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS_OPENGL
#include "OculusHMD.h"

#if PLATFORM_WINDOWS
#ifndef WINDOWS_PLATFORM_TYPES_GUARD
#include "AllowWindowsPlatformTypes.h"
#endif
#endif

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FCustomPresentGL
//-------------------------------------------------------------------------------------------------

class FOpenGLCustomPresent : public FCustomPresent
{
public:
	FOpenGLCustomPresent(FOculusHMD* InOculusHMD);

	// Implementation of FCustomPresent, called by Plugin itself
	virtual FTextureRHIRef CreateTexture_RenderThread(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, FClearValueBinding InBinding, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, ERHIResourceType InResourceType, ovrpTextureHandle InTexture, uint32 InTexCreateFlags) override;
	virtual void AliasTextureResources_RHIThread(FTextureRHIParamRef DestTexture, FTextureRHIParamRef SrcTexture) override;
};


FOpenGLCustomPresent::FOpenGLCustomPresent(FOculusHMD* InOculusHMD) :
	FCustomPresent(InOculusHMD, ovrpRenderAPI_OpenGL, PF_R8G8B8A8, true)
{
}


FTextureRHIRef FOpenGLCustomPresent::CreateTexture_RenderThread(uint32 InSizeX, uint32 InSizeY, EPixelFormat InFormat, FClearValueBinding InBinding, uint32 InNumMips, uint32 InNumSamples, uint32 InNumSamplesTileMem, ERHIResourceType InResourceType, ovrpTextureHandle InTexture, uint32 InTexCreateFlags)
{
	CheckInRenderThread();

	FOpenGLDynamicRHI* DynamicRHI = static_cast<FOpenGLDynamicRHI*>(GDynamicRHI);

	switch (InResourceType)
	{
	case RRT_Texture2D:
		return DynamicRHI->RHICreateTexture2DFromResource(InFormat, InSizeX, InSizeY, InNumMips, InNumSamples, InNumSamplesTileMem, InBinding, (GLuint) InTexture, InTexCreateFlags).GetReference();

	case RRT_Texture2DArray:
		return DynamicRHI->RHICreateTexture2DArrayFromResource(InFormat, InSizeX, InSizeY, 2, InNumMips, InNumSamples, InNumSamplesTileMem, InBinding, (GLuint) InTexture, InTexCreateFlags).GetReference();

	case RRT_TextureCube:
		return DynamicRHI->RHICreateTextureCubeFromResource(InFormat, InSizeX, false, 1, InNumMips, InNumSamples, InNumSamplesTileMem, InBinding, (GLuint) InTexture, InTexCreateFlags).GetReference();

	default:
		return nullptr;
	}
}


void FOpenGLCustomPresent::AliasTextureResources_RHIThread(FTextureRHIParamRef DestTexture, FTextureRHIParamRef SrcTexture)
{
	CheckInRHIThread();

	FOpenGLDynamicRHI* DynamicRHI = static_cast<FOpenGLDynamicRHI*>(GDynamicRHI);
	DynamicRHI->RHIAliasTextureResources(DestTexture, SrcTexture);
}


//-------------------------------------------------------------------------------------------------
// APIs
//-------------------------------------------------------------------------------------------------

FCustomPresent* CreateCustomPresent_OpenGL(FOculusHMD* InOculusHMD)
{
	return new FOpenGLCustomPresent(InOculusHMD);
}


} // namespace OculusHMD

#if PLATFORM_WINDOWS
#undef WINDOWS_PLATFORM_TYPES_GUARD
#endif

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS_OPENGL
