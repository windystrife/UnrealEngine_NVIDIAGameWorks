// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigTrackEditor.h"
#include "MovieSceneControlRigSection.h"
#include "MovieSceneControlRigTrack.h"
#include "ClassViewerModule.h"
#include "ClassViewerFilter.h"
#include "FloatCurveKeyArea.h"
#include "ISectionLayoutBuilder.h"
#include "SequencerSectionPainter.h"
#include "ControlRig.h"
#include "SequencerUtilities.h"
#include "ControlRigSequence.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "LevelEditor.h"
#include "EditorStyleSet.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "MultiBoxBuilder.h"
#include "SlateApplication.h"
#include "SBox.h"

namespace ControlRigEditorConstants
{
	// @todo Sequencer Allow this to be customizable
	const uint32 AnimationTrackHeight = 20;
}

#define LOCTEXT_NAMESPACE "FControlRigTrackEditor"

/** Class for animation sections */
class FControlRigSection
	: public ISequencerSection
	, public TSharedFromThis<FControlRigSection>
{
public:

	/** Constructor. */
	FControlRigSection(UMovieSceneSection& InSection, TSharedRef<ISequencer> InSequencer)
		: Sequencer(InSequencer)
		, Section(*CastChecked<UMovieSceneControlRigSection>(&InSection))
	{
	}

	/** Virtual destructor. */
	virtual ~FControlRigSection() { }

public:

	// ISequencerSection interface

	virtual UMovieSceneSection* GetSectionObject() override
	{
		return &Section;
	}

	virtual FText GetSectionTitle() const override
	{
		if (Section.GetSequence() != nullptr)
		{
			return Section.GetSequence()->GetDisplayName();
		}
		return LOCTEXT("NoSequenceSection", "No Sequence");
	}

	virtual float GetSectionHeight() const override
	{
		return (float)ControlRigEditorConstants::AnimationTrackHeight;
	}

	virtual void GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const override
	{
		WeightArea = MakeShareable(new FFloatCurveKeyArea(&Section.Weight, &Section));

		LayoutBuilder.AddKeyArea("Weight", LOCTEXT("WeightArea", "Weight"), WeightArea.ToSharedRef());
	}

	virtual int32 OnPaintSection(FSequencerSectionPainter& InPainter) const override
	{
		int32 LayerId = InPainter.PaintSectionBackground();

		const float SectionSize = Section.GetTimeSize();

		if (SectionSize <= 0.0f)
		{
			return LayerId;
		}

		const float DrawScale = InPainter.SectionGeometry.Size.X / SectionSize;
		const ESlateDrawEffect DrawEffects = InPainter.bParentEnabled
			? ESlateDrawEffect::None
			: ESlateDrawEffect::DisabledEffect;

		UMovieScene* MovieScene = nullptr;
		FFloatRange PlaybackRange;
		if (Section.GetSequence() != nullptr)
		{
			MovieScene = Section.GetSequence()->GetMovieScene();
			PlaybackRange = MovieScene->GetPlaybackRange();
		}
		else
		{
			UMovieSceneTrack* MovieSceneTrack = CastChecked<UMovieSceneTrack>(Section.GetOuter());
			MovieScene = CastChecked<UMovieScene>(MovieSceneTrack->GetOuter());
			PlaybackRange = MovieScene->GetPlaybackRange();
		}

		// add box for the working size
		const float StartOffset = 1.0f/Section.Parameters.TimeScale * Section.Parameters.StartOffset;
		const float WorkingStart = -1.0f/Section.Parameters.TimeScale * PlaybackRange.GetLowerBoundValue() - StartOffset;
		const float WorkingSize = 1.0f/Section.Parameters.TimeScale * (MovieScene != nullptr ? MovieScene->GetEditorData().WorkingRange.Size<float>() : 1.0f);

		// add dark tint for left out-of-bounds & working range
		if (StartOffset < 0.0f)
		{
			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				++LayerId,
				InPainter.SectionGeometry.ToPaintGeometry(
					FVector2D(0.0f, 0.f),
					FVector2D(-StartOffset * DrawScale, InPainter.SectionGeometry.Size.Y)
				),
				FEditorStyle::GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				FLinearColor::Black.CopyWithNewOpacity(0.2f)
			);
		}

		// add green line for playback start
		if (StartOffset < 0)
		{
			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				++LayerId,
				InPainter.SectionGeometry.ToPaintGeometry(
					FVector2D(-StartOffset * DrawScale, 0.f),
					FVector2D(1.0f, InPainter.SectionGeometry.Size.Y)
				),
				FEditorStyle::GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				FColor(32, 128, 32)	// 120, 75, 50 (HSV)
			);
		}

		// add dark tint for right out-of-bounds & working range
		const float PlaybackEnd = 1.0f/Section.Parameters.TimeScale * PlaybackRange.Size<float>() - StartOffset;

		if (PlaybackEnd < SectionSize)
		{
			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				++LayerId,
				InPainter.SectionGeometry.ToPaintGeometry(
					FVector2D(PlaybackEnd * DrawScale, 0.f),
					FVector2D((SectionSize - PlaybackEnd) * DrawScale, InPainter.SectionGeometry.Size.Y)
				),
				FEditorStyle::GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				FLinearColor::Black.CopyWithNewOpacity(0.2f)
			);
		}

		// add red line for playback end
		if (PlaybackEnd <= SectionSize)
		{
			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				++LayerId,
				InPainter.SectionGeometry.ToPaintGeometry(
					FVector2D(PlaybackEnd * DrawScale, 0.f),
					FVector2D(1.0f, InPainter.SectionGeometry.Size.Y)
				),
				FEditorStyle::GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				FColor(128, 32, 32)	// 0, 75, 50 (HSV)
			);
		}

		return LayerId;
	}

	virtual FReply OnSectionDoubleClicked(const FGeometry& SectionGeometry, const FPointerEvent& MouseEvent, const FGuid& ObjectBinding) override
	{
		Sequencer.Pin()->FocusSequenceInstance(Section);

		return FReply::Handled();
	}

private:

	/** The sequencer we are editing in */
	TWeakPtr<ISequencer> Sequencer;

	/** The section we are visualizing */
	UMovieSceneControlRigSection& Section;

	/** Weight key areas. */
	mutable TSharedPtr<FFloatCurveKeyArea> WeightArea;
};

FControlRigTrackEditor::FControlRigTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FSubTrackEditor( InSequencer ) 
{ 
}

TSharedRef<ISequencerTrackEditor> FControlRigTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new FControlRigTrackEditor( InSequencer ) );
}

bool FControlRigTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	return Type == UMovieSceneControlRigTrack::StaticClass();
}

TSharedRef<ISequencerSection> FControlRigTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding )
{
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) );
	
	return MakeShareable( new FControlRigSection(SectionObject, GetSequencer().ToSharedRef()) );
}

void FControlRigTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	// do nothing
}

void FControlRigTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	if (ObjectClass->IsChildOf(USkeletalMeshComponent::StaticClass()) || ObjectClass->IsChildOf(AActor::StaticClass()))
	{
		const TSharedPtr<ISequencer> ParentSequencer = GetSequencer();

		UMovieSceneTrack* Track = nullptr;

		MenuBuilder.AddSubMenu(
			LOCTEXT("AddControlRig", "Animation ControlRig"), NSLOCTEXT("Sequencer", "AddControlRigTooltip", "Adds an animation ControlRig track."),
			FNewMenuDelegate::CreateRaw(this, &FControlRigTrackEditor::AddControlRigSubMenu, ObjectBinding, Track)
		);
	}
}

TSharedRef<SWidget> FControlRigTrackEditor::BuildControlRigSubMenu(FGuid ObjectBinding, UMovieSceneTrack* Track)
{
	FMenuBuilder MenuBuilder(true, nullptr);
	
	AddControlRigSubMenu(MenuBuilder, ObjectBinding, Track);

	return MenuBuilder.MakeWidget();
}

TSharedPtr<SWidget> FControlRigTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	if (ObjectBinding.IsValid())
	{
		// Create a container edit box
		return SNew(SHorizontalBox)

			// Add the sub sequence combo box
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				FSequencerUtilities::MakeAddButton(LOCTEXT("SubText", "Sequence"), FOnGetContent::CreateSP(this, &FControlRigTrackEditor::HandleAddSubSequenceComboButtonGetMenuContent, ObjectBinding, Track), Params.NodeIsHovered)
			];
	}
	else
	{
		return TSharedPtr<SWidget>();
	}
}

void FControlRigTrackEditor::BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track )
{
	
}

void FControlRigTrackEditor::AddControlRigSubMenu(FMenuBuilder& MenuBuilder, FGuid ObjectBinding, UMovieSceneTrack* Track)
{
	MenuBuilder.BeginSection(TEXT("ChooseSequence"), LOCTEXT("ChooseSequence", "Choose Sequence"));
	{
		FAssetPickerConfig AssetPickerConfig;
		{
			AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw(this, &FControlRigTrackEditor::OnSequencerAssetSelected, ObjectBinding, Track);
			AssetPickerConfig.bAllowNullSelection = false;
			AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
			AssetPickerConfig.Filter.bRecursiveClasses = true;
			AssetPickerConfig.Filter.ClassNames.Add(UControlRigSequence::StaticClass()->GetFName());
		}

		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

		TSharedPtr<SBox> MenuEntry = SNew(SBox)
			.WidthOverride(300.0f)
			.HeightOverride(300.0f)
			[
				ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
			];

		MenuBuilder.AddWidget(MenuEntry.ToSharedRef(), FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();
}

void FControlRigTrackEditor::OnSequencerAssetSelected(const FAssetData& AssetData, FGuid ObjectBinding, UMovieSceneTrack* Track)
{
	FSlateApplication::Get().DismissAllMenus();

	UObject* SelectedObject = AssetData.GetAsset();

	if (SelectedObject && SelectedObject->IsA(UControlRigSequence::StaticClass()))
	{
		UControlRigSequence* Sequence = CastChecked<UControlRigSequence>(AssetData.GetAsset());
		AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FControlRigTrackEditor::AddKeyInternal, ObjectBinding, Sequence, Track));
	}
}

FKeyPropertyResult FControlRigTrackEditor::AddKeyInternal(float KeyTime, FGuid ObjectBinding, UControlRigSequence* Sequence, UMovieSceneTrack* Track)
{
	FKeyPropertyResult KeyPropertyResult;
	bool bHandleCreated = false;
	bool bTrackCreated = false;
	bool bTrackModified = false;

	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	if (SequencerPtr.IsValid())
	{
		UObject* Object = SequencerPtr->FindSpawnedObjectOrTemplate(ObjectBinding);

		FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject(Object);
		FGuid ObjectHandle = HandleResult.Handle;
		KeyPropertyResult.bHandleCreated |= HandleResult.bWasCreated;
		if (ObjectBinding.IsValid())
		{
			if (!Track)
			{
				Track = AddTrack(GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene(), ObjectBinding, UMovieSceneControlRigTrack::StaticClass(), NAME_None);
				KeyPropertyResult.bTrackCreated = true;
			}

			if (ensure(Track))
			{
				Cast<UMovieSceneControlRigTrack>(Track)->AddNewControlRig(KeyTime, Sequence);
				KeyPropertyResult.bTrackModified = true;
			}
		}
	}

	return KeyPropertyResult;
}


TSharedRef<SWidget> FControlRigTrackEditor::HandleAddSubSequenceComboButtonGetMenuContent(FGuid ObjectBinding, UMovieSceneTrack* InTrack)
{
	FMenuBuilder MenuBuilder(true, nullptr);

	AddControlRigSubMenu(MenuBuilder, ObjectBinding, InTrack);

	return MenuBuilder.MakeWidget();
}

const FSlateBrush* FControlRigTrackEditor::GetIconBrush() const
{
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
