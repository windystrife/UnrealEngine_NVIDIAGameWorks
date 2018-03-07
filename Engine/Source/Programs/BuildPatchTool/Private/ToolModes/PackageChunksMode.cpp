// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PackageChunksMode.h"
#include "Interfaces/IBuildPatchServicesModule.h"
#include "BuildPatchTool.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Containers/Algo/Find.h"

using namespace BuildPatchTool;

class FPackageChunksToolMode : public IToolMode
{
public:
	FPackageChunksToolMode(IBuildPatchServicesModule& InBpsInterface)
		: BpsInterface(InBpsInterface)
	{}

	virtual ~FPackageChunksToolMode()
	{}

	virtual EReturnCode Execute() override
	{
		// Parse commandline.
		if (ProcessCommandline() == false)
		{
			return EReturnCode::ArgumentProcessingError;
		}

		// Print help if requested.
		if (bHelp)
		{
			UE_LOG(LogBuildPatchTool, Log, TEXT("PACKAGE CHUNKS MODE"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("This tool mode supports packaging data required for an installation into larger files which can be used as local sources for build patch installers."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			UE_LOG(LogBuildPatchTool, Log, TEXT("Required arguments:"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -mode=PackageChunks  Must be specified to launch the tool in package chunks mode."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -ManifestFile=\"\"     Specifies in quotes the file path to the manifest to enumerate chunks from."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -OutputFile=\"\"       Specifies in quotes the file path the output package. Extension of .chunkdb will be added if not present."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			UE_LOG(LogBuildPatchTool, Log, TEXT("Optional arguments:"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -CloudDir=\"\"         Specifies in quotes the cloud directory where chunks to be packaged can be found."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -MaxOutputFileSize=  When specified, the size of each output file (in bytes) will be limited to a maximum of the provided value."));
			UE_LOG(LogBuildPatchTool, Log, TEXT(""));
			UE_LOG(LogBuildPatchTool, Log, TEXT("NB: If CloudDir is not specified, the manifest file location will be used as the cloud directory."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("NB: MaxOutputFileSize is recommended to be as large as possible. The minimum individual chunkdb filesize is equal to one chunk plus chunkdb"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("    header, and thus will not result in efficient behavior."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("NB: If MaxOutputFileSize is not specified, the one output file will be produced containing all required data."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("NB: If MaxOutputFileSize is specified, the output files will be generated as Name.part01.chunkdb, Name.part02.chunkdb etc. The part number will"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("    have the number of digits required for highest numbered part."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("NB: If MaxOutputFileSize is specified, then each part can be equal to or less than the specified size, depending on the size of the last chunk"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("    that fits."));
			return EReturnCode::OK;
		}

		// Run the enumeration routine.
		bool bSuccess = BpsInterface.PackageChunkData(ManifestFile, OutputFile, CloudDir, MaxOutputFileSize);
		return bSuccess ? EReturnCode::OK : EReturnCode::ToolFailure;
	}

private:

	bool ProcessCommandline()
	{
#define HAS_SWITCH(Switch) (Algo::FindByPredicate(Switches, [](const FString& Elem){ return Elem.StartsWith(TEXT(#Switch "="));}) != nullptr)
#define PARSE_SWITCH(Switch) ParseSwitch(TEXT(#Switch "="), Switch, Switches)
		TArray<FString> Tokens, Switches;
		FCommandLine::Parse(FCommandLine::Get(), Tokens, Switches);

		bHelp = ParseOption(TEXT("help"), Switches);
		if (bHelp)
		{
			return true;
		}

		// Get all required parameters.
		if (!(PARSE_SWITCH(ManifestFile)
		   && PARSE_SWITCH(OutputFile)))
		{
			UE_LOG(LogBuildPatchTool, Error, TEXT("ManifestFile and OutputFile are required parameters"));
			return false;
		}
		FPaths::NormalizeFilename(ManifestFile);
		FPaths::NormalizeFilename(OutputFile);

		// Get optional parameters.
		if (!PARSE_SWITCH(CloudDir))
		{
			// If not provided we use the location of the manifest file.
			CloudDir = FPaths::GetPath(ManifestFile);
		}
		FPaths::NormalizeDirectoryName(CloudDir);

		if (HAS_SWITCH(MaxOutputFileSize))
		{
			if (!PARSE_SWITCH(MaxOutputFileSize))
			{
				// Failing to parse a provided MaxOutputFileSize is an error.
				UE_LOG(LogBuildPatchTool, Error, TEXT("MaxOutputFileSize must be a valid uint64"));
				return false;
			}
		}
		else
		{
			// If not provided we don't limit the size, which is the equivalent of limiting to max uint64.
			MaxOutputFileSize = MAX_uint64;
		}

		return true;
#undef PARSE_SWITCH
#undef HAS_SWITCH
	}

private:
	IBuildPatchServicesModule& BpsInterface;
	bool bHelp;
	FString ManifestFile;
	FString OutputFile;
	FString CloudDir;
	uint64 MaxOutputFileSize;
};

BuildPatchTool::IToolModeRef BuildPatchTool::FPackageChunksToolModeFactory::Create(IBuildPatchServicesModule& BpsInterface)
{
	return MakeShareable(new FPackageChunksToolMode(BpsInterface));
}
