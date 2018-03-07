// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "UObject/SoftObjectPath.h"
#include "ClassViewerProjectSettings.generated.h"


/**
 * Implements the settings for the Class Viewer Project Settings
 */
UCLASS(config=Engine, defaultconfig)
class CLASSVIEWER_API UClassViewerProjectSettings : public UObject
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	/** The base directories to be considered Internal Only for the class picker.*/
	UPROPERTY(EditAnywhere, config, Category = ClassVisibilityManagement, meta = (DisplayName = "List of directories to consider Internal Only.", ContentDir, LongPackageName))
	TArray<FDirectoryPath> InternalOnlyPaths;

	/** The base classes to be considered Internal Only for the class picker.*/
	UPROPERTY(EditAnywhere, config, Category = ClassVisibilityManagement, meta = (MetaClass = "Object", DisplayName = "List of base classes to consider Internal Only.", AllowAbstract = "true", ShowTreeView, HideViewOptions))
	TArray<FSoftClassPath> InternalOnlyClasses;
#endif
};
