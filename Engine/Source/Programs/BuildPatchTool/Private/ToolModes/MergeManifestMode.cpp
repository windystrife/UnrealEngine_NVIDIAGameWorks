// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MergeManifestMode.h"
#include "Interfaces/IBuildPatchServicesModule.h"
#include "BuildPatchTool.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"

using namespace BuildPatchTool;

class FMergeManifestToolMode : public IToolMode
{
public:
	FMergeManifestToolMode(IBuildPatchServicesModule& InBpsInterface)
		: BpsInterface(InBpsInterface)
	{}

	virtual ~FMergeManifestToolMode()
	{}

	virtual EReturnCode Execute() override
	{
		// Parse commandline
		if (ProcessCommandline() == false)
		{
			return EReturnCode::ArgumentProcessingError;
		}

		// Print help if requested
		if (bHelp)
		{
			UE_LOG(LogBuildPatchTool, Log, TEXT("MERGE MANIFEST MODE"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("This tool supports generating a hotfix manifest from two existing manifest files."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			UE_LOG(LogBuildPatchTool, Log, TEXT("Required arguments:"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -mode=MergeManifests    Must be specified to launch the tool in merge manifests mode."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -ManifestA=\"\"           Specifies in quotes the file path to the base manifest."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -ManifestB=\"\"           Specifies in quotes the file path to the update manifest."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -ManifestC=\"\"           Specifies in quotes the file path to the output manifest."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -BuildVersion=\"\"        Specifies in quotes the new version string for the build being produced."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			UE_LOG(LogBuildPatchTool, Log, TEXT("Optional arguments:"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -MergeFileList=\"\"       Specifies in quotes, the path to a text file containing complete list of desired build root relative files followed by \\t character, followed by A or B to select the manifest to pull from. These should be seperated by \\r\\n line endings."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			UE_LOG(LogBuildPatchTool, Log, TEXT("NB: If -MergeFileList is not specified, then union of all files will be selected, preferring ManifestB's version."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("NB: With the exception of the new version string for the build, all meta will be copied from only ManifestB."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			return EReturnCode::OK;
		}

		// Run the merge manifest routine
		bool bSuccess = BpsInterface.MergeManifests(ManifestA, ManifestB, ManifestC, BuildVersion, MergeFileList);
		return bSuccess ? EReturnCode::OK : EReturnCode::ToolFailure;
	}

private:

	bool ProcessCommandline()
	{
#define PARSE_SWITCH(Switch) ParseSwitch(TEXT(#Switch L"="), Switch, Switches)
		TArray<FString> Tokens, Switches;
		FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);

		bHelp = ParseOption(TEXT("help"), Switches);
		if (bHelp)
		{
			return true;
		}

		// Get all required parameters
		if (!(PARSE_SWITCH(ManifestA)
		   && PARSE_SWITCH(ManifestB)
		   && PARSE_SWITCH(ManifestC)
		   && PARSE_SWITCH(BuildVersion)))
		{
			UE_LOG(LogBuildPatchTool, Error, TEXT("ManifestA, ManifestB, ManifestC, and BuildVersion are required parameters"));
			return false;
		}
		FPaths::NormalizeDirectoryName(ManifestA);
		FPaths::NormalizeDirectoryName(ManifestB);
		FPaths::NormalizeDirectoryName(ManifestC);

		// Optional list to pick specific files, otherwise it is A stomped by B
		PARSE_SWITCH(MergeFileList);
		FPaths::NormalizeDirectoryName(MergeFileList);

		return true;
#undef PARSE_SWITCH
	}

private:
	IBuildPatchServicesModule& BpsInterface;
	bool bHelp;
	FString ManifestA;
	FString ManifestB;
	FString ManifestC;
	FString BuildVersion;
	FString MergeFileList;
};

BuildPatchTool::IToolModeRef BuildPatchTool::FMergeManifestToolModeFactory::Create(IBuildPatchServicesModule& BpsInterface)
{
	return MakeShareable(new FMergeManifestToolMode(BpsInterface));
}
