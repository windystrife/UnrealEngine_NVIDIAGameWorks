// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#if !PLATFORM_ANDROIDESDEFERRED

#include "OpenGLDrvPrivate.h"
#include "AndroidOpenGL.h"
#include "OpenGLDrvPrivate.h"
#include "OpenGLES2.h"
#include "AndroidWindow.h"
#include "AndroidOpenGLPrivate.h"

PFNEGLGETSYSTEMTIMENVPROC eglGetSystemTimeNV_p = NULL;
PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR_p = NULL;
PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR_p = NULL;
PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR_p = NULL;

// Occlusion Queries
PFNGLGENQUERIESEXTPROC 					glGenQueriesEXT = NULL;
PFNGLDELETEQUERIESEXTPROC 				glDeleteQueriesEXT = NULL;
PFNGLISQUERYEXTPROC 					glIsQueryEXT = NULL;
PFNGLBEGINQUERYEXTPROC 					glBeginQueryEXT = NULL;
PFNGLENDQUERYEXTPROC 					glEndQueryEXT = NULL;
PFNGLGETQUERYIVEXTPROC 					glGetQueryivEXT = NULL;
PFNGLGETQUERYOBJECTIVEXTPROC 			glGetQueryObjectivEXT = NULL;
PFNGLGETQUERYOBJECTUIVEXTPROC 			glGetQueryObjectuivEXT = NULL;

PFNGLQUERYCOUNTEREXTPROC				glQueryCounterEXT = NULL;
PFNGLGETQUERYOBJECTUI64VEXTPROC			glGetQueryObjectui64vEXT = NULL;

// Offscreen MSAA rendering
PFNBLITFRAMEBUFFERNVPROC				glBlitFramebufferNV = NULL;
PFNGLDISCARDFRAMEBUFFEREXTPROC			glDiscardFramebufferEXT = NULL;
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC	glFramebufferTexture2DMultisampleEXT = NULL;
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC	glRenderbufferStorageMultisampleEXT = NULL;

PFNGLPUSHGROUPMARKEREXTPROC				glPushGroupMarkerEXT = NULL;
PFNGLPOPGROUPMARKEREXTPROC				glPopGroupMarkerEXT = NULL;
PFNGLLABELOBJECTEXTPROC					glLabelObjectEXT = NULL;
PFNGLGETOBJECTLABELEXTPROC				glGetObjectLabelEXT = NULL;

PFNGLMAPBUFFEROESPROC					glMapBufferOESa = NULL;
PFNGLUNMAPBUFFEROESPROC					glUnmapBufferOESa = NULL;

PFNGLTEXSTORAGE2DPROC					glTexStorage2D = NULL;

// KHR_debug
PFNGLDEBUGMESSAGECONTROLKHRPROC			glDebugMessageControlKHR = NULL;
PFNGLDEBUGMESSAGEINSERTKHRPROC			glDebugMessageInsertKHR = NULL;
PFNGLDEBUGMESSAGECALLBACKKHRPROC		glDebugMessageCallbackKHR = NULL;
PFNGLGETDEBUGMESSAGELOGKHRPROC			glDebugMessageLogKHR = NULL;
PFNGLGETPOINTERVKHRPROC					glGetPointervKHR = NULL;
PFNGLPUSHDEBUGGROUPKHRPROC				glPushDebugGroupKHR = NULL;
PFNGLPOPDEBUGGROUPKHRPROC				glPopDebugGroupKHR = NULL;
PFNGLOBJECTLABELKHRPROC					glObjectLabelKHR = NULL;
PFNGLGETOBJECTLABELKHRPROC				glGetObjectLabelKHR = NULL;
PFNGLOBJECTPTRLABELKHRPROC				glObjectPtrLabelKHR = NULL;
PFNGLGETOBJECTPTRLABELKHRPROC			glGetObjectPtrLabelKHR = NULL;

PFNGLDRAWELEMENTSINSTANCEDPROC			glDrawElementsInstanced = NULL;
PFNGLDRAWARRAYSINSTANCEDPROC			glDrawArraysInstanced = NULL;
PFNGLVERTEXATTRIBDIVISORPROC			glVertexAttribDivisor = NULL;

PFNGLUNIFORM4UIVPROC					glUniform4uiv = NULL;
PFNGLTEXIMAGE3DPROC						glTexImage3D = NULL;
PFNGLTEXSUBIMAGE3DPROC					glTexSubImage3D = NULL;
PFNGLCOMPRESSEDTEXIMAGE3DPROC			glCompressedTexImage3D = NULL;
PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC		glCompressedTexSubImage3D = NULL;
PFNGLCOPYTEXSUBIMAGE3DPROC				glCopyTexSubImage3D = NULL;
PFNGLCLEARBUFFERFIPROC					glClearBufferfi = NULL;
PFNGLCLEARBUFFERFVPROC					glClearBufferfv = NULL;
PFNGLCLEARBUFFERIVPROC					glClearBufferiv = NULL;
PFNGLCLEARBUFFERUIVPROC					glClearBufferuiv = NULL;
PFNGLDRAWBUFFERSPROC					glDrawBuffers = NULL;
PFNGLTEXBUFFEREXTPROC					glTexBufferEXT = NULL;

PFNGLGETPROGRAMBINARYOESPROC            glGetProgramBinary = NULL;
PFNGLPROGRAMBINARYOESPROC               glProgramBinary = NULL;

PFNGLBINDBUFFERRANGEPROC				glBindBufferRange = NULL;
PFNGLBINDBUFFERBASEPROC					glBindBufferBase = NULL;
PFNGLGETUNIFORMBLOCKINDEXPROC			glGetUniformBlockIndex = NULL;
PFNGLUNIFORMBLOCKBINDINGPROC			glUniformBlockBinding = NULL;
PFNGLVERTEXATTRIBIPOINTERPROC			glVertexAttribIPointer = NULL;

PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC glFramebufferTextureMultiviewOVR = NULL;
PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC glFramebufferTextureMultisampleMultiviewOVR = NULL;

struct FPlatformOpenGLDevice
{

	void SetCurrentSharedContext();
	void SetCurrentRenderingContext();
	void SetCurrentNULLContext();

	FPlatformOpenGLDevice();
	~FPlatformOpenGLDevice();
	void Init();
	void LoadEXT();
	void Terminate();
	void ReInit();
};


FPlatformOpenGLDevice::~FPlatformOpenGLDevice()
{
	FAndroidAppEntry::ReleaseEGL();
}

FPlatformOpenGLDevice::FPlatformOpenGLDevice()
{
}

// call out to JNI to see if the application was packaged for GearVR
extern bool AndroidThunkCpp_IsGearVRApplication();

void FPlatformOpenGLDevice::Init()
{
	extern void InitDebugContext();

	FPlatformMisc::LowLevelOutputDebugString(TEXT("FPlatformOpenGLDevice:Init"));
	bool bCreateSurface = !AndroidThunkCpp_IsGearVRApplication();
	AndroidEGL::GetInstance()->InitSurface(false, bCreateSurface);
	PlatformRenderingContextSetup(this);

	LoadEXT();

	InitDefaultGLContextState();
	InitDebugContext();

	PlatformSharedContextSetup(this);
	InitDefaultGLContextState();
	InitDebugContext();

	AndroidEGL::GetInstance()->InitBackBuffer(); //can be done only after context is made current.
}

FPlatformOpenGLDevice* PlatformCreateOpenGLDevice()
{
	FPlatformOpenGLDevice* Device = new FPlatformOpenGLDevice();
	Device->Init();
	return Device;
}

bool PlatformCanEnableGPUCapture()
{
	return false;
}

void PlatformReleaseOpenGLContext(FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context)
{
}

void* PlatformGetWindow(FPlatformOpenGLContext* Context, void** AddParam)
{
	check(Context);

	return (void*)&Context->eglContext;
}

bool PlatformBlitToViewport( FPlatformOpenGLDevice* Device, const FOpenGLViewport& Viewport, uint32 BackbufferSizeX, uint32 BackbufferSizeY, bool bPresent,bool bLockToVsync, int32 SyncInterval )
{
	if (bPresent && Viewport.GetCustomPresent())
	{
		bPresent = Viewport.GetCustomPresent()->Present(SyncInterval);
	}
	if (bPresent)
	{
		AndroidEGL::GetInstance()->SwapBuffers(bLockToVsync ? SyncInterval : 0);
	}
	return bPresent;
}

void PlatformRenderingContextSetup(FPlatformOpenGLDevice* Device)
{
	Device->SetCurrentRenderingContext();
}

void PlatformFlushIfNeeded()
{
}

void PlatformRebindResources(FPlatformOpenGLDevice* Device)
{
}

void PlatformSharedContextSetup(FPlatformOpenGLDevice* Device)
{
	Device->SetCurrentSharedContext();
}

void PlatformNULLContextSetup()
{
	AndroidEGL::GetInstance()->SetCurrentContext(EGL_NO_CONTEXT, EGL_NO_SURFACE);
}

EOpenGLCurrentContext PlatformOpenGLCurrentContext(FPlatformOpenGLDevice* Device)
{
	return (EOpenGLCurrentContext)AndroidEGL::GetInstance()->GetCurrentContextType();
}

void PlatformRestoreDesktopDisplayMode()
{
}

bool PlatformInitOpenGL()
{
	check(!FAndroidMisc::ShouldUseVulkan());

	{
		// determine ES version. PlatformInitOpenGL happens before ProcessExtensions and therefore FAndroidOpenGL::bES31Support.
		FString SubVersion;
		const bool bES31Supported = FAndroidGPUInfo::Get().GLVersion.Split(TEXT("OpenGL ES 3."), nullptr, &SubVersion) && FCString::Atoi(*SubVersion) >= 1;
		static const auto CVarDisableES31 = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Android.DisableOpenGLES31Support"));

		bool bBuildForES31 = false;
		GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bBuildForES31"), bBuildForES31, GEngineIni);
		const bool bSupportsFloatingPointRTs = FAndroidMisc::SupportsFloatingPointRenderTargets();
		const bool bSupportsShaderIOBlocks = FAndroidMisc::SupportsShaderIOBlocks();

		if (bBuildForES31 && bES31Supported && bSupportsFloatingPointRTs && bSupportsShaderIOBlocks && CVarDisableES31->GetValueOnAnyThread() == 0)
		{
			FAndroidOpenGL::CurrentFeatureLevelSupport = FAndroidOpenGL::EFeatureLevelSupport::ES31;
			// shut down existing ES2 egl.
			UE_LOG(LogRHI, Log, TEXT("App is packaged for OpenGL ES 3.1 and an ES 3.1-capable device was detected. Reinitializing OpenGL ES with a 3.1 context."));
			FAndroidAppEntry::ReleaseEGL();
			// Re-init gles for 3.1
			AndroidEGL::GetInstance()->Init(AndroidEGL::AV_OpenGLES, 3, 1, false);
		}
		else
		{
			FAndroidOpenGL::CurrentFeatureLevelSupport = FAndroidOpenGL::EFeatureLevelSupport::ES2;
			if (bBuildForES31)
			{
				UE_LOG(LogRHI, Log, TEXT("App is packaged for OpenGL ES 3.1 but device has not met all the requirements for ES 3.1 :"));
				if (CVarDisableES31->GetValueOnAnyThread())
				{
					UE_LOG(LogRHI, Log, TEXT("	ES 3.1 support was disabled via r.Android.DisableOpenGLES31Support"));
				}

				UE_LOG(LogRHI, Log, TEXT("	Device has OpenGL ES 3.1 support: %s"), bES31Supported ? TEXT("YES") : TEXT("NO"));
				UE_LOG(LogRHI, Log, TEXT("	Floating point render target support: %s"), bSupportsFloatingPointRTs ? TEXT("YES") : TEXT("NO"));
				UE_LOG(LogRHI, Log, TEXT("	EXT_shader_io_blocks support: %s"), bSupportsShaderIOBlocks ? TEXT("YES") : TEXT("NO"));
			}

			bool bBuildForES2 = false;
			GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("bBuildForES2"), bBuildForES2, GEngineIni);
			// If we're here and there's no ES2 data then we're in trouble.
			checkf(bBuildForES2, TEXT("This device only supports OpenGL ES 2 but the app was not packaged with ES2 support."));
		}
	}
	return true;
}

bool PlatformOpenGLContextValid()
{
	return AndroidEGL::GetInstance()->IsCurrentContextValid();
}

void PlatformGetBackbufferDimensions( uint32& OutWidth, uint32& OutHeight )
{
	AndroidEGL::GetInstance()->GetDimensions(OutWidth, OutHeight);
}

// =============================================================

void PlatformGetNewOcclusionQuery( GLuint* OutQuery, uint64* OutQueryContext )
{
}

bool PlatformContextIsCurrent( uint64 QueryContext )
{
	return true;
}

void FPlatformOpenGLDevice::LoadEXT()
{
	eglGetSystemTimeNV_p = (PFNEGLGETSYSTEMTIMENVPROC)((void*)eglGetProcAddress("eglGetSystemTimeNV"));
	eglCreateSyncKHR_p = (PFNEGLCREATESYNCKHRPROC)((void*)eglGetProcAddress("eglCreateSyncKHR"));
	eglDestroySyncKHR_p = (PFNEGLDESTROYSYNCKHRPROC)((void*)eglGetProcAddress("eglDestroySyncKHR"));
	eglClientWaitSyncKHR_p = (PFNEGLCLIENTWAITSYNCKHRPROC)((void*)eglGetProcAddress("eglClientWaitSyncKHR"));

	glDebugMessageControlKHR = (PFNGLDEBUGMESSAGECONTROLKHRPROC)((void*)eglGetProcAddress("glDebugMessageControlKHR"));

	// Some PowerVR drivers (Rogue Han and Intel-based devices) are crashing using glDebugMessageControlKHR (causes signal 11 crash)
	if (glDebugMessageControlKHR != NULL && FAndroidMisc::GetGPUFamily().Contains(TEXT("PowerVR")))
	{
		glDebugMessageControlKHR = NULL;
	}

	glDebugMessageInsertKHR = (PFNGLDEBUGMESSAGEINSERTKHRPROC)((void*)eglGetProcAddress("glDebugMessageInsertKHR"));
	glDebugMessageCallbackKHR = (PFNGLDEBUGMESSAGECALLBACKKHRPROC)((void*)eglGetProcAddress("glDebugMessageCallbackKHR"));
	glDebugMessageLogKHR = (PFNGLGETDEBUGMESSAGELOGKHRPROC)((void*)eglGetProcAddress("glDebugMessageLogKHR"));
	glGetPointervKHR = (PFNGLGETPOINTERVKHRPROC)((void*)eglGetProcAddress("glGetPointervKHR"));
	glPushDebugGroupKHR = (PFNGLPUSHDEBUGGROUPKHRPROC)((void*)eglGetProcAddress("glPushDebugGroupKHR"));
	glPopDebugGroupKHR = (PFNGLPOPDEBUGGROUPKHRPROC)((void*)eglGetProcAddress("glPopDebugGroupKHR"));
	glObjectLabelKHR = (PFNGLOBJECTLABELKHRPROC)((void*)eglGetProcAddress("glObjectLabelKHR"));
	glGetObjectLabelKHR = (PFNGLGETOBJECTLABELKHRPROC)((void*)eglGetProcAddress("glGetObjectLabelKHR"));
	glObjectPtrLabelKHR = (PFNGLOBJECTPTRLABELKHRPROC)((void*)eglGetProcAddress("glObjectPtrLabelKHR"));
	glGetObjectPtrLabelKHR = (PFNGLGETOBJECTPTRLABELKHRPROC)((void*)eglGetProcAddress("glGetObjectPtrLabelKHR"));

	glGetProgramBinary = (PFNGLGETPROGRAMBINARYOESPROC)((void*)eglGetProcAddress("glGetProgramBinaryOES"));
	glProgramBinary = (PFNGLPROGRAMBINARYOESPROC)((void*)eglGetProcAddress("glProgramBinaryOES"));
}

FPlatformOpenGLContext* PlatformCreateOpenGLContext(FPlatformOpenGLDevice* Device, void* InWindowHandle)
{
	//Assumes Device is already initialized and context already created.
	return AndroidEGL::GetInstance()->GetRenderingContext();
}

void PlatformDestroyOpenGLContext(FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context)
{
	delete Device; //created here, destroyed here, but held by RHI.
}

FRHITexture* PlatformCreateBuiltinBackBuffer(FOpenGLDynamicRHI* OpenGLRHI, uint32 SizeX, uint32 SizeY)
{
	uint32 Flags = TexCreate_RenderTargetable;
	FOpenGLTexture2D* Texture2D = new FOpenGLTexture2D(OpenGLRHI, AndroidEGL::GetInstance()->GetOnScreenColorRenderBuffer(), GL_RENDERBUFFER, GL_COLOR_ATTACHMENT0, SizeX, SizeY, 0, 1, 1, 1, 1, PF_B8G8R8A8, false, false, Flags, nullptr, FClearValueBinding::Transparent);
	OpenGLTextureAllocated(Texture2D, Flags);

	return Texture2D;
}

void PlatformResizeGLContext( FPlatformOpenGLDevice* Device, FPlatformOpenGLContext* Context, uint32 SizeX, uint32 SizeY, bool bFullscreen, bool bWasFullscreen, GLenum BackBufferTarget, GLuint BackBufferResource)
{
	check(Context);

	glViewport(0, 0, SizeX, SizeY);
	VERIFY_GL(glViewport);
}

void PlatformGetSupportedResolution(uint32 &Width, uint32 &Height)
{
}

bool PlatformGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
{
	return true;
}

int32 PlatformGlGetError()
{
	return glGetError();
}

// =============================================================

void PlatformReleaseOcclusionQuery( GLuint Query, uint64 QueryContext )
{
}

void FPlatformOpenGLDevice::SetCurrentSharedContext()
{
	AndroidEGL::GetInstance()->SetCurrentSharedContext();
}

void PlatformDestroyOpenGLDevice(FPlatformOpenGLDevice* Device)
{
	delete Device;
}

void FPlatformOpenGLDevice::SetCurrentRenderingContext()
{
	AndroidEGL::GetInstance()->SetCurrentRenderingContext();
}

void PlatformLabelObjects()
{
	// @todo: Check that there is a valid id (non-zero) as LabelObject will fail otherwise
	GLuint RenderBuffer = AndroidEGL::GetInstance()->GetOnScreenColorRenderBuffer();
	if (RenderBuffer != 0)
	{
		FOpenGL::LabelObject(GL_RENDERBUFFER, RenderBuffer, "OnScreenColorRB");
	}

	GLuint FrameBuffer = AndroidEGL::GetInstance()->GetResolveFrameBuffer();
	if (FrameBuffer != 0)
	{
		FOpenGL::LabelObject(GL_FRAMEBUFFER, FrameBuffer, "ResolveFB");
	}
}

//--------------------------------

void PlatformGetNewRenderQuery( GLuint* OutQuery, uint64* OutQueryContext )
{
	GLuint NewQuery = 0;
	FOpenGL::GenQueries( 1, &NewQuery );
	*OutQuery = NewQuery;
	*OutQueryContext = 0;
}

void PlatformReleaseRenderQuery( GLuint Query, uint64 QueryContext )
{
	FOpenGL::DeleteQueries(1, &Query );
}


bool FAndroidOpenGL::bUseHalfFloatTexStorage = false;
bool FAndroidOpenGL::bSupportsTextureBuffer = false;
bool FAndroidOpenGL::bUseES30ShadingLanguage = false;
bool FAndroidOpenGL::bES30Support = false;
bool FAndroidOpenGL::bES31Support = false;
bool FAndroidOpenGL::bSupportsInstancing = false;
bool FAndroidOpenGL::bHasHardwareHiddenSurfaceRemoval = false;
bool FAndroidOpenGL::bSupportsMobileMultiView = false;
bool FAndroidOpenGL::bSupportsImageExternal = false;
FAndroidOpenGL::EImageExternalType FAndroidOpenGL::ImageExternalType = FAndroidOpenGL::EImageExternalType::None;
GLint FAndroidOpenGL::MaxMSAASamplesTileMem = 1;

FAndroidOpenGL::EFeatureLevelSupport FAndroidOpenGL::CurrentFeatureLevelSupport = FAndroidOpenGL::EFeatureLevelSupport::Invalid;

void FAndroidOpenGL::ProcessExtensions(const FString& ExtensionsString)
{
	FOpenGLES2::ProcessExtensions(ExtensionsString);

	FString VersionString = FString(ANSI_TO_TCHAR((const ANSICHAR*)glGetString(GL_VERSION)));
	FString SubVersionString;
	bES30Support = VersionString.Split(TEXT("OpenGL ES 3."), nullptr, &SubVersionString);
	bES31Support = bES30Support && FCString::Atoi(*SubVersionString) >= 1;

	// Get procedures
	if (bSupportsOcclusionQueries || bSupportsDisjointTimeQueries)
	{
		glGenQueriesEXT        = (PFNGLGENQUERIESEXTPROC)       ((void*)eglGetProcAddress("glGenQueriesEXT"));
		glDeleteQueriesEXT     = (PFNGLDELETEQUERIESEXTPROC)    ((void*)eglGetProcAddress("glDeleteQueriesEXT"));
		glIsQueryEXT           = (PFNGLISQUERYEXTPROC)          ((void*)eglGetProcAddress("glIsQueryEXT"));
		glBeginQueryEXT        = (PFNGLBEGINQUERYEXTPROC)       ((void*)eglGetProcAddress("glBeginQueryEXT"));
		glEndQueryEXT          = (PFNGLENDQUERYEXTPROC)         ((void*)eglGetProcAddress("glEndQueryEXT"));
		glGetQueryivEXT        = (PFNGLGETQUERYIVEXTPROC)       ((void*)eglGetProcAddress("glGetQueryivEXT"));
		glGetQueryObjectivEXT  = (PFNGLGETQUERYOBJECTIVEXTPROC) ((void*)eglGetProcAddress("glGetQueryObjectivEXT"));
		glGetQueryObjectuivEXT = (PFNGLGETQUERYOBJECTUIVEXTPROC)((void*)eglGetProcAddress("glGetQueryObjectuivEXT"));
	}

	if (bSupportsDisjointTimeQueries)
	{
		glQueryCounterEXT			= (PFNGLQUERYCOUNTEREXTPROC)		((void*)eglGetProcAddress("glQueryCounterEXT"));
		glGetQueryObjectui64vEXT	= (PFNGLGETQUERYOBJECTUI64VEXTPROC)	((void*)eglGetProcAddress("glGetQueryObjectui64vEXT"));

		// If EXT_disjoint_timer_query wasn't found, NV_timer_query might be available
		if (glQueryCounterEXT == NULL)
		{
			glQueryCounterEXT = (PFNGLQUERYCOUNTEREXTPROC)eglGetProcAddress("glQueryCounterNV");
		}
		if (glGetQueryObjectui64vEXT == NULL)
		{
			glGetQueryObjectui64vEXT = (PFNGLGETQUERYOBJECTUI64VEXTPROC)eglGetProcAddress("glGetQueryObjectui64vNV");
		}
	}

	glDiscardFramebufferEXT = (PFNGLDISCARDFRAMEBUFFEREXTPROC)((void*)eglGetProcAddress("glDiscardFramebufferEXT"));
	glPushGroupMarkerEXT = (PFNGLPUSHGROUPMARKEREXTPROC)((void*)eglGetProcAddress("glPushGroupMarkerEXT"));
	glPopGroupMarkerEXT = (PFNGLPOPGROUPMARKEREXTPROC)((void*)eglGetProcAddress("glPopGroupMarkerEXT"));

	if (ExtensionsString.Contains(TEXT("GL_EXT_DEBUG_LABEL")))
	{
		glLabelObjectEXT = (PFNGLLABELOBJECTEXTPROC)((void*)eglGetProcAddress("glLabelObjectEXT"));
		glGetObjectLabelEXT = (PFNGLGETOBJECTLABELEXTPROC)((void*)eglGetProcAddress("glGetObjectLabelEXT"));
	}

	if (ExtensionsString.Contains(TEXT("GL_EXT_multisampled_render_to_texture")))
	{
		glFramebufferTexture2DMultisampleEXT = (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)((void*)eglGetProcAddress("glFramebufferTexture2DMultisampleEXT"));
		glRenderbufferStorageMultisampleEXT = (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)((void*)eglGetProcAddress("glRenderbufferStorageMultisampleEXT"));
		glGetIntegerv(GL_MAX_SAMPLES_EXT, &MaxMSAASamplesTileMem);
		MaxMSAASamplesTileMem = FMath::Max<GLint>(MaxMSAASamplesTileMem, 1);
		UE_LOG(LogRHI, Log, TEXT("Support for %dx MSAA detected"), MaxMSAASamplesTileMem);
	}
	else
	{
		// indicates RHI supports on-chip MSAA but this device does not.
		MaxMSAASamplesTileMem = 1;
	}

	bSupportsETC2 = bES30Support;
	bUseES30ShadingLanguage = bES30Support;

	FString RendererString = FString(ANSI_TO_TCHAR((const ANSICHAR*)glGetString(GL_RENDERER)));

	// Common GPU types
	const bool bIsNvidiaBased = RendererString.Contains(TEXT("NVIDIA"));
	const bool bIsPoverVRBased = RendererString.Contains(TEXT("PowerVR"));
	const bool bIsAdrenoBased = RendererString.Contains(TEXT("Adreno"));
	const bool bIsMaliBased = RendererString.Contains(TEXT("Mali"));

	// Check for external image support for different ES versions
	ImageExternalType = EImageExternalType::None;

	static const auto CVarOverrideExternalTextureSupport = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Android.OverrideExternalTextureSupport"));
	const int32 OverrideExternalTextureSupport = CVarOverrideExternalTextureSupport->GetValueOnAnyThread();
	switch (OverrideExternalTextureSupport)
	{
		case 1:
			ImageExternalType = EImageExternalType::None;
			break;

		case 2:
			ImageExternalType = EImageExternalType::ImageExternal100;
			break;

		case 3:
			ImageExternalType = EImageExternalType::ImageExternal300;
			break;	

		case 4:
			ImageExternalType = EImageExternalType::ImageExternalESSL300;
			break;

		case 0:
		default:
			// auto-detect by extensions (default)
			bool bHasImageExternal = ExtensionsString.Contains(TEXT("GL_OES_EGL_image_external ")) || ExtensionsString.EndsWith(TEXT("GL_OES_EGL_image_external"));
			bool bHasImageExternalESSL3 = ExtensionsString.Contains(TEXT("OES_EGL_image_external_essl3"));
			if (bHasImageExternal || bHasImageExternalESSL3)
			{
				ImageExternalType = EImageExternalType::ImageExternal100;
				if (bUseES30ShadingLanguage)
				{
					if (bHasImageExternalESSL3)
					{
						ImageExternalType = EImageExternalType::ImageExternalESSL300;
					}
					else
					{
						// Adreno 5xx can do essl3 even without extension in list
						if (bIsAdrenoBased && RendererString.Contains(TEXT("(TM) 5")))
						{
							ImageExternalType = EImageExternalType::ImageExternalESSL300;
						}
					}
				}
				if (bIsNvidiaBased)
				{
					// Nvidia needs version 100 even though it supports ES3
					ImageExternalType = EImageExternalType::ImageExternal100;
				}
			}
			break;
	}
	switch (ImageExternalType)
	{
		case EImageExternalType::None:
			UE_LOG(LogRHI, Log, TEXT("Image external disabled"));
			break;

		case EImageExternalType::ImageExternal100:
			UE_LOG(LogRHI, Log, TEXT("Image external enabled: ImageExternal100"));
			break;

		case EImageExternalType::ImageExternal300:
			UE_LOG(LogRHI, Log, TEXT("Image external enabled: ImageExternal300"));
			break;

		case EImageExternalType::ImageExternalESSL300:
			UE_LOG(LogRHI, Log, TEXT("Image external enabled: ImageExternalESSL300"));
			break;

		default:
			ImageExternalType = EImageExternalType::None;
			UE_LOG(LogRHI, Log, TEXT("Image external disabled; unknown type"));
	}
	bSupportsImageExternal = ImageExternalType != EImageExternalType::None;

	if (RendererString.Contains(TEXT("SGX 540")))
	{
		UE_LOG(LogRHI, Warning, TEXT("Disabling support for GL_OES_packed_depth_stencil on SGX 540"));
		bSupportsPackedDepthStencil = false;
		bRequiresTexture2DPrecisionHack = true;
	}

	if (bIsPoverVRBased)
	{
		bHasHardwareHiddenSurfaceRemoval = true;
		UE_LOG(LogRHI, Log, TEXT("Enabling support for Hidden Surface Removal on PowerVR"));
	}

	if (bIsAdrenoBased)
	{
		// This is to avoid a bug in Adreno drivers that define GL_ARM_shader_framebuffer_fetch_depth_stencil even when device does not support this extension
		// OpenGL ES 3.1 V@127.0 (GIT@I1af360237c)
		bRequiresARMShaderFramebufferFetchDepthStencilUndef = !bSupportsShaderDepthStencilFetch;

		// Adreno 2xx doesn't work with packed depth stencil enabled
		if (RendererString.Contains(TEXT("Adreno (TM) 2")))
		{
			UE_LOG(LogRHI, Warning, TEXT("Disabling support for GL_OES_packed_depth_stencil on Adreno 2xx"));
			bSupportsPackedDepthStencil = false;
		}
	}

	if (bES30Support)
	{
		glDrawElementsInstanced = (PFNGLDRAWELEMENTSINSTANCEDPROC)((void*)eglGetProcAddress("glDrawElementsInstanced"));
		glDrawArraysInstanced = (PFNGLDRAWARRAYSINSTANCEDPROC)((void*)eglGetProcAddress("glDrawArraysInstanced"));
		glVertexAttribDivisor = (PFNGLVERTEXATTRIBDIVISORPROC)((void*)eglGetProcAddress("glVertexAttribDivisor"));
		glUniform4uiv = (PFNGLUNIFORM4UIVPROC)((void*)eglGetProcAddress("glUniform4uiv"));
		glTexImage3D = (PFNGLTEXIMAGE3DPROC)((void*)eglGetProcAddress("glTexImage3D"));
		glTexSubImage3D = (PFNGLTEXSUBIMAGE3DPROC)((void*)eglGetProcAddress("glTexSubImage3D"));
		glCompressedTexImage3D = (PFNGLCOMPRESSEDTEXIMAGE3DPROC)((void*)eglGetProcAddress("glCompressedTexImage3D"));
		glCompressedTexSubImage3D = (PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC)((void*)eglGetProcAddress("glCompressedTexSubImage3D"));
		glCopyTexSubImage3D = (PFNGLCOPYTEXSUBIMAGE3DPROC)((void*)eglGetProcAddress("glCopyTexSubImage3D"));
		glClearBufferfi = (PFNGLCLEARBUFFERFIPROC)((void*)eglGetProcAddress("glClearBufferfi"));
		glClearBufferfv = (PFNGLCLEARBUFFERFVPROC)((void*)eglGetProcAddress("glClearBufferfv"));
		glClearBufferiv = (PFNGLCLEARBUFFERIVPROC)((void*)eglGetProcAddress("glClearBufferiv"));
		glClearBufferuiv = (PFNGLCLEARBUFFERUIVPROC)((void*)eglGetProcAddress("glClearBufferuiv"));
		glDrawBuffers = (PFNGLDRAWBUFFERSPROC)((void*)eglGetProcAddress("glDrawBuffers"));


		glBindBufferRange = (PFNGLBINDBUFFERRANGEPROC)((void*)eglGetProcAddress("glBindBufferRange"));
		glBindBufferBase = (PFNGLBINDBUFFERBASEPROC)((void*)eglGetProcAddress("glBindBufferBase"));
		glGetUniformBlockIndex = (PFNGLGETUNIFORMBLOCKINDEXPROC)((void*)eglGetProcAddress("glGetUniformBlockIndex"));
		glUniformBlockBinding = (PFNGLUNIFORMBLOCKBINDINGPROC)((void*)eglGetProcAddress("glUniformBlockBinding"));
		glVertexAttribIPointer = (PFNGLVERTEXATTRIBIPOINTERPROC)((void*)eglGetProcAddress("glVertexAttribIPointer"));


		// Required by the ES3 spec
		bSupportsInstancing = true;
		bSupportsTextureFloat = true;
		bSupportsTextureHalfFloat = true;
		bSupportsRGB10A2 = true;
		bSupportsVertexHalfFloat = true;

		// According to https://www.khronos.org/registry/gles/extensions/EXT/EXT_color_buffer_float.txt
		bSupportsColorBufferHalfFloat = (bSupportsColorBufferHalfFloat || bSupportsColorBufferFloat);
	}

	if (bES30Support)
	{
		// Mobile multi-view setup
		const bool bMultiViewSupport = ExtensionsString.Contains(TEXT("GL_OVR_multiview"));
		const bool bMultiView2Support = ExtensionsString.Contains(TEXT("GL_OVR_multiview2"));
		const bool bMultiViewMultiSampleSupport = ExtensionsString.Contains(TEXT("GL_OVR_multiview_multisampled_render_to_texture"));
		if (bMultiViewSupport && bMultiView2Support && bMultiViewMultiSampleSupport)
		{
			glFramebufferTextureMultiviewOVR = (PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC)((void*)eglGetProcAddress("glFramebufferTextureMultiviewOVR"));
			glFramebufferTextureMultisampleMultiviewOVR = (PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)((void*)eglGetProcAddress("glFramebufferTextureMultisampleMultiviewOVR"));

			bSupportsMobileMultiView = (glFramebufferTextureMultiviewOVR != NULL) && (glFramebufferTextureMultisampleMultiviewOVR != NULL);

			// Just because the driver declares multi-view support and hands us valid function pointers doesn't actually guarantee the feature works...
			if (bSupportsMobileMultiView)
			{
				UE_LOG(LogRHI, Log, TEXT("Device supports mobile multi-view."));
			}
		}
	}

	if (bES31Support)
	{
		bSupportsTextureBuffer = ExtensionsString.Contains(TEXT("GL_EXT_texture_buffer"));
		if (bSupportsTextureBuffer)
		{
			glTexBufferEXT = (PFNGLTEXBUFFEREXTPROC)((void*)eglGetProcAddress("glTexBufferEXT"));
		}
	}

	if (bES30Support || bIsAdrenoBased)
	{
		// Attempt to find ES 3.0 glTexStorage2D if we're on an ES 3.0 device
		glTexStorage2D = (PFNGLTEXSTORAGE2DPROC)((void*)eglGetProcAddress("glTexStorage2D"));
		if( glTexStorage2D != NULL )
		{
			bUseHalfFloatTexStorage = true;
		}
		else
		{
			// need to disable GL_EXT_color_buffer_half_float support because we have no way to allocate the storage and the driver doesn't work without it.
			UE_LOG(LogRHI,Warning,TEXT("Disabling support for GL_EXT_color_buffer_half_float as we cannot bind glTexStorage2D"));
			bSupportsColorBufferHalfFloat = false;
		}
	}

	//@todo android: need GMSAAAllowed	 ?
	if (bSupportsNVFrameBufferBlit)
	{
		glBlitFramebufferNV = (PFNBLITFRAMEBUFFERNVPROC)((void*)eglGetProcAddress("glBlitFramebufferNV"));
	}

	glMapBufferOESa = (PFNGLMAPBUFFEROESPROC)((void*)eglGetProcAddress("glMapBufferOES"));
	glUnmapBufferOESa = (PFNGLUNMAPBUFFEROESPROC)((void*)eglGetProcAddress("glUnmapBufferOES"));

	//On Android, there are problems compiling shaders with textureCubeLodEXT calls in the glsl code,
	// so we set this to false to modify the glsl manually at compile-time.
	bSupportsTextureCubeLodEXT = false;

	// On some Android devices with Mali GPUs textureCubeLod is not available.
	if (RendererString.Contains(TEXT("Mali-400")))
	{
		bSupportsShaderTextureCubeLod = false;
	}

	// Nexus 5 (Android 4.4.2) doesn't like glVertexAttribDivisor(index, 0) called when not using a glDrawElementsInstanced
	if (bIsAdrenoBased && VersionString.Contains(TEXT("OpenGL ES 3.0 V@66.0 AU@  (CL@)")))
	{
		UE_LOG(LogRHI, Warning, TEXT("Disabling support for hardware instancing on Adreno 330 OpenGL ES 3.0 V@66.0 AU@  (CL@)"));
		bSupportsInstancing = false;
	}

	// PowerVR Rogue doesn't like glVertexAttribIPointer so disable it
	if (bIsPoverVRBased && bES30Support)
	{
		if (RendererString.Contains(TEXT("Rogue")))
		{
			glVertexAttribIPointer = nullptr;
			UE_LOG(LogRHI, Warning, TEXT("Disabling glVertexAttribIPointer on PowerVR Rogue"));
		}
	}

	if (bSupportsBGRA8888)
	{
		// Check whether device supports BGRA as color attachment
		GLuint FrameBuffer;
		glGenFramebuffers(1, &FrameBuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, FrameBuffer);
		GLuint BGRA8888Texture;
		glGenTextures(1, &BGRA8888Texture);
		glBindTexture(GL_TEXTURE_2D, BGRA8888Texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, 256, 256, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, BGRA8888Texture, 0);

		bSupportsBGRA8888RenderTarget = (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

		glDeleteTextures(1, &BGRA8888Texture);
		glDeleteFramebuffers(1, &FrameBuffer);
	}

	if (IsES31Usable())
	{
		// ES3.1 requires sRGB texture sampling, these formats do not support it
		bSupportsATITC = false;
		bSupportsPVRTC = false;
	}
}


FString FAndroidMisc::GetGPUFamily()
{
	return FAndroidGPUInfo::Get().GPUFamily;
}

FString FAndroidMisc::GetGLVersion()
{
	return FAndroidGPUInfo::Get().GLVersion;
}

bool FAndroidMisc::SupportsFloatingPointRenderTargets()
{
	return FAndroidGPUInfo::Get().bSupportsFloatingPointRenderTargets;
}

bool FAndroidMisc::SupportsShaderFramebufferFetch()
{
	return FAndroidGPUInfo::Get().bSupportsFrameBufferFetch;
}

bool FAndroidMisc::SupportsES30()
{
	return FAndroidGPUInfo::Get().bES30Support;
}

bool FAndroidMisc::SupportsShaderIOBlocks()
{
	return FAndroidGPUInfo::Get().bSupportsShaderIOBlocks;
}

void FAndroidMisc::GetValidTargetPlatforms(TArray<FString>& TargetPlatformNames)
{
	TargetPlatformNames = FAndroidGPUInfo::Get().TargetPlatformNames;
}

void FAndroidAppEntry::PlatformInit()
{
	// create an ES2 EGL here for gpu queries.
	AndroidEGL::GetInstance()->Init(AndroidEGL::AV_OpenGLES, 2, 0, false);
}

void FAndroidAppEntry::ReleaseEGL()
{
	AndroidEGL* EGL = AndroidEGL::GetInstance();
	if (EGL->IsInitialized())
	{
		EGL->DestroyBackBuffer();
		EGL->Terminate();
	}
}

#endif
