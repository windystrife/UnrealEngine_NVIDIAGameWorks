// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/InlineValue.h"
#include "MovieSceneNameableTrack.h"
#include "Compilation/MovieSceneSegmentCompiler.h"
#include "MovieSceneCameraAnimTrack.generated.h"

class UMovieSceneCameraAnimSection;
struct FMovieSceneEvaluationTrack;

/**
 * 
 */
UCLASS(MinimalAPI)
class UMovieSceneCameraAnimTrack : public UMovieSceneNameableTrack
{
	GENERATED_BODY()

public:
	virtual void AddNewCameraAnim(float KeyTime, class UCameraAnim* CameraAnim);
	void GetCameraAnimSectionsAtTime(float Time, TArray<UMovieSceneCameraAnimSection*>& OutSections);

public:

	// UMovieSceneTrack interface
	virtual bool HasSection(const UMovieSceneSection& Section) const override;
	virtual void AddSection(UMovieSceneSection& Section) override;
	virtual void RemoveSection(UMovieSceneSection& Section) override;
	virtual bool IsEmpty() const override;
	virtual TRange<float> GetSectionBoundaries() const override;
	virtual bool SupportsMultipleRows() const override { return true; }
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const override;
	virtual UMovieSceneSection* CreateNewSection() override;
	virtual void RemoveAllAnimationData() override;
	virtual TInlineValue<FMovieSceneSegmentCompilerRules> GetTrackCompilerRules() const override;
	virtual void PostCompile(FMovieSceneEvaluationTrack& OutTrack, const FMovieSceneTrackCompilerArgs& Args) const override;
	
#if WITH_EDITORONLY_DATA
	virtual FText GetDisplayName() const override;
#endif

private:
	/** List of all sections */
	UPROPERTY()
	TArray<UMovieSceneSection*> CameraAnimSections;
};
