// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HTML5OpenGL.h: Public OpenGL ES 2.0 definitions for HTML5-specific functionality
=============================================================================*/

#pragma once

#define GL_GLEXT_PROTOTYPES
#include <SDL_opengles2.h>

typedef char UGLsync;
typedef long long  GLint64;
typedef unsigned long long  GLuint64;

// empty functions.
#include <GLES3/gl3.h>
void glDeleteQueriesEXT(GLsizei n,  const GLuint * ids);
void glGenQueriesEXT(GLsizei n,  GLuint * ids);
void glBeginQueryEXT(GLenum target,  GLuint id);
void glEndQueryEXT (GLenum target);
void glGetQueryObjectuivEXT(GLuint id,  GLenum pname,  GLuint * params);
void glLabelObjectEXT(GLenum type, GLuint object, GLsizei length, const GLchar *label);
void glPushGroupMarkerEXT (GLsizei length, const GLchar *marker);
void glPopGroupMarkerEXT (void);
void glGetObjectLabelEXT (GLenum type, GLuint object, GLsizei bufSize, GLsizei *length, GLchar *label);

#define GL_BGRA									0x80E1
#define GL_QUERY_COUNTER_BITS_EXT				0x8864
#define GL_CURRENT_QUERY_EXT					0x8865
#define GL_QUERY_RESULT_EXT						0x8866
#define GL_QUERY_RESULT_AVAILABLE_EXT			0x8867
#define GL_SAMPLES_PASSED_EXT					0x8914
#define GL_ANY_SAMPLES_PASSED_EXT				0x8C2F

#ifndef GL_LUMINANCE8_EXT
#define GL_LUMINANCE8_EXT GL_LUMINANCE
#endif

#ifndef GL_BGRA8_EXT
#define GL_BGRA8_EXT GL_BGRA
#endif

#ifndef GL_ALPHA8_EXT
#define GL_ALPHA8_EXT GL_ALPHA
#endif




/** Unreal tokens that maps to different OpenGL tokens by platform. */
#ifndef GL_DRAW_FRAMEBUFFER
#define GL_DRAW_FRAMEBUFFER	GL_FRAMEBUFFER
#endif
#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER GL_FRAMEBUFFER
#endif

#include "OpenGLES2.h"

struct FHTML5OpenGL : public FOpenGLES2
{
	static FORCEINLINE bool IsSync(UGLsync Sync)
	{
		return true;
	}

	static FORCEINLINE EFenceResult ClientWaitSync(UGLsync Sync, GLbitfield Flags, GLuint64 Timeout)
	{
		return FR_ConditionSatisfied;
	}

	static FORCEINLINE void CheckFrameBuffer()
	{
		GLenum CompleteResult = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (CompleteResult != GL_FRAMEBUFFER_COMPLETE)
		{
			UE_LOG(LogRHI, Warning,TEXT("Framebuffer not complete. Status = 0x%x"), CompleteResult);
		}
	}

	static FORCEINLINE bool SupportsFastBufferData()
	{
		// In WebGL, rendering without VBOs is not supported at all, so avoid rendering from client side memory altogether.
		// (The fastest way to upload dynamic vertex data in WebGL is to precreate a VBO with glBufferData() at load time,
		// and use glBufferSubData() to upload to it each frame)
		return true;
	}

	static FORCEINLINE bool SupportsMapBuffer()							{ return false; }
	static FORCEINLINE bool SupportsCombinedDepthStencilAttachment()	{ return bCombinedDepthStencilAttachment; }
	static FORCEINLINE bool SupportsMultipleRenderTargets()				{ return bIsWebGL2; }
	static FORCEINLINE bool SupportsInstancing()						{ return bSupportsInstancing; }
	static FORCEINLINE bool SupportsDrawBuffers()						{ return bSupportsDrawBuffers; }
// TODO: Uncomment the following when migrating shaders fully to GLES3/WebGL2.
//	static FORCEINLINE bool SupportsUniformBuffers()					{ return bIsWebGL2; }
	static FORCEINLINE bool SupportsBlitFramebuffer()					{ return bIsWebGL2; }
	static FORCEINLINE GLenum GetDepthFormat()							{ return GL_DEPTH_COMPONENT; }
	static FORCEINLINE GLenum GetShadowDepthFormat()					{ return GL_DEPTH_COMPONENT; }
	// Optional
	static FORCEINLINE void BeginQuery(GLenum QueryType, GLuint QueryId) UGL_OPTIONAL_VOID
	static FORCEINLINE void EndQuery(GLenum QueryType) UGL_OPTIONAL_VOID
	static FORCEINLINE void LabelObject(GLenum Type, GLuint Object, const ANSICHAR* Name)UGL_OPTIONAL_VOID
	static FORCEINLINE void PushGroupMarker(const ANSICHAR* Name)UGL_OPTIONAL_VOID
	static FORCEINLINE void PopGroupMarker() UGL_OPTIONAL_VOID
	static FORCEINLINE void GetQueryObject(GLuint QueryId, EQueryMode QueryMode, uint64 *OutResult) UGL_OPTIONAL_VOID
	static FORCEINLINE void* MapBufferRange(GLenum Type, uint32 InOffset, uint32 InSize, EResourceLockMode LockMode)	UGL_OPTIONAL(NULL)
	static FORCEINLINE void UnmapBuffer(GLenum Type) UGL_OPTIONAL_VOID
	static FORCEINLINE void UnmapBufferRange(GLenum Type, uint32 InOffset, uint32 InSize)UGL_OPTIONAL_VOID
	static FORCEINLINE void GenQueries(GLsizei NumQueries, GLuint* QueryIDs) UGL_OPTIONAL_VOID
	static FORCEINLINE void DeleteQueries(GLsizei NumQueries, const GLuint* QueryIDs )UGL_OPTIONAL_VOID
	static FORCEINLINE void GetQueryObject(GLuint QueryId, EQueryMode QueryMode, GLuint *OutResult)UGL_OPTIONAL_VOID
	static FORCEINLINE void FramebufferTexture2D(GLenum Target, GLenum Attachment, GLenum TexTarget, GLuint Texture, GLint Level)
	{
		// XXX HACK: Currently rendering to mip levels produces incorrect results. Don't know if it is the UE4 engine fault
		//           or web browser WebGL 2 implementation fault. To workaround for now, ignore rendering to mip levels.
		if (Level != 0) return;

		check(Attachment == GL_COLOR_ATTACHMENT0 || Attachment == GL_DEPTH_ATTACHMENT || Attachment == GL_DEPTH_STENCIL_ATTACHMENT);
		glFramebufferTexture2D(Target, Attachment, TexTarget, Texture, Level);
		VERIFY_GL(FramebufferTexture_2D)
	}

	static FORCEINLINE void BlitFramebuffer(GLint SrcX0, GLint SrcY0, GLint SrcX1, GLint SrcY1, GLint DstX0, GLint DstY0, GLint DstX1, GLint DstY1, GLbitfield Mask, GLenum Filter)
	{
#ifdef UE4_HTML5_TARGET_WEBGL2
		if (bIsWebGL2)
		{
			glBlitFramebuffer(SrcX0, SrcY0, SrcX1, SrcY1, DstX0, DstY0, DstX1, DstY1, Mask, Filter);
		}
#endif
	}

	static FORCEINLINE void DrawArraysInstanced(GLenum Mode, GLint First, GLsizei Count, GLsizei InstanceCount)
	{
		glDrawArraysInstanced(Mode, First, Count, InstanceCount);
	}

	static FORCEINLINE void DrawElementsInstanced(GLenum Mode, GLsizei Count, GLenum Type, const GLvoid* Indices, GLsizei InstanceCount)
	{
		glDrawElementsInstanced(Mode, Count, Type, Indices, InstanceCount);
	}

	static FORCEINLINE void VertexAttribDivisor(GLuint Index, GLuint Divisor)
	{
		if (SupportsInstancing() && currentVertexAttribDivisor[Index] != Divisor)
		{
			glVertexAttribDivisor(Index, Divisor);
			currentVertexAttribDivisor[Index] = Divisor;
		}
	}

	static FORCEINLINE void DrawBuffers(GLsizei NumBuffers, const GLenum* Buffers)
	{
		glDrawBuffers(NumBuffers, Buffers);
	}

	static FORCEINLINE void ClearBufferfv(GLenum Buffer, GLint DrawBufferIndex, const GLfloat* Value)
	{
#ifdef UE4_HTML5_TARGET_WEBGL2
		if (bIsWebGL2)
		{
			glClearBufferfv(Buffer, DrawBufferIndex, Value);
		}
#endif
	}

	static FORCEINLINE void ClearBufferfi(GLenum Buffer, GLint DrawBufferIndex, GLfloat Depth, GLint Stencil)
	{
#ifdef UE4_HTML5_TARGET_WEBGL2
		if (bIsWebGL2)
		{
			glClearBufferfi(Buffer, DrawBufferIndex, Depth, Stencil);
		}
#endif
	}

	static FORCEINLINE void ClearBufferiv(GLenum Buffer, GLint DrawBufferIndex, const GLint* Value)
	{
#ifdef UE4_HTML5_TARGET_WEBGL2
		if (bIsWebGL2)
		{
			glClearBufferiv(Buffer, DrawBufferIndex, Value);
		}
#endif
	}

	static FORCEINLINE void BufferSubData(GLenum Target, GLintptr Offset, GLsizeiptr Size, const GLvoid* Data)
	{
		glBufferSubData(Target, Offset, Size, Data);
	}

	static FORCEINLINE void BindBufferBase(GLenum Target, GLuint Index, GLuint Buffer)
	{
		glBindBufferBase(Target, Index, Buffer);
	}

	static FORCEINLINE void BindBufferRange(GLenum Target, GLuint Index, GLuint Buffer, GLintptr Offset, GLsizeiptr Size)
	{
#ifdef UE4_HTML5_TARGET_WEBGL2
		if (bIsWebGL2)
		{
			glBindBufferRange(Target, Index, Buffer, Offset, Size);
		}
#endif
	}

	static FORCEINLINE GLuint GetUniformBlockIndex(GLuint Program, const GLchar* UniformBlockName)
	{
		return glGetUniformBlockIndex(Program, UniformBlockName);
	}

	static FORCEINLINE void UniformBlockBinding(GLuint Program, GLuint UniformBlockIndex, GLuint UniformBlockBinding)
	{
		glUniformBlockBinding(Program, UniformBlockIndex, UniformBlockBinding);
	}

	static FORCEINLINE void DiscardFramebufferEXT(GLenum Target, GLsizei NumAttachments, const GLenum* Attachments) UGL_OPTIONAL_VOID
	static void ProcessExtensions(const FString& ExtensionsString);

	static FORCEINLINE EShaderPlatform GetShaderPlatform()
	{
		// TODO: When WebGL 2.1 ships, make this return SP_OPENGL_ES31_EXT or SP_OPENGL_ES3_1_ANDROID (figure out which one maps better)
		return SP_OPENGL_ES2_WEBGL;
	}

	static FORCEINLINE FString GetAdapterName() { return TEXT(""); }

	// TODO: Make this true or false depending on whether WebGL 2 is available or not.
	static FORCEINLINE bool UseES30ShadingLanguage() { return false;}

	static FORCEINLINE void ProgramUniform4uiv(GLuint Program, GLint Location, GLsizei Count, const GLuint *Value)
	{
#ifdef UE4_HTML5_TARGET_WEBGL2
		if (bIsWebGL2)
		{
			glUniform4uiv(Location, Count, Value);
		}
#endif
	}

	static FORCEINLINE GLsizei GetLabelObject(GLenum Type, GLuint Object, GLsizei BufferSize, ANSICHAR* OutName)
	{
		return 0;
	}

	static FORCEINLINE int32 GetReadHalfFloatPixelsEnum()				{ return GL_FLOAT; }
protected:

	/** http://www.khronos.org/registry/webgl/extensions/WEBGL_depth_texture/ or WebGL 2.0 */
	static bool bCombinedDepthStencilAttachment;

	/** WEBGL_draw_buffers or WebGL 2.0 */
	static bool bSupportsDrawBuffers;

	/** ANGLE_instanced_arrays or WebGL 2.0 */
	static bool bSupportsInstancing;

	static bool bIsWebGL2;

	// WebGL API calls are slow, so do a state cache to remember the previous call to glVertexAttribDivisor so that we don't call these if they don't actually change.
	// TODO: When adding support for VAOs, whenever a new VAO is bound, call ResetVertexAttribDivisorCache() to reset this cache.
	static uint8_t currentVertexAttribDivisor[64];

	static void ResetVertexAttribDivisorCache()
	{
		memset(currentVertexAttribDivisor, 0xFF, sizeof(currentVertexAttribDivisor));
	}
};


typedef FHTML5OpenGL FOpenGL;

