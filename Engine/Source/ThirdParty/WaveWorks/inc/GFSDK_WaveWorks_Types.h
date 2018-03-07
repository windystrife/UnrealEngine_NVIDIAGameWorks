// This code contains NVIDIA Confidential Information and is disclosed 
// under the Mutual Non-Disclosure Agreement. 
// 
// Notice 
// ALL NVIDIA DESIGN SPECIFICATIONS AND CODE ("MATERIALS") ARE PROVIDED "AS IS" NVIDIA MAKES 
// NO REPRESENTATIONS, WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO 
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ANY IMPLIED WARRANTIES OF NONINFRINGEMENT, 
// MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE. 
// 
// NVIDIA Corporation assumes no responsibility for the consequences of use of such 
// information or for any infringement of patents or other rights of third parties that may 
// result from its use. No license is granted by implication or otherwise under any patent 
// or patent rights of NVIDIA Corporation. No third party distribution is allowed unless 
// expressly authorized by NVIDIA.  Details are subject to change without notice. 
// This code supersedes and replaces all information previously supplied. 
// NVIDIA Corporation products are not authorized for use as critical 
// components in life support devices or systems without express written approval of 
// NVIDIA Corporation. 
// 
// Copyright © 2008- 2013 NVIDIA Corporation. All rights reserved.
//
// NVIDIA Corporation and its licensors retain all intellectual property and proprietary
// rights in and to this software and related documentation and any modifications thereto.
// Any use, reproduction, disclosure or distribution of this software and related
// documentation without an express license agreement from NVIDIA Corporation is
// strictly prohibited.
//

#ifndef _GFSDK_WAVEWORKS_TYPES_H
#define _GFSDK_WAVEWORKS_TYPES_H

#include <GFSDK_WaveWorks_Common.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
  Calling conventions
  ===========================================================================*/

#define GFSDK_WAVEWORKS_CALL_CONV __GFSDK_CDECL__


/*===========================================================================
  Result codes
  ===========================================================================*/
enum gfsdk_waveworks_result {
	gfsdk_waveworks_result_INTERNAL_ERROR = -2,
	gfsdk_waveworks_result_FAIL = -1,
	gfsdk_waveworks_result_OK = 0,
	gfsdk_waveworks_result_NONE = 1,
	gfsdk_waveworks_result_WOULD_BLOCK = 2,
};

/*===========================================================================
  Fwd decls of system types
  ===========================================================================*/
struct _D3DADAPTER_IDENTIFIER9;
struct IDXGIAdapter;
struct IDirect3DDevice9;
struct IDirect3D9;
struct ID3D10Device;
struct ID3D11Device;
struct ID3D11DeviceContext;

/*===========================================================================
  Value class used in answering queries about shader inputs
  ===========================================================================*/
struct GFSDK_WaveWorks_ShaderInput_Desc
{
	enum InputType
	{
		VertexShader_FloatConstant = 0,
		VertexShader_ConstantBuffer,
		VertexShader_Texture,
		VertexShader_Sampler,
		HullShader_FloatConstant,
		HullShader_ConstantBuffer,
		HullShader_Texture,
		HullShader_Sampler,
		DomainShader_FloatConstant,
		DomainShader_ConstantBuffer,
		DomainShader_Texture,
		DomainShader_Sampler,
		PixelShader_FloatConstant,
		PixelShader_ConstantBuffer,
		PixelShader_Texture,
		PixelShader_Sampler,
		GL_VertexShader_UniformLocation,
		GL_TessEvalShader_UniformLocation,
		GL_FragmentShader_UniformLocation,
		GL_VertexShader_TextureBindLocation,
		GL_TessEvalShader_TextureBindLocation,
		GL_FragmentShader_TextureBindLocation,
		GL_VertexShader_TextureArrayBindLocation,
		GL_TessEvalShader_TextureArrayBindLocation,
		GL_FragmentShader_TextureArrayBindLocation,
		GL_AttribLocation
	};

	InputType Type;
	gfsdk_cstr Name;
	gfsdk_U32 RegisterOffset;	// This will be the offset specified to the shader macro i.e. 'Regoff'
};

enum { GFSDK_WaveWorks_UnusedShaderInputRegisterMapping = 0xFFFFFFFF };

/*===========================================================================
  Flags used to specify what state to preserve during rendering
  ===========================================================================*/
enum GFSDK_WaveWorks_StatePreserveFlags
{
	GFSDK_WaveWorks_StatePreserve_None = 0,
	GFSDK_WaveWorks_StatePreserve_Shaders = 1,
	GFSDK_WaveWorks_StatePreserve_ShaderConstants = 2,
	GFSDK_WaveWorks_StatePreserve_Samplers = 4,			// Includes textures/shader-resources
	GFSDK_WaveWorks_StatePreserve_RenderTargets = 8,
	GFSDK_WaveWorks_StatePreserve_Viewports = 16,
	GFSDK_WaveWorks_StatePreserve_Streams = 32,			// Includes vertex/index-buffers, decls/input-layouts
	GFSDK_WaveWorks_StatePreserve_UnorderedAccessViews = 64,
	GFSDK_WaveWorks_StatePreserve_Other = 128,
	GFSDK_WaveWorks_StatePreserve_All = 0xFFFFFFFF,
	GFSDK_WaveWorks_StatePreserve_ForceDWORD = 0xFFFFFFFF
};

/*===========================================================================
  Specifies the detail level of the simulation
  ===========================================================================*/
enum GFSDK_WaveWorks_Simulation_DetailLevel
{
	GFSDK_WaveWorks_Simulation_DetailLevel_Normal = 0,
	GFSDK_WaveWorks_Simulation_DetailLevel_High,
	GFSDK_WaveWorks_Simulation_DetailLevel_Extreme,
	Num_GFSDK_WaveWorks_Simulation_DetailLevels
};

/*===========================================================================
  Controls the threading model when the CPU simulation path is used
  ===========================================================================*/
enum GFSDK_WaveWorks_Simulation_CPU_Threading_Model
{
	GFSDK_WaveWorks_Simulation_CPU_Threading_Model_None = -1,		// Do not use worker threads
	GFSDK_WaveWorks_Simulation_CPU_Threading_Model_Automatic = 0,	// Use an automatically-determined number of worker threads
	GFSDK_WaveWorks_Simulation_CPU_Threading_Model_1 = 1,			// Use 1 worker thread
	GFSDK_WaveWorks_Simulation_CPU_Threading_Model_2 = 2,			// Use 2 worker threads
	GFSDK_WaveWorks_Simulation_CPU_Threading_Model_3 = 3,			// Use 3 worker threads
	// etc...
	// i.e. it's safe to use higher values to represent even larger thread counts
};

/*===========================================================================
  Handles
  ===========================================================================*/
struct GFSDK_WaveWorks_Context;
struct GFSDK_WaveWorks_Simulation;
struct GFSDK_WaveWorks_Quadtree;
struct GFSDK_WaveWorks_Savestate;
typedef GFSDK_WaveWorks_Context* GFSDK_WaveWorks_ContextHandle;
typedef GFSDK_WaveWorks_Simulation* GFSDK_WaveWorks_SimulationHandle;
typedef GFSDK_WaveWorks_Quadtree* GFSDK_WaveWorks_QuadtreeHandle;
typedef GFSDK_WaveWorks_Savestate* GFSDK_WaveWorks_SavestateHandle;

/*===========================================================================
  API GUID
  ===========================================================================*/
struct GFSDK_WaveWorks_API_GUID
{
	GFSDK_WaveWorks_API_GUID(gfsdk_U32 c1, gfsdk_U32 c2, gfsdk_U32 c3, gfsdk_U32 c4) :
		Component1(c1), Component2(c2), Component3(c3), Component4(c4)
	{
	}

	gfsdk_U32 Component1;
	gfsdk_U32 Component2;
	gfsdk_U32 Component3;
	gfsdk_U32 Component4;
};

/*===========================================================================
  Kick IDs
  ===========================================================================*/

#define GFSDK_WaveWorks_InvalidKickID gfsdk_U64(-1)

/*===========================================================================
  OpenGL types
  ===========================================================================*/

// We reluctantly pull in the defs for ptrdiff_t and uint64_t here - there are of course other ways
// of defining these on a particular platform without needing an include, but they are not generally
// portable
#if !defined(_MSC_VER)
#include <stddef.h>
#include <stdint.h>
#endif

// GL base types
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
#if defined(_MSC_VER)
typedef unsigned __int64 GLuint64;
#else
typedef uint64_t GLuint64;
#endif
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef void GLvoid;
typedef unsigned int GLbitfield;
#if defined(__ANDROID__) 
typedef long GLintptr;
typedef long GLsizeiptr;
#else
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
#endif
typedef char GLchar;
typedef void* GLhandleARB;

// GL function type definitions
#define APIENTRYP __GFSDK_STDCALL__ *
typedef GLboolean (APIENTRYP PFNGLUNMAPBUFFERPROC) (GLenum target);
typedef GLenum (APIENTRYP PFNGLCHECKFRAMEBUFFERSTATUSPROC) (GLenum target);
typedef GLenum (APIENTRYP PFNGLGETERRORPROC) (void);
typedef GLint (APIENTRYP PFNGLGETATTRIBLOCATIONPROC) (GLuint program, const GLchar *name);
typedef GLint (APIENTRYP PFNGLGETUNIFORMLOCATIONPROC) (GLuint program, const GLchar *name);
typedef GLint (APIENTRYP PFNGLGETUNIFORMLOCATIONPROC) (GLuint program, const GLchar *name);
typedef GLuint (APIENTRYP PFNGLCREATEPROGRAMPROC) (void);
typedef GLuint (APIENTRYP PFNGLCREATESHADERPROC) (GLenum type);
typedef GLvoid* (APIENTRYP PFNGLMAPBUFFERRANGEPROC) (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
typedef void (APIENTRYP PFNGLACTIVETEXTUREPROC) (GLenum texture);
typedef void (APIENTRYP PFNGLATTACHSHADERPROC) (GLuint program, GLuint shader);
typedef void (APIENTRYP PFNGLBINDBUFFERPROC) (GLenum target, GLuint buffer);
typedef void (APIENTRYP PFNGLBINDFRAMEBUFFERPROC) (GLenum target, GLuint framebuffer);
typedef void (APIENTRYP PFNGLBINDTEXTUREPROC) (GLenum target, GLuint texture);
typedef void (APIENTRYP PFNGLBLITFRAMEBUFFERPROC) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef void (APIENTRYP PFNGLBUFFERDATAPROC) (GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);
typedef void (APIENTRYP PFNGLCLEARCOLORPROC) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (APIENTRYP PFNGLCLEARPROC) (GLbitfield mask);
typedef void (APIENTRYP PFNGLCOLORMASKPROC) (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
typedef void (APIENTRYP PFNGLCOMPILESHADERPROC) (GLuint shader);
typedef void (APIENTRYP PFNGLDELETEBUFFERSPROC) (GLsizei n, const GLuint *buffers);
typedef void (APIENTRYP PFNGLDELETEFRAMEBUFFERSPROC) (GLsizei n, const GLuint *framebuffers);
typedef void (APIENTRYP PFNGLDELETEPROGRAMPROC) (GLuint program);
typedef void (APIENTRYP PFNGLDELETEQUERIESPROC) (GLsizei n, const GLuint *ids);
typedef void (APIENTRYP PFNGLDELETESHADERPROC) (GLuint shader);
typedef void (APIENTRYP PFNGLDELETETEXTURESPROC) (GLsizei n, const GLuint *textures);
typedef void (APIENTRYP PFNGLDISABLEPROC) (GLenum cap);
typedef void (APIENTRYP PFNGLDISABLEVERTEXATTRIBARRAYPROC) (GLuint index);
typedef void (APIENTRYP PFNGLDRAWELEMENTSPROC) (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
typedef void (APIENTRYP PFNGLENABLEVERTEXATTRIBARRAYPROC) (GLuint index);
typedef void (APIENTRYP PFNGLFRAMEBUFFERTEXTURE2DPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (APIENTRYP PFNGLFRAMEBUFFERTEXTURELAYERPROC) (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
typedef void (APIENTRYP PFNGLGENBUFFERSPROC) (GLsizei n, GLuint *buffers);
typedef void (APIENTRYP PFNGLGENERATEMIPMAPPROC) (GLenum target);
typedef void (APIENTRYP PFNGLGENFRAMEBUFFERSPROC) (GLsizei n, GLuint *framebuffers);
typedef void (APIENTRYP PFNGLGENQUERIESPROC) (GLsizei n, GLuint *ids);
typedef void (APIENTRYP PFNGLGETINTEGERVPROC) (GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLGETQUERYOBJECTUI64VPROC) (GLuint id, GLenum pname, GLuint64 *params);
typedef void (APIENTRYP PFNGLGENTEXTURESPROC) (GLsizei n, GLuint *textures);
typedef void (APIENTRYP PFNGLGETPROGRAMINFOLOGPROC) (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRYP PFNGLGETPROGRAMIVPROC) (GLuint program, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLGETSHADERINFOLOGPROC) (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRYP PFNGLGETSHADERIVPROC) (GLuint shader, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLLINKPROGRAMPROC) (GLuint program);
typedef void (APIENTRYP PFNGLPATCHPARAMETERIPROC) (GLenum pname, GLint value);
typedef void (APIENTRYP PFNGLQUERYCOUNTERPROC) (GLuint id, GLenum target);
typedef void (APIENTRYP PFNGLSHADERSOURCEPROC) (GLuint shader, GLsizei count, const GLchar* const *string, const GLint *length);
typedef void (APIENTRYP PFNGLTEXIMAGE2DPROC) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRYP PFNGLTEXIMAGE3DPROC) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRYP PFNGLTEXPARAMETERFPROC) (GLenum target, GLenum pname, GLfloat param);
typedef void (APIENTRYP PFNGLTEXPARAMETERIPROC) (GLenum target, GLenum pname, GLint param);
typedef void (APIENTRYP PFNGLTEXSUBIMAGE2DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
typedef void (APIENTRYP PFNGLUNIFORM1FPROC) (GLint location, GLfloat v0);
typedef void (APIENTRYP PFNGLUNIFORM1IPROC) (GLint location, GLint v0);
typedef void (APIENTRYP PFNGLUNIFORM3FVPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORM4FVPROC) (GLint location, GLsizei count, const GLfloat *value);
typedef void (APIENTRYP PFNGLUNIFORMMATRIX3X4FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
typedef void (APIENTRYP PFNGLUSEPROGRAMPROC) (GLuint program);
typedef void (APIENTRYP PFNGLVERTEXATTRIBPOINTERPROC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRYP PFNGLVIEWPORTPROC) (GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRYP PFNGLGETACTIVEATTRIBPROC) (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, char *name);
typedef void (APIENTRYP PFNGLREADBUFFERPROC) (GLenum);
typedef void (APIENTRYP PFNGLDRAWBUFFERSPROC) (GLsizei n, const GLenum *bufs); 


// GL functions used by WaveWorks, in alphabetic order
struct GFSDK_WAVEWORKS_GLFunctions
{
	PFNGLACTIVETEXTUREPROC glActiveTexture;
	PFNGLATTACHSHADERPROC glAttachShader;
	PFNGLBINDBUFFERPROC glBindBuffer;
	PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
	PFNGLBINDTEXTUREPROC glBindTexture;
	PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer;
	PFNGLBUFFERDATAPROC glBufferData;
	PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
	PFNGLCLEARCOLORPROC glClearColor;
	PFNGLCLEARPROC glClear;
	PFNGLCOLORMASKPROC glColorMask;
	PFNGLCOMPILESHADERPROC glCompileShader;
	PFNGLCREATEPROGRAMPROC glCreateProgram;
	PFNGLCREATESHADERPROC glCreateShader;
	PFNGLDELETEBUFFERSPROC glDeleteBuffers;
	PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
	PFNGLDELETEPROGRAMPROC glDeleteProgram;
	PFNGLDELETEQUERIESPROC glDeleteQueries;
	PFNGLDELETESHADERPROC glDeleteShader;
	PFNGLDELETETEXTURESPROC glDeleteTextures;
	PFNGLDISABLEPROC glDisable;
	PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
	PFNGLDRAWELEMENTSPROC glDrawElements;
	PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
	PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
	PFNGLFRAMEBUFFERTEXTURELAYERPROC glFramebufferTextureLayer;
	PFNGLGENBUFFERSPROC glGenBuffers;
	PFNGLGENERATEMIPMAPPROC glGenerateMipmap;
	PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
	PFNGLGENQUERIESPROC glGenQueries;
	PFNGLGENTEXTURESPROC glGenTextures;
	PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
	PFNGLGETERRORPROC glGetError;
	PFNGLGETINTEGERVPROC glGetIntegerv;
	PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
	PFNGLGETPROGRAMIVPROC glGetProgramiv;
	PFNGLGETQUERYOBJECTUI64VPROC glGetQueryObjectui64v;
	PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
	PFNGLGETSHADERIVPROC glGetShaderiv;
	PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
	PFNGLLINKPROGRAMPROC glLinkProgram;
	PFNGLMAPBUFFERRANGEPROC glMapBufferRange;
	PFNGLPATCHPARAMETERIPROC glPatchParameteri;
	PFNGLQUERYCOUNTERPROC glQueryCounter;
	PFNGLSHADERSOURCEPROC glShaderSource;
	PFNGLTEXIMAGE2DPROC glTexImage2D;
	PFNGLTEXIMAGE3DPROC glTexImage3D;
	PFNGLTEXPARAMETERFPROC glTexParameterf;
	PFNGLTEXPARAMETERIPROC glTexParameteri;
	PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D;
	PFNGLUNIFORM1FPROC glUniform1f;
	PFNGLUNIFORM1IPROC glUniform1i;
	PFNGLUNIFORM3FVPROC glUniform3fv;
	PFNGLUNIFORM4FVPROC glUniform4fv;
	PFNGLUNIFORMMATRIX3X4FVPROC glUniformMatrix3x4fv;
	PFNGLUNMAPBUFFERPROC glUnmapBuffer;
	PFNGLUSEPROGRAMPROC glUseProgram;
	PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
	PFNGLVIEWPORTPROC glViewport;
	PFNGLGETACTIVEATTRIBPROC glGetActiveAttrib;
	PFNGLREADBUFFERPROC glReadBuffer;
	PFNGLDRAWBUFFERSPROC glDrawBuffers;
};

#ifdef __cplusplus
}; //extern "C" {
#endif

#endif	// _GFSDK_WAVEWORKS_TYPES_H
