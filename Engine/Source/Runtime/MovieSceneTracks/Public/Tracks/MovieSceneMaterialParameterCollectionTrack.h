// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneMaterialTrack.h"
#include "MovieSceneMaterialParameterCollectionTrack.generated.h"

class UMaterialParameterCollection;

/**
 * Handles manipulation of material parameter collections in a movie scene.
 */
UCLASS()
class MOVIESCENETRACKS_API UMovieSceneMaterialParameterCollectionTrack
	: public UMovieSceneMaterialTrack
{
public:

	GENERATED_BODY()

	/** The material parameter collection to manipulate */
	UPROPERTY(EditAnywhere, Category=General, DisplayName="Material Parameter Collection")
	UMaterialParameterCollection* MPC;

	UMovieSceneMaterialParameterCollectionTrack(const FObjectInitializer& ObjectInitializer);

	virtual UMovieSceneSection* CreateNewSection() override;
	virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const override;
#if WITH_EDITORONLY_DATA
	virtual FText GetDefaultDisplayName() const override;
#endif
};