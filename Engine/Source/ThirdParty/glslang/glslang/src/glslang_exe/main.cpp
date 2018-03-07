
//#include "Worklist.h"
#include "glslang/Include/ShHandle.h"
#include "glslang/Include/revision.h"
#include "glslang/Public/ShaderLang.h"
#include "SPIRV/GlslangToSpv.h"
#include "SPIRV/GLSL.std.450.h"
#include "SPIRV/doc.h"
#include "SPIRV/disassemble.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "glslang/OSDependent/osinclude.h"

static TBuiltInResource Resources;


static void InitResources()
{
	Resources.maxLights = 32;
	Resources.maxClipPlanes = 6;
	Resources.maxTextureUnits = 32;
	Resources.maxTextureCoords = 32;
	Resources.maxVertexAttribs = 64;
	Resources.maxVertexUniformComponents = 4096;
	Resources.maxVaryingFloats = 64;
	Resources.maxVertexTextureImageUnits = 32;
	Resources.maxCombinedTextureImageUnits = 80;
	Resources.maxTextureImageUnits = 32;
	Resources.maxFragmentUniformComponents = 4096;
	Resources.maxDrawBuffers = 32;
	Resources.maxVertexUniformVectors = 128;
	Resources.maxVaryingVectors = 8;
	Resources.maxFragmentUniformVectors = 16;
	Resources.maxVertexOutputVectors = 16;
	Resources.maxFragmentInputVectors = 15;
	Resources.minProgramTexelOffset = -8;
	Resources.maxProgramTexelOffset = 7;
	Resources.maxClipDistances = 8;
	Resources.maxComputeWorkGroupCountX = 65535;
	Resources.maxComputeWorkGroupCountY = 65535;
	Resources.maxComputeWorkGroupCountZ = 65535;
	Resources.maxComputeWorkGroupSizeX = 1024;
	Resources.maxComputeWorkGroupSizeY = 1024;
	Resources.maxComputeWorkGroupSizeZ = 64;
	Resources.maxComputeUniformComponents = 1024;
	Resources.maxComputeTextureImageUnits = 16;
	Resources.maxComputeImageUniforms = 8;
	Resources.maxComputeAtomicCounters = 8;
	Resources.maxComputeAtomicCounterBuffers = 1;
	Resources.maxVaryingComponents = 60;
	Resources.maxVertexOutputComponents = 64;
	Resources.maxGeometryInputComponents = 64;
	Resources.maxGeometryOutputComponents = 128;
	Resources.maxFragmentInputComponents = 128;
	Resources.maxImageUnits = 8;
	Resources.maxCombinedImageUnitsAndFragmentOutputs = 8;
	Resources.maxCombinedShaderOutputResources = 8;
	Resources.maxImageSamples = 0;
	Resources.maxVertexImageUniforms = 0;
	Resources.maxTessControlImageUniforms = 0;
	Resources.maxTessEvaluationImageUniforms = 0;
	Resources.maxGeometryImageUniforms = 0;
	Resources.maxFragmentImageUniforms = 8;
	Resources.maxCombinedImageUniforms = 8;
	Resources.maxGeometryTextureImageUnits = 16;
	Resources.maxGeometryOutputVertices = 256;
	Resources.maxGeometryTotalOutputComponents = 1024;
	Resources.maxGeometryUniformComponents = 1024;
	Resources.maxGeometryVaryingComponents = 64;
	Resources.maxTessControlInputComponents = 128;
	Resources.maxTessControlOutputComponents = 128;
	Resources.maxTessControlTextureImageUnits = 16;
	Resources.maxTessControlUniformComponents = 1024;
	Resources.maxTessControlTotalOutputComponents = 4096;
	Resources.maxTessEvaluationInputComponents = 128;
	Resources.maxTessEvaluationOutputComponents = 128;
	Resources.maxTessEvaluationTextureImageUnits = 16;
	Resources.maxTessEvaluationUniformComponents = 1024;
	Resources.maxTessPatchComponents = 120;
	Resources.maxPatchVertices = 32;
	Resources.maxTessGenLevel = 64;
	Resources.maxViewports = 16;
	Resources.maxVertexAtomicCounters = 0;
	Resources.maxTessControlAtomicCounters = 0;
	Resources.maxTessEvaluationAtomicCounters = 0;
	Resources.maxGeometryAtomicCounters = 0;
	Resources.maxFragmentAtomicCounters = 8;
	Resources.maxCombinedAtomicCounters = 8;
	Resources.maxAtomicCounterBindings = 1;
	Resources.maxVertexAtomicCounterBuffers = 0;
	Resources.maxTessControlAtomicCounterBuffers = 0;
	Resources.maxTessEvaluationAtomicCounterBuffers = 0;
	Resources.maxGeometryAtomicCounterBuffers = 0;
	Resources.maxFragmentAtomicCounterBuffers = 1;
	Resources.maxCombinedAtomicCounterBuffers = 1;
	Resources.maxAtomicCounterBufferSize = 16384;
	Resources.maxTransformFeedbackBuffers = 4;
	Resources.maxTransformFeedbackInterleavedComponents = 64;
	Resources.maxCullDistances = 8;
	Resources.maxCombinedClipAndCullDistances = 8;
	Resources.maxSamples = 4;
	Resources.limits.nonInductiveForLoops = 1;
	Resources.limits.whileLoops = 1;
	Resources.limits.doWhileLoops = 1;
	Resources.limits.generalUniformIndexing = 1;
	Resources.limits.generalAttributeMatrixVectorIndexing = 1;
	Resources.limits.generalVaryingIndexing = 1;
	Resources.limits.generalSamplerIndexing = 1;
	Resources.limits.generalVariableIndexing = 1;
	Resources.limits.generalConstantMatrixVectorIndexing = 1;
}

char* ReadFileData(const char* fileName)
{
	FILE *in = nullptr;
	int errorCode = fopen_s(&in, fileName, "r");

	int count = 0;
	while (fgetc(in) != EOF)
	{
		count++;
	}

	fseek(in, 0, SEEK_SET);

	char *fdata = (char*)malloc(count + 2);
	if ((int)fread(fdata, 1, count, in) != count)
	{
		free(fdata);
	}

	fdata[count] = '\0';
	fclose(in);

	return fdata;
}


static void DoCompile(const char* File)
{
	glslang::TProgram& program = *new glslang::TProgram;

	EShLanguage Stage = EShLangVertex;
	glslang::TShader* shader = new glslang::TShader(Stage);
	shader->setStrings(&File, 1);

	const int defaultVersion = 100;// Options & EOptionDefaultDesktop ? 110 : 100;

	EShMessages messages = EShMsgDefault;
	bool CompileFailed = false;
	if (!shader->parse(&Resources, defaultVersion, false, messages))
	{
		CompileFailed = true;
	}

	program.addShader(shader);

	bool LinkFailed = false;
	if (/*!(Options & EOptionOutputPreprocessed) && */!program.link(messages))
	{
		LinkFailed = true;
	}

	// Needed?
	program.buildReflection();
	program.dumpReflection();

	if (program.getIntermediate(Stage))
	{
		std::vector<unsigned int> spirv;
		glslang::GlslangToSpv(*program.getIntermediate((EShLanguage)Stage), spirv);

		glslang::OutputSpv(spirv, "xxx"/*GetBinaryName((EShLanguage)stage)*/);

		// Disasm
		spv::Disassemble(std::cout, spirv);
	}

	delete shader;
}

int C_DECL main(int argc, char* argv[])
{
	{
		// One time init
		InitResources();
		glslang::InitializeProcess();
		spv::Parameterize();
	}

	const char* File = ReadFileData(argv[1]);
	DoCompile(File);

	glslang::FinalizeProcess();

	return 0;
}
