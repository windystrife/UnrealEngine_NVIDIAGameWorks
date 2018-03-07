// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================
	AndroidESDeferredOpenGL.h: Public OpenGL ES definitions for Android-specific functionality
=============================================================================*/

// We support SM5 feature level on Android.
#define OPENGL_SUPPORTS_SM5 1

#include "AndroidEGL.h"
#include "RenderingThread.h"

#include <EGL/eglext.h>
#include <EGL/eglplatform.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

union FEGLGLSync
{
	FEGLGLSync() : EGL(nullptr)
	{

	}
	FEGLGLSync(GLsync InGL) : GL(InGL)
	{
	}

	FEGLGLSync(EGLSyncKHR InEGL) : EGL(InEGL)
	{

	}

	operator bool() const
	{
		return GL;
	}

	EGLSyncKHR EGL;
	GLsync GL;
};

typedef FEGLGLSync UGLsync;
	
#define GLdouble		GLfloat
#define GL_BGRA			GL_BGRA_EXT
#define GL_UNSIGNED_INT_8_8_8_8_REV	GL_UNSIGNED_BYTE
#define GL_UNSIGNED_INT_8_8_8_8	0x8035

#ifndef EGL_KHR_create_context
#define EGL_KHR_create_context 1

#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR            0x1
#define EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR                   0x1
#define EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR   0x2
#define EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR      0x2
#define EGL_CONTEXT_MAJOR_VERSION_KHR                      0x3098     /* 12440 */
#define EGL_CONTEXT_MINOR_VERSION_KHR                      0x30fb     /* 12539 */
#define EGL_CONTEXT_FLAGS_KHR                              0x30fc     /* 12540 */
#define EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR                0x30fd     /* 12541 */
#define EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR 0x31bd     /* 12733 */
#define EGL_NO_RESET_NOTIFICATION_KHR                      0x31be     /* 12734 */
#define EGL_LOSE_CONTEXT_ON_RESET_KHR                      0x31bf     /* 12735 */
#define EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR           0x4
#define EGL_OPENGL_ES3_BIT_KHR                             0x40       /* 64 */
#endif

// _EXT was needed for Android, so make both work if non _EXT isn't defined
#ifndef GL_TESS_CONTROL_SHADER_BIT
#define GL_TESS_CONTROL_SHADER_BIT GL_TESS_CONTROL_SHADER_BIT_EXT
#define GL_TESS_EVALUATION_SHADER_BIT GL_TESS_EVALUATION_SHADER_BIT_EXT
#define GL_GEOMETRY_SHADER_BIT GL_GEOMETRY_SHADER_BIT_EXT
#endif

#ifndef GL_NV_bindless_texture
#define GL_NV_bindless_texture 1
typedef uint64_t GLuint64EXT;
#define GL_UNSIGNED_INT64_NV             0x140F
typedef GLuint64(GL_APIENTRYP PFNGLGETTEXTUREHANDLENVPROC) (GLuint texture);
typedef GLuint64(GL_APIENTRYP PFNGLGETTEXTURESAMPLERHANDLENVPROC) (GLuint texture, GLuint sampler);
typedef void (GL_APIENTRYP PFNGLMAKETEXTUREHANDLERESIDENTNVPROC) (GLuint64 handle);
typedef void (GL_APIENTRYP PFNGLMAKETEXTUREHANDLENONRESIDENTNVPROC) (GLuint64 handle);
typedef GLuint64(GL_APIENTRYP PFNGLGETIMAGEHANDLENVPROC) (GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum format);
typedef void (GL_APIENTRYP PFNGLMAKEIMAGEHANDLERESIDENTNVPROC) (GLuint64 handle, GLenum access);
typedef void (GL_APIENTRYP PFNGLMAKEIMAGEHANDLENONRESIDENTNVPROC) (GLuint64 handle);
typedef void (GL_APIENTRYP PFNGLUNIFORMHANDLEUI64NVPROC) (GLint location, GLuint64 value);
typedef void (GL_APIENTRYP PFNGLUNIFORMHANDLEUI64VNVPROC) (GLint location, GLsizei count, const GLuint64 *value);
typedef void (GL_APIENTRYP PFNGLPROGRAMUNIFORMHANDLEUI64NVPROC) (GLuint program, GLint location, GLuint64 value);
typedef void (GL_APIENTRYP PFNGLPROGRAMUNIFORMHANDLEUI64VNVPROC) (GLuint program, GLint location, GLsizei count, const GLuint64 *values);
typedef GLboolean(GL_APIENTRYP PFNGLISTEXTUREHANDLERESIDENTNVPROC) (GLuint64 handle);
typedef GLboolean(GL_APIENTRYP PFNGLISIMAGEHANDLERESIDENTNVPROC) (GLuint64 handle);
typedef void (GL_APIENTRYP PFNGLVERTEXATTRIBL1UI64NVPROC) (GLuint index, GLuint64EXT x);
typedef void (GL_APIENTRYP PFNGLVERTEXATTRIBL1UI64VNVPROC) (GLuint index, const GLuint64EXT *v);
typedef void (GL_APIENTRYP PFNGLGETVERTEXATTRIBLUI64VNVPROC) (GLuint index, GLenum pname, GLuint64EXT *params);
#ifdef GL_GLEXT_PROTOTYPES
GL_APICALL GLuint64 GL_APIENTRY glGetTextureHandleNV(GLuint texture);
GL_APICALL GLuint64 GL_APIENTRY glGetTextureSamplerHandleNV(GLuint texture, GLuint sampler);
GL_APICALL void GL_APIENTRY glMakeTextureHandleResidentNV(GLuint64 handle);
GL_APICALL void GL_APIENTRY glMakeTextureHandleNonResidentNV(GLuint64 handle);
GL_APICALL GLuint64 GL_APIENTRY glGetImageHandleNV(GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum format);
GL_APICALL void GL_APIENTRY glMakeImageHandleResidentNV(GLuint64 handle, GLenum access);
GL_APICALL void GL_APIENTRY glMakeImageHandleNonResidentNV(GLuint64 handle);
GL_APICALL void GL_APIENTRY glUniformHandleui64NV(GLint location, GLuint64 value);
GL_APICALL void GL_APIENTRY glUniformHandleui64vNV(GLint location, GLsizei count, const GLuint64 *value);
GL_APICALL void GL_APIENTRY glProgramUniformHandleui64NV(GLuint program, GLint location, GLuint64 value);
GL_APICALL void GL_APIENTRY glProgramUniformHandleui64vNV(GLuint program, GLint location, GLsizei count, const GLuint64 *values);
GL_APICALL GLboolean GL_APIENTRY glIsTextureHandleResidentNV(GLuint64 handle);
GL_APICALL GLboolean GL_APIENTRY glIsImageHandleResidentNV(GLuint64 handle);
GL_APICALL void GL_APIENTRY glVertexAttribL1ui64NV(GLuint index, GLuint64EXT x);
GL_APICALL void GL_APIENTRY glVertexAttribL1ui64vNV(GLuint index, const GLuint64EXT *v);
GL_APICALL void GL_APIENTRY glGetVertexAttribLui64vNV(GLuint index, GLenum pname, GLuint64EXT *params);
#endif
#endif /* GL_NV_bindless_texture */

// Mobile multi-view
typedef void (GL_APIENTRYP PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint baseViewIndex, GLsizei numViews);
typedef void (GL_APIENTRYP PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLsizei samples, GLint baseViewIndex, GLsizei numViews);

// List entrypoints core to OpenGL ES2, unless linking directly
#define ENUM_GL_ENTRYPOINTS_CORE(EnumMacro)

// List all core OpenGL entry points used by Unreal that need to be loaded manually
#define ENUM_GL_ENTRYPOINTS_MANUAL(EnumMacro) \
	EnumMacro(PFNGLBEGINQUERYPROC,glBeginQuery) \
	EnumMacro(PFNGLBINDBUFFERBASEPROC,glBindBufferBase) \
	EnumMacro(PFNGLBINDSAMPLERPROC,glBindSampler) \
	EnumMacro(PFNGLBINDVERTEXARRAYPROC,glBindVertexArray) \
	EnumMacro(PFNGLBLITFRAMEBUFFERPROC,glBlitFramebuffer) \
	EnumMacro(PFNGLCLEARBUFFERFIPROC,glClearBufferfi) \
	EnumMacro(PFNGLCLEARBUFFERFVPROC,glClearBufferfv) \
	EnumMacro(PFNGLCLEARBUFFERIVPROC,glClearBufferiv) \
	EnumMacro(PFNGLCLEARBUFFERUIVPROC,glClearBufferuiv) \
	EnumMacro(PFNGLCLIENTWAITSYNCPROC,glClientWaitSync) \
	EnumMacro(PFNGLCOLORMASKIEXTPROC,glColorMaskiEXT) \
	EnumMacro(PFNGLCOMPRESSEDTEXIMAGE3DPROC,glCompressedTexImage3D) \
	EnumMacro(PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC,glCompressedTexSubImage3D) \
	EnumMacro(PFNGLCOPYBUFFERSUBDATAPROC,glCopyBufferSubData) \
	EnumMacro(PFNGLCOPYTEXSUBIMAGE3DPROC,glCopyTexSubImage3D) \
	EnumMacro(PFNGLDELETEQUERIESPROC,glDeleteQueries) \
	EnumMacro(PFNGLDELETESAMPLERSPROC,glDeleteSamplers) \
	EnumMacro(PFNGLDELETESYNCPROC,glDeleteSync)\
	EnumMacro(PFNGLDELETEVERTEXARRAYSPROC,glDeleteVertexArrays) \
	EnumMacro(PFNGLDISABLEIEXTPROC,glDisableiEXT) \
	EnumMacro(PFNGLDRAWARRAYSINSTANCEDPROC,glDrawArraysInstanced) \
	EnumMacro(PFNGLDRAWBUFFERSPROC,glDrawBuffers) \
	EnumMacro(PFNGLDRAWELEMENTSINSTANCEDPROC,glDrawElementsInstanced) \
	EnumMacro(PFNGLDRAWRANGEELEMENTSPROC,glDrawRangeElements) \
	EnumMacro(PFNGLENABLEIEXTPROC,glEnableiEXT) \
	EnumMacro(PFNGLENDQUERYPROC,glEndQuery) \
	EnumMacro(PFNGLFENCESYNCPROC,glFenceSync)\
	EnumMacro(PFNGLFLUSHMAPPEDBUFFERRANGEPROC,glFlushMappedBufferRange) \
	EnumMacro(PFNGLFRAMEBUFFERTEXTURELAYERPROC,glFramebufferTextureLayer) \
	EnumMacro(PFNGLFRAMEBUFFERTEXTUREEXTPROC,glFramebufferTextureEXT) \
	EnumMacro(PFNGLGENQUERIESPROC,glGenQueries) \
	EnumMacro(PFNGLGENSAMPLERSPROC,glGenSamplers) \
	EnumMacro(PFNGLGENVERTEXARRAYSPROC,glGenVertexArrays) \
	EnumMacro(PFNGLGETBOOLEANI_VPROC,glGetBooleani_v) \
	EnumMacro(PFNGLGETBUFFERPOINTERVPROC,glGetBufferPointerv) \
	EnumMacro(PFNGLGETINTEGERI_VPROC,glGetIntegeri_v) \
	EnumMacro(PFNGLGETQUERYIVPROC,glGetQueryiv) \
	EnumMacro(PFNGLGETQUERYOBJECTUIVPROC,glGetQueryObjectuiv) \
	EnumMacro(PFNGLGETSTRINGIPROC,glGetStringi) \
	EnumMacro(PFNGLGETSYNCIVPROC,glGetSynciv)\
	EnumMacro(PFNGLGETTEXLEVELPARAMETERFVPROC,glGetTexLevelParameterfv) \
	EnumMacro(PFNGLGETTEXLEVELPARAMETERIVPROC,glGetTexLevelParameteriv) \
	EnumMacro(PFNGLGETUNIFORMBLOCKINDEXPROC,glGetUniformBlockIndex) \
	EnumMacro(PFNGLISENABLEDIEXTPROC,glIsEnablediEXT) \
	EnumMacro(PFNGLISQUERYPROC,glIsQuery) \
	EnumMacro(PFNGLISSYNCPROC,glIsSync)\
	EnumMacro(PFNGLMAPBUFFERRANGEPROC,glMapBufferRange) \
	EnumMacro(PFNGLREADBUFFERPROC,glReadBuffer) \
	EnumMacro(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC,glRenderbufferStorageMultisample) \
	EnumMacro(PFNGLSAMPLERPARAMETERIPROC,glSamplerParameteri) \
	EnumMacro(PFNGLTEXBUFFEREXTPROC,glTexBufferEXT) \
	EnumMacro(PFNGLTEXIMAGE3DPROC,glTexImage3D) \
	EnumMacro(PFNGLTEXSUBIMAGE3DPROC,glTexSubImage3D) \
	EnumMacro(PFNGLUNIFORM1UIVPROC,glUniform1uiv) \
	EnumMacro(PFNGLUNIFORM2UIVPROC,glUniform2uiv) \
	EnumMacro(PFNGLUNIFORM3UIVPROC,glUniform3uiv) \
	EnumMacro(PFNGLUNIFORM4UIVPROC,glUniform4uiv) \
	EnumMacro(PFNGLUNIFORMBLOCKBINDINGPROC,glUniformBlockBinding) \
	EnumMacro(PFNGLUNIFORMMATRIX2X3FVPROC,glUniformMatrix2x3fv) \
	EnumMacro(PFNGLUNIFORMMATRIX2X4FVPROC,glUniformMatrix2x4fv) \
	EnumMacro(PFNGLUNIFORMMATRIX3X2FVPROC,glUniformMatrix3x2fv) \
	EnumMacro(PFNGLUNIFORMMATRIX3X4FVPROC,glUniformMatrix3x4fv) \
	EnumMacro(PFNGLUNIFORMMATRIX4X2FVPROC,glUniformMatrix4x2fv) \
	EnumMacro(PFNGLUNIFORMMATRIX4X3FVPROC,glUniformMatrix4x3fv) \
	EnumMacro(PFNGLUNMAPBUFFERPROC,glUnmapBuffer) \
	EnumMacro(PFNGLVERTEXATTRIBDIVISORPROC,glVertexAttribDivisor) \
	EnumMacro(PFNGLVERTEXATTRIBI4IVPROC,glVertexAttribI4iv) \
	EnumMacro(PFNGLVERTEXATTRIBI4UIVPROC,glVertexAttribI4uiv) \
	EnumMacro(PFNGLVERTEXATTRIBIPOINTERPROC,glVertexAttribIPointer) \
	EnumMacro(PFNGLBINDBUFFERRANGEPROC, glBindBufferRange)

#define ENUM_GL_ENTRYPOINTS_OPTIONAL(EnumMacro) \
	EnumMacro(PFNGLBINDIMAGETEXTUREPROC,glBindImageTexture) \
	EnumMacro(PFNGLBLENDEQUATIONIEXTPROC,glBlendEquationiEXT) \
	EnumMacro(PFNGLBLENDEQUATIONSEPARATEIEXTPROC,glBlendEquationSeparateiEXT) \
	EnumMacro(PFNGLBLENDFUNCIEXTPROC,glBlendFunciEXT) \
	EnumMacro(PFNGLBLENDFUNCSEPARATEIEXTPROC,glBlendFuncSeparateiEXT) \
	EnumMacro(PFNGLDEBUGMESSAGECALLBACKKHRPROC,glDebugMessageCallbackKHR) \
	EnumMacro(PFNGLDEBUGMESSAGECONTROLKHRPROC,glDebugMessageControlKHR) \
	EnumMacro(PFNGLDISPATCHCOMPUTEINDIRECTPROC,glDispatchComputeIndirect) \
	EnumMacro(PFNGLDISPATCHCOMPUTEPROC,glDispatchCompute) \
	EnumMacro(PFNGLGETACTIVEUNIFORMBLOCKIVPROC,glGetActiveUniformBlockiv) \
	EnumMacro(PFNGLGETACTIVEUNIFORMBLOCKNAMEPROC,glGetActiveUniformBlockName) \
	EnumMacro(PFNGLGETACTIVEUNIFORMSIVPROC,glGetActiveUniformsiv) \
	EnumMacro(PFNGLGETBUFFERPARAMETERI64VPROC,glGetBufferParameteri64v) \
	EnumMacro(PFNGLGETSAMPLERPARAMETERFVPROC,glGetSamplerParameterfv) \
	EnumMacro(PFNGLGETSAMPLERPARAMETERIVPROC,glGetSamplerParameteriv) \
	EnumMacro(PFNGLGETUNIFORMUIVPROC,glGetUniformuiv) \
	EnumMacro(PFNGLGETVERTEXATTRIBIUIVPROC,glGetVertexAttribIuiv) \
	EnumMacro(PFNGLMEMORYBARRIERPROC,glMemoryBarrier) \
	EnumMacro(PFNGLPATCHPARAMETERIEXTPROC, glPatchParameteriEXT)\
	EnumMacro(PFNGLBINDVERTEXBUFFERPROC, glBindVertexBuffer)\
	EnumMacro(PFNGLVERTEXATTRIBFORMATPROC, glVertexAttribFormat)\
	EnumMacro(PFNGLVERTEXATTRIBIFORMATPROC, glVertexAttribIFormat)\
	EnumMacro(PFNGLVERTEXATTRIBBINDINGPROC, glVertexAttribBinding)\
	EnumMacro(PFNGLVERTEXBINDINGDIVISORPROC, glVertexBindingDivisor)\
	EnumMacro(PFNGLCOPYIMAGESUBDATAEXTPROC, glCopyImageSubDataEXT)\
	EnumMacro(PFNGLTEXSTORAGE2DPROC, glTexStorage2D)\
	EnumMacro(PFNGLTEXSTORAGE3DPROC, glTexStorage3D)\
	EnumMacro(PFNGLTEXTUREVIEWEXTPROC, glTextureViewEXT)\
	EnumMacro(PFNGLTEXSTORAGE2DMULTISAMPLEPROC, glTexStorage2DMultisample)\
	EnumMacro(PFNGLDRAWELEMENTSINDIRECTPROC, glDrawElementsIndirect)\
	EnumMacro(PFNGLDRAWARRAYSINDIRECTPROC, glDrawArraysIndirect)\
	EnumMacro(PFNGLOBJECTLABELKHRPROC,glObjectLabelKHR) \
	EnumMacro(PFNGLOBJECTPTRLABELKHRPROC,glObjectPtrLabelKHR) \
	EnumMacro(PFNGLPOPDEBUGGROUPKHRPROC,glPopDebugGroupKHR) \
	EnumMacro(PFNGLPUSHDEBUGGROUPKHRPROC,glPushDebugGroupKHR) \
	EnumMacro(PFNGLMAPBUFFEROESPROC, glMapBufferOES) \
	EnumMacro(PFNGLUNMAPBUFFEROESPROC, glUnmapBufferOES) \
	EnumMacro(PFNGLQUERYCOUNTEREXTPROC, glQueryCounterEXT) \
	EnumMacro(PFNGLGETQUERYOBJECTUI64VEXTPROC, glGetQueryObjectui64vEXT) \
	EnumMacro(PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC, glFramebufferTexture2DMultisampleEXT) \
	EnumMacro(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC, glRenderbufferStorageMultisampleEXT) \
	EnumMacro(PFNGLGETTEXTUREHANDLENVPROC, glGetTextureHandleNV)\
	EnumMacro(PFNGLGETTEXTURESAMPLERHANDLENVPROC, glGetTextureSamplerHandleNV)\
	EnumMacro(PFNGLMAKETEXTUREHANDLERESIDENTNVPROC, glMakeTextureHandleResidentNV)\
	EnumMacro(PFNGLUNIFORMHANDLEUI64NVPROC, glUniformHandleui64NV)\
	EnumMacro(PFNGLMAKETEXTUREHANDLENONRESIDENTNVPROC, glMakeTextureHandleNonResidentNV)\
	EnumMacro(PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC, glFramebufferTextureMultiviewOVR)\
	EnumMacro(PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC, glFramebufferTextureMultisampleMultiviewOVR)

extern "C"
{
	extern PFNEGLGETSYSTEMTIMENVPROC eglGetSystemTimeNV_p;
	extern PFNEGLCREATESYNCKHRPROC eglCreateSyncKHR_p;
	extern PFNEGLDESTROYSYNCKHRPROC eglDestroySyncKHR_p;
	extern PFNEGLCLIENTWAITSYNCKHRPROC eglClientWaitSyncKHR_p;
}

// List of all OpenGL entry points
#define ENUM_GL_ENTRYPOINTS_ALL(EnumMacro) \
	ENUM_GL_ENTRYPOINTS_CORE(EnumMacro) \
	ENUM_GL_ENTRYPOINTS_MANUAL(EnumMacro) \
	ENUM_GL_ENTRYPOINTS_OPTIONAL(EnumMacro)

// Declare all GL functions
#define DECLARE_GL_ENTRYPOINTS(Type,Func) extern Type Func;
ENUM_GL_ENTRYPOINTS_ALL(DECLARE_GL_ENTRYPOINTS);

#include "OpenGLESDeferred.h"

struct FAndroidESDeferredOpenGL : public FOpenGLESDeferred
{
	static FORCEINLINE EShaderPlatform GetShaderPlatform()
	{
		return bES2Fallback ? SP_OPENGL_ES2_ANDROID : SP_OPENGL_ES31_EXT;
	}

	static FORCEINLINE void InitDebugContext()
	{
		bDebugContext = glIsEnabled( GL_DEBUG_OUTPUT_KHR) != GL_FALSE;
	}

	static FORCEINLINE void LabelObject(GLenum Type, GLuint Object, const ANSICHAR* Name)
	{
		if (glObjectLabelKHR && bDebugContext)
		{
			glObjectLabelKHR(Type, Object, -1, Name);
		}
	}

	static FORCEINLINE void PushGroupMarker(const ANSICHAR* Name)
	{
		if (glPushDebugGroupKHR && bDebugContext)
		{
			glPushDebugGroupKHR( GL_DEBUG_SOURCE_APPLICATION_KHR, 1, -1,Name);
		}
	}

	static FORCEINLINE void PopGroupMarker()
	{
		if (glPopDebugGroupKHR && bDebugContext)
		{
			glPopDebugGroupKHR();
		}
	}

	static FORCEINLINE bool TexStorage2D(GLenum Target, GLint Levels, GLint InternalFormat, GLsizei Width, GLsizei Height, GLenum Format, GLenum Type, uint32 Flags)
	{
		if( glTexStorage2D != NULL )
		{
			glTexStorage2D(Target, Levels, InternalFormat, Width, Height);
			return true;
		}
		else
		{
			return false;
		}
	}

	static FORCEINLINE void TexStorage3D(GLenum Target, GLint Levels, GLint InternalFormat, GLsizei Width, GLsizei Height, GLsizei Depth, GLenum Format, GLenum Type)
	{
		if (glTexStorage3D)
		{
			glTexStorage3D( Target, Levels, InternalFormat, Width, Height, Depth);
		}
		else
		{
			const bool bArrayTexture = Target == GL_TEXTURE_2D_ARRAY || Target == GL_TEXTURE_CUBE_MAP_ARRAY;

			for(uint32 MipIndex = 0; MipIndex < uint32(Levels); MipIndex++)
			{
				glTexImage3D(
					Target,
					MipIndex,
					InternalFormat,
					FMath::Max<uint32>(1,(Width >> MipIndex)),
					FMath::Max<uint32>(1,(Height >> MipIndex)),
					(bArrayTexture) ? Depth : FMath::Max<uint32>(1,(Depth >> MipIndex)),
					0,
					Format,
					Type,
					NULL
					);
			}
		}
	}

	static FORCEINLINE void CopyImageSubData(GLuint SrcName, GLenum SrcTarget, GLint SrcLevel, GLint SrcX, GLint SrcY, GLint SrcZ, GLuint DstName, GLenum DstTarget, GLint DstLevel, GLint DstX, GLint DstY, GLint DstZ, GLsizei Width, GLsizei Height, GLsizei Depth)
	{
		glCopyImageSubDataEXT( SrcName, SrcTarget, SrcLevel, SrcX, SrcY, SrcZ, DstName, DstTarget, DstLevel, DstX, DstY, DstZ, Width, Height, Depth);
	}

	static FORCEINLINE void QueryTimestampCounter(GLuint QueryId)
	{
		glQueryCounterEXT(QueryId, GL_TIMESTAMP_EXT);
	}

	static FORCEINLINE void GetQueryObject(GLuint QueryId, EQueryMode QueryMode, GLuint *OutResult)
	{
		GLenum QueryName = (QueryMode == QM_Result) ? GL_QUERY_RESULT_EXT : GL_QUERY_RESULT_AVAILABLE_EXT;
		glGetQueryObjectuiv(QueryId, QueryName, OutResult);
	}

	static FORCEINLINE void GetQueryObject(GLuint QueryId, EQueryMode QueryMode, GLuint64* OutResult)
	{
		GLenum QueryName = (QueryMode == QM_Result) ? GL_QUERY_RESULT_EXT : GL_QUERY_RESULT_AVAILABLE_EXT;
		GLuint64 Result = 0;
		glGetQueryObjectui64vEXT(QueryId, QueryName, &Result);
		*OutResult = Result;
	}

	static FORCEINLINE void DeleteSync(UGLsync Sync)
	{
		if (IsES2())
		{ 
			if (GUseThreadedRendering)
			{
				//handle error here
				EGLBoolean Result = eglDestroySyncKHR_p(AndroidEGL::GetInstance()->GetDisplay(), Sync.EGL);
				if (Result == EGL_FALSE)
				{
					//handle error here
				}
			}
		}
		else
		{
			glDeleteSync(Sync.GL);
		}
	}

	static FORCEINLINE UGLsync FenceSync(GLenum Condition, GLbitfield Flags)
	{
		if (IsES2())
		{
			check(Condition == GL_SYNC_GPU_COMMANDS_COMPLETE && Flags == 0);
			return UGLsync(GUseThreadedRendering ? eglCreateSyncKHR_p(AndroidEGL::GetInstance()->GetDisplay(), EGL_SYNC_FENCE_KHR, NULL) : 0);
		}
		else
		{
			return UGLsync(glFenceSync(Condition, Flags));
		}
	}

	static FORCEINLINE bool IsSync(UGLsync Sync)
	{
		if (IsES2())
		{
			if (GUseThreadedRendering)
			{
				return (Sync.EGL != EGL_NO_SYNC_KHR) ? true : false;
			}
			else
			{
				return true;
			}
		}
		else
		{
			return (glIsSync(Sync.GL) == GL_TRUE) ? true : false;
		}
	}

	static FORCEINLINE EFenceResult ClientWaitSync(UGLsync Sync, GLbitfield Flags, GLuint64 Timeout)
	{
		if (IsES2())
		{
			if (GUseThreadedRendering)
			{
				// check( Flags == GL_SYNC_FLUSH_COMMANDS_BIT );
				GLenum Result = eglClientWaitSyncKHR_p(AndroidEGL::GetInstance()->GetDisplay(), Sync.EGL, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, Timeout);
				switch (Result)
				{
					case EGL_TIMEOUT_EXPIRED_KHR:		return FR_TimeoutExpired;
					case EGL_CONDITION_SATISFIED_KHR:	return FR_ConditionSatisfied;
				}
				return FR_WaitFailed;
			}
			else
			{
				return FR_ConditionSatisfied;
			}
			return FR_WaitFailed;
		}
		else
		{
			GLenum Result = glClientWaitSync(Sync.GL, Flags, Timeout);
			switch (Result)
			{
				case GL_ALREADY_SIGNALED:		return FR_AlreadySignaled;
				case GL_TIMEOUT_EXPIRED:		return FR_TimeoutExpired;
				case GL_CONDITION_SATISFIED:	return FR_ConditionSatisfied;
			}
			return FR_WaitFailed;

		}
	}
	static FORCEINLINE void EnableIndexed(GLenum Parameter, GLuint Index)
	{
		if (IsES2())
		{
			check(Parameter == GL_BLEND);
			glEnable(Parameter);
		}
		else
		{
			glEnableiEXT(Parameter, Index);
		}
	}

	static FORCEINLINE void DisableIndexed(GLenum Parameter, GLuint Index)
	{
		if (IsES2())
		{
			check(Parameter == GL_BLEND);

			glDisable(Parameter);
		}
		else
		{

			glDisableiEXT(Parameter, Index);
		}
	}

	static FORCEINLINE void ColorMaskIndexed(GLuint Index, GLboolean Red, GLboolean Green, GLboolean Blue, GLboolean Alpha)
	{
		if (IsES2())
		{
			check(Index == 0);
			glColorMask(Red, Green, Blue, Alpha);
		}
		else
		{
			glColorMaskiEXT(Index, Red, Green, Blue, Alpha);
		}
	}

	static FORCEINLINE void ReadBuffer(GLenum Mode)
	{
		if (IsES2())
		{

		}
		else
		{
			glReadBuffer(Mode);
		}
	}

	static FORCEINLINE void DrawBuffer(GLenum Mode)
	{
		if (IsES2())
		{
		}
		else
		{
			glDrawBuffers(1, &Mode);
		}
	}

	static FORCEINLINE bool IsES2()				{ return bES2Fallback; }

	static FORCEINLINE bool SupportsBindlessTexture()
	{
		return bSupportsBindlessTexture;
	}

	static FORCEINLINE bool SupportsMobileMultiView()
	{
		return bSupportsMobileMultiView;
	}

	static FORCEINLINE bool SupportsImageExternal()
	{
		return false;
	}

	enum class EImageExternalType : uint8
	{
		None,
		ImageExternal100,
		ImageExternal300,
		ImageExternalESSL300
	};

	static FORCEINLINE EImageExternalType GetImageExternalType()
	{
		return EImageExternalType::None;
	}

	static FORCEINLINE GLuint64 GetTextureSamplerHandle(GLuint Texture, GLuint Sampler)
	{
		return glGetTextureSamplerHandleNV(Texture, Sampler);
	}

	static FORCEINLINE GLuint64 GetTextureHandle(GLuint Texture)
	{
		return glGetTextureHandleNV(Texture);
	}

	static FORCEINLINE void MakeTextureHandleResident(GLuint64 TextureHandle)
	{
		glMakeTextureHandleResidentNV(TextureHandle);
	}

	static FORCEINLINE void MakeTextureHandleNonResident(GLuint64 TextureHandle)
	{
		glMakeTextureHandleNonResidentNV(TextureHandle);
	}

	static FORCEINLINE void UniformHandleui64(GLint Location, GLuint64 Value)
	{
		glUniformHandleui64NV(Location, Value);
	}

	static void ProcessExtensions(const FString& ExtensionsString);

	// whether NV_bindless_texture is supported
	static bool bSupportsBindlessTexture;

	/** Whether device supports mobile multi-view */
	static bool bSupportsMobileMultiView;
};

typedef FAndroidESDeferredOpenGL FOpenGL;
