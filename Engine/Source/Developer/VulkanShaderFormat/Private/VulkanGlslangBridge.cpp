// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// Updated to SDK 1.0.42.2

#include "VulkanShaderFormat.h"
#include "ShaderCore.h"

#if defined(_MSC_VER) && _MSC_VER == 1800
	#pragma warning(push)
	#pragma warning(disable:4510) // warning C4510: 'glslang::TShader::Includer::IncludeResult' : default constructor could not be generated
	#pragma warning(disable:4610) // error C4610: struct 'glslang::TShader::Includer::IncludeResult' can never be instantiated - user defined constructor required
#endif // _MSC_VER == 1800
#if defined(_MSC_VER) && _MSC_VER >= 1800
	#pragma warning(push)
	#pragma warning(disable: 6031) // warning C6031: Return value ignored: 'snprintf'.
	#pragma warning(disable: 6340) // warning C6340: Mismatch on sign: 'const unsigned int' passed as _Param_(4) when some signed type is required in call to 'snprintf'.
#endif

THIRD_PARTY_INCLUDES_START
	#include "glslang/Include/ShHandle.h"
	#include "glslang/Include/revision.h"
	#include "glslang/Public/ShaderLang.h"
	#include "SPIRV/GlslangToSpv.h"
	#include "SPIRV/GLSL.std.450.h"
	#include "SPIRV/doc.h"
	#include "SPIRV/disassemble.h"
THIRD_PARTY_INCLUDES_END

#if defined(_MSC_VER) && _MSC_VER >= 1800
	#pragma warning(pop)
#endif
#if defined(_MSC_VER) && _MSC_VER == 1800
	#pragma warning(pop)
#endif // _MSC_VER == 1800

#include "hlslcc.h"

#include <fstream>

struct FSpirvResources : public TBuiltInResource
{
	//#todo-rco: Make this based off platform (eg Android, PC, etc)
	FSpirvResources()
	{
		maxLights = 32;
		maxClipPlanes = 6;
		maxTextureUnits = 32;
		maxTextureCoords = 32;
		maxVertexAttribs = 64;
		maxVertexUniformComponents = 4096;
		maxVaryingFloats = 64;
		maxVertexTextureImageUnits = 32;
		maxCombinedTextureImageUnits = 80;
		maxTextureImageUnits = 32;
		maxFragmentUniformComponents = 4096;
		maxDrawBuffers = 32;
		maxVertexUniformVectors = 128;
		maxVaryingVectors = 8;
		maxFragmentUniformVectors = 16;
		maxVertexOutputVectors = 16;
		maxFragmentInputVectors = 15;
		minProgramTexelOffset = -8;
		maxProgramTexelOffset = 7;
		maxClipDistances = 8;
		maxComputeWorkGroupCountX = 65535;
		maxComputeWorkGroupCountY = 65535;
		maxComputeWorkGroupCountZ = 65535;
		maxComputeWorkGroupSizeX = 1024;
		maxComputeWorkGroupSizeY = 1024;
		maxComputeWorkGroupSizeZ = 64;
		maxComputeUniformComponents = 1024;
		maxComputeTextureImageUnits = 16;
		maxComputeImageUniforms = 8;
		maxComputeAtomicCounters = 8;
		maxComputeAtomicCounterBuffers = 1;
		maxVaryingComponents = 60;
		maxVertexOutputComponents = 64;
		maxGeometryInputComponents = 64;
		maxGeometryOutputComponents = 128;
		maxFragmentInputComponents = 128;
		maxImageUnits = 8;
		maxCombinedImageUnitsAndFragmentOutputs = 8;
		maxCombinedShaderOutputResources = 8;
		maxImageSamples = 0;
		maxVertexImageUniforms = 0;
		maxTessControlImageUniforms = 0;
		maxTessEvaluationImageUniforms = 0;
		maxGeometryImageUniforms = 0;
		maxFragmentImageUniforms = 8;
		maxCombinedImageUniforms = 8;
		maxGeometryTextureImageUnits = 16;
		maxGeometryOutputVertices = 256;
		maxGeometryTotalOutputComponents = 1024;
		maxGeometryUniformComponents = 1024;
		maxGeometryVaryingComponents = 64;
		maxTessControlInputComponents = 128;
		maxTessControlOutputComponents = 128;
		maxTessControlTextureImageUnits = 16;
		maxTessControlUniformComponents = 1024;
		maxTessControlTotalOutputComponents = 4096;
		maxTessEvaluationInputComponents = 128;
		maxTessEvaluationOutputComponents = 128;
		maxTessEvaluationTextureImageUnits = 16;
		maxTessEvaluationUniformComponents = 1024;
		maxTessPatchComponents = 120;
		maxPatchVertices = 32;
		maxTessGenLevel = 64;
		maxViewports = 16;
		maxVertexAtomicCounters = 0;
		maxTessControlAtomicCounters = 0;
		maxTessEvaluationAtomicCounters = 0;
		maxGeometryAtomicCounters = 0;
		maxFragmentAtomicCounters = 8;
		maxCombinedAtomicCounters = 8;
		maxAtomicCounterBindings = 1;
		maxVertexAtomicCounterBuffers = 0;
		maxTessControlAtomicCounterBuffers = 0;
		maxTessEvaluationAtomicCounterBuffers = 0;
		maxGeometryAtomicCounterBuffers = 0;
		maxFragmentAtomicCounterBuffers = 1;
		maxCombinedAtomicCounterBuffers = 1;
		maxAtomicCounterBufferSize = 16384;
		maxTransformFeedbackBuffers = 4;
		maxTransformFeedbackInterleavedComponents = 64;
		maxCullDistances = 8;
		maxCombinedClipAndCullDistances = 8;
		maxSamples = 4;
		limits.nonInductiveForLoops = 1;
		limits.whileLoops = 1;
		limits.doWhileLoops = 1;
		limits.generalUniformIndexing = 1;
		limits.generalAttributeMatrixVectorIndexing = 1;
		limits.generalVaryingIndexing = 1;
		limits.generalSamplerIndexing = 1;
		limits.generalVariableIndexing = 1;
		limits.generalConstantMatrixVectorIndexing = 1;


		// One time init
		glslang::InitializeProcess();
	}

	~FSpirvResources()
	{
		glslang::FinalizeProcess();
	}
};

static FSpirvResources GSpirvResources;


static EShLanguage GetStage(EHlslShaderFrequency Frequency)
{
	switch (Frequency)
	{
	case HSF_VertexShader: return EShLangVertex;
	case HSF_PixelShader: return EShLangFragment;
	case HSF_GeometryShader: return EShLangGeometry;
	case HSF_ComputeShader: return EShLangCompute;
	case HSF_HullShader: return EShLangTessControl;
	case HSF_DomainShader: return EShLangTessEvaluation;
	default: break;
	}

	return EShLangCount;
}

bool GenerateSpirv(const ANSICHAR* Source, FCompilerInfo& CompilerInfo, FString& OutErrors, const FString& DumpDebugInfoPath, FSpirv& OutSpirv)
{
	glslang::TProgram* Program = new glslang::TProgram;

	EShLanguage Stage = GetStage(CompilerInfo.Frequency);
	glslang::TShader* Shader = new glslang::TShader(Stage);

	// Skip to #version
	const char* GlslSourceSkipHeader = strstr(Source, "#version");
	Shader->setStrings(&GlslSourceSkipHeader, 1);

	auto DoGenerate = [&]()
	{
		const int DefaultVersion = 100;// Options & EOptionDefaultDesktop ? 110 : 100;

		EShMessages Messages = (EShMessages)(EShMsgDefault | EShMsgSpvRules | EShMsgVulkanRules);
		if (!Shader->parse(&GSpirvResources, DefaultVersion, false, Messages))
		{
			OutErrors += ANSI_TO_TCHAR(Shader->getInfoLog());
			return false;
		}

		Program->addShader(Shader);

		if (!Program->link(Messages))
		{
			OutErrors += ANSI_TO_TCHAR(Program->getInfoLog());
			return false;
		}

		if (!Program->getIntermediate(Stage))
		{
			OutErrors += ANSI_TO_TCHAR(Program->getInfoLog());
			return false;
		}

		if (!Program->buildReflection())
		{
			OutErrors += ANSI_TO_TCHAR(Program->getInfoLog());
			return false;
		}

		std::vector<unsigned int> Spirv;
		glslang::GlslangToSpv(*Program->getIntermediate((EShLanguage)Stage), Spirv);

		uint32 SizeInBytes = Spirv.size() * sizeof(Spirv[0]);
		OutSpirv.Data.AddZeroed(SizeInBytes);
		FMemory::Memcpy(OutSpirv.Data.GetData(), &Spirv[0], SizeInBytes);

		if (CompilerInfo.bDebugDump)
		{
			// Binary SpirV
			FString SpirvFile = DumpDebugInfoPath / (TEXT("Output.spv"));
			glslang::OutputSpvBin(Spirv, TCHAR_TO_ANSI(*SpirvFile));

			// Text spirv
			FString SpirvTextFile = DumpDebugInfoPath / (TEXT("Output.spvasm"));
			std::ofstream File;
			File.open(TCHAR_TO_ANSI(*SpirvTextFile), std::fstream::out/* | std::fstream::binary*/);
			if (File.is_open())
			{
				spv::Parameterize();
				spv::Disassemble(File, Spirv);
				File.close();
			}
		}
		else if (CompilerInfo.Input.bSkipPreprocessedCache)
		{
			spv::Parameterize();
			spv::Disassemble(std::cout, Spirv);
		}

		if (CompilerInfo.Input.bSkipPreprocessedCache)
		{
			Program->dumpReflection();
		}

		for (int32 Index = 0; Index < Program->getNumLiveUniformVariables(); ++Index)
		{
			const char* AnsiName = Program->getUniformName(Index);
			FString Name = AnsiName ? ANSI_TO_TCHAR(AnsiName) : TEXT("NULL");
			const auto* Type = Program->getAttributeTType(Index);
			int32 Binding = -1;
			if (Type && Type->getQualifier().hasBinding())
			{
				Binding = Type->getQualifier().layoutBinding;
			}
			else
			{
				Binding = Program->getUniformBinding(Index);
			}
			FSpirv::FEntry Entry{Name, Binding};
			OutSpirv.ReflectionInfo.Add(Entry);
		}

		for (int32 Index = 0; Index < Program->getNumLiveUniformBlocks(); ++Index)
		{
			const char* AnsiName = Program->getUniformBlockName(Index);
			FString Name = AnsiName ? ANSI_TO_TCHAR(AnsiName) : TEXT("NULL");
			const auto* Type = Program->getUniformBlockTType(Index);
			int32 Binding = -1;
			if (Type && Type->getQualifier().hasBinding())
			{
				Binding = Type->getQualifier().layoutBinding;
			}
			else
			{
				Binding = Program->getUniformBinding(Index);
			}
			FSpirv::FEntry Entry{Name, Binding};
			OutSpirv.ReflectionInfo.Add(Entry);
		}

		return true;
	};

	bool bResult = DoGenerate();

	delete Program;
	delete Shader;

	return bResult;
}
