// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/DeveloperSettings.h"
#include "MeshSimplificationSettings.generated.h"

UCLASS(config=Engine, defaultconfig, meta=(DisplayName="Mesh Simplification"))
class UMeshSimplificationSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

public:
	virtual FName GetContainerName() const override;
	virtual FName GetCategoryName() const override;
public:
	/** Mesh reduction plugin to use when simplifying mesh geometry */
	UPROPERTY(config, EditAnywhere, Category=General, meta=(ConsoleVariable="r.MeshReductionModule", DisplayName="Mesh Reduction Plugin", ConfigRestartRequired=true))
	FName MeshReductionModuleName;

	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

};
