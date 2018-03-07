// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BuildPatchTool.h"
#include "UObject/Object.h"
#include "RequiredProgramMainCPPInclude.h"
#include "ToolMode.h"
#include "Misc/OutputDeviceError.h"

using namespace BuildPatchTool;

IMPLEMENT_APPLICATION(BuildPatchTool, "BuildPatchTool");
DEFINE_LOG_CATEGORY(LogBuildPatchTool);

class FBuildPatchOutputDevice : public FOutputDevice
{
public:
	virtual void Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category ) override
	{
#if PLATFORM_WINDOWS
#if PLATFORM_USE_LS_SPEC_FOR_WIDECHAR
		printf("\n%ls", *FOutputDeviceHelper::FormatLogLine(Verbosity, Category, V, GPrintLogTimes));
#else
		wprintf(TEXT("\n%s"), *FOutputDeviceHelper::FormatLogLine(Verbosity, Category, V, GPrintLogTimes));
#endif
		fflush( stdout );
#endif
	}
};

const TCHAR* HandleLegacyCommandline(const TCHAR* CommandLine)
{
	static FString CommandLineString;
	CommandLineString = CommandLine;

#if UE_BUILD_DEBUG
	// Run smoke tests in debug
	CommandLineString += TEXT(" -bForceSmokeTests ");
#endif

	// No longer supported options
	if (CommandLineString.Contains(TEXT("-nochunks")))
	{
		UE_LOG(LogBuildPatchTool, Error, TEXT("NoChunks is no longer a supported mode. Remove this commandline option."));
		return nullptr;
	}

	// Check for legacy tool mode switching, if we don't have a mode and this was not a -help request, add the correct mode
	if (!CommandLineString.Contains(TEXT("-mode=")) && !CommandLineString.Contains(TEXT("-help")))
	{
		if (CommandLineString.Contains(TEXT("-compactify")))
		{
			CommandLineString = CommandLineString.Replace(TEXT("-compactify"), TEXT("-mode=compactify"));
		}
		else if (CommandLineString.Contains(TEXT("-dataenumerate")))
		{
			CommandLineString = CommandLineString.Replace(TEXT("-dataenumerate"), TEXT("-mode=enumeration"));
		}
		// Patch generation did not have a mode flag, but does have some unique and required params
		else if (CommandLineString.Contains(TEXT("-BuildRoot=")) && CommandLineString.Contains(TEXT("-BuildVersion=")))
		{
			FString NewCommandline(TEXT("-mode=patchgeneration "), CommandLineString.Len());
			NewCommandline += CommandLineString;
			CommandLineString = MoveTemp(NewCommandline);
		}
	}

	return *CommandLineString;
}

EReturnCode RunBuildPatchTool()
{
	// Load the BuildPatchServices Module
	IBuildPatchServicesModule& BuildPatchServicesModule = FModuleManager::LoadModuleChecked<IBuildPatchServicesModule>(TEXT("BuildPatchServices"));

	// Initialise the UObject system and process our uobject classes
	FModuleManager::Get().LoadModule(TEXT("CoreUObject"));
	FCoreDelegates::OnInit.Broadcast();
	ProcessNewlyLoadedUObjects();

	TSharedRef<IToolMode> ToolMode = FToolModeFactory::Create(BuildPatchServicesModule);
	return ToolMode->Execute();
}

EReturnCode BuildPatchToolMain(const TCHAR* CommandLine)
{
	// Add log device for stdout
	GLog->AddOutputDevice(new FBuildPatchOutputDevice());

	// Handle legacy commandlines
	CommandLine = HandleLegacyCommandline(CommandLine);
	if (CommandLine == nullptr)
	{
		return EReturnCode::ArgumentProcessingError;
	}

	// Initialise application
	GEngineLoop.PreInit(CommandLine);
	UE_LOG(LogBuildPatchTool, Log, TEXT("Executed with commandline: %s"), CommandLine);

	// Run the application
	EReturnCode ReturnCode = RunBuildPatchTool();
	if (ReturnCode != EReturnCode::OK)
	{
		UE_LOG(LogBuildPatchTool, Error, TEXT("Tool exited with: %d"), (int32)ReturnCode);
	}

	// Shutdown
	FCoreDelegates::OnExit.Broadcast();

	return ReturnCode;
}

const TCHAR* ProcessApplicationCommandline(int32 ArgC, TCHAR* ArgV[])
{
	static FString CommandLine = TEXT("-usehyperthreading -UNATTENDED");
	for (int32 Option = 1; Option < ArgC; Option++)
	{
		CommandLine += TEXT(" ");
		FString Argument(ArgV[Option]);
		if (Argument.Contains(TEXT(" ")))
		{
			if (Argument.Contains(TEXT("=")))
			{
				FString ArgName;
				FString ArgValue;
				Argument.Split(TEXT("="), &ArgName, &ArgValue);
				Argument = FString::Printf(TEXT("%s=\"%s\""), *ArgName, *ArgValue);
			}
			else
			{
				Argument = FString::Printf(TEXT("\"%s\""), *Argument);
			}
		}
		CommandLine += Argument;
	}
	return *CommandLine;
}

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	EReturnCode ReturnCode;
	// Using try&catch is the windows-specific method of interfacing with CrashReportClient
#if PLATFORM_WINDOWS && !PLATFORM_SEH_EXCEPTIONS_DISABLED
	__try
#endif
	{
		// SetCrashHandler(nullptr) sets up default behavior for Linux and Mac interfacing with CrashReportClient
		FPlatformMisc::SetCrashHandler(nullptr);
		GIsGuarded = 1;
		ReturnCode = BuildPatchToolMain(ProcessApplicationCommandline(ArgC, ArgV));
		GIsGuarded = 0;
	}
#if PLATFORM_WINDOWS && !PLATFORM_SEH_EXCEPTIONS_DISABLED
	__except (ReportCrash(GetExceptionInformation()))
	{
		ReturnCode = EReturnCode::Crash;
		GError->HandleError();
	}
#endif
	return static_cast<int32>(ReturnCode);
}
