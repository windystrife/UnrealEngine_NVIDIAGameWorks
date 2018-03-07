// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NullRHI.h"
#include "Misc/CoreMisc.h"
#include "Containers/List.h"
#include "RenderResource.h"


FNullDynamicRHI::FNullDynamicRHI()
{
	GMaxRHIShaderPlatform = ShaderFormatToLegacyShaderPlatform(FName(FPlatformMisc::GetNullRHIShaderFormat()));
	GMaxTextureDimensions = 16384;
	GMaxTextureMipCount = FPlatformMath::CeilLogTwo(GMaxTextureDimensions) + 1;
	GMaxTextureMipCount = FPlatformMath::Min<int32>( MAX_TEXTURE_MIP_COUNT, GMaxTextureMipCount );
}


void FNullDynamicRHI::Init()
{
#if PLATFORM_WINDOWS
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2] = SP_PCD3D_ES2;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_PCD3D_ES3_1;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM4] = SP_PCD3D_SM4;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = SP_PCD3D_SM5;
#elif PLATFORM_MAC
    GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2] = SP_METAL_MACES2;
    GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_METAL_MACES3_1;
    GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM4] = SP_METAL_SM4;
    GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = SP_METAL_SM5;
#else
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2] = SP_OPENGL_PCES2;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_NumPlatforms;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM4] = SP_OPENGL_SM4;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = SP_OPENGL_SM5;
#endif
	
	check(!GIsRHIInitialized);

	// do not do this at least on dedicated server; clients with -NullRHI may need additional consideration
#if !WITH_EDITOR
	if (!IsRunningDedicatedServer())
#endif
	{
		// Notify all initialized FRenderResources that there's a valid RHI device to create their RHI resources for now.
		for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
		{
			ResourceIt->InitRHI();
		}
		// Dynamic resources can have dependencies on static resources (with uniform buffers) and must initialized last!
		for(TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList());ResourceIt;ResourceIt.Next())
		{
			ResourceIt->InitDynamicRHI();
		}
	}

	GIsRHIInitialized = true;
}


void FNullDynamicRHI::Shutdown()
{
}


/**
 * Return a shared large static buffer that can be used to return from any 
 * function that needs to return a valid pointer (but can be garbage data)
 */
void* FNullDynamicRHI::GetStaticBuffer()
{
#if !WITH_EDITOR
	static bool bLogOnce = false;

	if (!bLogOnce && (IsRunningDedicatedServer()))
	{
		UE_LOG(LogRHI, Log, TEXT("NullRHI preferably does not allocate memory on the server. Try to change the caller to avoid doing allocs in when FApp::ShouldUseNullRHI() is true."));
		bLogOnce = true;
	}
#endif

	static void* Buffer = nullptr;
	if (!Buffer)
	{
		// allocate an 64 meg buffer, should be big enough for any texture/surface
		Buffer = FMemory::Malloc(64 * 1024 * 1024);
	}

	return Buffer;
}


/** Value between 0-100 that determines the percentage of the vertical scan that is allowed to pass while still allowing us to swap when VSYNC'ed.
This is used to get the same behavior as the old *_OR_IMMEDIATE present modes. */
uint32 GPresentImmediateThreshold = 100;




// Suppress linker warning "warning LNK4221: no public symbols found; archive member will be inaccessible"
int32 NullRHILinkerHelper;
