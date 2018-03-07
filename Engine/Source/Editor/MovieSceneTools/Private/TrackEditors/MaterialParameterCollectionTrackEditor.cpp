// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/MaterialParameterCollectionTrackEditor.h"
#include "Tracks/MovieSceneMaterialParameterCollectionTrack.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Materials/MaterialParameterCollection.h"
#include "Sections/MovieSceneParameterSection.h"
#include "Sections/ParameterSection.h"
#include "SequencerUtilities.h"
#include "Algo/Sort.h"
#include "SlateIconFinder.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "SBox.h"
#include "SlateApplication.h"

#define LOCTEXT_NAMESPACE "MaterialParameterCollectionTrackEditor"


FMaterialParameterCollectionTrackEditor::FMaterialParameterCollectionTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer)
{
}

TSharedRef<ISequencerTrackEditor> FMaterialParameterCollectionTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer)
{
	return MakeShared<FMaterialParameterCollectionTrackEditor>(OwningSequencer);
}

TSharedRef<ISequencerSection> FMaterialParameterCollectionTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	UMovieSceneParameterSection* ParameterSection = Cast<UMovieSceneParameterSection>(&SectionObject);
	checkf(ParameterSection != nullptr, TEXT("Unsupported section type."));

	return MakeShareable(new FParameterSection(*ParameterSection, FText::FromName(ParameterSection->GetFName())));
}

TSharedRef<SWidget> CreateAssetPicker(FOnAssetSelected OnAssetSelected)
{
	FAssetPickerConfig AssetPickerConfig;
	{
		AssetPickerConfig.OnAssetSelected = OnAssetSelected;
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		AssetPickerConfig.Filter.bRecursiveClasses = true;
		AssetPickerConfig.Filter.ClassNames.Add(UMaterialParameterCollection::StaticClass()->GetFName());
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	return SNew(SBox)
		.WidthOverride(300.0f)
		.HeightOverride(300.f)
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];
}

void FMaterialParameterCollectionTrackEditor::BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track)
{
	UMovieSceneMaterialParameterCollectionTrack* MPCTrack = Cast<UMovieSceneMaterialParameterCollectionTrack>(Track);

	auto AssignAsset = [MPCTrack](const FAssetData& InAssetData)
	{
		UMaterialParameterCollection* MPC = Cast<UMaterialParameterCollection>(InAssetData.GetAsset());

		if (MPC)
		{
			FScopedTransaction Transaction(LOCTEXT("SetAssetTransaction", "Assign Material Parameter Collection"));
			MPCTrack->Modify();
			MPCTrack->MPC = MPC;
		}
	};

	auto SubMenuCallback = [this, AssignAsset](FMenuBuilder& SubMenuBuilder)
	{
		SubMenuBuilder.AddWidget(CreateAssetPicker(FOnAssetSelected::CreateLambda(AssignAsset)), FText::GetEmpty(), true);
	};

	MenuBuilder.AddSubMenu(
		LOCTEXT("SetAsset", "Set Asset"),
		LOCTEXT("SetAsset_ToolTip", "Sets the Material Parameter Collection that this track animates."),
		FNewMenuDelegate::CreateLambda(SubMenuCallback)
	);
}


void FMaterialParameterCollectionTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	auto SubMenuCallback = [this](FMenuBuilder& SubMenuBuilder)
	{
		SubMenuBuilder.AddWidget(CreateAssetPicker(FOnAssetSelected::CreateRaw(this, &FMaterialParameterCollectionTrackEditor::AddTrackToSequence)), FText::GetEmpty(), true);
	};

	MenuBuilder.AddSubMenu(
		LOCTEXT("AddMPCTrack", "Material Parameter Collection Track"),
		LOCTEXT("AddMPCTrackToolTip", "Adds a new track that controls parameters within a Material Parameter Collection."),
		FNewMenuDelegate::CreateLambda(SubMenuCallback),
		false,
		FSlateIconFinder::FindIconForClass(UMaterialParameterCollection::StaticClass())
	);
}

void FMaterialParameterCollectionTrackEditor::AddTrackToSequence(const FAssetData& InAssetData)
{
	FSlateApplication::Get().DismissAllMenus();

	UMaterialParameterCollection* MPC = Cast<UMaterialParameterCollection>(InAssetData.GetAsset());
	UMovieScene* MovieScene = GetFocusedMovieScene();
	if (!MPC || !MovieScene)
	{
		return;
	}

	// Attempt to find an existing MPC track that animates this object
	for (UMovieSceneTrack* Track : MovieScene->GetMasterTracks())
	{
		if (auto* MPCTrack = Cast<UMovieSceneMaterialParameterCollectionTrack>(Track))
		{
			if (MPCTrack->MPC == MPC)
			{
				return;
			}
		}
	}

	const FScopedTransaction Transaction(LOCTEXT("AddTrackDescription", "Add Material Parameter Collection Track"));

	MovieScene->Modify();
	UMovieSceneMaterialParameterCollectionTrack* Track = MovieScene->AddMasterTrack<UMovieSceneMaterialParameterCollectionTrack>();
	check(Track);

	UMovieSceneSection* NewSection = Track->CreateNewSection();
	check(NewSection);

	Track->AddSection(*NewSection);
	Track->MPC = MPC;
	Track->SetDisplayName(FText::FromString(MPC->GetName()));

	GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
}

bool FMaterialParameterCollectionTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return Type == UMovieSceneMaterialParameterCollectionTrack::StaticClass();
}

bool FMaterialParameterCollectionTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	static UClass* LevelSequenceClass = FindObject<UClass>(ANY_PACKAGE, TEXT("LevelSequence"), true);
	static UClass* WidgetAnimationClass = FindObject<UClass>(ANY_PACKAGE, TEXT("WidgetAnimation"), true);
	return InSequence != nullptr &&
		((LevelSequenceClass != nullptr && InSequence->GetClass()->IsChildOf(LevelSequenceClass)) ||
		(WidgetAnimationClass != nullptr && InSequence->GetClass()->IsChildOf(WidgetAnimationClass)));
}

const FSlateBrush* FMaterialParameterCollectionTrackEditor::GetIconBrush() const
{
	return FSlateIconFinder::FindIconForClass(UMaterialParameterCollection::StaticClass()).GetIcon();
}

TSharedPtr<SWidget> FMaterialParameterCollectionTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	UMovieSceneMaterialParameterCollectionTrack* MPCTrack = Cast<UMovieSceneMaterialParameterCollectionTrack>(Track);
	FOnGetContent MenuContent = FOnGetContent::CreateSP(this, &FMaterialParameterCollectionTrackEditor::OnGetAddParameterMenuContent, MPCTrack);

	return FSequencerUtilities::MakeAddButton(LOCTEXT("AddParameterButton", "Parameter"), MenuContent, Params.NodeIsHovered);
}

TSharedRef<SWidget> FMaterialParameterCollectionTrackEditor::OnGetAddParameterMenuContent(UMovieSceneMaterialParameterCollectionTrack* MPCTrack)
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ScalarParametersHeading", "Scalar"));
	{
		TArray<FCollectionScalarParameter> ScalarParameters = MPCTrack->MPC->ScalarParameters;
		Algo::SortBy(ScalarParameters, &FCollectionParameterBase::ParameterName);

		for (const FCollectionScalarParameter& Scalar : ScalarParameters)
		{
			MenuBuilder.AddMenuEntry(
				FText::FromName(Scalar.ParameterName),
				FText(),
				FSlateIcon(),
				FExecuteAction::CreateSP(this, &FMaterialParameterCollectionTrackEditor::AddScalarParameter, MPCTrack, Scalar)
				);
		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("VectorParametersHeading", "Vector"));
	{
		TArray<FCollectionVectorParameter> VectorParameters = MPCTrack->MPC->VectorParameters;
		Algo::SortBy(VectorParameters, &FCollectionParameterBase::ParameterName);

		for (const FCollectionVectorParameter& Vector : VectorParameters)
		{
			MenuBuilder.AddMenuEntry(
				FText::FromName(Vector.ParameterName),
				FText(),
				FSlateIcon(),
				FExecuteAction::CreateSP(this, &FMaterialParameterCollectionTrackEditor::AddVectorParameter, MPCTrack, Vector)
				);
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}


void FMaterialParameterCollectionTrackEditor::AddScalarParameter(UMovieSceneMaterialParameterCollectionTrack* Track, FCollectionScalarParameter Parameter)
{
	if (!Track->MPC)
	{
		return;
	}

	float KeyTime = GetTimeForKey();

	const FScopedTransaction Transaction(LOCTEXT("AddScalarParameter", "Add scalar parameter"));
	Track->Modify();
	Track->AddScalarParameterKey(Parameter.ParameterName, KeyTime, Parameter.DefaultValue);
	GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
}


void FMaterialParameterCollectionTrackEditor::AddVectorParameter(UMovieSceneMaterialParameterCollectionTrack* Track, FCollectionVectorParameter Parameter)
{
	if (!Track->MPC)
	{
		return;
	}

	float KeyTime = GetTimeForKey();

	const FScopedTransaction Transaction(LOCTEXT("AddVectorParameter", "Add vector parameter"));
	Track->Modify();
	Track->AddColorParameterKey(Parameter.ParameterName, KeyTime, Parameter.DefaultValue);
	GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemAdded);
}

bool FMaterialParameterCollectionTrackEditor::HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid)
{
	UMaterialParameterCollection* MPC = Cast<UMaterialParameterCollection>(Asset);
	if (MPC)
	{
		AddTrackToSequence(FAssetData(MPC));
	}

	return MPC != nullptr;
}

#undef LOCTEXT_NAMESPACE
