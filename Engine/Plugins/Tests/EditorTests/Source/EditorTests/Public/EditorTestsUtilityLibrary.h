// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/MeshMerging.h"

#include "EditorTestsUtilityLibrary.generated.h"

class UMaterialOptions;
struct FMeshMergingSettings;

/** Blueprint library for altering and analyzing animation / skeletal data */
UCLASS()
class UEditorTestsUtilityLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Bakes out material in-place for the given set of static mesh components using the MaterialMergeOptions */
	UFUNCTION(BlueprintCallable, Category = "MeshMergingLibrary|Test")
	static void BakeMaterialsForComponent(UStaticMeshComponent* InStaticMeshComponent, const UMaterialOptions* MaterialOptions, const UMaterialMergeOptions* MaterialMergeOptions);

	/** Merges meshes and bakes out materials into a atlas-material for the given set of static mesh components using the MergeSettings */
	UFUNCTION(BlueprintCallable, Category = "MeshMergingLibrary|Test")
	static void MergeStaticMeshComponents(TArray<UStaticMeshComponent*> InStaticMeshComponents, const FMeshMergingSettings& MergeSettings, const bool bReplaceActors, TArray<int32>& OutLODIndices);
};