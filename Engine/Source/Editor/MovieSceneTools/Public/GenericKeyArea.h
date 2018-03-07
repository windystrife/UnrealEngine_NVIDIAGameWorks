// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#include "Curves/KeyHandle.h"
#include "NamedKeyArea.h"
#include "ClipboardTypes.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "UObject/StructOnScope.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Curves/IKeyFrameManipulator.h"
#include "Curves/CurveInterface.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SSpinBox.h"
#include "SequencerClipboardReconciler.h"
#include "IDetailsView.h"
#include "GenericKeyArea.generated.h"

class ISequencer;
struct FSequencerPasteEnvironment;

/** Interface for editing a key value/time */
struct ICurveEditInterface
{
	virtual ~ICurveEditInterface() {}

	/** Extend the specified (empty) details customization with the specified key handle */
	virtual void Extend(FKeyHandle KeyHandle, IDetailLayoutBuilder& DetailBuilder) = 0;
};

/** Largely redundant structure that is used to forward detail customizations for keys */
USTRUCT()
struct FMovieSceneCurveInterfaceKeyEditStruct
{
	GENERATED_BODY()

	UPROPERTY()
	bool bBenignPropertyToEnsurePropertyCustomization;

	/** The key handle to edit */
	FKeyHandle KeyHandle;

	/** The curve interface to edit with */
	ICurveEditInterface* EditInterface;
};

/**
 * A generic key area that utilizes TCurveInterface for interaction
 */
template<typename KeyValueType, typename TimeType>
class TGenericKeyArea
	: public FNamedKeyArea
	, public ICurveEditInterface
{
public:

	/**
	 * Create and initialize a new instance.
	 *
	 * @param InCurve The curve to assign to this key area.
	 * @param InOwningSection The section that owns this key area.
	 */
	TGenericKeyArea(TArray<TimeType>* KeyTimesParam, TArray<KeyValueType>* KeyValuesParam, UMovieSceneSection* InOwningSection)
		: CurveInterface(KeyTimesParam, KeyValuesParam)
		, OwningSection(InOwningSection)
	{ }

	TGenericKeyArea(TCurveInterface<KeyValueType, TimeType>&& InCurveInterface, UMovieSceneSection* InOwningSection)
		: CurveInterface(InCurveInterface)
		, OwningSection(InOwningSection)
	{ }

public:

	//~ IKeyArea interface

	virtual TArray<FKeyHandle> AddKeyUnique(TimeType Time, EMovieSceneKeyInterpolation InKeyInterpolation, TimeType TimeToCopyFrom) override
	{
		ModifySection();

		TArray<FKeyHandle> AddedKeyHandles;
		TOptional<FKeyHandle> CurrentKey = CurveInterface.FindKey(
			[=](TimeType ExistingTime)
			{
				return FMath::IsNearlyEqual(Time, ExistingTime, KINDA_SMALL_NUMBER);
			}
		);

		if (CurrentKey.IsSet())
		{
			return AddedKeyHandles;
		}

		ExtendSectionBounds(Time);

		FKeyHandle Handle = CurveInterface.AddKey(Time);
		AddedKeyHandles.Add(Handle);

		return AddedKeyHandles;
	}

	virtual TOptional<FKeyHandle> DuplicateKey(FKeyHandle KeyToDuplicate) override
	{
		ModifySection();

		TOptional<TKeyFrameProxy<KeyValueType, TimeType>> Key = CurveInterface.GetKey(KeyToDuplicate);
		if (Key.IsSet())
		{
			return CurveInterface.AddKeyValue(Key->Time, CopyTemp(Key->Value));
		}

		return TOptional<FKeyHandle>();
	}

	virtual void DeleteKey(FKeyHandle KeyHandle) override
	{
		ModifySection();

		CurveInterface.RemoveKey(KeyHandle);
	}

	virtual TimeType GetKeyTime(FKeyHandle KeyHandle) const override
	{
		TOptional<TimeType> Time = CurveInterface.GetKeyTime(KeyHandle);
		return Time.Get(TNumericLimits<TimeType>::Lowest());
	}

	virtual void SetKeyTime(FKeyHandle KeyHandle, TimeType NewKeyTime) override
	{
		ModifySection();

		CurveInterface.SetKeyTime(KeyHandle, NewKeyTime);
		ExtendSectionBounds(NewKeyTime);
	}

	virtual FKeyHandle DilateKey(FKeyHandle KeyHandle, TimeType Scale, TimeType Origin) override
	{
		TOptional<TimeType> Time = CurveInterface.GetKeyTime(KeyHandle);
		if (Time.IsSet())
		{
			ModifySection();
			
			float NewKeyTime = Time.GetValue();
			NewKeyTime = (NewKeyTime - Origin) * Scale + Origin;
			CurveInterface.SetKeyTime(KeyHandle, NewKeyTime);
		}
		return KeyHandle;
	}

	virtual FKeyHandle MoveKey(FKeyHandle KeyHandle, TimeType DeltaPosition) override
	{
		TOptional<TimeType> Time = CurveInterface.GetKeyTime(KeyHandle);
		if (Time.IsSet())
		{
			ModifySection();
			
			CurveInterface.SetKeyTime(KeyHandle, Time.GetValue() + DeltaPosition);
		}
		return KeyHandle;
	}

	virtual UMovieSceneSection* GetOwningSection() override
	{
		return OwningSection.Get();
	}

	virtual TArray<FKeyHandle> GetUnsortedKeyHandles() const override
	{
		TKeyTimeIterator<TimeType> Iterator = CurveInterface.IterateKeys();

		TArray<FKeyHandle> OutKeyHandles;
		OutKeyHandles.Reserve(Iterator.GetEndIndex() - Iterator.GetStartIndex());

		for (auto It = begin(Iterator); It != end(Iterator); ++It)
		{
			OutKeyHandles.Add(It.GetKeyHandle());
		}

		return OutKeyHandles;
	}

	virtual TSharedPtr<FStructOnScope> GetKeyStruct(FKeyHandle KeyHandle) override
	{
		auto Struct = MakeShared<FStructOnScope>(FMovieSceneCurveInterfaceKeyEditStruct::StaticStruct());

		auto* StructPtr = reinterpret_cast<FMovieSceneCurveInterfaceKeyEditStruct*>(Struct->GetStructMemory());
		StructPtr->KeyHandle = KeyHandle;
		StructPtr->EditInterface = this;

		return Struct;
	}

	virtual bool CanCreateKeyEditor() override																{ return false; }
	virtual TSharedRef<SWidget> CreateKeyEditor(ISequencer* Sequencer) override								{ return SNullWidget::NullWidget; }
	virtual TOptional<FLinearColor> GetColor() override														{ return TOptional<FLinearColor>(); }
	virtual ERichCurveExtrapolation GetExtrapolationMode(bool bPreInfinity) const override					{ return RCCE_None; }
	virtual ERichCurveInterpMode GetKeyInterpMode(FKeyHandle Keyhandle) const override						{ return RCIM_None; }
	virtual ERichCurveTangentMode GetKeyTangentMode(FKeyHandle KeyHandle) const override					{ return RCTM_None; }
	virtual void SetExtrapolationMode(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity) override		{}
	virtual void SetKeyInterpMode(FKeyHandle KeyHandle, ERichCurveInterpMode InterpMode) override			{}
	virtual void SetKeyTangentMode(FKeyHandle KeyHandle, ERichCurveTangentMode TangentMode) override		{}
	virtual FRichCurve* GetRichCurve() override																{ return nullptr; }
	
	virtual void CopyKeys(FMovieSceneClipboardBuilder& ClipboardBuilder, const TFunctionRef<bool(FKeyHandle, const IKeyArea&)>& KeyMask) const override
	{
		const UMovieSceneSection* Section = OwningSection.Get();
		UMovieSceneTrack* Track = Section ? Section->GetTypedOuter<UMovieSceneTrack>() : nullptr;
		if (Track)
		{
			FMovieSceneClipboardKeyTrack* KeyTrack = nullptr;
			TKeyIterator<const KeyValueType, TimeType> Iterator = CurveInterface.IterateKeysAndValues();

			for (auto It = begin(Iterator); It != end(Iterator); ++It)
			{
				if (KeyMask(It.GetKeyHandle(), *this))
				{
					if (!KeyTrack)
					{
						KeyTrack = &ClipboardBuilder.FindOrAddKeyTrack<KeyValueType>(GetName(), *Track);
					}

					KeyTrack->AddKey(It->Time, It->Value);
				}
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

				KeyValueType NewKeyValue = Key.GetValue<KeyValueType>();

				FKeyHandle KeyHandle = CurveInterface.UpdateOrAddKey(Time, NewKeyValue, KINDA_SMALL_NUMBER);
				DstEnvironment.ReportPastedKey(KeyHandle, *this);
			}

			return true;
		});
	}

private:

	virtual void Extend(FKeyHandle KeyHandle, IDetailLayoutBuilder& DetailBuilder) override
	{
		TOptional<TKeyFrameProxy<KeyValueType, TimeType>> Key = CurveInterface.GetKey(KeyHandle);
		if (Key.IsSet())
		{
			IDetailCategoryBuilder& GeneralCategory = DetailBuilder.EditCategory("General");

			const IDetailsView* DetailsView = DetailBuilder.GetDetailsView();
			auto OnValueChanged = [=](TimeType InTime)
			{
				this->SetKeyTime(KeyHandle, InTime);
				DetailsView->OnFinishedChangingProperties().Broadcast(FPropertyChangedEvent(nullptr));
			};

			FText TimeText = NSLOCTEXT("GenericKeyArea", "TimeParameter", "Time");
			FText TimeTooltipText = NSLOCTEXT("GenericKeyArea", "TimeParameter_ToolTip", "The time of this key");
			GeneralCategory.AddCustomRow(TimeText, false)
				.NameContent()
				[
					SNew(STextBlock)
					.Text(TimeText)
					.Font(DetailBuilder.GetDetailFont())
					.ToolTipText(TimeTooltipText)
				]
				.ValueContent()
				[
					SNew(SSpinBox<TimeType>)
					.Value_Lambda([=]{ return this->CurveInterface.GetKeyTime(KeyHandle).Get(TimeType()); })
					.OnValueChanged_Lambda(OnValueChanged)
					.OnValueCommitted_Lambda([=](TimeType InTime, ETextCommit::Type){ OnValueChanged(InTime); })
					.ToolTipText(TimeTooltipText)
				];

			TSharedRef<FStructOnScope> KeyValue = MakeShared<FStructOnScope>(KeyValueType::StaticStruct(), (uint8*)&Key->Value);
			GeneralCategory.AddAllExternalStructureProperties(KeyValue);
		}
	}

	void ExtendSectionBounds(TimeType IncludeTime)
	{
		UMovieSceneSection* Section = OwningSection.Get();
		if (!Section)
		{
			return;
		}

		if (Section->GetStartTime() > IncludeTime)
		{
			Section->SetStartTime(IncludeTime);
		}

		if (Section->GetEndTime() < IncludeTime)
		{
			Section->SetEndTime(IncludeTime);
		}
	}

	void ModifySection()
	{
		if (UMovieSceneSection* Section = OwningSection.Get())
		{
			Section->Modify();
		}
	}

protected:

	/** The curve managed by this area. */
	TCurveInterface<KeyValueType, TimeType> CurveInterface;

	/** The section that owns this area. */
	TWeakObjectPtr<UMovieSceneSection> OwningSection;
};
