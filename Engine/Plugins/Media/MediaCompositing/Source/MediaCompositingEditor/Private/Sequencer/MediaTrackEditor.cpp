// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MediaTrackEditor.h"

#include "ContentBrowserModule.h"
#include "EditorStyleSet.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IContentBrowserSingleton.h"
#include "ISequencer.h"
#include "ISequencerObjectChangeListener.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MovieSceneBinding.h"
#include "MovieSceneMediaSection.h"
#include "MovieSceneMediaTrack.h"
#include "MovieSceneToolsUserSettings.h"
#include "MediaPlane.h"
#include "MediaPlaneComponent.h"
#include "SequencerUtilities.h"
#include "TrackEditorThumbnail/TrackEditorThumbnailPool.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"

#include "MediaThumbnailSection.h"


#define LOCTEXT_NAMESPACE "FMediaTrackEditor"


/* FMediaTrackEditor static functions
 *****************************************************************************/

TArray<FAnimatedPropertyKey, TInlineAllocator<1>> FMediaTrackEditor::GetAnimatedPropertyTypes()
{
	return TArray<FAnimatedPropertyKey, TInlineAllocator<1>>({ FAnimatedPropertyKey::FromObjectType(UMediaPlayer::StaticClass()) });
}


/* FMediaTrackEditor structors
 *****************************************************************************/

FMediaTrackEditor::FMediaTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer)
	, PropertyKey(FAnimatedPropertyKey::FromObjectType(UMediaPlayer::StaticClass()))
{
	ThumbnailPool = MakeShared<FTrackEditorThumbnailPool>(InSequencer);
	OnPropertyChangedHandle = InSequencer->GetObjectChangeListener().GetOnAnimatablePropertyChanged(PropertyKey).AddRaw(this, &FMediaTrackEditor::OnAnimatedPropertyChanged);
}


FMediaTrackEditor::~FMediaTrackEditor()
{
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();

	if (SequencerPtr.IsValid())
	{
		SequencerPtr->GetObjectChangeListener().GetOnAnimatablePropertyChanged(PropertyKey).Remove(OnPropertyChangedHandle);
	}
}


/* FMovieSceneTrackEditor interface
 *****************************************************************************/

UMovieSceneTrack* FMediaTrackEditor::AddTrack(UMovieScene* FocusedMovieScene, const FGuid& ObjectHandle, TSubclassOf<class UMovieSceneTrack> TrackClass, FName UniqueTypeName)
{
	UMovieSceneTrack* Track = FocusedMovieScene->AddTrack(TrackClass, ObjectHandle);
	if (UMovieSceneMediaTrack* MediaTrack = Cast<UMovieSceneMediaTrack>(Track))
	{
		MediaTrack->UniqueTrackName = UniqueTypeName;
	}
	return Track;
}

void FMediaTrackEditor::OnAnimatedPropertyChanged(const FPropertyChangedParams& PropertyChangedParams)
{
	const UProperty* ChangedProperty = PropertyChangedParams.PropertyPath.GetLeafMostProperty().Property.Get();
	if (!ChangedProperty)
	{
		return;
	}

	FText DisplayText = ChangedProperty->GetDisplayNameText();
	FName UniqueName(*PropertyChangedParams.PropertyPath.ToString(TEXT(".")));

	// Set up the appropriate name for the track from an array index if necessary
	for (int32 PropertyIndex = PropertyChangedParams.PropertyPath.GetNumProperties() - 1; PropertyIndex >= 0; --PropertyIndex)
	{
		const FPropertyInfo& Info = PropertyChangedParams.PropertyPath.GetPropertyInfo(PropertyIndex);
		const UArrayProperty* ParentArrayProperty = PropertyIndex > 0 ? Cast<UArrayProperty>(PropertyChangedParams.PropertyPath.GetPropertyInfo(PropertyIndex-1).Property.Get()) : nullptr;

		UProperty* Property = Info.Property.Get();
		if (Property && Info.ArrayIndex != INDEX_NONE)
		{
			DisplayText = FText::Format(LOCTEXT("MediaTrackNameFormat", "{0} ({1}[{2}])"),
				ChangedProperty->GetDisplayNameText(),
				(ParentArrayProperty ? ParentArrayProperty : Property)->GetDisplayNameText(),
				FText::AsNumber(Info.ArrayIndex)
				);
			break;
		}
	}

	for (UObject* Object : PropertyChangedParams.ObjectsThatChanged)
	{
		FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject(Object);
		if (!ensure(HandleResult.Handle.IsValid()))
		{
			continue;
		}
		
		FFindOrCreateTrackResult TrackResult = FindOrCreateTrackForObject(HandleResult.Handle, UMovieSceneMediaTrack::StaticClass(), UniqueName);
		UMovieSceneMediaTrack* MediaTrack = Cast<UMovieSceneMediaTrack>(TrackResult.Track);
		
		if (TrackResult.bWasCreated)
		{
			MediaTrack->SetPropertyNameAndPath(ChangedProperty->GetFName(), PropertyChangedParams.PropertyPath.ToString(TEXT(".")));
			MediaTrack->SetDisplayName(DisplayText);
		}

		GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
	}
}

void FMediaTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	// We only know how to add specific properties for image planes and components. Anything else must be keyed through the generic MediaPlayer property
	if (!ObjectClass->IsChildOf(AMediaPlane::StaticClass()) && !ObjectClass->IsChildOf(UMediaPlaneComponent::StaticClass()))
	{
		return;
	}

	// Find the spawned object or its template
	UObject* Object = GetSequencer()->FindSpawnedObjectOrTemplate(ObjectBinding);
	if (!Object)
	{
		return;
	}

	auto GeneratePropertyPath = [this](UMediaPlaneComponent* MediaPlaneComponent)
	{
		check(MediaPlaneComponent);

		UStructProperty* MediaPlanesProperty = MediaPlaneComponent->GetMediaPlaneProperty();

		check(MediaPlanesProperty);

		TSharedRef<FPropertyPath> Path = FPropertyPath::CreateEmpty();
		Path->AddProperty(FPropertyInfo(MediaPlanesProperty));

		return Path;
	};

	auto AddNewTrack = [this](UMediaPlaneComponent* MediaPlaneComponent, TSharedRef<FPropertyPath> PropertyPath)
	{
		check(MediaPlaneComponent);

		FPropertyChangedParams ChangedParams(TArray<UObject*>({ MediaPlaneComponent }), *PropertyPath, NAME_None, ESequencerKeyMode::ManualKeyForced);
		this->OnAnimatedPropertyChanged(ChangedParams);
	};

	// Try and root out an image plane component
	UMediaPlaneComponent* Component = Cast<UMediaPlaneComponent>(Object);
	if (AMediaPlane* MediaPlane = Cast<AMediaPlane>(Object))
	{
		Component = Cast<UMediaPlaneComponent>(MediaPlane->GetRootComponent());
	}

	if (Component)
	{
		UMovieScene* MovieScene = GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene();

		TSharedRef<FPropertyPath> Path = GeneratePropertyPath(Component);

		bool bCanAddTrack = true;

		const FMovieSceneBinding* Binding = MovieScene->GetBindings().FindByPredicate([=](const FMovieSceneBinding& InBinding){ return InBinding.GetObjectGuid() == ObjectBinding; });
		if (Binding)
		{
			FString PredicatePath = Path->ToString(TEXT("."));

			bCanAddTrack = !Binding->GetTracks().ContainsByPredicate(
				[&](UMovieSceneTrack* Track)
				{
					UMovieSceneMediaTrack* MediaTrack = Cast<UMovieSceneMediaTrack>(Track);
					return MediaTrack && MediaTrack->GetPropertyPath() == PredicatePath;
				}
			);
		}

		MenuBuilder.AddMenuEntry(
			LOCTEXT("AddMediaTrack_Text", "Media"),
			LOCTEXT("AddMediaTrack_Tip", "Adds a media track that controls the media presented to the media plane."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda(AddNewTrack, Component, Path),
				FCanExecuteAction::CreateLambda([=]{ return bCanAddTrack; })
			)
		);
	}
}

TSharedPtr<SWidget> FMediaTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	UMovieSceneMediaTrack* MediaTrack = Cast<UMovieSceneMediaTrack>(Track);
	if (!MediaTrack)
	{
		return SNullWidget::NullWidget;
	}

	auto CreatePicker = [this, MediaTrack]
	{
		FAssetPickerConfig AssetPickerConfig;
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw(this, &FMediaTrackEditor::AddNewSection, MediaTrack);
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;

		AssetPickerConfig.Filter.bRecursiveClasses = true;
		AssetPickerConfig.Filter.ClassNames.Add(UMediaSource::StaticClass()->GetFName());

		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		TSharedRef<SBox> Picker = SNew(SBox)
			.WidthOverride(300.0f)
			.HeightOverride(300.f)
			[
				ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
			];

		return Picker;
	};

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			FSequencerUtilities::MakeAddButton(LOCTEXT("AddMediaSection_Text", "Media"), FOnGetContent::CreateLambda(CreatePicker), Params.NodeIsHovered)
		];
}

void FMediaTrackEditor::AddNewSection(const FAssetData& AssetData, UMovieSceneMediaTrack* Track)
{
	UMediaSource* MediaSource = Cast<UMediaSource>(AssetData.GetAsset());
	if (!MediaSource)
	{
		return;
	}

	check(Track);

	float TimeToStart = GetSequencer()->GetLocalTime();

	UMediaPlayer* TransientPlayer = NewObject<UMediaPlayer>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UMediaPlayer::StaticClass()));
	TransientPlayer->AddToRoot();

	TRange<float> SectionRange(TimeToStart, TimeToStart + 1.f);

	if (TransientPlayer->OpenSource(MediaSource))
	{
		// @todo: Make this more robust for all media types
		FTimespan Length = TransientPlayer->GetDuration();
		if (Length.GetTotalSeconds() > 0)
		{
			SectionRange = TRange<float>(TimeToStart, TRangeBound<float>::Inclusive(TimeToStart + float(Length.GetTicks()) / ETimespan::TicksPerSecond));
		}

		TransientPlayer->Close();
	}

	TransientPlayer->RemoveFromRoot();

	// Find a row to put this video on
	TArray<int32> InvalidRows;
	for (UMovieSceneSection* Section : Track->GetAllSections())
	{
		if (Section->GetRange().Overlaps(SectionRange))
		{
			InvalidRows.Add(Section->GetRowIndex());
		}
	}

	int32 BestRow = 0;
	for ( ; ; ++BestRow)
	{
		if (!InvalidRows.Contains(BestRow))
		{
			break;
		}
	}

	UMovieSceneMediaSection* Section = CastChecked<UMovieSceneMediaSection>(Track->CreateNewSection());
	Section->SetRange(SectionRange);
	Section->SetRowIndex(BestRow);
	Section->SetMediaSource(MediaSource);
	Track->AddSection(*Section);

	GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
}

void FMediaTrackEditor::Tick(float DeltaTime)
{
	ThumbnailPool->DrawThumbnails();
}

TSharedRef<ISequencerSection> FMediaTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	check(SupportsType(SectionObject.GetOuter()->GetClass()));
	return MakeShared<FMediaThumbnailSection>(*CastChecked<UMovieSceneMediaSection>(&SectionObject), ThumbnailPool, GetSequencer());
}

bool FMediaTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const
{
	return TrackClass.Get() && TrackClass.Get()->IsChildOf(UMovieSceneMediaTrack::StaticClass());
}


#undef LOCTEXT_NAMESPACE
