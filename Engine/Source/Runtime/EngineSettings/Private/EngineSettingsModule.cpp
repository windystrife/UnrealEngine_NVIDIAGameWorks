// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "ConsoleSettings.h"
#include "GameNetworkManagerSettings.h"
#include "GameMapsSettings.h"
#include "GameSessionSettings.h"
#include "GeneralEngineSettings.h"
#include "GeneralProjectSettings.h"
#include "HudSettings.h"
#include "Misc/ConfigCacheIni.h"


/**
 * Implements the EngineSettings module.
 */
class FEngineSettingsModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface

	virtual void StartupModule( ) override { }
	virtual void ShutdownModule( ) override { }

	virtual bool SupportsDynamicReloading( ) override
	{
		return true;
	}
};


/* Class constructors
 *****************************************************************************/

UConsoleSettings::UConsoleSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }


UGameMapsSettings::UGameMapsSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, bUseSplitscreen(true)
	, TwoPlayerSplitscreenLayout(ETwoPlayerSplitScreenType::Horizontal)
	, ThreePlayerSplitscreenLayout(EThreePlayerSplitScreenType::FavorTop)
	, bOffsetPlayerGamepadIds(false)
{ }


UGameNetworkManagerSettings::UGameNetworkManagerSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }


UGameSessionSettings::UGameSessionSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }


UGeneralEngineSettings::UGeneralEngineSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }


UGeneralProjectSettings::UGeneralProjectSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, bShouldWindowPreserveAspectRatio(true)
	, bUseBorderlessWindow(false)
	, bStartInVR(false)
	, bStartInAR(false)
	, bAllowWindowResize(true)
	, bAllowClose(true)
	, bAllowMaximize(true)
	, bAllowMinimize(true)
{ }


UHudSettings::UHudSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{ }


/* Static functions
 *****************************************************************************/

FString UGameMapsSettings::GetGameDefaultMap( )
{
	return IsRunningDedicatedServer()
		? GetDefault<UGameMapsSettings>()->ServerDefaultMap.GetLongPackageName()
		: GetDefault<UGameMapsSettings>()->GameDefaultMap.GetLongPackageName();
}


FString UGameMapsSettings::GetGlobalDefaultGameMode( )
{
	UGameMapsSettings* GameMapsSettings = Cast<UGameMapsSettings>(UGameMapsSettings::StaticClass()->GetDefaultObject());

	return IsRunningDedicatedServer() && GameMapsSettings->GlobalDefaultServerGameMode.IsValid()
		? GameMapsSettings->GlobalDefaultServerGameMode.ToString()
		: GameMapsSettings->GlobalDefaultGameMode.ToString();
}

FString UGameMapsSettings::GetGameModeForName(const FString& GameModeName)
{
	UGameMapsSettings* GameMapsSettings = Cast<UGameMapsSettings>(UGameMapsSettings::StaticClass()->GetDefaultObject());

	// Look to see if this should be remapped from a shortname to full class name
	for (const FGameModeName& Alias : GameMapsSettings->GameModeClassAliases)
	{
		if (GameModeName == Alias.Name)
		{
			// switch GameClassName to the full name
			return Alias.GameMode.ToString();
		}
	}

	// Check deprecated config
	FConfigSection* GameModeSection = GConfig->GetSectionPrivate(TEXT("/Script/Engine.GameMode"), false, true, GGameIni);
	
	if (GameModeSection)
	{
		TArray<FString> ConfigLines;
		GameModeSection->MultiFind(TEXT("GameModeClassAliases"), ConfigLines);

		if (ConfigLines.Num())
		{
			UE_LOG(LogLoad, Warning, TEXT("GameMode::GameModeClassAliases are deprecated, move to GameMapsSettings"));

			for (FString& ConfigString : ConfigLines)
			{
				FString ModeName, ModePath;
				if (FParse::Value(*ConfigString, TEXT("ShortName="), ModeName) && FParse::Value(*ConfigString, TEXT("GameClassName="), ModePath))
				{
					if (ModeName == GameModeName)
					{
						return ModePath;
					}
				}
			}

		}
	}

	return GameModeName;
}

FString UGameMapsSettings::GetGameModeForMapName(const FString& MapName)
{
	UGameMapsSettings* GameMapsSettings = Cast<UGameMapsSettings>(UGameMapsSettings::StaticClass()->GetDefaultObject());

	// See if we have a per-prefix default specified
	for (const FGameModeName& Prefix : GameMapsSettings->GameModeMapPrefixes)
	{
		if ((Prefix.Name.Len() > 0) && MapName.StartsWith(Prefix.Name))
		{
			return Prefix.GameMode.ToString();
		}
	}

	// Check deprecated config
	FConfigSection* GameModeSection = GConfig->GetSectionPrivate(TEXT("/Script/Engine.WorldSettings"), false, true, GGameIni);

	if (GameModeSection)
	{
		TArray<FString> ConfigLines;
		GameModeSection->MultiFind(TEXT("DefaultMapPrefixes"), ConfigLines);

		if (ConfigLines.Num())
		{
			UE_LOG(LogLoad, Warning, TEXT("GameMode::DefaultMapPrefixes are deprecated, move to GameMapsSettings::GameModeMapPrefixes"));

			for (FString& ConfigString : ConfigLines)
			{
				FString Prefix, ModePath;
				if (FParse::Value(*ConfigString, TEXT("Prefix="), Prefix) && FParse::Value(*ConfigString, TEXT("GameMode="), ModePath))
				{
					if (MapName.StartsWith(Prefix))
					{
						return ModePath;
					}
				}
			}

		}
	}

	return FString();
}

void UGameMapsSettings::SetGameDefaultMap( const FString& NewMap )
{
	UGameMapsSettings* GameMapsSettings = Cast<UGameMapsSettings>(UGameMapsSettings::StaticClass()->GetDefaultObject());

	if (IsRunningDedicatedServer())
	{
		GameMapsSettings->ServerDefaultMap = NewMap;
	}
	else
	{
		GameMapsSettings->GameDefaultMap = NewMap;
	}
}

void UGameMapsSettings::SetGlobalDefaultGameMode( const FString& NewGameMode )
{
	UGameMapsSettings* GameMapsSettings = Cast<UGameMapsSettings>(UGameMapsSettings::StaticClass()->GetDefaultObject());
	
	GameMapsSettings->GlobalDefaultGameMode = NewGameMode;
}

// Backwards compat for map strings
void FixMapAssetRef(FSoftObjectPath& MapAssetReference)
{
	const FString AssetRefStr = MapAssetReference.ToString();
	int32 DummyIndex;
	if (!AssetRefStr.IsEmpty() && !AssetRefStr.FindLastChar(TEXT('.'), DummyIndex))
	{
		FString MapName, MapPath;
		AssetRefStr.Split(TEXT("/"), &MapPath, &MapName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		MapAssetReference.SetPath(FString::Printf(TEXT("%s/%s.%s"),*MapPath, *MapName, *MapName));
	}
};

void UGameMapsSettings::PostInitProperties()
{
	Super::PostInitProperties();

	FixMapAssetRef(EditorStartupMap);
	FixMapAssetRef(GameDefaultMap);
	FixMapAssetRef(ServerDefaultMap);
	FixMapAssetRef(TransitionMap);
}

void UGameMapsSettings::PostReloadConfig( UProperty* PropertyThatWasLoaded )
{
	if (PropertyThatWasLoaded)
	{
		if (PropertyThatWasLoaded->GetFName() == GET_MEMBER_NAME_CHECKED(UGameMapsSettings, EditorStartupMap))
		{
			FixMapAssetRef(EditorStartupMap);
		}
		else if (PropertyThatWasLoaded->GetFName() == GET_MEMBER_NAME_CHECKED(UGameMapsSettings, GameDefaultMap))
		{
			FixMapAssetRef(GameDefaultMap);
		}
		else if (PropertyThatWasLoaded->GetFName() == GET_MEMBER_NAME_CHECKED(UGameMapsSettings, ServerDefaultMap))
		{
			FixMapAssetRef(ServerDefaultMap);
		}
		else if (PropertyThatWasLoaded->GetFName() == GET_MEMBER_NAME_CHECKED(UGameMapsSettings, TransitionMap))
		{
			FixMapAssetRef(TransitionMap);
		}
	}
	else
	{
		FixMapAssetRef(EditorStartupMap);
		FixMapAssetRef(GameDefaultMap);
		FixMapAssetRef(ServerDefaultMap);
		FixMapAssetRef(TransitionMap);
	}
}


IMPLEMENT_MODULE(FEngineSettingsModule, EngineSettings);
