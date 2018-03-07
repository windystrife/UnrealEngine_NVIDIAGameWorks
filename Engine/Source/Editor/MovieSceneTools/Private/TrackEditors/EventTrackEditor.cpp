// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/EventTrackEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "EditorStyleSet.h"
#include "Package.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Sections/EventTrackSection.h"
#include "MovieSceneObjectBindingIDPicker.h"
#include "IDetailCustomization.h"
#include "PropertyEditorModule.h"
#include "ModuleManager.h"
#include "DetailLayoutBuilder.h"
#include "MovieSceneObjectBindingIDCustomization.h"
#include "DetailCategoryBuilder.h"
#include "IDetailPropertyRow.h"

#define LOCTEXT_NAMESPACE "FEventTrackEditor"


/* FEventTrackEditor static functions
 *****************************************************************************/

TSharedRef<ISequencerTrackEditor> FEventTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FEventTrackEditor(InSequencer));
}


/* FEventTrackEditor structors
 *****************************************************************************/

FEventTrackEditor::FEventTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer)
{ }


/* ISequencerTrackEditor interface
 *****************************************************************************/

void FEventTrackEditor::BuildAddTrackMenu(FMenuBuilder& MenuBuilder)
{
	UMovieSceneSequence* RootMovieSceneSequence = GetSequencer()->GetRootMovieSceneSequence();

	if (RootMovieSceneSequence == nullptr)
	{
		return;
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddEventTrack", "Event Track"),
		LOCTEXT("AddEventTooltip", "Adds a new event track that can trigger events on the timeline."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.Tracks.Event"),
		FUIAction(
			FExecuteAction::CreateRaw(this, &FEventTrackEditor::HandleAddEventTrackMenuEntryExecute)
		)
	);
}


void FEventTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	if (!ObjectClass->IsChildOf(AActor::StaticClass()))
	{
		return;
	}

}


void FEventTrackEditor::BuildTrackContextMenu(FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track)
{
	UMovieSceneEventTrack* EventTrack = CastChecked<UMovieSceneEventTrack>(Track);
	UProperty* EventPositionProperty = FindField<UProperty>(Track->GetClass(), GET_MEMBER_NAME_STRING_CHECKED(UMovieSceneEventTrack, EventPosition));

	/** Specific details customization for the event track */
	class FEventTrackCustomization : public IDetailCustomization
	{
	public:
		FEventTrackCustomization(TWeakPtr<IDetailsView> InDetailsView, TSharedPtr<ISequencer> InSequencer)
			: WeakDetailsView(InDetailsView)
		{
			FOnGetPropertyTypeCustomizationInstance Factory = FOnGetPropertyTypeCustomizationInstance::CreateLambda([=]{ return MakeShared<FMovieSceneObjectBindingIDCustomization>(InSequencer->GetFocusedTemplateID(), InSequencer); });

			// Register an object binding ID customization that can use the current sequencer interface
			WeakDetailsView.Pin()->RegisterInstancedCustomPropertyTypeLayout("MovieSceneObjectBindingID", Factory);
		}

		~FEventTrackCustomization()
		{
			auto PinnedDetailsView = WeakDetailsView.Pin();
			if (PinnedDetailsView.IsValid())
			{
				PinnedDetailsView->UnregisterInstancedCustomPropertyTypeLayout("MovieSceneObjectBindingID");
			}
		}

		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
		{
			DetailBuilder.HideCategory("Track");
			DetailBuilder.HideCategory("General");

			IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("TrackEvent");
			Category.AddProperty("EventReceivers").ShouldAutoExpand(true);
		}

		TWeakPtr<IDetailsView> WeakDetailsView;
	};

	auto PopulateSubMenu = [this, EventTrack](FMenuBuilder& SubMenuBuilder)
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		// Create a details view for the track
		FDetailsViewArgs DetailsViewArgs(false,false,false,FDetailsViewArgs::HideNameArea,true);
		DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Automatic;
		DetailsViewArgs.bShowOptions = false;
		
		TSharedRef<IDetailsView> DetailsView = PropertyEditor.CreateDetailView(DetailsViewArgs);
		
		// Register the custom type layout for the class
		TWeakPtr<IDetailsView> WeakDetailsView = DetailsView;
		FOnGetDetailCustomizationInstance CreateInstance = FOnGetDetailCustomizationInstance::CreateLambda([=]{ return MakeShared<FEventTrackCustomization>(WeakDetailsView, GetSequencer()); });
		DetailsView->RegisterInstancedCustomPropertyLayout(UMovieSceneEventTrack::StaticClass(), CreateInstance);

		// Assign the object
		DetailsView->SetObject(EventTrack, true);

		// Add it to the menu
		SubMenuBuilder.AddWidget(DetailsView, FText(), true, false);
	};

	MenuBuilder.AddSubMenu(LOCTEXT("Properties_MenuText", "Properties"), FText(), FNewMenuDelegate::CreateLambda(PopulateSubMenu));
}


TSharedRef<ISequencerSection> FEventTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	return MakeShareable(new FEventTrackSection(SectionObject, GetSequencer()));
}


bool FEventTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return (Type == UMovieSceneEventTrack::StaticClass());
}

bool  FEventTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	static UClass* LevelSequenceClass = FindObject<UClass>(ANY_PACKAGE, TEXT("LevelSequence"), true);
	static UClass* WidgetAnimationClass = FindObject<UClass>(ANY_PACKAGE, TEXT("WidgetAnimation"), true);
	return InSequence != nullptr &&
		((LevelSequenceClass != nullptr && InSequence->GetClass()->IsChildOf(LevelSequenceClass)) ||
		(WidgetAnimationClass != nullptr && InSequence->GetClass()->IsChildOf(WidgetAnimationClass)));
}

const FSlateBrush* FEventTrackEditor::GetIconBrush() const
{
	return FEditorStyle::GetBrush("Sequencer.Tracks.Event");
}

/* FEventTrackEditor callbacks
 *****************************************************************************/

void FEventTrackEditor::HandleAddEventTrackMenuEntryExecute()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();

	if (FocusedMovieScene == nullptr)
	{
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "AddEventTrack_Transaction", "Add Event Track"));
	FocusedMovieScene->Modify();
	
	UMovieSceneEventTrack* NewTrack = FocusedMovieScene->AddMasterTrack<UMovieSceneEventTrack>();
	check(NewTrack);

	UMovieSceneSection* NewSection = NewTrack->CreateNewSection();
	check(NewSection);

	NewTrack->AddSection(*NewSection);
	NewTrack->SetDisplayName(LOCTEXT("TrackName", "Events"));

	GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
}


#undef LOCTEXT_NAMESPACE
