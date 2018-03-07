// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "EditorTutorial.h"
#include "TutorialSettings.generated.h"

/** Per-project tutorial settings */
UCLASS(config=Editor)
class UTutorialSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Categories for tutorials */
	UPROPERTY(Config, EditAnywhere, Category="Tutorials")
	TArray<FTutorialCategory> Categories;

	/** Tutorial to start on project startup */
	UPROPERTY(Config, EditAnywhere, Category="Tutorials", meta=(MetaClass="EditorTutorial"))
	FSoftClassPath StartupTutorial;
};
