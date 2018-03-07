// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DiffManifestMode.h"
#include "Interfaces/IBuildPatchServicesModule.h"
#include "BuildPatchTool.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"

using namespace BuildPatchTool;

class FDiffManifestToolMode : public IToolMode
{
public:
	FDiffManifestToolMode(IBuildPatchServicesModule& InBpsInterface)
		: BpsInterface(InBpsInterface)
	{}

	virtual ~FDiffManifestToolMode()
	{}

	virtual EReturnCode Execute() override
	{
		// Parse commandline.
		if (ProcessCommandLine() == false)
		{
			return EReturnCode::ArgumentProcessingError;
		}

		// Print help if requested.
		if (bHelp)
		{
			UE_LOG(LogBuildPatchTool, Log, TEXT("DIFF MANIFEST MODE"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("This tool mode reports the changes between two existing manifest files."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			UE_LOG(LogBuildPatchTool, Log, TEXT("Required arguments:"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -mode=DiffManifests    Must be specified to launch the tool in diff manifests mode."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -ManifestA=\"\"          Specifies in quotes the file path to the base manifest."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -ManifestB=\"\"          Specifies in quotes the file path to the update manifest."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			UE_LOG(LogBuildPatchTool, Log, TEXT("Optional arguments:"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -InstallTagsA=\"\"       Specifies in quotes a comma seperated list of install tags used on ManifestA. You should include empty string if you want to count untagged files."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("                           Leaving the parameter out will use all files."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("                           -InstallTagsA=\"\" will be untagged files only."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("                           -InstallTagsA=\",tag\" will be untagged files plus files tagged with 'tag'."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("                           -InstallTagsA=\"tag\" will be files tagged with 'tag' only."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -InstallTagsB=\"\"       Specifies in quotes a comma seperated list of install tags used on ManifestB. Same rules apply as InstallTagsA."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -OutputFile=\"\"         Specifies in quotes the file path where the diff will be exported as a JSON object."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			return EReturnCode::OK;
		}

		// Calc desired tags.
		TSet<FString> TagSetA, TagSetB;
		if (bHasTagsA)
		{
			TagSetA = ProcessTagList(InstallTagsA);
		}
		if (bHasTagsB)
		{
			TagSetB = ProcessTagList(InstallTagsB);
		}

		// Run the merge manifest routine.
		bool bSuccess = BpsInterface.DiffManifests(ManifestA, TagSetA, ManifestB, TagSetB, OutputFile);
		return bSuccess ? EReturnCode::OK : EReturnCode::ToolFailure;
	}

private:

	bool ProcessCommandLine()
	{
#define PARSE_SWITCH(Switch) ParseSwitch(TEXT(#Switch L"="), Switch, Switches)
		TArray<FString> Tokens, Switches;
		FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);

		bHelp = ParseOption(TEXT("help"), Switches);
		if (bHelp)
		{
			return true;
		}

		// Get all required parameters.
		if (!(PARSE_SWITCH(ManifestA)
		   && PARSE_SWITCH(ManifestB)))
		{
			UE_LOG(LogBuildPatchTool, Error, TEXT("ManifestA and ManifestB are required parameters."));
			return false;
		}
		FPaths::NormalizeDirectoryName(ManifestA);
		FPaths::NormalizeDirectoryName(ManifestB);

		// Get optional parameters
		bHasTagsA = PARSE_SWITCH(InstallTagsA);
		bHasTagsB = PARSE_SWITCH(InstallTagsB);
		PARSE_SWITCH(OutputFile);
		FPaths::NormalizeDirectoryName(OutputFile);

		return true;
#undef PARSE_SWITCH
	}

	TSet<FString> ProcessTagList(const FString& TagCommandLine) const
	{
		TArray<FString> TagArray;
		TagCommandLine.ParseIntoArray(TagArray, TEXT(","), false);
		for (FString& Tag : TagArray)
		{
			Tag.TrimStartAndEndInline();
		}
		if (TagArray.Num() == 0)
		{
			TagArray.Add(TEXT(""));
		}
		return TSet<FString>(MoveTemp(TagArray));
	}

private:
	IBuildPatchServicesModule& BpsInterface;
	bool bHelp;
	FString ManifestA;
	FString ManifestB;
	bool bHasTagsA;
	bool bHasTagsB;
	FString InstallTagsA;
	FString InstallTagsB;
	FString OutputFile;
};

BuildPatchTool::IToolModeRef BuildPatchTool::FDiffManifestToolModeFactory::Create(IBuildPatchServicesModule& BpsInterface)
{
	return MakeShareable(new FDiffManifestToolMode(BpsInterface));
}
