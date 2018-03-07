// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "Engine/DeveloperSettings.h"
#include "UMGEditorProjectSettings.generated.h"

USTRUCT()
struct FDebugResolution
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Resolution)
	int32 Width;

	UPROPERTY(EditAnywhere, Category = Resolution)
	int32 Height;

	UPROPERTY(EditAnywhere, Category=Resolution)
	FString Description;

	UPROPERTY(EditAnywhere, Category = Resolution)
	FLinearColor Color;
};

/**
 * Implements the settings for the UMG Editor Project Settings
 */
UCLASS(config=Editor, defaultconfig)
class UMGEDITOR_API UUMGEditorProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UUMGEditorProjectSettings();

#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
#endif

public:

	/**
	 * As a precaution, the slow construction widget tree is cooked in case some non-fast construct widget
	 * needs it.  If your project does not need the slow path at all, then disable this, so that you can recoop
	 * that memory.
	 */
	UPROPERTY(EditAnywhere, config, Category=Compiler)
	bool bCookSlowConstructionWidgetTree;

	UPROPERTY(EditAnywhere, config, Category=Designer)
	TArray<FDebugResolution> DebugResolutions;

	UPROPERTY(EditAnywhere, config, Category="Class Filtering")
	bool bShowWidgetsFromEngineContent;

	UPROPERTY(EditAnywhere, config, Category="Class Filtering")
	bool bShowWidgetsFromDeveloperContent;

	UPROPERTY(EditAnywhere, config, AdvancedDisplay, Category="Class Filtering")
	TArray<FString> CategoriesToHide;

	UPROPERTY(EditAnywhere, config, Category = "Class Filtering", meta = (MetaClass = "Widget"))
	TArray<FSoftClassPath> WidgetClassesToHide;
};
