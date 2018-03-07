// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLShaders.cpp: OpenGL shader RHI implementation.
=============================================================================*/
 
#include "OpenGLShaders.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "OpenGLDrvPrivate.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "SceneUtils.h"

#define CHECK_FOR_GL_SHADERS_TO_REPLACE 0

#if PLATFORM_WINDOWS
#include <mmintrin.h>
#endif
#include "SceneUtils.h"

const uint32 SizeOfFloat4 = 16;
const uint32 NumFloatsInFloat4 = 4;

FORCEINLINE void FOpenGLShaderParameterCache::FRange::MarkDirtyRange(uint32 NewStartVector, uint32 NewNumVectors)
{
	if (NumVectors > 0)
	{
		uint32 High = StartVector + NumVectors;
		uint32 NewHigh = NewStartVector + NewNumVectors;
		
		uint32 MaxVector = FMath::Max(High, NewHigh);
		uint32 MinVector = FMath::Min(StartVector, NewStartVector);
		
		StartVector = MinVector;
		NumVectors = (MaxVector - MinVector) + 1;
	}
	else
	{
		StartVector = NewStartVector;
		NumVectors = NewNumVectors;
	}
}

/**
 * Verify that an OpenGL program has linked successfully.
 */
static bool VerifyLinkedProgram(GLuint Program)
{
	SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderLinkVerifyTime);

#if UE_BUILD_DEBUG || DEBUG_GL_SHADERS
	GLint LinkStatus = 0;
	glGetProgramiv(Program, GL_LINK_STATUS, &LinkStatus);
	if (LinkStatus != GL_TRUE)
	{
		GLint LogLength;
		ANSICHAR DefaultLog[] = "No log";
		ANSICHAR *CompileLog = DefaultLog;
		glGetProgramiv(Program, GL_INFO_LOG_LENGTH, &LogLength);
		if (LogLength > 1)
		{
			CompileLog = (ANSICHAR *)FMemory::Malloc(LogLength);
			glGetProgramInfoLog(Program, LogLength, NULL, CompileLog);
		}

		UE_LOG(LogRHI,Error,TEXT("Failed to link program. Compile log:\n%s"),
			ANSI_TO_TCHAR(CompileLog));

		if (LogLength > 1)
		{
			FMemory::Free(CompileLog);
		}
		return false;
	}
#endif
	return true;
}

/**
 * Verify that an OpenGL shader has compiled successfully. 
 */
static bool VerifyCompiledShader(GLuint Shader, const ANSICHAR* GlslCode )
{
	SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderCompileVerifyTime);

#if UE_BUILD_DEBUG || DEBUG_GL_SHADERS
	if (FOpenGL::SupportsSeparateShaderObjects() && glIsProgram(Shader))
	{
		bool const bCompiledOK = VerifyLinkedProgram(Shader);
#if DEBUG_GL_SHADERS
        if (!bCompiledOK && GlslCode)
        {
            UE_LOG(LogRHI,Error,TEXT("Shader:\n%s"),ANSI_TO_TCHAR(GlslCode));
            
#if 0
            const ANSICHAR *Temp = GlslCode;
            
            for ( int i = 0; i < 30 && (*Temp != '\0'); ++i )
            {
                FString Converted = ANSI_TO_TCHAR( Temp );
                Converted.LeftChop( 256 );
                
                UE_LOG(LogRHI,Display,TEXT("%s"), *Converted );
                Temp += Converted.Len();
            }
#endif
        }
#endif
		return bCompiledOK;
	}
	else
	{
		GLint CompileStatus;
		glGetShaderiv(Shader, GL_COMPILE_STATUS, &CompileStatus);
		if (CompileStatus != GL_TRUE)
		{
			GLint LogLength;
			ANSICHAR DefaultLog[] = "No log";
			ANSICHAR *CompileLog = DefaultLog;
			glGetShaderiv(Shader, GL_INFO_LOG_LENGTH, &LogLength);
	#if PLATFORM_ANDROID
			if ( LogLength == 0 )
			{
				// make it big anyway
				// there was a bug in android 2.2 where glGetShaderiv would return 0 even though there was a error message
				// https://code.google.com/p/android/issues/detail?id=9953
				LogLength = 4096;
			}
	#endif
			if (LogLength > 1)
			{
				CompileLog = (ANSICHAR *)FMemory::Malloc(LogLength);
				glGetShaderInfoLog(Shader, LogLength, NULL, CompileLog);
			}

	#if DEBUG_GL_SHADERS
			if (GlslCode)
			{
				UE_LOG(LogRHI,Error,TEXT("Shader:\n%s"),ANSI_TO_TCHAR(GlslCode));

	#if 0
				const ANSICHAR *Temp = GlslCode;

				for ( int i = 0; i < 30 && (*Temp != '\0'); ++i )
				{
					FString Converted = ANSI_TO_TCHAR( Temp );
					Converted.LeftChop( 256 );

					UE_LOG(LogRHI,Display,TEXT("%s"), *Converted );
					Temp += Converted.Len();
				}
	#endif
			}	
	#endif
			UE_LOG(LogRHI,Fatal,TEXT("Failed to compile shader. Compile log:\n%s"), ANSI_TO_TCHAR(CompileLog));

			if (LogLength > 1)
			{
				FMemory::Free(CompileLog);
			}
			return false;
		}
	}
#endif
	return true;
}

static bool VerifyProgramPipeline(GLuint Program)
{
	bool bOK = true;
	// Don't try and validate SSOs here - the draw state matters to SSOs and it definitely can't be guaranteed to be valid at this stage
	if ( FOpenGL::SupportsSeparateShaderObjects() )
	{
#if DEBUG_GL_SHADERS
		bOK = FOpenGL::IsProgramPipeline(Program);
#endif
	}
	else
	{
		bOK = VerifyLinkedProgram(Program);
	}
	return bOK;
}

// ============================================================================================================================

class FOpenGLCompiledShaderKey
{
public:
	FOpenGLCompiledShaderKey(
		GLenum InTypeEnum,
		uint32 InCodeSize,
		uint32 InCodeCRC
		)
		: TypeEnum(InTypeEnum)
		, CodeSize(InCodeSize)
		, CodeCRC(InCodeCRC)
	{}

	friend bool operator ==(const FOpenGLCompiledShaderKey& A,const FOpenGLCompiledShaderKey& B)
	{
		return A.TypeEnum == B.TypeEnum && A.CodeSize == B.CodeSize && A.CodeCRC == B.CodeCRC;
	}

	friend uint32 GetTypeHash(const FOpenGLCompiledShaderKey &Key)
	{
		return GetTypeHash(Key.TypeEnum) ^ GetTypeHash(Key.CodeSize) ^ GetTypeHash(Key.CodeCRC);
	}

private:
	GLenum TypeEnum;
	uint32 CodeSize;
	uint32 CodeCRC;
};

typedef TMap<FOpenGLCompiledShaderKey,GLuint> FOpenGLCompiledShaderCache;

static FOpenGLCompiledShaderCache& GetOpenGLCompiledShaderCache()
{
	static FOpenGLCompiledShaderCache CompiledShaderCache;
	return CompiledShaderCache;
}

// ============================================================================================================================


static const TCHAR* ShaderNameFromShaderType(GLenum ShaderType)
{
	switch(ShaderType)
	{
		case GL_VERTEX_SHADER: return TEXT("vertex");
		case GL_FRAGMENT_SHADER: return TEXT("fragment");
		case GL_GEOMETRY_SHADER: return TEXT("geometry");
		case GL_TESS_CONTROL_SHADER: return TEXT("hull");
		case GL_TESS_EVALUATION_SHADER: return TEXT("domain");
		case GL_COMPUTE_SHADER: return TEXT("compute");
		default: return NULL;
	}
}

// ============================================================================================================================

namespace
{
	inline void AppendCString(TArray<ANSICHAR> & Dest, const ANSICHAR * Source)
	{
		if (Dest.Num() > 0)
		{
			Dest.Insert(Source, FCStringAnsi::Strlen(Source), Dest.Num() - 1);;
		}
		else
		{
			Dest.Append(Source, FCStringAnsi::Strlen(Source) + 1);
		}
	}

	inline void ReplaceCString(TArray<ANSICHAR> & Dest, const ANSICHAR * Source, const ANSICHAR * Replacement)
	{
		int32 SourceLen = FCStringAnsi::Strlen(Source);
		int32 ReplacementLen = FCStringAnsi::Strlen(Replacement);
		int32 FoundIndex = 0;
		for (const ANSICHAR * FoundPointer = FCStringAnsi::Strstr(Dest.GetData(), Source);
			nullptr != FoundPointer;
			FoundPointer = FCStringAnsi::Strstr(Dest.GetData()+FoundIndex, Source))
		{
			FoundIndex = FoundPointer - Dest.GetData();
			Dest.RemoveAt(FoundIndex, SourceLen);
			Dest.Insert(Replacement, ReplacementLen, FoundIndex);
		}
	}

	inline const ANSICHAR * CStringEndOfLine(const ANSICHAR * Text)
	{
		const ANSICHAR * LineEnd = FCStringAnsi::Strchr(Text, '\n');
		if (nullptr == LineEnd)
		{
			LineEnd = Text + FCStringAnsi::Strlen(Text);
		}
		return LineEnd;
	}

	inline bool CStringIsBlankLine(const ANSICHAR * Text)
	{
		while (!FCharAnsi::IsLinebreak(*Text))
		{
			if (!FCharAnsi::IsWhitespace(*Text))
			{
				return false;
			}
			++Text;
		}
		return true;
	}

	inline int CStringCountOccurances(TArray<ANSICHAR> & Source, const ANSICHAR * TargetString)
	{
		int32 TargetLen = FCStringAnsi::Strlen(TargetString);
		int Count = 0;
		int32 FoundIndex = 0;
		for (const ANSICHAR * FoundPointer = FCStringAnsi::Strstr(Source.GetData(), TargetString);
			nullptr != FoundPointer;
			FoundPointer = FCStringAnsi::Strstr(Source.GetData() + FoundIndex, TargetString))
		{
			FoundIndex = FoundPointer - Source.GetData();
			FoundIndex += TargetLen;
			Count++;
		}
		return Count;
	}

	inline bool MoveHashLines(TArray<ANSICHAR> & Dest, TArray<ANSICHAR> & Source)
	{
		// Walk through the lines to find the first non-# line...
		const ANSICHAR * LineStart = Source.GetData();
		for (bool FoundNonHashLine = false; !FoundNonHashLine;)
		{
			const ANSICHAR * LineEnd = CStringEndOfLine(LineStart);
			if (LineStart[0] != '#' && !CStringIsBlankLine(LineStart))
			{
				FoundNonHashLine = true;
			}
			else if (LineEnd[0] == '\n')
			{
				LineStart = LineEnd + 1;
			}
			else
			{
				LineStart = LineEnd;
			}
		}
		// Copy the hash lines over, if we found any. And delete from
		// the source.
		if (LineStart > Source.GetData())
		{
			int32 LineLength = LineStart - Source.GetData();
			if (Dest.Num() > 0)
			{
				Dest.Insert(Source.GetData(), LineLength, Dest.Num() - 1);
			}
			else
			{
				Dest.Append(Source.GetData(), LineLength);
				Dest.Append("", 1);
			}
			if (Dest.Last(1) != '\n')
			{
				Dest.Insert("\n", 1, Dest.Num() - 1);
			}
			Source.RemoveAt(0, LineStart - Source.GetData());
			return true;
		}
		return false;
	}
}

inline uint32 GetTypeHash(FAnsiCharArray const& CharArray)
{
	return FCrc::MemCrc32(CharArray.GetData(), CharArray.Num() * sizeof(ANSICHAR));
}

static void BindShaderLocations(GLenum TypeEnum, GLuint Resource, uint16 InOutMask, const uint8 * RemapTable = nullptr)
{
	if ( OpenGLShaderPlatformNeedsBindLocation(GMaxRHIShaderPlatform) )
	{
		ANSICHAR Buf[32] = {0};
		switch(TypeEnum)
		{
			case GL_VERTEX_SHADER:
			{
				uint32 Mask = InOutMask;
				uint32 Index = 0;
				FCStringAnsi::Strcpy(Buf, "in_ATTRIBUTE");
				while (Mask)
				{
					if (Mask & 0x1)
					{
						if (Index < 10)
						{
							Buf[12] = '0' + Index;
							Buf[13] = 0;
						}
						else
						{
							Buf[12] = '1';
							Buf[13] = '0' + (Index % 10);
							Buf[14] = 0;
						}

						if (FOpenGL::NeedsVertexAttribRemapTable())
						{
							check(RemapTable != nullptr);
							uint32 MappedAttributeIndex = RemapTable[Index];
							check(MappedAttributeIndex < NUM_OPENGL_VERTEX_STREAMS);
							glBindAttribLocation(Resource, MappedAttributeIndex, Buf);
						}
						else
						{
							glBindAttribLocation(Resource, Index, Buf);
						}
					}
					Index++;
					Mask >>= 1;
				}
				break;
			}
			case GL_FRAGMENT_SHADER:
			{
				uint32 Mask = (InOutMask) & 0x7fff; // mask out the depth bit
				uint32 Index = 0;
				FCStringAnsi::Strcpy(Buf, "out_Target");
				while (Mask)
				{
					if (Mask & 0x1)
					{
						if (Index < 10)
						{
							Buf[10] = '0' + Index;
							Buf[11] = 0;
						}
						else
						{
							Buf[10] = '1';
							Buf[11] = '0' + (Index % 10);
							Buf[12] = 0;
						}
						FOpenGL::BindFragDataLocation(Resource, Index, Buf);
					}
					Index++;
					Mask >>= 1;
				}
				break;
			}
			case GL_GEOMETRY_SHADER:
			case GL_COMPUTE_SHADER:
			case GL_TESS_CONTROL_SHADER:
			case GL_TESS_EVALUATION_SHADER:
				break;
			default:
				check(0);
				break;
		}
	}
}

// Helper to compile a shader and return success, logging errors if necessary.
GLint CompileCurrentShader(const GLuint Resource, const FAnsiCharArray& GlslCode)
{
	const ANSICHAR * GlslCodeString = GlslCode.GetData();
	int32 GlslCodeLength = GlslCode.Num() - 1;

	glShaderSource(Resource, 1, (const GLchar**)&GlslCodeString, &GlslCodeLength);
	glCompileShader(Resource);

	GLint CompileStatus = GL_TRUE;
#if PLATFORM_ANDROID
	// On Android the same shader is compiled with different hacks to find the right one(s) to apply so don't cache unless successful if currently testing them
	if (FOpenGL::IsCheckingShaderCompilerHacks())
	{
		glGetShaderiv(Resource, GL_COMPILE_STATUS, &CompileStatus);
	}
#endif
#if (PLATFORM_HTML5 || PLATFORM_ANDROID || PLATFORM_IOS) && !UE_BUILD_SHIPPING
	if (!FOpenGL::IsCheckingShaderCompilerHacks())
	{
		glGetShaderiv(Resource, GL_COMPILE_STATUS, &CompileStatus);
		if (CompileStatus == GL_FALSE)
		{
			char Msg[2048];
			glGetShaderInfoLog(Resource, 2048, nullptr, Msg);
			UE_LOG(LogRHI, Error, TEXT("Shader compile failed: %s\n Original Source is (len %d) %s"), ANSI_TO_TCHAR(Msg), GlslCodeLength, ANSI_TO_TCHAR(GlslCodeString));
		}
	}
#endif

#if PLATFORM_IOS // fix for running out of memory in the driver when compiling/linking a lot of shaders on the first frame
	if (FOpenGL::IsLimitingShaderCompileCount())
	{
		static int CompileCount = 0;
		CompileCount++;
		if (CompileCount == 2500)
		{
			glFlush();
			CompileCount = 0;
		}
	}
#endif

	return CompileStatus;
}

/**
 * Compiles an OpenGL shader using the given GLSL microcode.
 * @returns the compiled shader upon success.
 */
template <typename ShaderType>
ShaderType* CompileOpenGLShader(const TArray<uint8>& InShaderCode)
{
	SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderCompileTime);
	VERIFY_GL_SCOPE();

	FShaderCodeReader ShaderCode(InShaderCode);

	ShaderType* Shader = nullptr;
	const GLenum TypeEnum = ShaderType::TypeEnum;
	FMemoryReader Ar(InShaderCode, true);

	Ar.SetLimitSize(ShaderCode.GetActualShaderCodeSize());

	FOpenGLCodeHeader Header = { 0 };

	Ar << Header;
	// Suppress static code analysis warning about a potential comparison of two constants
	CA_SUPPRESS(6326);
	if (Header.GlslMarker != 0x474c534c
		|| (TypeEnum == GL_VERTEX_SHADER && Header.FrequencyMarker != 0x5653)
		|| (TypeEnum == GL_FRAGMENT_SHADER && Header.FrequencyMarker != 0x5053)
		|| (TypeEnum == GL_GEOMETRY_SHADER && Header.FrequencyMarker != 0x4753)
		|| (TypeEnum == GL_COMPUTE_SHADER && Header.FrequencyMarker != 0x4353 && FOpenGL::SupportsComputeShaders())
		|| (TypeEnum == GL_TESS_CONTROL_SHADER && Header.FrequencyMarker != 0x4853 && FOpenGL::SupportsTessellation()) /* hull shader*/
		|| (TypeEnum == GL_TESS_EVALUATION_SHADER && Header.FrequencyMarker != 0x4453 && FOpenGL::SupportsTessellation()) /* domain shader*/
		)
	{
		UE_LOG(LogRHI,Fatal,
			TEXT("Corrupt shader bytecode. GlslMarker=0x%08x FrequencyMarker=0x%04x"),
			Header.GlslMarker,
			Header.FrequencyMarker
			);
		return nullptr;
	}

	int32 CodeOffset = Ar.Tell();

	// The code as given to us.
	FAnsiCharArray GlslCodeOriginal;
	AppendCString(GlslCodeOriginal, (ANSICHAR*)InShaderCode.GetData() + CodeOffset);
	uint32 GlslCodeOriginalCRC = FCrc::MemCrc_DEPRECATED(GlslCodeOriginal.GetData(), GlslCodeOriginal.Num());

	// The amended code we actually compile.
	FAnsiCharArray GlslCode;

	// Find the existing compiled shader in the cache.
	FOpenGLCompiledShaderKey Key(TypeEnum, GlslCodeOriginal.Num(), GlslCodeOriginalCRC);
	GLuint Resource = GetOpenGLCompiledShaderCache().FindRef(Key);
	if (!Resource)
	{
#if CHECK_FOR_GL_SHADERS_TO_REPLACE
		{
			// 1. Check for specific file
			FString PotentialShaderFileName = FString::Printf(TEXT("%s-%d-0x%x.txt"), ShaderNameFromShaderType(TypeEnum), GlslCodeOriginal.Num(), GlslCodeOriginalCRC);
			FString PotentialShaderFile = FPaths::ProfilingDir();
			PotentialShaderFile *= PotentialShaderFileName;

			UE_LOG( LogRHI, Log, TEXT("Looking for shader file '%s' for potential replacement."), *PotentialShaderFileName );

			int64 FileSize = IFileManager::Get().FileSize(*PotentialShaderFile);
			if( FileSize > 0 )
			{
				FArchive* Ar = IFileManager::Get().CreateFileReader(*PotentialShaderFile);
				if( Ar != NULL )
				{
					UE_LOG(LogRHI, Log, TEXT("Replacing %s shader with length %d and CRC 0x%x with the one from a file."), (TypeEnum == GL_VERTEX_SHADER) ? TEXT("vertex") : ((TypeEnum == GL_FRAGMENT_SHADER) ? TEXT("fragment") : TEXT("geometry")), GlslCodeOriginal.Num(), GlslCodeOriginalCRC);

					// read in the file
					GlslCodeOriginal.Empty();
					GlslCodeOriginal.AddUninitialized(FileSize + 1);
					Ar->Serialize(GlslCodeOriginal.GetData(), FileSize);
					delete Ar;
					GlslCodeOriginal[FileSize] = 0;
				}
			}
		}
#endif

		Resource = FOpenGL::CreateShader(TypeEnum);

		// get a modified version of the shader based on device capabilities to compile (destructive to GlslCodeOriginal copy)
		FOpenGLShaderDeviceCapabilities Capabilities;
		GetCurrentOpenGLShaderDeviceCapabilities(Capabilities);
		GLSLToDeviceCompatibleGLSL(GlslCodeOriginal, Header.ShaderName, TypeEnum, Capabilities, GlslCode);

		GLint CompileStatus = GL_TRUE;

		// Save the code and defer compilation if our device supports program binaries and we're not checking for shader compatibility.
		if (!FOpenGLProgramBinaryCache::DeferShaderCompilation(Resource, GlslCode))
		{
			CompileStatus = CompileCurrentShader(Resource, GlslCode);
		}

		if ( CompileStatus == GL_TRUE )
		{
			if (Capabilities.bSupportsSeparateShaderObjects)
			{
				ANSICHAR Buf[32] = {0};
				// Create separate shader program
				GLuint SeparateResource = FOpenGL::CreateProgram();
				FOpenGL::ProgramParameter( SeparateResource, GL_PROGRAM_SEPARABLE, GL_TRUE );
				glAttachShader(SeparateResource, Resource);
				
				glLinkProgram(SeparateResource);
				bool const bLinkedOK = VerifyLinkedProgram(SeparateResource);
				if (!bLinkedOK)
				{
					const ANSICHAR* GlslCodeString = GlslCode.GetData();
					check(VerifyCompiledShader(Resource, GlslCodeString));
				}
			
#if ENABLE_UNIFORM_BUFFER_LAYOUT_VERIFICATION
				void VerifyUniformBufferLayouts(GLuint Program);
				VerifyUniformBufferLayouts(SeparateResource);
#endif // #if ENABLE_UNIFORM_BUFFER_LAYOUT_VERIFICATION
			
				Resource = SeparateResource;
			}
			
			// Cache it; compile status will be checked later on link (always caching will prevent multiple attempts to compile a failed shader)
			GetOpenGLCompiledShaderCache().Add(Key, Resource);
		}
	}

    Shader = new ShaderType();
	Shader->Resource = Resource;
	Shader->Bindings = Header.Bindings;
	Shader->UniformBuffersCopyInfo = Header.UniformBuffersCopyInfo;
	
	// If there is no shader cache then we must assign the hash here
	if (FOpenGL::SupportsSeparateShaderObjects() && !FShaderCache::GetShaderCache())
	{
		// Just use the CRC - if it isn't being cached & logged we'll be dependent on the CRC alone anyway
		FSHAHash Hash;
		FMemory::Memcpy(Hash.Hash, &GlslCodeOriginalCRC, sizeof(uint32));
		Shader->SetHash(Hash);
	}

#if DEBUG_GL_SHADERS
	Shader->GlslCode = GlslCode;
	Shader->GlslCodeString = (ANSICHAR*)Shader->GlslCode.GetData();
#endif

	return Shader;
}

void OPENGLDRV_API GetCurrentOpenGLShaderDeviceCapabilities(FOpenGLShaderDeviceCapabilities& Capabilities)
{
	FMemory::Memzero(Capabilities);

#if PLATFORM_DESKTOP
	Capabilities.TargetPlatform = EOpenGLShaderTargetPlatform::OGLSTP_Desktop;
#elif PLATFORM_ANDROID
	Capabilities.TargetPlatform = EOpenGLShaderTargetPlatform::OGLSTP_Android;
	Capabilities.bUseES30ShadingLanguage = FOpenGL::UseES30ShadingLanguage();
	Capabilities.bSupportsStandardDerivativesExtension = FOpenGL::SupportsStandardDerivativesExtension();
	Capabilities.bSupportsRenderTargetFormat_PF_FloatRGBA = GSupportsRenderTargetFormat_PF_FloatRGBA;
	Capabilities.bSupportsShaderFramebufferFetch = FOpenGL::SupportsShaderFramebufferFetch();
	Capabilities.bRequiresARMShaderFramebufferFetchDepthStencilUndef = FOpenGL::RequiresARMShaderFramebufferFetchDepthStencilUndef();
	Capabilities.bRequiresDontEmitPrecisionForTextureSamplers = FOpenGL::RequiresDontEmitPrecisionForTextureSamplers();
	Capabilities.bSupportsShaderTextureLod = FOpenGL::SupportsShaderTextureLod();
	Capabilities.bSupportsShaderTextureCubeLod = FOpenGL::SupportsShaderTextureCubeLod();
	Capabilities.bRequiresTextureCubeLodEXTToTextureCubeLodDefine = FOpenGL::RequiresTextureCubeLodEXTToTextureCubeLodDefine();
	Capabilities.bRequiresGLFragCoordVaryingLimitHack = FOpenGL::RequiresGLFragCoordVaryingLimitHack();
	Capabilities.MaxVaryingVectors = FOpenGL::GetMaxVaryingVectors();
	Capabilities.bRequiresTexture2DPrecisionHack = FOpenGL::RequiresTexture2DPrecisionHack();
#elif PLATFORM_HTML5
	Capabilities.TargetPlatform = EOpenGLShaderTargetPlatform::OGLSTP_HTML5;
	Capabilities.bUseES30ShadingLanguage = FOpenGL::UseES30ShadingLanguage();
	Capabilities.bSupportsShaderTextureLod = FOpenGL::SupportsShaderTextureLod();
#elif PLATFORM_IOS
	Capabilities.TargetPlatform = EOpenGLShaderTargetPlatform::OGLSTP_iOS;
#else
	Capabilities.TargetPlatform = EOpenGLShaderTargetPlatform::OGLSTP_Unknown;
#endif
	Capabilities.MaxRHIShaderPlatform = GMaxRHIShaderPlatform;
	Capabilities.bSupportsSeparateShaderObjects = FOpenGL::SupportsSeparateShaderObjects();

#if OPENGL_ES2 || OPENGL_ESDEFERRED
	Capabilities.bRequiresUEShaderFramebufferFetchDef = FOpenGL::RequiresUEShaderFramebufferFetchDef();
#endif
	
}

void OPENGLDRV_API GLSLToDeviceCompatibleGLSL(FAnsiCharArray& GlslCodeOriginal, const FString& ShaderName, GLenum TypeEnum, const FOpenGLShaderDeviceCapabilities& Capabilities, FAnsiCharArray& GlslCode)
{
	// Whether shader was compiled for ES 3.1
	const bool bES31 = (FCStringAnsi::Strstr(GlslCodeOriginal.GetData(), "#version 310 es") != nullptr);

	// Whether we need to emit mobile multi-view code or not.
	const bool bEmitMobileMultiView = (FCStringAnsi::Strstr(GlslCodeOriginal.GetData(), "gl_ViewID_OVR") != nullptr);

	// Whether we need to emit texture external code or not.
	const bool bEmitTextureExternal = (FCStringAnsi::Strstr(GlslCodeOriginal.GetData(), "samplerExternalOES") != nullptr);

	bool bUseES30ShadingLanguage = Capabilities.bUseES30ShadingLanguage;

#if PLATFORM_ANDROID
	FOpenGL::EImageExternalType ImageExternalType = FOpenGL::GetImageExternalType();

	if (bEmitTextureExternal && ImageExternalType == FOpenGL::EImageExternalType::ImageExternal100)
	{
		bUseES30ShadingLanguage = false;
	}
#endif

	bool bNeedsExtDrawInstancedDefine = false;
	if (Capabilities.TargetPlatform == EOpenGLShaderTargetPlatform::OGLSTP_Android || Capabilities.TargetPlatform == EOpenGLShaderTargetPlatform::OGLSTP_HTML5)
	{
		bNeedsExtDrawInstancedDefine = !bES31;
		if (IsES2Platform(Capabilities.MaxRHIShaderPlatform) && !bES31)
		{
			// #version NNN has to be the first line in the file, so it has to be added before anything else.
			if (bUseES30ShadingLanguage)
			{
				bNeedsExtDrawInstancedDefine = false;
				AppendCString(GlslCode, "#version 300 es\n");
			}
			else 
			{
				AppendCString(GlslCode, "#version 100\n");
			}
			ReplaceCString(GlslCodeOriginal, "#version 100", "");
		}
	}
	else if (Capabilities.TargetPlatform == EOpenGLShaderTargetPlatform::OGLSTP_iOS)
	{
		bNeedsExtDrawInstancedDefine = true;
		AppendCString(GlslCode, "#version 100\n");
		ReplaceCString(GlslCodeOriginal, "#version 100", "");
	}

	if (bNeedsExtDrawInstancedDefine)
	{
		// Check for the GL_EXT_draw_instanced extension if necessary (version < 300)
		AppendCString(GlslCode, "#ifdef GL_EXT_draw_instanced\n");
		AppendCString(GlslCode, "#define UE_EXT_draw_instanced 1\n");
		AppendCString(GlslCode, "#endif\n");
	}

	if (bEmitMobileMultiView)
	{
		MoveHashLines(GlslCode, GlslCodeOriginal);

		if (GSupportsMobileMultiView)
		{
			AppendCString(GlslCode, "\n\n");
			AppendCString(GlslCode, "#extension GL_OVR_multiview2 : enable\n");
			AppendCString(GlslCode, "\n\n");
		}
		else
		{
			// Strip out multi-view for devices that don't support it.
			AppendCString(GlslCode, "#define gl_ViewID_OVR 0\n");
		}
	}

	if (bEmitTextureExternal)
	{
		MoveHashLines(GlslCode, GlslCodeOriginal);

		if (GSupportsImageExternal)
		{
			AppendCString(GlslCode, "\n\n");

#if PLATFORM_ANDROID
			switch (ImageExternalType)
			{
				case FOpenGL::EImageExternalType::ImageExternal100:
					AppendCString(GlslCode, "#extension GL_OES_EGL_image_external : require\n");
					break;

				case FOpenGL::EImageExternalType::ImageExternal300:
					AppendCString(GlslCode, "#extension GL_OES_EGL_image_external : require\n");
					break;

				case FOpenGL::EImageExternalType::ImageExternalESSL300:
					// GL_OES_EGL_image_external_essl3 is only compatible with ES 3.x
					AppendCString(GlslCode, "#extension GL_OES_EGL_image_external_essl3 : require\n");
					break;
			}
#else
			AppendCString(GlslCode, "#extension GL_OES_EGL_image_external : require\n");
#endif
			AppendCString(GlslCode, "\n\n");
		}
		else
		{
			// Strip out texture external for devices that don't support it.
			AppendCString(GlslCode, "#define samplerExternalOES sampler2D\n");
		}
	}

	// Only desktop with separable shader platform can use GL_ARB_separate_shader_objects for reduced shader compile/link hitches
	// however ES3.1 relies on layout(location=) support
	bool const bNeedsBindLocation = OpenGLShaderPlatformNeedsBindLocation(Capabilities.MaxRHIShaderPlatform) && !bES31;
	if (OpenGLShaderPlatformSeparable(Capabilities.MaxRHIShaderPlatform) || !bNeedsBindLocation)
	{
		// Move version tag & extensions before beginning all other operations
		MoveHashLines(GlslCode, GlslCodeOriginal);
		
		// OpenGL SM5 shader platforms require location declarations for the layout, but don't necessarily use SSOs
		if (Capabilities.bSupportsSeparateShaderObjects || !bNeedsBindLocation)
		{
			if (Capabilities.TargetPlatform == EOpenGLShaderTargetPlatform::OGLSTP_Desktop)
			{
				AppendCString(GlslCode, "#extension GL_ARB_separate_shader_objects : enable\n");
				AppendCString(GlslCode, "#define INTERFACE_LOCATION(Pos) layout(location=Pos) \n");
				AppendCString(GlslCode, "#define INTERFACE_BLOCK(Pos, Interp, Modifiers, Semantic, PreType, PostType) layout(location=Pos) Interp Modifiers struct { PreType PostType; }\n");
			}
			else
			{
				AppendCString(GlslCode, "#define INTERFACE_LOCATION(Pos) layout(location=Pos) \n");
				AppendCString(GlslCode, "#define INTERFACE_BLOCK(Pos, Interp, Modifiers, Semantic, PreType, PostType) layout(location=Pos) Modifiers Semantic { PreType PostType; }\n");
			}
		}
		else
		{
			AppendCString(GlslCode, "#define INTERFACE_LOCATION(Pos) \n");
			AppendCString(GlslCode, "#define INTERFACE_BLOCK(Pos, Interp, Modifiers, Semantic, PreType, PostType) Modifiers Semantic { Interp PreType PostType; }\n");
		}
	}

	if (ShaderName.IsEmpty() == false)
	{
		AppendCString(GlslCode, "// ");
		AppendCString(GlslCode, TCHAR_TO_ANSI(ShaderName.GetCharArray().GetData()));
		AppendCString(GlslCode, "\n");
	}

	if (bEmitMobileMultiView && GSupportsMobileMultiView && TypeEnum == GL_VERTEX_SHADER)
	{
		AppendCString(GlslCode, "\n\n");
		AppendCString(GlslCode, "layout(num_views = 2) in;\n");
		AppendCString(GlslCode, "\n\n");
	}

	if (Capabilities.bRequiresUEShaderFramebufferFetchDef && TypeEnum == GL_FRAGMENT_SHADER)
	{
		// Some devices (Zenfone5) support GL_EXT_shader_framebuffer_fetch but do not define GL_EXT_shader_framebuffer_fetch in GLSL compiler
		// We can't define anything with GL_, so we use UE_EXT_shader_framebuffer_fetch to enable frame buffer fetch
		AppendCString(GlslCode, "#define UE_EXT_shader_framebuffer_fetch 1\n");
	}

	if (Capabilities.TargetPlatform == EOpenGLShaderTargetPlatform::OGLSTP_Android)
	{
		// Temporary patch to remove #extension GL_OES_standard_derivaties if not supported
		if (Capabilities.bSupportsStandardDerivativesExtension)
		{
			const ANSICHAR * FoundPointer = FCStringAnsi::Strstr(GlslCodeOriginal.GetData(), "#extension GL_OES_standard_derivatives");
			if (FoundPointer != nullptr)
			{
				// Replace the extension enable with dFdx, dFdy, and fwidth definitions so shader will compile.
				// Currently SimpleElementPixelShader.usf is the most likely place this will come from for mobile
				// as it is used for distance field text rendering (GammaDistanceFieldMain) so use a constant
				// for the texture step rate of 1/512.  This will not work for other use cases.
				ReplaceCString(GlslCodeOriginal, "#extension GL_OES_standard_derivatives : enable",
					"#define dFdx(a) (0.001953125)\n"
					"#define dFdy(a) (0.001953125)\n"
					"#define fwidth(a) (0.00390625)\n");
			}
		}

		if (IsES2Platform(Capabilities.MaxRHIShaderPlatform) && !bES31)
		{
			const ANSICHAR * EncodeModeDefine = nullptr;

			switch (GetMobileHDRMode())
			{
				case EMobileHDRMode::Disabled:
				case EMobileHDRMode::EnabledFloat16:
					EncodeModeDefine = "#define HDR_32BPP_ENCODE_MODE 0.0\n";
					break;
				case EMobileHDRMode::EnabledMosaic:
					EncodeModeDefine = "#define HDR_32BPP_ENCODE_MODE 1.0\n";
					break;
				case EMobileHDRMode::EnabledRGBE:
					EncodeModeDefine = "#define HDR_32BPP_ENCODE_MODE 2.0\n";
					break;
				case EMobileHDRMode::EnabledRGBA8:
					EncodeModeDefine = "#define HDR_32BPP_ENCODE_MODE 3.0\n";
					break;
				default:
					checkNoEntry();
					break;
			}
			AppendCString(GlslCode, EncodeModeDefine);

			if (Capabilities.bRequiresARMShaderFramebufferFetchDepthStencilUndef && TypeEnum == GL_FRAGMENT_SHADER)
			{
				// This is to avoid a bug in Adreno drivers that define GL_ARM_shader_framebuffer_fetch_depth_stencil even when device does not support this extension
				// OpenGL ES 3.1 V@127.0 (GIT@I1af360237c)
				AppendCString(GlslCode, "#undef GL_ARM_shader_framebuffer_fetch_depth_stencil\n");
			}

			// This #define fixes compiler errors on Android (which doesn't seem to support textureCubeLodEXT)
			if (bUseES30ShadingLanguage)
			{
				if (TypeEnum == GL_VERTEX_SHADER)
				{
					AppendCString(GlslCode,
						"#define texture2D texture \n"
						"#define texture2DProj textureProj \n"
						"#define texture2DLod textureLod \n"
						"#define texture2DLodEXT textureLod \n"
						"#define texture2DProjLod textureProjLod \n"
						"#define textureCube texture \n"
						"#define textureCubeLod textureLod \n"
						"#define textureCubeLodEXT textureLod \n"
						"#define texture3D texture \n"
						"#define texture3DProj textureProj \n"
						"#define texture3DLod textureLod \n");

					ReplaceCString(GlslCodeOriginal, "attribute", "in");
					ReplaceCString(GlslCodeOriginal, "varying", "out");
				} 
				else if (TypeEnum == GL_FRAGMENT_SHADER)
				{
					// #extension directives have to come before any non-# directives. Because
					// we add non-# stuff below and the #extension directives
					// get added to the incoming shader source we move any # directives
					// to be right after the #version to ensure they are always correct.
					MoveHashLines(GlslCode, GlslCodeOriginal);

					AppendCString(GlslCode,
						"#define texture2D texture \n"
						"#define texture2DProj textureProj \n"
						"#define texture2DLod textureLod \n"
						"#define texture2DLodEXT textureLod \n"
						"#define texture2DProjLod textureProjLod \n"
						"#define textureCube texture \n"
						"#define textureCubeLod textureLod \n"
						"#define textureCubeLodEXT textureLod \n"
						"#define texture3D texture \n"
						"#define texture3DProj textureProj \n"
						"#define texture3DLod textureLod \n"
						"#define texture3DProjLod textureProjLod \n"
						"\n"
						"#define gl_FragColor out_FragColor \n"
						"#ifdef EXT_shader_framebuffer_fetch_enabled \n"
						"inout mediump vec4 out_FragColor; \n"
						"#else \n"
						"out mediump vec4 out_FragColor; \n"
						"#endif \n");

					ReplaceCString(GlslCodeOriginal, "varying", "in");
				}
			}
			else 
			{
				if (TypeEnum == GL_FRAGMENT_SHADER)
				{
					// Apply #defines to deal with incompatible sections of code

					if (Capabilities.bRequiresDontEmitPrecisionForTextureSamplers)
					{
						AppendCString(GlslCode,
							"#define DONTEMITSAMPLERDEFAULTPRECISION \n");
					}

					if (!Capabilities.bSupportsShaderTextureLod || !Capabilities.bSupportsShaderTextureCubeLod)
					{
						AppendCString(GlslCode,
							"#define DONTEMITEXTENSIONSHADERTEXTURELODENABLE \n"
							"#define texture2DLodEXT(a, b, c) texture2D(a, b) \n"
							"#define textureCubeLodEXT(a, b, c) textureCube(a, b) \n");
					}
					else if (Capabilities.bRequiresTextureCubeLodEXTToTextureCubeLodDefine)
					{
						AppendCString(GlslCode,
							"#define textureCubeLodEXT textureCubeLod \n");
					}

					// Deal with gl_FragCoord using one of the varying vectors and shader possibly exceeding the limit
					if (Capabilities.bRequiresGLFragCoordVaryingLimitHack)
					{
						if (CStringCountOccurances(GlslCodeOriginal, "vec4 var_TEXCOORD") >= Capabilities.MaxVaryingVectors)
						{
							// It is likely gl_FragCoord is used for mosaic color output so use an appropriate constant
							ReplaceCString(GlslCodeOriginal, "gl_FragCoord.xy", "vec2(400.5,240.5)");
						}
					}

					if (Capabilities.bRequiresTexture2DPrecisionHack)
					{
						AppendCString(GlslCode,	"#define TEXCOORDPRECISIONWORKAROUND \n");
					}
				}
			}
		}
	}
	else if (Capabilities.TargetPlatform == EOpenGLShaderTargetPlatform::OGLSTP_HTML5)
	{
		// HTML5 use case is much simpler, use a separate chunk of code from android. 
		if (!Capabilities.bSupportsShaderTextureLod)
		{
			AppendCString(GlslCode,
				"#define DONTEMITEXTENSIONSHADERTEXTURELODENABLE \n"
				"#define texture2DLodEXT(a, b, c) texture2D(a, b) \n"
				"#define textureCubeLodEXT(a, b, c) textureCube(a, b) \n");
		}
	}

	if (FOpenGL::SupportsClipControl())
	{
		AppendCString(GlslCode, "#define HLSLCC_DX11ClipSpace 0 \n");
	}
	else
	{
		AppendCString(GlslCode, "#define HLSLCC_DX11ClipSpace 1 \n");
	}

	// Append the possibly edited shader to the one we will compile.
	// This is to make it easier to debug as we can see the whole
	// shader source.
	AppendCString(GlslCode, "\n\n");
	AppendCString(GlslCode, GlslCodeOriginal.GetData());
}

/**
 * Helper for constructing strings of the form XXXXX##.
 * @param Str - The string to build.
 * @param Offset - Offset into the string at which to set the number.
 * @param Index - Number to set. Must be in the range [0,100).
 */
static ANSICHAR* SetIndex(ANSICHAR* Str, int32 Offset, int32 Index)
{
	check(Index >= 0 && Index < 100);

	Str += Offset;
	if (Index >= 10)
	{
		*Str++ = '0' + (ANSICHAR)(Index / 10);
	}
	*Str++ = '0' + (ANSICHAR)(Index % 10);
	*Str = '\0';
	return Str;
}

FVertexShaderRHIRef FOpenGLDynamicRHI::RHICreateVertexShader(const TArray<uint8>& Code)
{
	return CompileOpenGLShader<FOpenGLVertexShader>(Code);
}

FPixelShaderRHIRef FOpenGLDynamicRHI::RHICreatePixelShader(const TArray<uint8>& Code)
{
	return CompileOpenGLShader<FOpenGLPixelShader>(Code);
}

FGeometryShaderRHIRef FOpenGLDynamicRHI::RHICreateGeometryShader(const TArray<uint8>& Code)
{
	return CompileOpenGLShader<FOpenGLGeometryShader>(Code);
}

FHullShaderRHIRef FOpenGLDynamicRHI::RHICreateHullShader(const TArray<uint8>& Code)
{
	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	return CompileOpenGLShader<FOpenGLHullShader>(Code);
}

FDomainShaderRHIRef FOpenGLDynamicRHI::RHICreateDomainShader(const TArray<uint8>& Code)
{
	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	return CompileOpenGLShader<FOpenGLDomainShader>(Code);
}

FGeometryShaderRHIRef FOpenGLDynamicRHI::RHICreateGeometryShaderWithStreamOutput(const TArray<uint8>& Code, const FStreamOutElementList& ElementList, uint32 NumStrides, const uint32* Strides, int32 RasterizedStream) 
{
	UE_LOG(LogRHI, Fatal,TEXT("OpenGL Render path does not support stream output!"));
	return NULL;
}


static void MarkShaderParameterCachesDirty(FOpenGLShaderParameterCache* ShaderParameters, bool UpdateCompute)
{
	const int32 StageStart = UpdateCompute  ? CrossCompiler::SHADER_STAGE_COMPUTE : CrossCompiler::SHADER_STAGE_VERTEX;
	const int32 StageEnd = UpdateCompute ? CrossCompiler::NUM_SHADER_STAGES : CrossCompiler::NUM_NON_COMPUTE_SHADER_STAGES;
	for (int32 Stage = StageStart; Stage < StageEnd; ++Stage)
	{
		ShaderParameters[Stage].MarkAllDirty();
	}
}

void FOpenGLDynamicRHI::BindUniformBufferBase(FOpenGLContextState& ContextState, int32 NumUniformBuffers, FUniformBufferRHIRef* BoundUniformBuffers, uint32 FirstUniformBuffer, bool ForceUpdate)
{
	SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLUniformBindTime);
	checkSlow(IsInRenderingThread());
	for (int32 BufferIndex = 0; BufferIndex < NumUniformBuffers; ++BufferIndex)
	{
		GLuint Buffer = 0;
		uint32 Offset = 0;
		uint32 Size = ZERO_FILLED_DUMMY_UNIFORM_BUFFER_SIZE;
		int32 BindIndex = FirstUniformBuffer + BufferIndex;
		if (IsValidRef(BoundUniformBuffers[BufferIndex]))
		{
			FRHIUniformBuffer* UB = BoundUniformBuffers[BufferIndex].GetReference();
			Buffer = ((FOpenGLUniformBuffer*)UB)->Resource;
			Size = ((FOpenGLUniformBuffer*)UB)->GetSize();
#if SUBALLOCATED_CONSTANT_BUFFER
			Offset = ((FOpenGLUniformBuffer*)UB)->Offset;
#endif
		}
		else
		{
			if (PendingState.ZeroFilledDummyUniformBuffer == 0)
			{
				void* ZeroBuffer = FMemory::Malloc(ZERO_FILLED_DUMMY_UNIFORM_BUFFER_SIZE);
				FMemory::Memzero(ZeroBuffer,ZERO_FILLED_DUMMY_UNIFORM_BUFFER_SIZE);
				FOpenGL::GenBuffers(1, &PendingState.ZeroFilledDummyUniformBuffer);
				check(PendingState.ZeroFilledDummyUniformBuffer != 0);
				CachedBindUniformBuffer(ContextState,PendingState.ZeroFilledDummyUniformBuffer);
				glBufferData(GL_UNIFORM_BUFFER, ZERO_FILLED_DUMMY_UNIFORM_BUFFER_SIZE, ZeroBuffer, GL_STATIC_DRAW);
				FMemory::Free(ZeroBuffer);
				IncrementBufferMemory(GL_UNIFORM_BUFFER, false, ZERO_FILLED_DUMMY_UNIFORM_BUFFER_SIZE);
			}

			Buffer = PendingState.ZeroFilledDummyUniformBuffer;
		}

		if (ForceUpdate || (Buffer != 0 && ContextState.UniformBuffers[BindIndex] != Buffer)|| ContextState.UniformBufferOffsets[BindIndex] != Offset)
		{
			FOpenGL::BindBufferRange(GL_UNIFORM_BUFFER, BindIndex, Buffer, Offset, Size);
			ContextState.UniformBuffers[BindIndex] = Buffer;
			ContextState.UniformBufferOffsets[BindIndex] = Offset;
			ContextState.UniformBufferBound = Buffer;	// yes, calling glBindBufferRange also changes uniform buffer binding.
		}
	}
}

// ============================================================================================================================

struct FOpenGLUniformName
{
	FOpenGLUniformName()
	{
		FMemory::Memzero(Buffer);
	}
	
	ANSICHAR Buffer[10];
	
	friend bool operator ==(const FOpenGLUniformName& A,const FOpenGLUniformName& B)
	{
		return FMemory::Memcmp(A.Buffer, B.Buffer, sizeof(A.Buffer)) == 0;
	}
	
	friend uint32 GetTypeHash(const FOpenGLUniformName &Key)
	{
		return FCrc::MemCrc32(Key.Buffer, sizeof(Key.Buffer));
	}
};

static TMap<GLuint, TMap<FOpenGLUniformName, int64>>& GetOpenGLUniformBlockLocations()
{
	static TMap<GLuint, TMap<FOpenGLUniformName, int64>> UniformBlockLocations;
	return UniformBlockLocations;
}

static TMap<GLuint, TMap<int64, int64>>& GetOpenGLUniformBlockBindings()
{
	static TMap<GLuint, TMap<int64, int64>> UniformBlockBindings;
	return UniformBlockBindings;
}

static GLuint GetOpenGLProgramUniformBlockIndex(GLuint Program, const FOpenGLUniformName& UniformBlockName)
{
	TMap<FOpenGLUniformName, int64>& Locations = GetOpenGLUniformBlockLocations().FindOrAdd(Program);
	int64* Location = Locations.Find(UniformBlockName);
	if(Location)
	{
		return *Location;
	}
	else
	{
		int64& Loc = Locations.Emplace(UniformBlockName);
		Loc = (int64)FOpenGL::GetUniformBlockIndex(Program, UniformBlockName.Buffer);
		return Loc;
	}
}

static void GetOpenGLProgramUniformBlockBinding(GLuint Program, GLuint UniformBlockIndex, GLuint UniformBlockBinding)
{
	TMap<int64, int64>& Bindings = GetOpenGLUniformBlockBindings().FindOrAdd(Program);
	int64* Bind = static_cast<int64 *>(Bindings.Find(UniformBlockIndex));
	if(!Bind)
	{
		Bind = &(Bindings.Emplace(UniformBlockIndex));
		check(Bind);
		*Bind = -1;
	}
	check(Bind);
	if(*Bind != static_cast<int64>(UniformBlockBinding))
	{
		*Bind = static_cast<int64>(UniformBlockBinding);
		FOpenGL::UniformBlockBinding(Program, UniformBlockIndex, UniformBlockBinding);
	}
}

// ============================================================================================================================

class FOpenGLLinkedProgram
{
public:
	FOpenGLLinkedProgramConfiguration Config;

	struct FPackedUniformInfo
	{
		GLint	Location;
		uint8	ArrayType;	// OGL_PACKED_ARRAYINDEX_TYPE
		uint8	Index;		// OGL_PACKED_INDEX_TYPE
	};

	// Holds information needed per stage regarding packed uniform globals and uniform buffers
	struct FStagePackedUniformInfo
	{
		// Packed Uniform Arrays (regular globals); array elements per precision/type
		TArray<FPackedUniformInfo>			PackedUniformInfos;

		// Packed Uniform Buffers; outer array is per Uniform Buffer; inner array is per precision/type
		TArray<TArray<FPackedUniformInfo>>	PackedUniformBufferInfos;

		// Holds the unique ID of the last uniform buffer uploaded to the program; since we don't reuse uniform buffers
		// (can't modify existing ones), we use this as a check for dirty/need to mem copy on Mobile
		TArray<uint32>						LastEmulatedUniformBufferSet;
	};
	FStagePackedUniformInfo	StagePackedUniformInfo[CrossCompiler::NUM_SHADER_STAGES];

	GLuint		Program;
	bool		bUsingTessellation;
	bool		bDrawn;

	TBitArray<>	TextureStageNeeds;
	TBitArray<>	UAVStageNeeds;
	int32		MaxTextureStage;

	TArray<FOpenGLBindlessSamplerInfo> Samplers;

	FOpenGLLinkedProgram()
	: Program(0), bUsingTessellation(false), bDrawn(false), MaxTextureStage(-1)
	{
		TextureStageNeeds.Init( false, FOpenGL::GetMaxCombinedTextureImageUnits() );
		UAVStageNeeds.Init( false, OGL_MAX_COMPUTE_STAGE_UAV_UNITS );
	}

	~FOpenGLLinkedProgram()
	{
		check(Program);
		FOpenGL::DeleteProgramPipelines(1, &Program);
		
		if (!FOpenGL::SupportsSeparateShaderObjects())
		{
			GetOpenGLUniformBlockLocations().Remove(Program);
			GetOpenGLUniformBlockBindings().Remove(Program);
		}
	}

	// Rebind the uniform blocks when changing the separable shader pipeline as different stages will have different uniform block arrangements. Does nothing for non-separable GLs.
	void VerifyUniformBlockBindings( int Stage, uint32 FirstUniformBuffer );

	void ConfigureShaderStage( int Stage, uint32 FirstUniformBuffer );

	// Make sure GlobalArrays (created from shader reflection) matches our info (from the cross compiler)
	static inline void SortPackedUniformInfos(const TArray<FPackedUniformInfo>& ReflectedUniformInfos, const TArray<CrossCompiler::FPackedArrayInfo>& PackedGlobalArrays, TArray<FPackedUniformInfo>& OutPackedUniformInfos)
	{
		check(OutPackedUniformInfos.Num() == 0);
		OutPackedUniformInfos.Empty(PackedGlobalArrays.Num());
		for (int32 Index = 0; Index < PackedGlobalArrays.Num(); ++Index)
		{
			auto& PackedArray = PackedGlobalArrays[Index];
			FPackedUniformInfo OutInfo = {-1, PackedArray.TypeName, CrossCompiler::PACKED_TYPEINDEX_MAX};

			// Find this Global Array in the reflection list
			for (int32 FindIndex = 0; FindIndex < ReflectedUniformInfos.Num(); ++FindIndex)
			{
				auto& ReflectedInfo = ReflectedUniformInfos[FindIndex];
				if (ReflectedInfo.ArrayType == PackedArray.TypeName)
				{
					OutInfo = ReflectedInfo;
					break;
				}
			}

			OutPackedUniformInfos.Add(OutInfo);
		}
	}
};

typedef TMap<FOpenGLLinkedProgramConfiguration,FOpenGLLinkedProgram*> FOpenGLProgramsForReuse;

static FOpenGLProgramsForReuse& GetOpenGLProgramsCache()
{
	static FOpenGLProgramsForReuse ProgramsCache;
	return ProgramsCache;
}

// This short queue preceding released programs cache is here because usually the programs are requested again
// very shortly after they're released, so looking through recently released programs first provides tangible
// performance improvement.

#define LAST_RELEASED_PROGRAMS_CACHE_COUNT 10

static FOpenGLLinkedProgram* StaticLastReleasedPrograms[LAST_RELEASED_PROGRAMS_CACHE_COUNT] = { 0 };
static int32 StaticLastReleasedProgramsIndex = 0;

// ============================================================================================================================

static int32 CountSetBits(const TBitArray<>& Array)
{
	int32 Result = 0;
	for (TBitArray<>::FConstIterator BitIt(Array); BitIt; ++BitIt)
	{
		Result += BitIt.GetValue();
	}
	return Result;
}

void FOpenGLLinkedProgram::VerifyUniformBlockBindings( int Stage, uint32 FirstUniformBuffer )
{
	if ( FOpenGL::SupportsSeparateShaderObjects() && FOpenGL::SupportsUniformBuffers() )
	{
		FOpenGLUniformName Name;
		Name.Buffer[0] = CrossCompiler::ShaderStageIndexToTypeName(Stage);
		Name.Buffer[1] = 'b';
		
		GLuint StageProgram = Config.Shaders[Stage].Resource;

		for (int32 BufferIndex = 0; BufferIndex < Config.Shaders[Stage].Bindings.NumUniformBuffers; ++BufferIndex)
		{
			SetIndex(Name.Buffer, 2, BufferIndex);
			GLint Location = GetOpenGLProgramUniformBlockIndex(StageProgram, Name);
			if (Location >= 0)
			{
				GetOpenGLProgramUniformBlockBinding(StageProgram, Location, FirstUniformBuffer + BufferIndex);
			}
		}
	}
}

void FOpenGLLinkedProgram::ConfigureShaderStage( int Stage, uint32 FirstUniformBuffer )
{
	static const GLint FirstTextureUnit[CrossCompiler::NUM_SHADER_STAGES] =
	{
		FOpenGL::GetFirstVertexTextureUnit(),
		FOpenGL::GetFirstPixelTextureUnit(),
		FOpenGL::GetFirstGeometryTextureUnit(),
		FOpenGL::GetFirstHullTextureUnit(),
		FOpenGL::GetFirstDomainTextureUnit(),
		FOpenGL::GetFirstComputeTextureUnit()
	};
	static const GLint FirstUAVUnit[CrossCompiler::NUM_SHADER_STAGES] =
	{
		OGL_UAV_NOT_SUPPORTED_FOR_GRAPHICS_UNIT,
		OGL_UAV_NOT_SUPPORTED_FOR_GRAPHICS_UNIT,
		OGL_UAV_NOT_SUPPORTED_FOR_GRAPHICS_UNIT,
		OGL_UAV_NOT_SUPPORTED_FOR_GRAPHICS_UNIT,
		OGL_UAV_NOT_SUPPORTED_FOR_GRAPHICS_UNIT,
		FOpenGL::GetFirstComputeUAVUnit()
	};
	
	// verify that only CS uses UAVs
	check((Stage != CrossCompiler::SHADER_STAGE_COMPUTE) ? (CountSetBits(UAVStageNeeds) == 0) : true);

	SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderBindParameterTime);
	VERIFY_GL_SCOPE();

	FOpenGLUniformName Name;
	Name.Buffer[0] = CrossCompiler::ShaderStageIndexToTypeName(Stage);

	GLuint StageProgram = FOpenGL::SupportsSeparateShaderObjects() ? Config.Shaders[Stage].Resource : Program;
	
	// Bind Global uniform arrays (vu_h, pu_i, etc)
	{
		Name.Buffer[1] = 'u';
		Name.Buffer[2] = '_';
		Name.Buffer[3] = 0;
		Name.Buffer[4] = 0;

		TArray<FPackedUniformInfo> PackedUniformInfos;
		for (uint8 Index = 0; Index < CrossCompiler::PACKED_TYPEINDEX_MAX; ++Index)
		{
			uint8 ArrayIndexType = CrossCompiler::PackedTypeIndexToTypeName(Index);
			Name.Buffer[3] = ArrayIndexType;
			GLint Location = glGetUniformLocation(StageProgram, Name.Buffer);
			if ((int32)Location != -1)
			{
				FPackedUniformInfo Info = {Location, ArrayIndexType, Index};
				PackedUniformInfos.Add(Info);
			}
		}

		SortPackedUniformInfos(PackedUniformInfos, Config.Shaders[Stage].Bindings.PackedGlobalArrays, StagePackedUniformInfo[Stage].PackedUniformInfos);
	}

	// Bind uniform buffer packed arrays (vc0_h, pc2_i, etc)
	{
		Name.Buffer[1] = 'c';
		Name.Buffer[2] = 0;
		Name.Buffer[3] = 0;
		Name.Buffer[4] = 0;
		Name.Buffer[5] = 0;
		Name.Buffer[6] = 0;
		for (uint8 UB = 0; UB < Config.Shaders[Stage].Bindings.NumUniformBuffers; ++UB)
		{
			TArray<FPackedUniformInfo> PackedBuffers;
			ANSICHAR* Str = SetIndex(Name.Buffer, 2, UB);
			*Str++ = '_';
			Str[1] = 0;
			for (uint8 Index = 0; Index < CrossCompiler::PACKED_TYPEINDEX_MAX; ++Index)
			{
				uint8 ArrayIndexType = CrossCompiler::PackedTypeIndexToTypeName(Index);
				Str[0] = ArrayIndexType;
				GLint Location = glGetUniformLocation(StageProgram, Name.Buffer);
				if ((int32)Location != -1)
				{
					FPackedUniformInfo Info = {Location, ArrayIndexType, Index};
					PackedBuffers.Add(Info);
				}
			}

			StagePackedUniformInfo[Stage].PackedUniformBufferInfos.Add(PackedBuffers);
		}
	}

	// Reserve and setup Space for Emulated Uniform Buffers
	StagePackedUniformInfo[Stage].LastEmulatedUniformBufferSet.Empty(Config.Shaders[Stage].Bindings.NumUniformBuffers);
	StagePackedUniformInfo[Stage].LastEmulatedUniformBufferSet.AddZeroed(Config.Shaders[Stage].Bindings.NumUniformBuffers);

	// Bind samplers.
	Name.Buffer[1] = 's';
	Name.Buffer[2] = 0;
	Name.Buffer[3] = 0;
	Name.Buffer[4] = 0;
	int32 LastFoundIndex = -1;
	for (int32 SamplerIndex = 0; SamplerIndex < Config.Shaders[Stage].Bindings.NumSamplers; ++SamplerIndex)
	{
		SetIndex(Name.Buffer, 2, SamplerIndex);
		GLint Location = glGetUniformLocation(StageProgram, Name.Buffer);
		if (Location == -1)
		{
			if (LastFoundIndex != -1)
			{
				// It may be an array of samplers. Get the initial element location, if available, and count from it.
				SetIndex(Name.Buffer, 2, LastFoundIndex);
				int32 OffsetOfArraySpecifier = (LastFoundIndex>9)?4:3;
				int32 ArrayIndex = SamplerIndex-LastFoundIndex;
				Name.Buffer[OffsetOfArraySpecifier] = '[';
				ANSICHAR* EndBracket = SetIndex(Name.Buffer, OffsetOfArraySpecifier+1, ArrayIndex);
				*EndBracket++ = ']';
				*EndBracket = 0;
				Location = glGetUniformLocation(StageProgram, Name.Buffer);
			}
		}
		else
		{
			LastFoundIndex = SamplerIndex;
		}

		if (Location != -1)
		{
			if ( OpenGLConsoleVariables::bBindlessTexture == 0 || !FOpenGL::SupportsBindlessTexture())
			{
				// Non-bindless, setup the unit info
				FOpenGL::ProgramUniform1i(StageProgram, Location, FirstTextureUnit[Stage] + SamplerIndex);
				TextureStageNeeds[ FirstTextureUnit[Stage] + SamplerIndex ] = true;
				MaxTextureStage = FMath::Max( MaxTextureStage, FirstTextureUnit[Stage] + SamplerIndex);
			}
			else
			{
				//Bindless, save off the slot information
				FOpenGLBindlessSamplerInfo Info;
				Info.Handle = Location;
				Info.Slot = FirstTextureUnit[Stage] + SamplerIndex;
				Samplers.Add(Info);
			}
		}
	}

	// Bind UAVs/images.
	Name.Buffer[1] = 'i';
	Name.Buffer[2] = 0;
	Name.Buffer[3] = 0;
	Name.Buffer[4] = 0;
	int32 LastFoundUAVIndex = -1;
	for (int32 UAVIndex = 0; UAVIndex < Config.Shaders[Stage].Bindings.NumUAVs; ++UAVIndex)
	{
		SetIndex(Name.Buffer, 2, UAVIndex);
		GLint Location = glGetUniformLocation(StageProgram, Name.Buffer);
		if (Location == -1)
		{
			if (LastFoundUAVIndex != -1)
			{
				// It may be an array of UAVs. Get the initial element location, if available, and count from it.
				SetIndex(Name.Buffer, 2, LastFoundUAVIndex);
				int32 OffsetOfArraySpecifier = (LastFoundUAVIndex>9)?4:3;
				int32 ArrayIndex = UAVIndex-LastFoundUAVIndex;
				Name.Buffer[OffsetOfArraySpecifier] = '[';
				ANSICHAR* EndBracket = SetIndex(Name.Buffer, OffsetOfArraySpecifier+1, ArrayIndex);
				*EndBracket++ = ']';
				*EndBracket = '\0';
				Location = glGetUniformLocation(StageProgram, Name.Buffer);
			}
		}
		else
		{
			LastFoundUAVIndex = UAVIndex;
		}

		if (Location != -1)
		{
			// compute shaders have layout(binding) for images 
			// glUniform1i(Location, FirstUAVUnit[Stage] + UAVIndex);
			
			UAVStageNeeds[ FirstUAVUnit[Stage] + UAVIndex ] = true;
		}
	}

	// Bind uniform buffers.
	if (FOpenGL::SupportsUniformBuffers())
	{
		Name.Buffer[1] = 'b';
		Name.Buffer[2] = 0;
		Name.Buffer[3] = 0;
		Name.Buffer[4] = 0;
		for (int32 BufferIndex = 0; BufferIndex < Config.Shaders[Stage].Bindings.NumUniformBuffers; ++BufferIndex)
		{
			SetIndex(Name.Buffer, 2, BufferIndex);
			GLint Location = GetOpenGLProgramUniformBlockIndex(StageProgram, Name);
			if (Location >= 0)
			{
				GetOpenGLProgramUniformBlockBinding(StageProgram, Location, FirstUniformBuffer + BufferIndex);
			}
		}
	}
}

#if ENABLE_UNIFORM_BUFFER_LAYOUT_VERIFICATION

#define ENABLE_UNIFORM_BUFFER_LAYOUT_NAME_MANGLING_CL1862097 1
/*
	As of CL 1862097 uniform buffer names are mangled to avoid collisions between variables referenced 
	in different shaders of the same program

	layout(std140) uniform _vb0
	{
	#define View View_vb0
	anon_struct_0000 View;
	};

	layout(std140) uniform _vb1
	{
	#define Primitive Primitive_vb1
	anon_struct_0001 Primitive;
	};
*/
	

struct UniformData
{
	UniformData(uint32 InOffset, uint32 InArrayElements)
		: Offset(InOffset)
		, ArrayElements(InArrayElements)
	{
	}
	uint32 Offset;
	uint32 ArrayElements;

	bool operator == (const UniformData& RHS) const
	{
		return	Offset == RHS.Offset &&	ArrayElements == RHS.ArrayElements;
	}
	bool operator != (const UniformData& RHS) const
	{
		return	!(*this == RHS);
	}
};
#if ENABLE_UNIFORM_BUFFER_LAYOUT_NAME_MANGLING_CL1862097
static void VerifyUniformLayout(const FString& BlockName, const TCHAR* UniformName, const UniformData& GLSLUniform)
#else
static void VerifyUniformLayout(const TCHAR* UniformName, const UniformData& GLSLUniform)
#endif //#if ENABLE_UNIFORM_BUFFER_LAYOUT_NAME_MANGLING_CL1862097
{
	static TMap<FString, UniformData> Uniforms;

	if(!Uniforms.Num())
	{
		for (TLinkedList<FUniformBufferStruct*>::TIterator StructIt(FUniformBufferStruct::GetStructList()); StructIt; StructIt.Next())
		{
#if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
			UE_LOG(LogRHI, Log, TEXT("UniformBufferStruct %s %s %d"),
				StructIt->GetStructTypeName(),
				StructIt->GetShaderVariableName(),
				StructIt->GetSize()
				);
#endif  // #if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
			const TArray<FUniformBufferStruct::FMember>& StructMembers = StructIt->GetMembers();
			for(int32 MemberIndex = 0;MemberIndex < StructMembers.Num();++MemberIndex)
			{
				const FUniformBufferStruct::FMember& Member = StructMembers[MemberIndex];

				FString BaseTypeName;
				switch(Member.GetBaseType())
				{
					case UBMT_STRUCT:  BaseTypeName = TEXT("struct");  break;
					case UBMT_BOOL:    BaseTypeName = TEXT("bool"); break;
					case UBMT_INT32:   BaseTypeName = TEXT("int"); break;
					case UBMT_UINT32:  BaseTypeName = TEXT("uint"); break;
					case UBMT_FLOAT32: BaseTypeName = TEXT("float"); break;
					case UBMT_TEXTURE: BaseTypeName = TEXT("texture"); break;
					case UBMT_SAMPLER: BaseTypeName = TEXT("sampler"); break;
					default:           UE_LOG(LogShaders, Fatal,TEXT("Unrecognized uniform buffer struct member base type."));
				};
#if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
				UE_LOG(LogRHI, Log, TEXT("  +%d %s%dx%d %s[%d]"),
					Member.GetOffset(),
					*BaseTypeName,
					Member.GetNumRows(),
					Member.GetNumColumns(),
					Member.GetName(),
					Member.GetNumElements()
					);
#endif // #if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
				FString CompositeName = FString(StructIt->GetShaderVariableName()) + TEXT("_") + Member.GetName();

				// GLSL returns array members with a "[0]" suffix
				if(Member.GetNumElements())
				{
					CompositeName += TEXT("[0]");
				}

				check(!Uniforms.Contains(CompositeName));
				Uniforms.Add(CompositeName, UniformData(Member.GetOffset(), Member.GetNumElements()));
			}
		}
	}

#if ENABLE_UNIFORM_BUFFER_LAYOUT_NAME_MANGLING_CL1862097
	/* unmangle the uniform name by stripping the block name from it
	
	layout(std140) uniform _vb0
	{
	#define View View_vb0
		anon_struct_0000 View;
	};
	*/
	FString RequestedUniformName(UniformName);
	RequestedUniformName = RequestedUniformName.Replace(*BlockName, TEXT(""));
	if(RequestedUniformName.StartsWith(TEXT(".")))
	{
		RequestedUniformName = RequestedUniformName.RightChop(1);
	}
#else
	FString RequestedUniformName = UniformName;
#endif

	const UniformData* FoundUniform = Uniforms.Find(RequestedUniformName);

	// MaterialTemplate uniform buffer does not have an entry in the FUniformBufferStructs list, so skipping it here
	if(!(RequestedUniformName.StartsWith("Material_") || RequestedUniformName.StartsWith("MaterialCollection")))
	{
		if(!FoundUniform || (*FoundUniform != GLSLUniform))
		{
			UE_LOG(LogRHI, Fatal, TEXT("uniform buffer member %s in the GLSL source doesn't match it's declaration in it's FUniformBufferStruct"), *RequestedUniformName);
		}
	}
}

static void VerifyUniformBufferLayouts(GLuint Program)
{
	GLint NumBlocks = 0;
	glGetProgramiv(Program, GL_ACTIVE_UNIFORM_BLOCKS, &NumBlocks);

#if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
	UE_LOG(LogRHI, Log, TEXT("program %d has %d uniform blocks"), Program, NumBlocks);
#endif  // #if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP

	for(GLint BlockIndex = 0; BlockIndex < NumBlocks; ++BlockIndex)
	{
		const GLsizei BufferSize = 256;
		char Buffer[BufferSize] = {0};
		GLsizei Length = 0;

		GLint ActiveUniforms = 0;
		GLint BlockBytes = 0;

		glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &ActiveUniforms);
		glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &BlockBytes);
		glGetActiveUniformBlockName(Program, BlockIndex, BufferSize, &Length, Buffer);

#if ENABLE_UNIFORM_BUFFER_LAYOUT_NAME_MANGLING_CL1862097
		FString BlockName(Buffer);
#endif // #if ENABLE_UNIFORM_BUFFER_LAYOUT_NAME_MANGLING_CL1862097

		FString ReferencedBy;
		{
			GLint ReferencedByVS = 0;
			GLint ReferencedByPS = 0;
			GLint ReferencedByGS = 0;
			GLint ReferencedByHS = 0;
			GLint ReferencedByDS = 0;
			GLint ReferencedByCS = 0;

			glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER, &ReferencedByVS);
			glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER, &ReferencedByPS);
#ifdef GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER
			glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_REFERENCED_BY_GEOMETRY_SHADER, &ReferencedByGS);
#endif
			if (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5)
			{
#ifdef GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_CONTROL_SHADER
				glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_CONTROL_SHADER, &ReferencedByHS);
				glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_REFERENCED_BY_TESS_EVALUATION_SHADER, &ReferencedByDS);
#endif
#ifdef GL_UNIFORM_BLOCK_REFERENCED_BY_COMPUTE_SHADER 
				glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_REFERENCED_BY_COMPUTE_SHADER, &ReferencedByCS);
#endif
			}

			if(ReferencedByVS) {ReferencedBy += TEXT("V");}
			if(ReferencedByHS) {ReferencedBy += TEXT("H");}
			if(ReferencedByDS) {ReferencedBy += TEXT("D");}
			if(ReferencedByGS) {ReferencedBy += TEXT("G");}
			if(ReferencedByPS) {ReferencedBy += TEXT("P");}
			if(ReferencedByCS) {ReferencedBy += TEXT("C");}
		}
#if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
		UE_LOG(LogRHI, Log, TEXT("  [%d] uniform block (%s) = %s, %d active uniforms, %d bytes {"),
			BlockIndex, 
			*ReferencedBy,
			ANSI_TO_TCHAR(Buffer),
			ActiveUniforms, 
			BlockBytes
			); 
#endif // #if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
		if(ActiveUniforms)
		{
			// the other TArrays copy construct this to get the proper array size
			TArray<GLint> ActiveUniformIndices;
			ActiveUniformIndices.Init(ActiveUniforms);

			glGetActiveUniformBlockiv(Program, BlockIndex, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, ActiveUniformIndices.GetData());
			
			TArray<GLint> ActiveUniformOffsets(ActiveUniformIndices);
			glGetActiveUniformsiv(Program, ActiveUniforms, reinterpret_cast<const GLuint*>(ActiveUniformIndices.GetData()), GL_UNIFORM_OFFSET, ActiveUniformOffsets.GetData());

			TArray<GLint> ActiveUniformSizes(ActiveUniformIndices);
			glGetActiveUniformsiv(Program, ActiveUniforms, reinterpret_cast<const GLuint*>(ActiveUniformIndices.GetData()), GL_UNIFORM_SIZE, ActiveUniformSizes.GetData());

			TArray<GLint> ActiveUniformTypes(ActiveUniformIndices);
			glGetActiveUniformsiv(Program, ActiveUniforms, reinterpret_cast<const GLuint*>(ActiveUniformIndices.GetData()), GL_UNIFORM_TYPE, ActiveUniformTypes.GetData());

			TArray<GLint> ActiveUniformArrayStrides(ActiveUniformIndices);
			glGetActiveUniformsiv(Program, ActiveUniforms, reinterpret_cast<const GLuint*>(ActiveUniformIndices.GetData()), GL_UNIFORM_ARRAY_STRIDE, ActiveUniformArrayStrides.GetData());

			extern const TCHAR* GetGLUniformTypeString( GLint UniformType );

			for(GLint i = 0; i < ActiveUniformIndices.Num(); ++i)
			{
				const GLint UniformIndex = ActiveUniformIndices[i];
				GLsizei Size = 0;
				GLenum Type = 0;
				glGetActiveUniform(Program, UniformIndex , BufferSize, &Length, &Size, &Type, Buffer);

#if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP				
				UE_LOG(LogRHI, Log, TEXT("    [%d] +%d %s %s %d elements %d array stride"),
					UniformIndex,
					ActiveUniformOffsets[i],
					GetGLUniformTypeString(ActiveUniformTypes[i]),
					ANSI_TO_TCHAR(Buffer),
					ActiveUniformSizes[i],
					ActiveUniformArrayStrides[i]
				); 
#endif // #if ENABLE_UNIFORM_BUFFER_LAYOUT_DUMP
		
				const UniformData GLSLUniform
				(
					ActiveUniformOffsets[i], 
					ActiveUniformArrayStrides[i] > 0 ? ActiveUniformSizes[i] : 0 // GLSL has 1 as array size for non-array uniforms, but FUniformBufferStruct assumes 0
				);
#if ENABLE_UNIFORM_BUFFER_LAYOUT_NAME_MANGLING_CL1862097
				VerifyUniformLayout(BlockName, ANSI_TO_TCHAR(Buffer), GLSLUniform);
#else
				VerifyUniformLayout(ANSI_TO_TCHAR(Buffer), GLSLUniform);
#endif
			}
		}
	}
}
#endif  // #if ENABLE_UNIFORM_BUFFER_LAYOUT_VERIFICATION

/**
 * Link vertex and pixel shaders in to an OpenGL program.
 */
static FOpenGLLinkedProgram* LinkProgram( const FOpenGLLinkedProgramConfiguration& Config)
{
	ANSICHAR Buf[32] = {0};

	SCOPE_CYCLE_COUNTER(STAT_OpenGLShaderLinkTime);
	VERIFY_GL_SCOPE();

	// ensure that compute shaders are always alone
	check( (Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Resource == 0) != (Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Resource == 0));
	check( (Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Resource == 0) != (Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Resource == 0));

	GLuint Program = 0;
	FOpenGL::GenProgramPipelines(1, &Program);

	bool bShouldLinkProgram = true;
	if (FOpenGLProgramBinaryCache::IsEnabled())
	{
		// Try to create program from a saved binary
		bShouldLinkProgram = !FOpenGLProgramBinaryCache::UseCachedProgram(Program, Config);
		if (bShouldLinkProgram)
		{
			// In case there is no saved binary in the cache, compile required shaders we have deferred before
			FOpenGLProgramBinaryCache::CompilePendingShaders(Config);
		}
	}

	if (bShouldLinkProgram)
	{
		if (Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Resource)
		{
			FOpenGL::UseProgramStages(Program, GL_VERTEX_SHADER_BIT, Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Resource);
		}
		if (Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Resource)
		{
			FOpenGL::UseProgramStages(Program, GL_FRAGMENT_SHADER_BIT, Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Resource);
		}
		if (Config.Shaders[CrossCompiler::SHADER_STAGE_GEOMETRY].Resource)
		{
			FOpenGL::UseProgramStages(Program, GL_GEOMETRY_SHADER_BIT, Config.Shaders[CrossCompiler::SHADER_STAGE_GEOMETRY].Resource);
		}
		if (Config.Shaders[CrossCompiler::SHADER_STAGE_HULL].Resource)
		{
			FOpenGL::UseProgramStages(Program, GL_TESS_CONTROL_SHADER_BIT, Config.Shaders[CrossCompiler::SHADER_STAGE_HULL].Resource);
		}
		if (Config.Shaders[CrossCompiler::SHADER_STAGE_DOMAIN].Resource)
		{
			FOpenGL::UseProgramStages(Program, GL_TESS_EVALUATION_SHADER_BIT, Config.Shaders[CrossCompiler::SHADER_STAGE_DOMAIN].Resource);
		}
		if (Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Resource)
		{
			FOpenGL::UseProgramStages(Program, GL_COMPUTE_SHADER_BIT, Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Resource);
		}
	
		if( !FOpenGL::SupportsSeparateShaderObjects() )
		{
			// E.g. GLSL_430 uses layout(location=xx) instead of having to call glBindAttribLocation and glBindFragDataLocation
			if (OpenGLShaderPlatformNeedsBindLocation(GMaxRHIShaderPlatform))
			{
				// Bind attribute indices.
				if (Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Resource)
				{
					auto& VertexBindings = Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Bindings;
					BindShaderLocations(GL_VERTEX_SHADER, Program, VertexBindings.InOutMask, VertexBindings.VertexAttributeRemap);
				}

				// Bind frag data locations.
				if (Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Resource)
				{
					BindShaderLocations(GL_FRAGMENT_SHADER, Program, Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Bindings.InOutMask);
				}
			}
		
			// Link.
			glLinkProgram(Program);

			if (FOpenGLProgramBinaryCache::IsEnabled())
			{
				FOpenGLProgramBinaryCache::CacheProgram(Program, Config);
			}
		}
	}
	
	if (!VerifyProgramPipeline(Program))
	{
		return nullptr;
	}
	
	FOpenGL::BindProgramPipeline(Program);

	FOpenGLLinkedProgram* LinkedProgram = new FOpenGLLinkedProgram;
	LinkedProgram->Config = Config;
	LinkedProgram->Program = Program;
	LinkedProgram->bUsingTessellation = Config.Shaders[CrossCompiler::SHADER_STAGE_HULL].Resource && Config.Shaders[CrossCompiler::SHADER_STAGE_DOMAIN].Resource;

	if (Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Resource)
	{
		LinkedProgram->ConfigureShaderStage(
			CrossCompiler::SHADER_STAGE_VERTEX,
			OGL_FIRST_UNIFORM_BUFFER
			);
		check(LinkedProgram->StagePackedUniformInfo[CrossCompiler::SHADER_STAGE_VERTEX].PackedUniformInfos.Num() <= Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Bindings.PackedGlobalArrays.Num());
	}

	if (Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Resource)
	{
		LinkedProgram->ConfigureShaderStage(
			CrossCompiler::SHADER_STAGE_PIXEL,
			OGL_FIRST_UNIFORM_BUFFER +
			Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Bindings.NumUniformBuffers
			);
		check(LinkedProgram->StagePackedUniformInfo[CrossCompiler::SHADER_STAGE_PIXEL].PackedUniformInfos.Num() <= Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Bindings.PackedGlobalArrays.Num());
	}

	if (Config.Shaders[CrossCompiler::SHADER_STAGE_GEOMETRY].Resource)
	{
		LinkedProgram->ConfigureShaderStage(
			CrossCompiler::SHADER_STAGE_GEOMETRY,
			OGL_FIRST_UNIFORM_BUFFER +
			Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Bindings.NumUniformBuffers +
			Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Bindings.NumUniformBuffers
			);
		check(LinkedProgram->StagePackedUniformInfo[CrossCompiler::SHADER_STAGE_GEOMETRY].PackedUniformInfos.Num() <= Config.Shaders[CrossCompiler::SHADER_STAGE_GEOMETRY].Bindings.PackedGlobalArrays.Num());
	}

	if (Config.Shaders[CrossCompiler::SHADER_STAGE_HULL].Resource)
	{
		LinkedProgram->ConfigureShaderStage(
			CrossCompiler::SHADER_STAGE_HULL,
			OGL_FIRST_UNIFORM_BUFFER +
			Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Bindings.NumUniformBuffers +
			Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Bindings.NumUniformBuffers +
			Config.Shaders[CrossCompiler::SHADER_STAGE_GEOMETRY].Bindings.NumUniformBuffers
		);
	}

	if (Config.Shaders[CrossCompiler::SHADER_STAGE_DOMAIN].Resource)
	{
		LinkedProgram->ConfigureShaderStage(
			CrossCompiler::SHADER_STAGE_DOMAIN,
			OGL_FIRST_UNIFORM_BUFFER +
			Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Bindings.NumUniformBuffers +
			Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL].Bindings.NumUniformBuffers +
			Config.Shaders[CrossCompiler::SHADER_STAGE_GEOMETRY].Bindings.NumUniformBuffers +
			Config.Shaders[CrossCompiler::SHADER_STAGE_HULL].Bindings.NumUniformBuffers
		);
	}

	if (Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Resource)
	{
		LinkedProgram->ConfigureShaderStage(
			CrossCompiler::SHADER_STAGE_COMPUTE,
			OGL_FIRST_UNIFORM_BUFFER
			);
		check(LinkedProgram->StagePackedUniformInfo[CrossCompiler::SHADER_STAGE_COMPUTE].PackedUniformInfos.Num() <= Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Bindings.PackedGlobalArrays.Num());
	}
#if ENABLE_UNIFORM_BUFFER_LAYOUT_VERIFICATION	
	VerifyUniformBufferLayouts(Program);
#endif // #if ENABLE_UNIFORM_BUFFER_LAYOUT_VERIFICATION
	return LinkedProgram;
}

FComputeShaderRHIRef FOpenGLDynamicRHI::RHICreateComputeShader(const TArray<uint8>& Code)
{
	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM5);
	
	FOpenGLComputeShader* ComputeShader = CompileOpenGLShader<FOpenGLComputeShader>(Code);
	const ANSICHAR* GlslCode = NULL;
	if (!ComputeShader->bSuccessfullyCompiled)
	{
#if DEBUG_GL_SHADERS
		GlslCode = ComputeShader->GlslCodeString;
#endif
		ComputeShader->bSuccessfullyCompiled = VerifyCompiledShader(ComputeShader->Resource, GlslCode);
	}

	check( ComputeShader != 0);

	// @todo WARNING: We have to hash here because of the way we immediately link and don't afford the cache a chance to set the OutputHash from ShaderCore.
	if (FShaderCache::GetShaderCache())
	{
		FSHAHash Hash;
		FSHA1::HashBuffer(Code.GetData(), Code.Num(), Hash.Hash);
		ComputeShader->SetHash(Hash);
	}

	FOpenGLLinkedProgramConfiguration Config;

	Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Resource = ComputeShader->Resource;
	Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Hash = ComputeShader->GetHash();
	Config.Shaders[CrossCompiler::SHADER_STAGE_COMPUTE].Bindings = ComputeShader->Bindings;

	ComputeShader->LinkedProgram = LinkProgram( Config );

	if (ComputeShader->LinkedProgram == NULL)
	{
#if DEBUG_GL_SHADERS
		if (ComputeShader->bSuccessfullyCompiled)
		{
			UE_LOG(LogRHI,Error,TEXT("Compute Shader:\n%s"),ANSI_TO_TCHAR(ComputeShader->GlslCode.GetData()));
		}
#endif //DEBUG_GL_SHADERS
		checkf(ComputeShader->LinkedProgram, TEXT("Compute shader failed to compile & link."));
	}

	return ComputeShader;
}

template<class TOpenGLStage>
static FString GetShaderStageSource(TOpenGLStage* Shader)
{
	FString Source;
#if DEBUG_GL_SHADERS
	Source = Shader->GlslCodeString;
#else
	GLsizei NumShaders = 0;
	glGetProgramiv(Shader->Resource, GL_ATTACHED_SHADERS, (GLint*)&NumShaders);
	if(NumShaders > 0)
	{
		GLuint* Shaders = (GLuint*)alloca(sizeof(GLuint)*NumShaders);
		glGetAttachedShaders(Shader->Resource, NumShaders, &NumShaders, Shaders);
		for(int32 i = 0; i < NumShaders; i++)
		{
			GLint Len = 0;
			glGetShaderiv(Shaders[i], GL_SHADER_SOURCE_LENGTH, &Len);
			if(Len > 0)
			{
				ANSICHAR* Code = new ANSICHAR[Len + 1];
				glGetShaderSource(Shaders[i], Len + 1, &Len, Code);
				Source += Code;
				delete [] Code;
			}
		}
	}
#endif
	return Source;
}

// ============================================================================================================================

struct FOpenGLShaderVaryingMapping
{
	FAnsiCharArray Name;
	int32 WriteLoc;
	int32 ReadLoc;
};

typedef TMap<FOpenGLLinkedProgramConfiguration,FOpenGLLinkedProgramConfiguration::ShaderInfo> FOpenGLSeparateShaderObjectCache;

static FOpenGLSeparateShaderObjectCache& GetOpenGLSeparateShaderObjectCache()
{
	static FOpenGLSeparateShaderObjectCache SeparateShaderObjectCache;
	return SeparateShaderObjectCache;
}

template<class TOpenGLStage0, class TOpenGLStage1>
static void BindShaderStage(FOpenGLLinkedProgramConfiguration::ShaderInfo& ShaderInfo, TOpenGLStage0* NextStage, FOpenGLLinkedProgramConfiguration::ShaderInfo& PrevInfo, TOpenGLStage1* PrevStage)
{
    check(NextStage && PrevStage);
    
    GLuint NextStageResource = NextStage->Resource;
    FOpenGLShaderBindings NextStageBindings = NextStage->Bindings;
    
    if ( FOpenGL::SupportsSeparateShaderObjects() )
    {
		FOpenGLLinkedProgramConfiguration Config;
		Config.Shaders[0] = PrevInfo;
		Config.Shaders[1] = ShaderInfo;
		FOpenGLLinkedProgramConfiguration::ShaderInfo* PrevResource = GetOpenGLSeparateShaderObjectCache().Find(Config);
		if(PrevResource)
		{
			PrevInfo.Bindings = PrevResource->Bindings;
			PrevInfo.Resource = PrevResource->Resource;
		}
		else
		{
			FOpenGLShaderBindings& PrevStageBindings = PrevStage->Bindings;
			TMap<FAnsiCharArray, int32> PrevStageVaryings;
			for (int32 i = 0; i < PrevStageBindings.OutputVaryings.Num(); i++)
			{
				FAnsiCharArray Name = PrevStageBindings.OutputVaryings[i].Varying;
				if ( Name.Num() >= 4 && (FCStringAnsi::Strncmp(Name.GetData(), "out_", 4) == 0 || FCStringAnsi::Strncmp(Name.GetData(), "var_", 4) == 0) )
				{
					Name.RemoveAt(0, 4);
				}
				PrevStageVaryings.Add(Name, PrevStageBindings.OutputVaryings[i].Location);
			}
			
			bool bInterpolatorMatches = true;
			
			TMap<FAnsiCharArray, int32> NextStageVaryings;
			TArray<FString> InputErrors;
			TArray<FOpenGLShaderVaryingMapping> VaryingMapping;
			for (int32 i = 0; i < NextStageBindings.InputVaryings.Num(); i++)
			{
				FAnsiCharArray Name = NextStageBindings.InputVaryings[i].Varying;
				if ( Name.Num() >= 3 && FCStringAnsi::Strncmp(Name.GetData(), "in_", 3) == 0 )
				{
					Name.RemoveAt(0, 3);
				}
				if ( Name.Num() >= 4 && FCStringAnsi::Strncmp(Name.GetData(), "var_", 4) == 0 )
				{
					Name.RemoveAt(0, 4);
				}
				NextStageVaryings.Add(Name, NextStageBindings.InputVaryings[i].Location);
				if( PrevStageVaryings.Contains(Name) )
				{
					int32& PrevLocation = PrevStageVaryings.FindChecked(Name);
					if(PrevLocation != NextStageBindings.InputVaryings[i].Location)
					{
						if(PrevLocation >= 0 && NextStageBindings.InputVaryings[i].Location >= 0)
						{
							FOpenGLShaderVaryingMapping Pair;
							Pair.Name = Name;
							Pair.WriteLoc = PrevLocation;
							Pair.ReadLoc = NextStageBindings.InputVaryings[i].Location;
							VaryingMapping.Add(Pair);
							UE_LOG(LogRHI,Warning,TEXT("Separate Shader Object Binding Warning: Input %s @ %d of stage 0x%x written by stage 0x%x at wrong location %d"), ANSI_TO_TCHAR(NextStageBindings.InputVaryings[i].Varying.GetData()), NextStageBindings.InputVaryings[i].Location, TOpenGLStage0::TypeEnum, TOpenGLStage1::TypeEnum, PrevLocation);
						}
						else if(NextStageBindings.InputVaryings[i].Location == -1)
						{
							InputErrors.Add(FString::Printf(TEXT("Separate Shader Object Binding Error: Input %s of stage 0x%x written by stage 0x%x at location %d, can't be rewritten."), ANSI_TO_TCHAR(NextStageBindings.InputVaryings[i].Varying.GetData()), TOpenGLStage0::TypeEnum, TOpenGLStage1::TypeEnum, PrevLocation));
											}
						else
						{
							InputErrors.Add(FString::Printf(TEXT("Separate Shader Object Binding Error: Input %s @ %d of stage 0x%x written by stage 0x%x without location, can't be rewritten."), ANSI_TO_TCHAR(NextStageBindings.InputVaryings[i].Varying.GetData()), NextStageBindings.InputVaryings[i].Location, TOpenGLStage0::TypeEnum, TOpenGLStage1::TypeEnum));
						}
						bInterpolatorMatches = false;
					}
				}
				else
				{
					InputErrors.Add(FString::Printf(TEXT("Separate Shader Object Binding Error: Input %s @ %d of stage 0x%x not written by stage 0x%x"), ANSI_TO_TCHAR(NextStageBindings.InputVaryings[i].Varying.GetData()), NextStageBindings.InputVaryings[i].Location, TOpenGLStage0::TypeEnum, TOpenGLStage1::TypeEnum));
					bInterpolatorMatches = false;
				}
			}
			
			TArray<FOpenGLShaderVarying> OutputElimination;
			for (int32 i = 0; i < PrevStageBindings.OutputVaryings.Num(); i++)
			{
				if ( PrevStageBindings.OutputVaryings[i].Location == -1 )
				{
					FAnsiCharArray Name = PrevStageBindings.OutputVaryings[i].Varying;
					if ( Name.Num() >= 4 && (FCStringAnsi::Strncmp(Name.GetData(), "out_", 4) == 0 || FCStringAnsi::Strncmp(Name.GetData(), "var_", 4) == 0) )
					{
						Name.RemoveAt(0, 4);
					}
					if( !NextStageVaryings.Contains(Name) )
					{
						OutputElimination.Add(PrevStageBindings.OutputVaryings[i]);
						UE_LOG(LogRHI,Warning,TEXT("Separate Shader Object Binding Warning: Named output %s of stage 0x%x not read by stage 0x%x"), ANSI_TO_TCHAR(PrevStageBindings.OutputVaryings[i].Varying.GetData()), TOpenGLStage1::TypeEnum, TOpenGLStage0::TypeEnum);
						bInterpolatorMatches = false;
					}
				}
			}
		
			if(!bInterpolatorMatches)
			{
				if(InputErrors.Num() == 0)
				{
					FOpenGLCodeHeader Header;
					Header.GlslMarker = 0x474c534c;
					CA_SUPPRESS(6326);
					switch ((int32)TOpenGLStage1::StaticFrequency)
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
							UE_LOG(LogRHI, Fatal, TEXT("Invalid shader frequency: %d"), (int32)TOpenGLStage1::StaticFrequency);
					}
					Header.Bindings = PrevStage->Bindings;
					Header.UniformBuffersCopyInfo = PrevStage->UniformBuffersCopyInfo;
					
					TArray<FString> PrevLines;
					FString PrevSource = GetShaderStageSource<TOpenGLStage1>(PrevStage);
					PrevSource.ParseIntoArrayLines(PrevLines);
					bool const bOutputElimination = OutputElimination.Num() > 0;
					for(FOpenGLShaderVarying Output : OutputElimination)
					{
						for(int32 i = 0; i < PrevLines.Num(); i++)
						{
							if(PrevLines[i].Contains(Output.Varying.GetData()))
							{
								PrevLines[i].Empty();
							}
						}
						for(int32 i = 0; i < Header.Bindings.OutputVaryings.Num(); i++)
						{
							if(Output == Header.Bindings.OutputVaryings[i])
							{
								Header.Bindings.OutputVaryings.RemoveAt(i);
								break;
							}
						}
					}
					OutputElimination.Empty();
					
					bool const bVaryingRemapping = VaryingMapping.Num() > 0;
					
					if (OutputElimination.Num() == 0 && VaryingMapping.Num() == 0 && (bOutputElimination || bVaryingRemapping))
					{
						FString NewPrevSource;
						for(FString Line : PrevLines)
						{
							if(!Line.IsEmpty())
							{
								NewPrevSource += Line + TEXT("\n");
							}
						}
						
						TArray<uint8> Bytes;
						FMemoryWriter Ar(Bytes);
						Ar << Header;
						TArray<ANSICHAR> Chars;
						int32 Len = FCStringAnsi::Strlen(TCHAR_TO_ANSI(*NewPrevSource)) + 1;
						Chars.Append(TCHAR_TO_ANSI(*NewPrevSource), Len);
						Ar.Serialize(Chars.GetData(), Chars.Num());
						
						TRefCountPtr<TOpenGLStage1> NewPrev(CompileOpenGLShader<TOpenGLStage1>(Bytes));
						PrevInfo.Bindings = Header.Bindings;
						PrevInfo.Resource = NewPrev->Resource;
					}
					
					bInterpolatorMatches = (OutputElimination.Num() == 0 && VaryingMapping.Num() == 0);
				}
				else
				{
					for(int32 i = 0; i < InputErrors.Num(); i++)
					{
						UE_LOG(LogRHI, Error, TEXT("%s"), *InputErrors[i]);
					}
				}
				
				if(!bInterpolatorMatches)
				{
					FString PrevShaderStageSource = GetShaderStageSource<TOpenGLStage1>(PrevStage);
					FString NextShaderStageSource = GetShaderStageSource<TOpenGLStage0>(NextStage);
					UE_LOG(LogRHI, Error, TEXT("Separate Shader Object Stage 0x%x:\n%s"), TOpenGLStage1::TypeEnum, *PrevShaderStageSource);
					UE_LOG(LogRHI, Error, TEXT("Separate Shader Object Stage 0x%x:\n%s"), TOpenGLStage0::TypeEnum, *NextShaderStageSource);
				}
			}
			
			GetOpenGLSeparateShaderObjectCache().Add(Config, PrevInfo);
		}
    }
    
    ShaderInfo.Bindings = NextStageBindings;
    ShaderInfo.Resource = NextStageResource;
    ShaderInfo.Hash = NextStage->GetHash();
}

// ============================================================================================================================

FBoundShaderStateRHIRef FOpenGLDynamicRHI::RHICreateBoundShaderState(
	FVertexDeclarationRHIParamRef VertexDeclarationRHI, 
	FVertexShaderRHIParamRef VertexShaderRHI, 
	FHullShaderRHIParamRef HullShaderRHI,
	FDomainShaderRHIParamRef DomainShaderRHI, 
	FPixelShaderRHIParamRef PixelShaderRHI, 
	FGeometryShaderRHIParamRef GeometryShaderRHI
	)
{ 
	check(IsInRenderingThread());

	VERIFY_GL_SCOPE();

	SCOPE_CYCLE_COUNTER(STAT_OpenGLCreateBoundShaderStateTime);

	if(!PixelShaderRHI)
	{
		// use special null pixel shader when PixelShader was set to NULL
		PixelShaderRHI = TShaderMapRef<FNULLPS>(GetGlobalShaderMap(GMaxRHIFeatureLevel))->GetPixelShader();
	}

	// Check for an existing bound shader state which matches the parameters
	FCachedBoundShaderStateLink* CachedBoundShaderStateLink = GetCachedBoundShaderState(
		VertexDeclarationRHI,
		VertexShaderRHI,
		PixelShaderRHI,
		HullShaderRHI,
		DomainShaderRHI,
		GeometryShaderRHI
		);

	if(CachedBoundShaderStateLink)
	{
		// If we've already created a bound shader state with these parameters, reuse it.
		return CachedBoundShaderStateLink->BoundShaderState;
	}
	else
	{
		FOpenGLVertexShader* VertexShader = ResourceCast(VertexShaderRHI);
		FOpenGLPixelShader* PixelShader = ResourceCast(PixelShaderRHI);
		FOpenGLHullShader* HullShader = ResourceCast(HullShaderRHI);
		FOpenGLDomainShader* DomainShader = ResourceCast(DomainShaderRHI);
		FOpenGLGeometryShader* GeometryShader = ResourceCast(GeometryShaderRHI);

		FOpenGLLinkedProgramConfiguration Config;

		check(VertexShader);
		check(PixelShader);

		// Fill-in the configuration
		Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Bindings = VertexShader->Bindings;
		Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Resource = VertexShader->Resource;
		Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX].Hash = VertexShader->GetHash();
		
		if ( FOpenGL::SupportsTessellation())
		{
			if ( HullShader)
			{
                check(VertexShader);
                BindShaderStage(Config.Shaders[CrossCompiler::SHADER_STAGE_HULL], HullShader, Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX], VertexShader);
			}
			if ( DomainShader)
            {
                check(HullShader);
                BindShaderStage(Config.Shaders[CrossCompiler::SHADER_STAGE_DOMAIN], DomainShader, Config.Shaders[CrossCompiler::SHADER_STAGE_HULL], HullShader);
			}
		}
        
        if (GeometryShader)
        {
            check(DomainShader || VertexShader);
            if ( DomainShader )
            {
                BindShaderStage(Config.Shaders[CrossCompiler::SHADER_STAGE_GEOMETRY], GeometryShader, Config.Shaders[CrossCompiler::SHADER_STAGE_DOMAIN], DomainShader);
            }
            else
            {
                BindShaderStage(Config.Shaders[CrossCompiler::SHADER_STAGE_GEOMETRY], GeometryShader, Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX], VertexShader);
            }
        }
        
        check(DomainShader || GeometryShader || VertexShader);
        if ( DomainShader )
        {
            BindShaderStage(Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL], PixelShader, Config.Shaders[CrossCompiler::SHADER_STAGE_DOMAIN], DomainShader);
        }
        else if ( GeometryShader )
        {
            BindShaderStage(Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL], PixelShader, Config.Shaders[CrossCompiler::SHADER_STAGE_GEOMETRY], GeometryShader);
        }
        else
        {
            BindShaderStage(Config.Shaders[CrossCompiler::SHADER_STAGE_PIXEL], PixelShader, Config.Shaders[CrossCompiler::SHADER_STAGE_VERTEX], VertexShader);
        }

		// Check if we already have such a program in released programs cache. Use it, if we do.
		FOpenGLLinkedProgram* LinkedProgram = 0;

		int32 Index = StaticLastReleasedProgramsIndex;
		for( int CacheIndex = 0; CacheIndex < LAST_RELEASED_PROGRAMS_CACHE_COUNT; ++CacheIndex )
		{
			FOpenGLLinkedProgram* Prog = StaticLastReleasedPrograms[Index];
			if( Prog && Prog->Config == Config )
			{
				StaticLastReleasedPrograms[Index] = 0;
				LinkedProgram = Prog;
				break;
			}
			Index = (Index == LAST_RELEASED_PROGRAMS_CACHE_COUNT-1) ? 0 : Index+1;
		}

		if (!LinkedProgram)
		{
			FOpenGLLinkedProgram** CachedProgram = GetOpenGLProgramsCache().Find( Config);

			if (CachedProgram)
			{
				LinkedProgram = *CachedProgram;
			}
			else
			{
				// In case ProgramBinaryCache is enabled we defer shader compilation, look LinkProgram
				if (!FOpenGLProgramBinaryCache::IsEnabled())
				{
					const ANSICHAR* GlslCode = NULL;
					if (!VertexShader->bSuccessfullyCompiled)
					{
#if DEBUG_GL_SHADERS
						GlslCode = VertexShader->GlslCodeString;
#endif
						VertexShader->bSuccessfullyCompiled = VerifyCompiledShader(VertexShader->Resource, GlslCode);
					}
					if (!PixelShader->bSuccessfullyCompiled)
					{
#if DEBUG_GL_SHADERS
						GlslCode = PixelShader->GlslCodeString;
#endif
						PixelShader->bSuccessfullyCompiled = VerifyCompiledShader(PixelShader->Resource, GlslCode);
					}
					if (GeometryShader && !GeometryShader->bSuccessfullyCompiled)
					{
#if DEBUG_GL_SHADERS
						GlslCode = GeometryShader->GlslCodeString;
#endif
						GeometryShader->bSuccessfullyCompiled = VerifyCompiledShader(GeometryShader->Resource, GlslCode);
					}
					if (FOpenGL::SupportsTessellation())
					{
						if (HullShader && !HullShader->bSuccessfullyCompiled)
						{
#if DEBUG_GL_SHADERS
							GlslCode = HullShader->GlslCodeString;
#endif
							HullShader->bSuccessfullyCompiled = VerifyCompiledShader(HullShader->Resource, GlslCode);
						}
						if (DomainShader && !DomainShader->bSuccessfullyCompiled)
						{
#if DEBUG_GL_SHADERS
							GlslCode = DomainShader->GlslCodeString;
#endif
							DomainShader->bSuccessfullyCompiled = VerifyCompiledShader(DomainShader->Resource, GlslCode);
						}
					}
				}
				
				// Make sure we have OpenGL context set up, and invalidate the parameters cache and current program (as we'll link a new one soon)
				GetContextStateForCurrentContext().Program = -1;
				MarkShaderParameterCachesDirty(PendingState.ShaderParameters, false);

				// Link program, using the data provided in config
				LinkedProgram = LinkProgram(Config);

				// Add this program to the cache
				GetOpenGLProgramsCache().Add(Config,LinkedProgram);

				if (LinkedProgram == NULL)
				{
#if DEBUG_GL_SHADERS
					if (VertexShader->bSuccessfullyCompiled)
					{
						UE_LOG(LogRHI,Error,TEXT("Vertex Shader:\n%s"),ANSI_TO_TCHAR(VertexShader->GlslCode.GetData()));
					}
					if (PixelShader->bSuccessfullyCompiled)
					{
						UE_LOG(LogRHI,Error,TEXT("Pixel Shader:\n%s"),ANSI_TO_TCHAR(PixelShader->GlslCode.GetData()));
					}
					if (GeometryShader && GeometryShader->bSuccessfullyCompiled)
					{
						UE_LOG(LogRHI,Error,TEXT("Geometry Shader:\n%s"),ANSI_TO_TCHAR(GeometryShader->GlslCode.GetData()));
					}
					if ( FOpenGL::SupportsTessellation() )
					{
						if (HullShader && HullShader->bSuccessfullyCompiled)
						{
							UE_LOG(LogRHI,Error,TEXT("Hull Shader:\n%s"),ANSI_TO_TCHAR(HullShader->GlslCode.GetData()));
						}
						if (DomainShader && DomainShader->bSuccessfullyCompiled)
						{
							UE_LOG(LogRHI,Error,TEXT("Domain Shader:\n%s"),ANSI_TO_TCHAR(DomainShader->GlslCode.GetData()));
						}
					}
#endif //DEBUG_GL_SHADERS
					check(LinkedProgram);
				}
			}
		}

		if(FShaderCache::IsPrebindCall(FShaderCache::GetDefaultCacheState()) && !VertexDeclarationRHI)
		{
			return nullptr;
		}
		else
		{
			check(VertexDeclarationRHI);
			
			FOpenGLVertexDeclaration* VertexDeclaration = ResourceCast(VertexDeclarationRHI);
			FOpenGLBoundShaderState* BoundShaderState = new FOpenGLBoundShaderState(
				LinkedProgram,
				VertexDeclarationRHI,
				VertexShaderRHI,
				PixelShaderRHI,
				GeometryShaderRHI,
				HullShaderRHI,
				DomainShaderRHI
				);

PRAGMA_DISABLE_DEPRECATION_WARNINGS
			FShaderCache::LogBoundShaderState(FShaderCache::GetDefaultCacheState(), FOpenGL::GetShaderPlatform(), VertexDeclarationRHI, VertexShaderRHI, PixelShaderRHI, HullShaderRHI, DomainShaderRHI, GeometryShaderRHI, BoundShaderState);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

			return BoundShaderState;
		}
	}
}

void DestroyShadersAndPrograms()
{
	GetOpenGLUniformBlockLocations().Empty();
	GetOpenGLUniformBlockBindings().Empty();
	
	FOpenGLProgramsForReuse& ProgramCache = GetOpenGLProgramsCache();

	for( TMap<FOpenGLLinkedProgramConfiguration,FOpenGLLinkedProgram*>::TIterator It( ProgramCache ); It; ++It )
	{
		delete It.Value();
	}
	ProgramCache.Empty();
	
	StaticLastReleasedProgramsIndex = 0;

	FOpenGLCompiledShaderCache& ShaderCache = GetOpenGLCompiledShaderCache();
	for( TMap<FOpenGLCompiledShaderKey,GLuint>::TIterator It( ShaderCache ); It; ++It )
	{
		FOpenGL::DeleteShader(It.Value());
	}
	ShaderCache.Empty();
}

struct FSamplerPair
{
	GLuint Texture;
	GLuint Sampler;

	friend bool operator ==(const FSamplerPair& A,const FSamplerPair& B)
	{
		return A.Texture == B.Texture && A.Sampler == B.Sampler;
	}

	friend uint32 GetTypeHash(const FSamplerPair &Key)
	{
		return Key.Texture ^ (Key.Sampler << 18);
	}
};

static TMap<FSamplerPair, GLuint64> BindlessSamplerMap;

void FOpenGLDynamicRHI::SetupBindlessTextures( FOpenGLContextState& ContextState, const TArray<FOpenGLBindlessSamplerInfo> &Samplers )
{
	if ( OpenGLConsoleVariables::bBindlessTexture == 0 || !FOpenGL::SupportsBindlessTexture())
	{
		return;
	}

	// Bind all textures via Bindless
	for (int32 Texture = 0; Texture < Samplers.Num(); Texture++)
	{
		const FOpenGLBindlessSamplerInfo &Sampler = Samplers[Texture];

		GLuint64 BindlessSampler = 0xffffffff;
		FSamplerPair Pair;
		Pair.Texture = PendingState.Textures[Sampler.Slot].Resource;
		Pair.Sampler = (PendingState.SamplerStates[Sampler.Slot] != NULL) ? PendingState.SamplerStates[Sampler.Slot]->Resource : 0;

		if (Pair.Texture)
		{
			// Find Sampler pair
			if ( BindlessSamplerMap.Contains(Pair))
			{
				BindlessSampler = BindlessSamplerMap[Pair];
			}
			else
			{
				// if !found, create

				if (Pair.Sampler)
				{
					BindlessSampler = FOpenGL::GetTextureSamplerHandle( Pair.Texture, Pair.Sampler);
				}
				else
				{
					BindlessSampler = FOpenGL::GetTextureHandle( Pair.Texture);
				}

				FOpenGL::MakeTextureHandleResident( BindlessSampler);

				BindlessSamplerMap.Add( Pair, BindlessSampler);
			}

			FOpenGL::UniformHandleui64( Sampler.Handle, BindlessSampler);
		}
	}
}

void FOpenGLDynamicRHI::BindPendingShaderState( FOpenGLContextState& ContextState )
{
	SCOPE_CYCLE_COUNTER_DETAILED(STAT_OpenGLShaderBindTime);
	VERIFY_GL_SCOPE();

	bool ForceUniformBindingUpdate = false;

	GLuint PendingProgram = PendingState.BoundShaderState->LinkedProgram->Program;
	if (ContextState.Program != PendingProgram)
	{
		FOpenGL::BindProgramPipeline(PendingProgram);
		ContextState.Program = PendingProgram;
		ContextState.bUsingTessellation = PendingState.BoundShaderState->LinkedProgram->bUsingTessellation;
		MarkShaderParameterCachesDirty(PendingState.ShaderParameters, false);
		//Disable the forced rebinding to reduce driver overhead - required by SSOs
		ForceUniformBindingUpdate = FOpenGL::SupportsSeparateShaderObjects();
	}

	if (!GUseEmulatedUniformBuffers)
	{
		int32 NextUniformBufferIndex = OGL_FIRST_UNIFORM_BUFFER;

		int32 NumVertexUniformBuffers = PendingState.BoundShaderState->VertexShader->Bindings.NumUniformBuffers;
		PendingState.BoundShaderState->LinkedProgram->VerifyUniformBlockBindings(CrossCompiler::SHADER_STAGE_VERTEX, NextUniformBufferIndex);
		BindUniformBufferBase(
			ContextState,
			NumVertexUniformBuffers,
			PendingState.BoundUniformBuffers[SF_Vertex],
			NextUniformBufferIndex,
			ForceUniformBindingUpdate);
		NextUniformBufferIndex += NumVertexUniformBuffers;

		int32 NumPixelUniformBuffers = PendingState.BoundShaderState->PixelShader->Bindings.NumUniformBuffers;
		PendingState.BoundShaderState->LinkedProgram->VerifyUniformBlockBindings(CrossCompiler::SHADER_STAGE_PIXEL, NextUniformBufferIndex);
		BindUniformBufferBase(
			ContextState,
			NumPixelUniformBuffers,
			PendingState.BoundUniformBuffers[SF_Pixel],
			NextUniformBufferIndex,
			ForceUniformBindingUpdate);
		NextUniformBufferIndex += NumPixelUniformBuffers;

		if (PendingState.BoundShaderState->GeometryShader)
		{
			int32 NumGeometryUniformBuffers = PendingState.BoundShaderState->GeometryShader->Bindings.NumUniformBuffers;
			PendingState.BoundShaderState->LinkedProgram->VerifyUniformBlockBindings(CrossCompiler::SHADER_STAGE_GEOMETRY, NextUniformBufferIndex);
			BindUniformBufferBase(
				ContextState,
				NumGeometryUniformBuffers,
				PendingState.BoundUniformBuffers[SF_Geometry],
				NextUniformBufferIndex,
				ForceUniformBindingUpdate);
			NextUniformBufferIndex += NumGeometryUniformBuffers;
		}

		if (PendingState.BoundShaderState->HullShader)
		{
			int32 NumHullUniformBuffers = PendingState.BoundShaderState->HullShader->Bindings.NumUniformBuffers;
			PendingState.BoundShaderState->LinkedProgram->VerifyUniformBlockBindings(CrossCompiler::SHADER_STAGE_HULL, NextUniformBufferIndex);
			BindUniformBufferBase(ContextState,
				NumHullUniformBuffers,
				PendingState.BoundUniformBuffers[SF_Hull],
				NextUniformBufferIndex,
				ForceUniformBindingUpdate);
			NextUniformBufferIndex += NumHullUniformBuffers;
		}

		if (PendingState.BoundShaderState->DomainShader)
		{
			int32 NumDomainUniformBuffers = PendingState.BoundShaderState->DomainShader->Bindings.NumUniformBuffers;
			PendingState.BoundShaderState->LinkedProgram->VerifyUniformBlockBindings(CrossCompiler::SHADER_STAGE_DOMAIN, NextUniformBufferIndex);
			BindUniformBufferBase(ContextState,
				NumDomainUniformBuffers,
				PendingState.BoundUniformBuffers[SF_Domain],
				NextUniformBufferIndex,
				ForceUniformBindingUpdate);
			NextUniformBufferIndex += NumDomainUniformBuffers;
		}

		SetupBindlessTextures( ContextState, PendingState.BoundShaderState->LinkedProgram->Samplers );
	}
}

FOpenGLBoundShaderState::FOpenGLBoundShaderState(
	FOpenGLLinkedProgram* InLinkedProgram,
	FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
	FVertexShaderRHIParamRef InVertexShaderRHI,
	FPixelShaderRHIParamRef InPixelShaderRHI,
	FGeometryShaderRHIParamRef InGeometryShaderRHI,
	FHullShaderRHIParamRef InHullShaderRHI,
	FDomainShaderRHIParamRef InDomainShaderRHI
	)
	:	CacheLink(InVertexDeclarationRHI, InVertexShaderRHI, InPixelShaderRHI,
		InHullShaderRHI, InDomainShaderRHI,	InGeometryShaderRHI, this)
{
	FOpenGLVertexDeclaration* InVertexDeclaration = FOpenGLDynamicRHI::ResourceCast(InVertexDeclarationRHI);
	FOpenGLVertexShader* InVertexShader = FOpenGLDynamicRHI::ResourceCast(InVertexShaderRHI);
	FOpenGLPixelShader* InPixelShader = FOpenGLDynamicRHI::ResourceCast(InPixelShaderRHI);
	FOpenGLHullShader* InHullShader = FOpenGLDynamicRHI::ResourceCast(InHullShaderRHI);
	FOpenGLDomainShader* InDomainShader = FOpenGLDynamicRHI::ResourceCast(InDomainShaderRHI);
	FOpenGLGeometryShader* InGeometryShader = FOpenGLDynamicRHI::ResourceCast(InGeometryShaderRHI);

	VertexDeclaration = InVertexDeclaration;
	VertexShader = InVertexShader;
	PixelShader = InPixelShader;
	GeometryShader = InGeometryShader;

	HullShader = InHullShader;
	DomainShader = InDomainShader;

	LinkedProgram = InLinkedProgram;

	if (InVertexDeclaration)
	{
		FMemory::Memcpy(StreamStrides, InVertexDeclaration->StreamStrides, sizeof(StreamStrides));
	}
	else
	{
		FMemory::Memzero(StreamStrides, sizeof(StreamStrides));
	}
}

FOpenGLBoundShaderState::~FOpenGLBoundShaderState()
{
	check(LinkedProgram);
	FOpenGLLinkedProgram* Prog = StaticLastReleasedPrograms[StaticLastReleasedProgramsIndex];
	StaticLastReleasedPrograms[StaticLastReleasedProgramsIndex++] = LinkedProgram;
	if (StaticLastReleasedProgramsIndex == LAST_RELEASED_PROGRAMS_CACHE_COUNT)
	{
		StaticLastReleasedProgramsIndex = 0;
	}
	OnProgramDeletion(LinkedProgram->Program);
}

bool FOpenGLBoundShaderState::NeedsTextureStage(int32 TextureStageIndex)
{
	return LinkedProgram->TextureStageNeeds[TextureStageIndex];
}

int32 FOpenGLBoundShaderState::MaxTextureStageUsed()
{
	return LinkedProgram->MaxTextureStage;
}

bool FOpenGLBoundShaderState::RequiresDriverInstantiation()
{
	check(LinkedProgram);
	bool const bDrawn = LinkedProgram->bDrawn;
	LinkedProgram->bDrawn = true;
	return !bDrawn;
}

bool FOpenGLComputeShader::NeedsTextureStage(int32 TextureStageIndex)
{
	return LinkedProgram->TextureStageNeeds[TextureStageIndex];
}

int32 FOpenGLComputeShader::MaxTextureStageUsed()
{
	return LinkedProgram->MaxTextureStage;
}

bool FOpenGLComputeShader::NeedsUAVStage(int32 UAVStageIndex)
{
	return LinkedProgram->UAVStageNeeds[UAVStageIndex];
}

void FOpenGLDynamicRHI::BindPendingComputeShaderState(FOpenGLContextState& ContextState, FComputeShaderRHIParamRef ComputeShaderRHI)
{
	VERIFY_GL_SCOPE();
	FOpenGLComputeShader* ComputeShader = ResourceCast(ComputeShaderRHI);
	bool ForceUniformBindingUpdate = false;

	GLuint PendingProgram = ComputeShader->LinkedProgram->Program;
	if (ContextState.Program != PendingProgram)
	{
		FOpenGL::BindProgramPipeline(PendingProgram);
		ContextState.Program = PendingProgram;
		MarkShaderParameterCachesDirty(PendingState.ShaderParameters, true);
		ForceUniformBindingUpdate = true;
	}

	if (!GUseEmulatedUniformBuffers)
	{
		ComputeShader->LinkedProgram->VerifyUniformBlockBindings(CrossCompiler::SHADER_STAGE_COMPUTE, OGL_FIRST_UNIFORM_BUFFER);
		BindUniformBufferBase(
			ContextState,
			ComputeShader->Bindings.NumUniformBuffers,
			PendingState.BoundUniformBuffers[SF_Compute],
			OGL_FIRST_UNIFORM_BUFFER,
			ForceUniformBindingUpdate);
		SetupBindlessTextures( ContextState, ComputeShader->LinkedProgram->Samplers );
	}
}

/** Constructor. */
FOpenGLShaderParameterCache::FOpenGLShaderParameterCache() :
	GlobalUniformArraySize(-1)
{
	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniformDirty[ArrayIndex].StartVector = 0;
		PackedGlobalUniformDirty[ArrayIndex].NumVectors = 0;
	}
}

void FOpenGLShaderParameterCache::InitializeResources(int32 UniformArraySize)
{
	check(GlobalUniformArraySize == -1);

	// Uniform arrays have to be multiples of float4s.
	UniformArraySize = Align(UniformArraySize,SizeOfFloat4);

	PackedGlobalUniforms[0] = (uint8*)FMemory::Malloc(UniformArraySize * CrossCompiler::PACKED_TYPEINDEX_MAX);
	PackedUniformsScratch[0] = (uint8*)FMemory::Malloc(UniformArraySize * CrossCompiler::PACKED_TYPEINDEX_MAX);

	FMemory::Memzero(PackedGlobalUniforms[0], UniformArraySize * CrossCompiler::PACKED_TYPEINDEX_MAX);
	FMemory::Memzero(PackedUniformsScratch[0], UniformArraySize * CrossCompiler::PACKED_TYPEINDEX_MAX);
	for (int32 ArrayIndex = 1; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniforms[ArrayIndex] = PackedGlobalUniforms[ArrayIndex - 1] + UniformArraySize;
		PackedUniformsScratch[ArrayIndex] = PackedUniformsScratch[ArrayIndex - 1] + UniformArraySize;
	}
	GlobalUniformArraySize = UniformArraySize;

	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniformDirty[ArrayIndex].StartVector = 0;
		PackedGlobalUniformDirty[ArrayIndex].NumVectors = UniformArraySize / SizeOfFloat4;
	}
}

/** Destructor. */
FOpenGLShaderParameterCache::~FOpenGLShaderParameterCache()
{
	if (GlobalUniformArraySize > 0)
	{
		FMemory::Free(PackedUniformsScratch[0]);
		FMemory::Free(PackedGlobalUniforms[0]);
	}

	FMemory::Memzero(PackedUniformsScratch);
	FMemory::Memzero(PackedGlobalUniforms);

	GlobalUniformArraySize = -1;
}

/**
 * Marks all uniform arrays as dirty.
 */
void FOpenGLShaderParameterCache::MarkAllDirty()
{
	for (int32 ArrayIndex = 0; ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX; ++ArrayIndex)
	{
		PackedGlobalUniformDirty[ArrayIndex].StartVector = 0;
		PackedGlobalUniformDirty[ArrayIndex].NumVectors = GlobalUniformArraySize / SizeOfFloat4;
	}
}

/**
 * Set parameter values.
 */
void FOpenGLShaderParameterCache::Set(uint32 BufferIndexName, uint32 ByteOffset, uint32 NumBytes, const void* NewValues)
{
	uint32 BufferIndex = CrossCompiler::PackedTypeNameToTypeIndex(BufferIndexName);
	check(GlobalUniformArraySize != -1);
	check(BufferIndex < CrossCompiler::PACKED_TYPEINDEX_MAX);
	check(ByteOffset + NumBytes <= (uint32)GlobalUniformArraySize);
	PackedGlobalUniformDirty[BufferIndex].MarkDirtyRange(ByteOffset / SizeOfFloat4, (NumBytes + SizeOfFloat4 - 1) / SizeOfFloat4);
	FMemory::Memcpy(PackedGlobalUniforms[BufferIndex] + ByteOffset, NewValues, NumBytes);
}

/**
 * Commit shader parameters to the currently bound program.
 * @param ParameterTable - Information on the bound uniform arrays for the program.
 */
void FOpenGLShaderParameterCache::CommitPackedGlobals(const FOpenGLLinkedProgram* LinkedProgram, int32 Stage)
{
	SCOPE_CYCLE_COUNTER(STAT_OpenGLUniformCommitTime);
	VERIFY_GL_SCOPE();
	const uint32 BytesPerRegister = 16;

	/**
	 * Note that this always uploads the entire uniform array when it is dirty.
	 * The arrays are marked dirty either when the bound shader state changes or
	 * a value in the array is modified. OpenGL actually caches uniforms per-
	 * program. If we shadowed those per-program uniforms we could avoid calling
	 * glUniform4?v for values that have not changed since the last invocation
	 * of the program. 
	 *
	 * It's unclear whether the driver does the same thing and whether there is
	 * a performance benefit. Even if there is, this type of caching makes any
	 * multithreading vastly more difficult, so for now uniforms are not cached
	 * per-program.
	 */
	const TArray<FOpenGLLinkedProgram::FPackedUniformInfo>& PackedUniforms = LinkedProgram->StagePackedUniformInfo[Stage].PackedUniformInfos;
	const TArray<CrossCompiler::FPackedArrayInfo>& PackedArrays = LinkedProgram->Config.Shaders[Stage].Bindings.PackedGlobalArrays;
	for (int32 PackedUniform = 0; PackedUniform < PackedUniforms.Num(); ++PackedUniform)
	{
		const FOpenGLLinkedProgram::FPackedUniformInfo& UniformInfo = PackedUniforms[PackedUniform];
		if (UniformInfo.Location < 0)
		{
			// Probably this uniform array was optimized away in a linked program
			continue;
		}
		
		const uint32 ArrayIndex = UniformInfo.Index;
		check(ArrayIndex < CrossCompiler::PACKED_TYPEINDEX_MAX);
		const int32 NumVectors = PackedArrays[PackedUniform].Size / BytesPerRegister;
		GLint Location = UniformInfo.Location;
		const void* UniformData = PackedGlobalUniforms[ArrayIndex];

		// This has to be >=. If LowVector == HighVector it means that particular vector was written to.
		if (PackedGlobalUniformDirty[ArrayIndex].NumVectors > 0)
		{
			const int32 StartVector = PackedGlobalUniformDirty[ArrayIndex].StartVector;
			int32 NumDirtyVectors = FMath::Min((int32)PackedGlobalUniformDirty[ArrayIndex].NumVectors, NumVectors - StartVector);
			check(NumDirtyVectors);
			UniformData = (uint8*)UniformData + StartVector * SizeOfFloat4;
			Location += StartVector;
			switch (UniformInfo.Index)
			{
			case CrossCompiler::PACKED_TYPEINDEX_HIGHP:
			case CrossCompiler::PACKED_TYPEINDEX_MEDIUMP:
			case CrossCompiler::PACKED_TYPEINDEX_LOWP:
				FOpenGL::ProgramUniform4fv(LinkedProgram->Config.Shaders[Stage].Resource, Location, NumDirtyVectors, (GLfloat*)UniformData);
				break;

			case CrossCompiler::PACKED_TYPEINDEX_INT:
				FOpenGL::ProgramUniform4iv(LinkedProgram->Config.Shaders[Stage].Resource, Location, NumDirtyVectors, (GLint*)UniformData);
			break;

			case CrossCompiler::PACKED_TYPEINDEX_UINT:
#if PLATFORM_ANDROID || PLATFORM_IOS
				if (FOpenGL::GetFeatureLevel() == ERHIFeatureLevel::ES2)
				{
					// uint is not supported with ES2, set as int type.
					FOpenGL::ProgramUniform4iv(LinkedProgram->Config.Shaders[Stage].Resource, Location, NumDirtyVectors, (GLint*)UniformData);
				}
				else
				{
					FOpenGL::ProgramUniform4uiv(LinkedProgram->Config.Shaders[Stage].Resource, Location, NumDirtyVectors, (GLuint*)UniformData);
				}
#else
				FOpenGL::ProgramUniform4uiv(LinkedProgram->Config.Shaders[Stage].Resource, Location, NumDirtyVectors, (GLuint*)UniformData);
#endif
				break;
			}

			PackedGlobalUniformDirty[ArrayIndex].StartVector = 0;
			PackedGlobalUniformDirty[ArrayIndex].NumVectors = 0;
		}
	}
}

void FOpenGLShaderParameterCache::CommitPackedUniformBuffers(FOpenGLLinkedProgram* LinkedProgram, int32 Stage, FUniformBufferRHIRef* RHIUniformBuffers, const TArray<CrossCompiler::FUniformBufferCopyInfo>& UniformBuffersCopyInfo)
{
	SCOPE_CYCLE_COUNTER(STAT_OpenGLConstantBufferUpdateTime);
	VERIFY_GL_SCOPE();

	// Uniform Buffers are split into precision/type; the list of RHI UBs is traversed and if a new one was set, its
	// contents are copied per precision/type into corresponding scratch buffers which are then uploaded to the program
	const FOpenGLShaderBindings& Bindings = LinkedProgram->Config.Shaders[Stage].Bindings;
	check(Bindings.NumUniformBuffers <= FOpenGLRHIState::MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE);

	if (Bindings.bFlattenUB)
	{
		int32 LastInfoIndex = 0;
		for (int32 BufferIndex = 0; BufferIndex < Bindings.NumUniformBuffers; ++BufferIndex)
		{
			const FOpenGLUniformBuffer* UniformBuffer = (FOpenGLUniformBuffer*)RHIUniformBuffers[BufferIndex].GetReference();
			check(UniformBuffer);
			const uint32* RESTRICT SourceData = UniformBuffer->EmulatedBufferData->Data.GetData();
			for (int32 InfoIndex = LastInfoIndex; InfoIndex < UniformBuffersCopyInfo.Num(); ++InfoIndex)
			{
				const CrossCompiler::FUniformBufferCopyInfo& Info = UniformBuffersCopyInfo[InfoIndex];
				if (Info.SourceUBIndex == BufferIndex)
				{
					check((Info.DestOffsetInFloats + Info.SizeInFloats) * sizeof(float) <= (uint32)GlobalUniformArraySize);
					float* RESTRICT ScratchMem = (float*)PackedGlobalUniforms[Info.DestUBTypeIndex];
					ScratchMem += Info.DestOffsetInFloats;
					FMemory::Memcpy(ScratchMem, SourceData + Info.SourceOffsetInFloats, Info.SizeInFloats * sizeof(float));
					PackedGlobalUniformDirty[Info.DestUBTypeIndex].MarkDirtyRange(Info.DestOffsetInFloats / NumFloatsInFloat4, (Info.SizeInFloats + NumFloatsInFloat4 - 1) / NumFloatsInFloat4);
				}
				else
				{
					LastInfoIndex = InfoIndex;
					break;
				}
			}
		}
	}
	else
	{
		const auto& PackedUniformBufferInfos = LinkedProgram->StagePackedUniformInfo[Stage].PackedUniformBufferInfos;
		int32 LastCopyInfoIndex = 0;
		auto& EmulatedUniformBufferSet = LinkedProgram->StagePackedUniformInfo[Stage].LastEmulatedUniformBufferSet;
		for (int32 BufferIndex = 0; BufferIndex < Bindings.NumUniformBuffers; ++BufferIndex)
		{
			const FOpenGLUniformBuffer* UniformBuffer = (FOpenGLUniformBuffer*)RHIUniformBuffers[BufferIndex].GetReference();
			check(UniformBuffer);
			if (EmulatedUniformBufferSet[BufferIndex] != UniformBuffer->UniqueID)
			{
				EmulatedUniformBufferSet[BufferIndex] = UniformBuffer->UniqueID;

				// Go through the list of copy commands and perform the appropriate copy into the scratch buffer
				for (int32 InfoIndex = LastCopyInfoIndex; InfoIndex < UniformBuffersCopyInfo.Num(); ++InfoIndex)
				{
					const CrossCompiler::FUniformBufferCopyInfo& Info = UniformBuffersCopyInfo[InfoIndex];
					if (Info.SourceUBIndex == BufferIndex)
					{
						const uint32* RESTRICT SourceData = UniformBuffer->EmulatedBufferData->Data.GetData();
						SourceData += Info.SourceOffsetInFloats;
						float* RESTRICT ScratchMem = (float*)PackedUniformsScratch[Info.DestUBTypeIndex];
						ScratchMem += Info.DestOffsetInFloats;
						FMemory::Memcpy(ScratchMem, SourceData, Info.SizeInFloats * sizeof(float));
					}
					else if (Info.SourceUBIndex > BufferIndex)
					{
						// Done finding current copies
						LastCopyInfoIndex = InfoIndex;
						break;
					}

					// keep going since we could have skipped this loop when skipping cached UBs...
				}

				// Upload the split buffers to the program
				const auto& UniformBufferUploadInfoList = PackedUniformBufferInfos[BufferIndex];
				for (int32 InfoIndex = 0; InfoIndex < UniformBufferUploadInfoList.Num(); ++InfoIndex)
				{
					auto& UBInfo = Bindings.PackedUniformBuffers[BufferIndex];
					const auto& UniformInfo = UniformBufferUploadInfoList[InfoIndex];
					const void* RESTRICT UniformData = PackedUniformsScratch[UniformInfo.Index];
					int32 NumVectors = UBInfo[InfoIndex].Size / SizeOfFloat4;
					check(UniformInfo.ArrayType == UBInfo[InfoIndex].TypeName);
					switch (UniformInfo.Index)
					{
					case CrossCompiler::PACKED_TYPEINDEX_HIGHP:
					case CrossCompiler::PACKED_TYPEINDEX_MEDIUMP:
					case CrossCompiler::PACKED_TYPEINDEX_LOWP:
						FOpenGL::ProgramUniform4fv(LinkedProgram->Config.Shaders[Stage].Resource, UniformInfo.Location, NumVectors, (GLfloat*)UniformData);
						break;

					case CrossCompiler::PACKED_TYPEINDEX_INT:
						FOpenGL::ProgramUniform4iv(LinkedProgram->Config.Shaders[Stage].Resource, UniformInfo.Location, NumVectors, (GLint*)UniformData);
						break;

					case CrossCompiler::PACKED_TYPEINDEX_UINT:
#if PLATFORM_ANDROID || PLATFORM_IOS
						if (FOpenGL::GetFeatureLevel() == ERHIFeatureLevel::ES2)
						{
							// uint is not supported with ES2, set as int type.
							FOpenGL::ProgramUniform4iv(LinkedProgram->Config.Shaders[Stage].Resource, UniformInfo.Location, NumVectors, (GLint*)UniformData);
						}
						else
						{
							FOpenGL::ProgramUniform4uiv(LinkedProgram->Config.Shaders[Stage].Resource, UniformInfo.Location, NumVectors, (GLuint*)UniformData);
						}
#else
						FOpenGL::ProgramUniform4uiv(LinkedProgram->Config.Shaders[Stage].Resource, UniformInfo.Location, NumVectors, (GLuint*)UniformData);
#endif
						break;
					}
				}
			}
		}
	}
}


// Currently only Android platform can use binary program cache
TAutoConsoleVariable<int32> FOpenGLProgramBinaryCache::CVarUseProgramBinaryCache(
	TEXT("r.UseProgramBinaryCache"),
	0,
	TEXT("If true, enables binary program cache"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
	);

FOpenGLProgramBinaryCache* FOpenGLProgramBinaryCache::CachePtr = nullptr;

FOpenGLProgramBinaryCache::FOpenGLProgramBinaryCache(const FString& InCachePath)
	: CachePath(InCachePath)
{
}

FOpenGLProgramBinaryCache::~FOpenGLProgramBinaryCache()
{
};

bool FOpenGLProgramBinaryCache::IsEnabled()
{
	return CachePtr != nullptr;
}

void FOpenGLProgramBinaryCache::Initialize()
{
	check(CachePtr == nullptr);
	// Can be enabled only on Android platform right now
	bool bEnableCache = PLATFORM_ANDROID ? (CVarUseProgramBinaryCache.GetValueOnAnyThread() != 0) : false;
			
	if (bEnableCache && FOpenGL::SupportsProgramBinary())
	{
		FString CacheFolderPath;
#if PLATFORM_ANDROID
		extern FString GExternalFilePath;
		CacheFolderPath = GExternalFilePath / TEXT("ProgramBinaryCache");
			
#else
		CacheFolderPath = FPaths::ProjectSavedDir() / TEXT("ProgramBinaryCache");
#endif

		ANSICHAR* GLVersion = (ANSICHAR*)glGetString(GL_VERSION);
		ANSICHAR* GLRenderer = (ANSICHAR*)glGetString(GL_RENDERER);
		FString HashString;
		HashString.Append(GLVersion);
		HashString.Append(GLRenderer);
		FSHAHash VersionHash;
		FSHA1::HashBuffer(TCHAR_TO_ANSI(*HashString), HashString.Len(), VersionHash.Hash);
		CacheFolderPath = CacheFolderPath / VersionHash.ToString();

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (!PlatformFile.CreateDirectoryTree(*CacheFolderPath))
		{
			UE_LOG(LogRHI, Warning, TEXT("Failed to create directory for a program binary cache. Cache will be disabled: %s"), *CacheFolderPath);
		}
		else
		{
			CachePtr = new FOpenGLProgramBinaryCache(CacheFolderPath);
			UE_LOG(LogRHI, Log, TEXT("Using program binary cache: %s"), *CacheFolderPath);
		}
	}
}

void FOpenGLProgramBinaryCache::Shutdown()
{
	if (CachePtr)
	{
		delete CachePtr;
		CachePtr = nullptr;
	}
}

bool FOpenGLProgramBinaryCache::DeferShaderCompilation(GLuint Shader, const TArray<ANSICHAR>& GlslCode)
{
	bool bCanDeferShaderCompilation = true;
#if PLATFORM_ANDROID
	bCanDeferShaderCompilation = !FOpenGL::IsCheckingShaderCompilerHacks();
#endif
	
	if (CachePtr && bCanDeferShaderCompilation)
	{
		FPendingShaderCode PendingShaderCode;
		CompressShader(GlslCode, PendingShaderCode);
		CachePtr->ShadersPendingCompilation.Add(Shader, MoveTemp(PendingShaderCode));
		return true;
	}
	return false;
}

void FOpenGLProgramBinaryCache::CacheProgram(GLuint Program, const FOpenGLLinkedProgramConfiguration& Config)
{
	if (CachePtr)
	{
		GLint BinaryLength;
		glGetProgramiv(Program, GL_PROGRAM_BINARY_LENGTH, &BinaryLength);
		if (BinaryLength > 0)
		{
			TArray<uint8> ProgramBinary;
			// BinaryFormat will be stored at the start of ProgramBinary array
			ProgramBinary.SetNumUninitialized(BinaryLength + sizeof(GLenum));
			uint8* ProgramBinaryPtr = ProgramBinary.GetData();
			FOpenGL::GetProgramBinary(Program, BinaryLength, &BinaryLength, (GLenum*)ProgramBinaryPtr, ProgramBinaryPtr + sizeof(GLenum));
			CachePtr->SaveProgramBinary(Config, ProgramBinary);
		}
	}
}

bool FOpenGLProgramBinaryCache::UseCachedProgram(GLuint Program, const FOpenGLLinkedProgramConfiguration& Config)
{
	if (CachePtr)
	{
		TArray<uint8> ProgramBinary;
		if (CachePtr->LoadProgramBinary(Config, ProgramBinary))
		{
			int32 BinarySize = ProgramBinary.Num();
			uint8* ProgramBinaryPtr = ProgramBinary.GetData();
			// BinaryFormat is stored at the start of ProgramBinary array
			FOpenGL::ProgramBinary(Program, ((GLenum*)ProgramBinaryPtr)[0], ProgramBinaryPtr + sizeof(GLenum), BinarySize - sizeof(GLenum));
			return true;
		}
	}
	return false;
}

void FOpenGLProgramBinaryCache::CompilePendingShaders(const FOpenGLLinkedProgramConfiguration& Config)
{
	if (CachePtr)
	{
		for (int32 StageIdx = 0; StageIdx < ARRAY_COUNT(Config.Shaders); ++StageIdx)
		{
			GLuint ShaderResource = Config.Shaders[StageIdx].Resource;
			FPendingShaderCode* PendingShaderCodePtr = CachePtr->ShadersPendingCompilation.Find(ShaderResource);
			if (PendingShaderCodePtr)
			{
				TArray<ANSICHAR> GlslCode;
				UncompressShader(*PendingShaderCodePtr, GlslCode);
				CompileCurrentShader(ShaderResource, GlslCode);
				CachePtr->ShadersPendingCompilation.Remove(ShaderResource);
			}
		}
	}
}

FString FOpenGLProgramBinaryCache::GetProgramBinaryFilename(const FOpenGLLinkedProgramConfiguration& Config) const
{
	FString ProgramFilename = CachePath + TEXT("/");
	for (int32 StageIdx = 0; StageIdx < ARRAY_COUNT(Config.Shaders); StageIdx++)
	{
		if (Config.Shaders[StageIdx].Resource)
		{
			ProgramFilename.Append(Config.Shaders[StageIdx].Hash.ToString());
		}
	}
	
	return ProgramFilename;
}

bool FOpenGLProgramBinaryCache::LoadProgramBinary(const FOpenGLLinkedProgramConfiguration& Config, TArray<uint8>& OutBinary) const
{
	FString ProgramFilename = GetProgramBinaryFilename(Config);
	
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TUniquePtr<IFileHandle> FileHandle(PlatformFile.OpenRead(*ProgramFilename));
	if (FileHandle.IsValid())
	{
		int64 BinarySize = FileHandle->Size();
		OutBinary.SetNum((int32)BinarySize);
		FileHandle->Read(OutBinary.GetData(), BinarySize);
		return true;
	}

	return false;
}

void FOpenGLProgramBinaryCache::SaveProgramBinary(const FOpenGLLinkedProgramConfiguration& Config, const TArray<uint8>& InBinary) const
{
	FString ProgramFilename = GetProgramBinaryFilename(Config);
	
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TUniquePtr<IFileHandle> FileHandle(PlatformFile.OpenWrite(*ProgramFilename));
	if (FileHandle.IsValid())
	{
		FileHandle->Write(InBinary.GetData(), InBinary.Num());
	}
}

void FOpenGLProgramBinaryCache::CompressShader(const TArray<ANSICHAR>& InGlslCode, FPendingShaderCode& OutCompressedShader)
{
	check(InGlslCode.GetTypeSize() == sizeof(uint8));
	check(OutCompressedShader.GlslCode.GetTypeSize() == sizeof(uint8));
	
	int32 UncompressedSize = InGlslCode.Num();
	int32 CompressedSize = UncompressedSize * 4.f / 3.f;
	OutCompressedShader.GlslCode.Empty(CompressedSize);
	OutCompressedShader.GlslCode.SetNum(CompressedSize);

	OutCompressedShader.bCompressed = FCompression::CompressMemory(
		(ECompressionFlags)(COMPRESS_ZLIB | COMPRESS_BiasMemory),
		(void*)OutCompressedShader.GlslCode.GetData(),
		CompressedSize, 
		(void*)InGlslCode.GetData(), 
		UncompressedSize);

	if (OutCompressedShader.bCompressed)
	{
		// shrink buffer
		OutCompressedShader.GlslCode.SetNum(CompressedSize, true);
	}
	else
	{
		OutCompressedShader.GlslCode = InGlslCode;
	}
	
	OutCompressedShader.UncompressedSize = UncompressedSize;
	
}

void FOpenGLProgramBinaryCache::UncompressShader(const FPendingShaderCode& InCompressedShader, TArray<ANSICHAR>& OutGlslCode)
{
	check(OutGlslCode.GetTypeSize() == sizeof(uint8));
	check(InCompressedShader.GlslCode.GetTypeSize() == sizeof(uint8));

	if (InCompressedShader.bCompressed)
	{
		int32 UncompressedSize = InCompressedShader.UncompressedSize;
		OutGlslCode.Empty(UncompressedSize);
		OutGlslCode.SetNum(UncompressedSize);

		bool bResult = FCompression::UncompressMemory(
			(ECompressionFlags)(COMPRESS_ZLIB | COMPRESS_BiasMemory),
			(void*)OutGlslCode.GetData(),
			UncompressedSize,
			(void*)InCompressedShader.GlslCode.GetData(),
			InCompressedShader.GlslCode.Num());

		check(bResult);
	}
	else
	{
		OutGlslCode = InCompressedShader.GlslCode;
	}
}

