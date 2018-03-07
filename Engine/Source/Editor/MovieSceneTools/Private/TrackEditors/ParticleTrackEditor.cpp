// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/ParticleTrackEditor.h"
#include "Rendering/DrawElements.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Curves/IntegralCurve.h"
#include "SequencerSectionPainter.h"
#include "EditorStyleSet.h"
#include "Editor/UnrealEdEngine.h"
#include "Particles/Emitter.h"
#include "Particles/ParticleSystemComponent.h"
#include "UnrealEdGlobals.h"
#include "Tracks/MovieSceneParticleTrack.h"
#include "ISectionLayoutBuilder.h"
#include "Sections/MovieSceneParticleSection.h"
#include "CommonMovieSceneTools.h"
#include "Particles/ParticleLODLevel.h"
#include "Particles/ParticleModuleRequired.h"
#include "EnumKeyArea.h"
#include "Matinee/InterpTrackToggle.h"
#include "MatineeImportTools.h"


namespace AnimatableParticleEditorConstants
{
	// @todo Sequencer Allow this to be customizable
	const uint32 ParticleTrackHeight = 20;
}


#define LOCTEXT_NAMESPACE "FParticleTrackEditor"


FParticleSection::FParticleSection( UMovieSceneSection& InSection, TSharedRef<ISequencer>InOwningSequencer )
	: Section( InSection )
	, OwningSequencerPtr( InOwningSequencer )
{
	ParticleKeyEnum = FindObject<UEnum>( ANY_PACKAGE, TEXT( "EParticleKey" ) );
	checkf( ParticleKeyEnum != nullptr, TEXT( "FParticleSection could not find the EParticleKey UEnum by name." ) )

	LeftKeyBrush = FEditorStyle::GetBrush( "Sequencer.KeyLeft" );
	RightKeyBrush = FEditorStyle::GetBrush( "Sequencer.KeyRight" );
}


FParticleSection::~FParticleSection()
{
}


UMovieSceneSection* FParticleSection::GetSectionObject()
{
	return &Section;
}

float FParticleSection::GetSectionHeight() const
{
	return (float)AnimatableParticleEditorConstants::ParticleTrackHeight;
}

void FParticleSection::GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const
{
	UMovieSceneParticleSection* ParticleSection = Cast<UMovieSceneParticleSection>( &Section );
	LayoutBuilder.SetSectionAsKeyArea( MakeShareable( new FEnumKeyArea( ParticleSection->GetParticleCurve(), ParticleSection, ParticleKeyEnum ) ) );
}

int32 FParticleSection::OnPaintSection( FSequencerSectionPainter& InPainter ) const
{
	TSharedPtr<ISequencer> OwningSequencer = OwningSequencerPtr.Pin();

	if (!OwningSequencer.IsValid())
	{
		return InPainter.LayerId + 1;
	}

	const ESlateDrawEffect DrawEffects = InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	UMovieSceneParticleSection* AnimSection = Cast<UMovieSceneParticleSection>( &Section );
	const FTimeToPixel& TimeToPixelConverter = InPainter.GetTimeConverter();

	FLinearColor TrackColor;

	// @todo Sequencer - These values should be cached and then refreshed only when the particle system changes.
	bool bIsLooping = false;
	float LastEmitterEndTime = 0;
	UMovieSceneParticleSection* ParticleSection = Cast<UMovieSceneParticleSection>( &Section );
	if ( ParticleSection != nullptr )
	{
		UMovieSceneParticleTrack* ParentTrack = Cast<UMovieSceneParticleTrack>( ParticleSection->GetOuter() );
		if ( ParentTrack != nullptr )
		{
			TrackColor = ParentTrack->GetColorTint();

			FGuid ObjectHandle;
			for ( const FMovieSceneBinding& Binding : OwningSequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetBindings() )
			{
				if ( Binding.GetTracks().Contains( ParentTrack ) )
				{
					ObjectHandle = Binding.GetObjectGuid();
					break;
				}
			}

			if ( ObjectHandle.IsValid() )
			{
				for (TWeakObjectPtr<> Object : OwningSequencer->FindObjectsInCurrentSequence(ObjectHandle))
				{
					UParticleSystemComponent* ParticleSystemComponent = Cast<UParticleSystemComponent>(Object.Get());
					if(AEmitter* ParticleSystemActor = Cast<AEmitter>(Object.Get()))
					{
						ParticleSystemComponent = ParticleSystemActor->GetParticleSystemComponent();
					}

					if ( ParticleSystemComponent != nullptr && ParticleSystemComponent->Template != nullptr )
					{
						for ( UParticleEmitter* Emitter : ParticleSystemComponent->Template->Emitters )
						{
							UParticleModuleRequired* RequiredModule = Emitter->GetLODLevel( 0 )->RequiredModule;
							bIsLooping |= RequiredModule->EmitterLoops == 0;
							LastEmitterEndTime = FMath::Max( LastEmitterEndTime, RequiredModule->EmitterDelay + RequiredModule->EmitterDuration );
						}
					}
				}
			}
		}
	}

	// @todo Sequencer - This should only draw the visible ranges.
	TArray<TRange<float>> DrawRanges;
	TOptional<float> CurrentRangeStart;
	for ( auto KeyIterator = ParticleSection->GetParticleCurve().GetKeyIterator(); KeyIterator; ++KeyIterator )
	{
		FIntegralKey Key = *KeyIterator;
		if ( (EParticleKey::Type)Key.Value == EParticleKey::Activate )
		{
			if ( CurrentRangeStart.IsSet() == false )
			{
				CurrentRangeStart = Key.Time;
			}
			else
			{
				if ( bIsLooping == false )
				{
					if ( Key.Time > CurrentRangeStart.GetValue() + LastEmitterEndTime )
					{
						DrawRanges.Add( TRange<float>( CurrentRangeStart.GetValue(), CurrentRangeStart.GetValue() + LastEmitterEndTime ) );
					}
					else
					{
						DrawRanges.Add( TRange<float>( CurrentRangeStart.GetValue(), Key.Time ) );
					}
					CurrentRangeStart = Key.Time;
				}
			}
		}
		if ( (EParticleKey::Type)Key.Value == EParticleKey::Deactivate )
		{
			if ( CurrentRangeStart.IsSet() )
			{
				if (bIsLooping)
				{
					DrawRanges.Add( TRange<float>( CurrentRangeStart.GetValue(), Key.Time ) );
				}
				else
				{
					if ( Key.Time > CurrentRangeStart.GetValue() + LastEmitterEndTime )
					{
						DrawRanges.Add( TRange<float>( CurrentRangeStart.GetValue(), CurrentRangeStart.GetValue() + LastEmitterEndTime ) );
					}
					else
					{
						DrawRanges.Add( TRange<float>( CurrentRangeStart.GetValue(), Key.Time ) );
					}
				}
				CurrentRangeStart.Reset();
			}
		}
	}
	if ( CurrentRangeStart.IsSet() )
	{
		if ( bIsLooping )
		{
			DrawRanges.Add( TRange<float>( CurrentRangeStart.GetValue(), OwningSequencer->GetViewRange().GetUpperBoundValue() ) );
		}
		else
		{
			DrawRanges.Add( TRange<float>( CurrentRangeStart.GetValue(), CurrentRangeStart.GetValue() + LastEmitterEndTime ) );
		}
	}

	for ( const TRange<float>& DrawRange : DrawRanges )
	{
		float XOffset = TimeToPixelConverter.TimeToPixel(DrawRange.GetLowerBoundValue());
		float XSize = TimeToPixelConverter.TimeToPixel(DrawRange.GetUpperBoundValue()) - XOffset;
		FSlateDrawElement::MakeBox(
			InPainter.DrawElements,
			InPainter.LayerId,
			InPainter.SectionGeometry.ToPaintGeometry( FVector2D( XOffset, (InPainter.SectionGeometry.GetLocalSize().Y - SequencerSectionConstants::KeySize.Y) / 2 ), FVector2D( XSize, SequencerSectionConstants::KeySize.Y ) ),
			FEditorStyle::GetBrush( "Sequencer.Section.Background" ),
			DrawEffects
			);
		FSlateDrawElement::MakeBox(
			InPainter.DrawElements,
			InPainter.LayerId,
			InPainter.SectionGeometry.ToPaintGeometry( FVector2D( XOffset, (InPainter.SectionGeometry.GetLocalSize().Y - SequencerSectionConstants::KeySize.Y) / 2 ), FVector2D( XSize, SequencerSectionConstants::KeySize.Y ) ),
			FEditorStyle::GetBrush( "Sequencer.Section.BackgroundTint" ),
			DrawEffects,
			TrackColor
			);
	}

	return InPainter.LayerId+1;
}


const FSlateBrush* FParticleSection::GetKeyBrush( FKeyHandle KeyHandle ) const
{
	UMovieSceneParticleSection* ParticleSection = Cast<UMovieSceneParticleSection>( &Section );
	if ( ParticleSection != nullptr )
	{
		FIntegralKey ParticleKey = ParticleSection->GetParticleCurve().GetKey(KeyHandle);
		if ( (EParticleKey::Type)ParticleKey.Value == EParticleKey::Activate )
		{
			return LeftKeyBrush;
		}
		else if ( (EParticleKey::Type)ParticleKey.Value == EParticleKey::Deactivate )
		{
			return RightKeyBrush;
		}
	}
	return nullptr;
}

FVector2D FParticleSection::GetKeyBrushOrigin( FKeyHandle KeyHandle ) const
{
	UMovieSceneParticleSection* ParticleSection = Cast<UMovieSceneParticleSection>( &Section );
	if ( ParticleSection != nullptr )
	{
		FIntegralKey ParticleKey = ParticleSection->GetParticleCurve().GetKey(KeyHandle);
		if ( (EParticleKey::Type)ParticleKey.Value == EParticleKey::Activate )
		{
			return FVector2D(-1.0f, 1.0f);
		}
		else if ( (EParticleKey::Type)ParticleKey.Value == EParticleKey::Deactivate )
		{
			return FVector2D(1.0f, 1.0f);
		}
	}
	return FVector2D(0.0f, 0.0f);
}


FParticleTrackEditor::FParticleTrackEditor( TSharedRef<ISequencer> InSequencer )
	: FMovieSceneTrackEditor( InSequencer ) 
{ }


TSharedRef<ISequencerTrackEditor> FParticleTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new FParticleTrackEditor( InSequencer ) );
}


bool FParticleTrackEditor::SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const
{
	return Type == UMovieSceneParticleTrack::StaticClass();
}


TSharedRef<ISequencerSection> FParticleTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding )
{
	check( SupportsType( SectionObject.GetOuter()->GetClass() ) );

	const TSharedPtr<ISequencer> OwningSequencer = GetSequencer();
	return MakeShareable( new FParticleSection( SectionObject, OwningSequencer.ToSharedRef() ) );
}


void FParticleTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding, const UClass* ObjectClass)
{
	if (ObjectClass->IsChildOf(AEmitter::StaticClass()) || ObjectClass->IsChildOf(UParticleSystemComponent::StaticClass()))
	{
		const TSharedPtr<ISequencer> ParentSequencer = GetSequencer();

		MenuBuilder.AddMenuEntry(
			LOCTEXT("AddParticleTrack", "Particle Toggle Track"),
			LOCTEXT("TriggerParticlesTooltip", "Adds a track for controlling particle emitter state."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FParticleTrackEditor::AddParticleKey, ObjectBinding))
		);
	}
}


void FParticleTrackEditor::AddParticleKey( const FGuid ObjectGuid )
{
	TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	UObject* Object = SequencerPtr.IsValid() ? SequencerPtr->FindSpawnedObjectOrTemplate(ObjectGuid) : nullptr;

	if (Object)
	{
		AnimatablePropertyChanged( FOnKeyProperty::CreateRaw( this, &FParticleTrackEditor::AddKeyInternal, Object ) );
	}
}


FKeyPropertyResult FParticleTrackEditor::AddKeyInternal( float KeyTime, UObject* Object )
{
	FKeyPropertyResult KeyPropertyResult;

	FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject( Object );
	FGuid ObjectHandle = HandleResult.Handle;
	KeyPropertyResult.bHandleCreated |= HandleResult.bWasCreated;

	if (ObjectHandle.IsValid())
	{
		FFindOrCreateTrackResult TrackResult = FindOrCreateTrackForObject(ObjectHandle, UMovieSceneParticleTrack::StaticClass());
		UMovieSceneTrack* Track = TrackResult.Track;
		KeyPropertyResult.bTrackCreated |= TrackResult.bWasCreated;

		if (KeyPropertyResult.bTrackCreated && ensure(Track))
		{
			UMovieSceneParticleTrack* ParticleTrack = Cast<UMovieSceneParticleTrack>(Track);
			ParticleTrack->AddNewSection(KeyTime);
			ParticleTrack->SetDisplayName(LOCTEXT("TrackName", "Particle System"));
			KeyPropertyResult.bTrackModified = true;
		}
	}

	return KeyPropertyResult;
}


void CopyInterpParticleTrack(TSharedRef<ISequencer> Sequencer, UInterpTrackToggle* MatineeToggleTrack, UMovieSceneParticleTrack* ParticleTrack)
{
	if (FMatineeImportTools::CopyInterpParticleTrack(MatineeToggleTrack, ParticleTrack))
	{
		Sequencer.Get().NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::MovieSceneStructureItemAdded );
	}
}

void FParticleTrackEditor::BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track )
{
	UInterpTrackToggle* MatineeToggleTrack = nullptr;
	for ( UObject* CopyPasteObject : GUnrealEd->MatineeCopyPasteBuffer )
	{
		MatineeToggleTrack = Cast<UInterpTrackToggle>( CopyPasteObject );
		if ( MatineeToggleTrack != nullptr )
		{
			break;
		}
	}
	UMovieSceneParticleTrack* ParticleTrack = Cast<UMovieSceneParticleTrack>( Track );
	MenuBuilder.AddMenuEntry(
		NSLOCTEXT( "Sequencer", "PasteMatineeToggleTrack", "Paste Matinee Particle Track" ),
		NSLOCTEXT( "Sequencer", "PasteMatineeToggleTrackTooltip", "Pastes keys from a Matinee particle track into this track." ),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateStatic( &CopyInterpParticleTrack, GetSequencer().ToSharedRef(), MatineeToggleTrack, ParticleTrack ),
			FCanExecuteAction::CreateLambda( [=]()->bool { return MatineeToggleTrack != nullptr && MatineeToggleTrack->ToggleTrack.Num() > 0 && ParticleTrack != nullptr; } ) ) );
}


#undef LOCTEXT_NAMESPACE
