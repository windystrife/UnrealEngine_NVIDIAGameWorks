// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// .

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryWriter.h"
#include "ShaderFormatOpenGL.h"

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
	#include "Windows/PreWindowsApi.h"
	#include <objbase.h>
	#include <assert.h>
	#include <stdio.h>
	#include "Windows/PostWindowsApi.h"
	#include "Windows/MinWindows.h"
#include "HideWindowsPlatformTypes.h"
#endif
#include "ShaderCore.h"
#include "ShaderPreprocessor.h"
#include "ShaderCompilerCommon.h"
#include "GlslBackend.h"
#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
	#include <GL/glcorearb.h>
	#include <GL/glext.h>
	#include <GL/wglext.h>
#include "HideWindowsPlatformTypes.h"
#elif PLATFORM_LINUX
	#define GL_GLEXT_PROTOTYPES 1
	#include <GL/glcorearb.h>
	#include <GL/glext.h>
	#include "SDL.h"
	GLAPI GLuint APIENTRY glCreateShader (GLenum type);
	GLAPI void APIENTRY glShaderSource (GLuint shader, GLsizei count, const GLchar* const *string, const GLint *length);
	typedef SDL_Window*		SDL_HWindow;
	typedef SDL_GLContext	SDL_HGLContext;
	struct FPlatformOpenGLContext
	{
		SDL_HWindow		hWnd;
		SDL_HGLContext	hGLContext;		//	this is a (void*) pointer
	};
#elif PLATFORM_MAC
	#include <OpenGL/OpenGL.h>
	#include <OpenGL/gl3.h>
	#include <OpenGL/gl3ext.h>
	#ifndef GL_COMPUTE_SHADER
	#define GL_COMPUTE_SHADER 0x91B9
	#endif
	#ifndef GL_TESS_EVALUATION_SHADER
	#define GL_TESS_EVALUATION_SHADER 0x8E87
	#endif
	#ifndef GL_TESS_CONTROL_SHADER
	#define GL_TESS_CONTROL_SHADER 0x8E88
	#endif
#endif
	#include "OpenGLUtil.h"
#include "OpenGLShaderResources.h"

DEFINE_LOG_CATEGORY_STATIC(LogOpenGLShaderCompiler, Log, All);


#define VALIDATE_GLSL_WITH_DRIVER		0
#define ENABLE_IMAGINATION_COMPILER		1


static FORCEINLINE bool IsES2Platform(GLSLVersion Version)
{
	return (Version == GLSL_ES2 || Version == GLSL_150_ES2 || Version == GLSL_ES2_WEBGL || Version == GLSL_ES2_IOS || Version == GLSL_150_ES2_NOUB);
}

static FORCEINLINE bool IsPCES2Platform(GLSLVersion Version)
{
	return (Version == GLSL_150_ES2 || Version == GLSL_150_ES2_NOUB || Version == GLSL_150_ES3_1);
}

// This function should match OpenGLShaderPlatformSeparable
bool FOpenGLFrontend::SupportsSeparateShaderObjects(GLSLVersion Version)
{
	// Only desktop shader platforms can use separable shaders for now,
	// the generated code relies on macros supplied at runtime to determine whether
	// shaders may be separable and/or linked.
	return Version == GLSL_150 || Version == GLSL_150_ES2 || Version == GLSL_150_ES2_NOUB || Version == GLSL_150_ES3_1 || Version == GLSL_430;
}

/*------------------------------------------------------------------------------
	Shader compiling.
------------------------------------------------------------------------------*/

#if PLATFORM_WINDOWS
/** List all OpenGL entry points needed for shader compilation. */
#define ENUM_GL_ENTRYPOINTS(EnumMacro) \
	EnumMacro(PFNGLCOMPILESHADERPROC,glCompileShader) \
	EnumMacro(PFNGLCREATESHADERPROC,glCreateShader) \
	EnumMacro(PFNGLDELETESHADERPROC,glDeleteShader) \
	EnumMacro(PFNGLGETSHADERIVPROC,glGetShaderiv) \
	EnumMacro(PFNGLGETSHADERINFOLOGPROC,glGetShaderInfoLog) \
	EnumMacro(PFNGLSHADERSOURCEPROC,glShaderSource) \
	EnumMacro(PFNGLDELETEBUFFERSPROC,glDeleteBuffers)

/** Define all GL functions. */
#define DEFINE_GL_ENTRYPOINTS(Type,Func) static Type Func = NULL;
ENUM_GL_ENTRYPOINTS(DEFINE_GL_ENTRYPOINTS);

/** This function is handled separately because it is used to get a real context. */
static PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;

/** Platform specific OpenGL context. */
struct FPlatformOpenGLContext
{
	HWND WindowHandle;
	HDC DeviceContext;
	HGLRC OpenGLContext;
};

/**
 * A dummy wndproc.
 */
static LRESULT CALLBACK PlatformDummyGLWndproc(HWND hWnd, uint32 Message, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hWnd, Message, wParam, lParam);
}

/**
 * Initialize a pixel format descriptor for the given window handle.
 */
static void PlatformInitPixelFormatForDevice(HDC DeviceContext)
{
	// Pixel format descriptor for the context.
	PIXELFORMATDESCRIPTOR PixelFormatDesc;
	FMemory::Memzero(PixelFormatDesc);
	PixelFormatDesc.nSize		= sizeof(PIXELFORMATDESCRIPTOR);
	PixelFormatDesc.nVersion	= 1;
	PixelFormatDesc.dwFlags		= PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	PixelFormatDesc.iPixelType	= PFD_TYPE_RGBA;
	PixelFormatDesc.cColorBits	= 32;
	PixelFormatDesc.cDepthBits	= 0;
	PixelFormatDesc.cStencilBits	= 0;
	PixelFormatDesc.iLayerType	= PFD_MAIN_PLANE;

	// Set the pixel format and create the context.
	int32 PixelFormat = ChoosePixelFormat(DeviceContext, &PixelFormatDesc);
	if (!PixelFormat || !SetPixelFormat(DeviceContext, PixelFormat, &PixelFormatDesc))
	{
		UE_LOG(LogOpenGLShaderCompiler, Fatal,TEXT("Failed to set pixel format for device context."));
	}
}

/**
 * Create a dummy window used to construct OpenGL contexts.
 */
static void PlatformCreateDummyGLWindow(FPlatformOpenGLContext* OutContext)
{
	const TCHAR* WindowClassName = TEXT("DummyGLToolsWindow");

	// Register a dummy window class.
	static bool bInitializedWindowClass = false;
	if (!bInitializedWindowClass)
	{
		WNDCLASS wc;

		bInitializedWindowClass = true;
		FMemory::Memzero(wc);
		wc.style = CS_OWNDC;
		wc.lpfnWndProc = PlatformDummyGLWndproc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = NULL;
		wc.hIcon = NULL;
		wc.hCursor = NULL;
		wc.hbrBackground = (HBRUSH)(COLOR_MENUTEXT);
		wc.lpszMenuName = NULL;
		wc.lpszClassName = WindowClassName;
		ATOM ClassAtom = ::RegisterClass(&wc);
		check(ClassAtom);
	}

	// Create a dummy window.
	OutContext->WindowHandle = CreateWindowEx(
		WS_EX_WINDOWEDGE,
		WindowClassName,
		NULL,
		WS_POPUP,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, NULL, NULL);
	check(OutContext->WindowHandle);

	// Get the device context.
	OutContext->DeviceContext = GetDC(OutContext->WindowHandle);
	check(OutContext->DeviceContext);
	PlatformInitPixelFormatForDevice(OutContext->DeviceContext);
}

/**
 * Create a core profile OpenGL context.
 */
static void PlatformCreateOpenGLContextCore(FPlatformOpenGLContext* OutContext, int MajorVersion, int MinorVersion, HGLRC InParentContext)
{
	check(wglCreateContextAttribsARB);
	check(OutContext);
	check(OutContext->DeviceContext);

	int AttribList[] =
	{
		WGL_CONTEXT_MAJOR_VERSION_ARB, MajorVersion,
		WGL_CONTEXT_MINOR_VERSION_ARB, MinorVersion,
		WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB | WGL_CONTEXT_DEBUG_BIT_ARB,
		WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		0
	};

	OutContext->OpenGLContext = wglCreateContextAttribsARB(OutContext->DeviceContext, InParentContext, AttribList);
	check(OutContext->OpenGLContext);
}

/**
 * Make the context current.
 */
static void PlatformMakeGLContextCurrent(FPlatformOpenGLContext* Context)
{
	check(Context && Context->OpenGLContext && Context->DeviceContext);
	wglMakeCurrent(Context->DeviceContext, Context->OpenGLContext);
}

/**
 * Initialize an OpenGL context so that shaders can be compiled.
 */
static void PlatformInitOpenGL(void*& ContextPtr, void*& PrevContextPtr, int InMajorVersion, int InMinorVersion)
{
	static FPlatformOpenGLContext ShaderCompileContext = {0};

	ContextPtr = (void*)wglGetCurrentDC();
	PrevContextPtr = (void*)wglGetCurrentContext();

	if (ShaderCompileContext.OpenGLContext == NULL && InMajorVersion && InMinorVersion)
	{
		PlatformCreateDummyGLWindow(&ShaderCompileContext);

		// Disable warning C4191: 'type cast' : unsafe conversion from 'PROC' to 'XXX' while getting GL entry points.
		#pragma warning(push)
		#pragma warning(disable:4191)

		if (wglCreateContextAttribsARB == NULL)
		{
			// Create a dummy context so that wglCreateContextAttribsARB can be initialized.
			ShaderCompileContext.OpenGLContext = wglCreateContext(ShaderCompileContext.DeviceContext);
			check(ShaderCompileContext.OpenGLContext);
			PlatformMakeGLContextCurrent(&ShaderCompileContext);
			wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
			check(wglCreateContextAttribsARB);
			wglDeleteContext(ShaderCompileContext.OpenGLContext);
		}

		// Create a context so that remaining GL function pointers can be initialized.
		PlatformCreateOpenGLContextCore(&ShaderCompileContext, InMajorVersion, InMinorVersion, /*InParentContext=*/ NULL);
		check(ShaderCompileContext.OpenGLContext);
		PlatformMakeGLContextCurrent(&ShaderCompileContext);

		if (glCreateShader == NULL)
		{
			// Initialize all entry points.
			#define GET_GL_ENTRYPOINTS(Type,Func) Func = (Type)wglGetProcAddress(#Func);
			ENUM_GL_ENTRYPOINTS(GET_GL_ENTRYPOINTS);

			// Check that all of the entry points have been initialized.
			bool bFoundAllEntryPoints = true;
			#define CHECK_GL_ENTRYPOINTS(Type,Func) if (Func == NULL) { bFoundAllEntryPoints = false; UE_LOG(LogOpenGLShaderCompiler, Warning, TEXT("Failed to find entry point for %s"), TEXT(#Func)); }
			ENUM_GL_ENTRYPOINTS(CHECK_GL_ENTRYPOINTS);
			checkf(bFoundAllEntryPoints, TEXT("Failed to find all OpenGL entry points."));
		}

		// Restore warning C4191.
		#pragma warning(pop)
	}
	PlatformMakeGLContextCurrent(&ShaderCompileContext);
}
static void PlatformReleaseOpenGL(void* ContextPtr, void* PrevContextPtr)
{
	wglMakeCurrent((HDC)ContextPtr, (HGLRC)PrevContextPtr);
}
#elif PLATFORM_LINUX
static void _PlatformCreateDummyGLWindow(FPlatformOpenGLContext *OutContext)
{
	static bool bInitializedWindowClass = false;

	// Create a dummy window.
	OutContext->hWnd = SDL_CreateWindow(NULL,
		0, 0, 1, 1,
		SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN | SDL_WINDOW_SKIP_TASKBAR );
}

static void _PlatformCreateOpenGLContextCore(FPlatformOpenGLContext* OutContext)
{
	check(OutContext);
	SDL_HWindow PrevWindow = SDL_GL_GetCurrentWindow();
	SDL_HGLContext PrevContext = SDL_GL_GetCurrentContext();

	OutContext->hGLContext = SDL_GL_CreateContext(OutContext->hWnd);
	SDL_GL_MakeCurrent(PrevWindow, PrevContext);
}

static void _ContextMakeCurrent(SDL_HWindow hWnd, SDL_HGLContext hGLDC)
{
	GLint Result = SDL_GL_MakeCurrent( hWnd, hGLDC );
	check(!Result);
}

static void PlatformInitOpenGL(void*& ContextPtr, void*& PrevContextPtr, int InMajorVersion, int InMinorVersion)
{
	static bool bInitialized = (SDL_GL_GetCurrentWindow() != NULL) && (SDL_GL_GetCurrentContext() != NULL);

	if (!bInitialized)
	{
		check(InMajorVersion > 3 || (InMajorVersion == 3 && InMinorVersion >= 2));
		if (SDL_WasInit(0) == 0)
		{
			SDL_Init(SDL_INIT_VIDEO);
		}
		else
		{
			Uint32 InitializedSubsystemsMask = SDL_WasInit(SDL_INIT_EVERYTHING);
			if ((InitializedSubsystemsMask & SDL_INIT_VIDEO) == 0)
			{
				SDL_InitSubSystem(SDL_INIT_VIDEO);
			}
		}

		if (SDL_GL_LoadLibrary(NULL))
		{
			UE_LOG(LogOpenGLShaderCompiler, Fatal, TEXT("Unable to dynamically load libGL: %s"), ANSI_TO_TCHAR(SDL_GetError()));
		}

		if	(SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, InMajorVersion))
		{
			UE_LOG(LogOpenGLShaderCompiler, Fatal, TEXT("Failed to set GL major version: %s"), ANSI_TO_TCHAR(SDL_GetError()));
		}

		if	(SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, InMinorVersion))
		{
			UE_LOG(LogOpenGLShaderCompiler, Fatal, TEXT("Failed to set GL minor version: %s"), ANSI_TO_TCHAR(SDL_GetError()));
		}

		if	(SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG))
		{
			UE_LOG(LogOpenGLShaderCompiler, Fatal, TEXT("Failed to set GL flags: %s"), ANSI_TO_TCHAR(SDL_GetError()));
		}

		if	(SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE))
		{
			UE_LOG(LogOpenGLShaderCompiler, Fatal, TEXT("Failed to set GL mask/profile: %s"), ANSI_TO_TCHAR(SDL_GetError()));
		}

		// Create a dummy context to verify opengl support.
		FPlatformOpenGLContext DummyContext;
		_PlatformCreateDummyGLWindow(&DummyContext);
		_PlatformCreateOpenGLContextCore(&DummyContext);

		if (DummyContext.hGLContext)
		{
			_ContextMakeCurrent(DummyContext.hWnd, DummyContext.hGLContext);
		}
		else
		{
			UE_LOG(LogOpenGLShaderCompiler, Fatal, TEXT("OpenGL %d.%d not supported by driver"), InMajorVersion, InMinorVersion);
			return;
		}

		PrevContextPtr = NULL;
		ContextPtr = DummyContext.hGLContext;
		bInitialized = true;
	}

	PrevContextPtr = reinterpret_cast<void*>(SDL_GL_GetCurrentContext());
	SDL_HGLContext NewContext = SDL_GL_CreateContext(SDL_GL_GetCurrentWindow());
	SDL_GL_MakeCurrent(SDL_GL_GetCurrentWindow(), NewContext);
	ContextPtr = reinterpret_cast<void*>(NewContext);
}

static void PlatformReleaseOpenGL(void* ContextPtr, void* PrevContextPtr)
{
	SDL_GL_MakeCurrent(SDL_GL_GetCurrentWindow(), reinterpret_cast<SDL_HGLContext>(PrevContextPtr));
	SDL_GL_DeleteContext(reinterpret_cast<SDL_HGLContext>(ContextPtr));
}
#elif PLATFORM_MAC
static void PlatformInitOpenGL(void*& ContextPtr, void*& PrevContextPtr, int InMajorVersion, int InMinorVersion)
{
	check(InMajorVersion > 3 || (InMajorVersion == 3 && InMinorVersion >= 2));

	CGLPixelFormatAttribute AttribList[] =
	{
		kCGLPFANoRecovery,
		kCGLPFAAccelerated,
		kCGLPFAOpenGLProfile,
		(CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core,
		(CGLPixelFormatAttribute)0
	};

	CGLPixelFormatObj PixelFormat;
	GLint NumFormats = 0;
	CGLError Error = CGLChoosePixelFormat(AttribList, &PixelFormat, &NumFormats);
	check(Error == kCGLNoError);

	CGLContextObj ShaderCompileContext;
	Error = CGLCreateContext(PixelFormat, NULL, &ShaderCompileContext);
	check(Error == kCGLNoError);

	Error = CGLDestroyPixelFormat(PixelFormat);
	check(Error == kCGLNoError);

	PrevContextPtr = (void*)CGLGetCurrentContext();

	Error = CGLSetCurrentContext(ShaderCompileContext);
	check(Error == kCGLNoError);

	ContextPtr = (void*)ShaderCompileContext;
}
static void PlatformReleaseOpenGL(void* ContextPtr, void* PrevContextPtr)
{
	CGLContextObj ShaderCompileContext = (CGLContextObj)ContextPtr;
	CGLContextObj PreviousShaderCompileContext = (CGLContextObj)PrevContextPtr;
	CGLError Error;

	Error = CGLSetCurrentContext(PreviousShaderCompileContext);
	check(Error == kCGLNoError);

	Error = CGLDestroyContext(ShaderCompileContext);
	check(Error == kCGLNoError);
}
#endif

/** Map shader frequency -> GL shader type. */
GLenum GLFrequencyTable[] =
{
	GL_VERTEX_SHADER,	// SF_Vertex
	GL_TESS_CONTROL_SHADER,	 // SF_Hull
	GL_TESS_EVALUATION_SHADER, // SF_Domain
	GL_FRAGMENT_SHADER, // SF_Pixel
	GL_GEOMETRY_SHADER,	// SF_Geometry
	GL_COMPUTE_SHADER,  // SF_Compute

};

static_assert(ARRAY_COUNT(GLFrequencyTable) == SF_NumFrequencies, "Frequency table size mismatch.");

static inline bool IsDigit(TCHAR Char)
{
	return Char >= '0' && Char <= '9';
}

/**
 * Parse a GLSL error.
 * @param OutErrors - Storage for shader compiler errors.
 * @param InLine - A single line from the compile error log.
 */
void ParseGlslError(TArray<FShaderCompilerError>& OutErrors, const FString& InLine)
{
	const TCHAR* ErrorPrefix = TEXT("error: 0:");
	const TCHAR* p = *InLine;
	if (FCString::Strnicmp(p, ErrorPrefix, 9) == 0)
	{
		FString ErrorMsg;
		int32 LineNumber = 0;
		p += FCString::Strlen(ErrorPrefix);

		// Skip to a number, take that to be the line number.
		while (*p && !IsDigit(*p)) { p++; }
		while (*p && IsDigit(*p))
		{
			LineNumber = 10 * LineNumber + (*p++ - TEXT('0'));
		}

		// Skip to the next alphanumeric value, treat that as the error message.
		while (*p && !FChar::IsAlnum(*p)) { p++; }
		ErrorMsg = p;

		// Generate a compiler error.
		if (ErrorMsg.Len() > 0)
		{
			// Note that no mapping exists from the GLSL source to the original
			// HLSL source.
			FShaderCompilerError* CompilerError = new(OutErrors) FShaderCompilerError;
			CompilerError->StrippedErrorMessage = FString::Printf(
				TEXT("driver compile error(%d): %s"),
				LineNumber,
				*ErrorMsg
				);
		}
	}
}

static TArray<ANSICHAR> ParseIdentifierANSI(const FString& Str)
{
	TArray<ANSICHAR> Result;
	Result.Reserve(Str.Len());
	for (int32 Index = 0; Index < Str.Len(); ++Index)
	{
		Result.Add(FChar::ToLower((ANSICHAR)Str[Index]));
	}
	Result.Add('\0');

	return Result;
}

static uint32 ParseNumber(const TCHAR* Str)
{
	uint32 Num = 0;
	while (*Str && IsDigit(*Str))
	{
		Num = Num * 10 + *Str++ - '0';
	}
	return Num;
}


static ANSICHAR TranslateFrequencyToCrossCompilerPrefix(int32 Frequency)
{
	switch (Frequency)
	{
		case SF_Vertex: return 'v';
		case SF_Pixel: return 'p';
		case SF_Hull: return 'h';
		case SF_Domain: return 'd';
		case SF_Geometry: return 'g';
		case SF_Compute: return 'c';
	}
	return '\0';
}

static TCHAR* SetIndex(TCHAR* Str, int32 Offset, int32 Index)
{
	check(Index >= 0 && Index < 100);

	Str += Offset;
	if (Index >= 10)
	{
		*Str++ = '0' + (TCHAR)(Index / 10);
	}
	*Str++ = '0' + (TCHAR)(Index % 10);
	*Str = '\0';
	return Str;
}



/**
 * Construct the final microcode from the compiled and verified shader source.
 * @param ShaderOutput - Where to store the microcode and parameter map.
 * @param InShaderSource - GLSL source with input/output signature.
 * @param SourceLen - The length of the GLSL source code.
 */
void FOpenGLFrontend::BuildShaderOutput(
	FShaderCompilerOutput& ShaderOutput,
	const FShaderCompilerInput& ShaderInput,
	const ANSICHAR* InShaderSource,
	int32 SourceLen,
	GLSLVersion Version
	)
{
	const ANSICHAR* USFSource = InShaderSource;
	CrossCompiler::FHlslccHeader CCHeader;
	if (!CCHeader.Read(USFSource, SourceLen))
	{
		UE_LOG(LogOpenGLShaderCompiler, Error, TEXT("Bad hlslcc header found"));
	}

	if (*USFSource != '#')
	{
		UE_LOG(LogOpenGLShaderCompiler, Error, TEXT("Bad hlslcc header found! Missing '#'!"));
	}

	FOpenGLCodeHeader Header = {0};
	FShaderParameterMap& ParameterMap = ShaderOutput.ParameterMap;
	EShaderFrequency Frequency = (EShaderFrequency)ShaderOutput.Target.Frequency;

	TBitArray<> UsedUniformBufferSlots;
	UsedUniformBufferSlots.Init(false, 32);

	// Write out the magic markers.
	Header.GlslMarker = 0x474c534c;
	switch (Frequency)
	{
	case SF_Vertex:
		Header.FrequencyMarker = 0x5653;
		break;
	case SF_Pixel:
		Header.FrequencyMarker = 0x5053;
		break;
	case SF_Geometry:
		Header.FrequencyMarker = 0x4753;
		break;
	case SF_Hull:
		Header.FrequencyMarker = 0x4853;
		break;
	case SF_Domain:
		Header.FrequencyMarker = 0x4453;
		break;
	case SF_Compute:
		Header.FrequencyMarker = 0x4353;
		break;
	default:
		UE_LOG(LogOpenGLShaderCompiler, Fatal, TEXT("Invalid shader frequency: %d"), (int32)Frequency);
	}

	static const FString AttributePrefix = TEXT("in_ATTRIBUTE");
	static const FString GL_Prefix = TEXT("gl_");
	for (auto& Input : CCHeader.Inputs)
	{
		// Only process attributes for vertex shaders.
		if (Frequency == SF_Vertex && Input.Name.StartsWith(AttributePrefix))
		{
			int32 AttributeIndex = ParseNumber(*Input.Name + AttributePrefix.Len());
			Header.Bindings.InOutMask |= (1 << AttributeIndex);
		}
		// Record user-defined input varyings
		else if (!Input.Name.StartsWith(GL_Prefix))
		{
			FOpenGLShaderVarying Var;
			Var.Location = Input.Index;
			Var.Varying = ParseIdentifierANSI(Input.Name);
			Header.Bindings.InputVaryings.Add(Var);
		}
	}

	// Generate vertex attribute remapping table.
	// This is used on devices where GL_MAX_VERTEX_ATTRIBS < 16
	if (Frequency == SF_Vertex)
	{
		uint32 AttributeMask = Header.Bindings.InOutMask;
		int32 NextAttributeSlot = 0;
		Header.Bindings.VertexRemappedMask = 0;
		for (int32 AttributeIndex = 0; AttributeIndex < 16; AttributeIndex++, AttributeMask >>= 1)
		{
			if (AttributeMask & 0x1)
			{
				Header.Bindings.VertexRemappedMask |= (1 << NextAttributeSlot);
				Header.Bindings.VertexAttributeRemap[AttributeIndex] = NextAttributeSlot++;
			}
			else
			{
				Header.Bindings.VertexAttributeRemap[AttributeIndex] = -1;
			}
		}
	}

	static const FString TargetPrefix = "out_Target";
	static const FString GL_FragDepth = "gl_FragDepth";
	for (auto& Output : CCHeader.Outputs)
	{
		// Only targets for pixel shaders must be tracked.
		if (Frequency == SF_Pixel && Output.Name.StartsWith(TargetPrefix))
		{
			uint8 TargetIndex = ParseNumber(*Output.Name + TargetPrefix.Len());
			Header.Bindings.InOutMask |= (1 << TargetIndex);
		}
		// Only depth writes for pixel shaders must be tracked.
		else if (Frequency == SF_Pixel && Output.Name.Equals(GL_FragDepth))
		{
			Header.Bindings.InOutMask |= 0x8000;
		}
		// Record user-defined output varyings
		else if (!Output.Name.StartsWith(GL_Prefix))
		{
			FOpenGLShaderVarying Var;
			Var.Location = Output.Index;
			Var.Varying = ParseIdentifierANSI(Output.Name);
			Header.Bindings.OutputVaryings.Add(Var);
		}
	}

	// general purpose binding name
	TCHAR BindingName[] = TEXT("XYZ\0\0\0\0\0\0\0\0");
	BindingName[0] = TranslateFrequencyToCrossCompilerPrefix(Frequency);

	TMap<FString, FString> BindingNameMap;

	// Then 'normal' uniform buffers.
	for (auto& UniformBlock : CCHeader.UniformBlocks)
	{
		uint16 UBIndex = UniformBlock.Index;
		check(UBIndex == Header.Bindings.NumUniformBuffers);
		UsedUniformBufferSlots[UBIndex] = true;
		if (OutputTrueParameterNames())
		{
			// make the final name this will be in the shader
			BindingName[1] = 'b';
			SetIndex(BindingName, 2, UBIndex);
			BindingNameMap.Add(BindingName, UniformBlock.Name);
		}
		else
		{
			ParameterMap.AddParameterAllocation(*UniformBlock.Name, Header.Bindings.NumUniformBuffers, 0, 0);
		}
		Header.Bindings.NumUniformBuffers++;
	}

	const uint16 BytesPerComponent = 4;

	// Packed global uniforms
	TMap<ANSICHAR, uint16> PackedGlobalArraySize;
	for (auto& PackedGlobal : CCHeader.PackedGlobals)
	{
		ParameterMap.AddParameterAllocation(
			*PackedGlobal.Name,
			PackedGlobal.PackedType,
			PackedGlobal.Offset * BytesPerComponent,
			PackedGlobal.Count * BytesPerComponent
			);

		uint16& Size = PackedGlobalArraySize.FindOrAdd(PackedGlobal.PackedType);
		Size = FMath::Max<uint16>(BytesPerComponent * (PackedGlobal.Offset + PackedGlobal.Count), Size);
	}

	// Packed Uniform Buffers
	TMap<int, TMap<ANSICHAR, uint16> > PackedUniformBuffersSize;
	for (auto& PackedUB : CCHeader.PackedUBs)
	{
		checkf(OutputTrueParameterNames() == false, TEXT("Unexpected Packed UBs used with a shader format that needs true parameter names - If this is hit, we need to figure out how to handle them"));

		check(PackedUB.Attribute.Index == Header.Bindings.NumUniformBuffers);
		UsedUniformBufferSlots[PackedUB.Attribute.Index] = true;
		if (OutputTrueParameterNames())
		{
			BindingName[1] = 'b';
			// ???
		}
		else
		{
			ParameterMap.AddParameterAllocation(*PackedUB.Attribute.Name, Header.Bindings.NumUniformBuffers, 0, 0);
		}
		Header.Bindings.NumUniformBuffers++;

		// Nothing else...
		//for (auto& Member : PackedUB.Members)
		//{
		//}
	}

	// Packed Uniform Buffers copy lists & setup sizes for each UB/Precision entry
	enum EFlattenUBState
	{
		Unknown,
		GroupedUBs,
		FlattenedUBs,
	};
	EFlattenUBState UBState = Unknown;
	for (auto& PackedUBCopy : CCHeader.PackedUBCopies)
	{
		CrossCompiler::FUniformBufferCopyInfo CopyInfo;
		CopyInfo.SourceUBIndex = PackedUBCopy.SourceUB;
		CopyInfo.SourceOffsetInFloats = PackedUBCopy.SourceOffset;
		CopyInfo.DestUBIndex = PackedUBCopy.DestUB;
		CopyInfo.DestUBTypeName = PackedUBCopy.DestPackedType;
		CopyInfo.DestUBTypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(CopyInfo.DestUBTypeName);
		CopyInfo.DestOffsetInFloats = PackedUBCopy.DestOffset;
		CopyInfo.SizeInFloats = PackedUBCopy.Count;

		Header.UniformBuffersCopyInfo.Add(CopyInfo);

		auto& UniformBufferSize = PackedUniformBuffersSize.FindOrAdd(CopyInfo.DestUBIndex);
		uint16& Size = UniformBufferSize.FindOrAdd(CopyInfo.DestUBTypeName);
		Size = FMath::Max<uint16>(BytesPerComponent * (CopyInfo.DestOffsetInFloats + CopyInfo.SizeInFloats), Size);

		check(UBState == Unknown || UBState == GroupedUBs);
		UBState = GroupedUBs;
	}

	for (auto& PackedUBCopy : CCHeader.PackedUBGlobalCopies)
	{
		CrossCompiler::FUniformBufferCopyInfo CopyInfo;
		CopyInfo.SourceUBIndex = PackedUBCopy.SourceUB;
		CopyInfo.SourceOffsetInFloats = PackedUBCopy.SourceOffset;
		CopyInfo.DestUBIndex = PackedUBCopy.DestUB;
		CopyInfo.DestUBTypeName = PackedUBCopy.DestPackedType;
		CopyInfo.DestUBTypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(CopyInfo.DestUBTypeName);
		CopyInfo.DestOffsetInFloats = PackedUBCopy.DestOffset;
		CopyInfo.SizeInFloats = PackedUBCopy.Count;

		Header.UniformBuffersCopyInfo.Add(CopyInfo);

		uint16& Size = PackedGlobalArraySize.FindOrAdd(CopyInfo.DestUBTypeName);
		Size = FMath::Max<uint16>(BytesPerComponent * (CopyInfo.DestOffsetInFloats + CopyInfo.SizeInFloats), Size);

		check(UBState == Unknown || UBState == FlattenedUBs);
		UBState = FlattenedUBs;
	}

	Header.Bindings.bFlattenUB = (UBState == FlattenedUBs);

	// Setup Packed Array info
	Header.Bindings.PackedGlobalArrays.Reserve(PackedGlobalArraySize.Num());
	for (auto Iterator = PackedGlobalArraySize.CreateIterator(); Iterator; ++Iterator)
	{
		ANSICHAR TypeName = Iterator.Key();
		uint16 Size = Iterator.Value();
		Size = (Size + 0xf) & (~0xf);
		CrossCompiler::FPackedArrayInfo Info;
		Info.Size = Size;
		Info.TypeName = TypeName;
		Info.TypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(TypeName);
		Header.Bindings.PackedGlobalArrays.Add(Info);
	}

	// Setup Packed Uniform Buffers info
	Header.Bindings.PackedUniformBuffers.Reserve(PackedUniformBuffersSize.Num());
	for (auto Iterator = PackedUniformBuffersSize.CreateIterator(); Iterator; ++Iterator)
	{
		int BufferIndex = Iterator.Key();
		auto& ArraySizes = Iterator.Value();
		TArray<CrossCompiler::FPackedArrayInfo> InfoArray;
		InfoArray.Reserve(ArraySizes.Num());
		for (auto IterSizes = ArraySizes.CreateIterator(); IterSizes; ++IterSizes)
		{
			ANSICHAR TypeName = IterSizes.Key();
			uint16 Size = IterSizes.Value();
			Size = (Size + 0xf) & (~0xf);
			CrossCompiler::FPackedArrayInfo Info;
			Info.Size = Size;
			Info.TypeName = TypeName;
			Info.TypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(TypeName);
			InfoArray.Add(Info);
		}

		Header.Bindings.PackedUniformBuffers.Add(InfoArray);
	}

	// Then samplers.
	for (auto& Sampler : CCHeader.Samplers)
	{
		if (OutputTrueParameterNames())
		{
			BindingName[1] = 's';
			SetIndex(BindingName, 2, Sampler.Offset);
			BindingNameMap.Add(BindingName, Sampler.Name);
		}
		else
		{
		ParameterMap.AddParameterAllocation(
			*Sampler.Name,
			0,
			Sampler.Offset,
			Sampler.Count
			);
		}

		Header.Bindings.NumSamplers = FMath::Max<uint8>(
			Header.Bindings.NumSamplers,
			Sampler.Offset + Sampler.Count
			);

		for (auto& SamplerState : Sampler.SamplerStates)
		{
			if (OutputTrueParameterNames())
			{
				// add an entry for the sampler parameter as well
				BindingNameMap.Add(FString(BindingName) + TEXT("_samp"), SamplerState);
			}
			else
			{
				ParameterMap.AddParameterAllocation(
					*SamplerState,
					0,
					Sampler.Offset,
					Sampler.Count
					);
			}
		}
	}

	// Then UAVs (images in GLSL)
	for (auto& UAV : CCHeader.UAVs)
	{
		if (OutputTrueParameterNames())
		{
			// make the final name this will be in the shader
			BindingName[1] = 'i';
			SetIndex(BindingName, 2, UAV.Offset);
			BindingNameMap.Add(BindingName, UAV.Name);
		}
		else
		{
		ParameterMap.AddParameterAllocation(
			*UAV.Name,
			0,
			UAV.Offset,
			UAV.Count
			);
		}

		Header.Bindings.NumUAVs = FMath::Max<uint8>(
			Header.Bindings.NumSamplers,
			UAV.Offset + UAV.Count
			);
	}

	Header.ShaderName = CCHeader.Name;

	// perform any post processing this frontend class may need to do
	ShaderOutput.bSucceeded = PostProcessShaderSource(Version, Frequency, USFSource, SourceLen + 1 - (USFSource - InShaderSource), ParameterMap, BindingNameMap, ShaderOutput.Errors, ShaderInput);

	// Build the SRT for this shader.
	{
		// Build the generic SRT for this shader.
		FShaderCompilerResourceTable GenericSRT;
		BuildResourceTableMapping(ShaderInput.Environment.ResourceTableMap, ShaderInput.Environment.ResourceTableLayoutHashes, UsedUniformBufferSlots, ShaderOutput.ParameterMap, GenericSRT);

		// Copy over the bits indicating which resource tables are active.
		Header.Bindings.ShaderResourceTable.ResourceTableBits = GenericSRT.ResourceTableBits;

		Header.Bindings.ShaderResourceTable.ResourceTableLayoutHashes = GenericSRT.ResourceTableLayoutHashes;

		// Now build our token streams.
		BuildResourceTableTokenStream(GenericSRT.TextureMap, GenericSRT.MaxBoundResourceTable, Header.Bindings.ShaderResourceTable.TextureMap);
		BuildResourceTableTokenStream(GenericSRT.ShaderResourceViewMap, GenericSRT.MaxBoundResourceTable, Header.Bindings.ShaderResourceTable.ShaderResourceViewMap);
		BuildResourceTableTokenStream(GenericSRT.SamplerMap, GenericSRT.MaxBoundResourceTable, Header.Bindings.ShaderResourceTable.SamplerMap);
		BuildResourceTableTokenStream(GenericSRT.UnorderedAccessViewMap, GenericSRT.MaxBoundResourceTable, Header.Bindings.ShaderResourceTable.UnorderedAccessViewMap);
	}

	const int32 MaxSamplers = GetMaxSamplers(Version);

	if (Header.Bindings.NumSamplers > MaxSamplers)
	{
		ShaderOutput.bSucceeded = false;
		FShaderCompilerError* NewError = new(ShaderOutput.Errors) FShaderCompilerError();
		NewError->StrippedErrorMessage =
			FString::Printf(TEXT("shader uses %d samplers exceeding the limit of %d"),
				Header.Bindings.NumSamplers, MaxSamplers);
	}
	else if (ShaderOutput.bSucceeded)
	{
		// Write out the header
		FMemoryWriter Ar(ShaderOutput.ShaderCode.GetWriteAccess(), true);
		Ar << Header;

		if (OptionalSerializeOutputAndReturnIfSerialized(Ar) == false)
		{
		Ar.Serialize((void*)USFSource, SourceLen + 1 - (USFSource - InShaderSource));
			ShaderOutput.bSucceeded = true;
		}

		// store data we can pickup later with ShaderCode.FindOptionalData('n'), could be removed for shipping
		// Daniel L: This GenerateShaderName does not generate a deterministic output among shaders as the shader code can be shared.
		//			uncommenting this will cause the project to have non deterministic materials and will hurt patch sizes
		//ShaderOutput.ShaderCode.AddOptionalData('n', TCHAR_TO_UTF8(*ShaderInput.GenerateShaderName()));

		ShaderOutput.NumInstructions = 0;
		ShaderOutput.NumTextureSamplers = Header.Bindings.NumSamplers;
	}
}

void FOpenGLFrontend::ConvertOpenGLVersionFromGLSLVersion(GLSLVersion InVersion, int& OutMajorVersion, int& OutMinorVersion)
{
	switch(InVersion)
	{
		case GLSL_150:
			OutMajorVersion = 3;
			OutMinorVersion = 2;
			break;
		case GLSL_310_ES_EXT:
		case GLSL_430:
			OutMajorVersion = 4;
			OutMinorVersion = 3;
			break;
		case GLSL_150_ES2:
		case GLSL_150_ES2_NOUB:
		case GLSL_150_ES3_1:
			OutMajorVersion = 3;
			OutMinorVersion = 2;
			break;
		case GLSL_ES2_IOS:
		case GLSL_ES2_WEBGL:
		case GLSL_ES2:
		case GLSL_ES3_1_ANDROID:
			OutMajorVersion = 0;
			OutMinorVersion = 0;
			break;
		default:
			// Invalid enum
			check(0);
			OutMajorVersion = 0;
			OutMinorVersion = 0;
			break;
	}
}

static const TCHAR* GetGLSLES2CompilerExecutable(bool bNDACompiler)
{
	// Unfortunately no env var is set to handle install path
	return (bNDACompiler
		? TEXT("C:\\Imagination\\PowerVR\\GraphicsSDK\\Compilers\\OGLES\\Windows_x86_32\\glslcompiler_sgx543_nda.exe")
		: TEXT("C:\\Imagination\\PowerVR\\GraphicsSDK\\Compilers\\OGLES\\Windows_x86_32\\glslcompiler_sgx543.exe"));
}

static FString CreateGLSLES2CompilerArguments(const FString& ShaderFile, const FString& OutputFile, EHlslShaderFrequency Frequency, bool bNDACompiler)
{
	const TCHAR* FrequencySwitch = TEXT("");
	switch (Frequency)
	{
	case HSF_PixelShader:
		FrequencySwitch = TEXT(" -f");
		break;

	case HSF_VertexShader:
		FrequencySwitch = TEXT(" -v");
		break;

	default:
		return TEXT("");
	}

	FString Arguments = FString::Printf(TEXT("%s %s %s -profile -perfsim"), *FPaths::GetCleanFilename(ShaderFile), *FPaths::GetCleanFilename(OutputFile), FrequencySwitch);

	if (bNDACompiler)
	{
		Arguments += " -disasm";
	}

	return Arguments;
}

static FString CreateCommandLineGLSLES2(const FString& ShaderFile, const FString& OutputFile, GLSLVersion Version, EHlslShaderFrequency Frequency, bool bNDACompiler)
{
	if (Version != GLSL_ES2 && Version != GLSL_ES2_WEBGL && Version != GLSL_ES2_IOS)
	{
		return TEXT("");
	}

	FString CmdLine = FString(GetGLSLES2CompilerExecutable(bNDACompiler)) + TEXT(" ") + CreateGLSLES2CompilerArguments(ShaderFile, OutputFile, Frequency, bNDACompiler);
	CmdLine += FString(LINE_TERMINATOR) + TEXT("pause");
	return CmdLine;
}

/** Precompile a glsl shader for ES2. */
void FOpenGLFrontend::PrecompileGLSLES2(FShaderCompilerOutput& ShaderOutput, const FShaderCompilerInput& ShaderInput, const ANSICHAR* ShaderSource, EHlslShaderFrequency Frequency)
{
	const TCHAR* CompilerExecutableName = GetGLSLES2CompilerExecutable(false);
	const int32 SourceLen = FCStringAnsi::Strlen(ShaderSource);
	const bool bCompilerExecutableExists = FPaths::FileExists(CompilerExecutableName);

	// Using the debug info path to write out the files to disk for the PVR shader compiler
	if (ShaderInput.DumpDebugInfoPath != TEXT("") && bCompilerExecutableExists)
	{
		const FString GLSLSourceFile = (ShaderInput.DumpDebugInfoPath / TEXT("GLSLSource.txt"));
		bool bSavedSuccessfully = false;

		{
			FArchive* Ar = IFileManager::Get().CreateFileWriter(*GLSLSourceFile, FILEWRITE_EvenIfReadOnly);

			// Save the ansi file to disk so it can be used as input to the PVR shader compiler
			if (Ar)
			{
				bSavedSuccessfully = true;

				// @todo: Patch the code so that textureCubeLodEXT gets converted to textureCubeLod to workaround PowerVR issues
				const ANSICHAR* VersionString = FCStringAnsi::Strifind(ShaderSource, "#version 100");
				check(VersionString);
				VersionString += 12;	// strlen("# version 100");
				Ar->Serialize((void*)ShaderSource, (VersionString - ShaderSource) * sizeof(ANSICHAR));
				const char* PVRWorkaround = "\n#ifndef textureCubeLodEXT\n#define textureCubeLodEXT textureCubeLod\n#endif\n";
				Ar->Serialize((void*)PVRWorkaround, FCStringAnsi::Strlen(PVRWorkaround));
				Ar->Serialize((void*)VersionString, (SourceLen - (VersionString - ShaderSource)) * sizeof(ANSICHAR));
				delete Ar;
			}
		}

		if (bSavedSuccessfully && ENABLE_IMAGINATION_COMPILER)
		{
			const FString Arguments = CreateGLSLES2CompilerArguments(GLSLSourceFile, TEXT("ASM.txt"), Frequency, false);

			FString StdOut;
			FString StdErr;
			int32 ReturnCode = 0;

			// Run the PowerVR shader compiler and wait for completion
			FPlatformProcess::ExecProcess(GetGLSLES2CompilerExecutable(false), *Arguments, &ReturnCode, &StdOut, &StdErr);

			if (ReturnCode >= 0)
			{
				ShaderOutput.bSucceeded = true;
				ShaderOutput.Target = ShaderInput.Target;

				BuildShaderOutput(ShaderOutput, ShaderInput, ShaderSource, SourceLen, GLSL_ES2);

				// Parse the cycle count
				const int32 CycleCountStringLength = FPlatformString::Strlen(TEXT("Cycle count: "));
				const int32 CycleCountIndex = StdOut.Find(TEXT("Cycle count: "));

				if (CycleCountIndex != INDEX_NONE && CycleCountIndex + CycleCountStringLength < StdOut.Len())
				{
					const int32 CycleCountEndIndex = StdOut.Find(TEXT("\n"), ESearchCase::IgnoreCase, ESearchDir::FromStart, CycleCountIndex + CycleCountStringLength);

					if (CycleCountEndIndex != INDEX_NONE)
					{
						const FString InstructionSubstring = StdOut.Mid(CycleCountIndex + CycleCountStringLength, CycleCountEndIndex - (CycleCountIndex + CycleCountStringLength));
						ShaderOutput.NumInstructions = FCString::Atoi(*InstructionSubstring);
					}
				}
			}
			else
			{
				ShaderOutput.bSucceeded = false;

				FShaderCompilerError* NewError = new(ShaderOutput.Errors) FShaderCompilerError();
				// Print the name of the generated glsl file so we can open it with a double click in the VS.Net output window
				NewError->StrippedErrorMessage = FString::Printf(TEXT("%s \nPVR SDK glsl compiler for SGX543: %s"), *GLSLSourceFile, *StdOut);
			}
		}
		else
		{
			ShaderOutput.bSucceeded = true;
			ShaderOutput.Target = ShaderInput.Target;

			BuildShaderOutput(ShaderOutput, ShaderInput, ShaderSource, SourceLen, GLSL_ES2);
		}
	}
	else
	{
		ShaderOutput.bSucceeded = true;
		ShaderOutput.Target = ShaderInput.Target;

		BuildShaderOutput(ShaderOutput, ShaderInput, ShaderSource, SourceLen, GLSL_ES2);
	}
}

/**
 * Precompile a GLSL shader.
 * @param ShaderOutput - The precompiled shader.
 * @param ShaderInput - The shader input.
 * @param InPreprocessedShader - The preprocessed source code.
 */
void FOpenGLFrontend::PrecompileShader(FShaderCompilerOutput& ShaderOutput, const FShaderCompilerInput& ShaderInput, const ANSICHAR* ShaderSource, GLSLVersion Version, EHlslShaderFrequency Frequency)
{
	check(ShaderInput.Target.Frequency < SF_NumFrequencies);

	// Lookup the GL shader type.
	GLenum GLFrequency = GLFrequencyTable[ShaderInput.Target.Frequency];
	if (GLFrequency == GL_NONE)
	{
		ShaderOutput.bSucceeded = false;
		FShaderCompilerError* NewError = new(ShaderOutput.Errors) FShaderCompilerError();
		NewError->StrippedErrorMessage = FString::Printf(TEXT("%s shaders not supported for use in OpenGL."), CrossCompiler::GetFrequencyName((EShaderFrequency)ShaderInput.Target.Frequency));
		return;
	}

	if (Version == GLSL_ES2 || Version == GLSL_ES2_WEBGL || Version == GLSL_ES2_IOS)
	{
		PrecompileGLSLES2(ShaderOutput, ShaderInput, ShaderSource, Frequency);
	}
	else
	{
		// Create the shader with the preprocessed source code.
		void* ContextPtr;
		void* PrevContextPtr;
		int MajorVersion = 0;
		int MinorVersion = 0;
		ConvertOpenGLVersionFromGLSLVersion(Version, MajorVersion, MinorVersion);
		PlatformInitOpenGL(ContextPtr, PrevContextPtr, MajorVersion, MinorVersion);

		GLint SourceLen = FCStringAnsi::Strlen(ShaderSource);
		GLuint Shader = glCreateShader(GLFrequency);
		{
			const GLchar* SourcePtr = ShaderSource;
			glShaderSource(Shader, 1, &SourcePtr, &SourceLen);
		}

		// Compile and get results.
		glCompileShader(Shader);
		{
			GLint CompileStatus;
			glGetShaderiv(Shader, GL_COMPILE_STATUS, &CompileStatus);
			if (CompileStatus == GL_TRUE)
			{
				ShaderOutput.Target = ShaderInput.Target;
				BuildShaderOutput(
					ShaderOutput,
					ShaderInput,
					ShaderSource,
					(int32)SourceLen,
					Version
					);
			}
			else
			{
				GLint LogLength;
				glGetShaderiv(Shader, GL_INFO_LOG_LENGTH, &LogLength);
				if (LogLength > 1)
				{
					TArray<ANSICHAR> RawCompileLog;
					FString CompileLog;
					TArray<FString> LogLines;

					RawCompileLog.Empty(LogLength);
					RawCompileLog.AddZeroed(LogLength);
					glGetShaderInfoLog(Shader, LogLength, /*OutLength=*/ NULL, RawCompileLog.GetData());
					CompileLog = ANSI_TO_TCHAR(RawCompileLog.GetData());
					CompileLog.ParseIntoArray(LogLines, TEXT("\n"), true);

					for (int32 Line = 0; Line < LogLines.Num(); ++Line)
					{
						ParseGlslError(ShaderOutput.Errors, LogLines[Line]);
					}

					if (ShaderOutput.Errors.Num() == 0)
					{
						FShaderCompilerError* NewError = new(ShaderOutput.Errors) FShaderCompilerError();
						NewError->StrippedErrorMessage = FString::Printf(
							TEXT("GLSL source:\n%sGL compile log: %s\n"),
							ANSI_TO_TCHAR(ShaderSource),
							ANSI_TO_TCHAR(RawCompileLog.GetData())
							);
					}
				}
				else
				{
					FShaderCompilerError* NewError = new(ShaderOutput.Errors) FShaderCompilerError();
					NewError->StrippedErrorMessage = TEXT("Shader compile failed without errors.");
				}

				ShaderOutput.bSucceeded = false;
			}
		}
		glDeleteShader(Shader);
		PlatformReleaseOpenGL(ContextPtr, PrevContextPtr);
	}
}

void FOpenGLFrontend::SetupPerVersionCompilationEnvironment(GLSLVersion Version, FShaderCompilerDefinitions& AdditionalDefines, EHlslCompileTarget& HlslCompilerTarget)
	{
	switch (Version)
	{
		case GLSL_ES3_1_ANDROID:
			AdditionalDefines.SetDefine(TEXT("COMPILER_GLSL_ES3_1"), 1);
			AdditionalDefines.SetDefine(TEXT("ES3_1_PROFILE"), 1);
			HlslCompilerTarget = HCT_FeatureLevelES3_1;
			break;

		case GLSL_310_ES_EXT:
			AdditionalDefines.SetDefine(TEXT("COMPILER_GLSL"), 1);
			AdditionalDefines.SetDefine(TEXT("COMPILER_GLSL_ES3_1_EXT"), 1);
			AdditionalDefines.SetDefine(TEXT("ESDEFERRED_PROFILE"), 1);
			AdditionalDefines.SetDefine(TEXT("GL4_PROFILE"), 1);
			HlslCompilerTarget = HCT_FeatureLevelES3_1Ext;
			break;

		case GLSL_430:
			AdditionalDefines.SetDefine(TEXT("COMPILER_GLSL"), 1);
			AdditionalDefines.SetDefine(TEXT("GL4_PROFILE"), 1);
			HlslCompilerTarget = HCT_FeatureLevelSM5;
			break;

		case GLSL_150:
			AdditionalDefines.SetDefine(TEXT("COMPILER_GLSL"), 1);
			AdditionalDefines.SetDefine(TEXT("GL3_PROFILE"), 1);
			HlslCompilerTarget = HCT_FeatureLevelSM4;
			break;

		case GLSL_ES2_WEBGL:
			AdditionalDefines.SetDefine(TEXT("WEBGL"), 1);
			AdditionalDefines.SetDefine(TEXT("COMPILER_GLSL_ES2"), 1);
			AdditionalDefines.SetDefine(TEXT("ES2_PROFILE"), 1);
			HlslCompilerTarget = HCT_FeatureLevelES2;
			AdditionalDefines.SetDefine(TEXT("row_major"), TEXT(""));
			break;

		case GLSL_ES2_IOS:
			AdditionalDefines.SetDefine(TEXT("IOS"), 1);
			AdditionalDefines.SetDefine(TEXT("COMPILER_GLSL_ES2"), 1);
			AdditionalDefines.SetDefine(TEXT("ES2_PROFILE"), 1);
			HlslCompilerTarget = HCT_FeatureLevelES2;
			AdditionalDefines.SetDefine(TEXT("row_major"), TEXT(""));
			AdditionalDefines.SetDefine(TEXT("noperspective"), TEXT(""));

			break;

		case GLSL_ES2:
			AdditionalDefines.SetDefine(TEXT("COMPILER_GLSL_ES2"), 1);
			AdditionalDefines.SetDefine(TEXT("ES2_PROFILE"), 1);
			HlslCompilerTarget = HCT_FeatureLevelES2;
			AdditionalDefines.SetDefine(TEXT("row_major"), TEXT(""));
			break;

		case GLSL_150_ES2:
		case GLSL_150_ES2_NOUB:
			AdditionalDefines.SetDefine(TEXT("COMPILER_GLSL"), 1);
			AdditionalDefines.SetDefine(TEXT("ES2_PROFILE"), 1);
			HlslCompilerTarget = HCT_FeatureLevelSM4;
			AdditionalDefines.SetDefine(TEXT("row_major"), TEXT(""));
			break;

		case GLSL_150_ES3_1:
			AdditionalDefines.SetDefine(TEXT("COMPILER_GLSL"), 1);
			AdditionalDefines.SetDefine(TEXT("ES3_1_PROFILE"), 1);
			HlslCompilerTarget = HCT_FeatureLevelSM4;
			AdditionalDefines.SetDefine(TEXT("row_major"), TEXT(""));
			break;

		default:
			check(0);
	}
}

uint32 FOpenGLFrontend::GetMaxSamplers(GLSLVersion Version)
{
	switch (Version)
	{
		// Assume that GL4.3 targets support 32 samplers as we don't currently support separate sampler objects
		case GLSL_430:
			return 32;

		// mimicing the old GetFeatureLevelMaxTextureSamplers for the rest
		case GLSL_ES2:
		case GLSL_ES2_IOS:
		case GLSL_150_ES2:
		case GLSL_150_ES2_NOUB:
			return 8;

		case GLSL_ES2_WEBGL:
			// For WebGL 1 and 2, GL_MAX_TEXTURE_IMAGE_UNITS is generally much higher than on old GLES 2 Android
			// devices, but we only know the limit at runtime. Assume a decent desktop default.
			return 32;

		default:
			return 16;
	}
}

uint32 FOpenGLFrontend::CalculateCrossCompilerFlags(GLSLVersion Version, bool bCompileES2With310, bool bUseFullPrecisionInPS)
{
	uint32  CCFlags = HLSLCC_NoPreprocess | HLSLCC_PackUniforms | HLSLCC_DX11ClipSpace;
	if (IsES2Platform(Version) && !IsPCES2Platform(Version))
	{
		CCFlags |= HLSLCC_FlattenUniformBuffers | HLSLCC_FlattenUniformBufferStructures;
		// Currently only enabled for ES2, as there are still features to implement for SM4+ (atomics, global store, UAVs, etc)
		CCFlags |= HLSLCC_ApplyCommonSubexpressionElimination;
	}

	if (bUseFullPrecisionInPS)
	{
		CCFlags |= HLSLCC_UseFullPrecisionInPS;
	}

	if (bCompileES2With310)
	{
		CCFlags |= HLSLCC_FlattenUniformBuffers | HLSLCC_FlattenUniformBufferStructures;
	}

	if (Version == GLSL_150_ES2_NOUB)
	{
		CCFlags |= HLSLCC_FlattenUniformBuffers | HLSLCC_FlattenUniformBufferStructures;
	}

	if (SupportsSeparateShaderObjects(Version))
	{
		CCFlags |= HLSLCC_SeparateShaderObjects;
	}

	return CCFlags;
}

FGlslCodeBackend* FOpenGLFrontend::CreateBackend(GLSLVersion Version, uint32 CCFlags, EHlslCompileTarget HlslCompilerTarget)
{
	return new FGlslCodeBackend(CCFlags, HlslCompilerTarget);
}

FGlslLanguageSpec* FOpenGLFrontend::CreateLanguageSpec(GLSLVersion Version)
{
#if PLATFORM_HTML5
	// For backwards compatibility when targeting WebGL 2 shaders,
	// generate GLES2/WebGL 1 style shaders but with GLES3/WebGL 2
	// constructs available.
	return new FGlslLanguageSpec(true);
#else
	return new FGlslLanguageSpec(IsES2Platform(Version) && !IsPCES2Platform(Version));
#endif
}

/**
 * Compile a shader for OpenGL on Windows.
 * @param Input - The input shader code and environment.
 * @param Output - Contains shader compilation results upon return.
 */
void FOpenGLFrontend::CompileShader(const FShaderCompilerInput& Input,FShaderCompilerOutput& Output,const FString& WorkingDirectory, GLSLVersion Version)
{
	FString PreprocessedShader;
	FShaderCompilerDefinitions AdditionalDefines;
	EHlslCompileTarget HlslCompilerTarget = HCT_InvalidTarget;
	ECompilerFlags PlatformFlowControl = CFLAG_AvoidFlowControl;

	const bool bCompileES2With310 = (Version == GLSL_ES2 && Input.Environment.CompilerFlags.Contains(CFLAG_FeatureLevelES31));
	if (bCompileES2With310)
	{
		Version = GLSL_310_ES_EXT;
	}

	// set up compiler env based on version
	SetupPerVersionCompilationEnvironment(Version, AdditionalDefines, HlslCompilerTarget);

	AdditionalDefines.SetDefine(TEXT("COMPILER_HLSLCC"), 1);

	const bool bDumpDebugInfo = (Input.DumpDebugInfoPath != TEXT("") && IFileManager::Get().DirectoryExists(*Input.DumpDebugInfoPath));

	if(Input.Environment.CompilerFlags.Contains(CFLAG_AvoidFlowControl) || PlatformFlowControl == CFLAG_AvoidFlowControl)
	{
		AdditionalDefines.SetDefine(TEXT("COMPILER_SUPPORTS_ATTRIBUTES"), (uint32)1);
	}
	else
	{
		AdditionalDefines.SetDefine(TEXT("COMPILER_SUPPORTS_ATTRIBUTES"), (uint32)0);
	}

	const bool bUseFullPrecisionInPS = Input.Environment.CompilerFlags.Contains(CFLAG_UseFullPrecisionInPS);
	if (bUseFullPrecisionInPS)
	{
		AdditionalDefines.SetDefine(TEXT("FORCE_FLOATS"), (uint32)1);
	}

	if (Input.bSkipPreprocessedCache)
	{
		if (!FFileHelper::LoadFileToString(PreprocessedShader, *Input.VirtualSourceFilePath))
		{
			return;
		}

		// Remove const as we are on debug-only mode
		CrossCompiler::CreateEnvironmentFromResourceTable(PreprocessedShader, (FShaderCompilerEnvironment&)Input.Environment);
	}
	else
	{
		if (!PreprocessShader(PreprocessedShader, Output, Input, AdditionalDefines))
		{
			// The preprocessing stage will add any relevant errors.
			return;
		}
	}

	char* GlslShaderSource = NULL;
	char* ErrorLog = NULL;

	const bool bIsSM5 = IsSM5(Version);

	const EHlslShaderFrequency FrequencyTable[] =
	{
		HSF_VertexShader,
		bIsSM5 ? HSF_HullShader : HSF_InvalidFrequency,
		bIsSM5 ? HSF_DomainShader : HSF_InvalidFrequency,
		HSF_PixelShader,
		IsES2Platform(Version) ? HSF_InvalidFrequency : HSF_GeometryShader,
		bIsSM5 ? HSF_ComputeShader : HSF_InvalidFrequency
	};

	const EHlslShaderFrequency Frequency = FrequencyTable[Input.Target.Frequency];
	if (Frequency == HSF_InvalidFrequency)
	{
		Output.bSucceeded = false;
		FShaderCompilerError* NewError = new(Output.Errors) FShaderCompilerError();
		NewError->StrippedErrorMessage = FString::Printf(
			TEXT("%s shaders not supported for use in OpenGL."),
			CrossCompiler::GetFrequencyName((EShaderFrequency)Input.Target.Frequency)
			);
		return;
	}

	// This requires removing the HLSLCC_NoPreprocess flag later on!
	if (!RemoveUniformBuffersFromSource(PreprocessedShader))
	{
		return;
	}

	// Write out the preprocessed file and a batch file to compile it if requested (DumpDebugInfoPath is valid)
	if (bDumpDebugInfo)
	{
		FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*(Input.DumpDebugInfoPath / Input.GetSourceFilename()));
		if (FileWriter)
		{
			auto AnsiSourceFile = StringCast<ANSICHAR>(*PreprocessedShader);
			FileWriter->Serialize((ANSICHAR*)AnsiSourceFile.Get(), AnsiSourceFile.Length());
			{
				FString Line = CrossCompiler::CreateResourceTableFromEnvironment(Input.Environment);
				FileWriter->Serialize(TCHAR_TO_ANSI(*Line), Line.Len());
			}
			FileWriter->Close();
			delete FileWriter;
		}

		if (Input.bGenerateDirectCompileFile)
		{
			FFileHelper::SaveStringToFile(CreateShaderCompilerWorkerDirectCommandLine(Input), *(Input.DumpDebugInfoPath / TEXT("DirectCompile.txt")));
		}
	}

	uint32 CCFlags = CalculateCrossCompilerFlags(Version, bCompileES2With310, bUseFullPrecisionInPS);

	// Required as we added the RemoveUniformBuffersFromSource() function (the cross-compiler won't be able to interpret comments w/o a preprocessor)
	CCFlags &= ~HLSLCC_NoPreprocess;

	FGlslCodeBackend* BackEnd = CreateBackend(Version, CCFlags, HlslCompilerTarget);
	FGlslLanguageSpec* LanguageSpec = CreateLanguageSpec(Version);

	int32 Result = 0;
	FHlslCrossCompilerContext CrossCompilerContext(CCFlags, Frequency, HlslCompilerTarget);
	if (CrossCompilerContext.Init(TCHAR_TO_ANSI(*Input.VirtualSourceFilePath), LanguageSpec))
	{
		Result = CrossCompilerContext.Run(
			TCHAR_TO_ANSI(*PreprocessedShader),
			TCHAR_TO_ANSI(*Input.EntryPointName),
			BackEnd,
			&GlslShaderSource,
			&ErrorLog
			) ? 1 : 0;
	}

	delete BackEnd;
	delete LanguageSpec;

	if (Result != 0)
	{
		int32 GlslSourceLen = GlslShaderSource ? FCStringAnsi::Strlen(GlslShaderSource) : 0;
		if (bDumpDebugInfo)
		{
			const FString GLSLFile = (Input.DumpDebugInfoPath / TEXT("Output.glsl"));
			const FString GLBatchFileContents = CreateCommandLineGLSLES2(GLSLFile, (Input.DumpDebugInfoPath / TEXT("Output.asm")), Version, Frequency, false);
			if (!GLBatchFileContents.IsEmpty())
			{
				FFileHelper::SaveStringToFile(GLBatchFileContents, *(Input.DumpDebugInfoPath / TEXT("GLSLCompile.bat")));
			}

			const FString NDABatchFileContents = CreateCommandLineGLSLES2(GLSLFile, (Input.DumpDebugInfoPath / TEXT("Output.asm")), Version, Frequency, true);
			if (!NDABatchFileContents.IsEmpty())
			{
				FFileHelper::SaveStringToFile(NDABatchFileContents, *(Input.DumpDebugInfoPath / TEXT("NDAGLSLCompile.bat")));
			}

			if (GlslSourceLen > 0)
			{
				uint32 Len = FCStringAnsi::Strlen(TCHAR_TO_ANSI(*Input.VirtualSourceFilePath)) + FCStringAnsi::Strlen(TCHAR_TO_ANSI(*Input.EntryPointName)) + FCStringAnsi::Strlen(GlslShaderSource) + 20;
				char* Dest = (char*)malloc(Len);
				FCStringAnsi::Snprintf(Dest, Len, "// ! %s:%s\n%s", (const char*)TCHAR_TO_ANSI(*Input.VirtualSourceFilePath), (const char*)TCHAR_TO_ANSI(*Input.EntryPointName), (const char*)GlslShaderSource);
				free(GlslShaderSource);
				GlslShaderSource = Dest;
				GlslSourceLen = FCStringAnsi::Strlen(GlslShaderSource);
					
				FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*(Input.DumpDebugInfoPath / Input.VirtualSourceFilePath + TEXT(".glsl")));
				if (FileWriter)
				{
					FileWriter->Serialize(GlslShaderSource,GlslSourceLen+1);
					FileWriter->Close();
					delete FileWriter;
				}
			}
		}

#if VALIDATE_GLSL_WITH_DRIVER
		PrecompileShader(Output, Input, GlslShaderSource, Version, Frequency);
#else // VALIDATE_GLSL_WITH_DRIVER
		int32 SourceLen = FCStringAnsi::Strlen(GlslShaderSource);
		Output.Target = Input.Target;
		BuildShaderOutput(Output, Input, GlslShaderSource, SourceLen, Version);
#endif // VALIDATE_GLSL_WITH_DRIVER
	}
	else
	{
		if (bDumpDebugInfo)
		{
			// Generate the batch file to help track down cross-compiler issues if necessary
			const FString GLSLFile = (Input.DumpDebugInfoPath / TEXT("Output.glsl"));
			const FString GLBatchFileContents = CreateCommandLineGLSLES2(GLSLFile, (Input.DumpDebugInfoPath / TEXT("Output.asm")), Version, Frequency, false);
			if (!GLBatchFileContents.IsEmpty())
			{
				FFileHelper::SaveStringToFile(GLBatchFileContents, *(Input.DumpDebugInfoPath / TEXT("GLSLCompile.bat")));
			}
		}

		FString Tmp = ANSI_TO_TCHAR(ErrorLog);
		TArray<FString> ErrorLines;
		Tmp.ParseIntoArray(ErrorLines, TEXT("\n"), true);
		for (int32 LineIndex = 0; LineIndex < ErrorLines.Num(); ++LineIndex)
		{
			const FString& Line = ErrorLines[LineIndex];
			CrossCompiler::ParseHlslccError(Output.Errors, Line);
		}
	}

	if (GlslShaderSource)
	{
		free(GlslShaderSource);
	}
	if (ErrorLog)
	{
		free(ErrorLog);
	}
}
