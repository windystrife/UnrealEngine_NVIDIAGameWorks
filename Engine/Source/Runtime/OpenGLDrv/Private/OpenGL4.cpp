// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGL4.cpp: OpenGL 4.3 implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "OpenGLDrv.h"
#include "OpenGLDrvPrivate.h"

#if OPENGL_GL4

GLint FOpenGL4::MaxComputeTextureImageUnits = -1;
GLint FOpenGL4::MaxComputeUniformComponents = -1;
bool FOpenGL4::bSupportsComputeShaders = true;
bool FOpenGL4::bSupportsGPUMemoryInfo = false;
bool FOpenGL4::bSupportsVertexAttribBinding = true;
bool FOpenGL4::bSupportsTextureView = true;

void FOpenGL4::ProcessQueryGLInt()
{
	if (bSupportsComputeShaders)
	{
		GET_GL_INT(GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS, 0, MaxComputeTextureImageUnits);
		GET_GL_INT(GL_MAX_COMPUTE_UNIFORM_COMPONENTS, 0, MaxComputeUniformComponents);
	}

}

void FOpenGL4::ProcessExtensions( const FString& ExtensionsString )
{
    int32 MajorVersion =0;
    int32 MinorVersion =0;
 
	FString Version = ANSI_TO_TCHAR((const ANSICHAR*)glGetString(GL_VERSION));
	FString MajorString, MinorString;
	if (Version.Split(TEXT("."), &MajorString, &MinorString))
	{
		MajorVersion = FCString::Atoi(*MajorString);
		MinorVersion = FCString::Atoi(*MinorString);
	}
	check(MajorVersion!=0);


	bSupportsGPUMemoryInfo = ExtensionsString.Contains(TEXT("GL_NVX_gpu_memory_info"));
	bSupportsComputeShaders = ExtensionsString.Contains(TEXT("GL_ARB_compute_shader")) || (MajorVersion ==4 && MinorVersion >= 3) || (MajorVersion > 4);
	bSupportsVertexAttribBinding = ExtensionsString.Contains(TEXT("GL_ARB_vertex_attrib_binding")) || (MajorVersion == 4 && MinorVersion >= 3) || (MajorVersion > 4);
	bSupportsTextureView = ExtensionsString.Contains(TEXT("GL_ARB_texture_view")) || (MajorVersion == 4 && MinorVersion >= 3) || (MajorVersion > 4);

	//Process Queries after extensions to avoid queries that use functionality that might not be present
	ProcessQueryGLInt();

	FOpenGL3::ProcessExtensions(ExtensionsString);
}

#ifndef GL_NVX_gpu_memory_info
#define GL_NVX_gpu_memory_info 1
#define GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX          0x9047
#define GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX    0x9048
#define GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX  0x9049
#define GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX            0x904A
#define GL_GPU_MEMORY_INFO_EVICTED_MEMORY_NVX            0x904B
#endif

uint64 FOpenGL4::GetVideoMemorySize()
{
	uint64 VideoMemorySize = 0;

	if (bSupportsGPUMemoryInfo)
	{
		GLint VMSizeKB = 0;

		glGetIntegerv( GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &VMSizeKB);

		VideoMemorySize = VMSizeKB * 1024ll;
	}

	return VideoMemorySize;
}

static FORCEINLINE bool SupportsVertexAttribBinding()
{
	return glVertexAttribBinding != NULL;
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

#endif
