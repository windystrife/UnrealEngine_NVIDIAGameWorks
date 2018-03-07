// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ShaderPreprocessor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "PreprocessorPrivate.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, ShaderPreprocessor);

/**
 * Append defines to an MCPP command line.
 * @param OutOptions - Upon return contains MCPP command line parameters as a string appended to the current string.
 * @param Definitions - Definitions to add.
 */
static void AddMcppDefines(FString& OutOptions, const TMap<FString,FString>& Definitions)
{
	for (TMap<FString,FString>::TConstIterator It(Definitions); It; ++It)
	{
		OutOptions += FString::Printf(TEXT(" \"-D%s=%s\""), *(It.Key()), *(It.Value()));
	}
}

/**
 * Helper class used to load shader source files for MCPP.
 */
class FMcppFileLoader
{
public:
	/** Initialization constructor. */
	explicit FMcppFileLoader(const FShaderCompilerInput& InShaderInput, FShaderCompilerOutput& InShaderOutput)
		: ShaderInput(InShaderInput)
		, ShaderOutput(InShaderOutput)
	{
		FString InputShaderSource;
		if (LoadShaderSourceFile(*InShaderInput.VirtualSourceFilePath, InputShaderSource, nullptr))
		{
			InputShaderSource = FString::Printf(TEXT("%s\n#line 1\n%s"), *ShaderInput.SourceFilePrefix, *InputShaderSource);
			CachedFileContents.Add(InShaderInput.VirtualSourceFilePath, StringToArray<ANSICHAR>(*InputShaderSource, InputShaderSource.Len()));
		}
	}

	/** Retrieves the MCPP file loader interface. */
	file_loader GetMcppInterface()
	{
		file_loader Loader;
		Loader.get_file_contents = GetFileContents;
		Loader.user_data = (void*)this;
		return Loader;
	}

private:
	/** Holder for shader contents (string + size). */
	typedef TArray<ANSICHAR> FShaderContents;

	/** MCPP callback for retrieving file contents. */
	static int GetFileContents(void* InUserData, const ANSICHAR* InVirtualFilePath, const ANSICHAR** OutContents, size_t* OutContentSize)
	{
		FMcppFileLoader* This = (FMcppFileLoader*)InUserData;

		FShaderContents* CachedContents = This->CachedFileContents.Find(InVirtualFilePath);
		if (!CachedContents)
		{
			FString VirtualFilePath = (ANSI_TO_TCHAR(InVirtualFilePath));
			FString FileContents;

			if (This->ShaderInput.Environment.IncludeVirtualPathToContentsMap.Contains(VirtualFilePath))
			{
				FileContents = FString(UTF8_TO_TCHAR(
					This->ShaderInput.Environment.IncludeVirtualPathToContentsMap.FindRef(VirtualFilePath).GetData()));
			}
			else
			{
				LoadShaderSourceFile(*VirtualFilePath, FileContents, &This->ShaderOutput.Errors);
			}

			if (FileContents.Len() > 0)
			{
				// Adds a #line 1 "<Absolute file path>" on top of every file content to have nice absolute virtual source
				// file path in error messages.
				FileContents = FString::Printf(TEXT("#line 1 \"%s\"\n%s"), *VirtualFilePath, *FileContents);

				CachedContents = &This->CachedFileContents.Add(InVirtualFilePath, StringToArray<ANSICHAR>(*FileContents, FileContents.Len()));
			}
		}

		if (OutContents)
		{
			*OutContents = CachedContents ? CachedContents->GetData() : NULL;
		}
		if (OutContentSize)
		{
			*OutContentSize = CachedContents ? CachedContents->Num() : 0;
		}

		return CachedContents != nullptr;
	}

	/** Shader input data. */
	const FShaderCompilerInput& ShaderInput;
	/** Shader output data. */
	FShaderCompilerOutput& ShaderOutput;
	/** File contents are cached as needed. */
	TMap<FString,FShaderContents> CachedFileContents;
};

/**
 * Preprocess a shader.
 * @param OutPreprocessedShader - Upon return contains the preprocessed source code.
 * @param ShaderOutput - ShaderOutput to which errors can be added.
 * @param ShaderInput - The shader compiler input.
 * @param AdditionalDefines - Additional defines with which to preprocess the shader.
 * @returns true if the shader is preprocessed without error.
 */
bool PreprocessShader(
	FString& OutPreprocessedShader,
	FShaderCompilerOutput& ShaderOutput,
	const FShaderCompilerInput& ShaderInput,
	const FShaderCompilerDefinitions& AdditionalDefines
	)
{
	// Skip the cache system and directly load the file path (used for debugging)
	if (ShaderInput.bSkipPreprocessedCache)
	{
		return FFileHelper::LoadFileToString(OutPreprocessedShader, *ShaderInput.VirtualSourceFilePath);
	}
	else
	{
		check(CheckVirtualShaderFilePath(ShaderInput.VirtualSourceFilePath));
	}

	FString McppOptions;
	FString McppOutput, McppErrors;
	ANSICHAR* McppOutAnsi = NULL;
	ANSICHAR* McppErrAnsi = NULL;
	bool bSuccess = false;

	// MCPP is not threadsafe.
	static FCriticalSection McppCriticalSection;
	FScopeLock McppLock(&McppCriticalSection);

	FMcppFileLoader FileLoader(ShaderInput, ShaderOutput);

	AddMcppDefines(McppOptions, ShaderInput.Environment.GetDefinitions());
	AddMcppDefines(McppOptions, AdditionalDefines.GetDefinitionMap());
	McppOptions += TEXT(" -V199901L");

	int32 Result = mcpp_run(
		TCHAR_TO_ANSI(*McppOptions),
		TCHAR_TO_ANSI(*ShaderInput.VirtualSourceFilePath),
		&McppOutAnsi,
		&McppErrAnsi,
		FileLoader.GetMcppInterface()
		);

	McppOutput = McppOutAnsi;
	McppErrors = McppErrAnsi;

	if (ParseMcppErrors(ShaderOutput.Errors, ShaderOutput.PragmaDirectives, McppErrors))
	{
		// exchange strings
		FMemory::Memswap( &OutPreprocessedShader, &McppOutput, sizeof(FString) );
		bSuccess = true;
	}

	return bSuccess;
}
