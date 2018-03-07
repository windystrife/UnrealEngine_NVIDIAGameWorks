// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/ActorReferencePropertySection.h"
#include "GameFramework/Actor.h"
#include "ISectionLayoutBuilder.h"
#include "Sections/MovieSceneActorReferenceSection.h"


void FActorReferenceKeyArea::CopyKeys(FMovieSceneClipboardBuilder& ClipboardBuilder, const TFunctionRef<bool(FKeyHandle, const IKeyArea&)>& KeyMask) const
{
	const UMovieSceneActorReferenceSection* Section = Cast<UMovieSceneActorReferenceSection>(OwningSection);
	UMovieSceneTrack* Track = Section ? Section->GetTypedOuter<UMovieSceneTrack>() : nullptr;
	if (!Track)
	{
		return;
	}

	FMovieSceneClipboardKeyTrack* KeyTrack = nullptr;

	for (auto It(Curve.GetKeyHandleIterator()); It; ++It)
	{
		FKeyHandle Handle = It.Key();
		if (KeyMask(Handle, *this))
		{
			if (!KeyTrack)
			{
				KeyTrack = &ClipboardBuilder.FindOrAddKeyTrack<FGuid>(GetName(), *Track);
			}

			// We can't copy the curve values directly since the actor reference curve hold indices to
			// the actual guids since there is no FGuid curve.
			// TODO: Fix up this code once there is an FGuid curve.
			float KeyTime = Curve.GetKeyTime(Handle);
			FGuid KeyValue = Section->Eval(KeyTime);
			KeyTrack->AddKey(KeyTime, KeyValue);
		}
	}
}


void FActorReferenceKeyArea::PasteKeys(const FMovieSceneClipboardKeyTrack& KeyTrack, const FMovieSceneClipboardEnvironment& SrcEnvironment, const FSequencerPasteEnvironment& DstEnvironment)
{
	float PasteAt = DstEnvironment.CardinalTime;

	KeyTrack.IterateKeys([&](const FMovieSceneClipboardKey& Key) {
		UMovieSceneActorReferenceSection* Section = Cast<UMovieSceneActorReferenceSection>(GetOwningSection());
		if (!Section)
		{
			return true;
		}

		if (Section->TryModify())
		{
			float Time = PasteAt + Key.GetTime();
			if (Section->GetStartTime() > Time)
			{
				Section->SetStartTime(Time);
			}
			if (Section->GetEndTime() < Time)
			{
				Section->SetEndTime(Time);
			}

			// We can't paste the curve values directly since the actor reference curve hold indices to
			// the actual guids since there is no FGuid curve.
			// TODO: Fix up this code once there is an FGuid curve.
			FKeyHandle KeyHandle = Section->AddKey(Time, Key.GetValue<FGuid>());
			DstEnvironment.ReportPastedKey(KeyHandle, *this);
		}

		return true;
	});
}


void FActorReferenceKeyArea::EvaluateAndAddKey(float Time, float TimeToCopyFrom, FKeyHandle& CurrentKey)
{
	UMovieSceneActorReferenceSection* ActorReferenceSection = Cast<UMovieSceneActorReferenceSection>(OwningSection);
	checkf(ActorReferenceSection, TEXT("Incompatible section in FActorReferenceKeyArea"));

	// Add the keys to the section here instead of the curve because the actor reference section doesn't
	// store the guids directly in the curve.
	// TODO: Fix up this code once there is an FGuid curve.
	if (ActorReferenceSection != nullptr)
	{
		FGuid Value;
		if (ExternalValue.IsSet() && ExternalValue.Get().IsSet() && TimeToCopyFrom == FLT_MAX)
		{
			Value = ExternalValue.Get().GetValue();
		}
		else
		{
			float EvalTime = TimeToCopyFrom != FLT_MAX ? Time : TimeToCopyFrom;
			Value = ActorReferenceSection->Eval(EvalTime);
		}

		ActorReferenceSection->AddKey(Time, Value);
	}
}


void FActorReferenceKeyArea::UpdateKeyWithExternalValue(float Time)
{
	UMovieSceneActorReferenceSection* ActorReferenceSection = Cast<UMovieSceneActorReferenceSection>(OwningSection);
	checkf(ActorReferenceSection, TEXT("Incompatible section in FActorReferenceKeyArea"));

	// Set the key through the section here instead of the curve because the actor reference section doesn't
	// store the guids directly in the curve.
	if (ExternalValue.IsSet() && ExternalValue.Get().IsSet())
	{
		ActorReferenceSection->AddKey(Time, ExternalValue.Get().GetValue());
	}
}


void FActorReferencePropertySection::GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const
{
	UMovieSceneActorReferenceSection* ActorReferenceSection = Cast<UMovieSceneActorReferenceSection>( &SectionObject );

	TAttribute<TOptional<FGuid>> ActorGuidExternalValue = TAttribute<TOptional<FGuid>>::Create(
		TAttribute<TOptional<FGuid>>::FGetter::CreateRaw(this, &FActorReferencePropertySection::GetActorGuid));
	TSharedRef<FActorReferenceKeyArea> KeyArea = MakeShareable( new FActorReferenceKeyArea( ActorReferenceSection->GetActorReferenceCurve(), ActorGuidExternalValue, ActorReferenceSection ) );
	LayoutBuilder.SetSectionAsKeyArea( KeyArea );
}


TOptional<FGuid> FActorReferencePropertySection::GetActorGuid() const
{
	TOptional<AActor*> CurrentActor = GetPropertyValue<AActor*>();
	if (CurrentActor.IsSet() && CurrentActor.GetValue() != nullptr)
	{
		ISequencer* SequencerPtr = GetSequencer();
		return TOptional<FGuid>(SequencerPtr->FindObjectId(*CurrentActor.GetValue(), SequencerPtr->GetFocusedTemplateID()));
	}
	return TOptional<FGuid>(FGuid());
}
