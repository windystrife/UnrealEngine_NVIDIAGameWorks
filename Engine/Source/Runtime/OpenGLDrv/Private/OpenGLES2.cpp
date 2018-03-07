// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLES2.cpp: OpenGL ES2 implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "OpenGLDrv.h"
#include "OpenGLDrvPrivate.h"

#if !PLATFORM_DESKTOP

#if OPENGL_ES2

/** GL_OES_vertex_array_object */
bool FOpenGLES2::bSupportsVertexArrayObjects = false;

/** GL_OES_mapbuffer */
bool FOpenGLES2::bSupportsMapBuffer = false;

/** GL_OES_depth_texture */
bool FOpenGLES2::bSupportsDepthTexture = false;

/** GL_ARB_occlusion_query2, GL_EXT_occlusion_query_boolean */
bool FOpenGLES2::bSupportsOcclusionQueries = false;

/** GL_EXT_disjoint_timer_query */
bool FOpenGLES2::bSupportsDisjointTimeQueries = false;

static TAutoConsoleVariable<int32> CVarDisjointTimerQueries(
	TEXT("r.DisjointTimerQueries"),
	0,
	TEXT("If set to 1, allows GPU time to be measured (e.g. STAT UNIT). It defaults to 0 because some devices supports it but very slowly."),
	ECVF_RenderThreadSafe);

/** Some timer query implementations are never disjoint */
bool FOpenGLES2::bTimerQueryCanBeDisjoint = true;

/** GL_OES_rgb8_rgba8 */
bool FOpenGLES2::bSupportsRGBA8 = false;

/** GL_APPLE_texture_format_BGRA8888 */
bool FOpenGLES2::bSupportsBGRA8888 = false;

/** Whether BGRA supported as color attachment */
bool FOpenGLES2::bSupportsBGRA8888RenderTarget = false;

/** GL_EXT_discard_framebuffer */
bool FOpenGLES2::bSupportsDiscardFrameBuffer = false;

/** GL_OES_vertex_half_float */
bool FOpenGLES2::bSupportsVertexHalfFloat = false;

/** GL_OES_texture_float */
bool FOpenGLES2::bSupportsTextureFloat = false;

/** GL_OES_texture_half_float */
bool FOpenGLES2::bSupportsTextureHalfFloat = false;

/** GL_EXT_color_buffer_half_float */
bool FOpenGLES2::bSupportsColorBufferHalfFloat = false;

/** GL_EXT_color_buffer_float */
bool FOpenGLES2::bSupportsColorBufferFloat = false;

/** GL_EXT_shader_framebuffer_fetch */
bool FOpenGLES2::bSupportsShaderFramebufferFetch = false;

/* This is to avoid a bug where device supports GL_EXT_shader_framebuffer_fetch but does not define it in GLSL */
bool FOpenGLES2::bRequiresUEShaderFramebufferFetchDef = false;

/** GL_ARM_shader_framebuffer_fetch_depth_stencil */
bool FOpenGLES2::bSupportsShaderDepthStencilFetch = false;

/** GL_EXT_multisampled_render_to_texture */
bool FOpenGLES2::bSupportsMultisampledRenderToTexture = false;

/** GL_EXT_sRGB */
bool FOpenGLES2::bSupportsSGRB = false;

/** GL_NV_texture_compression_s3tc, GL_EXT_texture_compression_s3tc */
bool FOpenGLES2::bSupportsDXT = false;

/** GL_IMG_texture_compression_pvrtc */
bool FOpenGLES2::bSupportsPVRTC = false;

/** GL_ATI_texture_compression_atitc, GL_AMD_compressed_ATC_texture */
bool FOpenGLES2::bSupportsATITC = false;

/** GL_OES_compressed_ETC1_RGB8_texture */
bool FOpenGLES2::bSupportsETC1 = false;

/** OpenGL ES 3.0 profile */
bool FOpenGLES2::bSupportsETC2 = false;

/** GL_FRAGMENT_SHADER, GL_LOW_FLOAT */
int FOpenGLES2::ShaderLowPrecision = 0;

/** GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT */
int FOpenGLES2::ShaderMediumPrecision = 0;

/** GL_FRAGMENT_SHADER, GL_HIGH_FLOAT */
int FOpenGLES2::ShaderHighPrecision = 0;

/** GL_NV_framebuffer_blit */
bool FOpenGLES2::bSupportsNVFrameBufferBlit = false;

/** GL_OES_packed_depth_stencil */
bool FOpenGLES2::bSupportsPackedDepthStencil = false;

/** textureCubeLodEXT */
bool FOpenGLES2::bSupportsTextureCubeLodEXT = true;

/** GL_EXT_shader_texture_lod */
bool FOpenGLES2::bSupportsShaderTextureLod = false;

/** textureCubeLod */
bool FOpenGLES2::bSupportsShaderTextureCubeLod = true;

/** GL_APPLE_copy_texture_levels */
bool FOpenGLES2::bSupportsCopyTextureLevels = false;

/** GL_OES_texture_npot */
bool FOpenGLES2::bSupportsTextureNPOT = false;

/** GL_EXT_texture_storage */
bool FOpenGLES2::bSupportsTextureStorageEXT = false;

/* This is a hack to remove the calls to "precision sampler" defaults which are produced by the cross compiler however don't compile on some android platforms */
bool FOpenGLES2::bRequiresDontEmitPrecisionForTextureSamplers = false;

/* Some android platforms require textureCubeLod to be used some require textureCubeLodEXT however they either inconsistently or don't use the GL_TextureCubeLodEXT extension definition */
bool FOpenGLES2::bRequiresTextureCubeLodEXTToTextureCubeLodDefine = false;

/* Some android platforms do not support the GL_OES_standard_derivatives extension */
bool FOpenGLES2::bSupportsStandardDerivativesExtension = false;

/* This is a hack to remove the gl_FragCoord if shader will fail to link if exceeding the max varying on android platforms */
bool FOpenGLES2::bRequiresGLFragCoordVaryingLimitHack = false;

/** Vertex attributes need remapping if GL_MAX_VERTEX_ATTRIBS < 16 */
bool FOpenGLES2::bNeedsVertexAttribRemap = false;

/* This hack fixes an issue with SGX540 compiler which can get upset with some operations that mix highp and mediump */
bool FOpenGLES2::bRequiresTexture2DPrecisionHack = false;

/* This is to avoid a bug in Adreno drivers that define GL_ARM_shader_framebuffer_fetch_depth_stencil even when device does not support this extension  */
bool FOpenGLES2::bRequiresARMShaderFramebufferFetchDepthStencilUndef = false;

/* Indicates shader compiler hack checks are being tested */
bool FOpenGLES2::bIsCheckingShaderCompilerHacks = false;

/** GL_OES_vertex_type_10_10_10_2 */
bool FOpenGLES2::bSupportsRGB10A2 = false;

/** GL_OES_program_binary extension */
bool FOpenGLES2::bSupportsProgramBinary = false;

/* Indicates shader compiler hack checks are being tested */
bool FOpenGLES2::bIsLimitingShaderCompileCount = false;

bool FOpenGLES2::SupportsDisjointTimeQueries()
{
	bool bAllowDisjointTimerQueries = false;
	bAllowDisjointTimerQueries = (CVarDisjointTimerQueries.GetValueOnRenderThread() == 1);
	return bSupportsDisjointTimeQueries && bAllowDisjointTimerQueries;
}

void FOpenGLES2::ProcessQueryGLInt()
{
	GLint MaxVertexAttribs;
	LOG_AND_GET_GL_INT(GL_MAX_VERTEX_ATTRIBS, 0, MaxVertexAttribs);
	bNeedsVertexAttribRemap = MaxVertexAttribs < 16;
	if (bNeedsVertexAttribRemap)
	{
		UE_LOG(LogRHI, Warning,
			TEXT("Device reports support for %d vertex attributes, UE4 requires 16. Rendering artifacts may occur."),
			MaxVertexAttribs
			);
	}

	LOG_AND_GET_GL_INT(GL_MAX_VARYING_VECTORS, 0, MaxVaryingVectors);
	LOG_AND_GET_GL_INT(GL_MAX_VERTEX_UNIFORM_VECTORS, 0, MaxVertexUniformComponents);
	LOG_AND_GET_GL_INT(GL_MAX_FRAGMENT_UNIFORM_VECTORS, 0, MaxPixelUniformComponents);

	const GLint RequiredMaxVertexUniformComponents = 256;
	if (MaxVertexUniformComponents < RequiredMaxVertexUniformComponents)
	{
		UE_LOG(LogRHI,Warning,
			TEXT("Device reports support for %d vertex uniform vectors, UE4 requires %d. Rendering artifacts may occur, especially with skeletal meshes. Some drivers, e.g. iOS, report a smaller number than is actually supported."),
			MaxVertexUniformComponents,
			RequiredMaxVertexUniformComponents
			);
	}
	MaxVertexUniformComponents = FMath::Max<GLint>(MaxVertexUniformComponents, RequiredMaxVertexUniformComponents);

	MaxGeometryUniformComponents = 0;

	MaxGeometryTextureImageUnits = 0;
	MaxHullTextureImageUnits = 0;
	MaxDomainTextureImageUnits = 0;
}

void FOpenGLES2::ProcessExtensions( const FString& ExtensionsString )
{
	ProcessQueryGLInt();
	FOpenGLBase::ProcessExtensions(ExtensionsString);

	bSupportsMapBuffer = ExtensionsString.Contains(TEXT("GL_OES_mapbuffer"));
	bSupportsDepthTexture = ExtensionsString.Contains(TEXT("GL_OES_depth_texture"));
	bSupportsOcclusionQueries = ExtensionsString.Contains(TEXT("GL_ARB_occlusion_query2")) || ExtensionsString.Contains(TEXT("GL_EXT_occlusion_query_boolean"));
	bSupportsDisjointTimeQueries = ExtensionsString.Contains(TEXT("GL_EXT_disjoint_timer_query")) || ExtensionsString.Contains(TEXT("GL_NV_timer_query"));
	bTimerQueryCanBeDisjoint = !ExtensionsString.Contains(TEXT("GL_NV_timer_query"));
	bSupportsRGBA8 = ExtensionsString.Contains(TEXT("GL_OES_rgb8_rgba8"));
	bSupportsBGRA8888 = ExtensionsString.Contains(TEXT("GL_APPLE_texture_format_BGRA8888")) || ExtensionsString.Contains(TEXT("GL_IMG_texture_format_BGRA8888")) || ExtensionsString.Contains(TEXT("GL_EXT_texture_format_BGRA8888"));
	bSupportsBGRA8888RenderTarget = bSupportsBGRA8888;
	bSupportsVertexHalfFloat = ExtensionsString.Contains(TEXT("GL_OES_vertex_half_float"));
	bSupportsTextureFloat = ExtensionsString.Contains(TEXT("GL_OES_texture_float"));
	bSupportsTextureHalfFloat = ExtensionsString.Contains(TEXT("GL_OES_texture_half_float"));
	bSupportsSGRB = ExtensionsString.Contains(TEXT("GL_EXT_sRGB"));
	bSupportsColorBufferFloat = ExtensionsString.Contains(TEXT("GL_EXT_color_buffer_float"));
	bSupportsColorBufferHalfFloat = ExtensionsString.Contains(TEXT("GL_EXT_color_buffer_half_float"));
	bSupportsShaderFramebufferFetch = ExtensionsString.Contains(TEXT("GL_EXT_shader_framebuffer_fetch")) || ExtensionsString.Contains(TEXT("GL_NV_shader_framebuffer_fetch")) 
		|| ExtensionsString.Contains(TEXT("GL_ARM_shader_framebuffer_fetch ")); // has space at the end to exclude GL_ARM_shader_framebuffer_fetch_depth_stencil match
	bRequiresUEShaderFramebufferFetchDef = ExtensionsString.Contains(TEXT("GL_EXT_shader_framebuffer_fetch"));
	bSupportsShaderDepthStencilFetch = ExtensionsString.Contains(TEXT("GL_ARM_shader_framebuffer_fetch_depth_stencil"));
	bSupportsMultisampledRenderToTexture = ExtensionsString.Contains(TEXT("GL_EXT_multisampled_render_to_texture"));
	// @todo ios7: SRGB support does not work with our texture format setup (ES2 docs indicate that internalFormat and format must match, but they don't at all with sRGB enabled)
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
	bSupportsTextureStorageEXT = ExtensionsString.Contains(TEXT("GL_EXT_texture_storage"));
	bSupportsCopyTextureLevels = bSupportsTextureStorageEXT && ExtensionsString.Contains(TEXT("GL_APPLE_copy_texture_levels"));
	bSupportsTextureNPOT = ExtensionsString.Contains(TEXT("GL_OES_texture_npot")) || ExtensionsString.Contains(TEXT("GL_ARB_texture_non_power_of_two"));
	bSupportsStandardDerivativesExtension = ExtensionsString.Contains(TEXT("GL_OES_standard_derivatives"));
	bSupportsRGB10A2 = ExtensionsString.Contains(TEXT("GL_OES_vertex_type_10_10_10_2"));
	bSupportsProgramBinary = ExtensionsString.Contains(TEXT("GL_OES_get_program_binary"));
	if (!bSupportsStandardDerivativesExtension)
	{
		UE_LOG(LogRHI, Warning, TEXT("GL_OES_standard_derivatives not supported. There may be rendering errors if materials depend on dFdx, dFdy, or fwidth."));
	}

	// Report shader precision
	int Range[2];
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_LOW_FLOAT, Range, &ShaderLowPrecision);
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT, Range, &ShaderMediumPrecision);
	glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, Range, &ShaderHighPrecision);
	UE_LOG(LogRHI, Log, TEXT("Fragment shader lowp precision: %d"), ShaderLowPrecision);
	UE_LOG(LogRHI, Log, TEXT("Fragment shader mediump precision: %d"), ShaderMediumPrecision);
	UE_LOG(LogRHI, Log, TEXT("Fragment shader highp precision: %d"), ShaderHighPrecision);

	if ( FPlatformMisc::IsDebuggerPresent() && UE_BUILD_DEBUG )
	{
		// Enable GL debug markers if we're running in Xcode
		extern int32 GEmitMeshDrawEvent;
		GEmitMeshDrawEvent = 1;
		GEmitDrawEvents = true;
	}

	// ES2 requires a color attachment when rendering to depth-only.
	GSupportsDepthRenderTargetWithoutColorRenderTarget = false;
}

#endif

#endif //desktop
