// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Curves/KeyHandle.h"
#include "Curves/IntegralCurve.h"
#include "MovieSceneTrack.h"
#include "NamedKeyArea.h"
#include "SequencerClipboardReconciler.h"
#include "ClipboardTypes.h"

class FStructOnScope;

/**
 * Abstract base class for integral curve key areas.
 */
class MOVIESCENETOOLS_API FIntegralCurveKeyAreaBase
	: public FNamedKeyArea
{
public:

	/** Create and initialize a new instance. */
	FIntegralCurveKeyAreaBase(FIntegralCurve& InCurve, UMovieSceneSection* InOwningSection)
		: Curve(InCurve)
		, OwningSection(InOwningSection)
	{ }

public:

	//~ IKeyArea interface

	virtual TArray<FKeyHandle> AddKeyUnique( float Time, EMovieSceneKeyInterpolation InKeyInterpolation, float TimeToCopyFrom = FLT_MAX ) override;
	virtual TOptional<FKeyHandle> DuplicateKey(FKeyHandle KeyToDuplicate) override;
	virtual void DeleteKey(FKeyHandle KeyHandle) override;
	virtual TOptional<FLinearColor> GetColor() override;
	virtual ERichCurveExtrapolation GetExtrapolationMode(bool bPreInfinity) const override;
	virtual ERichCurveInterpMode GetKeyInterpMode(FKeyHandle KeyHandle) const override;
	virtual TSharedPtr<FStructOnScope> GetKeyStruct(FKeyHandle KeyHandle) override;
	virtual ERichCurveTangentMode GetKeyTangentMode(FKeyHandle KeyHandle) const override;
	virtual float GetKeyTime(FKeyHandle KeyHandle) const override;
	virtual UMovieSceneSection* GetOwningSection() override;
	virtual FRichCurve* GetRichCurve() override;
	virtual TArray<FKeyHandle> GetUnsortedKeyHandles() const override;
	virtual FKeyHandle DilateKey(FKeyHandle KeyHandle, float Scale, float Origin) override;
	virtual FKeyHandle MoveKey(FKeyHandle KeyHandle, float DeltaPosition) override;
	virtual void SetExtrapolationMode(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity) override;
	virtual void SetKeyInterpMode(FKeyHandle KeyHandle, ERichCurveInterpMode InterpMode) override;
	virtual void SetKeyTangentMode(FKeyHandle KeyHandle, ERichCurveTangentMode TangentMode) override;
	virtual void SetKeyTime(FKeyHandle KeyHandle, float NewKeyTime) override;

protected:

	virtual void EvaluateAndAddKey(float Time, float TimeToCopyFrom, FKeyHandle& CurrentKey) = 0;
	virtual void UpdateKeyWithExternalValue(float Time) = 0;

protected:

	/** Curve with keys in this area */
	FIntegralCurve& Curve;

	/** The section that owns this key area. */
	UMovieSceneSection* OwningSection;
};


/**
 * Template for integral curve key areas.
 */
template<typename IntegralType>
class MOVIESCENETOOLS_API FIntegralKeyArea
	: public FIntegralCurveKeyAreaBase
{
public:

	/**
	* Creates a new key area for editing integral curves.
	*
	* @param InCurve The integral curve which has the integral keys.
	* @param InOwningSection The section which owns the curve which is being displayed and edited by this area.
	*/
	FIntegralKeyArea(FIntegralCurve& InCurve, UMovieSceneSection* InOwningSection)
		: FIntegralCurveKeyAreaBase(InCurve, InOwningSection)
	{ }

	/**
	* Creates a new key area for editing integral curves whose value can be overridden externally.
	*
	* @param InCurve The integral curve which has the integral keys.
	* @param InExternalValue An attribute which can provide an external value for this key area.  External values are
	*        useful for things like property tracks where the property value can change without changing the animation
	*        and we want to be able to key and update using the new property value.
	* @param InOwningSection The section which owns the curve which is being displayed and edited by this area.
	*/
	FIntegralKeyArea(FIntegralCurve& InCurve, TAttribute<TOptional<IntegralType>> InExternalValue, UMovieSceneSection* InOwningSection)
		: FIntegralCurveKeyAreaBase(InCurve, InOwningSection)
		, ExternalValue(InExternalValue)
	{ }

	virtual void CopyKeys(FMovieSceneClipboardBuilder& ClipboardBuilder, const TFunctionRef<bool(FKeyHandle, const IKeyArea&)>& KeyMask) const override
	{
		const UMovieSceneSection* Section = const_cast<FIntegralKeyArea*>(this)->GetOwningSection();
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
					KeyTrack = &ClipboardBuilder.FindOrAddKeyTrack<IntegralType>(GetName(), *Track);
				}

				FIntegralKey Key = Curve.GetKey(Handle);
				IntegralType Value = MovieSceneClipboard::TImplicitConversionFacade<decltype(Key.Value), IntegralType>::Cast(Key.Value);
				KeyTrack->AddKey(Key.Time, Value);
			}
		}
	}

	virtual void PasteKeys(const FMovieSceneClipboardKeyTrack& KeyTrack, const FMovieSceneClipboardEnvironment& SrcEnvironment, const FSequencerPasteEnvironment& DstEnvironment) override
	{
		float PasteAt = DstEnvironment.CardinalTime;

		KeyTrack.IterateKeys([&](const FMovieSceneClipboardKey& Key){
			UMovieSceneSection* Section = GetOwningSection();
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

				FKeyHandle KeyHandle = Curve.UpdateOrAddKey(Time, Key.GetValue<IntegralType>());
				DstEnvironment.ReportPastedKey(KeyHandle, *this);
			}
				
			return true;
		});
	}

protected:

	virtual IntegralType ConvertCurveValueToIntegralType(int32 CurveValue) const = 0;

	virtual void EvaluateAndAddKey(float Time, float TimeToCopyFrom, FKeyHandle& CurrentKey) override
	{
		IntegralType Value;
		if (ExternalValue.IsSet() && ExternalValue.Get().IsSet() && TimeToCopyFrom == FLT_MAX)
		{
			Value = ExternalValue.Get().GetValue();
		}
		else
		{
			float EvalTime = TimeToCopyFrom != FLT_MAX ? Time : TimeToCopyFrom;
			IntegralType DefaultValue = 0;
			Value = ConvertCurveValueToIntegralType(Curve.Evaluate(EvalTime, DefaultValue));
		}

		Curve.AddKey(Time, Value, CurrentKey);
	}

	virtual void UpdateKeyWithExternalValue(float Time) override
	{
		if (ExternalValue.IsSet() && ExternalValue.Get().IsSet())
		{
			IntegralType Value = ExternalValue.Get().GetValue();

			Curve.UpdateOrAddKey(Time, Value);
		}
	}

protected:

	TAttribute<TOptional<IntegralType>> ExternalValue;
};
