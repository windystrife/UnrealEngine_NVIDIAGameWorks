// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ImagePlateTrackEditor.h"
#include "ImagePlateFileSequence.h"

#include "ISequencer.h"
#include "ISequencerObjectChangeListener.h"
#include "MovieSceneImagePlateTrack.h"
#include "SequencerSectionPainter.h"
#include "MovieSceneToolsUserSettings.h"
#include "MovieSceneImagePlateSection.h"
#include "TrackEditorThumbnail/TrackEditorThumbnail.h"
#include "TrackEditorThumbnail/TrackEditorThumbnailPool.h"
#include "EditorStyleSet.h"
#include "Sections/ThumbnailSection.h"
#include "GCObject.h"
#include "ImagePlate.h"
#include "ImagePlateComponent.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "SequencerUtilities.h"
#include "MovieSceneBinding.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DDynamic.h"
#include "Rendering/DrawElements.h"

#include "Widgets/SBoxPanel.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "AssetData.h"
#include "RHI.h"
#include "TextureResource.h"

#define LOCTEXT_NAMESPACE "ImagePlateTrackEditor"


class FImagePlateSection : public FThumbnailSection, public ICustomThumbnailClient, FGCObject
{
public:

	FImagePlateSection(UMovieSceneImagePlateSection& InSection, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, TSharedPtr<ISequencer> InSequencer)
		: FThumbnailSection(InSequencer, InThumbnailPool, this, InSection)
	{
		RenderTexture = nullptr;
		TimeSpace = ETimeSpace::Local;
	}
	
	virtual FText GetSectionTitle() const override
	{
		UMovieSceneImagePlateSection* ImagePlateSection = CastChecked<UMovieSceneImagePlateSection>(Section);
		return ImagePlateSection->FileSequence ? FText::FromString(ImagePlateSection->FileSequence->SequencePath.Path) : LOCTEXT("NoSequence", "Empty");
	}

	virtual void SetSingleTime(float GlobalTime) override
	{
		UMovieSceneImagePlateSection* ImagePlateSection = CastChecked<UMovieSceneImagePlateSection>(Section);
		if (ImagePlateSection)
		{
			ImagePlateSection->SetThumbnailReferenceOffset(GlobalTime - ImagePlateSection->GetStartTime());
		}
	}

	// virtual TSharedRef<SWidget> GenerateSectionWidget() override
	// {
	// 	return SNew(SBox)
	// 		.HAlign(HAlign_Left)
	// 		.VAlign(VAlign_Top)
	// 		.Padding(GetContentPadding())
	// 		[
	// 			SNew(STextBlock)
	// 			.Text(this, &FImagePlateSection::GetSectionTitle)
	// 			.ShadowOffset(FVector2D(1,1))
	// 		];
	// }

	virtual void AddReferencedObjects( FReferenceCollector& Collector )
	{
		Collector.AddReferencedObject(RenderTexture);
	}

	virtual float GetSectionHeight() const override
	{
		// Make space for the film border
		return FThumbnailSection::GetSectionHeight() + 2*9.f;
	}

	virtual FMargin GetContentPadding() const override
	{
		return FMargin(8.f, 15.f);
	}
	
	virtual void Tick(const FGeometry& AllottedGeometry, const FGeometry& ClippedGeometry, const double InCurrentTime, const float InDeltaTime)
	{
		UMovieSceneImagePlateSection* ImagePlateSection = Cast<UMovieSceneImagePlateSection>(Section);
		if (ImagePlateSection)
		{
			if (GetDefault<UMovieSceneUserThumbnailSettings>()->bDrawSingleThumbnails)
			{
				ThumbnailCache.SetSingleReferenceFrame(ImagePlateSection->GetThumbnailReferenceOffset());
			}
			else
			{
				ThumbnailCache.SetSingleReferenceFrame(TOptional<float>());
			}
		}

		FThumbnailSection::Tick(AllottedGeometry, ClippedGeometry, InCurrentTime, InDeltaTime);
	}

	virtual int32 OnPaintSection(FSequencerSectionPainter& InPainter) const override
	{
		static const FSlateBrush* FilmBorder = FEditorStyle::GetBrush("Sequencer.Section.FilmBorder");

		InPainter.LayerId = InPainter.PaintSectionBackground();

		FVector2D LocalSectionSize = InPainter.SectionGeometry.GetLocalSize();

		FSlateDrawElement::MakeBox(
			InPainter.DrawElements,
			InPainter.LayerId++,
			InPainter.SectionGeometry.ToPaintGeometry(FVector2D(LocalSectionSize.X-2.f, 7.f), FSlateLayoutTransform(FVector2D(1.f, 4.f))),
			FilmBorder,
			InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect
		);

		FSlateDrawElement::MakeBox(
			InPainter.DrawElements,
			InPainter.LayerId++,
			InPainter.SectionGeometry.ToPaintGeometry(FVector2D(LocalSectionSize.X-2.f, 7.f), FSlateLayoutTransform(FVector2D(1.f, LocalSectionSize.Y - 11.f))),
			FilmBorder,
			InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect
		);

		return FThumbnailSection::OnPaintSection(InPainter);
	}

	virtual void Setup() override
	{
		UMovieSceneImagePlateSection* ImagePlateSection = CastChecked<UMovieSceneImagePlateSection>(Section);
		FileSequence = ImagePlateSection->FileSequence;

		if (ImagePlateSection->FileSequence)
		{
			ThumbnailLoader = ImagePlateSection->FileSequence->GetAsyncCache();
			RenderTexture = NewObject<UTexture2DDynamic>(GetTransientPackage(), FName(), RF_Transient);
			RenderTexture->Init(256, 256, PF_R8G8B8A8);
		}
		else
		{
			RenderTexture = nullptr;
			ThumbnailLoader.Reset();
		}
	}

	virtual void Draw(FTrackEditorThumbnail& TrackEditorThumbnail) override
	{
		float SequenceTime = FMath::Max(0.f, TrackEditorThumbnail.GetEvalPosition());

		UMovieSceneImagePlateSection* ImagePlateSection = CastChecked<UMovieSceneImagePlateSection>(Section);
		if (ImagePlateSection->FileSequence)
		{
			if (FileSequence != ImagePlateSection->FileSequence)
			{
				FileSequence = ImagePlateSection->FileSequence;
				ThumbnailLoader = ImagePlateSection->FileSequence->GetAsyncCache();
			}
		}
		else
		{
			ThumbnailLoader.Reset();
			return;
		}

		FImagePlateSourceFrame FrameData = ThumbnailLoader->RequestFrame(SequenceTime, 0, 0).Get();

		if (FrameData.IsValid())
		{
			// Wait for the texture to render
			FrameData.CopyTo(RenderTexture).Get();

			FTexture2DRHIRef Texture2DRHI = RenderTexture && RenderTexture->Resource && RenderTexture->Resource->TextureRHI.IsValid() ? RenderTexture->Resource->TextureRHI->GetTexture2D() : nullptr;
			if (Texture2DRHI.IsValid())
			{
				// Resolve the media player texture to the track editor thumbnail
				TrackEditorThumbnail.CopyTextureIn(Texture2DRHI);

				TSharedPtr<ISequencer> Sequencer = SequencerPtr.Pin();
				if (Sequencer.IsValid())
				{
					TrackEditorThumbnail.SetupFade(Sequencer->GetSequencerWidget());
				}
			}
		}
	}

private:

	UTexture2DDynamic* RenderTexture;
	TOptional<FImagePlateAsyncCache> ThumbnailLoader;
	TWeakObjectPtr<UImagePlateFileSequence> FileSequence;
};

FImagePlateTrackEditor::FImagePlateTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer)
{
	ThumbnailPool = MakeShared<FTrackEditorThumbnailPool>(InSequencer);

	for (auto& PropertyKey : GetAnimatedPropertyTypes())
	{
		InSequencer->GetObjectChangeListener().GetOnAnimatablePropertyChanged(PropertyKey).AddRaw(this, &FImagePlateTrackEditor::OnAnimatedPropertyChanged);
	}
}

FImagePlateTrackEditor::~FImagePlateTrackEditor()
{
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (SequencerPtr.IsValid())
	{
		for (auto& PropertyKey : GetAnimatedPropertyTypes())
		{
			SequencerPtr->GetObjectChangeListener().GetOnAnimatablePropertyChanged(PropertyKey).RemoveAll(this);
		}
	}
}

UMovieSceneTrack* FImagePlateTrackEditor::AddTrack(UMovieScene* FocusedMovieScene, const FGuid& ObjectHandle, TSubclassOf<class UMovieSceneTrack> TrackClass, FName UniqueTypeName)
{
	UMovieSceneTrack* Track = FocusedMovieScene->AddTrack(TrackClass, ObjectHandle);
	if (UMovieSceneImagePlateTrack* VideoTrack = Cast<UMovieSceneImagePlateTrack>(Track))
	{
		VideoTrack->UniqueTrackName = UniqueTypeName;
	}
	return Track;
}

void FImagePlateTrackEditor::OnAnimatedPropertyChanged(const FPropertyChangedParams& PropertyChangedParams)
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
			DisplayText = FText::Format(LOCTEXT("VideoTrackNameFormat", "{0} ({1}[{2}])"),
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
		
		FFindOrCreateTrackResult TrackResult = FindOrCreateTrackForObject(HandleResult.Handle, UMovieSceneImagePlateTrack::StaticClass(), UniqueName);
		UMovieSceneImagePlateTrack* ImagePlateTrack = Cast<UMovieSceneImagePlateTrack>(TrackResult.Track);
		
		if (TrackResult.bWasCreated)
		{
			ImagePlateTrack->SetPropertyNameAndPath(ChangedProperty->GetFName(), PropertyChangedParams.PropertyPath.ToString(TEXT(".")));
			ImagePlateTrack->SetDisplayName(DisplayText);
		}

		GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
	}
}

void FImagePlateTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	// We only know how to add specific properties for image plates and components. Anything else must be keyed through the generic MediaPlayer property
	if (!ObjectClass->IsChildOf(AImagePlate::StaticClass()) && !ObjectClass->IsChildOf(UImagePlateComponent::StaticClass()))
	{
		return;
	}

	// Find the spawned object or its template
	UObject* Object = GetSequencer()->FindSpawnedObjectOrTemplate(ObjectBinding);
	if (!Object)
	{
		return;
	}

	auto GeneratePropertyPath = [this](UImagePlateComponent* ImagePlateComponent)
	{
		check(ImagePlateComponent);

		UStructProperty* ImagePlateProperty = ImagePlateComponent->GetImagePlateProperty();
		UProperty* RenderTargetProperty = FindField<UProperty>(FImagePlateParameters::StaticStruct(), GET_MEMBER_NAME_CHECKED(FImagePlateParameters, RenderTexture));

		check(ImagePlateProperty);
		check(RenderTargetProperty);

		TSharedRef<FPropertyPath> Path = FPropertyPath::CreateEmpty();
		Path->AddProperty(FPropertyInfo(ImagePlateProperty));
		Path->AddProperty(FPropertyInfo(RenderTargetProperty));

		return Path;
	};

	auto AddNewTrack = [this](UImagePlateComponent* ImagePlateComponent, TSharedRef<FPropertyPath> PropertyPath)
	{
		check(ImagePlateComponent);

		FPropertyChangedParams ChangedParams(TArray<UObject*>({ ImagePlateComponent }), *PropertyPath, NAME_None, ESequencerKeyMode::ManualKeyForced);
		this->OnAnimatedPropertyChanged(ChangedParams);
	};

	// Try and root out an image plate component
	UImagePlateComponent* Component = Cast<UImagePlateComponent>(Object);
	if (AImagePlate* ImagePlate = Cast<AImagePlate>(Object))
	{
		Component = Cast<UImagePlateComponent>(ImagePlate->GetRootComponent());
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
					UMovieSceneImagePlateTrack* ImagePlateTrack = Cast<UMovieSceneImagePlateTrack>(Track);
					return ImagePlateTrack && ImagePlateTrack->GetPropertyPath() == PredicatePath;
				}
			);
		}

		MenuBuilder.AddMenuEntry(
			LOCTEXT("AddImagePlateTrack_Text", "Image Plate"),
			LOCTEXT("AddImagePlateTrack_Tip", "Adds an image plate track that controls media presented to the plate."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda(AddNewTrack, Component, Path),
				FCanExecuteAction::CreateLambda([=]{ return bCanAddTrack; })
			)
		);
	}
}

TSharedPtr<SWidget> FImagePlateTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	UMovieSceneImagePlateTrack* ImagePlateTrack = Cast<UMovieSceneImagePlateTrack>(Track);
	if (!ImagePlateTrack)
	{
		return SNullWidget::NullWidget;
	}

	auto CreatePicker = [this, ImagePlateTrack]
	{
		FAssetPickerConfig AssetPickerConfig;
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw(this, &FImagePlateTrackEditor::AddNewSection, ImagePlateTrack);
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;

		AssetPickerConfig.Filter.bRecursiveClasses = true;
		AssetPickerConfig.Filter.ClassNames.Add(UImagePlateFileSequence::StaticClass()->GetFName());

		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		TSharedRef<SBox> Picker = SNew(SBox)
			.WidthOverride(300.0f)
			.HeightOverride(300.f)
			[
				ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
			];

		return Picker;
	};

	return FSequencerUtilities::MakeAddButton(LOCTEXT("AddImagePlateSection_Text", "Image Sequence"), FOnGetContent::CreateLambda(CreatePicker), Params.NodeIsHovered);
}

void FImagePlateTrackEditor::AddNewSection(const FAssetData& AssetData, UMovieSceneImagePlateTrack* Track)
{
	UImagePlateFileSequence* FileSequence = Cast<UImagePlateFileSequence>(AssetData.GetAsset());
	if (!FileSequence)
	{
		return;
	}

	check(Track);

	const float TimeToStart = GetSequencer()->GetLocalTime();
	const float Length = FileSequence->Framerate <= 0.f ? 1.f : FMath::Max(0.f, FileSequence->GetAsyncCache().Length() / FileSequence->Framerate);

	TRange<float> SectionRange(TimeToStart, TimeToStart + Length);

	// Find a row to put this image sequence on
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

	UMovieSceneImagePlateSection* Section = CastChecked<UMovieSceneImagePlateSection>(Track->CreateNewSection());
	Section->SetRange(SectionRange);
	Section->SetRowIndex(BestRow);
	Track->AddSection(*Section);

	Section->FileSequence = FileSequence;

	GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
}

void FImagePlateTrackEditor::Tick(float DeltaTime)
{
	ThumbnailPool->DrawThumbnails();
}

TSharedRef<ISequencerSection> FImagePlateTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	check(SupportsType(SectionObject.GetOuter()->GetClass()));
	return MakeShared<FImagePlateSection>(*CastChecked<UMovieSceneImagePlateSection>(&SectionObject), ThumbnailPool, GetSequencer());
}

bool FImagePlateTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const
{
	return TrackClass.Get() && TrackClass.Get()->IsChildOf(UMovieSceneImagePlateTrack::StaticClass());
}


#undef LOCTEXT_NAMESPACE
