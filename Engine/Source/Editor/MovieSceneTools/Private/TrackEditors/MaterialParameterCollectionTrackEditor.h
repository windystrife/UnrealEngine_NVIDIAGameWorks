// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneTrackEditor.h"

struct FCollectionScalarParameter;
struct FCollectionVectorParameter;
struct FAssetData;
class UMovieSceneMaterialParameterCollectionTrack;

/**
 * Track editor for material parameter collections.
 */
class MOVIESCENETOOLS_API FMaterialParameterCollectionTrackEditor
	: public FMovieSceneTrackEditor
{
public:

	/** Constructor. */
	FMaterialParameterCollectionTrackEditor(TSharedRef<ISequencer> InSequencer);

	/** Factory function */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

public:

	//~ ISequencerTrackEditor interface
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override;
	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override;
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) override;
	virtual void BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track) override;

private:

	/** Provides the contents of the add parameter menu. */
	TSharedRef<SWidget> OnGetAddParameterMenuContent(UMovieSceneMaterialParameterCollectionTrack* MaterialTrack);

	void AddScalarParameter(UMovieSceneMaterialParameterCollectionTrack* Track, FCollectionScalarParameter Parameter);
	void AddVectorParameter(UMovieSceneMaterialParameterCollectionTrack* Track, FCollectionVectorParameter Parameter);

	void AddTrackToSequence(const FAssetData& InAssetData);
};
