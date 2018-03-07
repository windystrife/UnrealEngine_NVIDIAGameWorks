// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HlslccDefinitions.h"

/**
 * Compiler version.
 */
enum
{
	HLSLCC_VersionMajor = 0,
	HLSLCC_VersionMinor = 66,
};


class ir_function_signature;

// Interface for generating source code
class FCodeBackend
{
protected:
	// Built from EHlslCompileFlag
	const unsigned int HlslCompileFlags;
	const EHlslCompileTarget Target;

public:
	FCodeBackend(unsigned int InHlslCompileFlags, EHlslCompileTarget InTarget) :
		HlslCompileFlags(InHlslCompileFlags),
		Target(InTarget)
	{
	}

	virtual ~FCodeBackend() {}

	/**
	* Generate GLSL code for the given instructions.
	* @param ir - IR instructions.
	* @param ParseState - Parse state.
	* @returns Target source code that implements the IR instructions (Glsl, etc).
	*/
	virtual char* GenerateCode(struct exec_list* ir, struct _mesa_glsl_parse_state* ParseState, EHlslShaderFrequency Frequency) = 0;

	// Returns false if there were restrictions that made compilation fail
	virtual bool ApplyAndVerifyPlatformRestrictions(exec_list* Instructions, _mesa_glsl_parse_state* ParseState, EHlslShaderFrequency Frequency) { return true; }

	// Returns false if any issues
	virtual bool GenerateMain(EHlslShaderFrequency Frequency, const char* EntryPoint, exec_list* Instructions, _mesa_glsl_parse_state* ParseState) { return false; }

	// Returns false if any issues. This should be called after every specialized step that might modify IR.
	inline bool OptimizeAndValidate(exec_list* Instructions, _mesa_glsl_parse_state* ParseState)
	{
		if (Optimize(Instructions, ParseState))
		{
			return Validate(Instructions, ParseState);
		}
		return false;
	}

	bool Optimize(exec_list* Instructions, _mesa_glsl_parse_state* ParseState);
	bool Validate(exec_list* Instructions, _mesa_glsl_parse_state* ParseState);

	static ir_function_signature* FindEntryPointFunction(exec_list* Instructions, _mesa_glsl_parse_state* ParseState, const char* EntryPoint);

protected:
	ir_function_signature* GetMainFunction(exec_list* Instructions);
};

/**
 * Cross compile HLSL shader code to GLSL.
 * @param InSourceFilename - The filename of the shader source code. This file
 *                           is referenced when generating compile errors.
 * @param InShaderSource - HLSL shader source code.
 * @param InEntryPoint - The name of the entry point.
 * @param InShaderFrequency - The shader frequency.
 * @param InFlags - Flags, see the EHlslCompileFlag enum.
 * @param InCompileTarget - Cross compilation target.
 * @param OutShaderSource - Upon return contains GLSL shader source.
 * @param OutErrorLog - Upon return contains the error log, if any.
 * @returns 0 if compilation failed, non-zero otherwise.
 */
class FHlslCrossCompilerContext
{
public:
	FHlslCrossCompilerContext(int InFlags, EHlslShaderFrequency InShaderFrequency, EHlslCompileTarget InCompileTarget);
	~FHlslCrossCompilerContext();

	// Initialize allocator, types, etc and validate flags. Returns false if it will not be able to proceed (eg Compute on ES2).
	bool Init(
		const char* InSourceFilename,
		struct ILanguageSpec* InLanguageSpec);

	// Run the actual compiler & generate source & errors
	bool Run(
		const char* InShaderSource,
		const char* InEntryPoint,
		FCodeBackend* InShaderBackEnd,
		char** OutShaderSource,
		char** OutErrorLog
		);

protected:
	// Preprocessor, Lexer, AST->HIR
	bool RunFrontend(const char** InOutShaderSource);

	// Optimization, generate main, code gen backend
	bool RunBackend(
		const char* InShaderSource,
		const char* InEntryPoint,
		FCodeBackend* InShaderBackEnd);

	void* MemContext;
	struct _mesa_glsl_parse_state* ParseState;
	struct exec_list* ir;
	int Flags;
	const EHlslShaderFrequency ShaderFrequency;
	const EHlslCompileTarget CompileTarget;
};

// Memory Leak detection for VS
#define ENABLE_CRT_MEM_LEAKS		0 && WIN32

#if ENABLE_CRT_MEM_LEAKS
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

class FCRTMemLeakScope
{
public:
	FCRTMemLeakScope(bool bInDumpLeaks = false) :
		bDumpLeaks(bInDumpLeaks)
	{
#if ENABLE_CRT_MEM_LEAKS	
		_CrtMemCheckpoint(&Begin);
#endif
	}

	~FCRTMemLeakScope()
	{
#if ENABLE_CRT_MEM_LEAKS	
		_CrtMemCheckpoint(&End);

		if (_CrtMemDifference(&Delta, &Begin, &End))
		{
			_CrtMemDumpStatistics(&Delta);
		}

		if (bDumpLeaks)
		{
			_CrtDumpMemoryLeaks();
		}
#endif
	}

	static void BreakOnBlock(long Block)
	{
#if ENABLE_CRT_MEM_LEAKS
		_CrtSetBreakAlloc(Block);
#endif
	}

	static void CheckIntegrity()
	{
#if ENABLE_CRT_MEM_LEAKS
		_CrtCheckMemory();
#endif
	}

protected:
	bool bDumpLeaks;
#if ENABLE_CRT_MEM_LEAKS
	_CrtMemState Begin, End, Delta;
#endif
};
