// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


// ShaderCompileWorker.cpp : Defines the entry point for the console application.
//

#include "CoreMinimal.h"
#include "RequiredProgramMainCPPInclude.h"
#include "ShaderCore.h"
#include "ExceptionHandling.h"
#include "IShaderFormat.h"
#include "IShaderFormatModule.h"

#define DEBUG_USING_CONSOLE	0

// this is for the protocol, not the data, bump if FShaderCompilerInput or ProcessInputFromArchive changes (also search for the second one with the same name, todo: put into one header file)
const int32 ShaderCompileWorkerInputVersion = 8;
// this is for the protocol, not the data, bump if FShaderCompilerOutput or WriteToOutputArchive changes (also search for the second one with the same name, todo: put into one header file)
// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
// This version number is checked at load time in Engine\Source\Runtime\Engine\Private\ShaderCompiler\ShaderCompiler.cpp
const int32 ShaderCompileWorkerOutputVersion = 1003;
#else
const int32 ShaderCompileWorkerOutputVersion = 3;
#endif
// this is for the protocol, not the data, bump if FShaderCompilerOutput or WriteToOutputArchive changes (also search for the second one with the same name, todo: put into one header file)
const int32 ShaderCompileWorkerSingleJobHeader = 'S';
// this is for the protocol, not the data, bump if FShaderCompilerOutput or WriteToOutputArchive changes (also search for the second one with the same name, todo: put into one header file)
const int32 ShaderCompileWorkerPipelineJobHeader = 'P';

enum class ESCWErrorCode
{
	Success,
	GeneralCrash,
	BadShaderFormatVersion,
	BadInputVersion,
	BadSingleJobHeader,
	BadPipelineJobHeader,
	CantDeleteInputFile,
	CantSaveOutputFile,
	NoTargetShaderFormatsFound,
	CantCompileForSpecificFormat,
};

double LastCompileTime = 0.0;

enum class EXGEMode
{
	None,
	Xml,
	Intercept
};

static EXGEMode GXGEMode = EXGEMode::None;

inline bool IsUsingXGE()
{
	return GXGEMode != EXGEMode::None;
}

static ESCWErrorCode GFailedErrorCode = ESCWErrorCode::Success;

static void OnXGEJobCompleted(const TCHAR* WorkingDirectory)
{
	if (GXGEMode == EXGEMode::Xml)
	{
	// To signal compilation completion, create a zero length file in the working directory.
		// This is only required in Xml mode.
	delete IFileManager::Get().CreateFileWriter(*FString::Printf(TEXT("%s/Success"), WorkingDirectory), FILEWRITE_EvenIfReadOnly);
	}
}

#if USING_CODE_ANALYSIS
	FUNCTION_NO_RETURN_START static inline void ExitWithoutCrash(ESCWErrorCode ErrorCode, const FString& Message) FUNCTION_NO_RETURN_END;
#endif

static inline void ExitWithoutCrash(ESCWErrorCode ErrorCode, const FString& Message)
{
	GFailedErrorCode = ErrorCode;
	FCString::Snprintf(GErrorExceptionDescription, sizeof(GErrorExceptionDescription), TEXT("%s"), *Message);
	UE_LOG(LogShaders, Fatal, TEXT("%s"), *Message);
}

static const TArray<const IShaderFormat*>& GetShaderFormats()
{
	static bool bInitialized = false;
	static TArray<const IShaderFormat*> Results;

	if (!bInitialized)
	{
		bInitialized = true;
		Results.Empty(Results.Num());

		TArray<FName> Modules;
		FModuleManager::Get().FindModules(SHADERFORMAT_MODULE_WILDCARD, Modules);

		if (!Modules.Num())
		{
			ExitWithoutCrash(ESCWErrorCode::NoTargetShaderFormatsFound, TEXT("No target shader formats found!"));
		}

		for (int32 Index = 0; Index < Modules.Num(); Index++)
		{
			IShaderFormat* Format = FModuleManager::LoadModuleChecked<IShaderFormatModule>(Modules[Index]).GetShaderFormat();
			if (Format != nullptr)
			{
				Results.Add(Format);
			}
		}
	}
	return Results;
}

static const IShaderFormat* FindShaderFormat(FName Name)
{
	const TArray<const IShaderFormat*>& ShaderFormats = GetShaderFormats();	

	for (int32 Index = 0; Index < ShaderFormats.Num(); Index++)
	{
		TArray<FName> Formats;
		ShaderFormats[Index]->GetSupportedFormats(Formats);
		for (int32 FormatIndex = 0; FormatIndex < Formats.Num(); FormatIndex++)
		{
			if (Formats[FormatIndex] == Name)
			{
				return ShaderFormats[Index];
			}
		}
	}

	return nullptr;
}
	
/** Processes a compilation job. */
static void ProcessCompilationJob(const FShaderCompilerInput& Input,FShaderCompilerOutput& Output,const FString& WorkingDirectory)
{
	const IShaderFormat* Compiler = FindShaderFormat(Input.ShaderFormat);
	if (!Compiler)
	{
		ExitWithoutCrash(ESCWErrorCode::CantCompileForSpecificFormat, FString::Printf(TEXT("Can't compile shaders for format %s"), *Input.ShaderFormat.ToString()));
	}

	// Compile the shader directly through the platform dll (directly from the shader dir as the working directory)
	Compiler->CompileShader(Input.ShaderFormat, Input, Output, WorkingDirectory);
}


class FWorkLoop
{
public:
	FWorkLoop(const TCHAR* ParentProcessIdText,const TCHAR* InWorkingDirectory,const TCHAR* InInputFilename,const TCHAR* InOutputFilename, TMap<FString, uint32>& InFormatVersionMap)
	:	ParentProcessId(FCString::Atoi(ParentProcessIdText))
	,	WorkingDirectory(InWorkingDirectory)
	,	InputFilename(InInputFilename)
	,	OutputFilename(InOutputFilename)
	,	InputFilePath(FString(InWorkingDirectory) + InInputFilename)
	,	OutputFilePath(FString(InWorkingDirectory) + InOutputFilename)
	,	FormatVersionMap(InFormatVersionMap)
	{
	}

	void Loop()
	{
		UE_LOG(LogShaders, Log, TEXT("Entering job loop"));

		while(true)
		{
			TArray<FJobResult> SingleJobResults;
			TArray<FPipelineJobResult> PipelineJobResults;

			// Read & Process Input
			{
				FArchive* InputFilePtr = OpenInputFile();
				if(!InputFilePtr)
				{
					break;
				}

				UE_LOG(LogShaders, Log, TEXT("Processing shader"));

				ProcessInputFromArchive(InputFilePtr, SingleJobResults, PipelineJobResults);

				LastCompileTime = FPlatformTime::Seconds();

				// Close the input file.
				delete InputFilePtr;
			}

			// Prepare for output
			FArchive* OutputFilePtr = CreateOutputArchive();
			check(OutputFilePtr);
			WriteToOutputArchive(OutputFilePtr, SingleJobResults, PipelineJobResults);

			// Close the output file.
			delete OutputFilePtr;

			// Change the output file name to requested one
			IFileManager::Get().Move(*OutputFilePath, *TempFilePath);

			if (IsUsingXGE())
			{
				// To signal compilation completion, create a zero length file in the working directory.
				OnXGEJobCompleted(*WorkingDirectory);

				// We only do one pass per process when using XGE.
				break;
			}
		}

		UE_LOG(LogShaders, Log, TEXT("Exiting job loop"));
	}

private:
	struct FJobResult
	{
		FShaderCompilerOutput CompilerOutput;
	};

	struct FPipelineJobResult
	{
		FString PipelineName;
		TArray<FJobResult> SingleJobs;
	};

	const int32 ParentProcessId;
	const FString WorkingDirectory;
	const FString InputFilename;
	const FString OutputFilename;

	const FString InputFilePath;
	const FString OutputFilePath;
	TMap<FString, uint32> FormatVersionMap;
	FString TempFilePath;

	/** Opens an input file, trying multiple times if necessary. */
	FArchive* OpenInputFile()
	{
		FArchive* InputFile = nullptr;
		bool bFirstOpenTry = true;
		while(!InputFile && !GIsRequestingExit)
		{
			// Try to open the input file that we are going to process
			InputFile = IFileManager::Get().CreateFileReader(*InputFilePath,FILEREAD_Silent);

			if(!InputFile && !bFirstOpenTry)
			{
				CheckExitConditions();
				// Give up CPU time while we are waiting
				FPlatformProcess::Sleep(0.01f);
			}
			bFirstOpenTry = false;
		}
		return InputFile;
	}

	void VerifyFormatVersions(TMap<FString, uint32>& ReceivedFormatVersionMap)
	{
		for (auto Pair : ReceivedFormatVersionMap)
		{
			auto* Found = FormatVersionMap.Find(Pair.Key);
			if (Found)
			{
				if (Pair.Value != *Found)
				{
					ExitWithoutCrash(ESCWErrorCode::BadShaderFormatVersion, FString::Printf(TEXT("Mismatched shader version for format %s; did you forget to build ShaderCompilerWorker?"), *Pair.Key, *Found, Pair.Value));
				}
			}
		}
	}

	void ProcessInputFromArchive(FArchive* InputFilePtr, TArray<FJobResult>& OutSingleJobResults, TArray<FPipelineJobResult>& OutPipelineJobResults)
	{
		FArchive& InputFile = *InputFilePtr;
		int32 InputVersion;
		InputFile << InputVersion;
		if (ShaderCompileWorkerInputVersion != InputVersion)
		{
			ExitWithoutCrash(ESCWErrorCode::BadInputVersion, FString::Printf(TEXT("Exiting due to ShaderCompilerWorker expecting input version %d, got %d instead! Did you forget to build ShaderCompilerWorker?"), ShaderCompileWorkerInputVersion, InputVersion));
		}

		TMap<FString, uint32> ReceivedFormatVersionMap;
		InputFile << ReceivedFormatVersionMap;

		VerifyFormatVersions(ReceivedFormatVersionMap);
		
		// Apply shader source directory mappings.
		{
			TMap<FString, FString> DirectoryMappings;
			InputFile << DirectoryMappings;

			FPlatformProcess::ResetAllShaderSourceDirectoryMappings();
			for (const auto& MappingEntry : DirectoryMappings)
			{
				FPlatformProcess::AddShaderSourceDirectoryMapping(MappingEntry.Key, MappingEntry.Value);
			}
		}

		// Individual jobs
		{
			int32 SingleJobHeader = ShaderCompileWorkerSingleJobHeader;
			InputFile << SingleJobHeader;
			if (ShaderCompileWorkerSingleJobHeader != SingleJobHeader)
			{
				ExitWithoutCrash(ESCWErrorCode::BadSingleJobHeader, FString::Printf(TEXT("Exiting due to ShaderCompilerWorker expecting job header %d, got %d instead! Did you forget to build ShaderCompilerWorker?"), ShaderCompileWorkerSingleJobHeader, SingleJobHeader));
			}

			int32 NumBatches = 0;
			InputFile << NumBatches;

			// Flush cache, to make sure we load the latest version of the input file.
			// (Otherwise quick changes to a shader file can result in the wrong output.)
			FlushShaderFileCache();

			for (int32 BatchIndex = 0; BatchIndex < NumBatches; BatchIndex++)
			{
				// Deserialize the job's inputs.
				FShaderCompilerInput CompilerInput;
				InputFile << CompilerInput;

				if (IsValidRef(CompilerInput.SharedEnvironment))
				{
					// Merge the shared environment into the per-shader environment before calling into the compile function
					CompilerInput.Environment.Merge(*CompilerInput.SharedEnvironment);
				}

				// Process the job.
				FShaderCompilerOutput CompilerOutput;
				ProcessCompilationJob(CompilerInput, CompilerOutput, WorkingDirectory);

				// Serialize the job's output.
				FJobResult& JobResult = *new(OutSingleJobResults) FJobResult;
				JobResult.CompilerOutput = CompilerOutput;
			}
		}

		// Shader pipeline jobs
		{
			int32 PipelineJobHeader = ShaderCompileWorkerPipelineJobHeader;
			InputFile << PipelineJobHeader;
			if (ShaderCompileWorkerPipelineJobHeader != PipelineJobHeader)
			{
				ExitWithoutCrash(ESCWErrorCode::BadPipelineJobHeader, FString::Printf(TEXT("Exiting due to ShaderCompilerWorker expecting pipeline job header %d, got %d instead! Did you forget to build ShaderCompilerWorker?"), ShaderCompileWorkerSingleJobHeader, PipelineJobHeader));
			}

			int32 NumPipelines = 0;
			InputFile << NumPipelines;

			for (int32 Index = 0; Index < NumPipelines; ++Index)
			{
				FPipelineJobResult& PipelineJob = *new(OutPipelineJobResults) FPipelineJobResult;

				InputFile << PipelineJob.PipelineName;

				int32 NumStages = 0;
				InputFile << NumStages;

				TArray<FShaderCompilerInput> CompilerInputs;
				CompilerInputs.AddDefaulted(NumStages);

				for (int32 StageIndex = 0; StageIndex < NumStages; ++StageIndex)
				{
					// Deserialize the job's inputs.
					InputFile << CompilerInputs[StageIndex];

					if (IsValidRef(CompilerInputs[StageIndex].SharedEnvironment))
					{
						// Merge the shared environment into the per-shader environment before calling into the compile function
						CompilerInputs[StageIndex].Environment.Merge(*CompilerInputs[StageIndex].SharedEnvironment);
					}
				}

				ProcessShaderPipelineCompilationJob(PipelineJob, CompilerInputs);
			}
		}
	}

	void ProcessShaderPipelineCompilationJob(FPipelineJobResult& PipelineJob, TArray<FShaderCompilerInput>& CompilerInputs)
	{
		checkf(CompilerInputs.Num() > 0, TEXT("Exiting due to Pipeline %s having zero jobs!"), *PipelineJob.PipelineName);

		// Process the job.
		FShaderCompilerOutput FirstCompilerOutput;
		CompilerInputs[0].bCompilingForShaderPipeline = true;
		CompilerInputs[0].bIncludeUsedOutputs = false;
		ProcessCompilationJob(CompilerInputs[0], FirstCompilerOutput, WorkingDirectory);

		// Serialize the job's output.
		{
			FJobResult& JobResult = *new(PipelineJob.SingleJobs) FJobResult;
			JobResult.CompilerOutput = FirstCompilerOutput;
		}

		bool bEnableRemovingUnused = true;

		//#todo-rco: Only remove for pure VS & PS stages
		for (int32 Index = 0; Index < CompilerInputs.Num(); ++Index)
		{
			auto Stage = CompilerInputs[Index].Target.Frequency;
			if (Stage != SF_Vertex && Stage != SF_Pixel)
			{
				bEnableRemovingUnused = false;
				break;
			}
		}

		for (int32 Index = 1; Index < CompilerInputs.Num(); ++Index)
		{
			if (bEnableRemovingUnused && PipelineJob.SingleJobs.Last().CompilerOutput.bSupportsQueryingUsedAttributes)
			{
				CompilerInputs[Index].bIncludeUsedOutputs = true;
				CompilerInputs[Index].bCompilingForShaderPipeline = true;
				CompilerInputs[Index].UsedOutputs = PipelineJob.SingleJobs.Last().CompilerOutput.UsedAttributes;
			}

			FShaderCompilerOutput CompilerOutput;
			ProcessCompilationJob(CompilerInputs[Index], CompilerOutput, WorkingDirectory);

			// Serialize the job's output.
			FJobResult& JobResult = *new(PipelineJob.SingleJobs) FJobResult;
			JobResult.CompilerOutput = CompilerOutput;
		}
	}

	FArchive* CreateOutputArchive()
	{
		FArchive* OutputFilePtr = nullptr;
		const double StartTime = FPlatformTime::Seconds();
		bool bResult = false;

		// It seems XGE does not support deleting files.
		// Don't delete the input file if we are running under Incredibuild.
		// In xml mode, we signal completion by creating a zero byte "Success" file after the output file has been fully written.
		// In intercept mode, completion is signaled by this process terminating.
		if (!IsUsingXGE())
		{
			do 
			{
				// Remove the input file so that it won't get processed more than once
				bResult = IFileManager::Get().Delete(*InputFilePath);
			} 
			while (!bResult && (FPlatformTime::Seconds() - StartTime < 2));

			if (!bResult)
			{
				ExitWithoutCrash(ESCWErrorCode::CantDeleteInputFile, FString::Printf(TEXT("Couldn't delete input file %s, is it readonly?"), *InputFilePath));
			}
		}

		// To make sure that the process waiting for results won't read unfinished output file,
		// we use a temp file name during compilation.
		do
		{
			FGuid Guid;
			FPlatformMisc::CreateGuid(Guid);
			TempFilePath = WorkingDirectory + Guid.ToString();
		} while (IFileManager::Get().FileSize(*TempFilePath) != INDEX_NONE);

		const double StartTime2 = FPlatformTime::Seconds();

		do 
		{
			// Create the output file.
			OutputFilePtr = IFileManager::Get().CreateFileWriter(*TempFilePath,FILEWRITE_EvenIfReadOnly);
		} 
		while (!OutputFilePtr && (FPlatformTime::Seconds() - StartTime2 < 2));
			
		if (!OutputFilePtr)
		{
			ExitWithoutCrash(ESCWErrorCode::CantSaveOutputFile, FString::Printf(TEXT("Couldn't save output file %s"), *TempFilePath));
		}

		return OutputFilePtr;
	}

	void WriteToOutputArchive(FArchive* OutputFilePtr, TArray<FJobResult>& SingleJobResults, TArray<FPipelineJobResult>& PipelineJobResults)
	{
		FArchive& OutputFile = *OutputFilePtr;

		int32 OutputVersion = ShaderCompileWorkerOutputVersion;
		OutputFile << OutputVersion;

		int32 ErrorCode = (int32)ESCWErrorCode::Success;
		OutputFile << ErrorCode;

		int32 ErrorStringLength = 0;
		OutputFile << ErrorStringLength;
		OutputFile << ErrorStringLength;

		{
			int32 SingleJobHeader = ShaderCompileWorkerSingleJobHeader;
			OutputFile << SingleJobHeader;

			int32 NumBatches = SingleJobResults.Num();
			OutputFile << NumBatches;

			for (int32 ResultIndex = 0; ResultIndex < SingleJobResults.Num(); ResultIndex++)
			{
				FJobResult& JobResult = SingleJobResults[ResultIndex];
				OutputFile << JobResult.CompilerOutput;
			}
		}

		{
			int32 PipelineJobHeader = ShaderCompileWorkerPipelineJobHeader;
			OutputFile << PipelineJobHeader;
			int32 NumBatches = PipelineJobResults.Num();
			OutputFile << NumBatches;

			for (int32 ResultIndex = 0; ResultIndex < PipelineJobResults.Num(); ResultIndex++)
			{
				auto& PipelineJob = PipelineJobResults[ResultIndex];
				OutputFile << PipelineJob.PipelineName;
				int32 NumStageJobs = PipelineJob.SingleJobs.Num();
				OutputFile << NumStageJobs;
				for (int32 Index = 0; Index < NumStageJobs; ++Index)
				{
					FJobResult& JobResult = PipelineJob.SingleJobs[Index];
					OutputFile << JobResult.CompilerOutput;
				}
			}
		}
	}

	/** Called in the idle loop, checks for conditions under which the helper should exit */
	void CheckExitConditions()
	{
		if (!InputFilename.Contains(TEXT("Only")))
		{
			UE_LOG(LogShaders, Log, TEXT("InputFilename did not contain 'Only', exiting after one job."));
			FPlatformMisc::RequestExit(false);
		}

#if PLATFORM_MAC || PLATFORM_LINUX
		if (!FPlatformMisc::IsDebuggerPresent() && ParentProcessId > 0)
		{
			// If the parent process is no longer running, exit
			if (!FPlatformProcess::IsApplicationRunning(ParentProcessId))
			{
				FString FilePath = FString(WorkingDirectory) + InputFilename;
				checkf(IFileManager::Get().FileSize(*FilePath) == INDEX_NONE, TEXT("Exiting due to the parent process no longer running and the input file is present!"));
				UE_LOG(LogShaders, Log, TEXT("Parent process no longer running, exiting"));
				FPlatformMisc::RequestExit(false);
			}
		}

		const double CurrentTime = FPlatformTime::Seconds();
		// If we have been idle for 20 seconds then exit
		if (CurrentTime - LastCompileTime > 20.0)
		{
			UE_LOG(LogShaders, Log, TEXT("No jobs found for 20 seconds, exiting"));
			FPlatformMisc::RequestExit(false);
		}
#else
		// Don't do these if the debugger is present
		//@todo - don't do these if Unreal is being debugged either
		if (!IsDebuggerPresent())
		{
			if (ParentProcessId > 0)
			{
				FString FilePath = FString(WorkingDirectory) + InputFilename;

				bool bParentStillRunning = true;
				HANDLE ParentProcessHandle = OpenProcess(SYNCHRONIZE, false, ParentProcessId);
				// If we couldn't open the process then it is no longer running, exit
				if (ParentProcessHandle == nullptr)
				{
					checkf(IFileManager::Get().FileSize(*FilePath) == INDEX_NONE, TEXT("Exiting due to OpenProcess(ParentProcessId) failing and the input file is present!"));
					UE_LOG(LogShaders, Log, TEXT("Couldn't OpenProcess, Parent process no longer running, exiting"));
					FPlatformMisc::RequestExit(false);
				}
				else
				{
					// If we did open the process, that doesn't mean it is still running
					// The process object stays alive as long as there are handles to it
					// We need to check if the process has signaled, which indicates that it has exited
					uint32 WaitResult = WaitForSingleObject(ParentProcessHandle, 0);
					if (WaitResult != WAIT_TIMEOUT)
					{
						checkf(IFileManager::Get().FileSize(*FilePath) == INDEX_NONE, TEXT("Exiting due to WaitForSingleObject(ParentProcessHandle) signaling and the input file is present!"));
						UE_LOG(LogShaders, Log, TEXT("WaitForSingleObject signaled, Parent process no longer running, exiting"));
						FPlatformMisc::RequestExit(false);
					}
					CloseHandle(ParentProcessHandle);
				}
			}

			const double CurrentTime = FPlatformTime::Seconds();
			// If we have been idle for 20 seconds then exit
			if (CurrentTime - LastCompileTime > 20.0)
			{
				UE_LOG(LogShaders, Log, TEXT("No jobs found for 20 seconds, exiting"));
				FPlatformMisc::RequestExit(false);
			}
		}
#endif
	}
};

static FName NAME_PCD3D_SM5(TEXT("PCD3D_SM5"));
static FName NAME_PCD3D_SM4(TEXT("PCD3D_SM4"));
static FName NAME_PCD3D_ES3_1(TEXT("PCD3D_ES31"));
static FName NAME_PCD3D_ES2(TEXT("PCD3D_ES2"));
static FName NAME_GLSL_150(TEXT("GLSL_150"));
static FName NAME_SF_PS4(TEXT("SF_PS4"));
static FName NAME_SF_XBOXONE_D3D12(TEXT("SF_XBOXONE_D3D12"));
static FName NAME_GLSL_430(TEXT("GLSL_430"));
static FName NAME_GLSL_150_ES2(TEXT("GLSL_150_ES2"));
static FName NAME_GLSL_150_ES2_NOUB(TEXT("GLSL_150_ES2_NOUB"));
static FName NAME_GLSL_150_ES31(TEXT("GLSL_150_ES31"));
static FName NAME_GLSL_ES2(TEXT("GLSL_ES2"));
static FName NAME_GLSL_ES2_WEBGL(TEXT("GLSL_ES2_WEBGL"));
static FName NAME_GLSL_ES2_IOS(TEXT("GLSL_ES2_IOS"));
static FName NAME_SF_METAL(TEXT("SF_METAL"));
static FName NAME_SF_METAL_MRT(TEXT("SF_METAL_MRT"));
static FName NAME_GLSL_310_ES_EXT(TEXT("GLSL_310_ES_EXT"));
static FName NAME_SF_METAL_SM5(TEXT("SF_METAL_SM5"));
static FName NAME_VULKAN_ES3_1_ANDROID(TEXT("SF_VULKAN_ES31_ANDROID"));
static FName NAME_VULKAN_ES3_1(TEXT("SF_VULKAN_ES31"));
static FName NAME_VULKAN_SM4_UB(TEXT("SF_VULKAN_SM4_UB"));
static FName NAME_VULKAN_SM4(TEXT("SF_VULKAN_SM4"));
static FName NAME_VULKAN_SM5_UB(TEXT("SF_VULKAN_SM5_UB"));
static FName NAME_VULKAN_SM5(TEXT("SF_VULKAN_SM5"));
static FName NAME_SF_METAL_SM4(TEXT("SF_METAL_SM4"));
static FName NAME_SF_METAL_MACES3_1(TEXT("SF_METAL_MACES3_1"));
static FName NAME_GLSL_ES3_1_ANDROID(TEXT("GLSL_ES3_1_ANDROID"));

static EShaderPlatform FormatNameToEnum(FName ShaderFormat)
{
	if (ShaderFormat == NAME_PCD3D_SM5)			return SP_PCD3D_SM5;
	if (ShaderFormat == NAME_PCD3D_SM4)			return SP_PCD3D_SM4;
	if (ShaderFormat == NAME_PCD3D_ES3_1)		return SP_PCD3D_ES3_1;
	if (ShaderFormat == NAME_PCD3D_ES2)			return SP_PCD3D_ES2;
	if (ShaderFormat == NAME_GLSL_150)			return SP_OPENGL_SM4;
	if (ShaderFormat == NAME_SF_PS4)			return SP_PS4;
	if (ShaderFormat == NAME_SF_XBOXONE_D3D12)	return SP_XBOXONE_D3D12;
	if (ShaderFormat == NAME_GLSL_430)			return SP_OPENGL_SM5;
	if (ShaderFormat == NAME_GLSL_150_ES2)		return SP_OPENGL_PCES2;
	if (ShaderFormat == NAME_GLSL_150_ES2_NOUB)	return SP_OPENGL_PCES2;
	if (ShaderFormat == NAME_GLSL_150_ES31)		return SP_OPENGL_PCES3_1;
	if (ShaderFormat == NAME_GLSL_ES2)			return SP_OPENGL_ES2_ANDROID;
	if (ShaderFormat == NAME_GLSL_ES2_WEBGL)	return SP_OPENGL_ES2_WEBGL;
	if (ShaderFormat == NAME_GLSL_ES2_IOS)		return SP_OPENGL_ES2_IOS;
	if (ShaderFormat == NAME_SF_METAL)			return SP_METAL;
	if (ShaderFormat == NAME_SF_METAL_MRT)		return SP_METAL_MRT;
	if (ShaderFormat == NAME_GLSL_310_ES_EXT)	return SP_OPENGL_ES31_EXT;
	if (ShaderFormat == NAME_SF_METAL_SM5)		return SP_METAL_SM5;
	if (ShaderFormat == NAME_VULKAN_SM4)			return SP_VULKAN_SM4;
	if (ShaderFormat == NAME_VULKAN_SM5)			return SP_VULKAN_SM5;
	if (ShaderFormat == NAME_VULKAN_ES3_1_ANDROID)	return SP_VULKAN_ES3_1_ANDROID;
	if (ShaderFormat == NAME_VULKAN_ES3_1)			return SP_VULKAN_PCES3_1;
	if (ShaderFormat == NAME_VULKAN_SM4_UB)		return SP_VULKAN_SM4;
	if (ShaderFormat == NAME_VULKAN_SM5_UB)		return SP_VULKAN_SM5;
	if (ShaderFormat == NAME_SF_METAL_SM4)		return SP_METAL_SM4;
	if (ShaderFormat == NAME_SF_METAL_MACES3_1)	return SP_METAL_MACES3_1;
	if (ShaderFormat == NAME_GLSL_ES3_1_ANDROID) return SP_OPENGL_ES3_1_ANDROID;
	return SP_NumPlatforms;
}

static void DirectCompile(const TArray<const class IShaderFormat*>& ShaderFormats)
{
	// Find all the info required for compiling a single shader
	TArray<FString> Tokens, Switches;
	FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);

	FString InputFile;

	FName FormatName;
	FString Entry = TEXT("Main");
	bool bPipeline = false;
	bool bUseMCPP = false;
	EShaderFrequency Frequency = SF_Pixel;
	TArray<FString> UsedOutputs;
	bool bIncludeUsedOutputs = false;
	uint64 CFlags = 0;
	for (const FString& Token : Tokens)
	{
		if (Switches.Contains(Token))
		{
			if (Token.StartsWith(TEXT("format=")))
			{
				FormatName = FName(*Token.RightChop(7));
			}
			else if (Token.StartsWith(TEXT("entry=")))
			{
				Entry = Token.RightChop(6);
			}
			else if (Token.StartsWith(TEXT("cflags=")))
			{
				CFlags = FCString::Atoi64(*Token.RightChop(7));
			}
			else if (!FCString::Strcmp(*Token, TEXT("ps")))
			{
				Frequency = SF_Pixel;
			}
			else if (!FCString::Strcmp(*Token, TEXT("vs")))
			{
				Frequency = SF_Vertex;
			}
			else if (!FCString::Strcmp(*Token, TEXT("hs")))
			{
				Frequency = SF_Hull;
			}
			else if (!FCString::Strcmp(*Token, TEXT("ds")))
			{
				Frequency = SF_Domain;
			}
			else if (!FCString::Strcmp(*Token, TEXT("gs")))
			{
				Frequency = SF_Geometry;
			}
			else if (!FCString::Strcmp(*Token, TEXT("cs")))
			{
				Frequency = SF_Compute;
			}
			else if (!FCString::Strcmp(*Token, TEXT("pipeline")))
			{
				bPipeline = true;
			}
			else if (!FCString::Strcmp(*Token, TEXT("mcpp")))
			{
				bUseMCPP = true;
			}
			else if (Token.StartsWith(TEXT("usedoutputs=")))
			{
				FString Outputs = Token.RightChop(12);
				bIncludeUsedOutputs = true;
				FString LHS, RHS;
				while (Outputs.Split(TEXT("+"), &LHS, &RHS))
				{
					Outputs = RHS;
					UsedOutputs.Add(LHS);
				}
				UsedOutputs.Add(Outputs);
			}
		}
		else
		{
			if (InputFile.Len() == 0)
			{
				InputFile = Token;
			}
		}
	}

	FString Dir = FPlatformProcess::UserTempDir();

	FShaderCompilerInput Input;
	Input.EntryPointName = Entry;
	Input.ShaderFormat = FormatName;
	Input.VirtualSourceFilePath = InputFile;
	Input.Target.Platform =  FormatNameToEnum(FormatName);
	Input.Target.Frequency = Frequency;
	Input.bSkipPreprocessedCache = !bUseMCPP;

	uint32 ResourceIndex = 0;
	auto AddResourceTableEntry = [&ResourceIndex](TMap<FString, FResourceTableEntry>& Map, const FString& Name, const FString& UBName, int32 Type)
	{
		FResourceTableEntry LambdaEntry;
		LambdaEntry.UniformBufferName = UBName;
		LambdaEntry.Type = Type;
		LambdaEntry.ResourceIndex = ResourceIndex;
		Map.Add(Name, LambdaEntry);
		++ResourceIndex;
	};

	uint32 CFlag = 0;
	while (CFlags != 0)
	{
		if ((CFlags & 1) != 0)
		{
			Input.Environment.CompilerFlags.Add(CFlag);
		}
		CFlags = (CFlags >> (uint64)1);
		++CFlag;
	}

	Input.bCompilingForShaderPipeline = bPipeline;
	Input.bIncludeUsedOutputs = bIncludeUsedOutputs;
	Input.UsedOutputs = UsedOutputs;

	FShaderCompilerOutput Output;

	for (const IShaderFormat* Format : ShaderFormats)
	{
		TArray<FName> SupportedFormats;
		Format->GetSupportedFormats(SupportedFormats);
		for (FName SupportedName : SupportedFormats)
		{
			if (SupportedName == FormatName)
			{
				Format->CompileShader(FormatName, Input, Output, Dir);
				return;
			}
		}
	}

	UE_LOG(LogShaders, Warning, TEXT("Unable to find shader compiler backend for format %s!"), *FormatName.ToString());
}


/** 
 * Main entrypoint, guarded by a try ... except.
 * This expects 4 parameters:
 *		The image path and name
 *		The working directory path, which has to be unique to the instigating process and thread.
 *		The parent process Id
 *		The thread Id corresponding to this worker
 */
static int32 GuardedMain(int32 argc, TCHAR* argv[], bool bDirectMode)
{
	GEngineLoop.PreInit(argc, argv, TEXT("-NOPACKAGECACHE -Multiprocess"));
#if DEBUG_USING_CONSOLE
	GLogConsole->Show( true );
#endif

	// We just enumerate the shader formats here for debugging.
	const TArray<const class IShaderFormat*>& ShaderFormats = GetShaderFormats();
	check(ShaderFormats.Num());
	TMap<FString, uint32> FormatVersionMap;
	for (int32 Index = 0; Index < ShaderFormats.Num(); Index++)
	{
		TArray<FName> OutFormats;
		ShaderFormats[Index]->GetSupportedFormats(OutFormats);
		check(OutFormats.Num());
		for (int32 InnerIndex = 0; InnerIndex < OutFormats.Num(); InnerIndex++)
		{
			UE_LOG(LogShaders, Display, TEXT("Available Shader Format %s"), *OutFormats[InnerIndex].ToString());
			uint32 Version = ShaderFormats[Index]->GetVersion(OutFormats[InnerIndex]);
			FormatVersionMap.Add(OutFormats[InnerIndex].ToString(), Version);
		}
	}

	LastCompileTime = FPlatformTime::Seconds();

	if (bDirectMode)
	{
		DirectCompile(ShaderFormats);
	}
	else
	{
#if PLATFORM_WINDOWS
		//@todo - would be nice to change application name or description to have the ThreadId in it for debugging purposes
		SetConsoleTitle(argv[3]);
#endif

		FWorkLoop WorkLoop(argv[2], argv[1], argv[4], argv[5], FormatVersionMap);
		WorkLoop.Loop();
	}

	return 0;
}

static int32 GuardedMainWrapper(int32 ArgC, TCHAR* ArgV[], const TCHAR* CrashOutputFile, bool bDirectMode)
{
	// We need to know whether we are using XGE now, in case an exception
	// is thrown before we parse the command line inside GuardedMain.
	if ((ArgC > 6) && FCString::Strcmp(ArgV[6], TEXT("-xge_int")) == 0)
	{
		GXGEMode = EXGEMode::Intercept;
	}
	else if ((ArgC > 6) && FCString::Strcmp(ArgV[6], TEXT("-xge_xml")) == 0)
	{
		GXGEMode = EXGEMode::Xml;
	}
	else
	{
		GXGEMode = EXGEMode::None;
	}

	int32 ReturnCode = 0;
#if PLATFORM_WINDOWS
	if (FPlatformMisc::IsDebuggerPresent())
#endif
	{
		ReturnCode = GuardedMain(ArgC, ArgV, bDirectMode);
	}
#if PLATFORM_WINDOWS
	else
	{
		// Don't want 32 dialogs popping up when SCW fails
		GUseCrashReportClient = false;
		__try
		{
			GIsGuarded = 1;
			ReturnCode = GuardedMain(ArgC, ArgV, bDirectMode);
			GIsGuarded = 0;
		}
		__except( ReportCrash( GetExceptionInformation() ) )
		{
			FArchive& OutputFile = *IFileManager::Get().CreateFileWriter(CrashOutputFile,FILEWRITE_NoFail);

			int32 OutputVersion = ShaderCompileWorkerOutputVersion;
			OutputFile << OutputVersion;

			if (GFailedErrorCode == ESCWErrorCode::Success)
			{
				GFailedErrorCode = ESCWErrorCode::GeneralCrash;
			}
			int32 ErrorCode = (int32)GFailedErrorCode;
			OutputFile << ErrorCode;

			// Note: Can't use FStrings here as SEH can't be used with destructors
			int32 CallstackLength = FCString::Strlen(GErrorHist);
			OutputFile << CallstackLength;

			int32 ExceptionInfoLength = FCString::Strlen(GErrorExceptionDescription);
			OutputFile << ExceptionInfoLength;

			OutputFile.Serialize(GErrorHist, CallstackLength * sizeof(TCHAR));
			OutputFile.Serialize(GErrorExceptionDescription, ExceptionInfoLength * sizeof(TCHAR));

			int32 NumBatches = 0;
			OutputFile << NumBatches;

			// Close the output file.
			delete &OutputFile;

			if (IsUsingXGE())
			{
				ReturnCode = 1;
				OnXGEJobCompleted(ArgV[1]);
			}
		}
	}
#endif

	FEngineLoop::AppPreExit();
	FModuleManager::Get().UnloadModulesAtShutdown();
	FEngineLoop::AppExit();

	return ReturnCode;
}

IMPLEMENT_APPLICATION(ShaderCompileWorker, "ShaderCompileWorker")


/**
 * Application entry point
 *
 * @param	ArgC	Command-line argument count
 * @param	ArgV	Argument strings
 */

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
#if PLATFORM_WINDOWS
	// Redirect for special XGE utilities...
	extern bool XGEMain(int ArgC, TCHAR* ArgV[], int32& ReturnCode);
	{
		int32 ReturnCode;
		if (XGEMain(ArgC, ArgV, ReturnCode))
		{
			return ReturnCode;
				}
	}
#endif

	TCHAR OutputFilePath[PLATFORM_MAX_FILEPATH_LENGTH] = TEXT("");
	bool bDirectMode = false;
	for (int32 Index = 1; Index < ArgC; ++Index)
	{
		if (FCString::Strcmp(ArgV[Index], TEXT("-directcompile")) == 0)
		{
			bDirectMode = true;
			break;
		}
	}

	if (!bDirectMode)
	{
		if (ArgC < 6)
		{
			printf("ShaderCompileWorker is called by UE4, it requires specific command like arguments.\n");
			return -1;
		}

		// Game exe can pass any number of parameters through with appGetSubprocessCommandline
		// so just make sure we have at least the minimum number of parameters.
		check(ArgC >= 6);

		FCString::Strncpy(OutputFilePath, ArgV[1], PLATFORM_MAX_FILEPATH_LENGTH);
		FCString::Strncat(OutputFilePath, ArgV[5], PLATFORM_MAX_FILEPATH_LENGTH);
	}

	return GuardedMainWrapper(ArgC, ArgV, OutputFilePath, bDirectMode);
}
