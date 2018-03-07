// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Interfaces/ToolMode.h"
#include "Misc/CommandLine.h"
#include "Interfaces/IBuildPatchServicesModule.h"
#include "ToolModes/PatchGenerationMode.h"
#include "ToolModes/CompactifyMode.h"
#include "ToolModes/EnumerationMode.h"
#include "ToolModes/MergeManifestMode.h"
#include "ToolModes/DiffManifestMode.h"
#include "ToolModes/PackageChunksMode.h"
#include "ToolModes/VerifyChunksMode.h"
#include "ToolModes/AutomationMode.h"
#include "BuildPatchTool.h"

namespace BuildPatchTool
{
	class FHelpToolMode : public IToolMode
	{
	public:
		FHelpToolMode()
		{}

		virtual ~FHelpToolMode()
		{}

		virtual EReturnCode Execute() override
		{
			bool bRequestedHelp = FParse::Param(FCommandLine::Get(), TEXT("help"));

			// Output generic help info
			if (!bRequestedHelp)
			{
				UE_LOG(LogBuildPatchTool, Error, TEXT("No supported mode detected."));
			}
			UE_LOG(LogBuildPatchTool, Log, TEXT("-help can be added with any mode selection to get extended information."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("Supported modes are:"));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -mode=PatchGeneration    Mode that generates patch data for the a new build."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -mode=Compactify         Mode that can clean up unneeded patch data from a given cloud directory with redundant data."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -mode=Enumeration        Mode that outputs the paths to referenced patch data given a single manifest."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -mode=MergeManifests     Mode that can combine two manifest files to create a new one, primarily used to create hotfixes."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -mode=DiffManifests      Mode that can diff two manifests and outputs what chunks would need to be downloaded and some stats."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -mode=PackageChunks      Mode that packages data required for an installation into larger files which can be used as local sources for build patch installers."));
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -mode=VerifyChunks       Mode that allows you to verify the integrity of patch data. It will load chunk or chunkdb files to verify they are not corrupt."));
#if !UE_BUILD_SHIPPING
			UE_LOG(LogBuildPatchTool, Log, TEXT("  -mode=AutomationTests    Mode that will run automation testing."));
#endif // !UE_BUILD_SHIPPING

			// Error if this wasn't just a help request
			return bRequestedHelp ? EReturnCode::OK : EReturnCode::UnknownToolMode;
		}
	};

	IToolModeRef FToolModeFactory::Create(IBuildPatchServicesModule& BpsInterface)
	{
		// Create the correct tool mode for the commandline given
		FString ToolModeValue;
		if (FParse::Value(FCommandLine::Get(), TEXT("mode="), ToolModeValue))
		{
			if (ToolModeValue == TEXT("patchgeneration"))
			{
				return FPatchGenerationToolModeFactory::Create(BpsInterface);
			}
			else if (ToolModeValue == TEXT("compactify"))
			{
				return FCompactifyToolModeFactory::Create(BpsInterface);
			}
			else if (ToolModeValue == TEXT("enumeration"))
			{
				return FEnumerationToolModeFactory::Create(BpsInterface);
			}
			else if (ToolModeValue == TEXT("mergemanifests"))
			{
				return FMergeManifestToolModeFactory::Create(BpsInterface);
			}
			else if (ToolModeValue == TEXT("diffmanifests"))
			{
				return FDiffManifestToolModeFactory::Create(BpsInterface);
			}
			else if (ToolModeValue == TEXT("packagechunks"))
			{
				return FPackageChunksToolModeFactory::Create(BpsInterface);
			}
			else if (ToolModeValue == TEXT("verifychunks"))
			{
				return FVerifyChunksToolModeFactory::Create(BpsInterface);
			}
#if !UE_BUILD_SHIPPING
			else if (ToolModeValue == TEXT("automationtests"))
			{
				return FAutomationToolModeFactory::Create(BpsInterface);
			}
#endif // !UE_BUILD_SHIPPING
		}
		
		// No mode provided so create the generic help, which will return ok if -help was provided else return UnknownToolMode error
		return MakeShareable(new FHelpToolMode());
	}
}
