// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
#include "AllowWindowsPlatformTypes.h"

// D3D headers.


// Disable macro redefinition warning for compatibility with Windows SDK 8+
#pragma warning(push)
#pragma warning(disable : 4005)	// macro redefinition

#include <d3d11.h>
#include <d3dx11.h>
#include <D3DX10.h>
#include <D3Dcompiler.h>

#pragma warning(pop)

#include "HideWindowsPlatformTypes.h"
#endif		// PLATFORM_WINDOWS

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/wglext.h>
#include "HideWindowsPlatformTypes.h"
#elif PLATFORM_MAC
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#elif PLATFORM_IOS

#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>

// renamed keywords
#define glMapBuffer		glMapBufferOES
#define glUnmapBuffer	glUnmapBufferOES
#define GL_WRITE_ONLY	GL_WRITE_ONLY_OES
#define GL_CLAMP		GL_CLAMP_TO_EDGE
#define GL_RGBA8		GL_RGBA
#define GL_UNSIGNED_INT_8_8_8_8_REV	GL_UNSIGNED_BYTE

// unused functions
#define glTexEnvf(...)
#define glTexEnvi(...)
#define glAlphaFunc(...)

#elif PLATFORM_LINUX
//#define GLCOREARB_PROTOTYPES 
#include <GL/glcorearb.h>
//#include <GL/glext.h>
#include "SDL.h"

#endif // PLATFORM_LINUX
