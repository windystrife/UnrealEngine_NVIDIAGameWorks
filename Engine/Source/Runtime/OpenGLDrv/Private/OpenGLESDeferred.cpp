// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGL3.cpp: OpenGL 3.2 implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "OpenGLDrv.h"
#include "OpenGLDrvPrivate.h"

#if PLATFORM_HTML5
#include "HTML5JavaScriptFx.h"
#endif

#if OPENGL_ESDEFERRED

static TAutoConsoleVariable<int32> CVarDisjointTimerQueries(
	TEXT("r.DisjointTimerQueries"),
	0,
	TEXT("If set to 1, allows GPU time to be measured (e.g. STAT UNIT). It defaults to 0 because some devices supports it but very slowly."),
	ECVF_RenderThreadSafe);

GLsizei FOpenGLESDeferred::NextTextureName = OPENGL_NAME_CACHE_SIZE;
GLuint FOpenGLESDeferred::TextureNamesCache[OPENGL_NAME_CACHE_SIZE];
GLsizei FOpenGLESDeferred::NextBufferName= OPENGL_NAME_CACHE_SIZE;
GLuint FOpenGLESDeferred::BufferNamesCache[OPENGL_NAME_CACHE_SIZE];

GLint FOpenGLESDeferred::MaxComputeTextureImageUnits = -1;
GLint FOpenGLESDeferred::MaxComputeUniformComponents = -1;

GLint FOpenGLESDeferred::TimestampQueryBits = 0;
bool FOpenGLESDeferred::bDebugContext = false;

bool FOpenGLESDeferred::bSupportsTessellation = false;
bool FOpenGLESDeferred::bSupportsTextureView = false;
bool FOpenGLESDeferred::bSupportsSeparateAlphaBlend = false;

bool FOpenGLESDeferred::bES2Fallback = true;

/** GL_OES_vertex_array_object */
bool FOpenGLESDeferred::bSupportsVertexArrayObjects = false;

/** GL_OES_mapbuffer */
bool FOpenGLESDeferred::bSupportsMapBuffer = false;

/** GL_OES_depth_texture */
bool FOpenGLESDeferred::bSupportsDepthTexture = false;

/** GL_ARB_occlusion_query2, GL_EXT_occlusion_query_boolean */
bool FOpenGLESDeferred::bSupportsOcclusionQueries = false;

/** GL_OES_rgb8_rgba8 */
bool FOpenGLESDeferred::bSupportsRGBA8 = false;

/** GL_APPLE_texture_format_BGRA8888 */
bool FOpenGLESDeferred::bSupportsBGRA8888 = false;

/** Whether BGRA supported as color attachment */
bool FOpenGLESDeferred::bSupportsBGRA8888RenderTarget = false;

/** GL_EXT_discard_framebuffer */
bool FOpenGLESDeferred::bSupportsDiscardFrameBuffer = false;

/** GL_OES_vertex_half_float */
bool FOpenGLESDeferred::bSupportsVertexHalfFloat = false;

/** GL_OES_texture_float */
bool FOpenGLESDeferred::bSupportsTextureFloat = false;

/** GL_OES_texture_half_float */
bool FOpenGLESDeferred::bSupportsTextureHalfFloat = false;

/** GL_EXT_color_buffer_float */
bool FOpenGLESDeferred::bSupportsColorBufferFloat = false;

/** GL_EXT_color_buffer_half_float */
bool FOpenGLESDeferred::bSupportsColorBufferHalfFloat = false;

/** GL_NV_image_formats */
bool FOpenGLESDeferred::bSupportsNvImageFormats = false;

/** GL_EXT_shader_framebuffer_fetch */
bool FOpenGLESDeferred::bSupportsShaderFramebufferFetch = false;

/* This is to avoid a bug where device supports GL_EXT_shader_framebuffer_fetch but does not define it in GLSL */
bool FOpenGLESDeferred::bRequiresUEShaderFramebufferFetchDef = false;

/** GL_ARM_shader_framebuffer_fetch_depth_stencil */
bool FOpenGLESDeferred::bSupportsShaderDepthStencilFetch = false;

/** GL_EXT_multisampled_render_to_texture */
bool FOpenGLESDeferred::bSupportsMultisampledRenderToTexture = false;

/** GL_EXT_sRGB */
bool FOpenGLESDeferred::bSupportsSGRB = false;

/** GL_NV_texture_compression_s3tc, GL_EXT_texture_compression_s3tc */
bool FOpenGLESDeferred::bSupportsDXT = false;

/** GL_IMG_texture_compression_pvrtc */
bool FOpenGLESDeferred::bSupportsPVRTC = false;

/** GL_ATI_texture_compression_atitc, GL_AMD_compressed_ATC_texture */
bool FOpenGLESDeferred::bSupportsATITC = false;

/** GL_OES_compressed_ETC1_RGB8_texture */
bool FOpenGLESDeferred::bSupportsETC1 = false;

/** OpenGL ES 3.0 profile */
bool FOpenGLESDeferred::bSupportsETC2 = false;

/** GL_FRAGMENT_SHADER, GL_LOW_FLOAT */
int FOpenGLESDeferred::ShaderLowPrecision = 0;

/** GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT */
int FOpenGLESDeferred::ShaderMediumPrecision = 0;

/** GL_FRAGMENT_SHADER, GL_HIGH_FLOAT */
int FOpenGLESDeferred::ShaderHighPrecision = 0;

/** GL_NV_framebuffer_blit */
bool FOpenGLESDeferred::bSupportsNVFrameBufferBlit = false;

/** GL_OES_packed_depth_stencil */
bool FOpenGLESDeferred::bSupportsPackedDepthStencil = false;

/** textureCubeLodEXT */
bool FOpenGLESDeferred::bSupportsTextureCubeLodEXT = true;

/** GL_EXT_shader_texture_lod */
bool FOpenGLESDeferred::bSupportsShaderTextureLod = false;

/** textureCubeLod */
bool FOpenGLESDeferred::bSupportsShaderTextureCubeLod = true;

/** GL_APPLE_copy_texture_levels */
bool FOpenGLESDeferred::bSupportsCopyTextureLevels = false;

/** GL_EXT_texture_storage */
bool FOpenGLESDeferred::bSupportsTextureStorageEXT = false;

/* This is a hack to remove the calls to "precision sampler" defaults which are produced by the cross compiler however don't compile on some android platforms */
bool FOpenGLESDeferred::bRequiresDontEmitPrecisionForTextureSamplers = false;

/* Some android platforms require textureCubeLod to be used some require textureCubeLodEXT however they either inconsistently or don't use the GL_TextureCubeLodEXT extension definition */
bool FOpenGLESDeferred::bRequiresTextureCubeLodEXTToTextureCubeLodDefine = false;

/* This is a hack to remove the gl_FragCoord if shader will fail to link if exceeding the max varying on android platforms */
bool FOpenGLESDeferred::bRequiresGLFragCoordVaryingLimitHack = false;

/* This hack fixes an issue with SGX540 compiler which can get upset with some operations that mix highp and mediump */
bool FOpenGLESDeferred::bRequiresTexture2DPrecisionHack = false;

/* This is to avoid a bug in Adreno drivers that define GL_ARM_shader_framebuffer_fetch_depth_stencil even when device does not support this extension  */
bool FOpenGLESDeferred::bRequiresARMShaderFramebufferFetchDepthStencilUndef = false;

/* Indicates shader compiler hack checks are being tested */
bool FOpenGLESDeferred::bIsCheckingShaderCompilerHacks = false;

/** GL_EXT_disjoint_timer_query or GL_NV_timer_query*/
bool FOpenGLESDeferred::bSupportsDisjointTimeQueries = false;

/** Some timer query implementations are never disjoint */
bool FOpenGLESDeferred::bTimerQueryCanBeDisjoint = true;

/** GL_NV_timer_query for timestamp queries */
bool FOpenGLESDeferred::bSupportsNvTimerQuery = false;

/** GL_OES_vertex_type_10_10_10_2 */
bool FOpenGLESDeferred::bSupportsRGB10A2 = false;

GLint FOpenGLESDeferred::MajorVersion = 0;
GLint FOpenGLESDeferred::MinorVersion = 0;

bool FOpenGLESDeferred::SupportsAdvancedFeatures()
{
	bool bResult = true;
	GLint LocalMajorVersion = 0;
	GLint LocalMinorVersion = 0;

	const FString ExtensionsString = ANSI_TO_TCHAR((const ANSICHAR*)glGetString(GL_EXTENSIONS));

	if (FString(ANSI_TO_TCHAR((const ANSICHAR*)glGetString(GL_VERSION))).Contains(TEXT("OpenGL ES 3.")))
	{
		glGetIntegerv(GL_MAJOR_VERSION, &LocalMajorVersion);
		glGetIntegerv(GL_MINOR_VERSION, &LocalMinorVersion);

		// Check for minimum ES 3.1 + extensions support to avoid the ES2 fallback
		bResult = (LocalMajorVersion == 3 && LocalMinorVersion >= 1);

		bResult &= ExtensionsString.Contains(TEXT("GL_ANDROID_extension_pack_es31a"));
		bResult &= ExtensionsString.Contains(TEXT("GL_EXT_color_buffer_half_float"));
	}
	else
	{
		bResult = false;
	}

	return bResult;
}

bool FOpenGLESDeferred::SupportsDisjointTimeQueries()
{
	bool bAllowDisjointTimerQueries = false;
	bAllowDisjointTimerQueries = (CVarDisjointTimerQueries.GetValueOnRenderThread() == 1);
	return bSupportsDisjointTimeQueries && bAllowDisjointTimerQueries;
}

void FOpenGLESDeferred::ProcessQueryGLInt()
{
	if(bES2Fallback)
	{
		LOG_AND_GET_GL_INT(GL_MAX_VARYING_VECTORS, 0, MaxVaryingVectors);
		LOG_AND_GET_GL_INT(GL_MAX_VERTEX_UNIFORM_VECTORS, 0, MaxVertexUniformComponents);
		LOG_AND_GET_GL_INT(GL_MAX_FRAGMENT_UNIFORM_VECTORS, 0, MaxPixelUniformComponents);
		MaxVaryingVectors *= 4;
		MaxVertexUniformComponents *= 4;
		MaxPixelUniformComponents *= 4;
		MaxGeometryUniformComponents = 0;

		MaxGeometryTextureImageUnits = 0;
		MaxHullTextureImageUnits = 0;
		MaxDomainTextureImageUnits = 0;
	}
	else
	{
		GET_GL_INT(GL_MAX_VARYING_VECTORS, 0, MaxVaryingVectors);
		GET_GL_INT(GL_MAX_VERTEX_UNIFORM_COMPONENTS, 0, MaxVertexUniformComponents);
		GET_GL_INT(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, 0, MaxPixelUniformComponents);
		GET_GL_INT(GL_MAX_GEOMETRY_UNIFORM_COMPONENTS_EXT, 0, MaxGeometryUniformComponents);

		GET_GL_INT(GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS_EXT, 0, MaxGeometryTextureImageUnits);

		GET_GL_INT(GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS, 0, MaxComputeTextureImageUnits);
		GET_GL_INT(GL_MAX_COMPUTE_UNIFORM_COMPONENTS, 0, MaxComputeUniformComponents);

		if (bSupportsTessellation)
		{
			GET_GL_INT(GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS_EXT, 0, MaxHullUniformComponents);
			GET_GL_INT(GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS_EXT, 0, MaxDomainUniformComponents);
			GET_GL_INT(GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS_EXT, 0, MaxHullTextureImageUnits);
			GET_GL_INT(GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS_EXT, 0, MaxDomainTextureImageUnits);
		}
		else
		{
			MaxHullUniformComponents = 0;
			MaxDomainUniformComponents = 0;
			MaxHullTextureImageUnits = 0;
			MaxDomainTextureImageUnits = 0;
		}
	}

	// No timestamps
	//LOG_AND_GET_GL_QUERY_INT(GL_TIMESTAMP, 0, TimestampQueryBits);
}

void FOpenGLESDeferred::ProcessExtensions( const FString& ExtensionsString )
{
	// Version setup first, need to check string for 3 or higher, then can use integer queries
	if (SupportsAdvancedFeatures())
	{
		glGetIntegerv(GL_MAJOR_VERSION, &MajorVersion);
		glGetIntegerv(GL_MINOR_VERSION, &MinorVersion);

		bES2Fallback = false;
		UE_LOG(LogRHI, Log, TEXT("bES2Fallback = false"));
	}
	else
	{
		MajorVersion = 2;
		MinorVersion = 0;
		bES2Fallback = true;
		UE_LOG(LogRHI, Log, TEXT("bES2Fallback = true"));
	}

	bSupportsSeparateAlphaBlend = ExtensionsString.Contains(TEXT("GL_EXT_draw_buffers_indexed"));

	static auto CVarAllowHighQualityLightMaps = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HighQualityLightMaps"));

	if (!bES2Fallback)
	{
		// only supported if we are at a minimum bar
		bSupportsTessellation = ExtensionsString.Contains(TEXT("GL_EXT_tessellation_shader"));
		bSupportsTextureView = ExtensionsString.Contains(TEXT("GL_EXT_texture_view"));
		CVarAllowHighQualityLightMaps->Set(1);
	}
	else
	{
		bSupportsRGB10A2 = ExtensionsString.Contains(TEXT("GL_OES_vertex_type_10_10_10_2"));
		CVarAllowHighQualityLightMaps->Set(0);
	}

	ProcessQueryGLInt();
	FOpenGLBase::ProcessExtensions(ExtensionsString);

	bSupportsMapBuffer = ExtensionsString.Contains(TEXT("GL_OES_mapbuffer"));
	bSupportsDepthTexture = ExtensionsString.Contains(TEXT("GL_OES_depth_texture"));
	bSupportsOcclusionQueries = ExtensionsString.Contains(TEXT("GL_ARB_occlusion_query2")) || ExtensionsString.Contains(TEXT("GL_EXT_occlusion_query_boolean"));
	bSupportsRGBA8 = ExtensionsString.Contains(TEXT("GL_OES_rgb8_rgba8"));
	bSupportsBGRA8888 = ExtensionsString.Contains(TEXT("GL_APPLE_texture_format_BGRA8888")) || ExtensionsString.Contains(TEXT("GL_IMG_texture_format_BGRA8888")) || ExtensionsString.Contains(TEXT("GL_EXT_texture_format_BGRA8888"));
	bSupportsVertexHalfFloat = ExtensionsString.Contains(TEXT("GL_OES_vertex_half_float"));
	bSupportsTextureFloat = !bES2Fallback || ExtensionsString.Contains(TEXT("GL_OES_texture_float"));
	bSupportsTextureHalfFloat = !bES2Fallback || ExtensionsString.Contains(TEXT("GL_OES_texture_half_float"));
	bSupportsSGRB = ExtensionsString.Contains(TEXT("GL_EXT_sRGB"));
	bSupportsColorBufferFloat = ExtensionsString.Contains(TEXT("GL_EXT_color_buffer_float"));
	bSupportsColorBufferHalfFloat = ExtensionsString.Contains(TEXT("GL_EXT_color_buffer_half_float"));
	bSupportsNvImageFormats = ExtensionsString.Contains(TEXT("GL_NV_image_formats"));
	bSupportsShaderFramebufferFetch = ExtensionsString.Contains(TEXT("GL_EXT_shader_framebuffer_fetch")) || ExtensionsString.Contains(TEXT("GL_NV_shader_framebuffer_fetch")) 
		|| ExtensionsString.Contains(TEXT("GL_ARM_shader_framebuffer_fetch ")); // has space at the end to exclude GL_ARM_shader_framebuffer_fetch_depth_stencil match
	bRequiresUEShaderFramebufferFetchDef = ExtensionsString.Contains(TEXT("GL_EXT_shader_framebuffer_fetch"));
	bSupportsShaderDepthStencilFetch = ExtensionsString.Contains(TEXT("GL_ARM_shader_framebuffer_fetch_depth_stencil"));
	bSupportsMultisampledRenderToTexture = ExtensionsString.Contains(TEXT("GL_EXT_multisampled_render_to_texture"));
	// @todo es3: SRGB support does not work with our texture format setup (ES2 docs indicate that internalFormat and format must match, but they don't at all with sRGB enabled)
	//             One possible solution us to use GLFormat.InternalFormat[bSRGB] instead of GLFormat.Format
	bSupportsSGRB = false;//ExtensionsString.Contains(TEXT("GL_EXT_sRGB"));
	bSupportsDXT = ExtensionsString.Contains(TEXT("GL_NV_texture_compression_s3tc")) || ExtensionsString.Contains(TEXT("GL_EXT_texture_compression_s3tc"));
	bSupportsPVRTC = ExtensionsString.Contains(TEXT("GL_IMG_texture_compression_pvrtc")) ;
	bSupportsATITC = ExtensionsString.Contains(TEXT("GL_ATI_texture_compression_atitc")) || ExtensionsString.Contains(TEXT("GL_AMD_compressed_ATC_texture"));
	bSupportsETC1 = ExtensionsString.Contains(TEXT("GL_OES_compressed_ETC1_RGB8_texture"));
	bSupportsVertexArrayObjects = ExtensionsString.Contains(TEXT("GL_OES_vertex_array_object")) ;
	bSupportsDiscardFrameBuffer = ExtensionsString.Contains(TEXT("GL_EXT_discard_framebuffer"));
	bSupportsNVFrameBufferBlit = ExtensionsString.Contains(TEXT("GL_NV_framebuffer_blit"));
	bSupportsPackedDepthStencil = ExtensionsString.Contains(TEXT("GL_OES_packed_depth_stencil"));
	bSupportsShaderTextureLod = ExtensionsString.Contains(TEXT("GL_EXT_shader_texture_lod"));
#if PLATFORM_HTML5
	// WebGL 1 extensions that were adopted to core WebGL 2 spec:
	if (UE_BrowserWebGLVersion() == 2)
	{
		bSupportsColorBufferHalfFloat = bSupportsShaderTextureLod = true;
	}
#endif
	bSupportsTextureStorageEXT = ExtensionsString.Contains(TEXT("GL_EXT_texture_storage"));
	bSupportsCopyTextureLevels = bSupportsTextureStorageEXT && ExtensionsString.Contains(TEXT("GL_APPLE_copy_texture_levels"));
	bSupportsDisjointTimeQueries = ExtensionsString.Contains(TEXT("GL_EXT_disjoint_timer_query"));// || ExtensionsString.Contains(TEXT("GL_NV_timer_query"));
	bTimerQueryCanBeDisjoint = !ExtensionsString.Contains(TEXT("GL_NV_timer_query"));
	bSupportsNvTimerQuery = ExtensionsString.Contains(TEXT("GL_NV_timer_query"));

	// Report shader precision
	int Range[2];
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_LOW_FLOAT, Range, &ShaderLowPrecision);
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT, Range, &ShaderMediumPrecision);
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, Range, &ShaderHighPrecision);
	UE_LOG(LogRHI, Log, TEXT("Fragment shader lowp precision: %d"), ShaderLowPrecision);
	UE_LOG(LogRHI, Log, TEXT("Fragment shader mediump precision: %d"), ShaderMediumPrecision);
	UE_LOG(LogRHI, Log, TEXT("Fragment shader highp precision: %d"), ShaderHighPrecision);

	// Test whether the GPU can support volume-texture rendering.
	// There is no API to query this - you just have to test whether a 3D texture is framebuffer-complete.
	if (!bES2Fallback)
	{
		GLuint FrameBuffer;
		glGenFramebuffers(1, &FrameBuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FrameBuffer);
		GLuint VolumeTexture;
		glGenTextures(1, &VolumeTexture);
		glBindTexture(GL_TEXTURE_3D, VolumeTexture);
		glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 256, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glFramebufferTextureEXT(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, VolumeTexture, 0);

		bSupportsVolumeTextureRendering = (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

		glDeleteTextures(1, &VolumeTexture);
		glDeleteFramebuffers(1, &FrameBuffer);
	}

	if (bSupportsBGRA8888)
	{
		// Check whether device supports BGRA as color attachment
		GLuint FrameBuffer;
		glGenFramebuffers(1, &FrameBuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FrameBuffer);
		GLuint BGRA8888Texture;
		glGenTextures(1, &BGRA8888Texture);
		glBindTexture(GL_TEXTURE_2D, BGRA8888Texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, 256, 256, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, BGRA8888Texture, 0);

		bSupportsBGRA8888RenderTarget = (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

		glDeleteTextures(1, &BGRA8888Texture);
		glDeleteFramebuffers(1, &FrameBuffer);
	}

	bSupportsCopyImage = ExtensionsString.Contains(TEXT("GL_EXT_copy_image"));
}

#endif
