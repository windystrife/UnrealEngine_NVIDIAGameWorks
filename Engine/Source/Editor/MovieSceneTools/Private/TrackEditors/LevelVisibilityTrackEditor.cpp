// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/LevelVisibilityTrackEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "Sections/MovieSceneLevelVisibilitySection.h"
#include "Sections/LevelVisibilitySection.h"
#include "Tracks/MovieSceneLevelVisibilityTrack.h"
#include "SequencerUtilities.h"

#define LOCTEXT_NAMESPACE "LevelVisibilityTrackEditor.h"

FLevelVisibilityTrackEditor::FLevelVisibilityTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FMovieSceneTrackEditor( InSequencer ) 
{ }


TSharedRef<ISequencerTrackEditor> FLevelVisibilityTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new FLevelVisibilityTrackEditor( InSequencer ) );
}

bool FLevelVisibilityTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	return (InSequence != nullptr) && (InSequence->GetClass()->GetName() == TEXT("LevelSequence"));
}

bool FLevelVisibilityTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	return Type == UMovieSceneLevelVisibilityTrack::StaticClass();
}

const FSlateBrush* FLevelVisibilityTrackEditor::GetIconBrush() const
{
	return FEditorStyle::GetBrush("Sequencer.Tracks.LevelVisibility");
}

TSharedRef<ISequencerSection> FLevelVisibilityTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding )
{
	UMovieSceneLevelVisibilitySection* LevelVisibilitySection = Cast<UMovieSceneLevelVisibilitySection>(&SectionObject);
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) && LevelVisibilitySection != nullptr );

	return MakeShareable( new FLevelVisibilitySection( *LevelVisibilitySection ) );
}

void FLevelVisibilityTrackEditor::BuildAddTrackMenu( FMenuBuilder& MenuBuilder )
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddTrack", "Level Visibility Track" ),
		LOCTEXT("AddAdTrackToolTip", "Adds a new track which can control level visibility." ),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "Sequencer.Tracks.LevelVisibility"),
		FUIAction( FExecuteAction::CreateRaw( this, &FLevelVisibilityTrackEditor::OnAddTrack ) ) );
}


TSharedPtr<SWidget> FLevelVisibilityTrackEditor::BuildOutlinerEditWidget( const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params )
{
	// Create a container edit box
	return FSequencerUtilities::MakeAddButton( 
		LOCTEXT( "AddVisibilityTrigger", "Visibility Trigger" ),
		FOnGetContent::CreateSP( this, &FLevelVisibilityTrackEditor::BuildAddVisibilityTriggerMenu, Track ),
		Params.NodeIsHovered );
}


void FLevelVisibilityTrackEditor::AddNewSection( UMovieScene* MovieScene, UMovieSceneTrack* LevelVisibilityTrack, ELevelVisibility Visibility )
{
	const FScopedTransaction Transaction( LOCTEXT( "AddLevelVisibilitySection_Transaction", "Add Level Visibility Trigger" ) );

	LevelVisibilityTrack->Modify();

	UMovieSceneLevelVisibilitySection* LevelVisibilitySection = CastChecked<UMovieSceneLevelVisibilitySection>( LevelVisibilityTrack->CreateNewSection() );
	LevelVisibilitySection->SetVisibility( Visibility );
	LevelVisibilitySection->SetStartTime( MovieScene->GetPlaybackRange().GetLowerBoundValue() );
	LevelVisibilitySection->SetEndTime( MovieScene->GetPlaybackRange().GetUpperBoundValue() );

	int32 RowIndex = -1;
	for ( const UMovieSceneSection* Section : LevelVisibilityTrack->GetAllSections() )
	{
		RowIndex = FMath::Max( RowIndex, Section->GetRowIndex() );
	}
	LevelVisibilitySection->SetRowIndex(RowIndex + 1);

	LevelVisibilityTrack->AddSection( *LevelVisibilitySection );
}


void FLevelVisibilityTrackEditor::OnAddTrack()
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();

	if ( FocusedMovieScene == nullptr )
	{
		return;
	}

	const FScopedTransaction Transaction( LOCTEXT( "AddLevelVisibilityTrack_Transaction", "Add Level Visibility Track" ) );
	FocusedMovieScene->Modify();

	UMovieSceneLevelVisibilityTrack* NewTrack = FocusedMovieScene->AddMasterTrack<UMovieSceneLevelVisibilityTrack>();
	checkf( NewTrack != nullptr, TEXT("Failed to create new level visibility track.") );

	AddNewSection( FocusedMovieScene, NewTrack, ELevelVisibility::Visible );

	GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
}


TSharedRef<SWidget> FLevelVisibilityTrackEditor::BuildAddVisibilityTriggerMenu( UMovieSceneTrack* LevelVisibilityTrack )
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(
		LOCTEXT( "AddVisibleTrigger", "Visible" ),
		LOCTEXT( "AddVisibleTriggerToolTip", "Add a trigger for visible levels." ),
		FSlateIcon(),
		FUIAction( FExecuteAction::CreateSP(
			this, &FLevelVisibilityTrackEditor::OnAddNewSection, LevelVisibilityTrack, ELevelVisibility::Visible ) ) );

	MenuBuilder.AddMenuEntry(
		LOCTEXT( "AddHiddenTrigger", "Hidden" ),
		LOCTEXT( "AddHiddenTriggerToolTip", "Add a trigger for hidden levels." ),
		FSlateIcon(),
		FUIAction( FExecuteAction::CreateSP(
		this, &FLevelVisibilityTrackEditor::OnAddNewSection, LevelVisibilityTrack, ELevelVisibility::Hidden ) ) );

	return MenuBuilder.MakeWidget();
}


void FLevelVisibilityTrackEditor::OnAddNewSection( UMovieSceneTrack* LevelVisibilityTrack, ELevelVisibility Visibility )
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();

	if ( FocusedMovieScene == nullptr )
	{
		return;
	}

	AddNewSection( FocusedMovieScene, LevelVisibilityTrack, Visibility );
	GetSequencer()->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
}

#undef LOCTEXT_NAMESPACE
