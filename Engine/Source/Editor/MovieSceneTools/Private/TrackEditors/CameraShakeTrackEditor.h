// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "Widgets/SWidget.h"
#include "ISequencer.h"
#include "MovieSceneTrack.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "MovieSceneTrackEditor.h"
#include "Camera/CameraShake.h"

struct FAssetData;
class FMenuBuilder;

/**
* Tools for playing a camera shake
*/
class FCameraShakeTrackEditor : public FMovieSceneTrackEditor
{
public:

	/**
	* Constructor
	*
	* @param InSequencer The sequencer instance to be used by this tool
	*/
	FCameraShakeTrackEditor(TSharedRef<ISequencer> InSequencer);

	/** Virtual destructor. */
	virtual ~FCameraShakeTrackEditor() { }

	/**
	* Creates an instance of this class.  Called by a sequencer
	*
	* @param OwningSequencer The sequencer instance to be used by this tool
	* @return The new instance of this class
	*/
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

public:

	// ISequencerTrackEditor interface
	virtual void AddKey(const FGuid& ObjectGuid) override;
	virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass) override;
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;

private:

	/** Delegate for AnimatablePropertyChanged in AddKey */
	FKeyPropertyResult AddKeyInternal(float KeyTime, const TArray<TWeakObjectPtr<UObject>> Objects, TSubclassOf<UCameraShake> ShakeClass);

	/** Animation sub menu */
	TSharedRef<SWidget> BuildCameraShakeSubMenu(FGuid ObjectBinding);
	void AddCameraShakeSubMenu(FMenuBuilder& MenuBuilder, FGuid ObjectBinding);

	/** Animation asset selected */
	void OnCameraShakeAssetSelected(const FAssetData& AssetData, FGuid ObjectBinding);

	class UCameraComponent* AcquireCameraComponentFromObjectGuid(const FGuid& Guid);
};


