// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCompiler.cpp: Platform independent shader compilations.
=============================================================================*/

#include "ShaderCompiler.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "HAL/ExceptionHandling.h"
#include "Serialization/NameAsStringProxyArchive.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Misc/Guid.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Materials/MaterialInterface.h"
#include "StaticBoundShaderState.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "EditorSupportDelegates.h"
#include "GlobalShader.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/IShaderFormat.h"
#include "Interfaces/IShaderFormatModule.h"
#include "RendererInterface.h"
#include "EngineModule.h"
#include "ComponentRecreateRenderStateContext.h"
#include "Components/PrimitiveComponent.h"
#include "DerivedDataCacheInterface.h"
#include "ShaderDerivedDataVersion.h"
#include "Misc/FileHelper.h"
#include "StaticBoundShaderState.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "ProfilingDebugging/CookStats.h"
#include "SceneInterface.h"

#define LOCTEXT_NAMESPACE "ShaderCompiler"

DEFINE_LOG_CATEGORY(LogShaderCompilers);

#if ENABLE_COOK_STATS
namespace GlobalShaderCookStats
{
	FCookStats::FDDCResourceUsageStats UsageStats;
	static int32 ShadersCompiled = 0;

	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		UsageStats.LogStats(AddStat, TEXT("GlobalShader.Usage"), TEXT(""));
		AddStat(TEXT("GlobalShader.Misc"), FCookStatsManager::CreateKeyValueArray(
			TEXT("ShadersCompiled"), ShadersCompiled
		));
	});
}
#endif

FString GetGlobalShaderMapDDCKey()
{
	return FString(GLOBALSHADERMAP_DERIVEDDATA_VER);
}

FString GetMaterialShaderMapDDCKey()
{
	return FString(MATERIALSHADERMAP_DERIVEDDATA_VER);
}

// this is for the protocol, not the data, bump if FShaderCompilerInput or ProcessInputFromArchive changes (also search for the second one with the same name, todo: put into one header file)
const int32 ShaderCompileWorkerInputVersion = 8;
// this is for the protocol, not the data, bump if FShaderCompilerOutput or WriteToOutputArchive changes (also search for the second one with the same name, todo: put into one header file)
// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
const int32 ShaderCompileWorkerOutputVersion = 1003;
#else
const int32 ShaderCompileWorkerOutputVersion = 3;
#endif
// NVCHANGE_END: Add VXGI
// this is for the protocol, not the data, bump if FShaderCompilerOutput or WriteToOutputArchive changes (also search for the second one with the same name, todo: put into one header file)
const int32 ShaderCompileWorkerSingleJobHeader = 'S';
// this is for the protocol, not the data, bump if FShaderCompilerOutput or WriteToOutputArchive changes (also search for the second one with the same name, todo: put into one header file)
const int32 ShaderCompileWorkerPipelineJobHeader = 'P';

static void ModalErrorOrLog(const FString& Text)
{
	if (FPlatformProperties::SupportsWindowedMode())
	{
		UE_LOG(LogShaderCompilers, Error, TEXT("%s"), *Text);
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Text));
		FPlatformMisc::RequestExit(false);
		return;
	}
	else
	{
		UE_LOG(LogShaderCompilers, Fatal, TEXT("%s"), *Text);
	}
}

// Set to 1 to debug ShaderCompilerWorker.exe. Set a breakpoint in LaunchWorker() to get the cmd-line.
#define DEBUG_SHADERCOMPILEWORKER 0

// Default value comes from bPromptToRetryFailedShaderCompiles in BaseEngine.ini
// This is set as a global variable to allow changing in the debugger even in release
// For example if there are a lot of content shader compile errors you want to skip over without relaunching
bool GRetryShaderCompilation = false;

static int32 GDumpShaderDebugInfo = 0;
static FAutoConsoleVariableRef CVarDumpShaderDebugInfo(
	TEXT("r.DumpShaderDebugInfo"),
	GDumpShaderDebugInfo,
	TEXT("When set to 1, will cause any material shaders that are then compiled to dump debug info to GameName/Saved/ShaderDebugInfo\n")
	TEXT("The debug info is platform dependent, but usually includes a preprocessed version of the shader source.\n")
	TEXT("Global shaders automatically dump debug info if r.ShaderDevelopmentMode is enabled, this cvar is not necessary.\n")
	TEXT("On iOS, if the PowerVR graphics SDK is installed to the default path, the PowerVR shader compiler will be called and errors will be reported during the cook.")
	);

static int32 GDumpShaderDebugInfoShort = 0;
static FAutoConsoleVariableRef CVarDumpShaderDebugShortNames(
	TEXT("r.DumpShaderDebugShortNames"),
	GDumpShaderDebugInfoShort,
	TEXT("Only valid when r.DumpShaderDebugInfo=1.\n")
	TEXT("When set to 1, will shorten names factory and shader type folder names to avoid issues with long paths.")
	);

static int32 GDumpShaderDebugInfoSCWCommandLine = 0;
static FAutoConsoleVariableRef CVarDumpShaderDebugSCWCommandLine(
	TEXT("r.DumpShaderDebugWorkerCommandLine"),
	GDumpShaderDebugInfoSCWCommandLine,
	TEXT("Only valid when r.DumpShaderDebugInfo=1.\n")
	TEXT("When set to 1, it will generate a file that can be used with ShaderCompileWorker's -directcompile.")
	);

static int32 GDumpSCWJobInfoOnCrash = 0;
static FAutoConsoleVariableRef CVarDumpSCWJobInfoOnCrash(
	TEXT("r.DumpSCWQueuedJobs"),
	GDumpSCWJobInfoOnCrash,
	TEXT("When set to 1, it will dump a job list to help track down crashes that happened on ShaderCompileWorker.")
);

static int32 GShowShaderWarnings = 0;
static FAutoConsoleVariableRef CVarShowShaderWarnings(
	TEXT("r.ShowShaderCompilerWarnings"),
	GShowShaderWarnings,
	TEXT("When set to 1, will display all warnings.")
	);

static TAutoConsoleVariable<int32> CVarKeepShaderDebugData(
	TEXT("r.Shaders.KeepDebugInfo"),
	0,
	TEXT("Whether to keep shader reflection and debug data from shader bytecode, default is to strip.  When using graphical debuggers like Nsight it can be useful to enable this on startup."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarOptimizeShaders(
	TEXT("r.Shaders.Optimize"),
	1,
	TEXT("Whether to optimize shaders.  When using graphical debuggers like Nsight it can be useful to disable this on startup."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarShaderFastMath(
	TEXT("r.Shaders.FastMath"),
	1,
	TEXT("Whether to use fast-math optimisations in shaders."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarShaderZeroInitialise(
	TEXT("r.Shaders.ZeroInitialise"),
	1,
	TEXT("Whether to enforce zero initialise local variables of primitive type in shaders. Defaults to 1 (enabled). Not all shader languages can omit zero initialisation."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarShaderBoundsChecking(
	TEXT("r.Shaders.BoundsChecking"),
	1,
	TEXT("Whether to enforce bounds-checking & flush-to-zero/ignore for buffer reads & writes in shaders. Defaults to 1 (enabled). Not all shader languages can omit bounds checking."),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarShaderFlowControl(
	TEXT("r.Shaders.FlowControlMode"),
	0,
	TEXT("Specifies whether the shader compiler should preserve or unroll flow-control in shader code.\n")
	TEXT("This is primarily a debugging aid and will override any per-shader or per-material settings if not left at the default value (0).\n")
	TEXT("\t0: Off (Default) - Entirely at the discretion of the platform compiler or the specific shader/material.\n")
	TEXT("\t1: Prefer - Attempt to preserve flow-control.\n")
	TEXT("\t2: Avoid - Attempt to unroll and flatten flow-control.\n"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarD3DRemoveUnusedInterpolators(
	TEXT("r.D3D.RemoveUnusedInterpolators"),
	1,
	TEXT("Enables removing unused interpolators mode when compiling pipelines for D3D.\n")
	TEXT(" -1: Do not actually remove, but make the app think it did (for debugging)\n")
	TEXT(" 0: Disable (default)\n")
	TEXT(" 1: Enable removing unused"),
	ECVF_ReadOnly
	);

int32 GCreateShadersOnLoad = 0;
static FAutoConsoleVariableRef CVarCreateShadersOnLoad(
	TEXT("r.CreateShadersOnLoad"),
	GCreateShadersOnLoad,
	TEXT("Whether to create shaders on load, which can reduce hitching, but use more memory.  Otherwise they will be created as needed.")
);

extern bool CompileShaderPipeline(const IShaderFormat* Compiler, FName Format, FShaderPipelineCompileJob* PipelineJob, const FString& Dir);

#if ENABLE_COOK_STATS
#include "ProfilingDebugging/ScopedTimers.h"
namespace ShaderCompilerCookStats
{
	static double BlockingTimeSec = 0.0;
	static double GlobalBeginCompileShaderTimeSec = 0.0;
	static int32 GlobalBeginCompileShaderCalls = 0;
	static double ProcessAsyncResultsTimeSec = 0.0;
	static double AsyncCompileTimeSec = 0.0;

	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		AddStat(TEXT("ShaderCompiler"), FCookStatsManager::CreateKeyValueArray(
			TEXT("BlockingTimeSec"), BlockingTimeSec,
			TEXT("AsyncCompileTimeSec"), AsyncCompileTimeSec,
			TEXT("GlobalBeginCompileShaderTimeSec"), GlobalBeginCompileShaderTimeSec,
			TEXT("GlobalBeginCompileShaderCalls"), GlobalBeginCompileShaderCalls,
			TEXT("ProcessAsyncResultsTimeSec"), ProcessAsyncResultsTimeSec
			));
	});
}
#endif

// Make functions so the crash reporter can disambiguate the actual error because of the different callstacks
namespace SCWErrorCode
{
	enum EErrors
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

	void HandleGeneralCrash(const TCHAR* ExceptionInfo, const TCHAR* Callstack)
	{
		UE_LOG(LogShaderCompilers, Fatal, TEXT("ShaderCompileWorker crashed!\n%s\n\t%s"), ExceptionInfo, Callstack);
	}

	void HandleBadShaderFormatVersion(const TCHAR* Data)
	{
		ModalErrorOrLog(FString::Printf(TEXT("ShaderCompileWorker failed:\n%s\n"), Data));
	}

	void HandleBadInputVersion(const TCHAR* Data)
	{
		ModalErrorOrLog(FString::Printf(TEXT("ShaderCompileWorker failed:\n%s\n"), Data));
	}

	void HandleBadSingleJobHeader(const TCHAR* Data)
	{
		ModalErrorOrLog(FString::Printf(TEXT("ShaderCompileWorker failed:\n%s\n"), Data));
	}

	void HandleBadPipelineJobHeader(const TCHAR* Data)
	{
		ModalErrorOrLog(FString::Printf(TEXT("ShaderCompileWorker failed:\n%s\n"), Data));
	}

	void HandleCantDeleteInputFile(const TCHAR* Data)
	{
		ModalErrorOrLog(FString::Printf(TEXT("ShaderCompileWorker failed:\n%s\n"), Data));
	}

	void HandleCantSaveOutputFile(const TCHAR* Data)
	{
		ModalErrorOrLog(FString::Printf(TEXT("ShaderCompileWorker failed:\n%s\n"), Data));
	}

	void HandleNoTargetShaderFormatsFound(const TCHAR* Data)
	{
		ModalErrorOrLog(FString::Printf(TEXT("ShaderCompileWorker failed:\n%s\n"), Data));
	}

	void HandleCantCompileForSpecificFormat(const TCHAR* Data)
	{
		ModalErrorOrLog(FString::Printf(TEXT("ShaderCompileWorker failed:\n%s\n"), Data));
	}
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
			UE_LOG(LogShaders, Error, TEXT("No target shader formats found!"));
		}

		for (int32 Index = 0; Index < Modules.Num(); Index++)
		{
			IShaderFormatModule* Module = FModuleManager::GetModulePtr<IShaderFormatModule>(Modules[Index]);
			if (Module != nullptr)
			{
				IShaderFormat* Format = Module->GetShaderFormat();

				if (Format != nullptr)
				{
					Results.Add(Format);
				}
			}
			else
			{
				UE_LOG(LogShaders, Display, TEXT("Unable to load module %s, skipping its shader formats."), *Modules[Index].ToString());
			}
		}
	}
	return Results;
}

static inline void GetFormatVersionMap(TMap<FString, uint32>& OutFormatVersionMap)
{
	if (OutFormatVersionMap.Num() == 0)
	{
		const TArray<const class IShaderFormat*>& ShaderFormats = GetShaderFormats();
		check(ShaderFormats.Num());
		for (int32 Index = 0; Index < ShaderFormats.Num(); Index++)
		{
			TArray<FName> OutFormats;
			ShaderFormats[Index]->GetSupportedFormats(OutFormats);
			check(OutFormats.Num());
			for (int32 InnerIndex = 0; InnerIndex < OutFormats.Num(); InnerIndex++)
			{
				uint32 Version = ShaderFormats[Index]->GetVersion(OutFormats[InnerIndex]);
				OutFormatVersionMap.Add(OutFormats[InnerIndex].ToString(), Version);
			}
		}
	}
}

static int32 GetNumTotalJobs(const TArray<FShaderCommonCompileJob*>& Jobs)
{
	int32 NumJobs = 0;
	for (int32 Index = 0; Index < Jobs.Num(); ++Index)
	{
		auto* PipelineJob = Jobs[Index]->GetShaderPipelineJob();
		NumJobs += PipelineJob ? PipelineJob->StageJobs.Num() : 1;
	}

	return NumJobs;
}

static void SplitJobsByType(const TArray<FShaderCommonCompileJob*>& QueuedJobs, TArray<FShaderCompileJob*>& OutQueuedSingleJobs, TArray<FShaderPipelineCompileJob*>& OutQueuedPipelineJobs)
{
	for (int32 Index = 0; Index < QueuedJobs.Num(); ++Index)
	{
		auto* CommonJob = QueuedJobs[Index];
		auto* PipelineJob = CommonJob->GetShaderPipelineJob();
		if (PipelineJob)
		{
			OutQueuedPipelineJobs.Add(PipelineJob);
		}
		else
		{
			auto* SingleJob = CommonJob->GetSingleShaderJob();
			check(SingleJob);
			OutQueuedSingleJobs.Add(SingleJob);
		}
	}
}

// Serialize Queued Job information
bool FShaderCompileUtilities::DoWriteTasks(const TArray<FShaderCommonCompileJob*>& QueuedJobs, FArchive& TransferFile)
{
	int32 InputVersion = ShaderCompileWorkerInputVersion;
	TransferFile << InputVersion;

	static TMap<FString, uint32> FormatVersionMap;
	GetFormatVersionMap(FormatVersionMap);

	TransferFile << FormatVersionMap;

	TMap<FString, FString> ShaderSourceDirectoryMappings = FPlatformProcess::AllShaderSourceDirectoryMappings();
	TransferFile << ShaderSourceDirectoryMappings;

	TArray<FShaderCompileJob*> QueuedSingleJobs;
	TArray<FShaderPipelineCompileJob*> QueuedPipelineJobs;
	SplitJobsByType(QueuedJobs, QueuedSingleJobs, QueuedPipelineJobs);

	// Write individual shader jobs
	{
		int32 SingleJobHeader = ShaderCompileWorkerSingleJobHeader;
		TransferFile << SingleJobHeader;

		int32 NumBatches = QueuedSingleJobs.Num();
		TransferFile << NumBatches;

		// Serialize all the batched jobs
		for (int32 JobIndex = 0; JobIndex < QueuedSingleJobs.Num(); JobIndex++)
		{
			TransferFile << QueuedSingleJobs[JobIndex]->Input;
		}
	}

	// Write shader pipeline jobs
	{
		int32 PipelineJobHeader = ShaderCompileWorkerPipelineJobHeader;
		TransferFile << PipelineJobHeader;

		int32 NumBatches = QueuedPipelineJobs.Num();
		TransferFile << NumBatches;
		for (int32 JobIndex = 0; JobIndex < QueuedPipelineJobs.Num(); JobIndex++)
		{
			auto* PipelineJob = QueuedPipelineJobs[JobIndex];
			FString PipelineName = PipelineJob->ShaderPipeline->GetName();
			TransferFile << PipelineName;
			int32 NumStageJobs = PipelineJob->StageJobs.Num();
			TransferFile << NumStageJobs;
			for (int32 Index = 0; Index < NumStageJobs; Index++)
			{
				TransferFile << PipelineJob->StageJobs[Index]->GetSingleShaderJob()->Input;
			}
		}
	}

	return TransferFile.Close();
}

static void ProcessErrors(const FShaderCompileJob& CurrentJob, TArray<FString>& UniqueErrors, FString& ErrorString)
{
	for (int32 ErrorIndex = 0; ErrorIndex < CurrentJob.Output.Errors.Num(); ErrorIndex++)
	{
		FShaderCompilerError CurrentError = CurrentJob.Output.Errors[ErrorIndex];
		int32 UniqueError = INDEX_NONE;

		if (UniqueErrors.Find(CurrentError.GetErrorString(), UniqueError))
		{
			// This unique error is being processed, remove it from the array
			UniqueErrors.RemoveAt(UniqueError);

			// Remap filenames
			if (CurrentError.ErrorVirtualFilePath == TEXT("/Engine/Generated/Material.ush"))
			{
				// MaterialTemplate.usf is dynamically included as Material.usf
				// Currently the material translator does not add new lines when filling out MaterialTemplate.usf,
				// So we don't need the actual filled out version to find the line of a code bug.
				CurrentError.ErrorVirtualFilePath = TEXT("/Engine/Private/MaterialTemplate.ush");
			}
			else if (CurrentError.ErrorVirtualFilePath.Contains(TEXT("memory")))
			{
				check(CurrentJob.ShaderType);

				// Files passed to the shader compiler through memory will be named memory
				// Only the shader's main file is passed through memory without a filename
				CurrentError.ErrorVirtualFilePath = FString(CurrentJob.ShaderType->GetShaderFilename());
			}
			else if (CurrentError.ErrorVirtualFilePath == TEXT("/Engine/Generated/VertexFactory.ush"))
			{
				// VertexFactory.usf is dynamically included from whichever vertex factory the shader was compiled with.
				check(CurrentJob.VFType);
				CurrentError.ErrorVirtualFilePath = FString(CurrentJob.VFType->GetShaderFilename());
			}
			else if (CurrentError.ErrorVirtualFilePath == TEXT("") && CurrentJob.ShaderType)
			{
				// Some shader compiler errors won't have a file and line number, so we just assume the error happened in file containing the entrypoint function.
				CurrentError.ErrorVirtualFilePath = FString(CurrentJob.ShaderType->GetShaderFilename());
			}

			FString UniqueErrorPrefix;

			if (CurrentJob.ShaderType)
			{
				// Construct a path that will enable VS.NET to find the shader file, relative to the solution
				const FString SolutionPath = FPaths::RootDir();
				FString ShaderFilePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*CurrentError.GetShaderSourceFilePath());
				UniqueErrorPrefix = FString::Printf(TEXT("%s(%s): Shader %s, VF %s:\n\t"),
					*ShaderFilePath,
					*CurrentError.ErrorLineString,
					CurrentJob.ShaderType->GetName(),
					CurrentJob.VFType ? CurrentJob.VFType->GetName() : TEXT("None"));
			}
			else
			{
				UniqueErrorPrefix = FString::Printf(TEXT("%s(0): "),
					*CurrentJob.Input.VirtualSourceFilePath);
			}

			FString UniqueErrorString = UniqueErrorPrefix + CurrentError.StrippedErrorMessage + TEXT("\n");

			if (GIsBuildMachine)
			{
				// Format everything on one line, and with the correct verbosity, so we can display proper errors in the failure logs.
				UE_LOG(LogShaderCompilers, Error, TEXT("%s%s"), *UniqueErrorPrefix.Replace(TEXT("\n"), TEXT("")), *CurrentError.StrippedErrorMessage);
			}
			else if (FPlatformMisc::IsDebuggerPresent() && !GIsBuildMachine)
			{
				// Using OutputDebugString to avoid any text getting added before the filename,
				// Which will throw off VS.NET's ability to take you directly to the file and line of the error when double clicking it in the output window.
				FPlatformMisc::LowLevelOutputDebugStringf(*UniqueErrorString);
			}
			else
			{
				UE_LOG(LogShaderCompilers, Warning, TEXT("%s"), *UniqueErrorString);
			}

			ErrorString += UniqueErrorString;
		}
	}
}

static void ReadSingleJob(FShaderCompileJob* CurrentJob, FArchive& OutputFile)
{
	check(!CurrentJob->bFinalized);
	CurrentJob->bFinalized = true;

	// Deserialize the shader compilation output.
	OutputFile << CurrentJob->Output;

	// Generate a hash of the output and cache it
	// The shader processing this output will use it to search for existing FShaderResources
	CurrentJob->Output.GenerateOutputHash();
	CurrentJob->bSucceeded = CurrentJob->Output.bSucceeded;
};

// Process results from Worker Process
void FShaderCompileUtilities::DoReadTaskResults(const TArray<FShaderCommonCompileJob*>& QueuedJobs, FArchive& OutputFile)
{
	int32 OutputVersion = ShaderCompileWorkerOutputVersion;
	OutputFile << OutputVersion;

	if (ShaderCompileWorkerOutputVersion != OutputVersion)
	{
		FString Text = FString::Printf(TEXT("Expecting ShaderCompilerWorker output version %d, got %d instead! Forgot to build ShaderCompilerWorker?"), ShaderCompileWorkerOutputVersion, OutputVersion);
		ModalErrorOrLog(Text);
	}

	int32 ErrorCode;
	OutputFile << ErrorCode;

	int32 CallstackLength = 0;
	OutputFile << CallstackLength;

	int32 ExceptionInfoLength = 0;
	OutputFile << ExceptionInfoLength;

	// Worker crashed
	if (ErrorCode != SCWErrorCode::Success)
	{
		TArray<TCHAR> Callstack;
		Callstack.AddUninitialized(CallstackLength + 1);
		OutputFile.Serialize(Callstack.GetData(), CallstackLength * sizeof(TCHAR));
		Callstack[CallstackLength] = 0;

		TArray<TCHAR> ExceptionInfo;
		ExceptionInfo.AddUninitialized(ExceptionInfoLength + 1);
		OutputFile.Serialize(ExceptionInfo.GetData(), ExceptionInfoLength * sizeof(TCHAR));
		ExceptionInfo[ExceptionInfoLength] = 0;

		// One entry per error code as we want to have different callstacks for crash reporter...
		switch (ErrorCode)
		{
		default:
		case SCWErrorCode::GeneralCrash:
			{
				if (GDumpSCWJobInfoOnCrash != 0)
				{
					auto DumpSingleJob = [](FShaderCompileJob* SingleJob) -> FString
					{
						if (!SingleJob)
						{
							return TEXT("Internal error, not a Job!");
						}
						FString String = SingleJob->Input.GenerateShaderName();
						if (SingleJob->VFType)
						{
							String += FString::Printf(TEXT(" VF '%s'"), SingleJob->VFType->GetName());
						}
						String += FString::Printf(TEXT(" Type '%s'"), SingleJob->ShaderType->GetName());
						String += FString::Printf(TEXT(" '%s' Entry '%s' "), *SingleJob->Input.VirtualSourceFilePath, *SingleJob->Input.EntryPointName);
						return String;
					};
					UE_LOG(LogShaderCompilers, Error, TEXT("SCW %d Queued Jobs:"), QueuedJobs.Num());
					for (int32 Index = 0; Index < QueuedJobs.Num(); ++Index)
					{
						FShaderCommonCompileJob* CommonJob = QueuedJobs[Index];
						FShaderCompileJob* SingleJob = CommonJob->GetSingleShaderJob();
						GLog->Flush();
						if (SingleJob)
						{
							UE_LOG(LogShaderCompilers, Error, TEXT("Job %d [Single] %s"), Index, *DumpSingleJob(SingleJob));
						}
						else
						{
							FShaderPipelineCompileJob* PipelineJob = CommonJob->GetShaderPipelineJob();
							UE_LOG(LogShaderCompilers, Error, TEXT("Job %d: Pipeline %s "), Index, PipelineJob->ShaderPipeline->GetName());
							for (int32 Job = 0; Job < PipelineJob->StageJobs.Num(); ++Job)
							{
								UE_LOG(LogShaderCompilers, Error, TEXT("PipelineJob %d %s"), Job, *DumpSingleJob(PipelineJob->StageJobs[Job]->GetSingleShaderJob()));
							}
						}
					}
				}
				SCWErrorCode::HandleGeneralCrash(ExceptionInfo.GetData(), Callstack.GetData());
			}
			break;
		case SCWErrorCode::BadShaderFormatVersion:
			SCWErrorCode::HandleBadShaderFormatVersion(ExceptionInfo.GetData());
			break;
		case SCWErrorCode::BadInputVersion:
			SCWErrorCode::HandleBadInputVersion(ExceptionInfo.GetData());
			break;
		case SCWErrorCode::BadSingleJobHeader:
			SCWErrorCode::HandleBadSingleJobHeader(ExceptionInfo.GetData());
			break;
		case SCWErrorCode::BadPipelineJobHeader:
			SCWErrorCode::HandleBadPipelineJobHeader(ExceptionInfo.GetData());
			break;
		case SCWErrorCode::CantDeleteInputFile:
			SCWErrorCode::HandleCantDeleteInputFile(ExceptionInfo.GetData());
			break;
		case SCWErrorCode::CantSaveOutputFile:
			SCWErrorCode::HandleCantSaveOutputFile(ExceptionInfo.GetData());
			break;
		case SCWErrorCode::NoTargetShaderFormatsFound:
			SCWErrorCode::HandleNoTargetShaderFormatsFound(ExceptionInfo.GetData());
			break;
		case SCWErrorCode::CantCompileForSpecificFormat:
			SCWErrorCode::HandleCantCompileForSpecificFormat(ExceptionInfo.GetData());
			break;
		case SCWErrorCode::Success:
			// Can't get here...
			break;
		}
	}

	TArray<FShaderCompileJob*> QueuedSingleJobs;
	TArray<FShaderPipelineCompileJob*> QueuedPipelineJobs;
	SplitJobsByType(QueuedJobs, QueuedSingleJobs, QueuedPipelineJobs);

	// Read single jobs
	{
		int32 SingleJobHeader = -1;
		OutputFile << SingleJobHeader;
		if (SingleJobHeader != ShaderCompileWorkerSingleJobHeader)
		{
			FString Text = FString::Printf(TEXT("Expecting ShaderCompilerWorker Single Jobs %d, got %d instead! Forgot to build ShaderCompilerWorker?"), ShaderCompileWorkerSingleJobHeader, SingleJobHeader);
			ModalErrorOrLog(Text);
		}

		int32 NumJobs;
		OutputFile << NumJobs;
		checkf(NumJobs == QueuedSingleJobs.Num(), TEXT("Worker returned %u single jobs, %u expected"), NumJobs, QueuedSingleJobs.Num());
		for (int32 JobIndex = 0; JobIndex < NumJobs; JobIndex++)
		{
			auto* CurrentJob = QueuedSingleJobs[JobIndex];
			ReadSingleJob(CurrentJob, OutputFile);
		}
	}

	// Pipeline jobs
	{
		int32 PipelineJobHeader = -1;
		OutputFile << PipelineJobHeader;
		if (PipelineJobHeader != ShaderCompileWorkerPipelineJobHeader)
		{
			FString Text = FString::Printf(TEXT("Expecting ShaderCompilerWorker Pipeline Jobs %d, got %d instead! Forgot to build ShaderCompilerWorker?"), ShaderCompileWorkerPipelineJobHeader, PipelineJobHeader);
			ModalErrorOrLog(Text);
		}

		int32 NumJobs;
		OutputFile << NumJobs;
		checkf(NumJobs == QueuedPipelineJobs.Num(), TEXT("Worker returned %u pipeline jobs, %u expected"), NumJobs, QueuedPipelineJobs.Num());
		for (int32 JobIndex = 0; JobIndex < NumJobs; JobIndex++)
		{
			FShaderPipelineCompileJob* CurrentJob = QueuedPipelineJobs[JobIndex];

			FString PipelineName;
			OutputFile << PipelineName;
			checkf(PipelineName == CurrentJob->ShaderPipeline->GetName(), TEXT("Worker returned Pipeline %s, expected %s!"), *PipelineName, CurrentJob->ShaderPipeline->GetName());

			check(!CurrentJob->bFinalized);
			CurrentJob->bFinalized = true;
			CurrentJob->bFailedRemovingUnused = false;

			int32 NumStageJobs = -1;
			OutputFile << NumStageJobs;

			if (NumStageJobs != CurrentJob->StageJobs.Num())
			{
				checkf(NumJobs == QueuedPipelineJobs.Num(), TEXT("Worker returned %u stage pipeline jobs, %u expected"), NumStageJobs, CurrentJob->StageJobs.Num());
			}

			CurrentJob->bSucceeded = true;
			for (int32 Index = 0; Index < NumStageJobs; Index++)
			{
				auto* SingleJob = CurrentJob->StageJobs[Index]->GetSingleShaderJob();
				ReadSingleJob(SingleJob, OutputFile);
				CurrentJob->bFailedRemovingUnused = CurrentJob->bFailedRemovingUnused | SingleJob->Output.bFailedRemovingUnused;
			}
		}
	}
}

static void CheckSingleJob(FShaderCompileJob* SingleJob, TArray<FString>& Errors)
{
	if (SingleJob->bSucceeded)
	{
		check(SingleJob->Output.ShaderCode.GetShaderCodeSize() > 0);
	}

	if (GShowShaderWarnings || !SingleJob->bSucceeded)
	{
		for (int32 ErrorIndex = 0; ErrorIndex < SingleJob->Output.Errors.Num(); ErrorIndex++)
		{
			Errors.AddUnique(SingleJob->Output.Errors[ErrorIndex].GetErrorString());
		}
	}
};

static void AddErrorsForFailedJob(const FShaderCompileJob& CurrentJob, TArray<EShaderPlatform>& ErrorPlatforms, TArray<FString>& UniqueErrors, TArray<const FShaderCommonCompileJob*>& ErrorJobs)
{
	ErrorPlatforms.AddUnique((EShaderPlatform)CurrentJob.Input.Target.Platform);

	for (int32 ErrorIndex = 0; ErrorIndex < CurrentJob.Output.Errors.Num(); ErrorIndex++)
	{
		const FShaderCompilerError& CurrentError = CurrentJob.Output.Errors[ErrorIndex];

		// Include warnings if LogShaders is unsuppressed, otherwise only include errors
		if (UE_LOG_ACTIVE(LogShaders, Log) || CurrentError.StrippedErrorMessage.Contains(TEXT("error")))
		{
			UniqueErrors.AddUnique(CurrentJob.Output.Errors[ErrorIndex].GetErrorString());
			ErrorJobs.AddUnique(&CurrentJob);
		}
	}
};

/** Information tracked for each shader compile worker process instance. */
struct FShaderCompileWorkerInfo
{
	/** Process handle of the worker app once launched.  Invalid handle means no process. */
	FProcHandle WorkerProcess;

	/** Tracks whether tasks have been issued to the worker. */
	bool bIssuedTasksToWorker;	

	/** Whether the worker has been launched for this set of tasks. */
	bool bLaunchedWorker;

	/** Tracks whether all tasks issued to the worker have been received. */
	bool bComplete;

	/** Time at which the worker started the most recent batch of tasks. */
	double StartTime;

	/** Jobs that this worker is responsible for compiling. */
	TArray<FShaderCommonCompileJob*> QueuedJobs;

	FShaderCompileWorkerInfo() :
		bIssuedTasksToWorker(false),		
		bLaunchedWorker(false),
		bComplete(false),
		StartTime(0)
	{
	}

	// warning: not virtual
	~FShaderCompileWorkerInfo()
	{
		if(WorkerProcess.IsValid())
		{
			FPlatformProcess::TerminateProc(WorkerProcess);
			FPlatformProcess::CloseProc(WorkerProcess);
		}
	}
};

FShaderCompileThreadRunnableBase::FShaderCompileThreadRunnableBase(FShaderCompilingManager* InManager)
	: Manager(InManager)
	, Thread(nullptr)
	, bTerminatedByError(false)
	, bForceFinish(false)
{
}
void FShaderCompileThreadRunnableBase::StartThread()
{
	if (Manager->bAllowAsynchronousShaderCompiling && !FPlatformProperties::RequiresCookedData())
	{
		Thread = FRunnableThread::Create(this, TEXT("ShaderCompilingThread"), 0, TPri_Normal, FPlatformAffinity::GetPoolThreadMask());
	}
}

FShaderCompileThreadRunnable::FShaderCompileThreadRunnable(FShaderCompilingManager* InManager)
	: FShaderCompileThreadRunnableBase(InManager)
	, LastCheckForWorkersTime(0)
{
	for (uint32 WorkerIndex = 0; WorkerIndex < Manager->NumShaderCompilingThreads; WorkerIndex++)
	{
		WorkerInfos.Add(new FShaderCompileWorkerInfo());
	}
}

FShaderCompileThreadRunnable::~FShaderCompileThreadRunnable()
{
	for (int32 Index = 0; Index < WorkerInfos.Num(); Index++)
	{
		delete WorkerInfos[Index];
	}

	WorkerInfos.Empty(0);
}

/** Entry point for the shader compiling thread. */
uint32 FShaderCompileThreadRunnableBase::Run()
{
#ifdef _MSC_VER
	if(!FPlatformMisc::IsDebuggerPresent())
	{
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
		__try
#endif
		{
			check(Manager->bAllowAsynchronousShaderCompiling);
			// Do the work
			while (!bForceFinish)
			{
				CompilingLoop();
			}
		}
#if !PLATFORM_SEH_EXCEPTIONS_DISABLED
		__except( 
#if PLATFORM_WINDOWS
			ReportCrash( GetExceptionInformation() )
#else
			EXCEPTION_EXECUTE_HANDLER
#endif
			)
		{
#if WITH_EDITORONLY_DATA
			ErrorMessage = GErrorHist;
#endif
			// Use a memory barrier to ensure that the main thread sees the write to ErrorMessage before
			// the write to bTerminatedByError.
			FPlatformMisc::MemoryBarrier();

			bTerminatedByError = true;
		}
#endif
	}
	else
#endif
	{
		check(Manager->bAllowAsynchronousShaderCompiling);
		while (!bForceFinish)
		{
			CompilingLoop();
		}
	}

	return 0;
}

/** Called by the main thread only, reports exceptions in the worker threads */
void FShaderCompileThreadRunnableBase::CheckHealth() const
{
	if (bTerminatedByError)
	{
#if WITH_EDITORONLY_DATA
		GErrorHist[0] = 0;
#endif
		GIsCriticalError = false;
		UE_LOG(LogShaderCompilers, Fatal,TEXT("Shader Compiling thread exception:\r\n%s"), *ErrorMessage);
	}
}

int32 FShaderCompileThreadRunnable::PullTasksFromQueue()
{
	int32 NumActiveThreads = 0;
	{
		// Enter the critical section so we can access the input and output queues
		FScopeLock Lock(&Manager->CompileQueueSection);

		const int32 NumWorkersToFeed = Manager->bCompilingDuringGame ? Manager->NumShaderCompilingThreadsDuringGame : WorkerInfos.Num();

		for (int32 WorkerIndex = 0; WorkerIndex < WorkerInfos.Num(); WorkerIndex++)
		{
			FShaderCompileWorkerInfo& CurrentWorkerInfo = *WorkerInfos[WorkerIndex];

			// If this worker doesn't have any queued jobs, look for more in the input queue
			if (CurrentWorkerInfo.QueuedJobs.Num() == 0 && WorkerIndex < NumWorkersToFeed)
			{
				check(!CurrentWorkerInfo.bComplete);

				if (Manager->CompileQueue.Num() > 0)
				{
					bool bAddedLowLatencyTask = false;
					int32 JobIndex = 0;

					// Try to grab up to MaxShaderJobBatchSize jobs
					// Don't put more than one low latency task into a batch
					for (; JobIndex < Manager->MaxShaderJobBatchSize && JobIndex < Manager->CompileQueue.Num() && !bAddedLowLatencyTask; JobIndex++)
					{
						bAddedLowLatencyTask |= Manager->CompileQueue[JobIndex]->bOptimizeForLowLatency;
						CurrentWorkerInfo.QueuedJobs.Add(Manager->CompileQueue[JobIndex]);
					}

					// Update the worker state as having new tasks that need to be issued					
					// don't reset worker app ID, because the shadercompilerworkers don't shutdown immediately after finishing a single job queue.
					CurrentWorkerInfo.bIssuedTasksToWorker = false;					
					CurrentWorkerInfo.bLaunchedWorker = false;
					CurrentWorkerInfo.StartTime = FPlatformTime::Seconds();
					NumActiveThreads++;
					Manager->CompileQueue.RemoveAt(0, JobIndex);
				}
			}
			else
			{
				if (CurrentWorkerInfo.QueuedJobs.Num() > 0)
				{
					NumActiveThreads++;
				}

				// Add completed jobs to the output queue, which is ShaderMapJobs
				if (CurrentWorkerInfo.bComplete)
				{
					for (int32 JobIndex = 0; JobIndex < CurrentWorkerInfo.QueuedJobs.Num(); JobIndex++)
					{
						FShaderMapCompileResults& ShaderMapResults = Manager->ShaderMapJobs.FindChecked(CurrentWorkerInfo.QueuedJobs[JobIndex]->Id);
						ShaderMapResults.FinishedJobs.Add(CurrentWorkerInfo.QueuedJobs[JobIndex]);
						ShaderMapResults.bAllJobsSucceeded = ShaderMapResults.bAllJobsSucceeded && CurrentWorkerInfo.QueuedJobs[JobIndex]->bSucceeded;
					}

					const float ElapsedTime = FPlatformTime::Seconds() - CurrentWorkerInfo.StartTime;

					Manager->WorkersBusyTime += ElapsedTime;
					COOK_STAT(ShaderCompilerCookStats::AsyncCompileTimeSec += ElapsedTime);

					// Log if requested or if there was an exceptionally slow batch, to see the offender easily
					if (Manager->bLogJobCompletionTimes || ElapsedTime > 30.0f)
					{
						FString JobNames;

						for (int32 JobIndex = 0; JobIndex < CurrentWorkerInfo.QueuedJobs.Num(); JobIndex++)
						{
							const FShaderCommonCompileJob& Job = *CurrentWorkerInfo.QueuedJobs[JobIndex];
							if (auto* SingleJob = Job.GetSingleShaderJob())
							{
								JobNames += FString(SingleJob->ShaderType->GetName()) + TEXT(" Instructions = ") + FString::FromInt(SingleJob->Output.NumInstructions);
							}
							else
							{
								auto* PipelineJob = Job.GetShaderPipelineJob();
								JobNames += FString(PipelineJob->ShaderPipeline->GetName());
								if (PipelineJob->bFailedRemovingUnused)
								{
									 JobNames += FString(TEXT("(failed to optimize)"));
								}
							}
							if (JobIndex < CurrentWorkerInfo.QueuedJobs.Num() - 1)
							{
								JobNames += TEXT(", ");
							}
						}

						UE_LOG(LogShaders, Display, TEXT("Finished batch of %u jobs in %.3fs, %s"), CurrentWorkerInfo.QueuedJobs.Num(), ElapsedTime, *JobNames);
					}

					// Using atomics to update NumOutstandingJobs since it is read outside of the critical section
					FPlatformAtomics::InterlockedAdd(&Manager->NumOutstandingJobs, -CurrentWorkerInfo.QueuedJobs.Num());

					CurrentWorkerInfo.bComplete = false;
					CurrentWorkerInfo.QueuedJobs.Empty();
				}
			}
		}
	}
	return NumActiveThreads;
}

void FShaderCompileThreadRunnable::WriteNewTasks()
{
	for (int32 WorkerIndex = 0; WorkerIndex < WorkerInfos.Num(); WorkerIndex++)
	{
		FShaderCompileWorkerInfo& CurrentWorkerInfo = *WorkerInfos[WorkerIndex];

		// Only write tasks once
		if (!CurrentWorkerInfo.bIssuedTasksToWorker && CurrentWorkerInfo.QueuedJobs.Num() > 0)
		{
			CurrentWorkerInfo.bIssuedTasksToWorker = true;

			const FString WorkingDirectory = Manager->AbsoluteShaderBaseWorkingDirectory + FString::FromInt(WorkerIndex);

			// To make sure that the process waiting for input file won't try to read it until it's ready
			// we use a temp file name during writing.
			FString TransferFileName;
			do
			{
				FGuid Guid;
				FPlatformMisc::CreateGuid(Guid);
				TransferFileName = WorkingDirectory + Guid.ToString();
			} while (IFileManager::Get().FileSize(*TransferFileName) != INDEX_NONE);

			// Write out the file that the worker app is waiting for, which has all the information needed to compile the shader.
			// 'Only' indicates that the worker should keep checking for more tasks after this one
			FArchive* TransferFile = NULL;

			int32 RetryCount = 0;
			// Retry over the next two seconds if we can't write out the input file
			// Anti-virus and indexing applications can interfere and cause this write to fail
			//@todo - switch to shared memory or some other method without these unpredictable hazards
			while (TransferFile == NULL && RetryCount < 2000)
			{
				if (RetryCount > 0)
				{
					FPlatformProcess::Sleep(0.01f);
				}
				TransferFile = IFileManager::Get().CreateFileWriter(*TransferFileName, FILEWRITE_EvenIfReadOnly);
				RetryCount++;
				if (TransferFile == NULL)
				{
					UE_LOG(LogShaderCompilers, Warning, TEXT("Could not create the shader compiler transfer file '%s', retrying..."), *TransferFileName);
				}
			}
			if (TransferFile == NULL)
			{
				UE_LOG(LogShaderCompilers, Fatal, TEXT("Could not create the shader compiler transfer file '%s'."), *TransferFileName);
			}
			check(TransferFile);

			if (!FShaderCompileUtilities::DoWriteTasks(CurrentWorkerInfo.QueuedJobs, *TransferFile))
			{
				uint64 TotalDiskSpace = 0;
				uint64 FreeDiskSpace = 0;
				FPlatformMisc::GetDiskTotalAndFreeSpace(TransferFileName, TotalDiskSpace, FreeDiskSpace);
				UE_LOG(LogShaderCompilers, Fatal, TEXT("Could not write the shader compiler transfer filename to '%s' (Free Disk Space: %llu."), *TransferFileName, FreeDiskSpace);
			}
			delete TransferFile;

			// Change the transfer file name to proper one
			FString ProperTransferFileName = WorkingDirectory / TEXT("WorkerInputOnly.in");
			if (!IFileManager::Get().Move(*ProperTransferFileName, *TransferFileName))
			{
				uint64 TotalDiskSpace = 0;
				uint64 FreeDiskSpace = 0;
				FPlatformMisc::GetDiskTotalAndFreeSpace(TransferFileName, TotalDiskSpace, FreeDiskSpace);
				UE_LOG(LogShaderCompilers, Fatal, TEXT("Could not rename the shader compiler transfer filename to '%s' from '%s' (Free Disk Space: %llu)."), *ProperTransferFileName, *TransferFileName, FreeDiskSpace);
			}
		}
	}
}

bool FShaderCompileThreadRunnable::LaunchWorkersIfNeeded()
{
	const double CurrentTime = FPlatformTime::Seconds();
	// Limit how often we check for workers running since IsApplicationRunning eats up some CPU time on Windows
	const bool bCheckForWorkerRunning = (CurrentTime - LastCheckForWorkersTime > .1f);
	bool bAbandonWorkers = false;

	if (bCheckForWorkerRunning)
	{
		LastCheckForWorkersTime = CurrentTime;
	}

	for (int32 WorkerIndex = 0; WorkerIndex < WorkerInfos.Num(); WorkerIndex++)
	{
		FShaderCompileWorkerInfo& CurrentWorkerInfo = *WorkerInfos[WorkerIndex];
		if (CurrentWorkerInfo.QueuedJobs.Num() == 0 )
		{
			// Skip if nothing to do
			// Also, use the opportunity to free OS resources by cleaning up handles of no more running processes
			if (CurrentWorkerInfo.WorkerProcess.IsValid() && !FShaderCompilingManager::IsShaderCompilerWorkerRunning(CurrentWorkerInfo.WorkerProcess))
			{
				FPlatformProcess::CloseProc(CurrentWorkerInfo.WorkerProcess);
				CurrentWorkerInfo.WorkerProcess = FProcHandle();
			}
			continue;
		}

		if (!CurrentWorkerInfo.WorkerProcess.IsValid() || (bCheckForWorkerRunning && !FShaderCompilingManager::IsShaderCompilerWorkerRunning(CurrentWorkerInfo.WorkerProcess)))
		{
			// @TODO: dubious design - worker should not be launched unless we know there's more work to do.
			bool bLaunchAgain = true;

			// Detect when the worker has exited due to fatal error
			// bLaunchedWorker check here is necessary to distinguish between 'process isn't running because it crashed' and 'process isn't running because it exited cleanly and the outputfile was already consumed'
			if (CurrentWorkerInfo.WorkerProcess.IsValid())
			{
				// shader compiler exited one way or another, so clear out the stale PID.
				FPlatformProcess::CloseProc(CurrentWorkerInfo.WorkerProcess);
				CurrentWorkerInfo.WorkerProcess = FProcHandle();

				if (CurrentWorkerInfo.bLaunchedWorker)
				{
					const FString WorkingDirectory = Manager->AbsoluteShaderBaseWorkingDirectory + FString::FromInt(WorkerIndex) + TEXT("/");
					const FString OutputFileNameAndPath = WorkingDirectory + TEXT("WorkerOutputOnly.out");

					if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*OutputFileNameAndPath))
					{
						// If the worker is no longer running but it successfully wrote out the output, no need to assert
						bLaunchAgain = false;
					}
					else
					{
						UE_LOG(LogShaderCompilers, Warning, TEXT("ShaderCompileWorker terminated unexpectedly!  Falling back to directly compiling which will be very slow.  Thread %u."), WorkerIndex);

						bAbandonWorkers = true;
						break;
					}
				}
			}

			if (bLaunchAgain)
			{
				const FString WorkingDirectory = Manager->ShaderBaseWorkingDirectory + FString::FromInt(WorkerIndex) + TEXT("/");
				FString InputFileName(TEXT("WorkerInputOnly.in"));
				FString OutputFileName(TEXT("WorkerOutputOnly.out"));

				// Store the handle with this thread so that we will know not to launch it again
				CurrentWorkerInfo.WorkerProcess = Manager->LaunchWorker(WorkingDirectory, Manager->ProcessId, WorkerIndex, InputFileName, OutputFileName);
				CurrentWorkerInfo.bLaunchedWorker = true;
			}
		}
	}

	return bAbandonWorkers;
}

void FShaderCompileThreadRunnable::ReadAvailableResults()
{
	for (int32 WorkerIndex = 0; WorkerIndex < WorkerInfos.Num(); WorkerIndex++)
	{
		FShaderCompileWorkerInfo& CurrentWorkerInfo = *WorkerInfos[WorkerIndex];

		// Check for available result files
		if (CurrentWorkerInfo.QueuedJobs.Num() > 0)
		{
			// Distributed compiles always use the same directory
			const FString WorkingDirectory = Manager->AbsoluteShaderBaseWorkingDirectory + FString::FromInt(WorkerIndex) + TEXT("/");
			// 'Only' indicates to the worker that it should log and continue checking for the input file after the first one is processed
			const TCHAR* InputFileName = TEXT("WorkerInputOnly.in");
			const FString OutputFileNameAndPath = WorkingDirectory + TEXT("WorkerOutputOnly.out");

			// In the common case the output file will not exist, so check for existence before opening
			// This is only a win if FileExists is faster than CreateFileReader, which it is on Windows
			if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*OutputFileNameAndPath))
			{
				FArchive* OutputFilePtr = IFileManager::Get().CreateFileReader(*OutputFileNameAndPath, FILEREAD_Silent);

				if (OutputFilePtr)
				{
					FArchive& OutputFile = *OutputFilePtr;
					check(!CurrentWorkerInfo.bComplete);
					FShaderCompileUtilities::DoReadTaskResults(CurrentWorkerInfo.QueuedJobs, OutputFile);

					// Close the output file.
					delete OutputFilePtr;

					// Delete the output file now that we have consumed it, to avoid reading stale data on the next compile loop.
					bool bDeletedOutput = IFileManager::Get().Delete(*OutputFileNameAndPath, true, true);
					int32 RetryCount = 0;
					// Retry over the next two seconds if we couldn't delete it
					while (!bDeletedOutput && RetryCount < 200)
					{
						FPlatformProcess::Sleep(0.01f);
						bDeletedOutput = IFileManager::Get().Delete(*OutputFileNameAndPath, true, true);
						RetryCount++;
					}
					checkf(bDeletedOutput, TEXT("Failed to delete %s!"), *OutputFileNameAndPath);

					CurrentWorkerInfo.bComplete = true;
				}
			}
		}
	}
}

void FShaderCompileThreadRunnable::CompileDirectlyThroughDll()
{
	// If we aren't compiling through workers, so we can just track the serial time here.
	COOK_STAT(FScopedDurationTimer CompileTimer (ShaderCompilerCookStats::AsyncCompileTimeSec));

	for (int32 WorkerIndex = 0; WorkerIndex < WorkerInfos.Num(); WorkerIndex++)
	{
		FShaderCompileWorkerInfo& CurrentWorkerInfo = *WorkerInfos[WorkerIndex];

		if (CurrentWorkerInfo.QueuedJobs.Num() > 0)
		{
			for (int32 JobIndex = 0; JobIndex < CurrentWorkerInfo.QueuedJobs.Num(); JobIndex++)
			{
				FShaderCommonCompileJob& CurrentJob = *CurrentWorkerInfo.QueuedJobs[JobIndex];

				check(!CurrentJob.bFinalized);
				CurrentJob.bFinalized = true;

				static ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
				auto* SingleJob = CurrentJob.GetSingleShaderJob();
				if (SingleJob)
				{
					const FName Format = LegacyShaderPlatformToShaderFormat(EShaderPlatform(SingleJob->Input.Target.Platform));
					const IShaderFormat* Compiler = TPM.FindShaderFormat(Format);

					if (!Compiler)
					{
						UE_LOG(LogShaderCompilers, Fatal, TEXT("Can't compile shaders for format %s, couldn't load compiler dll"), *Format.ToString());
					}
					CA_ASSUME(Compiler != NULL);

					if (IsValidRef(SingleJob->Input.SharedEnvironment))
					{
						// Merge the shared environment into the per-shader environment before calling into the compile function
						// Normally this happens in the worker
						SingleJob->Input.Environment.Merge(*SingleJob->Input.SharedEnvironment);
					}

					// Compile the shader directly through the platform dll (directly from the shader dir as the working directory)
					Compiler->CompileShader(Format, SingleJob->Input, SingleJob->Output, FString(FPlatformProcess::ShaderDir()));

					SingleJob->bSucceeded = SingleJob->Output.bSucceeded;

					if (SingleJob->Output.bSucceeded)
					{
						// Generate a hash of the output and cache it
						// The shader processing this output will use it to search for existing FShaderResources
						SingleJob->Output.GenerateOutputHash();
					}
				}
				else
				{
					auto* PipelineJob = CurrentJob.GetShaderPipelineJob();
					check(PipelineJob);

					EShaderPlatform Platform = (EShaderPlatform)PipelineJob->StageJobs[0]->GetSingleShaderJob()->Input.Target.Platform;
					const FName Format = LegacyShaderPlatformToShaderFormat(Platform);
					const IShaderFormat* Compiler = TPM.FindShaderFormat(Format);

					if (!Compiler)
					{
						UE_LOG(LogShaderCompilers, Fatal, TEXT("Can't compile shaders for format %s, couldn't load compiler dll"), *Format.ToString());
					}
					CA_ASSUME(Compiler != NULL);

					// Verify same platform on all stages
					for (int32 Index = 1; Index < PipelineJob->StageJobs.Num(); ++Index)
					{
						auto* SingleStage = PipelineJob->StageJobs[Index]->GetSingleShaderJob();
						if (!SingleStage)
						{
							UE_LOG(LogShaderCompilers, Fatal, TEXT("Can't nest Shader Pipelines inside Shader Pipeline '%s'!"), PipelineJob->ShaderPipeline->GetName());
						}
						if (Platform != SingleStage->Input.Target.Platform)
						{
							UE_LOG(LogShaderCompilers, Fatal, TEXT("Mismatched Target Platform %s while compiling Shader Pipeline '%s'."), *Format.GetPlainNameString(), PipelineJob->ShaderPipeline->GetName());
						}
					}

					CompileShaderPipeline(Compiler, Format, PipelineJob, FString(FPlatformProcess::ShaderDir()));
				}
			}

			CurrentWorkerInfo.bComplete = true;
		}
	}
}

int32 FShaderCompileThreadRunnable::CompilingLoop()
{
	// Grab more shader compile jobs from the input queue, and move completed jobs to Manager->ShaderMapJobs
	const int32 NumActiveThreads = PullTasksFromQueue();

	if (NumActiveThreads == 0 && Manager->bAllowAsynchronousShaderCompiling)
	{
		// Yield while there's nothing to do
		// Note: sleep-looping is bad threading practice, wait on an event instead!
		// The shader worker thread does it because it needs to communicate with other processes through the file system
		FPlatformProcess::Sleep(.010f);
	}

	if (Manager->bAllowCompilingThroughWorkers)
	{
		// Write out the files which are input to the shader compile workers
		WriteNewTasks();

		// Launch shader compile workers if they are not already running
		// Workers can time out when idle so they may need to be relaunched
		bool bAbandonWorkers = LaunchWorkersIfNeeded();

		if (bAbandonWorkers)
		{
			// Fall back to local compiles if the SCW crashed.
			// This is nasty but needed to work around issues where message passing through files to SCW is unreliable on random PCs
			Manager->bAllowCompilingThroughWorkers = false;
		}
		else
		{
			// Read files which are outputs from the shader compile workers
			ReadAvailableResults();
		}
	}
	else
	{
		CompileDirectlyThroughDll();
	}

	return NumActiveThreads;
}

FShaderCompilingManager* GShaderCompilingManager = NULL;

FShaderCompilingManager::FShaderCompilingManager() :
	bCompilingDuringGame(false),
	NumOutstandingJobs(0),
#if PLATFORM_MAC
	ShaderCompileWorkerName(TEXT("../../../Engine/Binaries/Mac/ShaderCompileWorker")),
#elif PLATFORM_LINUX
	ShaderCompileWorkerName(TEXT("../../../Engine/Binaries/Linux/ShaderCompileWorker")),
#else
	ShaderCompileWorkerName(TEXT("../../../Engine/Binaries/Win64/ShaderCompileWorker.exe")),
#endif
	SuppressedShaderPlatforms(0)
{
	WorkersBusyTime = 0;
	bFallBackToDirectCompiles = false;

	// Threads must use absolute paths on Windows in case the current directory is changed on another thread!
	ShaderCompileWorkerName = FPaths::ConvertRelativePathToFull(ShaderCompileWorkerName);

	// Read values from the engine ini
	verify(GConfig->GetBool( TEXT("DevOptions.Shaders"), TEXT("bAllowCompilingThroughWorkers"), bAllowCompilingThroughWorkers, GEngineIni ));
	verify(GConfig->GetBool( TEXT("DevOptions.Shaders"), TEXT("bAllowAsynchronousShaderCompiling"), bAllowAsynchronousShaderCompiling, GEngineIni ));

	// override the use of workers, can be helpful for debugging shader compiler code
	if (!FPlatformProcess::SupportsMultithreading() || FParse::Param(FCommandLine::Get(),TEXT("noshaderworker")))
	{
		bAllowCompilingThroughWorkers = false;
	}

	if (!FPlatformProcess::SupportsMultithreading())
	{
		bAllowAsynchronousShaderCompiling = false;
	}

	int32 NumUnusedShaderCompilingThreads;
	verify(GConfig->GetInt( TEXT("DevOptions.Shaders"), TEXT("NumUnusedShaderCompilingThreads"), NumUnusedShaderCompilingThreads, GEngineIni ));

	int32 NumUnusedShaderCompilingThreadsDuringGame;
	verify(GConfig->GetInt( TEXT("DevOptions.Shaders"), TEXT("NumUnusedShaderCompilingThreadsDuringGame"), NumUnusedShaderCompilingThreadsDuringGame, GEngineIni ));

	// Use all the cores on the build machines
	if (GIsBuildMachine || FParse::Param(FCommandLine::Get(), TEXT("USEALLAVAILABLECORES")))
	{
		NumUnusedShaderCompilingThreads = 0;
	}

	verify(GConfig->GetInt( TEXT("DevOptions.Shaders"), TEXT("MaxShaderJobBatchSize"), MaxShaderJobBatchSize, GEngineIni ));
	verify(GConfig->GetBool( TEXT("DevOptions.Shaders"), TEXT("bPromptToRetryFailedShaderCompiles"), bPromptToRetryFailedShaderCompiles, GEngineIni ));
	verify(GConfig->GetBool( TEXT("DevOptions.Shaders"), TEXT("bLogJobCompletionTimes"), bLogJobCompletionTimes, GEngineIni ));

	GRetryShaderCompilation = bPromptToRetryFailedShaderCompiles;

	verify(GConfig->GetFloat( TEXT("DevOptions.Shaders"), TEXT("ProcessGameThreadTargetTime"), ProcessGameThreadTargetTime, GEngineIni ));

#if UE_BUILD_DEBUG
	// Increase budget for processing results in debug or else it takes forever to finish due to poor framerate
	ProcessGameThreadTargetTime *= 3;
#endif

	// Get the current process Id, this will be used by the worker app to shut down when it's parent is no longer running.
	ProcessId = FPlatformProcess::GetCurrentProcessId();

	// Use a working directory unique to this game, process and thread so that it will not conflict 
	// With processes from other games, processes from the same game or threads in this same process.
	// Use IFileManager to do path conversion to properly handle sandbox paths (outside of standard paths in particular).
	//ShaderBaseWorkingDirectory = FPlatformProcess::ShaderWorkingDir() / FString::FromInt(ProcessId) + TEXT("/");

	{
		FGuid Guid;
		Guid = FGuid::NewGuid();
		FString LegacyShaderWorkingDirectory = FPaths::ProjectIntermediateDir() / TEXT("Shaders/WorkingDirectory/")  / FString::FromInt(ProcessId) + TEXT("/");
		ShaderBaseWorkingDirectory = FPlatformProcess::ShaderWorkingDir() / *Guid.ToString(EGuidFormats::Digits) + TEXT("/");
		UE_LOG(LogShaderCompilers, Log, TEXT("Guid format shader working directory is %d characters bigger than the processId version (%s)."), ShaderBaseWorkingDirectory.Len() - LegacyShaderWorkingDirectory.Len(), *LegacyShaderWorkingDirectory );
	}

	if (!IFileManager::Get().DeleteDirectory(*ShaderBaseWorkingDirectory, false, true))
	{
		UE_LOG(LogShaderCompilers, Fatal, TEXT("Could not delete the shader compiler working directory '%s'."), *ShaderBaseWorkingDirectory);
	}
	else
	{
		UE_LOG(LogShaderCompilers, Log, TEXT("Cleaned the shader compiler working directory '%s'."), *ShaderBaseWorkingDirectory);
	}
	FString AbsoluteBaseDirectory = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*ShaderBaseWorkingDirectory);
	FPaths::NormalizeDirectoryName(AbsoluteBaseDirectory);
	AbsoluteShaderBaseWorkingDirectory = AbsoluteBaseDirectory + TEXT("/");

	FString AbsoluteDebugInfoDirectory = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*(FPaths::ProjectSavedDir() / TEXT("ShaderDebugInfo")));
	FPaths::NormalizeDirectoryName(AbsoluteDebugInfoDirectory);
	AbsoluteShaderDebugInfoDirectory = AbsoluteDebugInfoDirectory;

	const int32 NumVirtualCores = FPlatformMisc::NumberOfCoresIncludingHyperthreads();

	NumShaderCompilingThreads = bAllowCompilingThroughWorkers ? (NumVirtualCores - NumUnusedShaderCompilingThreads) : 1;

	// Make sure there's at least one worker allowed to be active when compiling during the game
	NumShaderCompilingThreadsDuringGame = bAllowCompilingThroughWorkers ? (NumVirtualCores - NumUnusedShaderCompilingThreadsDuringGame) : 1;

	// On machines with few cores, each core will have a massive impact on compile time, so we prioritize compile latency over editor performance during the build
	if (NumVirtualCores <= 4)
	{
		NumShaderCompilingThreads = NumVirtualCores - 1;
		NumShaderCompilingThreadsDuringGame = NumVirtualCores - 1;
	}

	NumShaderCompilingThreads = FMath::Max<int32>(1, NumShaderCompilingThreads);
	NumShaderCompilingThreadsDuringGame = FMath::Max<int32>(1, NumShaderCompilingThreadsDuringGame);

	NumShaderCompilingThreadsDuringGame = FMath::Min<int32>(NumShaderCompilingThreadsDuringGame, NumShaderCompilingThreads);

#if PLATFORM_WINDOWS
	if (FShaderCompileXGEThreadRunnable_InterceptionInterface::IsSupported())
	{
		UE_LOG(LogShaderCompilers, Display, TEXT("Using XGE Shader Compiler (Interception Interface)."));
		Thread = MakeUnique<FShaderCompileXGEThreadRunnable_InterceptionInterface>(this);
	}
	else if (FShaderCompileXGEThreadRunnable_XmlInterface::IsSupported())
	{
		UE_LOG(LogShaderCompilers, Display, TEXT("Using XGE Shader Compiler (XML Interface)."));
		Thread = MakeUnique<FShaderCompileXGEThreadRunnable_XmlInterface>(this);
	}
	else
#endif // PLATFORM_WINDOWS
	{
		UE_LOG(LogShaderCompilers, Display, TEXT("Using Local Shader Compiler."));
		Thread = MakeUnique<FShaderCompileThreadRunnable>(this);
	}
	Thread->StartThread();
}

void FShaderCompilingManager::AddJobs(TArray<FShaderCommonCompileJob*>& NewJobs, bool bApplyCompletedShaderMapForRendering, bool bOptimizeForLowLatency, bool bRecreateComponentRenderStateOnCompletion)
{
	check(!FPlatformProperties::RequiresCookedData());

	// Lock CompileQueueSection so we can access the input and output queues
	FScopeLock Lock(&CompileQueueSection);

	if (bOptimizeForLowLatency)
	{
		int32 InsertIndex = 0;

		for (; InsertIndex < CompileQueue.Num(); InsertIndex++)
		{
			if (!CompileQueue[InsertIndex]->bOptimizeForLowLatency)
			{
				break;
			}
		}

		// Insert after the last low latency task, but before all the normal tasks
		// This is necessary to make sure that jobs from the same material get processed in order
		// Note: this is assuming that the value of bOptimizeForLowLatency never changes for a certain material
		CompileQueue.InsertZeroed(InsertIndex, NewJobs.Num());

		for (int32 JobIndex = 0; JobIndex < NewJobs.Num(); JobIndex++)
		{
			CompileQueue[InsertIndex + JobIndex] = NewJobs[JobIndex];
		}
	}
	else
	{
		CompileQueue.Append(NewJobs);
	}

	// Using atomics to update NumOutstandingJobs since it is read outside of the critical section
	FPlatformAtomics::InterlockedAdd(&NumOutstandingJobs, NewJobs.Num());

	for (int32 JobIndex = 0; JobIndex < NewJobs.Num(); JobIndex++)
	{
		NewJobs[JobIndex]->bOptimizeForLowLatency = bOptimizeForLowLatency;
		FShaderMapCompileResults& ShaderMapInfo = ShaderMapJobs.FindOrAdd(NewJobs[JobIndex]->Id);
		ShaderMapInfo.bApplyCompletedShaderMapForRendering = bApplyCompletedShaderMapForRendering;
		ShaderMapInfo.bRecreateComponentRenderStateOnCompletion = bRecreateComponentRenderStateOnCompletion;
		auto* PipelineJob = NewJobs[JobIndex]->GetShaderPipelineJob();
		if (PipelineJob)
		{
			ShaderMapInfo.NumJobsQueued += PipelineJob->StageJobs.Num();
		}
		else
		{
			ShaderMapInfo.NumJobsQueued++;
		}
	}
}

/** Launches the worker, returns the launched process handle. */
FProcHandle FShaderCompilingManager::LaunchWorker(const FString& WorkingDirectory, uint32 InProcessId, uint32 ThreadId, const FString& WorkerInputFile, const FString& WorkerOutputFile)
{
	// Setup the parameters that the worker application needs
	// Surround the working directory with double quotes because it may contain a space 
	// WorkingDirectory ends with a '\', so we have to insert another to meet the Windows commandline parsing rules 
	// http://msdn.microsoft.com/en-us/library/17w5ykft.aspx 
	// Use IFileManager to do path conversion to properly handle sandbox paths (outside of standard paths in particular).
	FString WorkerAbsoluteDirectory = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*WorkingDirectory);
	FPaths::NormalizeDirectoryName(WorkerAbsoluteDirectory);
	FString WorkerParameters = FString(TEXT("\"")) + WorkerAbsoluteDirectory + TEXT("/\" ") + FString::FromInt(InProcessId) + TEXT(" ") + FString::FromInt(ThreadId) + TEXT(" ") + WorkerInputFile + TEXT(" ") + WorkerOutputFile;
	WorkerParameters += FString(TEXT(" -communicatethroughfile "));
	if ( GIsBuildMachine )
	{
		WorkerParameters += FString(TEXT(" -buildmachine "));
	}
	if (PLATFORM_LINUX) //-V560
	{
		// suppress log generation as much as possible
		WorkerParameters += FString(TEXT(" -logcmds=\"Global None\" "));

		if (UE_BUILD_DEBUG)
		{
			// when running a debug build under Linux, make SCW crash with core for easier debugging
			WorkerParameters += FString(TEXT(" -core "));
		}
	}
	WorkerParameters += FCommandLine::GetSubprocessCommandline();

	// Launch the worker process
	int32 PriorityModifier = -1; // below normal

	if (DEBUG_SHADERCOMPILEWORKER)
	{
		// Note: Set breakpoint here and launch the ShaderCompileWorker with WorkerParameters a cmd-line
		const TCHAR* WorkerParametersText = *WorkerParameters;
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Launching shader compile worker w/ WorkerParameters\n\t%s\n"), WorkerParametersText);
		FProcHandle DummyHandle;
		return DummyHandle;
	}
	else
	{
#if UE_BUILD_DEBUG && PLATFORM_LINUX
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Launching shader compile worker:\n\t%s\n"), *WorkerParameters);
#endif
		// Disambiguate between SCW.exe missing vs other errors.
		static bool bFirstLaunch = true;
		uint32 WorkerId = 0;
		FProcHandle WorkerHandle = FPlatformProcess::CreateProc(*ShaderCompileWorkerName, *WorkerParameters, true, false, false, &WorkerId, PriorityModifier, NULL, NULL);
		if (WorkerHandle.IsValid())
		{
			// Process launched at least once successfully
			bFirstLaunch = false;
		}
		else
		{
			// If this doesn't error, the app will hang waiting for jobs that can never be completed
			if (bFirstLaunch)
			{
				// When using source builds users are likely to make a mistake of not building SCW (e.g. in particular on Linux, even though default makefile target builds it).
				// Make the engine exit gracefully with a helpful message instead of a crash.
				static bool bShowedMessageBox = false;
				if (!bShowedMessageBox && !IsRunningCommandlet() && !FApp::IsUnattended())
				{
					bShowedMessageBox = true;
					FText ErrorMessage = FText::Format(LOCTEXT("LaunchingShaderCompileWorkerFailed", "Unable to launch {0} - make sure you built ShaderCompileWorker."), FText::FromString(ShaderCompileWorkerName));
					FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, *ErrorMessage.ToString(),
												 *LOCTEXT("LaunchingShaderCompileWorkerFailedTitle", "Unable to launch ShaderCompileWorker.").ToString());
				}
				UE_LOG(LogShaderCompilers, Error, TEXT("Couldn't launch %s! Make sure you build ShaderCompileWorker."), *ShaderCompileWorkerName);
				// duplicate to printf() since threaded logs may not be always flushed
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Couldn't launch %s! Make sure you build ShaderCompileWorker.\n"), *ShaderCompileWorkerName);
				FPlatformMisc::RequestExitWithStatus(true, 1);
			}
			else
			{
				UE_LOG(LogShaderCompilers, Fatal, TEXT("Couldn't launch %s!"), *ShaderCompileWorkerName);
			}
		}

		return WorkerHandle;
	}
}

/** Flushes all pending jobs for the given shader maps. */
void FShaderCompilingManager::BlockOnShaderMapCompletion(const TArray<int32>& ShaderMapIdsToFinishCompiling, TMap<int32, FShaderMapFinalizeResults>& CompiledShaderMaps)
{
	COOK_STAT(FScopedDurationTimer BlockingTimer(ShaderCompilerCookStats::BlockingTimeSec));
	if (bAllowAsynchronousShaderCompiling)
	{
		int32 NumPendingJobs = 0;

		do 
		{
			Thread->CheckHealth();
			NumPendingJobs = 0;
			{
				// Lock CompileQueueSection so we can access the input and output queues
				FScopeLock Lock(&CompileQueueSection);

				for (int32 ShaderMapIndex = 0; ShaderMapIndex < ShaderMapIdsToFinishCompiling.Num(); ShaderMapIndex++)
				{
					const FShaderMapCompileResults* ResultsPtr = ShaderMapJobs.Find(ShaderMapIdsToFinishCompiling[ShaderMapIndex]);
					if (ResultsPtr)
					{
						const FShaderMapCompileResults& Results = *ResultsPtr;

						int32 FinishedJobs = GetNumTotalJobs(Results.FinishedJobs);
						if (FinishedJobs == Results.NumJobsQueued)
						{
							CompiledShaderMaps.Add(ShaderMapIdsToFinishCompiling[ShaderMapIndex], FShaderMapFinalizeResults(Results));
							ShaderMapJobs.Remove(ShaderMapIdsToFinishCompiling[ShaderMapIndex]);
						}
						else
						{
							NumPendingJobs += Results.NumJobsQueued;
						}
					}
				}
			}

			if (NumPendingJobs > 0)
			{
				// Yield CPU time while waiting
				FPlatformProcess::Sleep(.01f);
			}
		} 
		while (NumPendingJobs > 0);
	}
	else
	{
		int32 NumActiveWorkers = 0;
		do 
		{
			NumActiveWorkers = Thread->CompilingLoop();
		} 
		while (NumActiveWorkers > 0);

		check(CompileQueue.Num() == 0);

		for (int32 ShaderMapIndex = 0; ShaderMapIndex < ShaderMapIdsToFinishCompiling.Num(); ShaderMapIndex++)
		{
			const FShaderMapCompileResults* ResultsPtr = ShaderMapJobs.Find(ShaderMapIdsToFinishCompiling[ShaderMapIndex]);

			if (ResultsPtr)
			{
				const FShaderMapCompileResults& Results = *ResultsPtr;
				check(GetNumTotalJobs(Results.FinishedJobs) == Results.NumJobsQueued);

				CompiledShaderMaps.Add(ShaderMapIdsToFinishCompiling[ShaderMapIndex], FShaderMapFinalizeResults(Results));
				ShaderMapJobs.Remove(ShaderMapIdsToFinishCompiling[ShaderMapIndex]);
			}
		}
	}
}

void FShaderCompilingManager::BlockOnAllShaderMapCompletion(TMap<int32, FShaderMapFinalizeResults>& CompiledShaderMaps)
{
	COOK_STAT(FScopedDurationTimer BlockingTimer(ShaderCompilerCookStats::BlockingTimeSec));
	if (bAllowAsynchronousShaderCompiling)
	{
		int32 NumPendingJobs = 0;

		do 
		{
			Thread->CheckHealth();
			NumPendingJobs = 0;
			{
				// Lock CompileQueueSection so we can access the input and output queues
				FScopeLock Lock(&CompileQueueSection);

				for (TMap<int32, FShaderMapCompileResults>::TIterator It(ShaderMapJobs); It; ++It)
				{
					const FShaderMapCompileResults& Results = It.Value();

					if (GetNumTotalJobs(Results.FinishedJobs) == Results.NumJobsQueued)
					{
						CompiledShaderMaps.Add(It.Key(), FShaderMapFinalizeResults(Results));
						It.RemoveCurrent();
					}
					else
					{
						NumPendingJobs += Results.NumJobsQueued;
					}
				}
			}

			if (NumPendingJobs > 0)
			{
				// Yield CPU time while waiting
				FPlatformProcess::Sleep(.01f);
			}
		} 
		while (NumPendingJobs > 0);
	}
	else
	{
		int32 NumActiveWorkers = 0;
		do 
		{
			NumActiveWorkers = Thread->CompilingLoop();
		} 
		while (NumActiveWorkers > 0);

		check(CompileQueue.Num() == 0);

		for (TMap<int32, FShaderMapCompileResults>::TIterator It(ShaderMapJobs); It; ++It)
		{
			const FShaderMapCompileResults& Results = It.Value();

			check(GetNumTotalJobs(Results.FinishedJobs) == Results.NumJobsQueued);
			CompiledShaderMaps.Add(It.Key(), FShaderMapFinalizeResults(Results));
			It.RemoveCurrent();
		}
	}
}

void FShaderCompilingManager::ProcessCompiledShaderMaps(
	TMap<int32, FShaderMapFinalizeResults>& CompiledShaderMaps, 
	float TimeBudget)
{
	// Keeps shader maps alive as they are passed from the shader compiler and applied to the owning FMaterial
	TArray<TRefCountPtr<FMaterialShaderMap> > LocalShaderMapReferences;
	TMap<FMaterial*, FMaterialShaderMap*> MaterialsToUpdate;
	TMap<FMaterial*, FMaterialShaderMap*> MaterialsToApplyToScene;

	// Process compiled shader maps in FIFO order, in case a shader map has been enqueued multiple times,
	// Which can happen if a material is edited while a background compile is going on
	for (TMap<int32, FShaderMapFinalizeResults>::TIterator ProcessIt(CompiledShaderMaps); ProcessIt; ++ProcessIt)
	{
		TRefCountPtr<FMaterialShaderMap> ShaderMap = NULL;
		TArray<FMaterial*>* Materials = NULL;

		for (TMap<TRefCountPtr<FMaterialShaderMap>, TArray<FMaterial*> >::TIterator ShaderMapIt(FMaterialShaderMap::ShaderMapsBeingCompiled); ShaderMapIt; ++ShaderMapIt)
		{
			if (ShaderMapIt.Key()->CompilingId == ProcessIt.Key())
			{
				ShaderMap = ShaderMapIt.Key();
				Materials = &ShaderMapIt.Value();
				break;
			}
		}

		check((ShaderMap && Materials) || ProcessIt.Key() == GlobalShaderMapId);

		if (ShaderMap && Materials)
		{
			TArray<FString> Errors;
			FShaderMapFinalizeResults& CompileResults = ProcessIt.Value();
			const TArray<FShaderCommonCompileJob*>& ResultArray = CompileResults.FinishedJobs;

			// Make a copy of the array as this entry of FMaterialShaderMap::ShaderMapsBeingCompiled will be removed below
			TArray<FMaterial*> MaterialsArray = *Materials;
			bool bSuccess = true;

			for (int32 JobIndex = 0; JobIndex < ResultArray.Num(); JobIndex++)
			{
				FShaderCommonCompileJob& CurrentJob = *ResultArray[JobIndex];
				bSuccess = bSuccess && CurrentJob.bSucceeded;

				auto* SingleJob = CurrentJob.GetSingleShaderJob();
				if (SingleJob)
				{
					CheckSingleJob(SingleJob, Errors);
				}
				else
				{
					auto* PipelineJob = CurrentJob.GetShaderPipelineJob();
					for (int32 Index = 0; Index < PipelineJob->StageJobs.Num(); ++Index)
					{
						CheckSingleJob(PipelineJob->StageJobs[Index]->GetSingleShaderJob(), Errors);
					}
				}
			}

			bool bShaderMapComplete = true;

			if (bSuccess)
			{
				bShaderMapComplete = ShaderMap->ProcessCompilationResults(ResultArray, CompileResults.FinalizeJobIndex, TimeBudget, CompileResults.SharedPipelines);
			}


			if (bShaderMapComplete)
			{
				ShaderMap->bCompiledSuccessfully = bSuccess;

				// Pass off the reference of the shader map to LocalShaderMapReferences
				LocalShaderMapReferences.Add(ShaderMap);
				FMaterialShaderMap::ShaderMapsBeingCompiled.Remove(ShaderMap);
#if DEBUG_INFINITESHADERCOMPILE
				UE_LOG(LogTemp, Display, TEXT("Finished compile of shader map 0x%08X%08X"), (int)((int64)(ShaderMap.GetReference()) >> 32), (int)((int64)(ShaderMap.GetReference())));
#endif
				for (int32 MaterialIndex = 0; MaterialIndex < MaterialsArray.Num(); MaterialIndex++)
				{
					FMaterial* Material = MaterialsArray[MaterialIndex];
					FMaterialShaderMap* CompletedShaderMap = ShaderMap;
#if DEBUG_INFINITESHADERCOMPILE
					UE_LOG(LogTemp, Display, TEXT("Shader map %s complete, GameThreadShaderMap 0x%08X%08X, marking material %s as finished"), *ShaderMap->GetFriendlyName(), (int)((int64)(ShaderMap.GetReference()) >> 32), (int)((int64)(ShaderMap.GetReference())), *Material->GetFriendlyName());

					UE_LOG(LogTemp, Display, TEXT("Marking material as finished 0x%08X%08X"), (int)((int64)(Material) >> 32), (int)((int64)(Material)));
#endif
					Material->RemoveOutstandingCompileId(ShaderMap->CompilingId);

					// Only process results that still match the ID which requested a compile
					// This avoids applying shadermaps which are out of date and a newer one is in the async compiling pipeline
					if (Material->GetMaterialId() == CompletedShaderMap->GetShaderMapId().BaseMaterialId)
					{
						if (!bSuccess)
						{
							// Propagate error messages
							Material->CompileErrors = Errors;
							MaterialsToUpdate.Add( Material, NULL );

							if (Material->IsDefaultMaterial())
							{
								// Log the errors unsuppressed before the fatal error, so it's always obvious from the log what the compile error was
								for (int32 ErrorIndex = 0; ErrorIndex < Errors.Num(); ErrorIndex++)
								{
									UE_LOG(LogShaderCompilers, Warning, TEXT("	%s"), *Errors[ErrorIndex]);
								}
								// Assert if a default material could not be compiled, since there will be nothing for other failed materials to fall back on.
								UE_LOG(LogShaderCompilers, Fatal,TEXT("Failed to compile default material %s!"), *Material->GetBaseMaterialPathName());
							}

							UE_ASSET_LOG(LogShaderCompilers, Warning, *Material->GetBaseMaterialPathName(), TEXT("Failed to compile Material for platform %s, Default Material will be used in game."),
								*LegacyShaderPlatformToShaderFormat(ShaderMap->GetShaderPlatform()).ToString());

							for (int32 ErrorIndex = 0; ErrorIndex < Errors.Num(); ErrorIndex++)
							{
								FString ErrorMessage = Errors[ErrorIndex];
								// Work around build machine string matching heuristics that will cause a cook to fail
								ErrorMessage.ReplaceInline(TEXT("error "), TEXT("err0r "), ESearchCase::CaseSensitive);
								UE_LOG(LogShaderCompilers, Log, TEXT("	%s"), *ErrorMessage);
							}
						}
						else
						{
							// if we succeeded and our shader map is not complete this could be because the material was being edited quicker then the compile could be completed
							// Don't modify materials for which the compiled shader map is no longer complete
							// This can happen if a material being compiled is edited, or if CheckMaterialUsage changes a flag and causes a recompile
							if (CompletedShaderMap->IsComplete(Material, true))
							{
								MaterialsToUpdate.Add(Material, CompletedShaderMap);
								// Note: if !CompileResults.bApplyCompletedShaderMapForRendering, RenderingThreadShaderMap must be set elsewhere to match up with the new value of GameThreadShaderMap
								if (CompileResults.bApplyCompletedShaderMapForRendering)
								{
									MaterialsToApplyToScene.Add(Material, CompletedShaderMap);
								}
							}

							if (GShowShaderWarnings && Errors.Num() > 0)
							{
								UE_LOG(LogShaderCompilers, Warning, TEXT("Warnings while compiling Material %s for platform %s:"),
									*Material->GetBaseMaterialPathName(),
									*LegacyShaderPlatformToShaderFormat(ShaderMap->GetShaderPlatform()).ToString());
								for (int32 ErrorIndex = 0; ErrorIndex < Errors.Num(); ErrorIndex++)
								{
									UE_LOG(LogShaders, Warning, TEXT("	%s"), *Errors[ErrorIndex]);
								}
							}
						}
					}
				}

				// Cleanup shader jobs and compile tracking structures
				for (int32 JobIndex = 0; JobIndex < ResultArray.Num(); JobIndex++)
				{
					delete ResultArray[JobIndex];
				}

				CompiledShaderMaps.Remove(ShaderMap->CompilingId);
			}

			if (TimeBudget < 0)
			{
				break;
			}
		}
		else if (ProcessIt.Key() == GlobalShaderMapId)
		{
			FShaderMapFinalizeResults* GlobalShaderResults = CompiledShaderMaps.Find(GlobalShaderMapId);

			if (GlobalShaderResults)
			{
				const TArray<FShaderCommonCompileJob*>& CompilationResults = GlobalShaderResults->FinishedJobs;

				ProcessCompiledGlobalShaders(CompilationResults);

				for (int32 ResultIndex = 0; ResultIndex < CompilationResults.Num(); ResultIndex++)
				{
					delete CompilationResults[ResultIndex];
				}

				CompiledShaderMaps.Remove(GlobalShaderMapId);
			}
		}
	}

	if (MaterialsToUpdate.Num() > 0)
	{
		for (TMap<FMaterial*, FMaterialShaderMap*>::TConstIterator It(MaterialsToUpdate); It; ++It)
		{
			FMaterial* Material = It.Key();
			FMaterialShaderMap* ShaderMap = It.Value();
			check(!ShaderMap || ShaderMap->IsValidForRendering());

			Material->SetGameThreadShaderMap(It.Value());
		}

		const TSet<FSceneInterface*>& AllocatedScenes = GetRendererModule().GetAllocatedScenes();

		for (TSet<FSceneInterface*>::TConstIterator SceneIt(AllocatedScenes); SceneIt; ++SceneIt)
		{
			(*SceneIt)->SetShaderMapsOnMaterialResources(MaterialsToApplyToScene);
		}

		for (TMap<FMaterial*, FMaterialShaderMap*>::TIterator It(MaterialsToUpdate); It; ++It)
		{
			FMaterial* Material = It.Key();

			Material->NotifyCompilationFinished();
		}

		PropagateMaterialChangesToPrimitives(MaterialsToUpdate);

#if WITH_EDITOR
		FEditorSupportDelegates::RedrawAllViewports.Broadcast();
#endif // WITH_EDITOR
	}
}




void FShaderCompilingManager::PropagateMaterialChangesToPrimitives(const TMap<FMaterial*, FMaterialShaderMap*>& MaterialsToUpdate)
{
	TArray<UMaterialInterface*> UsedMaterials;
	TIndirectArray<FComponentRecreateRenderStateContext> ComponentContexts;

	for (TObjectIterator<UPrimitiveComponent> PrimitiveIt; PrimitiveIt; ++PrimitiveIt)
	{
		UPrimitiveComponent* PrimitiveComponent = *PrimitiveIt;

		if (PrimitiveComponent->IsRenderStateCreated())
		{
			UsedMaterials.Reset();
			bool bPrimitiveIsDependentOnMaterial = false;

			// Note: relying on GetUsedMaterials to be accurate, or else we won't propagate to the right primitives and the renderer will crash later
			// FPrimitiveSceneProxy::VerifyUsedMaterial is used to make sure that all materials used for rendering are reported in GetUsedMaterials
			PrimitiveComponent->GetUsedMaterials(UsedMaterials);

			if (UsedMaterials.Num() > 0)
			{
				for (TMap<FMaterial*, FMaterialShaderMap*>::TConstIterator MaterialIt(MaterialsToUpdate); MaterialIt; ++MaterialIt)
				{
					FMaterial* UpdatedMaterial = MaterialIt.Key();
					UMaterialInterface* UpdatedMaterialInterface = UpdatedMaterial->GetMaterialInterface();
						
					if (UpdatedMaterialInterface)
					{
						for (int32 MaterialIndex = 0; MaterialIndex < UsedMaterials.Num(); MaterialIndex++)
						{
							UMaterialInterface* TestMaterial = UsedMaterials[MaterialIndex];

							if (TestMaterial && (TestMaterial == UpdatedMaterialInterface || TestMaterial->IsDependent(UpdatedMaterialInterface)))
							{
								bPrimitiveIsDependentOnMaterial = true;
								break;
							}
						}
					}
				}

				if (bPrimitiveIsDependentOnMaterial)
				{
					new(ComponentContexts) FComponentRecreateRenderStateContext(PrimitiveComponent);
				}
			}
		}
	}

	ComponentContexts.Empty();
}


/**
 * Shutdown the shader compile manager
 * this function should be used when ending the game to shutdown shader compile threads
 * will not complete current pending shader compilation
 */
void FShaderCompilingManager::Shutdown()
{
	Thread->Stop();
	Thread->WaitForCompletion();
}


bool FShaderCompilingManager::HandlePotentialRetryOnError(TMap<int32, FShaderMapFinalizeResults>& CompletedShaderMaps)
{
	bool bRetryCompile = false;

	for (TMap<int32, FShaderMapFinalizeResults>::TIterator It(CompletedShaderMaps); It; ++It)
	{
		FShaderMapFinalizeResults& Results = It.Value();

		if (!Results.bAllJobsSucceeded)
		{
			bool bSpecialEngineMaterial = false;
			const FMaterialShaderMap* ShaderMap = NULL;

			for (TMap<TRefCountPtr<FMaterialShaderMap>, TArray<FMaterial*> >::TConstIterator ShaderMapIt(FMaterialShaderMap::ShaderMapsBeingCompiled); ShaderMapIt; ++ShaderMapIt)
			{
				const FMaterialShaderMap* TestShaderMap = ShaderMapIt.Key();
				checkSlow(TestShaderMap);

				if (TestShaderMap->CompilingId == It.Key())
				{
					ShaderMap = TestShaderMap;

					for (int32 MaterialIndex = 0; MaterialIndex < ShaderMapIt.Value().Num(); MaterialIndex++)
					{
						FMaterial* Material = ShaderMapIt.Value()[MaterialIndex];
						bSpecialEngineMaterial = bSpecialEngineMaterial || Material->IsSpecialEngineMaterial();
					}
					break;
				}
			}


#if WITH_EDITORONLY_DATA

			if (UE_LOG_ACTIVE(LogShaders, Log) 
				// Always log detailed errors when a special engine material or global shader fails to compile, as those will be fatal errors
				|| bSpecialEngineMaterial 
				|| It.Key() == GlobalShaderMapId)
			{
				const TArray<FShaderCommonCompileJob*>& CompleteJobs = Results.FinishedJobs;
				TArray<const FShaderCommonCompileJob*> ErrorJobs;
				TArray<FString> UniqueErrors;
				TArray<EShaderPlatform> ErrorPlatforms;

				// Gather unique errors
				for (int32 JobIndex = 0; JobIndex < CompleteJobs.Num(); JobIndex++)
				{
					const FShaderCommonCompileJob& CurrentJob = *CompleteJobs[JobIndex];
					if (!CurrentJob.bSucceeded)
					{
						const auto* SingleJob = CurrentJob.GetSingleShaderJob();
						if (SingleJob)
						{
							AddErrorsForFailedJob(*SingleJob, ErrorPlatforms, UniqueErrors, ErrorJobs);
						}
						else
						{
							const auto* PipelineJob = CurrentJob.GetShaderPipelineJob();
							check(PipelineJob);
							for (const FShaderCommonCompileJob* CommonJob : PipelineJob->StageJobs)
							{
								AddErrorsForFailedJob(*CommonJob->GetSingleShaderJob(), ErrorPlatforms, UniqueErrors, ErrorJobs);
							}
						}
					}
				}

				FString TargetShaderPlatformString;

				for (int32 PlatformIndex = 0; PlatformIndex < ErrorPlatforms.Num(); PlatformIndex++)
				{
					if (TargetShaderPlatformString.IsEmpty())
					{
						TargetShaderPlatformString = LegacyShaderPlatformToShaderFormat(ErrorPlatforms[PlatformIndex]).ToString();
					}
					else
					{
						TargetShaderPlatformString += FString(TEXT(", ")) + LegacyShaderPlatformToShaderFormat(ErrorPlatforms[PlatformIndex]).ToString();
					}
				}

				const TCHAR* MaterialName = ShaderMap ? *ShaderMap->GetFriendlyName() : TEXT("global shaders");
				FString ErrorString = FString::Printf(TEXT("%i Shader compiler errors compiling %s for platform %s:"), UniqueErrors.Num(), MaterialName, *TargetShaderPlatformString);
				UE_LOG(LogShaderCompilers, Warning, TEXT("%s"), *ErrorString);
				ErrorString += TEXT("\n");

				for (int32 JobIndex = 0; JobIndex < CompleteJobs.Num(); JobIndex++)
				{
					const FShaderCommonCompileJob& CurrentJob = *CompleteJobs[JobIndex];
					if (!CurrentJob.bSucceeded)
					{
						const auto* SingleJob = CurrentJob.GetSingleShaderJob();
						if (SingleJob)
						{
							ProcessErrors(*SingleJob, UniqueErrors, ErrorString);
						}
						else
						{
							const auto* PipelineJob = CurrentJob.GetShaderPipelineJob();
							check(PipelineJob);
							for (const FShaderCommonCompileJob* CommonJob : PipelineJob->StageJobs)
							{
								ProcessErrors(*CommonJob->GetSingleShaderJob(), UniqueErrors, ErrorString);
							}
						}
					}
				}

				if (UE_LOG_ACTIVE(LogShaders, Log) && bPromptToRetryFailedShaderCompiles)
				{
#if UE_BUILD_DEBUG
					// Use debug break in debug with the debugger attached, otherwise message box
					if (FPlatformMisc::IsDebuggerPresent())
					{
						// A shader compile error has occurred, see the debug output for information.
						// Double click the errors in the VS.NET output window and the IDE will take you directly to the file and line of the error.
						// Check ErrorJobs for more state on the failed shaders, for example in-memory includes like Material.usf
						FPlatformMisc::DebugBreak();
						// Set GRetryShaderCompilation to true in the debugger to enable retries in debug
						// NOTE: MaterialTemplate.usf will not be reloaded when retrying!
						bRetryCompile = GRetryShaderCompilation;
					}
					else 
#endif	//UE_BUILD_DEBUG
						if (FPlatformMisc::MessageBoxExt( EAppMsgType::YesNo, *FText::Format(NSLOCTEXT("UnrealEd", "Error_RetryShaderCompilation", "{0}\r\n\r\nRetry compilation?"),
							FText::FromString(ErrorString)).ToString(), TEXT("Error")) == EAppReturnType::Type::Yes)
						{
							bRetryCompile = true;
						}
				}

				if (bRetryCompile)
				{
					break;
				}
			}
#endif	//WITH_EDITORONLY_DATA
		}
	}

	if (bRetryCompile)
	{
		// Flush the shader file cache so that any changes will be propagated.
		FlushShaderFileCache();

		TArray<int32> MapsToRemove;

		for (TMap<int32, FShaderMapFinalizeResults>::TIterator It(CompletedShaderMaps); It; ++It)
		{
			FShaderMapFinalizeResults& Results = It.Value();

			if (!Results.bAllJobsSucceeded)
			{
				MapsToRemove.Add(It.Key());

				// Reset outputs
				for (int32 JobIndex = 0; JobIndex < Results.FinishedJobs.Num(); JobIndex++)
				{
					FShaderCommonCompileJob& CurrentJob = *Results.FinishedJobs[JobIndex];
					auto* SingleJob = CurrentJob.GetSingleShaderJob();

					// NOTE: Changes to MaterialTemplate.usf before retrying won't work, because the entry for Material.usf in CurrentJob.Environment.IncludeFileNameToContentsMap isn't reset
					if (SingleJob)
					{
						SingleJob->Output = FShaderCompilerOutput();
					}
					else
					{
						auto* PipelineJob = CurrentJob.GetShaderPipelineJob();
						for (FShaderCommonCompileJob* CommonJob : PipelineJob->StageJobs)
						{
							CommonJob->GetSingleShaderJob()->Output = FShaderCompilerOutput();
							CommonJob->bFinalized = false;
						}
					}
					CurrentJob.bFinalized = false;
				}

				// Send all the shaders from this shader map through the compiler again
				AddJobs(Results.FinishedJobs, Results.bApplyCompletedShaderMapForRendering, true, Results.bRecreateComponentRenderStateOnCompletion);
			}
		}

		const int32 OriginalNumShaderMaps = CompletedShaderMaps.Num();

		// Remove the failed shader maps
		for (int32 RemoveIndex = 0; RemoveIndex < MapsToRemove.Num(); RemoveIndex++)
		{
			CompletedShaderMaps.Remove(MapsToRemove[RemoveIndex]);
		}

		check(CompletedShaderMaps.Num() == OriginalNumShaderMaps - MapsToRemove.Num());

		// Block until the failed shader maps have been compiled again
		BlockOnShaderMapCompletion(MapsToRemove, CompletedShaderMaps);

		check(CompletedShaderMaps.Num() == OriginalNumShaderMaps);
	}

	return bRetryCompile;
}

void FShaderCompilingManager::CancelCompilation(const TCHAR* MaterialName, const TArray<int32>& ShaderMapIdsToCancel)
{
	check(!FPlatformProperties::RequiresCookedData());
	UE_LOG(LogShaders, Log, TEXT("CancelCompilation %s "), MaterialName ? MaterialName : TEXT(""));

	// Lock CompileQueueSection so we can access the input and output queues
	FScopeLock Lock(&CompileQueueSection);

	int32 TotalNumJobsRemoved = 0;
	for (int32 IdIndex = 0; IdIndex < ShaderMapIdsToCancel.Num(); ++IdIndex)
	{
		int32 MapIdx = ShaderMapIdsToCancel[IdIndex];
		if (FShaderMapCompileResults* ShaderMapJob = ShaderMapJobs.Find(MapIdx))
		{
			int32 NumJobsRemoved = 0;

			int32 JobIndex = CompileQueue.Num();
			while ( --JobIndex >= 0 )
			{
				if (FShaderCommonCompileJob* Job = CompileQueue[JobIndex])
				{
					if (Job->Id == MapIdx)
					{
						FShaderPipelineCompileJob* PipelineJob = Job->GetShaderPipelineJob();
						if (PipelineJob)
						{
							TotalNumJobsRemoved += PipelineJob->StageJobs.Num();
							NumJobsRemoved += PipelineJob->StageJobs.Num();
						}
						else
						{
							++TotalNumJobsRemoved;
							++NumJobsRemoved;
						}
						CompileQueue.RemoveAt(JobIndex, 1, false);
					}
				}
			}

			ShaderMapJob->NumJobsQueued -= NumJobsRemoved;

			if (ShaderMapJob->NumJobsQueued == 0)
			{
				//We've removed all the jobs for this shader map so remove it.
				ShaderMapJobs.Remove(MapIdx);
			}
		}
	}
	CompileQueue.Shrink();

	// Using atomics to update NumOutstandingJobs since it is read outside of the critical section
	FPlatformAtomics::InterlockedAdd(&NumOutstandingJobs, -TotalNumJobsRemoved);
}

void FShaderCompilingManager::FinishCompilation(const TCHAR* MaterialName, const TArray<int32>& ShaderMapIdsToFinishCompiling)
{
	check(!FPlatformProperties::RequiresCookedData());
	const double StartTime = FPlatformTime::Seconds();

	FText StatusUpdate;
	if ( MaterialName != NULL )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("MaterialName"), FText::FromString( MaterialName ) );
		StatusUpdate = FText::Format( NSLOCTEXT("ShaderCompilingManager", "CompilingShadersForMaterialStatus", "Compiling shaders: {MaterialName}..."), Args );
	}
	else
	{
		StatusUpdate = NSLOCTEXT("ShaderCompilingManager", "CompilingShadersStatus", "Compiling shaders...");
	}

	FScopedSlowTask SlowTask(0, StatusUpdate, GIsEditor && !IsRunningCommandlet());

	TMap<int32, FShaderMapFinalizeResults> CompiledShaderMaps;
	CompiledShaderMaps.Append( PendingFinalizeShaderMaps );
	PendingFinalizeShaderMaps.Empty();
	BlockOnShaderMapCompletion(ShaderMapIdsToFinishCompiling, CompiledShaderMaps);

	bool bRetry = false;
	do 
	{
		bRetry = HandlePotentialRetryOnError(CompiledShaderMaps);
	} 
	while (bRetry);

	ProcessCompiledShaderMaps(CompiledShaderMaps, FLT_MAX);
	check(CompiledShaderMaps.Num() == 0);

	const double EndTime = FPlatformTime::Seconds();

	UE_LOG(LogShaders, Log, TEXT("FinishCompilation %s %.3fs"), MaterialName ? MaterialName : TEXT(""), (float)(EndTime - StartTime));
}

void FShaderCompilingManager::FinishAllCompilation()
{
	check(!FPlatformProperties::RequiresCookedData());
	const double StartTime = FPlatformTime::Seconds();

	TMap<int32, FShaderMapFinalizeResults> CompiledShaderMaps;
	CompiledShaderMaps.Append( PendingFinalizeShaderMaps );
	PendingFinalizeShaderMaps.Empty();
	BlockOnAllShaderMapCompletion(CompiledShaderMaps);

	bool bRetry = false;
	do 
	{
		bRetry = HandlePotentialRetryOnError(CompiledShaderMaps);
	} 
	while (bRetry);

	ProcessCompiledShaderMaps(CompiledShaderMaps, FLT_MAX);
	check(CompiledShaderMaps.Num() == 0);

	const double EndTime = FPlatformTime::Seconds();

	UE_LOG(LogShaders, Log, TEXT("FinishAllCompilation %.3fs"), (float)(EndTime - StartTime));
}

void FShaderCompilingManager::ProcessAsyncResults(bool bLimitExecutionTime, bool bBlockOnGlobalShaderCompletion)
{
	COOK_STAT(FScopedDurationTimer Timer(ShaderCompilerCookStats::ProcessAsyncResultsTimeSec));
	if (bAllowAsynchronousShaderCompiling)
	{
		Thread->CheckHealth();
		{
			const double StartTime = FPlatformTime::Seconds();

			// Block on global shaders before checking for shader maps to finalize
			// So if we block on global shaders for a long time, we will get a chance to finalize all the non-global shader maps completed during that time.
			if (bBlockOnGlobalShaderCompletion)
			{
				TArray<int32> ShaderMapId;
				ShaderMapId.Add(GlobalShaderMapId);

				// Block until the global shader map jobs are complete
				GShaderCompilingManager->BlockOnShaderMapCompletion(ShaderMapId, PendingFinalizeShaderMaps);
			}

			int32 NumCompilingShaderMaps = 0;

			{
				// Lock CompileQueueSection so we can access the input and output queues
				FScopeLock Lock(&CompileQueueSection);

				if (!bBlockOnGlobalShaderCompletion)
				{
					bCompilingDuringGame = true;
				}

				TArray<int32> ShaderMapsToRemove;

				// Get all material shader maps to finalize
				//
				for (TMap<int32, FShaderMapCompileResults>::TIterator It(ShaderMapJobs); It; ++It)
				{
					const FShaderMapCompileResults& Results = It.Value();

					if (GetNumTotalJobs(Results.FinishedJobs) == Results.NumJobsQueued)
					{
						ShaderMapsToRemove.Add(It.Key());
						PendingFinalizeShaderMaps.Add(It.Key(), FShaderMapFinalizeResults(Results));
					}
				}

				for (int32 RemoveIndex = 0; RemoveIndex < ShaderMapsToRemove.Num(); RemoveIndex++)
				{
					ShaderMapJobs.Remove(ShaderMapsToRemove[RemoveIndex]);
				}

				NumCompilingShaderMaps = ShaderMapJobs.Num();
			}

			int32 NumPendingShaderMaps = PendingFinalizeShaderMaps.Num();

			if (PendingFinalizeShaderMaps.Num() > 0)
			{
				bool bRetry = false;
				do 
				{
					bRetry = HandlePotentialRetryOnError(PendingFinalizeShaderMaps);
				} 
				while (bRetry);

				const float TimeBudget = bLimitExecutionTime ? ProcessGameThreadTargetTime : FLT_MAX;
				ProcessCompiledShaderMaps(PendingFinalizeShaderMaps, TimeBudget);
				check(bLimitExecutionTime || PendingFinalizeShaderMaps.Num() == 0);
			}


			if (bBlockOnGlobalShaderCompletion)
			{
				check(PendingFinalizeShaderMaps.Num() == 0);

				if (NumPendingShaderMaps - PendingFinalizeShaderMaps.Num() > 0)
				{
					UE_LOG(LogShaders, Warning, TEXT("Blocking ProcessAsyncResults for %.1fs, processed %u shader maps, %u being compiled"), 
						(float)(FPlatformTime::Seconds() - StartTime),
						NumPendingShaderMaps - PendingFinalizeShaderMaps.Num(), 
						NumCompilingShaderMaps);
				}
			}
			else if (NumPendingShaderMaps - PendingFinalizeShaderMaps.Num() > 0)
			{
				UE_LOG(LogShaders, Log, TEXT("Completed %u async shader maps, %u more pending, %u being compiled"), 
					NumPendingShaderMaps - PendingFinalizeShaderMaps.Num(), 
					PendingFinalizeShaderMaps.Num(),
					NumCompilingShaderMaps);
			}
		}
	}
	else
	{
		check(CompileQueue.Num() == 0);
	}
}

bool FShaderCompilingManager::IsShaderCompilerWorkerRunning(FProcHandle & WorkerHandle)
{
	return FPlatformProcess::IsProcRunning(WorkerHandle);
}

/* Generates a uniform buffer struct member hlsl declaration using the member's metadata. */
static void GenerateUniformBufferStructMember(FString& Result, const FUniformBufferStruct::FMember& Member)
{
	// Generate the base type name.
	FString BaseTypeName;
	switch (Member.GetBaseType())
	{
		case UBMT_BOOL:    BaseTypeName = TEXT("bool"); break;
		case UBMT_INT32:   BaseTypeName = TEXT("int"); break;
		case UBMT_UINT32:  BaseTypeName = TEXT("uint"); break;
		case UBMT_FLOAT32:
			if (Member.GetPrecision() == EShaderPrecisionModifier::Float)
			{
				BaseTypeName = TEXT("float");
			}
			else if (Member.GetPrecision() == EShaderPrecisionModifier::Half)
			{
				BaseTypeName = TEXT("half");
			}
			else if (Member.GetPrecision() == EShaderPrecisionModifier::Fixed)
			{
				BaseTypeName = TEXT("fixed");
			}
			break;
		default:
			UE_LOG(LogShaders, Fatal, TEXT("Unrecognized uniform buffer struct member base type."));
	};

	// Generate the type dimensions for vectors and matrices.
	FString TypeDim;
	if (Member.GetNumRows() > 1)
	{
		TypeDim = FString::Printf(TEXT("%ux%u"), Member.GetNumRows(), Member.GetNumColumns());
	}
	else if (Member.GetNumColumns() > 1)
	{
		TypeDim = FString::Printf(TEXT("%u"), Member.GetNumColumns());
	}

	// Generate array dimension post fix
	FString ArrayDim;
	if (Member.GetNumElements() > 0)
	{
		ArrayDim = FString::Printf(TEXT("[%u]"), Member.GetNumElements());
	}

	Result = FString::Printf(TEXT("%s%s %s%s"), *BaseTypeName, *TypeDim, Member.GetName(), *ArrayDim);
}

/* Generates the instanced stereo hlsl code that's dependent on view uniform declarations. */
static void GenerateInstancedStereoCode(FString& Result)
{
	// Find the InstancedView uniform buffer struct
	const FUniformBufferStruct* InstancedView = nullptr;
	for (TLinkedList<FUniformBufferStruct*>::TIterator StructIt(FUniformBufferStruct::GetStructList()); StructIt; StructIt.Next())
	{
		if (StructIt->GetShaderVariableName() == FString(TEXT("InstancedView")))
		{
			InstancedView = *StructIt;
			break;
		}
	}
	checkSlow(InstancedView != nullptr);
	const TArray<FUniformBufferStruct::FMember>& StructMembers = InstancedView->GetMembers();

	// ViewState definition
	Result =  "struct ViewState\r\n";
	Result += "{\r\n";
	for (int32 MemberIndex = 0; MemberIndex < StructMembers.Num(); ++MemberIndex)
	{
		const FUniformBufferStruct::FMember& Member = StructMembers[MemberIndex];
		FString MemberDecl;
		GenerateUniformBufferStructMember(MemberDecl, StructMembers[MemberIndex]);
		Result += FString::Printf(TEXT("\t%s;\r\n"), *MemberDecl);
	}
	Result += "};\r\n";

	// GetPrimaryView definition
	Result += "ViewState GetPrimaryView()\r\n";
	Result += "{\r\n";
	Result += "\tViewState Result;\r\n";
	for (int32 MemberIndex = 0; MemberIndex < StructMembers.Num(); ++MemberIndex)
	{
		const FUniformBufferStruct::FMember& Member = StructMembers[MemberIndex];
		Result += FString::Printf(TEXT("\tResult.%s = View.%s;\r\n"), Member.GetName(), Member.GetName());
	}
	Result += "\treturn Result;\r\n";
	Result += "}\r\n";

	// GetInstancedView definition
	Result += "ViewState GetInstancedView()\r\n";
	Result += "{\r\n";
	Result += "\tViewState Result;\r\n";
	for (int32 MemberIndex = 0; MemberIndex < StructMembers.Num(); ++MemberIndex)
	{
		const FUniformBufferStruct::FMember& Member = StructMembers[MemberIndex];
		Result += FString::Printf(TEXT("\tResult.%s = InstancedView.%s;\r\n"), Member.GetName(), Member.GetName());
	}
	Result += "\treturn Result;\r\n";
	Result += "}\r\n";
	
	// ResolveView definition for metal, this allows us to change the branch to a conditional move in the cross compiler
	Result += "#if COMPILER_METAL\r\n";
	Result += "ViewState ResolveView(uint ViewIndex)\r\n";
	Result += "{\r\n";
	Result += "\tViewState Result;\r\n";
	for (int32 MemberIndex = 0; MemberIndex < StructMembers.Num(); ++MemberIndex)
	{
		const FUniformBufferStruct::FMember& Member = StructMembers[MemberIndex];
		Result += FString::Printf(TEXT("\tResult.%s = (ViewIndex == 0) ? View.%s : InstancedView.%s;\r\n"), Member.GetName(), Member.GetName(), Member.GetName());
	}
	Result += "\treturn Result;\r\n";
	Result += "}\r\n";
	Result += "#endif\r\n";
}

/** Enqueues a shader compile job with GShaderCompilingManager. */
void GlobalBeginCompileShader(
	const FString& DebugGroupName,
	FVertexFactoryType* VFType,
	FShaderType* ShaderType,
	const FShaderPipelineType* ShaderPipelineType,
	const TCHAR* SourceFilename,
	const TCHAR* FunctionName,
	FShaderTarget Target,
	FShaderCompileJob* NewJob,
	TArray<FShaderCommonCompileJob*>& NewJobs,
	bool bAllowDevelopmentShaderCompile
	)
{
	COOK_STAT(ShaderCompilerCookStats::GlobalBeginCompileShaderCalls++);
	COOK_STAT(FScopedDurationTimer DurationTimer(ShaderCompilerCookStats::GlobalBeginCompileShaderTimeSec));

	FShaderCompilerInput& Input = NewJob->Input;
	Input.Target = Target;
	Input.ShaderFormat = LegacyShaderPlatformToShaderFormat(EShaderPlatform(Target.Platform));
	Input.VirtualSourceFilePath = SourceFilename;
	Input.EntryPointName = FunctionName;
	Input.bCompilingForShaderPipeline = false;
	Input.bIncludeUsedOutputs = false;
	Input.bGenerateDirectCompileFile = (GDumpShaderDebugInfoSCWCommandLine != 0);
	Input.DumpDebugInfoRootPath = GShaderCompilingManager->GetAbsoluteShaderDebugInfoDirectory() / Input.ShaderFormat.ToString();
	// asset material name or "Global"
	Input.DebugGroupName = DebugGroupName;

	// Verify FShaderCompilerInput's file paths are consistent. 
	#if DO_CHECK
		check(CheckVirtualShaderFilePath(Input.VirtualSourceFilePath));

		checkf(FPaths::GetExtension(Input.VirtualSourceFilePath) == TEXT("usf"),
			TEXT("Incorrect virtual shader path extension for shader file to compile '%s': Only .usf files should be "
				 "compiled. .ush file are meant to be included only."),
			*Input.VirtualSourceFilePath);

		for (const auto& Entry : Input.Environment.IncludeVirtualPathToContentsMap)
		{
			FString VirtualShaderFilePath = Entry.Key;

			check(CheckVirtualShaderFilePath(VirtualShaderFilePath));

			checkf(VirtualShaderFilePath.Contains(TEXT("/Generated/")),
				TEXT("Incorrect virtual shader path for generated file '%s': Generated files must be located under an "
					 "non existing 'Generated' directory, for instance: /Engine/Generated/ or /Plugin/FooBar/Generated/."),
				*VirtualShaderFilePath);

			checkf(VirtualShaderFilePath == Input.VirtualSourceFilePath || FPaths::GetExtension(VirtualShaderFilePath) == TEXT("ush"),
				TEXT("Incorrect virtual shader path extension for generated file '%s': Generated file must either be the "
					 "USF to compile, or a USH file to be included."),
				*VirtualShaderFilePath);
		}
	#endif

	if (ShaderPipelineType)
	{
		Input.DebugGroupName = Input.DebugGroupName / ShaderPipelineType->GetName();
	}
	
	if (VFType)
	{
		FString VFName = VFType->GetName();
		if (GDumpShaderDebugInfoShort)
		{
			// Shorten vertex factory name
			if (VFName[0] == TCHAR('F') || VFName[0] == TCHAR('T'))
			{
				VFName.RemoveAt(0);
			}
			VFName.ReplaceInline(TEXT("VertexFactory"), TEXT("VF"));
			VFName.ReplaceInline(TEXT("GPUSkinAPEXCloth"), TEXT("APEX"));
			VFName.ReplaceInline(TEXT("true"), TEXT("_1"));
			VFName.ReplaceInline(TEXT("false"), TEXT("_0"));
		}
		Input.DebugGroupName = Input.DebugGroupName / VFName;
	}
	
	{
		FString ShaderTypeName = ShaderType->GetName();
		if (GDumpShaderDebugInfoShort)
		{
			// Shorten known types
			if (ShaderTypeName[0] == TCHAR('F') || ShaderTypeName[0] == TCHAR('T'))
			{
				ShaderTypeName.RemoveAt(0);
			}
		}
		Input.DebugGroupName = Input.DebugGroupName / ShaderTypeName;
		
		if (GDumpShaderDebugInfoShort)
		{
			Input.DebugGroupName.ReplaceInline(TEXT("BasePass"), TEXT("BP"));
			Input.DebugGroupName.ReplaceInline(TEXT("ForForward"), TEXT("Fwd"));
			Input.DebugGroupName.ReplaceInline(TEXT("Shadow"), TEXT("Shdw"));
			Input.DebugGroupName.ReplaceInline(TEXT("LightMap"), TEXT("LM"));
			Input.DebugGroupName.ReplaceInline(TEXT("EAtmosphereRenderFlag==E_"), TEXT(""));
			Input.DebugGroupName.ReplaceInline(TEXT("Atmospheric"), TEXT("Atm"));
			Input.DebugGroupName.ReplaceInline(TEXT("Atmosphere"), TEXT("Atm"));
			Input.DebugGroupName.ReplaceInline(TEXT("Ambient"), TEXT("Amb"));
			Input.DebugGroupName.ReplaceInline(TEXT("Perspective"), TEXT("Persp"));
			Input.DebugGroupName.ReplaceInline(TEXT("Occlusion"), TEXT("Occ"));
			Input.DebugGroupName.ReplaceInline(TEXT("Position"), TEXT("Pos"));
			Input.DebugGroupName.ReplaceInline(TEXT("Skylight"), TEXT("Sky"));
			Input.DebugGroupName.ReplaceInline(TEXT("LightingPolicy"), TEXT("LP"));
			Input.DebugGroupName.ReplaceInline(TEXT("TranslucentLighting"), TEXT("TranslLight"));
			Input.DebugGroupName.ReplaceInline(TEXT("Translucency"), TEXT("Transl"));
			Input.DebugGroupName.ReplaceInline(TEXT("DistanceField"), TEXT("DistFiel"));
			Input.DebugGroupName.ReplaceInline(TEXT("Indirect"), TEXT("Ind"));
			Input.DebugGroupName.ReplaceInline(TEXT("Cached"), TEXT("Cach"));
			Input.DebugGroupName.ReplaceInline(TEXT("Inject"), TEXT("Inj"));
			Input.DebugGroupName.ReplaceInline(TEXT("Visualization"), TEXT("Viz"));
			Input.DebugGroupName.ReplaceInline(TEXT("Instanced"), TEXT("Inst"));
			Input.DebugGroupName.ReplaceInline(TEXT("Evaluate"), TEXT("Eval"));
			Input.DebugGroupName.ReplaceInline(TEXT("Landscape"), TEXT("Land"));
			Input.DebugGroupName.ReplaceInline(TEXT("Dynamic"), TEXT("Dyn"));
			Input.DebugGroupName.ReplaceInline(TEXT("Vertex"), TEXT("Vtx"));
			Input.DebugGroupName.ReplaceInline(TEXT("Output"), TEXT("Out"));
			Input.DebugGroupName.ReplaceInline(TEXT("Directional"), TEXT("Dir"));
			Input.DebugGroupName.ReplaceInline(TEXT("Irradiance"), TEXT("Irr"));
			Input.DebugGroupName.ReplaceInline(TEXT("Deferred"), TEXT("Def"));
			Input.DebugGroupName.ReplaceInline(TEXT("true"), TEXT("_1"));
			Input.DebugGroupName.ReplaceInline(TEXT("false"), TEXT("_0"));
			Input.DebugGroupName.ReplaceInline(TEXT("PROPAGATE_AO"), TEXT("AO"));
			Input.DebugGroupName.ReplaceInline(TEXT("PROPAGATE_SECONDARY_OCCLUSION"), TEXT("SEC_OCC"));
			Input.DebugGroupName.ReplaceInline(TEXT("PROPAGATE_MULTIPLE_BOUNCES"), TEXT("MULT_BOUNC"));
			Input.DebugGroupName.ReplaceInline(TEXT("PostProcess"), TEXT("Post"));
			Input.DebugGroupName.ReplaceInline(TEXT("AntiAliasing"), TEXT("AA"));
			Input.DebugGroupName.ReplaceInline(TEXT("Mobile"), TEXT("Mob"));
			Input.DebugGroupName.ReplaceInline(TEXT("Linear"), TEXT("Lin"));
			Input.DebugGroupName.ReplaceInline(TEXT("INT32_MAX"), TEXT("IMAX"));
			Input.DebugGroupName.ReplaceInline(TEXT("Policy"), TEXT("Pol"));
		}
	}
	
	static const auto CVarShaderDevelopmentMode = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ShaderDevelopmentMode"));

	// Setup the debug info path if requested, or if this is a global shader and shader development mode is enabled
	if (GDumpShaderDebugInfo != 0 || (ShaderType->GetGlobalShaderType() != NULL && CVarShaderDevelopmentMode->GetInt() != 0))
	{
		Input.DumpDebugInfoPath = Input.DumpDebugInfoRootPath / Input.DebugGroupName;
		
		// Sanitize the name to be used as a path
		// List mostly comes from set of characters not allowed by windows in a path.  Just try to rename a file and type one of these for the list.
		Input.DumpDebugInfoPath.ReplaceInline(TEXT("<"), TEXT("("));
		Input.DumpDebugInfoPath.ReplaceInline(TEXT(">"), TEXT(")"));
		Input.DumpDebugInfoPath.ReplaceInline(TEXT("::"), TEXT("=="));
		Input.DumpDebugInfoPath.ReplaceInline(TEXT("|"), TEXT("_"));
		Input.DumpDebugInfoPath.ReplaceInline(TEXT("*"), TEXT("-"));
		Input.DumpDebugInfoPath.ReplaceInline(TEXT("?"), TEXT("!"));
		Input.DumpDebugInfoPath.ReplaceInline(TEXT("\""), TEXT("\'"));

		if (!IFileManager::Get().DirectoryExists(*Input.DumpDebugInfoPath))
		{
			verifyf(IFileManager::Get().MakeDirectory(*Input.DumpDebugInfoPath, true), TEXT("Failed to create directory for shader debug info '%s'"), *Input.DumpDebugInfoPath);
		}
	}

	// Add the appropriate definitions for the shader frequency.
	{
		Input.Environment.SetDefine(TEXT("PIXELSHADER"), Target.Frequency == SF_Pixel);
		Input.Environment.SetDefine(TEXT("DOMAINSHADER"), Target.Frequency == SF_Domain);
		Input.Environment.SetDefine(TEXT("HULLSHADER"), Target.Frequency == SF_Hull);
		Input.Environment.SetDefine(TEXT("VERTEXSHADER"), Target.Frequency == SF_Vertex);
		Input.Environment.SetDefine(TEXT("GEOMETRYSHADER"), Target.Frequency == SF_Geometry);
		Input.Environment.SetDefine(TEXT("COMPUTESHADER"), Target.Frequency == SF_Compute);
	}

	// #defines get stripped out by the preprocessor without this. We can override with this
	Input.Environment.SetDefine(TEXT("COMPILER_DEFINE"), TEXT("#define"));

	// Set VR definitions
	{
		static const auto CVarInstancedStereo = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.InstancedStereo"));
		static const auto CVarMultiView = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.MultiView"));
		static const auto CVarMobileMultiView = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.MobileMultiView"));
		static const auto CVarMonoscopicFarField = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.MonoscopicFarField"));

		const bool bIsInstancedStereoCVar = CVarInstancedStereo ? (CVarInstancedStereo->GetValueOnGameThread() != 0) : false;
		const bool bIsMultiViewCVar = CVarMultiView ? (CVarMultiView->GetValueOnGameThread() != 0) : false;
		const bool bIsMobileMultiViewCVar = CVarMobileMultiView ? (CVarMobileMultiView->GetValueOnGameThread() != 0) : false;
		const bool bIsMonoscopicFarField = CVarMonoscopicFarField && (CVarMonoscopicFarField->GetValueOnGameThread() != 0);

		const EShaderPlatform ShaderPlatform = static_cast<EShaderPlatform>(Target.Platform);
		
		const bool bIsInstancedStereo = bIsInstancedStereoCVar && RHISupportsInstancedStereo(ShaderPlatform);
		Input.Environment.SetDefine(TEXT("INSTANCED_STEREO"), bIsInstancedStereo);
		Input.Environment.SetDefine(TEXT("MULTI_VIEW"), bIsInstancedStereo && bIsMultiViewCVar && RHISupportsMultiView(ShaderPlatform));

		const bool bIsAndroidGLES = RHISupportsMobileMultiView(ShaderPlatform);
		Input.Environment.SetDefine(TEXT("MOBILE_MULTI_VIEW"), bIsMobileMultiViewCVar && bIsAndroidGLES);

		// Throw a warning if we are silently disabling ISR due to missing platform support.
		if (bIsInstancedStereoCVar && !bIsInstancedStereo && !GShaderCompilingManager->AreWarningsSuppressed(ShaderPlatform))
		{
			UE_LOG(LogShaderCompilers, Log, TEXT("Instanced stereo rendering is not supported for the %s shader platform."), *LegacyShaderPlatformToShaderFormat(ShaderPlatform).ToString());
			GShaderCompilingManager->SuppressWarnings(ShaderPlatform);
		}

		Input.Environment.SetDefine(TEXT("MONOSCOPIC_FAR_FIELD"), bIsMonoscopicFarField);
	}

	ShaderType->AddReferencedUniformBufferIncludes(Input.Environment, Input.SourceFilePrefix, (EShaderPlatform)Target.Platform);

	if (VFType)
	{
		VFType->AddReferencedUniformBufferIncludes(Input.Environment, Input.SourceFilePrefix, (EShaderPlatform)Target.Platform);
	}

	// Add generated instanced stereo code
	FString GeneratedInstancedStereoCode;
	GenerateInstancedStereoCode(GeneratedInstancedStereoCode);
	Input.Environment.IncludeVirtualPathToContentsMap.Add(TEXT("/Engine/Generated/GeneratedInstancedStereo.ush"), StringToArray<ANSICHAR>(*GeneratedInstancedStereoCode, GeneratedInstancedStereoCode.Len() + 1));


	{
		static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shaders.Optimize"));

		if (CVar->GetInt() == 0)
		{
			Input.Environment.CompilerFlags.Add(CFLAG_Debug);
		}
	}

	{
		static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shaders.KeepDebugInfo"));

		if (CVar->GetInt() != 0)
		{
			Input.Environment.CompilerFlags.Add(CFLAG_KeepDebugInfo);
		}
	}

	{
		static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shaders.FastMath"));
		if (CVar && CVar->GetInt() == 0)
		{
			Input.Environment.CompilerFlags.Add(CFLAG_NoFastMath);
		}
	}
	
	{
		static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shaders.FlowControlMode"));
		if (CVar)
		{
			switch(CVar->GetInt())
			{
				case 2:
					Input.Environment.CompilerFlags.Add(CFLAG_AvoidFlowControl);
					break;
				case 1:
					Input.Environment.CompilerFlags.Add(CFLAG_PreferFlowControl);
					break;
				case 0:
				default:
					break;
			}
		}
	}

	if (IsD3DPlatform((EShaderPlatform)Target.Platform, false))
	{
		static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.D3D.RemoveUnusedInterpolators"));
		if (CVar && CVar->GetInt() != 0)
		{
			Input.Environment.CompilerFlags.Add(CFLAG_ForceRemoveUnusedInterpolators);
		}
	} 
	
	if (IsMetalPlatform((EShaderPlatform)Target.Platform))
	{
		{
			static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shaders.ZeroInitialise"));
			
			if (CVar->GetInt() != 0)
			{
				Input.Environment.CompilerFlags.Add(CFLAG_ZeroInitialise);
			}
		}
		{
			static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shaders.BoundsChecking"));
			
			if (CVar->GetInt() != 0)
			{
				Input.Environment.CompilerFlags.Add(CFLAG_BoundsChecking);
			}
		}
		
		// Check whether we can compile metal shaders to bytecode - avoids poisoning the DDC
		static ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
		const FName Format = LegacyShaderPlatformToShaderFormat(EShaderPlatform(Target.Platform));
		const IShaderFormat* Compiler = TPM.FindShaderFormat(Format);
		static const bool bCanCompileOfflineMetalShaders = Compiler && Compiler->CanCompileBinaryShaders();
		if (!bCanCompileOfflineMetalShaders)
		{
			Input.Environment.CompilerFlags.Add(CFLAG_Debug);
		}
		else
		{
			// populate the data in the shader input environment
			FString RemoteServer;
			FString UserName;
			FString SSHKey;
			GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("RemoteServerName"), RemoteServer, GEngineIni);
			GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("RSyncUsername"), UserName, GEngineIni);
			GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("SSHPrivateKeyOverridePath"), SSHKey, GEngineIni);
			Input.Environment.RemoteServerData.Add(TEXT("RemoteServerName"), RemoteServer);
			Input.Environment.RemoteServerData.Add(TEXT("RSyncUsername"), UserName);
			if (SSHKey.Len() > 0)
			{
				Input.Environment.RemoteServerData.Add(TEXT("SSHPrivateKeyOverridePath"), SSHKey);
			}
		}
		
		// Shaders built for archiving - for Metal that requires compiling the code in a different way so that we can strip it later
		bool bArchive = false;
		GConfig->GetBool(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("bSharedMaterialNativeLibraries"), bArchive, GGameIni);
		if (bCanCompileOfflineMetalShaders && bArchive)
		{
			Input.Environment.CompilerFlags.Add(CFLAG_Archive);
		}
		
		{
			uint32 ShaderVersion = RHIGetShaderLanguageVersion(EShaderPlatform(Target.Platform));
			Input.Environment.SetDefine(TEXT("MAX_SHADER_LANGUAGE_VERSION"), ShaderVersion);
			
			FString AllowFastIntrinsics;
			bool bEnableMathOptimisations = true;
			if (IsPCPlatform(EShaderPlatform(Target.Platform)))
			{
				GConfig->GetString(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("UseFastIntrinsics"), AllowFastIntrinsics, GEngineIni);
				GConfig->GetBool(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("EnableMathOptimisations"), bEnableMathOptimisations, GEngineIni);
			}
			else
			{
				GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("UseFastIntrinsics"), AllowFastIntrinsics, GEngineIni);
				GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("EnableMathOptimisations"), bEnableMathOptimisations, GEngineIni);
				// Force no development shaders on iOS
				bAllowDevelopmentShaderCompile = false;
			}
			Input.Environment.SetDefine(TEXT("METAL_USE_FAST_INTRINSICS"), *AllowFastIntrinsics);
			
			// Same as console-variable above, but that's global and this is per-platform, per-project
			if (!bEnableMathOptimisations)
			{
				Input.Environment.CompilerFlags.Add(CFLAG_NoFastMath);
			}
		}
	}

	Input.Environment.SetDefine(TEXT("HAS_INVERTED_Z_BUFFER"), (bool)ERHIZBuffer::IsInverted);

	{
		FString ShaderPDBRoot;
		GConfig->GetString(TEXT("DevOptions.Shaders"), TEXT("ShaderPDBRoot"), ShaderPDBRoot, GEngineIni);
		if (!ShaderPDBRoot.IsEmpty())
		{
			Input.Environment.SetDefine(TEXT("SHADER_PDB_ROOT"), *ShaderPDBRoot);
		}
	}

	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ClearCoatNormal"));
		Input.Environment.SetDefine(TEXT("CLEAR_COAT_BOTTOM_NORMAL"), CVar ? (CVar->GetValueOnGameThread() != 0) : 0);
	}

	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("Compat.UseDXT5NormalMaps"));
		Input.Environment.SetDefine(TEXT("DXT5_NORMALMAPS"), CVar ? (CVar->GetValueOnGameThread() != 0) : 0);
	}

	if (bAllowDevelopmentShaderCompile)
	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.CompileShadersForDevelopment"));
		Input.Environment.SetDefine(TEXT("COMPILE_SHADERS_FOR_DEVELOPMENT"), CVar ? (CVar->GetValueOnGameThread() != 0) : 0);
	}

	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
		Input.Environment.SetDefine(TEXT("ALLOW_STATIC_LIGHTING"), CVar ? (CVar->GetValueOnGameThread() != 0) : 1);
	}

	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.BasePassOutputsVelocity"));
		Input.Environment.SetDefine(TEXT("GBUFFER_HAS_VELOCITY"), CVar ? (CVar->GetValueOnGameThread() != 0) : 0);
	}

	{
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SelectiveBasePassOutputs"));
		Input.Environment.SetDefine(TEXT("SELECTIVE_BASEPASS_OUTPUTS"), CVar ? (CVar->GetValueOnGameThread() != 0) : 0);
	}

	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DBuffer"));
		Input.Environment.SetDefine(TEXT("USE_DBUFFER"), CVar ? CVar->GetInt() : 0);
	}

	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AllowGlobalClipPlane"));
		Input.Environment.SetDefine(TEXT("PROJECT_ALLOW_GLOBAL_CLIP_PLANE"), CVar ? (CVar->GetInt() != 0) : 0);
	}

	static IConsoleVariable* CVarForwardShading = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ForwardShading"));
	const bool bForwardShading = CVarForwardShading ? (CVarForwardShading->GetInt() != 0) : false;

	Input.Environment.SetDefine(TEXT("FORWARD_SHADING"), bForwardShading);

	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EarlyZPassOnlyMaterialMasking"));
		Input.Environment.SetDefine(TEXT("EARLY_Z_PASS_ONLY_MATERIAL_MASKING"), CVar ? (CVar->GetInt() != 0) : 0);
	}

	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VertexFoggingForOpaque"));
		Input.Environment.SetDefine(TEXT("VERTEX_FOGGING_FOR_OPAQUE"), bForwardShading && (CVar ? (CVar->GetInt() != 0) : 0));
	}

	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Mobile.DisableVertexFog"));
		Input.Environment.SetDefine(TEXT("PROJECT_MOBILE_DISABLE_VERTEX_FOG"), CVar ? (CVar->GetInt() != 0) : 0);
	}

	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.PostProcessing.PropagateAlpha"));
		Input.Environment.SetDefine(TEXT("POST_PROCESS_ALPHA"), CVar ? (CVar->GetInt() != 0) : 0);
	}

	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DistanceFieldBuild.EightBit"));
		Input.Environment.SetDefine(TEXT("EIGHT_BIT_MESH_DISTANCE_FIELDS"), CVar ? (CVar->GetInt() != 0) : 0);
	}

	if (GSupportsRenderTargetWriteMask)
	{
		Input.Environment.SetDefine(TEXT("PLATFORM_SUPPORTS_RENDERTARGET_WRITE_MASK"), 1);
	}
	else
	{
		Input.Environment.SetDefine(TEXT("PLATFORM_SUPPORTS_RENDERTARGET_WRITE_MASK"), 0);
	}

	NewJobs.Add(NewJob);
}


/** Timer class used to report information on the 'recompileshaders' console command. */
class FRecompileShadersTimer
{
public:
	FRecompileShadersTimer(const TCHAR* InInfoStr=TEXT("Test")) :
		InfoStr(InInfoStr),
		bAlreadyStopped(false)
	{
		StartTime = FPlatformTime::Seconds();
	}

	FRecompileShadersTimer(const FString& InInfoStr) :
		InfoStr(InInfoStr),
		bAlreadyStopped(false)
	{
		StartTime = FPlatformTime::Seconds();
	}

	void Stop(bool DisplayLog = true)
	{
		if (!bAlreadyStopped)
		{
			bAlreadyStopped = true;
			EndTime = FPlatformTime::Seconds();
			TimeElapsed = EndTime-StartTime;
			if (DisplayLog)
			{
				UE_LOG(LogShaderCompilers, Warning, TEXT("		[%s] took [%.4f] s"),*InfoStr,TimeElapsed);
			}
		}
	}

	~FRecompileShadersTimer()
	{
		Stop(true);
	}

protected:
	double StartTime,EndTime;
	double TimeElapsed;
	FString InfoStr;
	bool bAlreadyStopped;
};

class FRecompileShaderMessageHandler : public IPlatformFile::IFileServerMessageHandler
{
public:
	FRecompileShaderMessageHandler( const TCHAR* InCmd ) :
		Cmd( InCmd )
	{
	}

	/** Subclass fills out an archive to send to the server */
	virtual void FillPayload(FArchive& Payload) override
	{
		bool bCompileChangedShaders = true;

		const TCHAR* CmdString = *Cmd;
		FString CmdName = FParse::Token(CmdString, 0);

		if( !CmdName.IsEmpty() && FCString::Stricmp(*CmdName,TEXT("Material"))==0 )
		{
			bCompileChangedShaders = false;

			// tell other side the material to load, by pathname
			FString RequestedMaterialName( FParse::Token( CmdString, 0 ) );

			for( TObjectIterator<UMaterialInterface> It; It; ++It )
			{
				UMaterial* Material = It->GetMaterial();

				if( Material && Material->GetName() == RequestedMaterialName)
				{
					MaterialsToLoad.Add( It->GetPathName() );
					break;
				}
			}
		}
		else
		{
			// tell other side all the materials to load, by pathname
			for( TObjectIterator<UMaterialInterface> It; It; ++It )
			{
				MaterialsToLoad.Add( It->GetPathName() );
			}
		}

		Payload << MaterialsToLoad;
		uint32 ShaderPlatform = ( uint32 )GMaxRHIShaderPlatform;
		Payload << ShaderPlatform;
		// tell the other side the Ids we have so it doesn't send back duplicates
		// (need to serialize this into a TArray since FShaderResourceId isn't known in the file server)
		TArray<FShaderResourceId> AllIds;
		FShaderResource::GetAllShaderResourceId( AllIds );

		TArray<uint8> SerializedBytes;
		FMemoryWriter Ar( SerializedBytes );
		Ar << AllIds;
		Payload << SerializedBytes;
		Payload << bCompileChangedShaders;
	}

	/** Subclass pulls data response from the server */
	virtual void ProcessResponse(FArchive& Response) override
	{
		// pull back the compiled mesh material data (if any)
		TArray<uint8> MeshMaterialMaps;
		Response << MeshMaterialMaps;

		// now we need to refresh the RHI resources
		FlushRenderingCommands();

		// reload the global shaders
		CompileGlobalShaderMap(true);

		//invalidate global bound shader states so they will be created with the new shaders the next time they are set (in SetGlobalBoundShaderState)
		for(TLinkedList<FGlobalBoundShaderStateResource*>::TIterator It(FGlobalBoundShaderStateResource::GetGlobalBoundShaderStateList());It;It.Next())
		{
			BeginUpdateResourceRHI(*It);
		}

		// load all the mesh material shaders if any were sent back
		if (MeshMaterialMaps.Num() > 0)
		{
			// this will stop the rendering thread, and reattach components, in the destructor
			FMaterialUpdateContext UpdateContext;

			// parse the shaders
			FMemoryReader MemoryReader(MeshMaterialMaps, true);
			FNameAsStringProxyArchive Ar(MemoryReader);
			FMaterialShaderMap::LoadForRemoteRecompile(Ar, GMaxRHIShaderPlatform, MaterialsToLoad);

			// gather the shader maps to reattach
			for (TObjectIterator<UMaterial> It; It; ++It)
			{
				UpdateContext.AddMaterial(*It);
			}

			// fixup uniform expressions
			UMaterialInterface::RecacheAllMaterialUniformExpressions();
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND(
			FRecreateBoundShaderStates,
		{
			RHIRecreateRecursiveBoundShaderStates();
		});
	}

private:
	/** The materials we send over the network and expect maps for on the return */
	TArray<FString> MaterialsToLoad;

	/** The recompileshader console command to parse */
	FString Cmd;
};

/**
* Forces a recompile of the global shaders.
*/
void RecompileGlobalShaders()
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		// Flush pending accesses to the existing global shaders.
		FlushRenderingCommands();

		UMaterialInterface::IterateOverActiveFeatureLevels([&](ERHIFeatureLevel::Type InFeatureLevel) {
			auto ShaderPlatform = GShaderPlatformForFeatureLevel[InFeatureLevel];
			GetGlobalShaderMap(ShaderPlatform)->Empty();
			VerifyGlobalShaders(ShaderPlatform, false);
		});

		GShaderCompilingManager->ProcessAsyncResults(false, true);

		//invalidate global bound shader states so they will be created with the new shaders the next time they are set (in SetGlobalBoundShaderState)
		for (TLinkedList<FGlobalBoundShaderStateResource*>::TIterator It(FGlobalBoundShaderStateResource::GetGlobalBoundShaderStateList()); It; It.Next())
		{
			BeginUpdateResourceRHI(*It);
		}
	}
}

bool RecompileShaders(const TCHAR* Cmd, FOutputDevice& Ar)
{
	// if this platform can't compile shaders, then we try to send a message to a file/cooker server
	if (FPlatformProperties::RequiresCookedData())
	{
		FRecompileShaderMessageHandler Handler( Cmd );

		// send the info, the handler will process the response (and update shaders, etc)
		IFileManager::Get().SendMessageToServer(TEXT("RecompileShaders"), &Handler);

		return true;
	}

	FString FlagStr(FParse::Token(Cmd, 0));
	if( FlagStr.Len() > 0 )
	{
		GWarn->BeginSlowTask( NSLOCTEXT("ShaderCompilingManager", "BeginRecompilingShadersTask", "Recompiling shaders"), true );

		// Flush the shader file cache so that any changes to shader source files will be detected
		FlushShaderFileCache();
		FlushRenderingCommands();

		if( FCString::Stricmp(*FlagStr,TEXT("Changed"))==0)
		{
			TArray<FShaderType*> OutdatedShaderTypes;
			TArray<const FVertexFactoryType*> OutdatedFactoryTypes;
			TArray<const FShaderPipelineType*> OutdatedShaderPipelineTypes;
			{
				FRecompileShadersTimer SearchTimer(TEXT("Searching for changed files"));
				FShaderType::GetOutdatedTypes(OutdatedShaderTypes, OutdatedFactoryTypes);
				FShaderPipelineType::GetOutdatedTypes(OutdatedShaderTypes, OutdatedShaderPipelineTypes, OutdatedFactoryTypes);
			}

			if (OutdatedShaderPipelineTypes.Num() > 0 || OutdatedShaderTypes.Num() > 0 || OutdatedFactoryTypes.Num() > 0)
			{
				FRecompileShadersTimer TestTimer(TEXT("RecompileShaders Changed"));

				UMaterialInterface::IterateOverActiveFeatureLevels([&](ERHIFeatureLevel::Type InFeatureLevel) {
					auto ShaderPlatform = GShaderPlatformForFeatureLevel[InFeatureLevel];
					BeginRecompileGlobalShaders(OutdatedShaderTypes, OutdatedShaderPipelineTypes, ShaderPlatform);
				});

				// Block on global shaders
				FinishRecompileGlobalShaders();

				// Kick off global shader recompiles
				UMaterialInterface::IterateOverActiveFeatureLevels([&](ERHIFeatureLevel::Type InFeatureLevel) {
					auto ShaderPlatform = GShaderPlatformForFeatureLevel[InFeatureLevel];
					UMaterial::UpdateMaterialShaders(OutdatedShaderTypes, OutdatedShaderPipelineTypes, OutdatedFactoryTypes, ShaderPlatform);
				});

				GWarn->StatusUpdate(0, 1, NSLOCTEXT("ShaderCompilingManager", "CompilingGlobalShaderStatus", "Compiling global shaders..."));
			}
			else
			{
				UE_LOG(LogShaderCompilers, Warning, TEXT("No Shader changes found."));
			}
		}
		else if( FCString::Stricmp(*FlagStr,TEXT("Global"))==0)
		{
			FRecompileShadersTimer TestTimer(TEXT("RecompileShaders Global"));
			RecompileGlobalShaders();
		}
		else if( FCString::Stricmp(*FlagStr,TEXT("Material"))==0)
		{
			FString RequestedMaterialName(FParse::Token(Cmd, 0));
			FRecompileShadersTimer TestTimer(FString::Printf(TEXT("Recompile Material %s"), *RequestedMaterialName));
			bool bMaterialFound = false;
			for( TObjectIterator<UMaterial> It; It; ++It )
			{
				UMaterial* Material = *It;
				if( Material && Material->GetName() == RequestedMaterialName)
				{
					bMaterialFound = true;
#if WITH_EDITOR
					// <Pre/Post>EditChange will force a re-creation of the resource,
					// in turn recompiling the shader.
					Material->PreEditChange(NULL);
					Material->PostEditChange();
#endif // WITH_EDITOR
					break;
				}
			}

			if (!bMaterialFound)
			{
				TestTimer.Stop(false);
				UE_LOG(LogShaderCompilers, Warning, TEXT("Couldn't find Material %s!"), *RequestedMaterialName);
			}
		}
		else if( FCString::Stricmp(*FlagStr,TEXT("All"))==0)
		{
			FRecompileShadersTimer TestTimer(TEXT("RecompileShaders"));
			RecompileGlobalShaders();

			FMaterialUpdateContext UpdateContext;
			for( TObjectIterator<UMaterial> It; It; ++It )
			{
				UMaterial* Material = *It;
				if( Material )
				{
					UE_LOG(LogShaderCompilers, Log, TEXT("recompiling [%s]"),*Material->GetFullName());
					UpdateContext.AddMaterial(Material);
#if WITH_EDITOR
					// <Pre/Post>EditChange will force a re-creation of the resource,
					// in turn recompiling the shader.
					Material->PreEditChange(NULL);
					Material->PostEditChange();
#endif // WITH_EDITOR
				}
			}
		}
		else
		{
			TArray<FShaderType*> ShaderTypes = FShaderType::GetShaderTypesByFilename(*FlagStr);
			TArray<const FShaderPipelineType*> ShaderPipelineTypes = FShaderPipelineType::GetShaderPipelineTypesByFilename(*FlagStr);
			if (ShaderTypes.Num() > 0 || ShaderPipelineTypes.Num() > 0)
			{
				FRecompileShadersTimer TestTimer(TEXT("RecompileShaders SingleShader"));
				
				TArray<const FVertexFactoryType*> FactoryTypes;

				UMaterialInterface::IterateOverActiveFeatureLevels([&](ERHIFeatureLevel::Type InFeatureLevel) {
					auto ShaderPlatform = GShaderPlatformForFeatureLevel[InFeatureLevel];
					BeginRecompileGlobalShaders(ShaderTypes, ShaderPipelineTypes, ShaderPlatform);
					//UMaterial::UpdateMaterialShaders(ShaderTypes, ShaderPipelineTypes, FactoryTypes, ShaderPlatform);
					FinishRecompileGlobalShaders();
				});
			}
		}

		GWarn->EndSlowTask();

		return 1;
	}

	UE_LOG(LogShaderCompilers, Warning, TEXT("Invalid parameter. Options are: \n'Changed', 'Global', 'Material [name]', 'All' 'Platform [name]'\nNote: Platform implies Changed, and requires the proper target platform modules to be compiled."));
	return 1;
}

FShaderCompileJob* FGlobalShaderTypeCompiler::BeginCompileShader(FGlobalShaderType* ShaderType, EShaderPlatform Platform, const FShaderPipelineType* ShaderPipeline, TArray<FShaderCommonCompileJob*>& NewJobs)
{
	FShaderCompileJob* NewJob = new FShaderCompileJob(GlobalShaderMapId, nullptr, ShaderType);
	FShaderCompilerEnvironment& ShaderEnvironment = NewJob->Input.Environment;

	UE_LOG(LogShaders, Verbose, TEXT("	%s"), ShaderType->GetName());
	COOK_STAT(GlobalShaderCookStats::ShadersCompiled++);

	// Allow the shader type to modify the compile environment.
	ShaderType->SetupCompileEnvironment(Platform, ShaderEnvironment);

	static FString GlobalName(TEXT("Global"));

	// Compile the shader environment passed in with the shader type's source code.
	::GlobalBeginCompileShader(
		GlobalName,
		nullptr,
		ShaderType,
		ShaderPipeline,
		ShaderType->GetShaderFilename(),
		ShaderType->GetFunctionName(),
		FShaderTarget(ShaderType->GetFrequency(), Platform),
		NewJob,
		NewJobs
	);

	return NewJob;
}

void FGlobalShaderTypeCompiler::BeginCompileShaderPipeline(EShaderPlatform Platform, const FShaderPipelineType* ShaderPipeline, const TArray<FGlobalShaderType*>& ShaderStages, TArray<FShaderCommonCompileJob*>& NewJobs)
{
	check(ShaderStages.Num() > 0);
	check(ShaderPipeline);
	UE_LOG(LogShaders, Verbose, TEXT("	Pipeline: %s"), ShaderPipeline->GetName());

	// Add all the jobs as individual first, then add the dependencies into a pipeline job
	auto* NewPipelineJob = new FShaderPipelineCompileJob(GlobalShaderMapId, ShaderPipeline, ShaderStages.Num());
	for (int32 Index = 0; Index < ShaderStages.Num(); ++Index)
	{
		auto* ShaderStage = ShaderStages[Index];
		BeginCompileShader(ShaderStage, Platform, ShaderPipeline, NewPipelineJob->StageJobs);
	}

	NewJobs.Add(NewPipelineJob);
}

FShader* FGlobalShaderTypeCompiler::FinishCompileShader(FGlobalShaderType* ShaderType, const FShaderCompileJob& CurrentJob, const FShaderPipelineType* ShaderPipelineType)
{
	FShader* Shader = nullptr;
	if (CurrentJob.bSucceeded)
	{
		FShaderType* SpecificType = CurrentJob.ShaderType->LimitShaderResourceToThisType() ? CurrentJob.ShaderType : nullptr;

		// Reuse an existing resource with the same key or create a new one based on the compile output
		// This allows FShaders to share compiled bytecode and RHI shader references
		FShaderResource* Resource = FShaderResource::FindOrCreateShaderResource(CurrentJob.Output, SpecificType);
		check(Resource);

		if (ShaderPipelineType && !ShaderPipelineType->ShouldOptimizeUnusedOutputs())
		{
			// If sharing shaders in this pipeline, remove it from the type/id so it uses the one in the shared shadermap list
			ShaderPipelineType = nullptr;
		}

		// Find a shader with the same key in memory
		Shader = CurrentJob.ShaderType->FindShaderById(FShaderId(GGlobalShaderMapHash, ShaderPipelineType, nullptr, CurrentJob.ShaderType, CurrentJob.Input.Target));

		// There was no shader with the same key so create a new one with the compile output, which will bind shader parameters
		if (!Shader)
		{
			Shader = (*(ShaderType->ConstructCompiledRef))(FGlobalShaderType::CompiledShaderInitializerType(ShaderType, CurrentJob.Output, Resource, GGlobalShaderMapHash, ShaderPipelineType, nullptr));
			CurrentJob.Output.ParameterMap.VerifyBindingsAreComplete(ShaderType->GetName(), CurrentJob.Output.Target, CurrentJob.VFType);
		}
	}

	if (CVarShowShaderWarnings->GetInt() && CurrentJob.Output.Errors.Num() > 0)
	{
		UE_LOG(LogShaderCompilers, Warning, TEXT("Warnings compiling global shader %s %s %s:\n"), CurrentJob.ShaderType->GetName(), ShaderPipelineType ? TEXT("ShaderPipeline") : TEXT(""), ShaderPipelineType ? ShaderPipelineType->GetName() : TEXT(""));
		for (int32 ErrorIndex = 0; ErrorIndex < CurrentJob.Output.Errors.Num(); ErrorIndex++)
		{
			UE_LOG(LogShaderCompilers, Warning, TEXT("	%s"), *CurrentJob.Output.Errors[ErrorIndex].GetErrorString());
		}
	}

	return Shader;
}

/**
* Makes sure all global shaders are loaded and/or compiled for the passed in platform.
* Note: if compilation is needed, this only kicks off the compile.
*
* @param	Platform	Platform to verify global shaders for
*/
void VerifyGlobalShaders(EShaderPlatform Platform, bool bLoadedFromCacheFile)
{
	check(IsInGameThread());
	check(!FPlatformProperties::IsServerOnly());
	check(GGlobalShaderMap[Platform]);

	UE_LOG(LogMaterial, Log, TEXT("Verifying Global Shaders for %s"), *LegacyShaderPlatformToShaderFormat(Platform).ToString());

	// Ensure that the global shader map contains all global shader types.
	TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(Platform);
	const bool bEmptyMap = GlobalShaderMap->IsEmpty();
	if (bEmptyMap)
	{
		UE_LOG(LogShaders, Warning, TEXT("	Empty global shader map, recompiling all global shaders"));
	}

	bool bErrorOnMissing = bLoadedFromCacheFile;
	if (FPlatformProperties::RequiresCookedData())
	{
		// We require all shaders to exist on cooked platforms because we can't compile them.
		bErrorOnMissing = true;
	}

	// All jobs, single & pipeline
	TArray<FShaderCommonCompileJob*> GlobalShaderJobs;

	// Add the single jobs first
	TMap<FShaderType*, FShaderCompileJob*> SharedShaderJobs;
	for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList()); ShaderTypeIt; ShaderTypeIt.Next())
	{
		FGlobalShaderType* GlobalShaderType = ShaderTypeIt->GetGlobalShaderType();
		if (GlobalShaderType && GlobalShaderType->ShouldCache(Platform))
		{
			if (!GlobalShaderMap->HasShader(GlobalShaderType))
			{
				if (bErrorOnMissing)
				{
					UE_LOG(LogShaders, Fatal, TEXT("Missing global shader %s, Please make sure cooking was successful."), GlobalShaderType->GetName());
				}

				if (!bEmptyMap)
				{
					UE_LOG(LogShaders, Warning, TEXT("	%s"), GlobalShaderType->GetName());
				}

				// Compile this global shader type.
				auto* Job = FGlobalShaderTypeCompiler::BeginCompileShader(GlobalShaderType, Platform, nullptr, GlobalShaderJobs);
				check(!SharedShaderJobs.Find(GlobalShaderType));
				SharedShaderJobs.Add(GlobalShaderType, Job);
			}
		}
	}

	// Now the pipeline jobs; if it's a shareable pipeline, do not add duplicate jobs
	for (TLinkedList<FShaderPipelineType*>::TIterator ShaderPipelineIt(FShaderPipelineType::GetTypeList()); ShaderPipelineIt; ShaderPipelineIt.Next())
	{
		const FShaderPipelineType* Pipeline = *ShaderPipelineIt;
		if (Pipeline->IsGlobalTypePipeline())
		{
			if (!GlobalShaderMap->GetShaderPipeline(Pipeline))
			{
				auto& StageTypes = Pipeline->GetStages();
				TArray<FGlobalShaderType*> ShaderStages;
				for (int32 Index = 0; Index < StageTypes.Num(); ++Index)
				{
					FGlobalShaderType* GlobalShaderType = ((FShaderType*)(StageTypes[Index]))->GetGlobalShaderType();
					if (GlobalShaderType->ShouldCache(Platform))
					{
						ShaderStages.Add(GlobalShaderType);
					}
					else
					{
						break;
					}
				}

				if (ShaderStages.Num() == StageTypes.Num())
				{
					if (bErrorOnMissing)
					{
						UE_LOG(LogShaders, Fatal, TEXT("Missing global shader pipeline %s, Please make sure cooking was successful."), Pipeline->GetName());
					}

					if (!bEmptyMap)
					{
						UE_LOG(LogShaders, Warning, TEXT("	%s"), Pipeline->GetName());
					}

					if (Pipeline->ShouldOptimizeUnusedOutputs())
					{
						// Make a pipeline job with all the stages
						FGlobalShaderTypeCompiler::BeginCompileShaderPipeline(Platform, Pipeline, ShaderStages, GlobalShaderJobs);
					}
					else
					{
						// If sharing shaders amongst pipelines, add this pipeline as a dependency of an existing individual job
						for (const FShaderType* ShaderType : StageTypes)
						{
							FShaderCompileJob** Job = SharedShaderJobs.Find(ShaderType);
							checkf(Job, TEXT("Couldn't find existing shared job for global shader %s on pipeline %s!"), ShaderType->GetName(), Pipeline->GetName());
							auto* SingleJob = (*Job)->GetSingleShaderJob();
							check(SingleJob);
							auto& SharedPipelinesInJob = SingleJob->SharingPipelines.FindOrAdd(nullptr);
							check(!SharedPipelinesInJob.Contains(Pipeline));
							SharedPipelinesInJob.Add(Pipeline);
						}
					}
				}
			}
		}
	}

	if (GlobalShaderJobs.Num() > 0)
	{
		GShaderCompilingManager->AddJobs(GlobalShaderJobs, true, true, false);

		const bool bAllowAsynchronousGlobalShaderCompiling =
			// OpenGL requires that global shader maps are compiled before attaching
			// primitives to the scene as it must be able to find FNULLPS.
			// TODO_OPENGL: Allow shaders to be compiled asynchronously.
			// Metal also needs this when using RHI thread because it uses TOneColorVS very early in RHIPostInit()
			!IsOpenGLPlatform(GMaxRHIShaderPlatform) && !IsVulkanPlatform(GMaxRHIShaderPlatform) &&
			!IsMetalPlatform(GMaxRHIShaderPlatform) &&
			GShaderCompilingManager->AllowAsynchronousShaderCompiling();

		if (!bAllowAsynchronousGlobalShaderCompiling)
		{
			TArray<int32> ShaderMapIds;
			ShaderMapIds.Add(GlobalShaderMapId);

			GShaderCompilingManager->FinishCompilation(TEXT("Global"), ShaderMapIds);
		}
	}
}

static FString GetGlobalShaderCacheFilename(EShaderPlatform Platform)
{
	return FString(TEXT("Engine")) / TEXT("GlobalShaderCache-") + LegacyShaderPlatformToShaderFormat(Platform).ToString() + TEXT(".bin");
}

/** Creates a string key for the derived data cache entry for the global shader map. */
FString GetGlobalShaderMapKeyString(const FGlobalShaderMapId& ShaderMapId, EShaderPlatform Platform)
{
	FName Format = LegacyShaderPlatformToShaderFormat(Platform);
	FString ShaderMapKeyString = Format.ToString() + TEXT("_") + FString(FString::FromInt(GetTargetPlatformManagerRef().ShaderFormatVersion(Format))) + TEXT("_");
	ShaderMapAppendKeyString(Platform, ShaderMapKeyString);
	ShaderMapId.AppendKeyString(ShaderMapKeyString);
	return FDerivedDataCacheInterface::BuildCacheKey(TEXT("GSM"), GLOBALSHADERMAP_DERIVEDDATA_VER, *ShaderMapKeyString);
}

/** Serializes the global shader map to an archive. */
static void SerializeGlobalShaders(FArchive& Ar, TShaderMap<FGlobalShaderType>* GlobalShaderMap)
{
	check(IsInGameThread());

	// Serialize the global shader map binary file tag.
	static const uint32 ReferenceTag = 0x47534D42;
	if (Ar.IsLoading())
	{
		// Initialize Tag to 0 as it won't be written to if the serialize fails (ie the global shader cache file is empty)
		uint32 Tag = 0;
		Ar << Tag;
		checkf(Tag == ReferenceTag, TEXT("Global shader map binary file is missing GSMB tag."));
	}
	else
	{
		uint32 Tag = ReferenceTag;
		Ar << Tag;
	}

	// Serialize the global shaders.
	GlobalShaderMap->SerializeInline(Ar, true, false);
	// And now register them.
	GlobalShaderMap->RegisterSerializedShaders();
}

/** Saves the platform's shader map to the DDC. */
void SaveGlobalShaderMapToDerivedDataCache(EShaderPlatform Platform)
{
	// We've finally built the global shader map, so we can count the miss as we put it in the DDC.
	COOK_STAT(auto Timer = GlobalShaderCookStats::UsageStats.TimeSyncWork());
	TArray<uint8> SaveData;
	FMemoryWriter Ar(SaveData, true);
	SerializeGlobalShaders(Ar, GGlobalShaderMap[Platform]);

	FGlobalShaderMapId ShaderMapId(Platform);
	GetDerivedDataCacheRef().Put(*GetGlobalShaderMapKeyString(ShaderMapId, Platform), SaveData);
	COOK_STAT(Timer.AddMiss(SaveData.Num()));
}

/** Saves the global shader map as a file for the target platform. */
FString SaveGlobalShaderFile(EShaderPlatform Platform, FString SavePath, class ITargetPlatform* TargetPlatform)
{
	TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(Platform);

	// Wait until all global shaders are compiled
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->ProcessAsyncResults(false, true);
	}

	TArray<uint8> GlobalShaderData;
	FMemoryWriter MemoryWriter(GlobalShaderData, true);
	if (TargetPlatform)
	{
		MemoryWriter.SetCookingTarget(TargetPlatform);
	}
	SerializeGlobalShaders(MemoryWriter, GlobalShaderMap);

	// make the final name
	FString FullPath = SavePath / GetGlobalShaderCacheFilename(Platform);
	if (!FFileHelper::SaveArrayToFile(GlobalShaderData, *FullPath))
	{
		UE_LOG(LogShaders, Fatal, TEXT("Could not save global shader file to '%s'"), *FullPath);
	}

	return FullPath;
}


static inline bool ShouldCacheGlobalShaderTypeName(const FGlobalShaderType* GlobalShaderType, const TCHAR* TypeNameSubstring, EShaderPlatform Platform)
{
	return GlobalShaderType
		&& (TypeNameSubstring == nullptr || (FPlatformString::Strstr(GlobalShaderType->GetName(), TypeNameSubstring) != nullptr))
		&& GlobalShaderType->ShouldCache(Platform);
};


bool IsGlobalShaderMapComplete(const TCHAR* TypeNameSubstring)
{
	for (int32 i = 0; i < SP_NumPlatforms; ++i)
	{
		EShaderPlatform Platform = (EShaderPlatform)i;

		TShaderMap<FGlobalShaderType>* GlobalShaderMap = GGlobalShaderMap[Platform];

		if (GlobalShaderMap)
		{
			// Check if the individual shaders are complete
			for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList()); ShaderTypeIt; ShaderTypeIt.Next())
			{
				FGlobalShaderType* GlobalShaderType = ShaderTypeIt->GetGlobalShaderType();
				if (ShouldCacheGlobalShaderTypeName(GlobalShaderType, TypeNameSubstring, Platform))
				{
					if (!GlobalShaderMap->HasShader(GlobalShaderType))
					{
						return false;
					}
				}
			}

			// Then the pipelines as it may be sharing shaders
			for (TLinkedList<FShaderPipelineType*>::TIterator ShaderPipelineIt(FShaderPipelineType::GetTypeList()); ShaderPipelineIt; ShaderPipelineIt.Next())
			{
				const FShaderPipelineType* Pipeline = *ShaderPipelineIt;
				if (Pipeline->IsGlobalTypePipeline())
				{
					auto& Stages = Pipeline->GetStages();
					int32 NumStagesNeeded = 0;
					for (const FShaderType* Shader : Stages)
					{
						const FGlobalShaderType* GlobalShaderType = Shader->GetGlobalShaderType();
						if (ShouldCacheGlobalShaderTypeName(GlobalShaderType, TypeNameSubstring, Platform))
						{
							++NumStagesNeeded;
						}
						else
						{
							break;
						}
					}

					if (NumStagesNeeded == Stages.Num())
					{
						if (!GlobalShaderMap->GetShaderPipeline(Pipeline))
						{
							return false;
						}
					}
				}
			}
		}
	}

	return true;
}

void CompileGlobalShaderMap(EShaderPlatform Platform, bool bRefreshShaderMap)
{
	// No global shaders needed on dedicated server or clients that use NullRHI. Note that cook commandlet needs to have them, even if it is not allowed to render otherwise.
	if (FPlatformProperties::IsServerOnly() || (!IsRunningCommandlet() && !FApp::CanEverRender()))
	{
		if (!GGlobalShaderMap[Platform])
		{
			GGlobalShaderMap[Platform] = new TShaderMap<FGlobalShaderType>(Platform);
		}
		return;
	}

	if (bRefreshShaderMap)
	{
		// delete the current global shader map
		delete GGlobalShaderMap[Platform];
		GGlobalShaderMap[Platform] = nullptr;

		// make sure we look for updated shader source files
		FlushShaderFileCache();
	}

	// If the global shader map hasn't been created yet, create it.
	if (!GGlobalShaderMap[Platform])
	{
		DECLARE_SCOPE_CYCLE_COUNTER(TEXT("GetGlobalShaderMap"), STAT_GetGlobalShaderMap, STATGROUP_LoadTime);
		// GetGlobalShaderMap is called the first time during startup in the main thread.
		check(IsInGameThread());

		FScopedSlowTask SlowTask(70);

		// verify that all shader source files are intact
		SlowTask.EnterProgressFrame(20);
		VerifyShaderSourceFiles();

		GGlobalShaderMap[Platform] = new TShaderMap<FGlobalShaderType>(Platform);

		bool bLoadedFromCacheFile = false;

		// Try to load the global shaders from a local cache file if it exists
		// This method is used exclusively with cooked content, since the DDC is not present
		if (FPlatformProperties::RequiresCookedData())
		{
			SlowTask.EnterProgressFrame(50);

			TArray<uint8> GlobalShaderData;
			FString GlobalShaderCacheFilename = FPaths::GetRelativePathToRoot() / GetGlobalShaderCacheFilename(Platform);
			FPaths::MakeStandardFilename(GlobalShaderCacheFilename);
			bLoadedFromCacheFile = FFileHelper::LoadFileToArray(GlobalShaderData, *GlobalShaderCacheFilename, FILEREAD_Silent);

			if (!bLoadedFromCacheFile)
			{
				// Handle this gracefully and exit.
				FString SandboxPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*GlobalShaderCacheFilename);
				// This can be too early to localize in some situations.
				const FText Message = FText::Format(NSLOCTEXT("Engine", "GlobalShaderCacheFileMissing", "The global shader cache file '{0}' is missing.\n\nYour application is built to load COOKED content. No COOKED content was found; This usually means you did not cook content for this build.\nIt also may indicate missing cooked data for a shader platform(e.g., OpenGL under Windows): Make sure your platform's packaging settings include this Targeted RHI.\n\nAlternatively build and run the UNCOOKED version instead."), FText::FromString(SandboxPath));
				if (FPlatformProperties::SupportsWindowedMode())
				{
					UE_LOG(LogShaders, Error, TEXT("%s"), *Message.ToString());
					FMessageDialog::Open(EAppMsgType::Ok, Message);
					FPlatformMisc::RequestExit(false);
					return;
				}
				else
				{
					UE_LOG(LogShaders, Fatal, TEXT("%s"), *Message.ToString());
				}
			}

			FMemoryReader MemoryReader(GlobalShaderData);
			SerializeGlobalShaders(MemoryReader, GGlobalShaderMap[Platform]);
		}
		// Uncooked platform
		else
		{
			FGlobalShaderMapId ShaderMapId(Platform);

			TArray<uint8> CachedData;
			SlowTask.EnterProgressFrame(40);
			const FString DataKey = GetGlobalShaderMapKeyString(ShaderMapId, Platform);

			// Find the shader map in the derived data cache
			SlowTask.EnterProgressFrame(10);

			COOK_STAT(auto Timer = GlobalShaderCookStats::UsageStats.TimeSyncWork());
			if (GetDerivedDataCacheRef().GetSynchronous(*DataKey, CachedData))
			{
				COOK_STAT(Timer.AddHit(CachedData.Num()));
				FMemoryReader Ar(CachedData, true);

				// Deserialize from the cached data
				SerializeGlobalShaders(Ar, GGlobalShaderMap[Platform]);
			}
			else
			{
				// it's a miss, but we haven't built anything yet. Save the counting until we actually have it built.
				COOK_STAT(Timer.TrackCyclesOnly());
			}
		}

		// If any shaders weren't loaded, compile them now.
		VerifyGlobalShaders(Platform, bLoadedFromCacheFile);

		if (GCreateShadersOnLoad && Platform == GMaxRHIShaderPlatform)
		{
			for (auto& Pair : GGlobalShaderMap[Platform]->GetShaders())
			{
				FShader* Shader = Pair.Value;
				if (Shader)
				{
					Shader->BeginInitializeResources();
				}
			}
		}
	}
}

void CompileGlobalShaderMap(ERHIFeatureLevel::Type InFeatureLevel, bool bRefreshShaderMap)
{
	EShaderPlatform Platform = GShaderPlatformForFeatureLevel[InFeatureLevel];
	CompileGlobalShaderMap(Platform, bRefreshShaderMap);
}

void CompileGlobalShaderMap(bool bRefreshShaderMap)
{
	CompileGlobalShaderMap(GMaxRHIFeatureLevel, bRefreshShaderMap);
}

bool RecompileChangedShadersForPlatform(const FString& PlatformName)
{
	// figure out what shader platforms to recompile
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	ITargetPlatform* TargetPlatform = TPM->FindTargetPlatform(PlatformName);
	if (TargetPlatform == NULL)
	{
		UE_LOG(LogShaders, Display, TEXT("Failed to find target platform module for %s"), *PlatformName);
		return false;
	}

	TArray<FName> DesiredShaderFormats;
	TargetPlatform->GetAllTargetedShaderFormats(DesiredShaderFormats);



	// figure out which shaders are out of date
	TArray<FShaderType*> OutdatedShaderTypes;
	TArray<const FVertexFactoryType*> OutdatedFactoryTypes;
	TArray<const FShaderPipelineType*> OutdatedShaderPipelineTypes;

	// Pick up new changes to shader files
	FlushShaderFileCache();

	FShaderType::GetOutdatedTypes(OutdatedShaderTypes, OutdatedFactoryTypes);
	FShaderPipelineType::GetOutdatedTypes(OutdatedShaderTypes, OutdatedShaderPipelineTypes, OutdatedFactoryTypes);
	UE_LOG(LogShaders, Display, TEXT("We found %d out of date shader types, %d outdated pipeline types, and %d out of date VF types!"), OutdatedShaderTypes.Num(), OutdatedShaderPipelineTypes.Num(), OutdatedFactoryTypes.Num());

	for (int32 FormatIndex = 0; FormatIndex < DesiredShaderFormats.Num(); FormatIndex++)
	{
		// get the shader platform enum
		const EShaderPlatform ShaderPlatform = ShaderFormatToLegacyShaderPlatform(DesiredShaderFormats[FormatIndex]);

		// Only compile for the desired platform if requested
		// Kick off global shader recompiles
		BeginRecompileGlobalShaders(OutdatedShaderTypes, OutdatedShaderPipelineTypes, ShaderPlatform);

		// Block on global shaders
		FinishRecompileGlobalShaders();
#if WITH_EDITOR
		// we only want to actually compile mesh shaders if we have out of date ones
		if (OutdatedShaderTypes.Num() || OutdatedFactoryTypes.Num())
		{
			for (TObjectIterator<UMaterialInterface> It; It; ++It)
			{
				(*It)->ClearCachedCookedPlatformData(TargetPlatform);
			}
		}
#endif
	}

	if (OutdatedFactoryTypes.Num() || OutdatedShaderTypes.Num())
	{
		return true;
	}
	return false;
}


void RecompileShadersForRemote(
	const FString& PlatformName,
	EShaderPlatform ShaderPlatformToCompile,
	const FString& OutputDirectory,
	const TArray<FString>& MaterialsToLoad,
	const TArray<uint8>& SerializedShaderResources,
	TArray<uint8>* MeshMaterialMaps,
	TArray<FString>* ModifiedFiles,
	bool bCompileChangedShaders)
{
	// figure out what shader platforms to recompile
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	ITargetPlatform* TargetPlatform = TPM->FindTargetPlatform(PlatformName);
	if (TargetPlatform == NULL)
	{
		UE_LOG(LogShaders, Display, TEXT("Failed to find target platform module for %s"), *PlatformName);
		return;
	}

	TArray<FName> DesiredShaderFormats;
	TargetPlatform->GetAllTargetedShaderFormats(DesiredShaderFormats);

	UE_LOG(LogShaders, Display, TEXT("Loading %d materials..."), MaterialsToLoad.Num());
	// make sure all materials the client has loaded will be processed
	TArray<UMaterialInterface*> MaterialsToCompile;

	for (int32 Index = 0; Index < MaterialsToLoad.Num(); Index++)
	{
		UE_LOG(LogShaders, Display, TEXT("   --> %s"), *MaterialsToLoad[Index]);
		MaterialsToCompile.Add(LoadObject<UMaterialInterface>(NULL, *MaterialsToLoad[Index]));
	}

	UE_LOG(LogShaders, Display, TEXT("  Done!"))

		// figure out which shaders are out of date
		TArray<FShaderType*> OutdatedShaderTypes;
	TArray<const FVertexFactoryType*> OutdatedFactoryTypes;
	TArray<const FShaderPipelineType*> OutdatedShaderPipelineTypes;

	// Pick up new changes to shader files
	FlushShaderFileCache();

	if (bCompileChangedShaders)
	{
		FShaderType::GetOutdatedTypes(OutdatedShaderTypes, OutdatedFactoryTypes);
		FShaderPipelineType::GetOutdatedTypes(OutdatedShaderTypes, OutdatedShaderPipelineTypes, OutdatedFactoryTypes);
		UE_LOG(LogShaders, Display, TEXT("We found %d out of date shader types, %d outdated pipeline types, and %d out of date VF types!"), OutdatedShaderTypes.Num(), OutdatedShaderPipelineTypes.Num(), OutdatedFactoryTypes.Num());
	}

	{
		for (int32 FormatIndex = 0; FormatIndex < DesiredShaderFormats.Num(); FormatIndex++)
		{
			// get the shader platform enum
			const EShaderPlatform ShaderPlatform = ShaderFormatToLegacyShaderPlatform(DesiredShaderFormats[FormatIndex]);

			// Only compile for the desired platform if requested
			if (ShaderPlatform == ShaderPlatformToCompile || ShaderPlatformToCompile == SP_NumPlatforms)
			{
				if (bCompileChangedShaders)
				{
					// Kick off global shader recompiles
					BeginRecompileGlobalShaders(OutdatedShaderTypes, OutdatedShaderPipelineTypes, ShaderPlatform);

					// Block on global shaders
					FinishRecompileGlobalShaders();
				}

				// we only want to actually compile mesh shaders if a client directly requested it, and there's actually some work to do
				if (MeshMaterialMaps != NULL && (OutdatedShaderTypes.Num() || OutdatedFactoryTypes.Num() || bCompileChangedShaders == false))
				{
					TMap<FString, TArray<TRefCountPtr<FMaterialShaderMap> > > CompiledShaderMaps;
					UMaterial::CompileMaterialsForRemoteRecompile(MaterialsToCompile, ShaderPlatform, CompiledShaderMaps);

					// write the shader compilation info to memory, converting fnames to strings
					FMemoryWriter MemWriter(*MeshMaterialMaps, true);
					FNameAsStringProxyArchive Ar(MemWriter);

					// pull the serialized resource ids into an array of resources
					TArray<FShaderResourceId> ClientResourceIds;
					FMemoryReader MemReader(SerializedShaderResources, true);
					MemReader << ClientResourceIds;

					// save out the shader map to the byte array
					FMaterialShaderMap::SaveForRemoteRecompile(Ar, CompiledShaderMaps, ClientResourceIds);
				}

				// save it out so the client can get it (and it's up to date next time)
				FString GlobalShaderFilename = SaveGlobalShaderFile(ShaderPlatform, OutputDirectory, TargetPlatform);

				// add this to the list of files to tell the other end about
				if (ModifiedFiles)
				{
					// need to put it in non-sandbox terms
					FString SandboxPath(GlobalShaderFilename);
					check(SandboxPath.StartsWith(OutputDirectory));
					SandboxPath.ReplaceInline(*OutputDirectory, TEXT("../../../"));
					FPaths::NormalizeFilename(SandboxPath);
					ModifiedFiles->Add(SandboxPath);
				}
			}
		}
	}
}

void BeginRecompileGlobalShaders(const TArray<FShaderType*>& OutdatedShaderTypes, const TArray<const FShaderPipelineType*>& OutdatedShaderPipelineTypes, EShaderPlatform ShaderPlatform)
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		// Flush pending accesses to the existing global shaders.
		FlushRenderingCommands();

		// Calling CompileGlobalShaderMap will force starting the compile jobs if the map is empty (by calling VerifyGlobalShaders)
		CompileGlobalShaderMap(ShaderPlatform, false);
		TShaderMap<FGlobalShaderType>* GlobalShaderMap = GetGlobalShaderMap(ShaderPlatform);

		// Now check if there is any work to be done wrt outdates types
		if (OutdatedShaderTypes.Num() > 0 || OutdatedShaderPipelineTypes.Num() > 0)
		{
			for (int32 TypeIndex = 0; TypeIndex < OutdatedShaderTypes.Num(); TypeIndex++)
			{
				FGlobalShaderType* CurrentGlobalShaderType = OutdatedShaderTypes[TypeIndex]->GetGlobalShaderType();
				if (CurrentGlobalShaderType)
				{
					UE_LOG(LogShaders, Log, TEXT("Flushing Global Shader %s"), CurrentGlobalShaderType->GetName());
					GlobalShaderMap->RemoveShaderType(CurrentGlobalShaderType);
				}
			}

			for (int32 PipelineTypeIndex = 0; PipelineTypeIndex < OutdatedShaderPipelineTypes.Num(); ++PipelineTypeIndex)
			{
				const FShaderPipelineType* ShaderPipelineType = OutdatedShaderPipelineTypes[PipelineTypeIndex];
				if (ShaderPipelineType->IsGlobalTypePipeline())
				{
					UE_LOG(LogShaders, Log, TEXT("Flushing Global Shader Pipeline %s"), ShaderPipelineType->GetName());
					GlobalShaderMap->RemoveShaderPipelineType(ShaderPipelineType);
				}
			}

			//invalidate global bound shader states so they will be created with the new shaders the next time they are set (in SetGlobalBoundShaderState)
			for (TLinkedList<FGlobalBoundShaderStateResource*>::TIterator It(FGlobalBoundShaderStateResource::GetGlobalBoundShaderStateList()); It; It.Next())
			{
				BeginUpdateResourceRHI(*It);
			}

			VerifyGlobalShaders(ShaderPlatform, false);
		}
	}
}

void FinishRecompileGlobalShaders()
{
	// Block until global shaders have been compiled and processed
	GShaderCompilingManager->ProcessAsyncResults(false, true);
}

static inline FShader* ProcessCompiledJob(FShaderCompileJob* SingleJob, const FShaderPipelineType* Pipeline, TArray<EShaderPlatform>& ShaderPlatformsProcessed, TArray<const FShaderPipelineType*>& OutSharedPipelines)
{
	FGlobalShaderType* GlobalShaderType = SingleJob->ShaderType->GetGlobalShaderType();
	check(GlobalShaderType);
	FShader* Shader = FGlobalShaderTypeCompiler::FinishCompileShader(GlobalShaderType, *SingleJob, Pipeline);
	if (Shader)
	{
		// Add the new global shader instance to the global shader map if it's a shared shader
		EShaderPlatform Platform = (EShaderPlatform)SingleJob->Input.Target.Platform;
		if (!Pipeline || !Pipeline->ShouldOptimizeUnusedOutputs())
		{
			GGlobalShaderMap[Platform]->AddShader(GlobalShaderType, Shader);
			// Add this shared pipeline to the list
			if (!Pipeline)
			{
				auto* JobSharedPipelines = SingleJob->SharingPipelines.Find(nullptr);
				if (JobSharedPipelines)
				{
					for (auto* SharedPipeline : *JobSharedPipelines)
					{
						OutSharedPipelines.AddUnique(SharedPipeline);
					}
				}
			}
		}
		ShaderPlatformsProcessed.AddUnique(Platform);
	}
	else
	{
		UE_LOG(LogShaders, Fatal, TEXT("Failed to compile global shader %s %s %s.  Enable 'r.ShaderDevelopmentMode' in ConsoleVariables.ini for retries."),
			GlobalShaderType->GetName(),
			Pipeline ? TEXT("for pipeline") : TEXT(""),
			Pipeline ? Pipeline->GetName() : TEXT(""));
	}

	return Shader;
};

void ProcessCompiledGlobalShaders(const TArray<FShaderCommonCompileJob*>& CompilationResults)
{
	UE_LOG(LogShaders, Warning, TEXT("Compiled %u global shaders"), CompilationResults.Num());

	TArray<EShaderPlatform> ShaderPlatformsProcessed;
	TArray<const FShaderPipelineType*> SharedPipelines;

	for (int32 ResultIndex = 0; ResultIndex < CompilationResults.Num(); ResultIndex++)
	{
		const FShaderCommonCompileJob& CurrentJob = *CompilationResults[ResultIndex];
		FShaderCompileJob* SingleJob = nullptr;
		if ((SingleJob = (FShaderCompileJob*)CurrentJob.GetSingleShaderJob()) != nullptr)
		{
			ProcessCompiledJob(SingleJob, nullptr, ShaderPlatformsProcessed, SharedPipelines);
		}
		else
		{
			const auto* PipelineJob = CurrentJob.GetShaderPipelineJob();
			check(PipelineJob);
			TArray<FShader*> ShaderStages;
			for (int32 Index = 0; Index < PipelineJob->StageJobs.Num(); ++Index)
			{
				SingleJob = PipelineJob->StageJobs[Index]->GetSingleShaderJob();
				FShader* Shader = ProcessCompiledJob(SingleJob, PipelineJob->ShaderPipeline, ShaderPlatformsProcessed, SharedPipelines);
				ShaderStages.Add(Shader);
			}

			FShaderPipeline* ShaderPipeline = new FShaderPipeline(PipelineJob->ShaderPipeline, ShaderStages);
			if (ShaderPipeline)
			{
				EShaderPlatform Platform = (EShaderPlatform)PipelineJob->StageJobs[0]->GetSingleShaderJob()->Input.Target.Platform;
				check(ShaderPipeline && !GGlobalShaderMap[Platform]->HasShaderPipeline(ShaderPipeline->PipelineType));
				GGlobalShaderMap[Platform]->AddShaderPipeline(PipelineJob->ShaderPipeline, ShaderPipeline);
			}
		}
	}

	for (int32 PlatformIndex = 0; PlatformIndex < ShaderPlatformsProcessed.Num(); PlatformIndex++)
	{
		{
			// Process the shader pipelines that share shaders
			EShaderPlatform Platform = ShaderPlatformsProcessed[PlatformIndex];
			auto* GlobalShaderMap = GGlobalShaderMap[Platform];
			for (const FShaderPipelineType* ShaderPipelineType : SharedPipelines)
			{
				check(ShaderPipelineType->IsGlobalTypePipeline());
				if (!GlobalShaderMap->HasShaderPipeline(ShaderPipelineType))
				{
					auto& StageTypes = ShaderPipelineType->GetStages();
					TArray<FShader*> ShaderStages;
					for (int32 Index = 0; Index < StageTypes.Num(); ++Index)
					{
						FGlobalShaderType* GlobalShaderType = ((FShaderType*)(StageTypes[Index]))->GetGlobalShaderType();
						if (GlobalShaderType->ShouldCache(Platform))
						{
							FShader* Shader = GlobalShaderMap->GetShader(GlobalShaderType);
							check(Shader);
							ShaderStages.Add(Shader);
						}
						else
						{
							break;
						}
					}

					checkf(StageTypes.Num() == ShaderStages.Num(), TEXT("Internal Error adding Global ShaderPipeline %s"), ShaderPipelineType->GetName());
					FShaderPipeline* ShaderPipeline = new FShaderPipeline(ShaderPipelineType, ShaderStages);
					GlobalShaderMap->AddShaderPipeline(ShaderPipelineType, ShaderPipeline);
				}
			}
		}

		// Save the global shader map for any platforms that were recompiled
		SaveGlobalShaderMapToDerivedDataCache(ShaderPlatformsProcessed[PlatformIndex]);
	}
}
#undef LOCTEXT_NAMESPACE
