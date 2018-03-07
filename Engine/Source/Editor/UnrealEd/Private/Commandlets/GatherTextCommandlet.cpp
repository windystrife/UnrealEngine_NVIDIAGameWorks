// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Commandlets/GatherTextCommandlet.h"
#include "UObject/Class.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/App.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogGatherTextCommandlet, Log, All);

/**
 *	UGatherTextCommandlet
 */
UGatherTextCommandlet::UGatherTextCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

const FString UGatherTextCommandlet::UsageText
	(
	TEXT("GatherTextCommandlet usage...\r\n")
	TEXT("    <GameName> GatherTextCommandlet -Config=<path to config ini file>\r\n")
	TEXT("    \r\n")
	TEXT("    <path to config ini file> Full path to the .ini config file that defines what gather steps the commandlet will run.\r\n")
	);


int32 UGatherTextCommandlet::Main( const FString& Params )
{
	const TCHAR* Parms = *Params;
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);
	
	// find the file corresponding to this object's loc file, loading it if necessary
	FString GatherTextConfigPath;
	const FString* ParamVal = ParamVals.Find(FString(TEXT("Config")));
	if (ParamVal)
	{
		GatherTextConfigPath = *ParamVal;
	}	
	else
	{
		UE_LOG(LogGatherTextCommandlet, Error, TEXT("-Config not specified.\n%s"), *UsageText);
		return -1;
	}

	if(FPaths::IsRelative(GatherTextConfigPath))
	{
		FString ProjectBasePath;
		if (!FPaths::ProjectDir().IsEmpty())
		{
			ProjectBasePath = FPaths::ProjectDir();
		}
		else
		{
			ProjectBasePath = FPaths::EngineDir();
		}
		GatherTextConfigPath = FPaths::Combine( *ProjectBasePath, *GatherTextConfigPath );
	}

	GConfig->LoadFile(*GatherTextConfigPath);

	if (!GConfig->FindConfigFile(*GatherTextConfigPath))
	{
		UE_LOG(LogGatherTextCommandlet, Error, TEXT("Loading Config File \"%s\" failed."), *GatherTextConfigPath);
		return -1; 
	}

	const bool bEnableSourceControl = Switches.Contains(TEXT("EnableSCC"));
	const bool bDisableSubmit = Switches.Contains(TEXT("DisableSCCSubmit"));

	UE_LOG(LogGatherTextCommandlet, Log,TEXT("Beginning GatherText Commandlet."));

	TSharedPtr< FLocalizationSCC > CommandletSourceControlInfo = nullptr;

	if( bEnableSourceControl )
	{
		CommandletSourceControlInfo = MakeShareable( new FLocalizationSCC() );

		FText SCCErrorStr;
		if( !CommandletSourceControlInfo->IsReady( SCCErrorStr ) )
		{
			UE_LOG( LogGatherTextCommandlet, Error, TEXT("Source Control error: %s"), *SCCErrorStr.ToString() );
			return -1;
		}
	}

	// Basic helper that can be used only to gather a new manifest for writing
	TSharedRef<FLocTextHelper> CommandletGatherManifestHelper = MakeShareable(new FLocTextHelper(MakeShareable(new FLocFileSCCNotifies(CommandletSourceControlInfo))));
	CommandletGatherManifestHelper->LoadManifest(ELocTextHelperLoadFlags::Create);

	const FString GatherTextStepPrefix = TEXT("GatherTextStep");

	// Read the list of steps from the config file (they all have the format GatherTextStep{N})
	TArray<FString> StepNames;
	GConfig->GetSectionNames(GatherTextConfigPath, StepNames);
	StepNames.RemoveAllSwap([&GatherTextStepPrefix](const FString& InStepName)
	{
		return !InStepName.StartsWith(GatherTextStepPrefix);
	});

	// Make sure the steps are sorted in ascending order (by numerical suffix)
	StepNames.Sort([&GatherTextStepPrefix](const FString& InStepNameOne, const FString& InStepNameTwo)
	{
		const FString NumericalSuffixOneStr = InStepNameOne.RightChop(GatherTextStepPrefix.Len());
		const int32 NumericalSuffixOne = FCString::Atoi(*NumericalSuffixOneStr);

		const FString NumericalSuffixTwoStr = InStepNameTwo.RightChop(GatherTextStepPrefix.Len());
		const int32 NumericalSuffixTwo = FCString::Atoi(*NumericalSuffixTwoStr);

		return NumericalSuffixOne < NumericalSuffixTwo;
	});

	//Execute each step defined in the config file.
	for (const FString& StepName : StepNames)
	{
		FString CommandletClassName = GConfig->GetStr( *StepName, TEXT("CommandletClass"), GatherTextConfigPath ) + TEXT("Commandlet");

		UClass* CommandletClass = FindObject<UClass>(ANY_PACKAGE,*CommandletClassName,false);
		if (!CommandletClass)
		{
			UE_LOG(LogGatherTextCommandlet, Error,TEXT("The commandlet name %s in section %s is invalid."), *CommandletClassName, *StepName);
			continue;
		}

		UGatherTextCommandletBase* Commandlet = NewObject<UGatherTextCommandletBase>(GetTransientPackage(), CommandletClass);
		check(Commandlet);
		Commandlet->AddToRoot();
		Commandlet->Initialize( CommandletGatherManifestHelper, CommandletSourceControlInfo );

		// Execute the commandlet.
		double CommandletExecutionStartTime = FPlatformTime::Seconds();

		UE_LOG(LogGatherTextCommandlet, Log,TEXT("Executing %s: %s"), *StepName, *CommandletClassName);
		
		FString GeneratedCmdLine = FString::Printf(TEXT("-Config=\"%s\" -Section=%s"), *GatherTextConfigPath , *StepName);

		// Add all the command params with the exception of config
		for(auto ParamIter = ParamVals.CreateConstIterator(); ParamIter; ++ParamIter)
		{
			const FString& Key = ParamIter.Key();
			const FString& Val = ParamIter.Value();
			if(Key != TEXT("config"))
			{
				GeneratedCmdLine += FString::Printf(TEXT(" -%s=%s"), *Key , *Val);
			}	
		}

		// Add all the command switches
		for(auto SwitchIter = Switches.CreateConstIterator(); SwitchIter; ++SwitchIter)
		{
			const FString& Switch = *SwitchIter;
			GeneratedCmdLine += FString::Printf(TEXT(" -%s"), *Switch);
		}

		if( 0 != Commandlet->Main( GeneratedCmdLine ) )
		{
			UE_LOG(LogGatherTextCommandlet, Error,TEXT("%s-%s reported an error."), *StepName, *CommandletClassName);
			if( CommandletSourceControlInfo.IsValid() )
			{
				FText SCCErrorStr;
				if( !CommandletSourceControlInfo->CleanUp( SCCErrorStr ) )
				{
					UE_LOG(LogGatherTextCommandlet, Error, TEXT("%s"), *SCCErrorStr.ToString());
				}
			}
			return -1;
		}

		UE_LOG(LogGatherTextCommandlet, Log,TEXT("Completed %s: %s"), *StepName, *CommandletClassName);
	}

	if( CommandletSourceControlInfo.IsValid() && !bDisableSubmit )
	{
		FText SCCErrorStr;
		if( CommandletSourceControlInfo->CheckinFiles( GetChangelistDescription(GatherTextConfigPath), SCCErrorStr ) )
		{
			UE_LOG(LogGatherTextCommandlet, Log,TEXT("Submitted Localization files."));
		}
		else
		{
			UE_LOG(LogGatherTextCommandlet, Error, TEXT("%s"), *SCCErrorStr.ToString());
			if( !CommandletSourceControlInfo->CleanUp( SCCErrorStr ) )
			{
				UE_LOG(LogGatherTextCommandlet, Error, TEXT("%s"), *SCCErrorStr.ToString());
			}
			return -1;
		}
	}

	return 0;
}

FText UGatherTextCommandlet::GetChangelistDescription( const FString& InConfigPath )
{
	// Find the target name to include in the change list description, this is just the config file name without path or extension info
	const FString TargetName = FPaths::GetBaseFilename( InConfigPath, true );

	// Find the project info to include in the changelist description from the config file path
	const FString AbsoluteConfigPath = FPaths::ConvertRelativePathToFull( InConfigPath );
	const FString RootDir = FPaths::RootDir();
	const FString PluginsDir = FPaths::ConvertRelativePathToFull( FPaths::ProjectPluginsDir() );
	bool bIsPlugin = false;
	FString ProjectName;
	if( AbsoluteConfigPath.StartsWith( RootDir ) )
	{
		// Strip the first portion of the config path which we are not interested in
		FString StrippedPath;
		if( AbsoluteConfigPath.StartsWith( PluginsDir ) )
		{
			bIsPlugin = true;
			StrippedPath = AbsoluteConfigPath.RightChop( PluginsDir.Len() );
		}
		else
		{
			StrippedPath = AbsoluteConfigPath.RightChop( RootDir.Len() );
		}

		// Grab the first token of our stripped path and use it as the project name
		FString Left, Right;
		StrippedPath.Split(TEXT("/"), &Left, &Right);
		ProjectName = Left;
	}
	else
	{
		// The config file falls outside of the root directory, we will use the game name if we have it
		if (FCString::Strlen(FApp::GetProjectName()) != 0)
		{
			ProjectName = FApp::GetProjectName();
		}
	}

	FString ChangeDescriptionString(TEXT("[Localization Update]"));

	if( !ProjectName.IsEmpty() )
	{
		ChangeDescriptionString += (bIsPlugin) ? TEXT(" Plugin: ") : TEXT(" Project: ");
		ChangeDescriptionString += ProjectName;
	}
	
	ChangeDescriptionString += FString::Printf( TEXT(" Target: %s"), *TargetName );
	return FText::FromString( ChangeDescriptionString );
}
