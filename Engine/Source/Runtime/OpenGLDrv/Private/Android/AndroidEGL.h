// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidEGL.h: Private EGL definitions for Android-specific functionality
=============================================================================*/
#pragma once

#if PLATFORM_ANDROID

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

struct AndroidESPImpl;
struct ANativeWindow;


DECLARE_LOG_CATEGORY_EXTERN(LogEGL, Log, All);

struct FPlatformOpenGLContext
{
	EGLContext	eglContext;
	GLuint		ViewportFramebuffer;
	EGLSurface	eglSurface;
	GLuint		DefaultVertexArrayObject;

	FPlatformOpenGLContext()
	{
		Reset();
	}

	void Reset()
	{
		eglContext = EGL_NO_CONTEXT;
		eglSurface = EGL_NO_SURFACE;
		ViewportFramebuffer = 0;
		DefaultVertexArrayObject = 0;
	}
};


class AndroidEGL
{
public:
	enum APIVariant
	{
		AV_OpenGLES,
		AV_OpenGLCore
	};

	static AndroidEGL* GetInstance();
	~AndroidEGL();	

	bool IsInitialized();
	void InitBackBuffer();
	void DestroyBackBuffer();
	void Init( APIVariant API, uint32 MajorVersion, uint32 MinorVersion, bool bDebug);
	void ReInit();
	void UnBind();
	bool SwapBuffers(int32 SyncInterval);
	void Terminate();
	void InitSurface(bool bUseSmallSurface, bool bCreateWndSurface);

	void GetDimensions(uint32& OutWidth, uint32& OutHeight);
	
	EGLDisplay GetDisplay() const;
	ANativeWindow* GetNativeWindow() const;

	EGLContext CreateContext(EGLContext InSharedContext = EGL_NO_CONTEXT);
	int32 GetError();
	EGLBoolean SetCurrentContext(EGLContext InContext, EGLSurface InSurface);
	GLuint GetOnScreenColorRenderBuffer();
	GLuint GetResolveFrameBuffer();
	bool IsCurrentContextValid();
	EGLContext  GetCurrentContext(  );
	void SetCurrentSharedContext();
	void SetSharedContext();
	void SetSingleThreadRenderingContext();
	void SetMultithreadRenderingContext();
	void SetCurrentRenderingContext();
	uint32_t GetCurrentContextType();
	FPlatformOpenGLContext* GetRenderingContext();
	
protected:
	AndroidEGL();
	static AndroidEGL* Singleton ;

private:
	void InitEGL(APIVariant API);
	void TerminateEGL();

	void CreateEGLSurface(ANativeWindow* InWindow, bool bCreateWndSurface);
	void DestroySurface();

	bool InitContexts();
	void DestroyContext(EGLContext InContext);

	void ResetDisplay();

	AndroidESPImpl* PImplData;

	void ResetInternal();
	void LogConfigInfo(EGLConfig  EGLConfigInfo);

	bool bSupportsKHRCreateContext;
	bool bSupportsKHRSurfacelessContext;

	int *ContextAttributes;
};

#endif
