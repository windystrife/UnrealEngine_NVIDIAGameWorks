// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "GameMapsSettings.generated.h"

/** Ways the screen can be split with two players. */
UENUM()
namespace ETwoPlayerSplitScreenType
{
	enum Type
	{
		Horizontal,
		Vertical
	};
}


/** Ways the screen can be split with three players. */
UENUM()
namespace EThreePlayerSplitScreenType
{
	enum Type
	{
		FavorTop,
		FavorBottom
	};
}

/** Helper structure, used to associate GameModes with shortcut names. */
USTRUCT()
struct FGameModeName
{
	GENERATED_USTRUCT_BODY()

	/** Abbreviation/prefix that can be used as an alias for the class name */
	UPROPERTY(EditAnywhere, Category = DefaultModes, meta = (MetaClass = "GameModeBase"))
	FString Name;

	/** GameMode class to load */
	UPROPERTY(EditAnywhere, Category = DefaultModes, meta = (MetaClass = "GameModeBase"))
	FSoftClassPath GameMode;
};

UCLASS(config=Engine, defaultconfig)
class ENGINESETTINGS_API UGameMapsSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

	/**
	 * Get the default map specified in the settings.
	 * Makes a choice based on running as listen server/client vs dedicated server
	 *
	 * @return the default map specified in the settings
	 */
	static FString GetGameDefaultMap( );

	/**
	 * Get the global default game type specified in the configuration
	 * Makes a choice based on running as listen server/client vs dedicated server
	 * 
	 * @return the proper global default game type
	 */
	static FString GetGlobalDefaultGameMode( );

	/**
	 * Searches the GameModeClassAliases list for a named game mode, if not found will return passed in string
	 * 
	 * @return the proper game type class path to load
	 */
	static FString GetGameModeForName( const FString& GameModeName );

	/**
	 * Searches the GameModeMapPrefixes list for a named game mode, if not found will return passed in string
	 * 
	 * @return the proper game type class path to load, or empty if not found
	 */
	static FString GetGameModeForMapName( const FString& MapName );

	/**
	 * Set the default map to use (see GameDefaultMap below)
	 *
	 * @param NewMap name of valid map to use
	 */
	static void SetGameDefaultMap( const FString& NewMap );

	/**
	 * Set the default game type (see GlobalDefaultGameMode below)
	 *
	 * @param NewGameMode name of valid map to use
	 */
	static void SetGlobalDefaultGameMode( const FString& NewGameMode );

	virtual void PostInitProperties() override;
	virtual void PostReloadConfig( class UProperty* PropertyThatWasLoaded ) override;

public:

	/** If set, this map will be loaded when the Editor starts up. */
	UPROPERTY(config, EditAnywhere, Category=DefaultMaps, meta=(AllowedClasses="World"))
	FSoftObjectPath EditorStartupMap;

	/** The default options that will be appended to a map being loaded. */
	UPROPERTY(config, EditAnywhere, Category=DefaultMaps, AdvancedDisplay)
	FString LocalMapOptions;

	/** The map loaded when transition from one map to another. */
	UPROPERTY(config, EditAnywhere, Category=DefaultMaps, AdvancedDisplay, meta=(AllowedClasses="World"))
	FSoftObjectPath TransitionMap;

	/** Whether the screen should be split or not when multiple local players are present */
	UPROPERTY(config, EditAnywhere, Category=LocalMultiplayer)
	bool bUseSplitscreen;

	/** The viewport layout to use if the screen should be split and there are two local players */
	UPROPERTY(config, EditAnywhere, Category=LocalMultiplayer, meta=(editcondition="bUseSplitScreen"))
	TEnumAsByte<ETwoPlayerSplitScreenType::Type> TwoPlayerSplitscreenLayout;

	/** The viewport layout to use if the screen should be split and there are three local players */
	UPROPERTY(config, EditAnywhere, Category=LocalMultiplayer, meta=(editcondition="bUseSplitScreen"))
	TEnumAsByte<EThreePlayerSplitScreenType::Type> ThreePlayerSplitscreenLayout;

	/**
	* If enabled, this will make so that gamepads start being assigned to the second controller ID in local multiplayer games.
	* In PIE sessions with multiple windows, this has the same effect as enabling "Route 1st Gamepad to 2nd Client"
	*/
	UPROPERTY(config, EditAnywhere, Category=LocalMultiplayer, meta=(DisplayName="Skip Assigning Gamepad to Player 1"))
	bool bOffsetPlayerGamepadIds;

	/** The class to use when instantiating the transient GameInstance class */
	UPROPERTY(config, noclear, EditAnywhere, Category=GameInstance, meta=(MetaClass="GameInstance"))
	FSoftClassPath GameInstanceClass;

private:

	/** The map that will be loaded by default when no other map is loaded. */
	UPROPERTY(config, EditAnywhere, Category=DefaultMaps, meta=(AllowedClasses="World"))
	FSoftObjectPath GameDefaultMap;

	/** The map that will be loaded by default when no other map is loaded (DEDICATED SERVER). */
	UPROPERTY(config, EditAnywhere, Category=DefaultMaps, AdvancedDisplay, meta=(AllowedClasses="World"))
	FSoftObjectPath ServerDefaultMap;

	/** GameMode to use if not specified in any other way. (e.g. per-map DefaultGameMode or on the URL). */
	UPROPERTY(config, noclear, EditAnywhere, Category=DefaultModes, meta=(MetaClass="GameModeBase", DisplayName="Default GameMode"))
	FSoftClassPath GlobalDefaultGameMode;

	/**
	 * GameMode to use if not specified in any other way. (e.g. per-map DefaultGameMode or on the URL) (DEDICATED SERVERS)
	 * If not set, the GlobalDefaultGameMode value will be used.
	 */
	UPROPERTY(config, EditAnywhere, Category=DefaultModes, meta=(MetaClass="GameModeBase"), AdvancedDisplay)
	FSoftClassPath GlobalDefaultServerGameMode;

	/** Overrides the GameMode to use when loading a map that starts with a specific prefix */
	UPROPERTY(config, EditAnywhere, Category = DefaultModes, AdvancedDisplay)
	TArray<FGameModeName> GameModeMapPrefixes;

	/** List of GameModes to load when game= is specified in the URL (e.g. "DM" could be an alias for "MyProject.MyGameModeMP_DM") */
	UPROPERTY(config, EditAnywhere, Category = DefaultModes, AdvancedDisplay)
	TArray<FGameModeName> GameModeClassAliases;
};
