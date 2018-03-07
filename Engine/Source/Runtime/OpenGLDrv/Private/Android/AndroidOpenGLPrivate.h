// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidOpenGLPrivate.h: Code shared betweeen AndroidOpenGL and AndroidESDeferredOpenGL
=============================================================================*/
#pragma once

#include "AndroidApplication.h"

bool GAndroidGPUInfoReady = false;

// call out to JNI to see if the application was packaged for GearVR
extern bool AndroidThunkCpp_IsGearVRApplication();

class FAndroidGPUInfo
{
public:
	static FAndroidGPUInfo& Get()
	{
		static FAndroidGPUInfo This;
		return This;
	}

	FString GPUFamily;
	FString GLVersion;
	bool bSupportsFloatingPointRenderTargets;
	bool bSupportsFrameBufferFetch;
	bool bSupportsShaderIOBlocks;
	bool bES30Support;
	TArray<FString> TargetPlatformNames;

private:
	FAndroidGPUInfo()
	{
		// this is only valid in the game thread, make sure we are initialized there before being called on other threads!
		check(IsInGameThread())

		// make sure GL is started so we can get the supported formats
		AndroidEGL* EGL = AndroidEGL::GetInstance();

		if (!EGL->IsInitialized())
		{
			FAndroidAppEntry::PlatformInit();
#if PLATFORM_ANDROIDESDEFERRED
			EGL->InitSurface(false, true);
#endif
		}
#if !PLATFORM_ANDROIDESDEFERRED
		// Do not create a window surface if the app is for GearVR (use small buffer)
		bool bCreateSurface = !AndroidThunkCpp_IsGearVRApplication();
		FPlatformMisc::LowLevelOutputDebugString(TEXT("FAndroidGPUInfo"));
		EGL->InitSurface(bCreateSurface, bCreateSurface);
#endif
		EGL->SetCurrentSharedContext();

		// get extensions
		// Process the extension caps directly here, as FOpenGL might not yet be setup
		// Do not process extensions here, because extension pointers may not be setup
		const ANSICHAR* GlGetStringOutput = (const ANSICHAR*) glGetString(GL_EXTENSIONS);
		FString ExtensionsString = GlGetStringOutput;

		GPUFamily = (const ANSICHAR*)glGetString(GL_RENDERER);
		check(!GPUFamily.IsEmpty());

		GLVersion = (const ANSICHAR*)glGetString(GL_VERSION);

		bES30Support = GLVersion.Contains(TEXT("OpenGL ES 3."));

#if PLATFORM_ANDROIDESDEFERRED
		TargetPlatformNames.Add(TEXT("Android_ESDEFERRED"));
#else
		// highest priority is the per-texture version
		if (ExtensionsString.Contains(TEXT("GL_KHR_texture_compression_astc_ldr")))
		{
			TargetPlatformNames.Add(TEXT("Android_ASTC"));
		}
		if (ExtensionsString.Contains(TEXT("GL_NV_texture_compression_s3tc")) || ExtensionsString.Contains(TEXT("GL_EXT_texture_compression_s3tc")))
		{
			TargetPlatformNames.Add(TEXT("Android_DXT"));
		}
		if (ExtensionsString.Contains(TEXT("GL_ATI_texture_compression_atitc")) || ExtensionsString.Contains(TEXT("GL_AMD_compressed_ATC_texture")))
		{
			TargetPlatformNames.Add(TEXT("Android_ATC"));
		}
		if (ExtensionsString.Contains(TEXT("GL_IMG_texture_compression_pvrtc")))
		{
			TargetPlatformNames.Add(TEXT("Android_PVRTC"));
		}
		if (bES30Support)
		{
			TargetPlatformNames.Add(TEXT("Android_ETC2"));
		}
		if (ExtensionsString.Contains(TEXT("GL_KHR_texture_compression_astc_ldr")))
		{
			TargetPlatformNames.Add(TEXT("Android_ASTC"));
		}

		// all devices support ETC
		TargetPlatformNames.Add(TEXT("Android_ETC1"));

		// finally, generic Android
		TargetPlatformNames.Add(TEXT("Android"));

#endif
		bSupportsFloatingPointRenderTargets = 
			ExtensionsString.Contains(TEXT("GL_EXT_color_buffer_half_float")) 
			// According to https://www.khronos.org/registry/gles/extensions/EXT/EXT_color_buffer_float.txt
			|| (bES30Support && ExtensionsString.Contains(TEXT("GL_EXT_color_buffer_float")));

		bSupportsFrameBufferFetch = ExtensionsString.Contains(TEXT("GL_EXT_shader_framebuffer_fetch")) || ExtensionsString.Contains(TEXT("GL_NV_shader_framebuffer_fetch")) 
			|| ExtensionsString.Contains(TEXT("GL_ARM_shader_framebuffer_fetch ")); // has space at the end to exclude GL_ARM_shader_framebuffer_fetch_depth_stencil match
		bSupportsShaderIOBlocks = ExtensionsString.Contains(TEXT("GL_EXT_shader_io_blocks"));

		GAndroidGPUInfoReady = true;
	}
};