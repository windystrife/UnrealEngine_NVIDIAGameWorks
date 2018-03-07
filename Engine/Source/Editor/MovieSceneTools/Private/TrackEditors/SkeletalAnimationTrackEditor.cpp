// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/SkeletalAnimationTrackEditor.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "AssetData.h"
#include "Animation/AnimSequenceBase.h"
#include "Modules/ModuleManager.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "SequencerSectionPainter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "CommonMovieSceneTools.h"
#include "AssetRegistryModule.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "MatineeImportTools.h"
#include "Matinee/InterpTrackAnimControl.h"
#include "SequencerUtilities.h"
#include "FloatCurveKeyArea.h"
#include "ISectionLayoutBuilder.h"
#include "Classes/Animation/AnimMontage.h"
#include "Classes/Animation/AnimSequence.h"
#include "EditorStyleSet.h"


namespace SkeletalAnimationEditorConstants
{
	// @todo Sequencer Allow this to be customizable
	const uint32 AnimationTrackHeight = 20;
}

#define LOCTEXT_NAMESPACE "FSkeletalAnimationTrackEditor"


FSkeletalAnimationSection::FSkeletalAnimationSection( UMovieSceneSection& InSection )
	: Section(*CastChecked<UMovieSceneSkeletalAnimationSection>(&InSection))
	, InitialStartOffsetDuringResize(0.f)
	, InitialStartTimeDuringResize(0.f)
{ }


UMovieSceneSection* FSkeletalAnimationSection::GetSectionObject()
{ 
	return &Section;
}


FText FSkeletalAnimationSection::GetSectionTitle() const
{
	if (Section.Params.Animation != nullptr)
	{
		return FText::FromString( Section.Params.Animation->GetName() );
	}
	return LOCTEXT("NoAnimationSection", "No Animation");
}


float FSkeletalAnimationSection::GetSectionHeight() const
{
	return (float)SkeletalAnimationEditorConstants::AnimationTrackHeight;
}

void FSkeletalAnimationSection::GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const
{
	WeightArea = MakeShareable( new FFloatCurveKeyArea( &Section.Params.Weight, &Section ) );

	LayoutBuilder.AddKeyArea( "Weight", NSLOCTEXT( "FSkeletalAnimationSection", "WeightArea", "Weight" ), WeightArea.ToSharedRef() );
}


int32 FSkeletalAnimationSection::OnPaintSection( FSequencerSectionPainter& Painter ) const
{
	const ESlateDrawEffect DrawEffects = Painter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	
	const FTimeToPixel& TimeToPixelConverter = Painter.GetTimeConverter();

	int32 LayerId = Painter.PaintSectionBackground();

	static const FSlateBrush* GenericDivider = FEditorStyle::GetBrush("Sequencer.GenericDivider");

	// Add lines where the animation starts and ends/loops
	float AnimPlayRate = FMath::IsNearlyZero(Section.Params.PlayRate) ? 1.0f : Section.Params.PlayRate;
	float SeqLength = (Section.Params.GetSequenceLength() - (Section.Params.StartOffset + Section.Params.EndOffset)) / AnimPlayRate;

	if (!FMath::IsNearlyZero(SeqLength, KINDA_SMALL_NUMBER) && SeqLength > 0)
	{
		float MaxOffset = Section.GetRange().Size<float>();
		float OffsetTime = SeqLength;

		while (OffsetTime < MaxOffset)
		{
			float OffsetPixel = TimeToPixelConverter.TimeToPixel(Section.GetStartTime() + OffsetTime) - TimeToPixelConverter.TimeToPixel(Section.GetStartTime());

			FSlateDrawElement::MakeBox(
				Painter.DrawElements,
				LayerId,
				Painter.SectionGeometry.MakeChild(
					FVector2D(2.f, Painter.SectionGeometry.Size.Y-2.f),
					FSlateLayoutTransform(FVector2D(OffsetPixel, 1.f))
				).ToPaintGeometry(),
				GenericDivider,
				DrawEffects
			);

			OffsetTime += SeqLength;
		}
	}

	return LayerId;
}

void FSkeletalAnimationSection::BeginResizeSection()
{
	InitialStartOffsetDuringResize = Section.Params.StartOffset;
	InitialStartTimeDuringResize = Section.GetStartTime();
}

void FSkeletalAnimationSection::ResizeSection(ESequencerSectionResizeMode ResizeMode, float ResizeTime)
{
	// Adjust the start offset when resizing from the beginning
	if (ResizeMode == SSRM_LeadingEdge)
	{
		float StartOffset = (ResizeTime - InitialStartTimeDuringResize) * Section.Params.PlayRate;
		StartOffset += InitialStartOffsetDuringResize;

		// Ensure start offset is not less than 0 and adjust ResizeTime
		if (StartOffset < 0)
		{
			ResizeTime = ResizeTime - (StartOffset / Section.Params.PlayRate);

			StartOffset = 0.f;
		}

		Section.Params.StartOffset = StartOffset;
	}

	ISequencerSection::ResizeSection(ResizeMode, ResizeTime);
}

void FSkeletalAnimationSection::BeginSlipSection()
{
	BeginResizeSection();
}

void FSkeletalAnimationSection::SlipSection(float SlipTime)
{
	float StartOffset = (SlipTime - InitialStartTimeDuringResize) * Section.Params.PlayRate;
	StartOffset += InitialStartOffsetDuringResize;

	// Ensure start offset is not less than 0
	if (StartOffset < 0)
	{
		StartOffset = 0.f;
	}

	Section.Params.StartOffset = StartOffset;

	ISequencerSection::SlipSection(SlipTime);
}

FSkeletalAnimationTrackEditor::FSkeletalAnimationTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FMovieSceneTrackEditor( InSequencer ) 
{ }


TSharedRef<ISequencerTrackEditor> FSkeletalAnimationTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new FSkeletalAnimationTrackEditor( InSequencer ) );
}


bool FSkeletalAnimationTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	return Type == UMovieSceneSkeletalAnimationTrack::StaticClass();
}


TSharedRef<ISequencerSection> FSkeletalAnimationTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding )
{
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) );
	
	return MakeShareable( new FSkeletalAnimationSection(SectionObject) );
}


void FSkeletalAnimationTrackEditor::AddKey(const FGuid& ObjectGuid)
{
	USkeleton* Skeleton = AcquireSkeletonFromObjectGuid(ObjectGuid);

	if (Skeleton)
	{
		// Load the asset registry module
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		// Collect a full list of assets with the specified class
		TArray<FAssetData> AssetDataList;
		AssetRegistryModule.Get().GetAssetsByClass(UAnimSequenceBase::StaticClass()->GetFName(), AssetDataList, true);

		if (AssetDataList.Num())
		{
			TSharedPtr< SWindow > Parent = FSlateApplication::Get().GetActiveTopLevelWindow(); 
			if (Parent.IsValid())
			{
				FSlateApplication::Get().PushMenu(
					Parent.ToSharedRef(),
					FWidgetPath(),
					BuildAnimationSubMenu(ObjectGuid, Skeleton, nullptr),
					FSlateApplication::Get().GetCursorPos(),
					FPopupTransitionEffect(FPopupTransitionEffect::TypeInPopup)
					);
			}
		}
	}
}


bool FSkeletalAnimationTrackEditor::HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid)
{
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();

	if (Asset->IsA<UAnimSequenceBase>() && SequencerPtr.IsValid())
	{
		UAnimSequenceBase* AnimSequence = Cast<UAnimSequenceBase>(Asset);
		
		if (TargetObjectGuid.IsValid())
		{
			USkeleton* Skeleton = AcquireSkeletonFromObjectGuid(TargetObjectGuid);

			if (Skeleton && Skeleton == AnimSequence->GetSkeleton())
			{
				UObject* Object = SequencerPtr->FindSpawnedObjectOrTemplate(TargetObjectGuid);
				
				UMovieSceneTrack* Track = nullptr;

				AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FSkeletalAnimationTrackEditor::AddKeyInternal, Object, AnimSequence, Track));

				return true;
			}
		}
	}
	return false;
}


void FSkeletalAnimationTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	if (ObjectClass->IsChildOf(USkeletalMeshComponent::StaticClass()) || ObjectClass->IsChildOf(AActor::StaticClass()))
	{
		const TSharedPtr<ISequencer> ParentSequencer = GetSequencer();

		USkeleton* Skeleton = AcquireSkeletonFromObjectGuid(ObjectBinding);

		if (Skeleton)
		{
			// Load the asset registry module
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

			// Collect a full list of assets with the specified class
			TArray<FAssetData> AssetDataList;
			AssetRegistryModule.Get().GetAssetsByClass(UAnimSequenceBase::StaticClass()->GetFName(), AssetDataList, true);

			if (AssetDataList.Num())
			{
				UMovieSceneTrack* Track = nullptr;

				MenuBuilder.AddSubMenu(
					LOCTEXT("AddAnimation", "Animation"), NSLOCTEXT("Sequencer", "AddAnimationTooltip", "Adds an animation track."),
					FNewMenuDelegate::CreateRaw(this, &FSkeletalAnimationTrackEditor::AddAnimationSubMenu, ObjectBinding, Skeleton, Track)
				);
			}
		}
	}
}

TSharedRef<SWidget> FSkeletalAnimationTrackEditor::BuildAnimationSubMenu(FGuid ObjectBinding, USkeleton* Skeleton, UMovieSceneTrack* Track)
{
	FMenuBuilder MenuBuilder(true, nullptr);
	
	AddAnimationSubMenu(MenuBuilder, ObjectBinding, Skeleton, Track);

	return MenuBuilder.MakeWidget();
}

bool FSkeletalAnimationTrackEditor::ShouldFilterAsset(const FAssetData& AssetData)
{
	// we don't want montage
	if (AssetData.AssetClass == UAnimMontage::StaticClass()->GetFName())
	{
		return true;
	}

	const FString EnumString = AssetData.GetTagValueRef<FString>(GET_MEMBER_NAME_CHECKED(UAnimSequence, AdditiveAnimType));
	if (EnumString.IsEmpty())
	{
		return false;
	}

	UEnum* AdditiveTypeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EAdditiveAnimationType"), true);
	return ((EAdditiveAnimationType)AdditiveTypeEnum->GetValueByName(*EnumString) == AAT_RotationOffsetMeshSpace);
}

void FSkeletalAnimationTrackEditor::AddAnimationSubMenu(FMenuBuilder& MenuBuilder, FGuid ObjectBinding, USkeleton* Skeleton, UMovieSceneTrack* Track)
{
	FAssetPickerConfig AssetPickerConfig;
	{
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateRaw( this, &FSkeletalAnimationTrackEditor::OnAnimationAssetSelected, ObjectBinding, Track);
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateRaw(this, &FSkeletalAnimationTrackEditor::ShouldFilterAsset);
		AssetPickerConfig.Filter.bRecursiveClasses = true;
		AssetPickerConfig.Filter.ClassNames.Add(UAnimSequenceBase::StaticClass()->GetFName());
		AssetPickerConfig.Filter.TagsAndValues.Add(TEXT("Skeleton"), FAssetData(Skeleton).GetExportTextName());
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	TSharedPtr<SBox> MenuEntry = SNew(SBox)
		.WidthOverride(300.0f)
		.HeightOverride(300.f)
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];

	MenuBuilder.AddWidget(MenuEntry.ToSharedRef(), FText::GetEmpty(), true);
}


void FSkeletalAnimationTrackEditor::OnAnimationAssetSelected(const FAssetData& AssetData, FGuid ObjectBinding, UMovieSceneTrack* Track)
{
	FSlateApplication::Get().DismissAllMenus();

	UObject* SelectedObject = AssetData.GetAsset();
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();

	if (SelectedObject && SelectedObject->IsA(UAnimSequenceBase::StaticClass()) && SequencerPtr.IsValid())
	{
		UAnimSequenceBase* AnimSequence = CastChecked<UAnimSequenceBase>(AssetData.GetAsset());

		UObject* Object = SequencerPtr->FindSpawnedObjectOrTemplate(ObjectBinding);
		AnimatablePropertyChanged( FOnKeyProperty::CreateRaw( this, &FSkeletalAnimationTrackEditor::AddKeyInternal, Object, AnimSequence, Track) );
	}
}


FKeyPropertyResult FSkeletalAnimationTrackEditor::AddKeyInternal( float KeyTime, UObject* Object, class UAnimSequenceBase* AnimSequence, UMovieSceneTrack* Track )
{
	FKeyPropertyResult KeyPropertyResult;

	FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject( Object );
	FGuid ObjectHandle = HandleResult.Handle;
	KeyPropertyResult.bHandleCreated |= HandleResult.bWasCreated;
	if (ObjectHandle.IsValid())
	{
		if (!Track)
		{
			Track = AddTrack(GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene(), ObjectHandle, UMovieSceneSkeletalAnimationTrack::StaticClass(), NAME_None);
			KeyPropertyResult.bTrackCreated = true;
		}

		if (ensure(Track))
		{
			Track->Modify();

			Cast<UMovieSceneSkeletalAnimationTrack>(Track)->AddNewAnimation( KeyTime, AnimSequence );
			KeyPropertyResult.bTrackModified = true;
		}
	}

	return KeyPropertyResult;
}


USkeleton* FSkeletalAnimationTrackEditor::AcquireSkeletonFromObjectGuid(const FGuid& Guid)
{
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	UObject* BoundObject = SequencerPtr.IsValid() ? SequencerPtr->FindSpawnedObjectOrTemplate(Guid) : nullptr;

	if (AActor* Actor = Cast<AActor>(BoundObject))
	{
		TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshComponents;
		Actor->GetComponents(SkeletalMeshComponents);

		for (int32 j = 0; j <SkeletalMeshComponents.Num(); ++j)
		{
			USkeletalMeshComponent* SkeletalMeshComp = SkeletalMeshComponents[j];
			if (SkeletalMeshComp->SkeletalMesh && SkeletalMeshComp->SkeletalMesh->Skeleton)
			{
				// @todo Multiple actors, multiple components
				return SkeletalMeshComp->SkeletalMesh->Skeleton;
			}
		}
	}
	else if(USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(BoundObject))
	{
		if (SkeletalMeshComponent->SkeletalMesh)
		{
			return SkeletalMeshComponent->SkeletalMesh->Skeleton;
		}
	}

	return nullptr;
}


void CopyInterpAnimControlTrack(TSharedRef<ISequencer> Sequencer, UInterpTrackAnimControl* MatineeAnimControlTrack, UMovieSceneSkeletalAnimationTrack* SkeletalAnimationTrack)
{
	float EndPlaybackRange = Sequencer.Get().GetFocusedMovieSceneSequence()->GetMovieScene()->GetPlaybackRange().GetUpperBoundValue();

	if (FMatineeImportTools::CopyInterpAnimControlTrack(MatineeAnimControlTrack, SkeletalAnimationTrack, EndPlaybackRange))
	{
		Sequencer.Get().NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
	}
}

void FSkeletalAnimationTrackEditor::BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track )
{
	UInterpTrackAnimControl* MatineeAnimControlTrack = nullptr;
	for ( UObject* CopyPasteObject : GUnrealEd->MatineeCopyPasteBuffer )
	{
		MatineeAnimControlTrack = Cast<UInterpTrackAnimControl>( CopyPasteObject );
		if ( MatineeAnimControlTrack != nullptr )
		{
			break;
		}
	}
	UMovieSceneSkeletalAnimationTrack* SkeletalAnimationTrack = Cast<UMovieSceneSkeletalAnimationTrack>( Track );
	MenuBuilder.AddMenuEntry(
		NSLOCTEXT( "Sequencer", "PasteMatineeAnimControlTrack", "Paste Matinee SkeletalAnimation Track" ),
		NSLOCTEXT( "Sequencer", "PasteMatineeAnimControlTrackTooltip", "Pastes keys from a Matinee float track into this track." ),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateStatic( &CopyInterpAnimControlTrack, GetSequencer().ToSharedRef(), MatineeAnimControlTrack, SkeletalAnimationTrack ),
			FCanExecuteAction::CreateLambda( [=]()->bool { return MatineeAnimControlTrack != nullptr && MatineeAnimControlTrack->AnimSeqs.Num() > 0 && SkeletalAnimationTrack != nullptr; } ) ) );
}

TSharedPtr<SWidget> FSkeletalAnimationTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	USkeleton* Skeleton = AcquireSkeletonFromObjectGuid(ObjectBinding);

	if (Skeleton)
	{
		// Create a container edit box
		return SNew(SHorizontalBox)

		// Add the animation combo box
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			FSequencerUtilities::MakeAddButton(LOCTEXT("AnimationText", "Animation"), FOnGetContent::CreateSP(this, &FSkeletalAnimationTrackEditor::BuildAnimationSubMenu, ObjectBinding, Skeleton, Track), Params.NodeIsHovered)
		];
	}

	else
	{
		return TSharedPtr<SWidget>();
	}
}

#undef LOCTEXT_NAMESPACE
