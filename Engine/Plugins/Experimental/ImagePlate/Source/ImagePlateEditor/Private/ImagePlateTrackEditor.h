// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneTrackEditor.h"
#include "AnimatedPropertyKey.h"
#include "Engine/Texture.h"

class ISequencer;
struct FAssetData;
class FTrackEditorThumbnailPool;
class UMovieSceneImagePlateTrack;

class FImagePlateTrackEditor : public FMovieSceneTrackEditor
{
public:

	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer) { return MakeShared<FImagePlateTrackEditor>(OwningSequencer); }

	FImagePlateTrackEditor(TSharedRef<ISequencer> InSequencer);
	~FImagePlateTrackEditor();

	static TArray<FAnimatedPropertyKey, TInlineAllocator<1>> GetAnimatedPropertyTypes()
	{
		return TArray<FAnimatedPropertyKey, TInlineAllocator<1>>({ FAnimatedPropertyKey::FromObjectType(UTexture::StaticClass()) });
	}

	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const override;
	virtual void Tick(float DeltaTime) override;
	virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass) override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;
	virtual UMovieSceneTrack* AddTrack(UMovieScene* FocusedMovieScene, const FGuid& ObjectHandle, TSubclassOf<class UMovieSceneTrack> TrackClass, FName UniqueTypeName) override;
	
private:

	void OnAnimatedPropertyChanged(const FPropertyChangedParams& PropertyChangedParams);

	void AddNewSection(const FAssetData& Asset, UMovieSceneImagePlateTrack* Track);

private:
	// FAnimatedPropertyKey PropertyKey;
	// FDelegateHandle OnPropertyChangedHandle;

	TSharedPtr<FTrackEditorThumbnailPool> ThumbnailPool;
};
