// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IOSOpenGL.h: Public OpenGL ES 2.0 definitions for iOS-specific functionality
=============================================================================*/

#pragma once

#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>


typedef GLsync UGLsync;

#include "OpenGLES2.h"

/** Unreal tokens that maps to different OpenGL tokens by platform. */
#undef UGL_DRAW_FRAMEBUFFER
#define UGL_DRAW_FRAMEBUFFER	GL_DRAW_FRAMEBUFFER_APPLE
#undef UGL_READ_FRAMEBUFFER
#define UGL_READ_FRAMEBUFFER	GL_READ_FRAMEBUFFER_APPLE

#undef GL_TEXTURE_MAX_LEVEL
#define GL_TEXTURE_MAX_LEVEL	GL_TEXTURE_MAX_LEVEL_APPLE


struct FIOSOpenGL : public FOpenGLES2
{
	static FORCEINLINE bool SupportsFences()					{ return true; }	// GL_APPLE_sync
	static FORCEINLINE bool SupportsTextureMaxLevel()			{ return true; }	// GL_APPLE_texture_max_level
	static FORCEINLINE bool HasHardwareHiddenSurfaceRemoval()	{ return true; }	// All iOS devices have PowerVR GPUs with hardware HSR
	static FORCEINLINE bool SupportsInstancing()				{ return true; }	// All iOS devices have EXT_draw_instanced + EXT_instanced_arrays

	static FORCEINLINE void DeleteSync(UGLsync Sync)
	{
		glDeleteSyncAPPLE( Sync );
	}

	static FORCEINLINE UGLsync FenceSync(GLenum Condition, GLbitfield Flags)
	{
		check( Condition == GL_SYNC_GPU_COMMANDS_COMPLETE && Flags == 0 );
		return glFenceSyncAPPLE( GL_SYNC_GPU_COMMANDS_COMPLETE_APPLE, 0 );
	}

	static FORCEINLINE bool IsSync(UGLsync Sync)
	{
		return (glIsSyncAPPLE( Sync ) == GL_TRUE) ? true : false;
	}

	static FORCEINLINE EFenceResult ClientWaitSync(UGLsync Sync, GLbitfield Flags, GLuint64 Timeout)
	{
//		check( Flags == GL_SYNC_FLUSH_COMMANDS_BIT );
		GLenum Result = glClientWaitSyncAPPLE( Sync, GL_SYNC_FLUSH_COMMANDS_BIT_APPLE, Timeout );
		switch (Result)
		{
			case GL_ALREADY_SIGNALED_APPLE:		return FR_AlreadySignaled;
			case GL_TIMEOUT_EXPIRED_APPLE:		return FR_TimeoutExpired;
			case GL_CONDITION_SATISFIED_APPLE:	return FR_ConditionSatisfied;
		}
		return FR_WaitFailed;
	}

	static FORCEINLINE bool TexStorage2D(GLenum Target, GLint Levels, GLint InternalFormat, GLsizei Width, GLsizei Height, GLenum Format, GLenum Type, uint32 Flags)
	{
		// TexStorage2D only seems to work with power-of-two textures and also fails for floating point textures & depth-stencil.
		if( bSupportsTextureStorageEXT && FMath::IsPowerOfTwo(Width) && FMath::IsPowerOfTwo(Height) && Type != GL_HALF_FLOAT_OES  && InternalFormat != GL_DEPTH_STENCIL)
		{
			glTexStorage2DEXT(Target, Levels, InternalFormat, Width, Height);
			VERIFY_GL(glTexStorage2DEXT)
			return true;
		}
		else
		{
			return false;
		}
	}

	static FORCEINLINE void CopyTextureLevels(GLuint destinationTexture, GLuint sourceTexture, GLint sourceBaseLevel, GLsizei sourceLevelCount)
	{
		// flush is to prevent driver crashing running out of memory in the Parameter Buffer
		glFlush();
		glCopyTextureLevelsAPPLE(destinationTexture, sourceTexture, sourceBaseLevel, sourceLevelCount);
		VERIFY_GL(glCopyTextureLevelsAPPLE)
	}

	static FORCEINLINE bool SupportsMapBuffer() { return true; }

	static FORCEINLINE void* MapBufferRange(GLenum Type, uint32 InOffset, uint32 InSize, EResourceLockMode LockMode)
	{
		checkf(LockMode == RLM_WriteOnly || LockMode == RLM_WriteOnlyUnsynchronized, TEXT("OpenGL ES 2.0 only supports write-only buffer locks"));
		check(Type == GL_ARRAY_BUFFER || Type == GL_ELEMENT_ARRAY_BUFFER);
		GLuint Flags = GL_MAP_WRITE_BIT_EXT | GL_MAP_FLUSH_EXPLICIT_BIT_EXT;
		if (LockMode == RLM_WriteOnlyUnsynchronized)
		{
			Flags |= GL_MAP_UNSYNCHRONIZED_BIT_EXT;
		}
		uint8_t* Data = (uint8*)glMapBufferRangeEXT(Type, InOffset, InSize, Flags);
		return Data ? Data + InOffset : NULL;
	}

	static FORCEINLINE void UnmapBufferRange(GLenum Type, uint32 InOffset, uint32 InSize)
	{
		glFlushMappedBufferRangeEXT(Type, InOffset, InSize);
		glUnmapBufferOES(Type);
	}

	static FORCEINLINE void DrawArraysInstanced(GLenum Mode, GLint First, GLsizei Count, GLsizei InstanceCount)
	{
		glDrawArraysInstancedEXT(Mode, First, Count, InstanceCount);
	}

	static FORCEINLINE void DrawElementsInstanced(GLenum Mode, GLsizei Count, GLenum Type, const GLvoid* Indices, GLsizei InstanceCount)
	{
		glDrawElementsInstancedEXT(Mode, Count, Type, Indices, InstanceCount);
	}

	static FORCEINLINE void VertexAttribDivisor(GLuint Index, GLuint Divisor)
	{
		glVertexAttribDivisorEXT(Index, Divisor);
	}
	
	static FORCEINLINE EShaderPlatform GetShaderPlatform()
	{
		return SP_OPENGL_ES2_IOS;
	}

	static FORCEINLINE bool SupportsFramebufferSRGBEnable()				{ return false; }

	static FORCEINLINE GLenum GetShadowDepthFormat()					{ return GL_DEPTH_COMPONENT16; }
};

typedef FIOSOpenGL FOpenGL;
