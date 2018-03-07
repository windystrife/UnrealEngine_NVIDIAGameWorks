// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLUtil.h: OpenGL RHI utility definitions.
=============================================================================*/

#pragma once

/** Set to 1 to enable the ability to dump OpenGL frame debug functionality */
#define ENABLE_OPENGL_FRAMEDUMP 0

/** Set to 1 to enable the VERIFY_GL macros which call glGetError */
#define ENABLE_VERIFY_GL (0 & UE_BUILD_DEBUG)
#define ENABLE_VERIFY_GL_TRACE 0

/** Set to 1 to verify that the the engine side uniform buffer layout matches the driver side of the GLSL shader*/
#define ENABLE_UNIFORM_BUFFER_LAYOUT_VERIFICATION ( 0 & UE_BUILD_DEBUG & (OPENGL_ESDEFERRED | OPENGL_GL3 | OPENGL_GL4))

/** Set to 1 to additinally dump uniform buffer layout at shader link time, this assumes ENABLE_UNIFORM_BUFFER_LAYOUT_VERIFICATION == 1 */
#define ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP 0

/** Set to 1 to enable shader debugging which e.g. keeps the GLSL source as members of TOpenGLShader*/
#define DEBUG_GL_SHADERS (UE_BUILD_DEBUG)

/** Set to 1 to enable calls to place event markers into the OpenGL stream
    this is purposefully not considered for OPENGL_PERFORMANCE_DATA_INVALID, 
	since there is an additional cvar OpenGLConsoleVariables::bEnableARBDebug*/

#define ENABLE_OPENGL_DEBUG_GROUPS 1

#define OPENGL_PERFORMANCE_DATA_INVALID (ENABLE_OPENGL_FRAMEDUMP | ENABLE_VERIFY_GL | ENABLE_UNIFORM_BUFFER_LAYOUT_VERIFICATION | DEBUG_GL_SHADERS)

/**
* Convert from ECubeFace to GLenum type
* @param Face - ECubeFace type to convert
* @return OpenGL cube face enum value
*/
FORCEINLINE GLenum GetOpenGLCubeFace(ECubeFace Face)
{
	switch(Face)
	{
	case CubeFace_PosX:
	default:
		return GL_TEXTURE_CUBE_MAP_POSITIVE_X;
	case CubeFace_NegX:
		return GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
	case CubeFace_PosY:
		return GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
	case CubeFace_NegY:
		return GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
	case CubeFace_PosZ:
		return GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
	case CubeFace_NegZ:
		return GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
	};
}

#if ENABLE_VERIFY_GL
	extern bool PlatformOpenGLContextValid();
	extern int32 PlatformGlGetError();

	void VerifyOpenGLResult(GLenum ErrorCode, const TCHAR* Msg1, const TCHAR* Msg2, const TCHAR* Filename, uint32 Line);
	#define VERIFY_GL(msg) { GLenum ErrorCode = PlatformGlGetError(); if (ErrorCode != GL_NO_ERROR) { VerifyOpenGLResult(ErrorCode,TEXT(#msg),TEXT(""),TEXT(__FILE__),__LINE__); } }

	struct FOpenGLErrorScope
	{
		const TCHAR* FunctionName;
		const TCHAR* Filename;
		const uint32 Line;

		FOpenGLErrorScope(
			const TCHAR* InFunctionName,
			const TCHAR* InFilename,
			const uint32 InLine)
			: FunctionName(InFunctionName)
			, Filename(InFilename)
			, Line(InLine)
		{
#if ENABLE_VERIFY_GL_TRACE
			UE_LOG(LogRHI, Log, TEXT("log before %s(%d): %s"), InFilename, InLine, InFunctionName);
#endif
			CheckForErrors(0);
		}

		~FOpenGLErrorScope()
		{
#if ENABLE_VERIFY_GL_TRACE
			UE_LOG(LogRHI, Log, TEXT("log after  %s(%d): %s"), Filename, Line, FunctionName);

#endif

			CheckForErrors(1);
		}

		void CheckForErrors(int32 BeforeOrAfter)
		{
			check(PlatformOpenGLContextValid());

			GLenum ErrorCode = PlatformGlGetError();
			if (ErrorCode != GL_NO_ERROR)
			{
				const TCHAR* PrefixStrings[] = { TEXT("Before "), TEXT("During ") };
				VerifyOpenGLResult(ErrorCode,PrefixStrings[BeforeOrAfter],FunctionName,Filename,Line);
			}
		}
	};
	#define MACRO_TOKENIZER(IdentifierName, Msg, FileName, LineNumber) FOpenGLErrorScope IdentifierName_ ## LineNumber (Msg, FileName, LineNumber)
	#define MACRO_TOKENIZER2(IdentifierName, Msg, FileName, LineNumber) MACRO_TOKENIZER(IdentiferName, Msg, FileName, LineNumber)
	#define VERIFY_GL_SCOPE_WITH_MSG_STR(MsgStr) MACRO_TOKENIZER2(ErrorScope_, MsgStr, TEXT(__FILE__), __LINE__)
	#define VERIFY_GL_SCOPE() VERIFY_GL_SCOPE_WITH_MSG_STR(ANSI_TO_TCHAR(__FUNCTION__))
	#define VERIFY_GL_FUNC(Func, ...) { VERIFY_GL_SCOPE_WITH_MSG_STR(TEXT(#Func)); Func(__VA_ARGS__); }

	/**
	 * Some important GL calls are trapped individually.
	 */
	#define glBlitFramebuffer(...) VERIFY_GL_FUNC(glBlitFramebuffer, __VA_ARGS__)
	#define glTexImage2D(...) VERIFY_GL_FUNC(glTexImage2D, __VA_ARGS__)
	#define glTexSubImage2D(...) VERIFY_GL_FUNC(glTexSubImage2D, __VA_ARGS__)
	#define glCompressedTexImage2D(...) VERIFY_GL_FUNC(glCompressedTexImage2D, __VA_ARGS__)

#else
	#define VERIFY_GL(...)
	#define VERIFY_GL_SCOPE(...)
#endif

/** OpenGL frame dump debug functionality */
#if ENABLE_OPENGL_FRAMEDUMP
	extern "C"
	{
		extern void SignalOpenGLDrawArraysEvent( GLenum Mode, GLint First, GLsizei Count );
		extern void SignalOpenGLDrawArraysInstancedEvent( GLenum Mode, GLint First, GLsizei Count, GLsizei PrimCount );
		extern void SignalOpenGLDrawRangeElementsEvent( GLenum Mode, GLuint Start, GLuint End, GLsizei Count, GLenum Type, const GLvoid* Indices );
		extern void SignalOpenGLDrawRangeElementsInstancedEvent( GLenum Mode, GLsizei Count, GLenum Type, const GLvoid* Indices, GLsizei PrimCount );
		extern void SignalOpenGLClearEvent( int8 ClearType, int8 NumColors, const float* Colors, float Depth, uint32 Stencil );
		extern void SignalOpenGLFramebufferBlitEvent( GLbitfield Mask );
		extern void SignalOpenGLEndFrameEvent( void );
		extern void TriggerOpenGLFrameDump( void );
		extern void TriggerOpenGLFrameDumpEveryXCalls( int32 X );
	}

	#define REPORT_GL_DRAW_ARRAYS_EVENT_FOR_FRAME_DUMP( a, b, c ) { SignalOpenGLDrawArraysEvent( a, b, c ); }
	#define REPORT_GL_DRAW_ARRAYS_INSTANCED_EVENT_FOR_FRAME_DUMP( a, b, c, d ) { SignalOpenGLDrawArraysInstancedEvent( a, b, c, d ); }
	#define REPORT_GL_DRAW_RANGE_ELEMENTS_EVENT_FOR_FRAME_DUMP( a, b, c, d, e, f ) { SignalOpenGLDrawRangeElementsEvent( a, b, c, d, e, f ); }
	#define REPORT_GL_DRAW_ELEMENTS_INSTANCED_EVENT_FOR_FRAME_DUMP( a, b, c, d, e ) { SignalOpenGLDrawRangeElementsInstancedEvent( a, b, c, d, e ); }
	#define REPORT_GL_CLEAR_EVENT_FOR_FRAME_DUMP( a, b, c, d, e ) { SignalOpenGLClearEvent( a, b, c, d, e ); }
	#define REPORT_GL_FRAMEBUFFER_BLIT_EVENT( a ) { SignalOpenGLFramebufferBlitEvent( a ); }
	#define REPORT_GL_END_BUFFER_EVENT_FOR_FRAME_DUMP() { SignalOpenGLEndFrameEvent(); }
	#define INITIATE_GL_FRAME_DUMP() { TriggerOpenGLFrameDump(); }
	#define INITIATE_GL_FRAME_DUMP_EVERY_X_CALLS( a ) { TriggerOpenGLFrameDumpEveryXCalls( a ); }
#else
	#define REPORT_GL_DRAW_ARRAYS_EVENT_FOR_FRAME_DUMP( a, b, c )
	#define REPORT_GL_DRAW_ARRAYS_INSTANCED_EVENT_FOR_FRAME_DUMP( a, b, c, d )
	#define REPORT_GL_DRAW_RANGE_ELEMENTS_EVENT_FOR_FRAME_DUMP( a, b, c, d, e, f )
	#define REPORT_GL_DRAW_ELEMENTS_INSTANCED_EVENT_FOR_FRAME_DUMP( a, b, c, d, e )
	#define REPORT_GL_CLEAR_EVENT_FOR_FRAME_DUMP( a, b, c, d, e )
	#define REPORT_GL_FRAMEBUFFER_BLIT_EVENT( a )
	#define REPORT_GL_END_BUFFER_EVENT_FOR_FRAME_DUMP()
	#define INITIATE_GL_FRAME_DUMP()
	#define INITIATE_GL_FRAME_DUMP_EVERY_X_CALLS( a )
#endif

