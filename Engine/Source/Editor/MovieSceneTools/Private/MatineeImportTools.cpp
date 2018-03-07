// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MatineeImportTools.h"
#include "MovieSceneSequence.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "ScopedTransaction.h"
#include "MovieSceneCommonHelpers.h"
#include "IMovieScenePlayer.h"

#include "Curves/CurveInterface.h"

#include "Matinee/MatineeActor.h"
#include "Matinee/InterpData.h"
#include "Matinee/InterpGroupInst.h"
#include "Matinee/InterpTrackLinearColorProp.h"
#include "Matinee/InterpTrackColorProp.h"
#include "Matinee/InterpTrackBoolProp.h"
#include "Matinee/InterpTrackMoveAxis.h"
#include "Matinee/InterpTrackAnimControl.h"
#include "Matinee/InterpTrackSound.h"
#include "Matinee/InterpTrackFade.h"
#include "Matinee/InterpTrackDirector.h"
#include "Matinee/InterpTrackEvent.h"
#include "Matinee/InterpTrackVectorProp.h"
#include "Matinee/InterpTrackVisibility.h"

#include "Tracks/MovieSceneBoolTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieSceneColorTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneParticleTrack.h"
#include "Tracks/MovieSceneSkeletalAnimationTrack.h"
#include "Tracks/MovieSceneFadeTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Tracks/MovieSceneVisibilityTrack.h"
#include "Tracks/MovieSceneAudioTrack.h"
#include "Tracks/MovieSceneVectorTrack.h"

#include "Sections/MovieSceneColorSection.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "Sections/MovieSceneAudioSection.h"
#include "Sections/MovieSceneFadeSection.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Sections/MovieSceneEventSection.h"
#include "Sections/MovieSceneVectorSection.h"


#include "Animation/AnimSequence.h"


ERichCurveInterpMode FMatineeImportTools::MatineeInterpolationToRichCurveInterpolation( EInterpCurveMode CurveMode )
{
	switch ( CurveMode )
	{
	case CIM_Constant:
		return ERichCurveInterpMode::RCIM_Constant;
	case CIM_CurveAuto:
	case CIM_CurveAutoClamped:
	case CIM_CurveBreak:
	case CIM_CurveUser:
		return ERichCurveInterpMode::RCIM_Cubic;
	case CIM_Linear:
		return ERichCurveInterpMode::RCIM_Linear;
	default:
		return ERichCurveInterpMode::RCIM_None;
	}
}


ERichCurveTangentMode FMatineeImportTools::MatineeInterpolationToRichCurveTangent( EInterpCurveMode CurveMode )
{
	switch ( CurveMode )
	{
	case CIM_CurveBreak:
		return ERichCurveTangentMode::RCTM_Break;
	case CIM_CurveUser:
	// Import auto-clamped curves as user curves because rich curves don't have support for clamped tangents, and if the
	// user moves the converted keys, the tangents will get mangled.
	case CIM_CurveAutoClamped:
		return ERichCurveTangentMode::RCTM_User;
	default:
		return ERichCurveTangentMode::RCTM_Auto;
	}
}

void CleanupCurveKeys(FRichCurve& InCurve)
{
	InCurve.RemoveRedundantKeys(KINDA_SMALL_NUMBER);
	InCurve.AutoSetTangents();
}


bool FMatineeImportTools::TryConvertMatineeToggleToOutParticleKey( ETrackToggleAction ToggleAction, EParticleKey::Type& OutParticleKey )
{
	switch ( ToggleAction )
	{
	case ETrackToggleAction::ETTA_On:
		OutParticleKey = EParticleKey::Activate;
		return true;
	case ETrackToggleAction::ETTA_Off:
		OutParticleKey = EParticleKey::Deactivate;
		return true;
	case ETrackToggleAction::ETTA_Trigger:
		OutParticleKey = EParticleKey::Trigger;
		return true;
	}
	return false;
}


void FMatineeImportTools::SetOrAddKey( FRichCurve& Curve, float Time, float Value, float ArriveTangent, float LeaveTangent, EInterpCurveMode MatineeInterpMode )
{
	FKeyHandle KeyHandle = Curve.FindKey(Time);

	if (!Curve.IsKeyHandleValid(KeyHandle))
	{
		KeyHandle = Curve.AddKey( Time, Value, false);
	}

	FRichCurveKey& Key = Curve.GetKey( KeyHandle );
	Key.ArriveTangent = ArriveTangent;
	Key.LeaveTangent = LeaveTangent;
	Key.InterpMode = MatineeInterpolationToRichCurveInterpolation( MatineeInterpMode );
	Key.TangentMode = MatineeInterpolationToRichCurveTangent( MatineeInterpMode );
}


bool FMatineeImportTools::CopyInterpBoolTrack( UInterpTrackBoolProp* MatineeBoolTrack, UMovieSceneBoolTrack* BoolTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeFBoolTrack", "Paste Matinee Bool Track" ) );
	bool bSectionCreated = false;

	BoolTrack->Modify();

	float KeyTime = MatineeBoolTrack->GetKeyframeTime( 0 );
	UMovieSceneBoolSection* Section = Cast<UMovieSceneBoolSection>( MovieSceneHelpers::FindSectionAtTime( BoolTrack->GetAllSections(), KeyTime ) );
	if ( Section == nullptr )
	{
		Section = Cast<UMovieSceneBoolSection>( BoolTrack->CreateNewSection() );
		BoolTrack->AddSection( *Section );
		Section->SetIsInfinite(true);
		bSectionCreated = true;
	}
	if (Section->TryModify())
	{
		float SectionMin = Section->GetStartTime();
		float SectionMax = Section->GetEndTime();

		FIntegralCurve& BoolCurve = Section->GetCurve();
		for ( const auto& Point : MatineeBoolTrack->BoolTrack )
		{
			BoolCurve.UpdateOrAddKey(Point.Time, Point.Value);
			SectionMin = FMath::Min( SectionMin, Point.Time );
			SectionMax = FMath::Max( SectionMax, Point.Time );
		}

		Section->SetStartTime( SectionMin );
		Section->SetEndTime( SectionMax );
	}

	return bSectionCreated;
}

bool FMatineeImportTools::CopyInterpFloatTrack( UInterpTrackFloatBase* MatineeFloatTrack, UMovieSceneFloatTrack* FloatTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeFloatTrack", "Paste Matinee Float Track" ) );
	bool bSectionCreated = false;

	FloatTrack->Modify();

	float KeyTime = MatineeFloatTrack->GetKeyframeTime( 0 );
	UMovieSceneFloatSection* Section = Cast<UMovieSceneFloatSection>( MovieSceneHelpers::FindSectionAtTime( FloatTrack->GetAllSections(), KeyTime ) );
	if ( Section == nullptr )
	{
		Section = Cast<UMovieSceneFloatSection>( FloatTrack->CreateNewSection() );
		FloatTrack->AddSection( *Section );
		Section->SetIsInfinite(true);
		bSectionCreated = true;
	}
	if (Section->TryModify())
	{
		float SectionMin = Section->GetStartTime();
		float SectionMax = Section->GetEndTime();

		FRichCurve& FloatCurve = Section->GetFloatCurve();
		for ( const auto& Point : MatineeFloatTrack->FloatTrack.Points )
		{
			FMatineeImportTools::SetOrAddKey( FloatCurve, Point.InVal, Point.OutVal, Point.ArriveTangent, Point.LeaveTangent, Point.InterpMode );
			SectionMin = FMath::Min( SectionMin, Point.InVal );
			SectionMax = FMath::Max( SectionMax, Point.InVal );
		}

		CleanupCurveKeys(FloatCurve);

		Section->SetStartTime( SectionMin );
		Section->SetEndTime( SectionMax );
	}

	return bSectionCreated;
}

bool FMatineeImportTools::CopyInterpVectorTrack( UInterpTrackVectorProp* MatineeVectorTrack, UMovieSceneVectorTrack* VectorTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeVectorTrack", "Paste Matinee Vector Track" ) );
	bool bSectionCreated = false;

	VectorTrack->Modify();

	float KeyTime = MatineeVectorTrack->GetKeyframeTime( 0 );
	UMovieSceneVectorSection* Section = Cast<UMovieSceneVectorSection>( MovieSceneHelpers::FindSectionAtTime( VectorTrack->GetAllSections(), KeyTime ) );
	if ( Section == nullptr )
	{
		Section = Cast<UMovieSceneVectorSection>( VectorTrack->CreateNewSection() );
		VectorTrack->AddSection( *Section );
		Section->SetIsInfinite(true);
		bSectionCreated = true;
	}
	if (Section->TryModify())
	{
		float SectionMin = Section->GetStartTime();
		float SectionMax = Section->GetEndTime();

		if (Section->GetChannelsUsed() == 3)
		{
			FRichCurve& XCurve = Section->GetCurve(0);
			FRichCurve& YCurve = Section->GetCurve(1);
			FRichCurve& ZCurve = Section->GetCurve(2);

			for ( const auto& Point : MatineeVectorTrack->VectorTrack.Points )
			{
				FMatineeImportTools::SetOrAddKey( XCurve, Point.InVal, Point.OutVal.X, Point.ArriveTangent.X, Point.LeaveTangent.X, Point.InterpMode );
				FMatineeImportTools::SetOrAddKey( YCurve, Point.InVal, Point.OutVal.Y, Point.ArriveTangent.Y, Point.LeaveTangent.Y, Point.InterpMode );
				FMatineeImportTools::SetOrAddKey( ZCurve, Point.InVal, Point.OutVal.Z, Point.ArriveTangent.Z, Point.LeaveTangent.Z, Point.InterpMode );
				SectionMin = FMath::Min( SectionMin, Point.InVal );
				SectionMax = FMath::Max( SectionMax, Point.InVal );
			}
			
			CleanupCurveKeys(XCurve);
			CleanupCurveKeys(YCurve);
			CleanupCurveKeys(ZCurve);
		}

		Section->SetStartTime( SectionMin );
		Section->SetEndTime( SectionMax );
	}

	return bSectionCreated;
}


bool FMatineeImportTools::CopyInterpColorTrack( UInterpTrackColorProp* ColorPropTrack, UMovieSceneColorTrack* ColorTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeColorTrack", "Paste Matinee Color Track" ) );
	bool bSectionCreated = false;

	ColorTrack->Modify();

	float KeyTime = ColorPropTrack->GetKeyframeTime( 0 );
	UMovieSceneColorSection* Section = Cast<UMovieSceneColorSection>( MovieSceneHelpers::FindSectionAtTime( ColorTrack->GetAllSections(), KeyTime ) );
	if ( Section == nullptr )
	{
		Section = Cast<UMovieSceneColorSection>( ColorTrack->CreateNewSection() );
		ColorTrack->AddSection( *Section );
		Section->GetRedCurve().SetDefaultValue(0.f);
		Section->GetGreenCurve().SetDefaultValue(0.f);
		Section->GetBlueCurve().SetDefaultValue(0.f);
		Section->GetAlphaCurve().SetDefaultValue(1.f);
		Section->SetIsInfinite(true);
		bSectionCreated = true;
	}

	if (Section->TryModify())
	{
		float SectionMin = Section->GetStartTime();
		float SectionMax = Section->GetEndTime();

		FRichCurve& RedCurve = Section->GetRedCurve();
		FRichCurve& GreenCurve = Section->GetGreenCurve();
		FRichCurve& BlueCurve = Section->GetBlueCurve();

		for ( const auto& Point : ColorPropTrack->VectorTrack.Points )
		{
			FMatineeImportTools::SetOrAddKey( RedCurve, Point.InVal, Point.OutVal.X, Point.ArriveTangent.X, Point.LeaveTangent.X, Point.InterpMode );
			FMatineeImportTools::SetOrAddKey( GreenCurve, Point.InVal, Point.OutVal.Y, Point.ArriveTangent.Y, Point.LeaveTangent.Y, Point.InterpMode );
			FMatineeImportTools::SetOrAddKey( BlueCurve, Point.InVal, Point.OutVal.Z, Point.ArriveTangent.Z, Point.LeaveTangent.Z, Point.InterpMode );
			SectionMin = FMath::Min( SectionMin, Point.InVal );
			SectionMax = FMath::Max( SectionMax, Point.InVal );
		}

		CleanupCurveKeys(RedCurve);
		CleanupCurveKeys(GreenCurve);
		CleanupCurveKeys(BlueCurve);

		Section->SetStartTime( SectionMin );
		Section->SetEndTime( SectionMax );
	}

	return bSectionCreated;
}


bool FMatineeImportTools::CopyInterpLinearColorTrack( UInterpTrackLinearColorProp* LinearColorPropTrack, UMovieSceneColorTrack* ColorTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeLinearColorTrack", "Paste Matinee Linear Color Track" ) );
	bool bSectionCreated = false;

	ColorTrack->Modify();

	float KeyTime = LinearColorPropTrack->GetKeyframeTime( 0 );
	UMovieSceneColorSection* Section = Cast<UMovieSceneColorSection>( MovieSceneHelpers::FindSectionAtTime( ColorTrack->GetAllSections(), KeyTime ) );
	if ( Section == nullptr )
	{
		Section = Cast<UMovieSceneColorSection>( ColorTrack->CreateNewSection() );
		ColorTrack->AddSection( *Section );
		Section->GetRedCurve().SetDefaultValue(0.f);
		Section->GetGreenCurve().SetDefaultValue(0.f);
		Section->GetBlueCurve().SetDefaultValue(0.f);
		Section->GetAlphaCurve().SetDefaultValue(1.f);
		Section->SetIsInfinite(true);
		bSectionCreated = true;
	}

	if (Section->TryModify())
	{
		float SectionMin = Section->GetStartTime();
		float SectionMax = Section->GetEndTime();

		FRichCurve& RedCurve = Section->GetRedCurve();
		FRichCurve& GreenCurve = Section->GetGreenCurve();
		FRichCurve& BlueCurve = Section->GetBlueCurve();
		FRichCurve& AlphaCurve = Section->GetAlphaCurve();

		for ( const auto& Point : LinearColorPropTrack->LinearColorTrack.Points)
		{
			FMatineeImportTools::SetOrAddKey( RedCurve, Point.InVal, Point.OutVal.R, Point.ArriveTangent.R, Point.LeaveTangent.R, Point.InterpMode );
			FMatineeImportTools::SetOrAddKey( GreenCurve, Point.InVal, Point.OutVal.G, Point.ArriveTangent.G, Point.LeaveTangent.G, Point.InterpMode );
			FMatineeImportTools::SetOrAddKey( BlueCurve, Point.InVal, Point.OutVal.B, Point.ArriveTangent.B, Point.LeaveTangent.B, Point.InterpMode );
			FMatineeImportTools::SetOrAddKey( AlphaCurve, Point.InVal, Point.OutVal.A, Point.ArriveTangent.A, Point.LeaveTangent.A, Point.InterpMode );
			SectionMin = FMath::Min( SectionMin, Point.InVal );
			SectionMax = FMath::Max( SectionMax, Point.InVal );
		}

		CleanupCurveKeys(RedCurve);
		CleanupCurveKeys(GreenCurve);
		CleanupCurveKeys(BlueCurve);
		CleanupCurveKeys(AlphaCurve);

		Section->SetStartTime( SectionMin );
		Section->SetEndTime( SectionMax );
	}

	return bSectionCreated;
}

bool FMatineeImportTools::CopyInterpMoveTrack( UInterpTrackMove* MoveTrack, UMovieScene3DTransformTrack* TransformTrack, const FVector& DefaultScale )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeMoveTrack", "Paste Matinee Move Track" ) );
	bool bSectionCreated = false;

	TransformTrack->Modify();

	float KeyTime = MoveTrack->GetKeyframeTime( 0 );
	UMovieScene3DTransformSection* Section = Cast<UMovieScene3DTransformSection>( MovieSceneHelpers::FindSectionAtTime( TransformTrack->GetAllSections(), KeyTime ) );
	if ( Section == nullptr )
	{
		Section = Cast<UMovieScene3DTransformSection>( TransformTrack->CreateNewSection() );
		Section->GetScaleCurve(EAxis::X).SetDefaultValue(DefaultScale.X);
		Section->GetScaleCurve(EAxis::Y).SetDefaultValue(DefaultScale.Y);
		Section->GetScaleCurve(EAxis::Z).SetDefaultValue(DefaultScale.Z);
		TransformTrack->AddSection( *Section );
		Section->SetIsInfinite(true);
		bSectionCreated = true;
	}

	if (Section->TryModify())
	{
		float SectionMin = Section->GetStartTime();
		float SectionMax = Section->GetEndTime();

		FRichCurve& TranslationXCurve = Section->GetTranslationCurve( EAxis::X );
		FRichCurve& TranslationYCurve = Section->GetTranslationCurve( EAxis::Y );
		FRichCurve& TranslationZCurve = Section->GetTranslationCurve( EAxis::Z );

		for ( const auto& Point : MoveTrack->PosTrack.Points )
		{
			FMatineeImportTools::SetOrAddKey( TranslationXCurve, Point.InVal, Point.OutVal.X, Point.ArriveTangent.X, Point.LeaveTangent.X, Point.InterpMode );
			FMatineeImportTools::SetOrAddKey( TranslationYCurve, Point.InVal, Point.OutVal.Y, Point.ArriveTangent.Y, Point.LeaveTangent.Y, Point.InterpMode );
			FMatineeImportTools::SetOrAddKey( TranslationZCurve, Point.InVal, Point.OutVal.Z, Point.ArriveTangent.Z, Point.LeaveTangent.Z, Point.InterpMode );
			SectionMin = FMath::Min( SectionMin, Point.InVal );
			SectionMax = FMath::Max( SectionMax, Point.InVal );
		}

		FRichCurve& RotationXCurve = Section->GetRotationCurve( EAxis::X );
		FRichCurve& RotationYCurve = Section->GetRotationCurve( EAxis::Y );
		FRichCurve& RotationZCurve = Section->GetRotationCurve( EAxis::Z );

		for ( const auto& Point : MoveTrack->EulerTrack.Points )
		{
			FMatineeImportTools::SetOrAddKey( RotationXCurve, Point.InVal, Point.OutVal.X, Point.ArriveTangent.X, Point.LeaveTangent.X, Point.InterpMode );
			FMatineeImportTools::SetOrAddKey( RotationYCurve, Point.InVal, Point.OutVal.Y, Point.ArriveTangent.Y, Point.LeaveTangent.Y, Point.InterpMode );
			FMatineeImportTools::SetOrAddKey( RotationZCurve, Point.InVal, Point.OutVal.Z, Point.ArriveTangent.Z, Point.LeaveTangent.Z, Point.InterpMode );
			SectionMin = FMath::Min( SectionMin, Point.InVal );
			SectionMax = FMath::Max( SectionMax, Point.InVal );
		}

		for (auto SubTrack : MoveTrack->SubTracks)
		{
			if (SubTrack->IsA(UInterpTrackMoveAxis::StaticClass()))
			{
				UInterpTrackMoveAxis* MoveSubTrack = Cast<UInterpTrackMoveAxis>(SubTrack);
				if (MoveSubTrack)
				{
					FRichCurve* SubTrackCurve = nullptr;

					if (MoveSubTrack->MoveAxis == EInterpMoveAxis::AXIS_TranslationX)
					{
						SubTrackCurve = &TranslationXCurve;
					}
					else if (MoveSubTrack->MoveAxis == EInterpMoveAxis::AXIS_TranslationY)
					{
						SubTrackCurve = &TranslationYCurve;
					}
					else if (MoveSubTrack->MoveAxis == EInterpMoveAxis::AXIS_TranslationZ)
					{
						SubTrackCurve = &TranslationZCurve;
					}
					else if (MoveSubTrack->MoveAxis == EInterpMoveAxis::AXIS_RotationX)
					{
						SubTrackCurve = &RotationXCurve;
					}
					else if (MoveSubTrack->MoveAxis == EInterpMoveAxis::AXIS_RotationY)
					{
						SubTrackCurve = &RotationYCurve;
					}
					else if (MoveSubTrack->MoveAxis == EInterpMoveAxis::AXIS_RotationZ)
					{
						SubTrackCurve = &RotationZCurve;
					}
							
					if (SubTrackCurve != nullptr)
					{
						for (const auto& Point : MoveSubTrack->FloatTrack.Points)
						{
							FMatineeImportTools::SetOrAddKey( *SubTrackCurve, Point.InVal, Point.OutVal, Point.ArriveTangent, Point.LeaveTangent, Point.InterpMode );
							SectionMax = FMath::Max( SectionMax, Point.InVal );
						}

						CleanupCurveKeys(*SubTrackCurve);
					}
				}
			}
		}

		CleanupCurveKeys(TranslationXCurve);
		CleanupCurveKeys(TranslationYCurve);
		CleanupCurveKeys(TranslationZCurve);

		CleanupCurveKeys(RotationXCurve);
		CleanupCurveKeys(RotationYCurve);
		CleanupCurveKeys(RotationZCurve);

		Section->SetStartTime( SectionMin );
		Section->SetEndTime( SectionMax );
	}

	return bSectionCreated;
}


bool FMatineeImportTools::CopyInterpParticleTrack( UInterpTrackToggle* MatineeToggleTrack, UMovieSceneParticleTrack* ParticleTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeParticleTrack", "Paste Matinee Particle Track" ) );
	bool bSectionCreated = false;

	ParticleTrack->Modify();

	float KeyTime = MatineeToggleTrack->GetKeyframeTime( 0 );
	UMovieSceneParticleSection* Section = Cast<UMovieSceneParticleSection>( MovieSceneHelpers::FindSectionAtTime( ParticleTrack->GetAllSections(), KeyTime ) );
	if ( Section == nullptr )
	{
		Section = Cast<UMovieSceneParticleSection>( ParticleTrack->CreateNewSection() );
		ParticleTrack->AddSection( *Section );
		bSectionCreated = true;
	}

	if (Section->TryModify())
	{
		float SectionMin = Section->GetStartTime();
		float SectionMax = Section->GetEndTime();

		FIntegralCurve& ParticleCurve = Section->GetParticleCurve();
		for ( const auto& Key : MatineeToggleTrack->ToggleTrack )
		{
			EParticleKey::Type ParticleKey;
			if ( TryConvertMatineeToggleToOutParticleKey( Key.ToggleAction, ParticleKey ) )
			{
				ParticleCurve.AddKey( Key.Time, (int32)ParticleKey, ParticleCurve.FindKey( Key.Time ) );
			}
			SectionMin = FMath::Min( SectionMin, Key.Time );
			SectionMax = FMath::Max( SectionMax, Key.Time );
		}

		Section->SetStartTime( SectionMin );
		Section->SetEndTime( SectionMax );
	}

	return bSectionCreated;
}


bool FMatineeImportTools::CopyInterpAnimControlTrack( UInterpTrackAnimControl* MatineeAnimControlTrack, UMovieSceneSkeletalAnimationTrack* SkeletalAnimationTrack, float EndPlaybackRange )
{
	// @todo - Sequencer - Add support for slot names once they are implemented.
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeAnimTrack", "Paste Matinee Anim Track" ) );
	bool bSectionCreated = false;
	
	SkeletalAnimationTrack->Modify();
	SkeletalAnimationTrack->RemoveAllAnimationData();

	for (int32 i = 0; i < MatineeAnimControlTrack->AnimSeqs.Num(); i++)
	{
		const auto& AnimSeq = MatineeAnimControlTrack->AnimSeqs[i];

		float EndTime;
		if( AnimSeq.bLooping )
		{
			if( i < MatineeAnimControlTrack->AnimSeqs.Num() - 1 )
			{
				EndTime = MatineeAnimControlTrack->AnimSeqs[i + 1].StartTime;
			}
			else
			{
				EndTime = EndPlaybackRange;
			}
		}
		else
		{
			EndTime = AnimSeq.StartTime + ( ( ( AnimSeq.AnimSeq->SequenceLength - AnimSeq.AnimEndOffset ) - AnimSeq.AnimStartOffset ) / AnimSeq.AnimPlayRate );

			// Clamp to next clip's start time
			if (i+1 < MatineeAnimControlTrack->AnimSeqs.Num())
			{
				float NextStartTime = MatineeAnimControlTrack->AnimSeqs[i+1].StartTime;
				EndTime = FMath::Min(NextStartTime, EndTime);
			}
		}

		UMovieSceneSkeletalAnimationSection* NewSection = Cast<UMovieSceneSkeletalAnimationSection>( SkeletalAnimationTrack->CreateNewSection() );
		NewSection->SetStartTime( AnimSeq.StartTime );
		NewSection->SetEndTime( EndTime );
		NewSection->Params.StartOffset = AnimSeq.AnimStartOffset;
		NewSection->Params.EndOffset = AnimSeq.AnimEndOffset;
		NewSection->Params.PlayRate = AnimSeq.AnimPlayRate;
		NewSection->Params.Animation = AnimSeq.AnimSeq;
		NewSection->Params.SlotName = MatineeAnimControlTrack->SlotName;

		SkeletalAnimationTrack->AddSection( *NewSection );
		bSectionCreated = true;
	}

	return bSectionCreated;
}

bool FMatineeImportTools::CopyInterpSoundTrack( UInterpTrackSound* MatineeSoundTrack, UMovieSceneAudioTrack* AudioTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeSoundTrack", "Paste Matinee Sound Track" ) );
	bool bSectionCreated = false;

	AudioTrack->Modify();
	
	int MaxSectionRowIndex = -1;
	for ( UMovieSceneSection* Section : AudioTrack->GetAllSections() )
	{
		MaxSectionRowIndex = FMath::Max( MaxSectionRowIndex, Section->GetRowIndex() );
	}

	for ( const FSoundTrackKey& SoundTrackKey : MatineeSoundTrack->Sounds )
	{
		AudioTrack->AddNewSound( SoundTrackKey.Sound, SoundTrackKey.Time );

		UMovieSceneAudioSection* NewAudioSection = Cast<UMovieSceneAudioSection>(AudioTrack->GetAllSections().Last());
		NewAudioSection->SetRowIndex( MaxSectionRowIndex + 1 );
		NewAudioSection->GetPitchMultiplierCurve().SetDefaultValue( SoundTrackKey.Pitch );
		NewAudioSection->GetSoundVolumeCurve().SetDefaultValue( SoundTrackKey.Volume );

		AudioTrack->AddSection( *NewAudioSection );
		bSectionCreated = true;
	}

	return bSectionCreated;
}

bool FMatineeImportTools::CopyInterpFadeTrack( UInterpTrackFade* MatineeFadeTrack, UMovieSceneFadeTrack* FadeTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeFadeTrack", "Paste Matinee Fade Track" ) );
	bool bSectionCreated = false;

	FadeTrack->Modify();
	
	float KeyTime = MatineeFadeTrack->GetKeyframeTime( 0 );
	UMovieSceneFadeSection* Section = Cast<UMovieSceneFadeSection>( MovieSceneHelpers::FindSectionAtTime( FadeTrack->GetAllSections(), KeyTime ) );
	if ( Section == nullptr )
	{
		Section = Cast<UMovieSceneFadeSection>( FadeTrack->CreateNewSection() );
		FadeTrack->AddSection( *Section );
		bSectionCreated = true;
	}
	if (Section->TryModify())
	{
		float SectionMin = Section->GetStartTime();		
		float SectionMax = Section->GetEndTime();
		
		FRichCurve& FloatCurve = Section->GetFloatCurve();
		for ( const auto& Point : MatineeFadeTrack->FloatTrack.Points )
		{
			FMatineeImportTools::SetOrAddKey( FloatCurve, Point.InVal, Point.OutVal, Point.ArriveTangent, Point.LeaveTangent, Point.InterpMode );
			SectionMin = FMath::Min( SectionMin, Point.InVal );
			SectionMax = FMath::Max( SectionMax, Point.InVal );
		}

		Section->SetStartTime( SectionMin );
		Section->SetEndTime( SectionMax );
	
		Section->FadeColor = MatineeFadeTrack->FadeColor;
		Section->bFadeAudio = MatineeFadeTrack->bFadeAudio;
	}

	return bSectionCreated;
}

bool FMatineeImportTools::CopyInterpDirectorTrack( UInterpTrackDirector* DirectorTrack, UMovieSceneCameraCutTrack* CameraCutTrack, AMatineeActor* MatineeActor, IMovieScenePlayer& Player )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeDirectorTrack", "Paste Matinee Director Track" ) );
	bool bCutsAdded = false;

	CameraCutTrack->Modify();
	
	for (FDirectorTrackCut TrackCut : DirectorTrack->CutTrack)
	{
		int32 GroupIndex = MatineeActor->MatineeData->FindGroupByName(TrackCut.TargetCamGroup);
		
		UInterpGroupInst* ViewGroupInst = (GroupIndex != INDEX_NONE) ? MatineeActor->FindFirstGroupInstByName( TrackCut.TargetCamGroup.ToString() ) : NULL;
		if ( GroupIndex != INDEX_NONE && ViewGroupInst )
		{
			// Find a valid move track for this cut.
			UInterpGroup* Group = MatineeActor->MatineeData->InterpGroups[GroupIndex];
			if (Group)
			{
				AActor* CameraActor = ViewGroupInst->GetGroupActor();
		
				FGuid CameraHandle = Player.FindObjectId(*CameraActor, MovieSceneSequenceID::Root);
				if (CameraHandle.IsValid())
				{
					CameraCutTrack->AddNewCameraCut(CameraHandle, TrackCut.Time);
					bCutsAdded = true;
				}
			}
		}
	}

	return bCutsAdded;
}

bool FMatineeImportTools::CopyInterpEventTrack( UInterpTrackEvent* MatineeEventTrack, UMovieSceneEventTrack* EventTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeEventTrack", "Paste Matinee Event Track" ) );
	bool bSectionCreated = false;

	EventTrack->Modify();

	if (MatineeEventTrack->EventTrack.Num())
	{
		float KeyTime = MatineeEventTrack->EventTrack[0].Time;
		UMovieSceneEventSection* Section = Cast<UMovieSceneEventSection>( MovieSceneHelpers::FindSectionAtTime( EventTrack->GetAllSections(), KeyTime ) );
		if ( Section == nullptr )
		{
			Section = Cast<UMovieSceneEventSection>( EventTrack->CreateNewSection() );
			EventTrack->AddSection( *Section );
			bSectionCreated = true;
		}
		if (Section->TryModify())
		{
			float SectionMin = Section->GetStartTime();
			float SectionMax = Section->GetEndTime();

			TCurveInterface<FEventPayload, float> CurveInterface = Section->GetCurveInterface();
			for (FEventTrackKey EventTrackKey : MatineeEventTrack->EventTrack)
			{
				CurveInterface.UpdateOrAddKey(EventTrackKey.Time, FEventPayload(EventTrackKey.EventName), KINDA_SMALL_NUMBER);
				SectionMin = FMath::Min( SectionMin, EventTrackKey.Time );
				SectionMax = FMath::Max( SectionMax, EventTrackKey.Time );
			}

			Section->SetStartTime( SectionMin );
			Section->SetEndTime( SectionMax );
		}
	}	
	
	return bSectionCreated;
}

bool FMatineeImportTools::CopyInterpVisibilityTrack( UInterpTrackVisibility* MatineeVisibilityTrack, UMovieSceneVisibilityTrack* VisibilityTrack )
{
	const FScopedTransaction Transaction( NSLOCTEXT( "Sequencer", "PasteMatineeVisibilityTrack", "Paste Matinee Visibility track" ) );
	bool bSectionCreated = false;

	VisibilityTrack->Modify();

	if (MatineeVisibilityTrack->VisibilityTrack.Num())
	{
		float KeyTime = MatineeVisibilityTrack->VisibilityTrack[0].Time;
		UMovieSceneBoolSection* Section = Cast<UMovieSceneBoolSection>( MovieSceneHelpers::FindSectionAtTime( VisibilityTrack->GetAllSections(), KeyTime ) );
		if ( Section == nullptr )
		{
			Section = Cast<UMovieSceneBoolSection>( VisibilityTrack->CreateNewSection() );
			VisibilityTrack->AddSection( *Section );
			bSectionCreated = true;
		}
		if (Section->TryModify())
		{
			float SectionMin = Section->GetStartTime();
			float SectionMax = Section->GetEndTime();
			
			bool bVisible = true;
			FIntegralCurve& VisibilityCurve = Section->GetCurve();
			for (FVisibilityTrackKey VisibilityTrackKey : MatineeVisibilityTrack->VisibilityTrack)
			{
				if (VisibilityTrackKey.Action == EVisibilityTrackAction::EVTA_Hide)
				{
					bVisible = false;
				}
				else if (VisibilityTrackKey.Action == EVisibilityTrackAction::EVTA_Show)
				{
					bVisible = true;
				}
				else if (VisibilityTrackKey.Action == EVisibilityTrackAction::EVTA_Toggle)
				{
					bVisible = !bVisible;
				}

				VisibilityCurve.UpdateOrAddKey(VisibilityTrackKey.Time, bVisible);
				SectionMin = FMath::Min( SectionMin, VisibilityTrackKey.Time );
				SectionMax = FMath::Max( SectionMax, VisibilityTrackKey.Time );
			}

			Section->SetStartTime( SectionMin );
			Section->SetEndTime( SectionMax );
		}
	}	
	
	return bSectionCreated;
}
