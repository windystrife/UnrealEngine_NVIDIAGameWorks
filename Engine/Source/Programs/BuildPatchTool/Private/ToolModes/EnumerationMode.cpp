// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EnumerationMode.h"
#include "Interfaces/IBuildPatchServicesModule.h"
#include "BuildPatchTool.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"

using namespace BuildPatchTool;

class FEnumerationToolMode : public IToolMode
{
public:
	FEnumerationToolMode(IBuildPatchServicesModule& InBpsInterface)
		: BpsInterface(InBpsInterface)
	{}

	virtual ~FEnumerationToolMode()
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
			UE_LOG(LogBuildPatchTool, Log, TEXT("ENUMERATION MODE"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("This tool supports enumerating patch data referenced by a build manifest."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			UE_LOG(LogBuildPatchTool, Log, TEXT("Required arguments:"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -mode=Enumeration    Must be specified to launch the tool in enumeration mode."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -InputFile=\"\"        Specifies in quotes the file path to the manifest to enumerate from."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -OutputFile=\"\"       Specifies in quotes the file path to a file where the list will be saved out."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			UE_LOG(LogBuildPatchTool, Log, TEXT("Optional arguments:"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -includesizes        When specified, the size of each file in bytes will also be output (see notes)."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			UE_LOG(LogBuildPatchTool, Log, TEXT("NB: If InputFile is a manifest, the output file format will be text file with one line per chunk, each containing cloud relative path."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("    e.g. path/to/chunk"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("    If InputFile is a chunk package, the output file format will be text file with one line per chunk, each containing tab separated hex chunk"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("    GUID, hex chunk rolling hash, and hex chunk SHA1."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("    e.g. 2CC26D05B64363780D5CF292E6B570A3\\t078070129133079067060057\\t527490FCA1DA6FAAB0E6F6E369E372FA693CCFBB"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("    Line endings are \\r\\n."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("NB: If includesizes is specified, each line of the output text file will end with a tab then the number of bytes of the chunk file."));
			return EReturnCode::OK;
		}

		// Run the enumeration routine
		bool bSuccess = BpsInterface.EnumeratePatchData(InputFile, OutputFile, bIncludeSizes);
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

		// Check for deprecated ManifestFile param.
		FString ManifestFile;
		if (PARSE_SWITCH(ManifestFile))
		{
			InputFile = ManifestFile;
		}

		// Get all required parameters
		if ((!PARSE_SWITCH(InputFile) && InputFile.IsEmpty())
		  || !PARSE_SWITCH(OutputFile))
		{
			UE_LOG(LogBuildPatchTool, Error, TEXT("InputFile and OutputFile are required parameters"));
			return false;
		}
		FPaths::NormalizeDirectoryName(InputFile);
		FPaths::NormalizeDirectoryName(OutputFile);

		// Get optional parameters
		bIncludeSizes = ParseOption(TEXT("includesizes"), Switches);

		return true;
#undef PARSE_SWITCH
	}

private:
	IBuildPatchServicesModule& BpsInterface;
	bool bHelp;
	FString InputFile;
	FString OutputFile;
	bool bIncludeSizes;
};

BuildPatchTool::IToolModeRef BuildPatchTool::FEnumerationToolModeFactory::Create(IBuildPatchServicesModule& BpsInterface)
{
	return MakeShareable(new FEnumerationToolMode(BpsInterface));
}
