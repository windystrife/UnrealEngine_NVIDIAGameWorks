// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <Windows.h>
#include "ShaderCompilerCommon.h"
#include "hlslcc.h"
#include "LanguageSpec.h"
//#include "glsl/ir_gen_glsl.h"

/** Debug output. */
static char* DebugBuffer = 0;
static void dprintf(const char* Format, ...)
{
	const int BufSize = (1 << 20);
	va_list Args;
	int Count;

	if (DebugBuffer == nullptr)
	{
		DebugBuffer = (char*)malloc(BufSize);
	}

	va_start(Args, Format);
#if WIN32
	Count = vsnprintf_s(DebugBuffer, BufSize, _TRUNCATE, Format, Args);
#else
	Count = vsnprintf(DebugBuffer, BufSize, Format, Args);
#endif
	va_end(Args);

	if (Count < -1)
	{
		// Overflow, add a line feed and null terminate the string.
		DebugBuffer[BufSize - 2] = '\n';
		DebugBuffer[BufSize - 1] = 0;
	}
	else
	{
		// Make sure the string is null terminated.
		DebugBuffer[Count] = 0;
	}
	
#if WIN32
	OutputDebugStringA(DebugBuffer);
#elif __APPLE__
	syslog(LOG_DEBUG, "%s", DebugBuffer);
#endif
	fprintf(stdout, "%s", DebugBuffer);
}

#include "ir.h"
#include "irDump.h"

struct FGlslCodeBackend : public FCodeBackend
{
	FGlslCodeBackend(unsigned int InHlslCompileFlags, EHlslCompileTarget InTarget) : FCodeBackend(InHlslCompileFlags, InTarget) {}

	// Returns false if any issues
	virtual bool GenerateMain(EHlslShaderFrequency Frequency, const char* EntryPoint, exec_list* Instructions, _mesa_glsl_parse_state* ParseState) override
	{
		ir_function_signature* EntryPointSig = FindEntryPointFunction(Instructions, ParseState, EntryPoint);
		if (EntryPointSig)
		{
			EntryPointSig->is_main = true;
			return true;
		}

		return false;
	}

	virtual char* GenerateCode(struct exec_list* ir, struct _mesa_glsl_parse_state* ParseState, EHlslShaderFrequency Frequency) override
	{
		IRDump(ir, ParseState);
		return 0;
	}
};
#define FRAMEBUFFER_FETCH_ES2	"FramebufferFetchES2"
#include "ir.h"

struct FGlslLanguageSpec : public ILanguageSpec
{
	virtual bool SupportsDeterminantIntrinsic() const {return false;}
	virtual bool SupportsTransposeIntrinsic() const {return false;}
	virtual bool SupportsIntegerModulo() const {return true;}
	virtual bool AllowsSharingSamplers() const { return false; }

	// half3x3 <-> float3x3
	virtual bool SupportsMatrixConversions() const {return false;}
	virtual void SetupLanguageIntrinsics(_mesa_glsl_parse_state* State, exec_list* ir)
	{
		make_intrinsic_genType(ir, State, FRAMEBUFFER_FETCH_ES2, ir_invalid_opcode, IR_INTRINSIC_FLOAT, 0, 4, 4);

		{
			/**
			* Create GLSL functions that are left out of the symbol table
			*  Prevent pollution, but make them so thay can be used to
			*  implement the hlsl barriers
			*/
			const int glslFuncCount = 7;
			const char * glslFuncName[glslFuncCount] =
			{
				"barrier", "memoryBarrier", "memoryBarrierAtomicCounter", "memoryBarrierBuffer",
				"memoryBarrierShared", "memoryBarrierImage", "groupMemoryBarrier"
			};
			ir_function* glslFuncs[glslFuncCount];

			for (int i = 0; i < glslFuncCount; i++)
			{
				void* ctx = State;
				ir_function* func = new(ctx)ir_function(glslFuncName[i]);
				ir_function_signature* sig = new(ctx)ir_function_signature(glsl_type::void_type);
				sig->is_builtin = true;
				func->add_signature(sig);
				ir->push_tail(func);
				glslFuncs[i] = func;
			}

			/** Implement HLSL barriers in terms of GLSL functions */
			const char * functions[] =
			{
				"GroupMemoryBarrier", "GroupMemoryBarrierWithGroupSync",
				"DeviceMemoryBarrier", "DeviceMemoryBarrierWithGroupSync",
				"AllMemoryBarrier", "AllMemoryBarrierWithGroupSync"
			};
			const int max_children = 4;
			ir_function * implFuncs[][max_children] =
			{
				{glslFuncs[4]} /**{"memoryBarrierShared"}*/,
				{glslFuncs[4], glslFuncs[0]} /**{"memoryBarrierShared","barrier"}*/,
				{glslFuncs[2], glslFuncs[3], glslFuncs[5]} /**{"memoryBarrierAtomicCounter", "memoryBarrierBuffer", "memoryBarrierImage"}*/,
				{glslFuncs[2], glslFuncs[3], glslFuncs[5], glslFuncs[0]} /**{"memoryBarrierAtomicCounter", "memoryBarrierBuffer", "memoryBarrierImage", "barrier"}*/,
				{glslFuncs[1]} /**{"memoryBarrier"}*/,
				{glslFuncs[1], glslFuncs[0]} /**{"groupMemoryBarrier","barrier"}*/
			};

			for (size_t i = 0; i < sizeof(functions) / sizeof(const char*); i++)
			{
				void* ctx = State;
				ir_function* func = new(ctx)ir_function(functions[i]);

				ir_function_signature* sig = new(ctx)ir_function_signature(glsl_type::void_type);
				sig->is_builtin = true;
				sig->is_defined = true;

				for (int j = 0; j < max_children; j++)
				{
					if (implFuncs[i][j] == NULL)
						break;
					ir_function* child = implFuncs[i][j];
					check(child);
					check(child->signatures.get_head() == child->signatures.get_tail());
					ir_function_signature *childSig = (ir_function_signature *)child->signatures.get_head();
					exec_list actual_parameter;
					sig->body.push_tail(
						new(ctx)ir_call(childSig, NULL, &actual_parameter)
					);
				}

				func->add_signature(sig);

				State->symbols->add_global_function(func);
				ir->push_tail(func);
			}
		}
	}
};


char* LoadShaderFromFile(const char* Filename);

struct SCmdOptions
{
	const char* ShaderFilename;
	EHlslShaderFrequency Frequency;
	EHlslCompileTarget Target;
	const char* Entry;
	bool bDumpAST;
	bool bNoPreprocess;
	bool bFlattenUB;
	bool bFlattenUBStructures;
	bool bUseDX11Clip;
	bool bGroupFlattenedUB;
	bool bExpandExpressions;
	bool bCSE;
	bool bSeparateShaderObjects;
	bool bPackIntoUBs;
	const char* OutFile;

	SCmdOptions() 
	{
		ShaderFilename = nullptr;
		Frequency = HSF_InvalidFrequency;
		Target = HCT_InvalidTarget;
		Entry = nullptr;
		bDumpAST = false;
		bNoPreprocess = false;
		bFlattenUB = false;
		bFlattenUBStructures = false;
		bUseDX11Clip = false;
		bGroupFlattenedUB = false;
		bExpandExpressions = false;
		bCSE = false;
		bSeparateShaderObjects = false;
		bPackIntoUBs = false;
		OutFile = nullptr;
	}
};

static int ParseCommandLine( int argc, char** argv, SCmdOptions& OutOptions)
{
	while (argc)
	{
		if (**argv == '-')
		{
			if (!strcmp(*argv, "-vs"))
			{
				OutOptions.Frequency = HSF_VertexShader;
			}
			else if (!strcmp(*argv, "-ps"))
			{
				OutOptions.Frequency = HSF_PixelShader;
			}
			else if (!strcmp(*argv, "-gs"))
			{
				OutOptions.Frequency = HSF_GeometryShader;
			}
			else if (!strcmp(*argv, "-ds"))
			{
				OutOptions.Frequency = HSF_DomainShader;
			}
			else if (!strcmp(*argv, "-hs"))
			{
				OutOptions.Frequency = HSF_HullShader;
			}
			else if (!strcmp(*argv, "-cs"))
			{
				OutOptions.Frequency = HSF_ComputeShader;
			}
			else if (!strcmp(*argv, "-sm4"))
			{
				OutOptions.Target = HCT_FeatureLevelSM4;
			}
			else if (!strcmp(*argv, "-sm5"))
			{
				OutOptions.Target = HCT_FeatureLevelSM5;
			}
			else if (!strcmp(*argv, "-es31"))
			{
				OutOptions.Target = HCT_FeatureLevelES3_1;
			}
			else if (!strcmp(*argv, "-es31ext"))
			{
				OutOptions.Target = HCT_FeatureLevelES3_1Ext;
			}
			else if (!strcmp(*argv, "-es2"))
			{
				OutOptions.Target = HCT_FeatureLevelES2;
			}
			else if (!strncmp(*argv, "-entry=", 7))
			{
				OutOptions.Entry = (*argv) + 7;
			}
			else if (!strcmp(*argv, "-ast"))
			{
				OutOptions.bDumpAST = true;
			}
			else if (!strcmp(*argv, "-nopp"))
			{
				OutOptions.bNoPreprocess = true;
			}
			else if (!strcmp(*argv, "-flattenub"))
			{
				OutOptions.bFlattenUB = true;
			}
			else if (!strcmp(*argv, "-flattenubstruct"))
			{
				OutOptions.bFlattenUBStructures = true;
			}
			else if (!strcmp(*argv, "-dx11clip"))
			{
				OutOptions.bUseDX11Clip = true;
			}
			else if (!strcmp(*argv, "-groupflatub"))
			{
				OutOptions.bGroupFlattenedUB = true;
			}
			else if (!strcmp(*argv, "-cse"))
			{
				OutOptions.bCSE = true;
			}
			else if (!strcmp(*argv, "-xpxpr"))
			{
				OutOptions.bExpandExpressions = true;
			}
			else if (!strncmp(*argv, "-o=", 3))
			{
				OutOptions.OutFile = (*argv) + 3;
			}
			else if (!strcmp( *argv, "-separateshaders"))
			{
				OutOptions.bSeparateShaderObjects = true;
			}
			else if (!strcmp(*argv, "-packintoubs"))
			{
				OutOptions.bPackIntoUBs = true;
			}
			else
			{
				dprintf("Warning: Unknown option %s\n", *argv);
			}
		}
		else
		{
			OutOptions.ShaderFilename = *argv;
		}

		argc--;
		argv++;
	}

	if (!OutOptions.ShaderFilename)
	{
		dprintf( "Provide a shader filename\n");
		return -1;
	}
	if (!OutOptions.Entry)
	{
		//default to Main
		dprintf( "No shader entrypoint specified, defaulting to 'Main'\n");
		OutOptions.Entry = "Main";
	}
	if (OutOptions.Frequency == HSF_InvalidFrequency)
	{
		//default to PixelShaders
		dprintf( "No shader frequency specified, defaulting to PS\n");
		OutOptions.Frequency = HSF_PixelShader;
	}
	if (OutOptions.Target == HCT_InvalidTarget)
	{
		dprintf("No shader model specified, defaulting to SM5\n");

		//Default to GL3 shaders
		OutOptions.Target = HCT_FeatureLevelSM5;
	}

	return 0;
}

/* 
to debug issues which only show up when multiple shaders get compiled by the same process,
such as the ShaderCompilerWorker
*/ 
#define NUMBER_OF_MAIN_RUNS 1
#if NUMBER_OF_MAIN_RUNS > 1
int actual_main( int argc, char** argv);

int main( int argc, char** argv)
{
	int result = 0;

	for(int c = 0; c < NUMBER_OF_MAIN_RUNS; ++c)
	{
		char** the_real_argv = new char*[argc];

		for(int i = 0; i < argc; ++i)
		{
			the_real_argv[i] = strdup(argv[i]);
		}
		
		int the_result = actual_main(argc, the_real_argv);
		
		for(int i = 0; i < argc; ++i)
		{
			delete the_real_argv[i];
		}

		delete[] the_real_argv;


		result += the_result;
	}
	return result;
}

int actual_main( int argc, char** argv)
#else
int main( int argc, char** argv)
#endif
{
#if ENABLE_CRT_MEM_LEAKS	
	int Flag = _CRTDBG_ALLOC_MEM_DF | /*_CRTDBG_CHECK_ALWAYS_DF |*/ /*_CRTDBG_CHECK_CRT_DF |*/ _CRTDBG_DELAY_FREE_MEM_DF;
	int OldFlag = _CrtSetDbgFlag(Flag);
	//FCRTMemLeakScope::BreakOnBlock(15828);
#endif
	char* HLSLShaderSource = nullptr;
	char* GLSLShaderSource = nullptr;
	char* ErrorLog = nullptr;

	SCmdOptions Options;
	{
		int Result = ParseCommandLine( argc-1, argv+1, Options);
		if (Result != 0)
		{
			return Result;
		}
	}
	
	HLSLShaderSource = LoadShaderFromFile(Options.ShaderFilename);
	if (!HLSLShaderSource)
	{
		dprintf( "Failed to open input shader %s\n", Options.ShaderFilename);
		return -2;
	}

	int Flags = HLSLCC_PackUniforms; // | HLSLCC_NoValidation | HLSLCC_PackUniforms;
	Flags |= Options.bNoPreprocess ? HLSLCC_NoPreprocess : 0;
	Flags |= Options.bDumpAST ? HLSLCC_PrintAST : 0;
	Flags |= Options.bUseDX11Clip ? HLSLCC_DX11ClipSpace : 0;
	Flags |= Options.bFlattenUB ? HLSLCC_FlattenUniformBuffers : 0;
	Flags |= Options.bFlattenUBStructures ? HLSLCC_FlattenUniformBufferStructures : 0;
	Flags |= Options.bGroupFlattenedUB ? HLSLCC_GroupFlattenedUniformBuffers : 0;
	Flags |= Options.bCSE ? HLSLCC_ApplyCommonSubexpressionElimination : 0;
	Flags |= Options.bExpandExpressions ? HLSLCC_ExpandSubexpressions : 0;
	Flags |= Options.bSeparateShaderObjects ? HLSLCC_SeparateShaderObjects : 0;
	Flags |= Options.bPackIntoUBs ? HLSLCC_PackUniformsIntoUniformBuffers : 0;

	FGlslCodeBackend GlslCodeBackend(Flags, Options.Target);
	FGlslLanguageSpec GlslLanguageSpec;//(Options.Target == HCT_FeatureLevelES2);

	FCodeBackend* CodeBackend = &GlslCodeBackend;
	ILanguageSpec* LanguageSpec = &GlslLanguageSpec;

	int Result = 0;
	{
		FCRTMemLeakScope MemLeakScopeContext(true);
		//for (int32 Index = 0; Index < 256; ++Index)
		{
			FHlslCrossCompilerContext Context(Flags, Options.Frequency, Options.Target);
			if (Context.Init(Options.ShaderFilename, LanguageSpec))
			{
				//FCRTMemLeakScope MemLeakScopeRun;
				Result = Context.Run(
					HLSLShaderSource,
					Options.Entry,
					CodeBackend,
					&GLSLShaderSource,
					&ErrorLog) ? 1 : 0;
			}

			if (GLSLShaderSource)
			{
				dprintf("GLSL Shader Source --------------------------------------------------------------\n");
				dprintf("%s",GLSLShaderSource);
				dprintf("\n-------------------------------------------------------------------------------\n\n");
			}

			if (ErrorLog)
			{
				dprintf("Error Log ----------------------------------------------------------------------\n");
				dprintf("%s",ErrorLog);
				dprintf("\n-------------------------------------------------------------------------------\n\n");
			}

			if (Options.OutFile && GLSLShaderSource)
			{
				FILE *fp = fopen( Options.OutFile, "w");

				if (fp)
				{
					fprintf( fp, "%s", GLSLShaderSource);
					fclose(fp);
				}
			}

			free(HLSLShaderSource);
			free(GLSLShaderSource);
			free(ErrorLog);

			if (DebugBuffer)
			{
				free(DebugBuffer);
				DebugBuffer = nullptr;
			}
		}
	}

#if ENABLE_CRT_MEM_LEAKS	
	Flag = _CrtSetDbgFlag(OldFlag);
#endif

	return 0;
}

char* LoadShaderFromFile(const char* Filename)
{
	char* Source = 0;
	FILE* fp = fopen(Filename, "r");
	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		size_t FileSize = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		Source = (char*)malloc(FileSize + 1);
		size_t NumRead = fread(Source, 1, FileSize, fp);
		Source[NumRead] = 0;
		fclose(fp);
	}
	return Source;
}
