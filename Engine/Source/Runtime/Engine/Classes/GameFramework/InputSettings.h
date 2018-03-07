// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "InputCoreTypes.h"
#include "UObject/SoftObjectPath.h"
#include "GameFramework/PlayerInput.h"

#include "InputSettings.generated.h"

/**
 * Project wide settings for input handling
 * 
 * @see https://docs.unrealengine.com/latest/INT/Gameplay/Input/index.html
 */
UCLASS(config=Input, defaultconfig)
class ENGINE_API UInputSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

	/** Properties of Axis controls */
	UPROPERTY(config, EditAnywhere, EditFixedSize, Category="Bindings", meta=(ToolTip="List of Axis Properties"), AdvancedDisplay)
	TArray<struct FInputAxisConfigEntry> AxisConfig;

	UPROPERTY(config, EditAnywhere, Category="Bindings", AdvancedDisplay)
	uint32 bAltEnterTogglesFullscreen:1;

	UPROPERTY(config, EditAnywhere, Category = "Bindings", AdvancedDisplay)
	uint32 bF11TogglesFullscreen : 1;

	// Allow mouse to be used for touch
	UPROPERTY(config, EditAnywhere, Category="MouseProperties")
	uint32 bUseMouseForTouch:1;

	// Mouse smoothing control
	UPROPERTY(config, EditAnywhere, Category="MouseProperties", AdvancedDisplay)
	uint32 bEnableMouseSmoothing:1;

	// Scale the mouse based on the player camera manager's field of view
	UPROPERTY(config, EditAnywhere, Category="MouseProperties", AdvancedDisplay)
	uint32 bEnableFOVScaling:1;

	// The scaling value to multiply the field of view by
	UPROPERTY(config, EditAnywhere, Category="MouseProperties", AdvancedDisplay, meta=(editcondition="bEnableFOVScaling"))
	float FOVScale;

	/** If a key is pressed twice in this amount of time it is considered a "double click" */
	UPROPERTY(config, EditAnywhere, Category="MouseProperties", AdvancedDisplay)
	float DoubleClickTime;

	/** Controls if the viewport will capture the mouse on Launch of the application */
	UPROPERTY(config, EditAnywhere, Category = "ViewportProperties")
	bool bCaptureMouseOnLaunch;
	
	/** The default mouse capture mode for the game viewport */
	UPROPERTY(config, EditAnywhere, Category = "ViewportProperties")
	EMouseCaptureMode DefaultViewportMouseCaptureMode;

	/** The default mouse lock state when the viewport acquires capture */
	UPROPERTY(config)
	bool bDefaultViewportMouseLock_DEPRECATED;

	/** The default mouse lock state behavior when the viewport acquires capture */
	UPROPERTY(config, EditAnywhere, Category = "ViewportProperties")
	EMouseLockMode DefaultViewportMouseLockMode;

	/** List of Action Mappings */
	UPROPERTY(config, EditAnywhere, Category="Bindings")
	TArray<struct FInputActionKeyMapping> ActionMappings;

	/** List of Axis Mappings */
	UPROPERTY(config, EditAnywhere, Category="Bindings")
	TArray<struct FInputAxisKeyMapping> AxisMappings;

	/** Should the touch input interface be shown always, or only when the platform has a touch screen? */
	UPROPERTY(config, EditAnywhere, Category="Mobile")
	bool bAlwaysShowTouchInterface;

	/** Whether or not to show the console on 4 finger tap, on mobile platforms */
	UPROPERTY(config, EditAnywhere, Category="Mobile")
	bool bShowConsoleOnFourFingerTap;

	/** The default on-screen touch input interface for the game (can be null to disable the onscreen interface) */
	UPROPERTY(config, EditAnywhere, Category="Mobile", meta=(AllowedClasses="TouchInterface"))
	FSoftObjectPath DefaultTouchInterface;

	/** The key which opens the console. */
	UPROPERTY(config)
	FKey ConsoleKey_DEPRECATED;

	/** The keys which open the console. */
	UPROPERTY(config, EditAnywhere, Category="Console")
	TArray<FKey> ConsoleKeys;

	// UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PostReloadConfig( class UProperty* PropertyThatWasLoaded ) override;
#endif

	virtual void PostInitProperties() override;
	// End of UObject interface

	/** Returns the game local input settings (action mappings, axis mappings, etc...) */
	UFUNCTION(BlueprintPure, Category = Settings)
	static UInputSettings* GetInputSettings();

	/** Programmatically add an action mapping to the project defaults */
	UFUNCTION(BlueprintCallable, Category = Settings)
	void AddActionMapping(const FInputActionKeyMapping& KeyMapping, bool bForceRebuildKeymaps = true);

	UFUNCTION(BlueprintPure, Category = Settings)
	void GetActionMappingByName(const FName InActionName, TArray<FInputActionKeyMapping>& OutMappings) const;

	/** Programmatically remove an action mapping to the project defaults */
	UFUNCTION(BlueprintCallable, Category = Settings)
	void RemoveActionMapping(const FInputActionKeyMapping& KeyMapping, bool bForceRebuildKeymaps = true);

	/** Programmatically add an axis mapping to the project defaults */
	UFUNCTION(BlueprintCallable, Category = Settings)
	void AddAxisMapping(const FInputAxisKeyMapping& KeyMapping, bool bForceRebuildKeymaps = true);

	/** Retrieve all axis mappings by a certain name. */
	UFUNCTION(BlueprintPure, Category = Settings)
	void GetAxisMappingByName(const FName InAxisName, TArray<FInputAxisKeyMapping>& OutMappings) const;

	/** Programmatically remove an axis mapping to the project defaults */
	UFUNCTION(BlueprintCallable, Category = Settings)
	void RemoveAxisMapping(const FInputAxisKeyMapping& KeyMapping, bool bForceRebuildKeymaps = true);

	/** Flush the current mapping values to the config file */
	UFUNCTION(BlueprintCallable, Category=Settings)
	void SaveKeyMappings();

	/** Populate a list of all defined action names */
	UFUNCTION(BlueprintCallable, Category = Settings)
	void GetActionNames(TArray<FName>& ActionNames) const;

	/** Populate a list of all defined axis names */
	UFUNCTION(BlueprintCallable, Category = Settings)
	void GetAxisNames(TArray<FName>& AxisNames) const;

	/** When changes are made to the default mappings, push those changes out to PlayerInput key maps */
	UFUNCTION(BlueprintCallable, Category = Settings)
	void ForceRebuildKeymaps();

private:
	void PopulateAxisConfigs();
};
