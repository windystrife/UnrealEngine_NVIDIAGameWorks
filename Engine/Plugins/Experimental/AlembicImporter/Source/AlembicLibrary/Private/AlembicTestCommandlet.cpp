// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AlembicTestCommandlet.h"
#include "AbcImporter.h"
#include "AbcImportSettings.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogAlembicCommandlet, Log, All);

static const char* Files[9]
{
	"C:/Jurre/OldDesktop/TestFiles/Alembic/Jenga.abc",
	"C:/Jurre/OldDesktop/TestFiles/Alembic/bullet_v1.abc",
	"C:/Jurre/OldDesktop/TestFiles/Alembic/Flag.abc",
	"C:/Jurre/OldDesktop/TestFiles/Alembic/Shatter.abc",
	"C:/Jurre/OldDesktop/TestFiles/Alembic/bullet.abc",	
	"C:/Jurre/OldDesktop/TestFiles/Alembic/group_animation.abc",
	"C:/Jurre/OldDesktop/TestFiles/Alembic/plane_anim.abc",
	"C:/Jurre/OldDesktop/TestFiles/Alembic/blobby_thing_v2.abc",
	"C:/Users/Jurre.deBaare/Desktop/loop_test.abc"
};

/**
 * UAlembicTestCommandlet
 *
 * Commandlet used for testing the alembic importer
 */

UAlembicTestCommandlet::UAlembicTestCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UAlembicTestCommandlet::Main(const FString& Params)
{
	bool bSuccess = false;

 	const TCHAR* ParamStr = *Params;
	ParseCommandLine(ParamStr, CmdLineTokens, CmdLineSwitches);

	FAbcImporter Importer;
	if (CmdLineSwitches.Num())
	{
		int32 FileIndex;
		FString FilePath;

		float PercentageBase = 1.0f;
		FParse::Value(*Params, TEXT("basepercentage="), PercentageBase);

		int32 FixedNumBases = 0;
		FParse::Value(*Params, TEXT("fixednumbases="), FixedNumBases);

		int32 NumThreads = 1;
		FParse::Value(*Params, TEXT("threads="), NumThreads);

		if (FParse::Value(*Params, TEXT("fileindex="), FileIndex))
		{
			if (FPaths::FileExists(Files[FileIndex]))
			{
				UE_LOG(LogAlembicCommandlet, Display, TEXT("Running Alembic test for %s using %i threads"), *FString(Files[FileIndex]), NumThreads);

				UAbcImportSettings* Settings = UAbcImportSettings::Get();
				Settings->CompressionSettings.MaxNumberOfBases = FixedNumBases;
				Settings->CompressionSettings.PercentageOfTotalBases = PercentageBase;

				Importer.OpenAbcFileForImport(Files[FileIndex]);		
				Importer.ImportTrackData(NumThreads, Settings);				
				Importer.ImportAsSkeletalMesh(nullptr, RF_NoFlags);

				bSuccess = true;
			}
			else
			{
				UE_LOG(LogAlembicCommandlet, Display, TEXT("File %s not found"), *FString(Files[FileIndex]));
			}
		}
		else if (FParse::Value(*Params, TEXT("file="), FilePath))
		{
			if (FPaths::FileExists(FilePath))
			{
				UE_LOG(LogAlembicCommandlet, Display, TEXT("Running Alembic test for %s using %i threads"), *FilePath, NumThreads);

				UAbcImportSettings* Settings = UAbcImportSettings::Get();
				Settings->CompressionSettings.MaxNumberOfBases = FixedNumBases;
				Settings->CompressionSettings.PercentageOfTotalBases = PercentageBase;

				Importer.OpenAbcFileForImport(FilePath);
				Importer.ImportTrackData(NumThreads, Settings);
				Importer.ImportAsSkeletalMesh(nullptr, RF_NoFlags);

				bSuccess = true;
			}
			else
			{
				UE_LOG(LogAlembicCommandlet, Display, TEXT("File %s not found"), *FilePath);
			}
		}
		else
		{
			UE_LOG(LogAlembicCommandlet, Display, TEXT("No correct command line arguments found"));
		}
	}	

	FPlatformProcess::Sleep(0.005f);

	return bSuccess ? 0 : 1;
}
