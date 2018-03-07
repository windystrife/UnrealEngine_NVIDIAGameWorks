// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CodeLiteSourceCodeAccessor.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "DesktopPlatformModule.h"
#include "Misc/UProjectInfo.h"
#include "Misc/App.h"

DEFINE_LOG_CATEGORY_STATIC(LogCodeLiteAccessor, Log, All);

#define LOCTEXT_NAMESPACE "CodeLiteSourceCodeAccessor"

void FCodeLiteSourceCodeAccessor::Startup()
{
	// Cache this so we don't have to do it on a background thread
	GetSolutionPath();
}

void FCodeLiteSourceCodeAccessor::Shutdown()
{

}

bool FCodeLiteSourceCodeAccessor::CanAccessSourceCode() const
{
	FString Path;
	return CanRunCodeLite(Path);
}

FName FCodeLiteSourceCodeAccessor::GetFName() const
{
	return FName("CodeLite");
}

FText FCodeLiteSourceCodeAccessor::GetNameText() const
{
	return LOCTEXT("CodeLiteDisplayName", "CodeLite 7/8.x");
}

FText FCodeLiteSourceCodeAccessor::GetDescriptionText() const
{
	return LOCTEXT("CodeLiteDisplayDesc", "Open source code files in CodeLite");
}

bool FCodeLiteSourceCodeAccessor::OpenSolution()
{
	FString SolutionPath = GetSolutionPath();
	return OpenSolutionAtPath(SolutionPath);
}

bool FCodeLiteSourceCodeAccessor::OpenSolutionAtPath(const FString& InSolutionPath)
{
	FString SolutionPath = InSolutionPath;
	if (!SolutionPath.EndsWith(TEXT(".workspace")))
	{
		SolutionPath += TEXT(".workspace");
	}

	FString CodeLitePath;
	if(!CanRunCodeLite(CodeLitePath))
	{
		UE_LOG(LogCodeLiteAccessor, Warning, TEXT("FCodeLiteSourceCodeAccessor::OpenSolution: Cannot find CodeLite binary"));
		return false;
	}

	UE_LOG(LogCodeLiteAccessor, Warning, TEXT("FCodeLiteSourceCodeAccessor::OpenSolution: \"%s\" \"%s\""), *CodeLitePath, *SolutionPath);
	
	FProcHandle Proc = FPlatformProcess::CreateProc(*CodeLitePath, *SolutionPath, true, false, false, nullptr, 0, nullptr, nullptr);
	if(Proc.IsValid())
	{
		FPlatformProcess::CloseProc(Proc);
		return true;
	}
	return false;
}

bool FCodeLiteSourceCodeAccessor::DoesSolutionExist() const
 {
	FString SolutionPath = GetSolutionPath();	
	return FPaths::FileExists(SolutionPath);
 }

bool FCodeLiteSourceCodeAccessor::OpenSourceFiles(const TArray<FString>& AbsoluteSourcePaths)
{
	FString CodeLitePath;
	if(!CanRunCodeLite(CodeLitePath))
	{
		UE_LOG(LogCodeLiteAccessor, Warning, TEXT("FCodeLiteSourceCodeAccessor::OpenSourceFiles: Cannot find CodeLite binary"));
		return false;
	}

	for(const auto& SourcePath : AbsoluteSourcePaths)
	{
		const FString Path = FString::Printf(TEXT("\"%s\""), *SourcePath);

		FProcHandle Proc = FPlatformProcess::CreateProc(*CodeLitePath, *Path, true, false, false, nullptr, 0, nullptr, nullptr);
		if(Proc.IsValid())
		{
			UE_LOG(LogCodeLiteAccessor, Warning, TEXT("CodeLiteSourceCodeAccessor::OpenSourceFiles: %s"), *Path);
			FPlatformProcess::CloseProc(Proc);
			return true;
		}
	}

	return false;
}

bool FCodeLiteSourceCodeAccessor::OpenFileAtLine(const FString& FullPath, int32 LineNumber, int32 ColumnNumber)
{
	FString CodeLitePath;
	if(!CanRunCodeLite(CodeLitePath))
	{
		UE_LOG(LogCodeLiteAccessor,Warning, TEXT("FCodeLiteSourceCodeAccessor::OpenFileAtLine: Cannot find CodeLite binary"));
		return false;
	}

	const FString Path = FString::Printf(TEXT("\"%s --line=%d\""), *FullPath, LineNumber);

	if(FPlatformProcess::CreateProc(*CodeLitePath, *Path, true, true, false, nullptr, 0, nullptr, nullptr).IsValid())
	{
		UE_LOG(LogCodeLiteAccessor, Warning, TEXT("FCodeLiteSourceCodeAccessor::OpenSolution: Cannot find CodeLite binary"));
	}

	UE_LOG(LogCodeLiteAccessor, Warning, TEXT("CodeLiteSourceCodeAccessor::OpenFileAtLine: %s %d"), *FullPath, LineNumber);

	return true;
}

bool FCodeLiteSourceCodeAccessor::AddSourceFiles(const TArray<FString>& AbsoluteSourcePaths, const TArray<FString>& AvailableModules)
{
	// TODO Is this possible without dbus here? Maybe we can add this functionality to CodeLite.
	return false;
}

bool FCodeLiteSourceCodeAccessor::SaveAllOpenDocuments() const
{
	// TODO Is this possible without dbus here? Maybe we can add this functionality to CodeLite.
	return false;
}

void FCodeLiteSourceCodeAccessor::Tick(const float DeltaTime)
{
	// TODO What can we do here?
}


bool FCodeLiteSourceCodeAccessor::CanRunCodeLite(FString& OutPath) const
{
	// TODO This might be not a good idea to find an executable.
	OutPath = TEXT("/usr/bin/codelite");
	return FPaths::FileExists(OutPath);
}


bool FCodeLiteSourceCodeAccessor::IsIDERunning()
{
#if PLATFORM_LINUX
	pid_t pid = FindProcess("codelite");
	if(pid == -1)
	{
		return false;
	}

	return true;
#endif
	return false;

}

FString FCodeLiteSourceCodeAccessor::GetSolutionPath() const
{
	if(IsInGameThread())
	{
		CachedSolutionPath = FPaths::ProjectDir();
		
		if (!FUProjectDictionary(FPaths::RootDir()).IsForeignProject(CachedSolutionPath))
		{
			CachedSolutionPath = FPaths::Combine(FPaths::RootDir(), TEXT("UE4.workspace"));
		}
		else
		{
			FString BaseName = FApp::HasProjectName() ? FApp::GetProjectName() : FPaths::GetBaseFilename(CachedSolutionPath);
			CachedSolutionPath = FPaths::Combine(CachedSolutionPath, BaseName + TEXT(".workspace"));
		}
	}
	return CachedSolutionPath;
}

#if PLATFORM_LINUX
// This needs to be changed to use platform abstraction layer (see FPlatformProcess::FProcEnumerator)
pid_t FCodeLiteSourceCodeAccessor::FindProcess(const char* name)
{
	// TODO This is only to test. Will be changed later.
	DIR* dir;
	struct dirent* ent;
	char* endptr;
	char buf[512];

	if(!(dir = opendir("/proc")))
	{
		perror("can't open /proc");
		return -1;
	}

	while((ent = readdir(dir)) != NULL)
	{
		long lpid = strtol(ent->d_name, &endptr, 10);
		if(*endptr != '\0')
		{
			continue;
		}

		// Try to open the cmdline file
		snprintf(buf, sizeof(buf), "/proc/%ld/cmdline", lpid);
		FILE* fp = fopen(buf, "r");
		if(fp)
		{
			if(fgets(buf, sizeof(buf), fp) != NULL)
			{
				// check the first token in the file, the program name.
				char* first = strtok(buf, " ");
				if(!strcmp(first, name))
				{
					fclose(fp);
					closedir(dir);
					return (pid_t)lpid;
				}
			}
			fclose(fp);
		}
	}

	closedir(dir);

	return -1;
}
#endif

#undef LOCTEXT_NAMESPACE
