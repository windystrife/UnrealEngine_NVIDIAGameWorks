// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VisualStudioCodeSourceCodeAccessor.h"
#include "VisualStudioCodeSourceCodeAccessModule.h"
#include "ISourceCodeAccessModule.h"
#include "ModuleManager.h"
#include "DesktopPlatformModule.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Misc/UProjectInfo.h"

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#endif
#include "Internationalization/Regex.h"

DEFINE_LOG_CATEGORY_STATIC(LogVSCodeAccessor, Log, All);

#define LOCTEXT_NAMESPACE "VisualStudioCodeSourceCodeAccessor"

static FString MakePath(const FString& InPath)
{
	return TEXT("\"") + InPath + TEXT("\""); 
}

FString FVisualStudioCodeSourceCodeAccessor::GetSolutionPath() const
{
	FScopeLock Lock(&CachedSolutionPathCriticalSection);

	if (IsInGameThread())
	{
		CachedSolutionPath = FPaths::ProjectDir();

		if (!FUProjectDictionary(FPaths::RootDir()).IsForeignProject(CachedSolutionPath))
		{
			CachedSolutionPath = FPaths::RootDir();
		}
	}

	return CachedSolutionPath;
}

/** save all open documents in visual studio, when recompiling */
void OnModuleCompileStarted(bool bIsAsyncCompile)
{
	FVisualStudioCodeSourceCodeAccessModule& VisualStudioCodeSourceCodeAccessModule = FModuleManager::LoadModuleChecked<FVisualStudioCodeSourceCodeAccessModule>(TEXT("VisualStudioCodeSourceCodeAccess"));
	VisualStudioCodeSourceCodeAccessModule.GetAccessor().SaveAllOpenDocuments();
}

void FVisualStudioCodeSourceCodeAccessor::Startup()
{
	GetSolutionPath();
	RefreshAvailability();
}

void FVisualStudioCodeSourceCodeAccessor::RefreshAvailability()
{
#if PLATFORM_WINDOWS
	FString IDEPath;

	if (!FWindowsPlatformMisc::QueryRegKey(HKEY_CURRENT_USER, TEXT("SOFTWARE\\Classes\\Applications\\Code.exe\\shell\\open\\command\\"), TEXT(""), IDEPath))
	{
		FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Classes\\Applications\\Code.exe\\shell\\open\\command\\"), TEXT(""), IDEPath);
	}

	FString PatternString(TEXT("\"(.*)\" \".*\""));
	FRegexPattern Pattern(PatternString);
	FRegexMatcher Matcher(Pattern, IDEPath);
	if (Matcher.FindNext())
	{
		FString URL = Matcher.GetCaptureGroup(1);
		if (FPaths::FileExists(URL))
		{
			Location.URL = URL;
		}
	}
#elif PLATFORM_LINUX
	FString OutURL;
	int32 ReturnCode = -1;

	FPlatformProcess::ExecProcess(TEXT("/bin/bash"), TEXT("-c \"type -p code\""), &ReturnCode, &OutURL, nullptr);
	if (ReturnCode == 0)
	{
		Location.URL = OutURL.TrimStartAndEnd();
	}
	else
	{
		// Fallback to default install location
		FString URL = TEXT("/usr/bin/code");
		if (FPaths::FileExists(URL))
		{
			Location.URL = URL;
		}
	}
#elif PLATFORM_MAC
	NSURL* AppURL = [[NSWorkspace sharedWorkspace] URLForApplicationWithBundleIdentifier:@"com.microsoft.VSCode"];
	if (AppURL != nullptr)
	{
		Location.URL = FString([AppURL path]);
	}
#endif
}

void FVisualStudioCodeSourceCodeAccessor::Shutdown()
{
}

bool FVisualStudioCodeSourceCodeAccessor::OpenSourceFiles(const TArray<FString>& AbsoluteSourcePaths)
{
	if (Location.IsValid())
	{
		FString SolutionDir = GetSolutionPath();
		TArray<FString> Args;
		Args.Add(MakePath(SolutionDir));

		for (const FString& SourcePath : AbsoluteSourcePaths)
		{
			Args.Add(MakePath(SourcePath));
		}

		return Launch(Args);
	}

	return false;
}

bool FVisualStudioCodeSourceCodeAccessor::AddSourceFiles(const TArray<FString>& AbsoluteSourcePaths, const TArray<FString>& AvailableModules)
{
	return false;
}

bool FVisualStudioCodeSourceCodeAccessor::OpenFileAtLine(const FString& FullPath, int32 LineNumber, int32 ColumnNumber)
{
	if (Location.IsValid())
	{
		// Column & line numbers are 1-based, so dont allow zero
		LineNumber = LineNumber > 0 ? LineNumber : 1;
		ColumnNumber = ColumnNumber > 0 ? ColumnNumber : 1;

		FString SolutionDir = GetSolutionPath();
		TArray<FString> Args;
		Args.Add(MakePath(SolutionDir));
		Args.Add(TEXT("-g ") + MakePath(FullPath) + FString::Printf(TEXT(":%d:%d"), LineNumber, ColumnNumber));
		return Launch(Args);
	}

	return false;
}

bool FVisualStudioCodeSourceCodeAccessor::CanAccessSourceCode() const
{
	// True if we have any versions of VS installed
	return Location.IsValid();
}

FName FVisualStudioCodeSourceCodeAccessor::GetFName() const
{
	return FName("VisualStudioCode");
}

FText FVisualStudioCodeSourceCodeAccessor::GetNameText() const
{
	return LOCTEXT("VisualStudioDisplayName", "Visual Studio Code");
}

FText FVisualStudioCodeSourceCodeAccessor::GetDescriptionText() const
{
	return LOCTEXT("VisualStudioDisplayDesc", "Open source code files in Visual Studio Code");
}

void FVisualStudioCodeSourceCodeAccessor::Tick(const float DeltaTime)
{
}

bool FVisualStudioCodeSourceCodeAccessor::OpenSolution()
{
	if (Location.IsValid())
	{
		FString SolutionDir = FPaths::Combine(GetSolutionPath(), TEXT("UE4"));
		return OpenSolutionAtPath(SolutionDir);
	}

	return false;
}

bool FVisualStudioCodeSourceCodeAccessor::OpenSolutionAtPath(const FString& InSolutionPath)
{
	if (Location.IsValid())
	{
		// Strip top element from the path. When creating new projects, this will be the base name of the solution which we don't need,
		// or if being called from OpenSolution() it will be a dummy "UE4" element that we added just so it can be stripped here
		FString SolutionPath = FPaths::GetPath(InSolutionPath);
		TArray<FString> Args;
		Args.Add(MakePath(SolutionPath));
		return Launch(Args);
	}

	return false;
}

bool FVisualStudioCodeSourceCodeAccessor::DoesSolutionExist() const
{
	FString VSCodeDir = FPaths::Combine(GetSolutionPath(), TEXT(".vscode"));
	return FPaths::DirectoryExists(VSCodeDir);
}


bool FVisualStudioCodeSourceCodeAccessor::SaveAllOpenDocuments() const
{
	return false;
}

bool FVisualStudioCodeSourceCodeAccessor::Launch(const TArray<FString>& InArgs)
{
	if (Location.IsValid())
	{
		FString ArgsString;
		for (const FString& Arg : InArgs)
		{
			ArgsString.Append(Arg);
			ArgsString.Append(TEXT(" "));
		}

		uint32 ProcessID;
		FProcHandle hProcess = FPlatformProcess::CreateProc(*Location.URL, *ArgsString, true, false, false, &ProcessID, 0, nullptr, nullptr, nullptr);
		return hProcess.IsValid();
	}
	
	return false;
}

#undef LOCTEXT_NAMESPACE
