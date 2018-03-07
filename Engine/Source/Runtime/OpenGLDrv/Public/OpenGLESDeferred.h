// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGL3.h: Public OpenGL 3.2 definitions for non-common functionality
=============================================================================*/

#pragma once


#define OPENGL_ESDEFERRED		1

#include "OpenGL.h"

// @todo-mobile: Use this to search for hacks required for fast ES2 bring up.
#define OPENGL_ES2_BRING_UP 0

#ifdef GL_AMD_debug_output
	#undef GL_AMD_debug_output
#endif

/** Unreal tokens that maps to different OpenGL tokens by platform. */
#undef UGL_ANY_SAMPLES_PASSED
#define UGL_ANY_SAMPLES_PASSED	GL_ANY_SAMPLES_PASSED_EXT
#undef UGL_CLAMP_TO_BORDER
#define UGL_CLAMP_TO_BORDER		FOpenGL::ClampToBorederMode()
#undef UGL_TIME_ELAPSED
#define UGL_TIME_ELAPSED		GL_TIME_ELAPSED_EXT

#define USE_OPENGL_NAME_CACHE 1
#define OPENGL_NAME_CACHE_SIZE 1024


struct FOpenGLESDeferred : public FOpenGLBase
{
	static FORCEINLINE bool SupportsVertexArrayObjects()				{ return bSupportsVertexArrayObjects || !bES2Fallback; }
	static FORCEINLINE bool SupportsMapBuffer()							{ return bSupportsMapBuffer || !bES2Fallback; }
	static FORCEINLINE bool SupportsDepthTexture()						{ return bSupportsDepthTexture || !bES2Fallback; }
	static FORCEINLINE bool SupportsDrawBuffers()						{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsPixelBuffers()						{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsUniformBuffers()					{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsStructuredBuffers()					{ return false; }
	static FORCEINLINE bool SupportsOcclusionQueries()					{ return bSupportsOcclusionQueries; }
	static FORCEINLINE bool SupportsExactOcclusionQueries()				{ return false; }
	static FORCEINLINE bool SupportsTimestampQueries()					{ return !bES2Fallback && bSupportsNvTimerQuery; }
	static bool SupportsDisjointTimeQueries();
	static FORCEINLINE bool SupportsBlitFramebuffer()					{ return bSupportsNVFrameBufferBlit || !bES2Fallback; }
	static FORCEINLINE bool SupportsDepthStencilRead()					{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsFloatReadSurface()					{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsMultipleRenderTargets()				{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsWideMRT()							{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsMultisampledTextures()				{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsFences()							{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsPolygonMode()						{ return false; }
	static FORCEINLINE bool SupportsSamplerObjects()					{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsTexture3D()							{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsMobileMultiView()					{ return false; }
	static FORCEINLINE bool SupportsImageExternal()						{ return false; }
	static FORCEINLINE bool SupportsTextureLODBias()					{ return false; }
	static FORCEINLINE bool SupportsTextureCompare()					{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsTextureBaseLevel()					{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsTextureMaxLevel()					{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsInstancing()						{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsVertexAttribInteger()				{ return true; }
	static FORCEINLINE bool SupportsVertexAttribShort()					{ return true; }
	static FORCEINLINE bool SupportsVertexAttribByte()					{ return true; }
	static FORCEINLINE bool SupportsVertexAttribDouble()				{ return true; }
	static FORCEINLINE bool SupportsDrawIndexOffset()					{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsResourceView()						{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsCopyBuffer()						{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsDiscardFrameBuffer()				{ return bSupportsDiscardFrameBuffer; }
	static FORCEINLINE bool SupportsIndexedExtensions()					{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsVertexHalfFloat()					{ return bSupportsVertexHalfFloat || !bES2Fallback; }
	static FORCEINLINE bool SupportsTextureFloat()						{ return bSupportsTextureFloat || !bES2Fallback; }
	static FORCEINLINE bool SupportsTextureHalfFloat()					{ return bSupportsTextureHalfFloat || !bES2Fallback; }
	static FORCEINLINE bool SupportsColorBufferFloat()					{ return bSupportsColorBufferFloat || !bES2Fallback; }
	static FORCEINLINE bool SupportsColorBufferHalfFloat()				{ return bSupportsColorBufferHalfFloat || !bES2Fallback; }
	static FORCEINLINE bool	SupportsRG16UI()							{ return bSupportsNvImageFormats && !bES2Fallback; }
	static FORCEINLINE bool SupportsR11G11B10F()						{ return bSupportsNvImageFormats && !bES2Fallback; }
	static FORCEINLINE bool SupportsShaderFramebufferFetch()			{ return bSupportsShaderFramebufferFetch; }
	static FORCEINLINE bool SupportsShaderDepthStencilFetch()			{ return bSupportsShaderDepthStencilFetch; }
	static FORCEINLINE bool SupportsMultisampledRenderToTexture()		{ return bSupportsMultisampledRenderToTexture; }
	static FORCEINLINE bool SupportsVertexArrayBGRA()					{ return false; }
	static FORCEINLINE bool SupportsBGRA8888()							{ return bSupportsBGRA8888; }
	static FORCEINLINE bool SupportsBGRA8888RenderTarget()				{ return bSupportsBGRA8888RenderTarget; }
	static FORCEINLINE bool SupportsSRGB()								{ return bSupportsSGRB || !bES2Fallback; }
	static FORCEINLINE bool SupportsRGBA8()								{ return bSupportsRGBA8; }
	static FORCEINLINE bool SupportsDXT()								{ return bSupportsDXT; }
	static FORCEINLINE bool SupportsPVRTC()								{ return bSupportsPVRTC; }
	static FORCEINLINE bool SupportsATITC()								{ return bSupportsATITC; }
	static FORCEINLINE bool SupportsETC1()								{ return bSupportsETC1; }
	static FORCEINLINE bool SupportsETC2()								{ return bSupportsETC2; }
	static FORCEINLINE bool SupportsCombinedDepthStencilAttachment()	{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsPackedDepthStencil()				{ return bSupportsPackedDepthStencil || !bES2Fallback; }
	static FORCEINLINE bool SupportsTextureCubeLodEXT()					{ return bES2Fallback ? bSupportsTextureCubeLodEXT : false; }
	static FORCEINLINE bool SupportsShaderTextureLod()					{ return bES2Fallback ? bSupportsShaderTextureLod : true; }
	static FORCEINLINE bool SupportsShaderTextureCubeLod()				{ return bES2Fallback ? bSupportsShaderTextureCubeLod : true; }
	static FORCEINLINE bool SupportsCopyTextureLevels()					{ return bSupportsCopyTextureLevels; }
	static FORCEINLINE GLenum GetDepthFormat()							{ return GL_DEPTH_COMPONENT16; }
	static FORCEINLINE GLenum GetShadowDepthFormat()					{ return GL_DEPTH_COMPONENT16; }

	static FORCEINLINE bool RequiresUEShaderFramebufferFetchDef() { return bRequiresUEShaderFramebufferFetchDef; }
	static FORCEINLINE bool RequiresDontEmitPrecisionForTextureSamplers() { return bRequiresDontEmitPrecisionForTextureSamplers; }
	static FORCEINLINE bool RequiresTextureCubeLodEXTToTextureCubeLodDefine() { return bRequiresTextureCubeLodEXTToTextureCubeLodDefine; }
	static FORCEINLINE bool SupportsStandardDerivativesExtension()		{ return true; }
	static FORCEINLINE bool RequiresGLFragCoordVaryingLimitHack()		{ return bRequiresGLFragCoordVaryingLimitHack; }
	static FORCEINLINE GLenum GetVertexHalfFloatFormat()				{ return bES2Fallback ? GL_HALF_FLOAT_OES : GL_HALF_FLOAT; }
	static FORCEINLINE bool RequiresTexture2DPrecisionHack()			{ return bRequiresTexture2DPrecisionHack; }
	static FORCEINLINE bool RequiresARMShaderFramebufferFetchDepthStencilUndef() { return bRequiresARMShaderFramebufferFetchDepthStencilUndef; }
	static FORCEINLINE bool IsCheckingShaderCompilerHacks()				{ return bIsCheckingShaderCompilerHacks; }
	static FORCEINLINE bool SupportsRGB10A2()							{ return bSupportsRGB10A2 || !bES2Fallback; }

	// On iOS both glMapBufferOES() and glBufferSubData() for immediate vertex and index data
	// is the slow path (they both hit GPU sync and data cache flush in driver according to profiling in driver symbols).
	// Turning this to false reverts back to not using vertex and index buffers 
	// for glDrawArrays() and glDrawElements() on dynamic data.
	static FORCEINLINE bool SupportsFastBufferData()					{ return !bES2Fallback; }

	// ES 2 will not work with non-power of two textures with non-clamp mode
	static FORCEINLINE bool HasSamplerRestrictions()					{ return bES2Fallback; }

	static FORCEINLINE bool UseES30ShadingLanguage()					{ return GetMajorVersion() == 3; }

	static FORCEINLINE bool IsDebugContent()							{ return bDebugContext; }
	
	static FORCEINLINE bool SupportsSeamlessCubeMap()					{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsVolumeTextureRendering()			{ return bSupportsVolumeTextureRendering; }
	static FORCEINLINE bool SupportsGenerateMipmap()					{ return true; }
	static FORCEINLINE bool SupportsTextureSwizzle()					{ return !bES2Fallback; }

	//from FOpenGL4
	static FORCEINLINE bool SupportsSeparateAlphaBlend()				{ return bSupportsSeparateAlphaBlend; }
	static FORCEINLINE bool SupportsTessellation()						{ return bSupportsTessellation; }
	static FORCEINLINE bool SupportsComputeShaders()					{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsDrawIndirect()						{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsVertexAttribBinding()				{ return !bES2Fallback; }
	static FORCEINLINE bool SupportsTextureView()						{ return bSupportsTextureView; }

	// Whether GLES 3.1 + extension pack is supported
	static bool SupportsAdvancedFeatures();

	static FORCEINLINE GLenum ClampToBorederMode()						{ return bES2Fallback ? GL_CLAMP_TO_EDGE : GL_CLAMP_TO_BORDER_EXT;}

	// Optional
	static FORCEINLINE void QueryTimestampCounter(GLuint QueryID)
	{
		//glQueryCounter(QueryID, GL_TIMESTAMP);
	}

	static FORCEINLINE void BeginQuery(GLenum QueryType, GLuint QueryId)
	{
		glBeginQuery( QueryType, QueryId );
	}

	static FORCEINLINE void EndQuery(GLenum QueryType)
	{
		glEndQuery( QueryType );
	}

	static FORCEINLINE void GetQueryObject(GLuint QueryId, EQueryMode QueryMode, GLuint64* OutResult)
	{
		GLenum QueryName = (QueryMode == QM_Result) ? GL_QUERY_RESULT : GL_QUERY_RESULT_AVAILABLE;
		GLuint Result = 0;
		glGetQueryObjectuiv(QueryId, QueryName, &Result);
		*OutResult = Result;
	}
	/*
	static FORCEINLINE void ReadBuffer(GLenum Mode)
	{
		glReadBuffer( Mode );
	}

	static FORCEINLINE void DrawBuffer(GLenum Mode)
	{
		glDrawBuffers( 1, &Mode );
	}

	static FORCEINLINE void DeleteSync(UGLsync Sync)
	{
		glDeleteSync( Sync );
	}

	static FORCEINLINE UGLsync FenceSync(GLenum Condition, GLbitfield Flags)
	{
		return glFenceSync( Condition, Flags );
	}

	static FORCEINLINE bool IsSync(UGLsync Sync)
	{
		return (glIsSync( Sync ) == GL_TRUE) ? true : false;
	}

	static FORCEINLINE EFenceResult ClientWaitSync(UGLsync Sync, GLbitfield Flags, GLuint64 Timeout)
	{
		GLenum Result = glClientWaitSync( Sync, Flags, Timeout );
		switch (Result)
		{
			case GL_ALREADY_SIGNALED:		return FR_AlreadySignaled;
			case GL_TIMEOUT_EXPIRED:		return FR_TimeoutExpired;
			case GL_CONDITION_SATISFIED:	return FR_ConditionSatisfied;
		}
		return FR_WaitFailed;
	}

	*/

	static FORCEINLINE void GenSamplers(GLsizei Count, GLuint* Samplers)
	{
		glGenSamplers(Count, Samplers);
	}

	static FORCEINLINE void DeleteSamplers(GLsizei Count, GLuint* Samplers)
	{
		glDeleteSamplers(Count, Samplers);
	}

	static FORCEINLINE void SetSamplerParameter(GLuint Sampler, GLenum Parameter, GLint Value)
	{
		glSamplerParameteri(Sampler, Parameter, Value);
	}

	static FORCEINLINE void BindSampler(GLuint Unit, GLuint Sampler)
	{
		glBindSampler(Unit, Sampler);
	}

	static FORCEINLINE void PolygonMode(GLenum Face, GLenum Mode)
	{
		// Not in ES
		//glPolygonMode(Face, Mode);
	}

	static FORCEINLINE void VertexAttribDivisor(GLuint Index, GLuint Divisor)
	{
		if (!bES2Fallback)
		{
			glVertexAttribDivisor(Index, Divisor);
		}
	}

	// Required
	static FORCEINLINE void* MapBufferRange(GLenum Type, uint32 InOffset, uint32 InSize, EResourceLockMode LockMode)
	{
		void *Result = nullptr;
		if (!bES2Fallback)
		{
			GLenum Access;
			switch ( LockMode )
			{
				case RLM_ReadOnly:
					Access = GL_MAP_READ_BIT;
					break;
				case RLM_WriteOnly:
					Access = (GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_WRITE_BIT );
					// Temp workaround for synchrnoization when a UBO is discarded while being referenced
					Access |= GL_MAP_UNSYNCHRONIZED_BIT;
					break;
				case RLM_WriteOnlyUnsynchronized:
					Access = (GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
					break;
				case RLM_WriteOnlyPersistent:
					Access = (GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
					break;
				case RLM_ReadWrite:
				default:
					Access = (GL_MAP_READ_BIT | GL_MAP_WRITE_BIT);
			}
			Result = glMapBufferRange(Type, InOffset, InSize, Access);
		}
		else
		{
#if OPENGL_ES2_BRING_UP	
			// Non-written areas retain prior values.
			// Lack of unsynchronized in glMapBufferOES() is a perf bug which needs to be fixed later.
			checkf(LockMode == RLM_WriteOnly || LockMode == RLM_WriteOnlyUnsynchronized, TEXT("OpenGL ES 2.0 only supports write-only buffer locks"));
#else
			checkf(LockMode == RLM_WriteOnly, TEXT("OpenGL ES 2.0 only supports write-only buffer locks"));
#endif
			check(Type == GL_ARRAY_BUFFER || Type == GL_ELEMENT_ARRAY_BUFFER);

			uint8* Data = (uint8*) glMapBufferOES(Type, GL_WRITE_ONLY_OES);
			Result = Data ? Data + InOffset : NULL;
		}
		return Result;
	}

	static FORCEINLINE void UnmapBuffer(GLenum Type)
	{
		if (!bES2Fallback)
		{
			glUnmapBuffer(Type);
		}
		else
		{
			check(Type == GL_ARRAY_BUFFER || Type == GL_ELEMENT_ARRAY_BUFFER);
			glUnmapBufferOES(Type);
		}
	}

	static FORCEINLINE void UnmapBufferRange(GLenum Type, uint32 InOffset, uint32 InSize)
	{
		UnmapBuffer(Type);
	}

	static FORCEINLINE void GenQueries(GLsizei NumQueries, GLuint* QueryIDs)
	{
		glGenQueries(NumQueries, QueryIDs);
	}

	static FORCEINLINE void DeleteQueries(GLsizei NumQueries, const GLuint* QueryIDs )
	{
		glDeleteQueries( NumQueries, QueryIDs );
	}

	static FORCEINLINE void GetQueryObject(GLuint QueryId, EQueryMode QueryMode, GLuint* OutResult)
	{
		GLenum QueryName = (QueryMode == QM_Result) ? GL_QUERY_RESULT : GL_QUERY_RESULT_AVAILABLE;
		glGetQueryObjectuiv(QueryId, QueryName, OutResult);
	}

	static FORCEINLINE void BindBufferBase(GLenum Target, GLuint Index, GLuint Buffer)
	{
		check(!bES2Fallback);
		glBindBufferBase(Target, Index, Buffer);
	}

	static FORCEINLINE void BindBufferRange(GLenum Target, GLuint Index, GLuint Buffer, GLintptr Offset, GLsizeiptr Size)
	{
		check(!bES2Fallback);
		glBindBufferRange(Target, Index, Buffer, Offset, Size);
	}

	static FORCEINLINE GLuint GetUniformBlockIndex(GLuint Program, const GLchar* UniformBlockName)
	{
		return glGetUniformBlockIndex(Program, UniformBlockName);
	}

	static FORCEINLINE void UniformBlockBinding(GLuint Program, GLuint UniformBlockIndex, GLuint UniformBlockBinding)
	{
		glUniformBlockBinding(Program, UniformBlockIndex, UniformBlockBinding);
	}

	static FORCEINLINE void BindFragDataLocation(GLuint Program, GLuint Color, const GLchar* Name)
	{
		// Not in ES
		//glBindFragDataLocation(Program, Color, Name);
	}

	static FORCEINLINE void TexParameter(GLenum Target, GLenum Parameter, GLint Value)
	{
		glTexParameteri(Target, Parameter, Value);
	}

	static FORCEINLINE void FramebufferTexture(GLenum Target, GLenum Attachment, GLuint Texture, GLint Level)
	{
		glFramebufferTextureEXT(Target, Attachment, Texture, Level);
	}

	static FORCEINLINE void FramebufferTexture3D(GLenum Target, GLenum Attachment, GLenum TexTarget, GLuint Texture, GLint Level, GLint ZOffset)
	{
		// ES 3.1 uses FramebufferLAyer
		//glFramebufferTexture3D(Target, Attachment, TexTarget, Texture, Level, ZOffset);
		glFramebufferTextureLayer(Target, Attachment, Texture, Level, ZOffset);
	}

	static FORCEINLINE void FramebufferTextureLayer(GLenum Target, GLenum Attachment, GLuint Texture, GLint Level, GLint Layer)
	{
		glFramebufferTextureLayer(Target, Attachment, Texture, Level, Layer);
	}

	static FORCEINLINE void Uniform4uiv(GLint Location, GLsizei Count, const GLuint* Value)
	{
		glUniform4uiv(Location, Count, Value);
	}
	
	static FORCEINLINE void ProgramUniform4uiv(GLuint Program, GLint Location, GLsizei Count, const GLuint *Value)
	{
		glUniform4uiv(Location, Count, Value);
	}

	static FORCEINLINE void BlitFramebuffer(GLint SrcX0, GLint SrcY0, GLint SrcX1, GLint SrcY1, GLint DstX0, GLint DstY0, GLint DstX1, GLint DstY1, GLbitfield Mask, GLenum Filter)
	{
		glBlitFramebuffer(SrcX0, SrcY0, SrcX1, SrcY1, DstX0, DstY0, DstX1, DstY1, Mask, Filter);
	}

	static FORCEINLINE void DrawBuffers(GLsizei NumBuffers, const GLenum* Buffers)
	{
		glDrawBuffers(NumBuffers, Buffers);
	}

	static FORCEINLINE void DepthRange(GLdouble Near, GLdouble Far)
	{
		glDepthRangef(Near, Far);
	}

	static FORCEINLINE void EnableIndexed(GLenum Parameter, GLuint Index)
	{
		glEnableiEXT(Parameter, Index);
	}

	static FORCEINLINE void DisableIndexed(GLenum Parameter, GLuint Index)
	{
		glDisableiEXT(Parameter, Index);
	}

	static FORCEINLINE void ColorMaskIndexed(GLuint Index, GLboolean Red, GLboolean Green, GLboolean Blue, GLboolean Alpha)
	{
		glColorMaskiEXT(Index, Red, Green, Blue, Alpha);
	}

	static FORCEINLINE void VertexAttribPointer(GLuint Index, GLint Size, GLenum Type, GLboolean Normalized, GLsizei Stride, const GLvoid* Pointer)
	{
		glVertexAttribPointer(Index, Size, Type, Normalized, Stride, Pointer);
	}

	static FORCEINLINE void VertexAttribIPointer(GLuint Index, GLint Size, GLenum Type, GLsizei Stride, const GLvoid* Pointer)
	{
		if (bES2Fallback)
		{
			glVertexAttribPointer(Index, Size, Type, GL_FALSE, Stride, Pointer);
		}
		else
		{
			glVertexAttribIPointer(Index, Size, Type, Stride, Pointer);
		}

		
	}
	
	/*
	 *
	 * ES 3 has deprecated most conversions, need to implement conversion routines
	 *
	 */

	template< typename Type, int Max> static void TVertexAttrib4Nv( GLuint AttributeIndex, const Type* Values)
	{
		GLfloat Params[4];

		for ( int32 Idx = 0; Idx < 4; Idx++)
		{
			Params[Idx] = FMath::Max( float(Values[Idx]) / float(Max), -1.0f);
		}
		glVertexAttrib4fv( AttributeIndex, Params);
	}

	template< typename Type> static void TVertexAttrib4v( GLuint AttributeIndex, const Type* Values)
	{
		GLfloat Params[4];

		for ( int32 Idx = 0; Idx < 4; Idx++)
		{
			Params[Idx] = float(Values[Idx]);
		}
		glVertexAttrib4fv( AttributeIndex, Params);
	}

	template< typename Type> static void TVertexAttrib4Iv( GLuint AttributeIndex, const Type* Values)
	{
		GLint Params[4];

		for ( int32 Idx = 0; Idx < 4; Idx++)
		{
			Params[Idx] = GLint(Values[Idx]);
		}
		glVertexAttribI4iv( AttributeIndex, Params);
	}

	template< typename Type> static void TVertexAttrib4UIv( GLuint AttributeIndex, const Type* Values)
	{
		GLuint Params[4];

		for ( int32 Idx = 0; Idx < 4; Idx++)
		{
			Params[Idx] = GLuint(Values[Idx]);
		}
		glVertexAttribI4uiv( AttributeIndex, Params);
	}

	static FORCEINLINE void VertexAttrib4Nsv(GLuint AttributeIndex, const GLshort* Values)
	{
		//glVertexAttrib4Nsv(AttributeIndex, Values);
		TVertexAttrib4Nv<GLshort, 32767>( AttributeIndex, Values);
	}

	static FORCEINLINE void VertexAttrib4sv(GLuint AttributeIndex, const GLshort* Values)
	{
		//glVertexAttrib4sv(AttributeIndex, Values);
		TVertexAttrib4v<GLshort>( AttributeIndex, Values);
	}

	static FORCEINLINE void VertexAttribI4sv(GLuint AttributeIndex, const GLshort* Values)
	{
		//glVertexAttribI4sv(AttributeIndex, Values);
		TVertexAttrib4Iv<GLshort>( AttributeIndex, Values);
	}

	static FORCEINLINE void VertexAttribI4usv(GLuint AttributeIndex, const GLushort* Values)
	{
		//glVertexAttribI4usv(AttributeIndex, Values);
		TVertexAttrib4UIv<GLushort>( AttributeIndex, Values);
	}

	static FORCEINLINE void VertexAttrib4Nubv(GLuint AttributeIndex, const GLubyte* Values)
	{
		//glVertexAttrib4Nubv(AttributeIndex, Values);
		TVertexAttrib4Nv<GLubyte, 255>( AttributeIndex, Values);
	}

	static FORCEINLINE void VertexAttrib4ubv(GLuint AttributeIndex, const GLubyte* Values)
	{
		//glVertexAttrib4ubv(AttributeIndex, Values);
		TVertexAttrib4v<GLubyte>( AttributeIndex, Values);
	}

	static FORCEINLINE void VertexAttribI4ubv(GLuint AttributeIndex, const GLubyte* Values)
	{
		//glVertexAttribI4ubv(AttributeIndex, Values);
		TVertexAttrib4UIv<GLubyte>( AttributeIndex, Values);
	}

	static FORCEINLINE void VertexAttrib4Nbv(GLuint AttributeIndex, const GLbyte* Values)
	{
		//glVertexAttrib4Nbv(AttributeIndex, Values);
		TVertexAttrib4Nv<GLbyte, 127>( AttributeIndex, Values);
	}

	static FORCEINLINE void VertexAttrib4bv(GLuint AttributeIndex, const GLbyte* Values)
	{
		//glVertexAttrib4bv(AttributeIndex, Values);
		TVertexAttrib4v<GLbyte>( AttributeIndex, Values);
	}

	static FORCEINLINE void VertexAttribI4bv(GLuint AttributeIndex, const GLbyte* Values)
	{
		//glVertexAttribI4bv(AttributeIndex, Values);
		TVertexAttrib4Iv<GLbyte>( AttributeIndex, Values);
	}

	static FORCEINLINE void VertexAttrib4dv(GLuint AttributeIndex, const GLdouble* Values)
	{
		//glVertexAttrib4dv(AttributeIndex, Values);
		TVertexAttrib4v<GLdouble>( AttributeIndex, Values);
	}

	static FORCEINLINE void VertexAttribI4iv(GLuint AttributeIndex, const GLint* Values)
	{
		glVertexAttribI4iv(AttributeIndex, Values);
	}

	static FORCEINLINE void VertexAttribI4uiv(GLuint AttributeIndex, const GLuint* Values)
	{
		glVertexAttribI4uiv(AttributeIndex, Values);
	}

	static FORCEINLINE void DrawArraysInstanced(GLenum Mode, GLint First, GLsizei Count, GLsizei InstanceCount)
	{
		glDrawArraysInstanced(Mode, First, Count, InstanceCount);
	}

	static FORCEINLINE void DrawElementsInstanced(GLenum Mode, GLsizei Count, GLenum Type, const GLvoid* Indices, GLsizei InstanceCount)
	{
		glDrawElementsInstanced(Mode, Count, Type, Indices, InstanceCount);
	}

	static FORCEINLINE void DrawRangeElements(GLenum Mode, GLuint Start, GLuint End, GLsizei Count, GLenum Type, const GLvoid* Indices)
	{
		glDrawRangeElements(Mode, Start, End, Count, Type, Indices);
	}

	static FORCEINLINE void ClearBufferfv(GLenum Buffer, GLint DrawBufferIndex, const GLfloat* Value)
	{
		glClearBufferfv(Buffer, DrawBufferIndex, Value);
	}

	static FORCEINLINE void ClearBufferfi(GLenum Buffer, GLint DrawBufferIndex, GLfloat Depth, GLint Stencil)
	{
		glClearBufferfi(Buffer, DrawBufferIndex, Depth, Stencil);
	}

	static FORCEINLINE void ClearBufferiv(GLenum Buffer, GLint DrawBufferIndex, const GLint* Value)
	{
		glClearBufferiv(Buffer, DrawBufferIndex, Value);
	}

	static FORCEINLINE void ClearDepth(GLdouble Depth)
	{
		glClearDepthf(Depth);
	}

	static FORCEINLINE void TexImage3D(GLenum Target, GLint Level, GLint InternalFormat, GLsizei Width, GLsizei Height, GLsizei Depth, GLint Border, GLenum Format, GLenum Type, const GLvoid* PixelData)
	{
		glTexImage3D(Target, Level, InternalFormat, Width, Height, Depth, Border, Format, Type, PixelData);
	}

	static FORCEINLINE void CompressedTexImage3D(GLenum Target, GLint Level, GLenum InternalFormat, GLsizei Width, GLsizei Height, GLsizei Depth, GLint Border, GLsizei ImageSize, const GLvoid* PixelData)
	{
		glCompressedTexImage3D(Target, Level, InternalFormat, Width, Height, Depth, Border, ImageSize, PixelData);
	}

	static FORCEINLINE void CompressedTexSubImage2D(GLenum Target, GLint Level, GLsizei Width, GLsizei Height, GLenum Format, GLsizei ImageSize, const GLvoid* PixelData)
	{
		glCompressedTexSubImage2D(Target, Level, 0, 0, Width, Height, Format, ImageSize, PixelData);
	}

	static FORCEINLINE void TexImage2DMultisample(GLenum Target, GLsizei Samples, GLint InternalFormat, GLsizei Width, GLsizei Height, GLboolean FixedSampleLocations) UGL_REQUIRED_VOID

	static FORCEINLINE void TexBuffer(GLenum Target, GLenum InternalFormat, GLuint Buffer)
	{
		glTexBufferEXT(Target, InternalFormat, Buffer);
	}

	static FORCEINLINE void TexSubImage3D(GLenum Target, GLint Level, GLint XOffset, GLint YOffset, GLint ZOffset, GLsizei Width, GLsizei Height, GLsizei Depth, GLenum Format, GLenum Type, const GLvoid* PixelData)
	{
		glTexSubImage3D(Target, Level, XOffset, YOffset, ZOffset, Width, Height, Depth, Format, Type, PixelData);
	}

	static FORCEINLINE void	CopyTexSubImage3D(GLenum Target, GLint Level, GLint XOffset, GLint YOffset, GLint ZOffset, GLint X, GLint Y, GLsizei Width, GLsizei Height)
	{
		glCopyTexSubImage3D(Target, Level, XOffset, YOffset, ZOffset, X, Y, Width, Height);
	}

	//ES lacks GetTexImage of any sort
	static FORCEINLINE void GetCompressedTexImage(GLenum Target, GLint Level, GLvoid* OutImageData) UGL_REQUIRED_VOID
	static FORCEINLINE void GetTexImage(GLenum Target, GLint Level, GLenum Format, GLenum Type, GLvoid* OutPixelData) UGL_REQUIRED_VOID

	static FORCEINLINE void CopyBufferSubData(GLenum ReadTarget, GLenum WriteTarget, GLintptr ReadOffset, GLintptr WriteOffset, GLsizeiptr Size)
	{
		glCopyBufferSubData(ReadTarget, WriteTarget, ReadOffset, WriteOffset, Size);
	}

	static FORCEINLINE void GenBuffers( GLsizei n, GLuint *buffers)
	{
#if USE_OPENGL_NAME_CACHE
		if ( n < OPENGL_NAME_CACHE_SIZE - NextBufferName)
		{
			FMemory::Memcpy( buffers, &BufferNamesCache[NextBufferName], sizeof(GLuint)*n);
			NextBufferName += n;
		}
		else
		{
			if ( n >= OPENGL_NAME_CACHE_SIZE)
			{
				glGenBuffers( n, buffers);
			}
			else
			{
				GLsizei Leftover = OPENGL_NAME_CACHE_SIZE - NextBufferName;

				FMemory::Memcpy( buffers, &BufferNamesCache[NextBufferName], sizeof(GLuint)*Leftover);

				glGenBuffers( OPENGL_NAME_CACHE_SIZE, BufferNamesCache);

				n -= Leftover;
				buffers += Leftover;

				FMemory::Memcpy( buffers, BufferNamesCache, sizeof(GLuint)*n);
				NextBufferName = n;
			}
		}
#else
		glGenBuffers( n, buffers);
#endif
	}

	static FORCEINLINE void GenTextures( GLsizei n, GLuint *textures)
	{
#if USE_OPENGL_NAME_CACHE
		if ( n < OPENGL_NAME_CACHE_SIZE - NextTextureName)
		{
			FMemory::Memcpy( textures, &TextureNamesCache[NextTextureName], sizeof(GLuint)*n);
			NextTextureName += n;
		}
		else
		{
			if ( n >= OPENGL_NAME_CACHE_SIZE)
			{
				glGenTextures( n, textures);
			}
			else
			{
				GLsizei Leftover = OPENGL_NAME_CACHE_SIZE - NextTextureName;

				FMemory::Memcpy( textures, &TextureNamesCache[NextTextureName], sizeof(GLuint)*Leftover);

				glGenTextures( OPENGL_NAME_CACHE_SIZE, TextureNamesCache);

				n -= Leftover;
				textures += Leftover;

				FMemory::Memcpy( textures, TextureNamesCache, sizeof(GLuint)*n);
				NextTextureName = n;
			}
		}
#else
		glGenTextures( n, textures);
#endif
	}

	static FORCEINLINE void CompressedTexSubImage3D(GLenum Target, GLint Level, GLint XOffset, GLint YOffset, GLint ZOffset, GLsizei Width, GLsizei Height, GLsizei Depth, GLenum Format, GLsizei ImageSize, const GLvoid* PixelData)
	{
		glCompressedTexSubImage3D( Target, Level, XOffset, YOffset, ZOffset, Width, Height, Depth, Format, ImageSize, PixelData);
	}

	static FORCEINLINE void GenerateMipmap( GLenum Target )
	{
		glGenerateMipmap( Target);
	}

	static FORCEINLINE const ANSICHAR* GetStringIndexed(GLenum Name, GLuint Index)
	{
		return (const ANSICHAR*)glGetStringi(Name, Index);
	}

	static FORCEINLINE GLuint GetMajorVersion()
	{
		return MajorVersion;
	}

	static FORCEINLINE GLuint GetMinorVersion()
	{
		return MinorVersion;
	}

	// From FOpenGL4
	static FORCEINLINE void BlendFuncSeparatei(GLuint Buf, GLenum SrcRGB, GLenum DstRGB, GLenum SrcAlpha, GLenum DstAlpha)
	{
		glBlendFuncSeparateiEXT(Buf, SrcRGB, DstRGB, SrcAlpha, DstAlpha);
	}
	static FORCEINLINE void BlendEquationSeparatei(GLuint Buf, GLenum ModeRGB, GLenum ModeAlpha)
	{
		glBlendEquationSeparateiEXT(Buf, ModeRGB, ModeAlpha);
	}
	static FORCEINLINE void BlendFunci(GLuint Buf, GLenum Src, GLenum Dst)
	{
		glBlendFunciEXT(Buf, Src, Dst);
	}
	static FORCEINLINE void BlendEquationi(GLuint Buf, GLenum Mode)
	{
		glBlendEquationiEXT(Buf, Mode);
	}
	static FORCEINLINE void PatchParameteri(GLenum Pname, GLint Value)
	{
		glPatchParameteriEXT(Pname, Value);
	}
	static FORCEINLINE void BindImageTexture(GLuint Unit, GLuint Texture, GLint Level, GLboolean Layered, GLint Layer, GLenum Access, GLenum Format)
	{
		glBindImageTexture(Unit, Texture, Level, Layered, Layer, Access, Format);
	}
	static FORCEINLINE void DispatchCompute(GLuint NumGroupsX, GLuint NumGroupsY, GLuint NumGroupsZ)
	{
		glDispatchCompute(NumGroupsX, NumGroupsY, NumGroupsZ);
	}
	static FORCEINLINE void DispatchComputeIndirect(GLintptr Offset)
	{
		glDispatchComputeIndirect(Offset);
	}
	static FORCEINLINE void MemoryBarrier(GLbitfield Barriers)
	{
		glMemoryBarrier(Barriers);
	}
	static FORCEINLINE void DrawArraysIndirect (GLenum Mode, const void *Offset)
	{
		glDrawArraysIndirect( Mode, Offset);
	}
	static FORCEINLINE void DrawElementsIndirect (GLenum Mode, GLenum Type, const void *Offset)
	{
		glDrawElementsIndirect( Mode, Type, Offset);
	}
	static FORCEINLINE void BindVertexBuffer(GLuint BindingIndex, GLuint Buffer, GLintptr Offset, GLsizei Stride)
	{
		glBindVertexBuffer(BindingIndex, Buffer, Offset, Stride);
	}
	static FORCEINLINE void VertexAttribFormat(GLuint AttribIndex, GLint Size, GLenum Type, GLboolean Normalized, GLuint RelativeOffset)
	{
		glVertexAttribFormat(AttribIndex, Size, Type, Normalized, RelativeOffset);
	}
	static FORCEINLINE void VertexAttribIFormat(GLuint AttribIndex, GLint Size, GLenum Type, GLuint RelativeOffset)
	{
		glVertexAttribIFormat(AttribIndex, Size, Type, RelativeOffset);
	}
	static FORCEINLINE void VertexAttribBinding(GLuint AttribIndex, GLuint BindingIndex)
	{
		glVertexAttribBinding(AttribIndex, BindingIndex);
	}
	static FORCEINLINE void VertexBindingDivisor(GLuint BindingIndex, GLuint Divisor)
	{
		glVertexBindingDivisor(BindingIndex, Divisor);
	}
	static FORCEINLINE void TextureView(GLuint ViewName, GLenum ViewTarget, GLuint SrcName, GLenum InternalFormat, GLuint MinLevel, GLuint NumLevels, GLuint MinLayer, GLuint NumLayers)
	{
		glTextureViewEXT(ViewName, ViewTarget, SrcName, InternalFormat, MinLevel, NumLevels, MinLayer, NumLayers);
	}

	static FORCEINLINE bool TimerQueryDisjoint()
	{
		bool Disjoint = false;

		if (bTimerQueryCanBeDisjoint)
		{
			GLint WasDisjoint = 0;
			glGetIntegerv(GL_GPU_DISJOINT_EXT, &WasDisjoint);
			Disjoint = (WasDisjoint != 0);
		}

		return Disjoint;
	}

	static FORCEINLINE GLint GetMaxComputeTextureImageUnits() { check(MaxComputeTextureImageUnits != -1); return MaxComputeTextureImageUnits; }
	static FORCEINLINE GLint GetMaxComputeUniformComponents() { check(MaxComputeUniformComponents != -1); return MaxComputeUniformComponents; }


	static FORCEINLINE ERHIFeatureLevel::Type GetFeatureLevel()
	{
		// Should this support a commandline forced ES2?
		return bES2Fallback ? ERHIFeatureLevel::ES2 : ERHIFeatureLevel::SM5;
	}

	static FORCEINLINE FString GetAdapterName()
	{
		return ANSI_TO_TCHAR((const ANSICHAR*)glGetString(GL_RENDERER));
	}

	static FPlatformOpenGLDevice*	CreateDevice()	UGL_REQUIRED(NULL)
	static FPlatformOpenGLContext*	CreateContext( FPlatformOpenGLDevice* Device, void* WindowHandle )	UGL_REQUIRED(NULL)

	static void ProcessQueryGLInt();
	static void ProcessExtensions(const FString& ExtensionsString);

	static FORCEINLINE int32 GetReadHalfFloatPixelsEnum() { return GL_HALF_FLOAT; }

protected:
	static GLsizei NextTextureName;
	static GLuint TextureNamesCache[OPENGL_NAME_CACHE_SIZE];
	static GLsizei NextBufferName;
	static GLuint BufferNamesCache[OPENGL_NAME_CACHE_SIZE];

	static GLint MaxComputeTextureImageUnits;
	static GLint MaxComputeUniformComponents;

	static GLint MajorVersion;
	static GLint MinorVersion;

	static GLint TimestampQueryBits;
	
	static bool bDebugContext;

	static bool bSupportsTessellation;
	static bool bSupportsTextureView;
	static bool bSupportsSeparateAlphaBlend;

	static bool bES2Fallback;

	/** GL_OES_vertex_array_object */
	static bool bSupportsVertexArrayObjects;

	/** GL_OES_depth_texture */
	static bool bSupportsDepthTexture;

	/** GL_OES_mapbuffer */
	static bool bSupportsMapBuffer;

	/** GL_ARB_occlusion_query2, GL_EXT_occlusion_query_boolean */
	static bool bSupportsOcclusionQueries;

	/** GL_OES_rgb8_rgba8 */
	static bool bSupportsRGBA8;

	/** GL_APPLE_texture_format_BGRA8888 */
	static bool bSupportsBGRA8888;

	/** Whether BGRA supported as color attachment */
	static bool bSupportsBGRA8888RenderTarget;

	/** GL_OES_vertex_half_float */
	static bool bSupportsVertexHalfFloat;

	/** GL_EXT_discard_framebuffer */
	static bool bSupportsDiscardFrameBuffer;

	/** GL_EXT_sRGB */
	static bool bSupportsSGRB;

	/** GL_NV_texture_compression_s3tc, GL_EXT_texture_compression_s3tc */
	static bool bSupportsDXT;

	/** GL_IMG_texture_compression_pvrtc */
	static bool bSupportsPVRTC;

	/** GL_ATI_texture_compression_atitc, GL_AMD_compressed_ATC_texture */
	static bool bSupportsATITC;

	/** GL_OES_compressed_ETC1_RGB8_texture */
	static bool bSupportsETC1;

	/** OpenGL ES 3.0 profile */
	static bool bSupportsETC2;

	/** GL_OES_texture_float */
	static bool bSupportsTextureFloat;

	/** GL_OES_texture_half_float */
	static bool bSupportsTextureHalfFloat;

	/** GL_EXT_color_buffer_float */
	static bool bSupportsColorBufferFloat;

	/** GL_EXT_color_buffer_half_float */
	static bool bSupportsColorBufferHalfFloat;

	/** GL_NV_image_formats */
	static bool bSupportsNvImageFormats;

	/** GL_EXT_shader_framebuffer_fetch */
	static bool bSupportsShaderFramebufferFetch;

	/** workaround for GL_EXT_shader_framebuffer_fetch */
	static bool bRequiresUEShaderFramebufferFetchDef;
	
	/** GL_ARM_shader_framebuffer_fetch_depth_stencil */
	static bool bSupportsShaderDepthStencilFetch;

	/** GL_EXT_MULTISAMPLED_RENDER_TO_TEXTURE */
	static bool bSupportsMultisampledRenderToTexture;

	/** GL_FRAGMENT_SHADER, GL_LOW_FLOAT */
	static int ShaderLowPrecision;

	/** GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT */
	static int ShaderMediumPrecision;

	/** GL_FRAGMENT_SHADER, GL_HIGH_FLOAT */
	static int ShaderHighPrecision;

	/** GL_NV_framebuffer_blit */
	static bool bSupportsNVFrameBufferBlit;

	/** GL_OES_packed_depth_stencil */
	static bool bSupportsPackedDepthStencil;

	/** textureCubeLodEXT */
	static bool bSupportsTextureCubeLodEXT;

	/** GL_EXT_shader_texture_lod */
	static bool bSupportsShaderTextureLod;

	/** textureCubeLod */
	static bool bSupportsShaderTextureCubeLod;

	/** GL_APPLE_copy_texture_levels */
	static bool bSupportsCopyTextureLevels;

	/** GL_EXT_texture_storage */
	static bool bSupportsTextureStorageEXT;

	/** GL_EXT_disjoint_timer_query or GL_NV_timer_query*/
	static bool bSupportsDisjointTimeQueries;

	/** Some timer query implementations are never disjoint */
	static bool bTimerQueryCanBeDisjoint;

	/** GL_NV_timer_query for timestamp queries */
	static bool bSupportsNvTimerQuery;

	/** GL_OES_vertex_type_10_10_10_2 */
	static bool bSupportsRGB10A2;

public:
	/* This is a hack to remove the calls to "precision sampler" defaults which are produced by the cross compiler however don't compile on some android platforms */
	static bool bRequiresDontEmitPrecisionForTextureSamplers;

	/* Some android platforms require textureCubeLod to be used some require textureCubeLodEXT however they either inconsistently or don't use the GL_TextureCubeLodEXT extension definition */
	static bool bRequiresTextureCubeLodEXTToTextureCubeLodDefine;

	/* This is a hack to remove the gl_FragCoord if shader will fail to link if exceeding the max varying on android platforms */
	static bool bRequiresGLFragCoordVaryingLimitHack;

	/* This hack fixes an issue with SGX540 compiler which can get upset with some operations that mix highp and mediump */
	static bool bRequiresTexture2DPrecisionHack;

	/* This is to avoid a bug in Adreno drivers that define GL_ARM_shader_framebuffer_fetch_depth_stencil even when device does not support this extension  */
	static bool bRequiresARMShaderFramebufferFetchDepthStencilUndef;

	/* Indicates shader compiler hack checks are being tested */
	static bool bIsCheckingShaderCompilerHacks;
};

// yes they are different between the ES2 extension and ES3.x and GL3.x core
static_assert(GL_HALF_FLOAT_OES != GL_HALF_FLOAT, "GL_HALF_FLOAT_OES and GL_HALF_FLOAT should not be #defined to the same value");

#ifndef GL_FILL
#define GL_FILL 0x1B02
#endif
#ifndef GL_SAMPLER_1D_SHADOW
#define GL_SAMPLER_1D_SHADOW 0x8B61
#endif
#ifndef GL_DOUBLE
#define GL_DOUBLE 0x140A
#endif
#ifndef GL_SAMPLER_1D
#define GL_SAMPLER_1D 0x8B5D
#endif
#ifndef GL_RGBA16
#define GL_RGBA16 0x805B
#endif
#ifndef GL_RG16
#define GL_RG16 0x822C
#endif
#ifndef GL_SAMPLES_PASSED
#define GL_SAMPLES_PASSED 0x8914
#endif

#ifndef GL_POLYGON_OFFSET_LINE
#define GL_POLYGON_OFFSET_LINE 0x2A02
#endif

#ifndef GL_POLYGON_OFFSET_POINT
#define GL_POLYGON_OFFSET_POINT 0x2A01
#endif

#ifndef GL_TEXTURE_LOD_BIAS
#define GL_TEXTURE_LOD_BIAS 0x8501
#endif

#ifndef GL_R16
#define GL_R16 0x822A
#endif

#ifndef GL_POINT
#define GL_POINT 0x1B00
#endif

#ifndef GL_LINE
#define GL_LINE 0x1B01
#endif 

#ifndef GL_TEXTURE_BUFFER
#define GL_TEXTURE_BUFFER GL_TEXTURE_BUFFER_EXT
#endif

#ifndef GL_DEBUG_SOURCE_OTHER_ARB
#define GL_DEBUG_SOURCE_OTHER_ARB GL_DEBUG_SOURCE_OTHER_KHR
#endif

#ifndef GL_DEBUG_SOURCE_API_ARB
#define GL_DEBUG_SOURCE_API_ARB GL_DEBUG_SOURCE_API_KHR
#endif

#ifndef GL_DEBUG_TYPE_ERROR_ARB
#define GL_DEBUG_TYPE_ERROR_ARB GL_DEBUG_TYPE_ERROR_KHR
#endif

#ifndef GL_DEBUG_TYPE_OTHER_ARB
#define GL_DEBUG_TYPE_OTHER_ARB GL_DEBUG_TYPE_OTHER_KHR
#endif

#ifndef GL_DEBUG_TYPE_MARKER
#define GL_DEBUG_TYPE_MARKER GL_DEBUG_TYPE_MARKER_KHR
#endif

#ifndef GL_DEBUG_TYPE_PUSH_GROUP
#define GL_DEBUG_TYPE_PUSH_GROUP GL_DEBUG_TYPE_PUSH_GROUP_KHR
#endif

#ifndef GL_DEBUG_TYPE_POP_GROUP
#define GL_DEBUG_TYPE_POP_GROUP GL_DEBUG_TYPE_POP_GROUP_KHR
#endif

#ifndef GL_DEBUG_SEVERITY_HIGH_ARB
#define GL_DEBUG_SEVERITY_HIGH_ARB GL_DEBUG_SEVERITY_HIGH_KHR
#endif 

#ifndef GL_DEBUG_SEVERITY_LOW_ARB
#define GL_DEBUG_SEVERITY_LOW_ARB GL_DEBUG_SEVERITY_LOW_KHR
#endif 

#ifndef GL_DEBUG_SEVERITY_NOTIFICATION
#define GL_DEBUG_SEVERITY_NOTIFICATION GL_DEBUG_SEVERITY_NOTIFICATION_KHR
#endif

#ifndef GL_GEOMETRY_SHADER
#define GL_GEOMETRY_SHADER GL_GEOMETRY_SHADER_EXT
#endif

#ifndef GL_FRAMEBUFFER_SRGB
#define GL_FRAMEBUFFER_SRGB GL_FRAMEBUFFER_SRGB_EXT
#endif
