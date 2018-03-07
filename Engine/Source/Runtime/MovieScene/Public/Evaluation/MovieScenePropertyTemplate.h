// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "IMovieScenePlayer.h"
#include "MovieSceneFwd.h"
#include "MovieSceneCommonHelpers.h"
#include "Evaluation/MovieSceneAnimTypeID.h"
#include "MovieSceneExecutionToken.h"
#include "Evaluation/PersistentEvaluationData.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "MovieSceneSequence.h"
#include "MovieSceneBlendingActuator.h"
#include "MovieScenePropertyTemplate.generated.h"

class UMovieScenePropertyTrack;

DECLARE_CYCLE_STAT(TEXT("Property Track Token Execute"), MovieSceneEval_PropertyTrack_TokenExecute, STATGROUP_MovieSceneEval);

namespace PropertyTemplate
{
	/** Persistent section data for a property section */
	struct FSectionData : IPersistentEvaluationData
	{
		MOVIESCENE_API FSectionData();

		/** Initialize track data with the specified property name, path, optional setter function, and optional notify function */
		MOVIESCENE_API void Initialize(FName InPropertyName, FString InPropertyPath, FName InFunctionName = NAME_None, FName InNotifyFunctionName = NAME_None);

		/** Property bindings used to get and set the property */
		TSharedPtr<FTrackInstancePropertyBindings> PropertyBindings;

		/** Cached identifier of the property we're editing */
		FMovieSceneAnimTypeID PropertyID;
	};

	/** The value of the object as it existed before this frame's evaluation */
	template<typename PropertyValueType> struct
	DEPRECATED(4.17, "Precaching of property values should no longer be necessary as it was only used to pass default values to curves on evaluation. Curves should now be checked for emptyness before attempting to animate an object.")
	TCachedValue
	{
		TWeakObjectPtr<UObject> WeakObject;
		PropertyValueType Value;
	};

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	template<typename T> struct TCachedValue_DEPRECATED : TCachedValue<T> {};
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	template<typename PropertyValueType> struct 
	DEPRECATED(4.17, "Precaching of property values should no longer be necessary as it was only used to pass default values to curves on evaluation. Curves should now be checked for emptyness before attempting to animate an object.")
	TCachedSectionData : FSectionData
	{
		/** Setup the data for the current frame */
		void SetupFrame(const FMovieSceneEvaluationOperand& Operand, IMovieScenePlayer& Player)
		{
			ObjectsAndValues.Reset();
			
			for (TWeakObjectPtr<> Object : Player.FindBoundObjects(Operand))
			{
				if (UObject* ObjectPtr = Object.Get())
				{
					PropertyBindings->CacheBinding(*ObjectPtr);
					if (UProperty* Property = PropertyBindings->GetProperty(*ObjectPtr))
					{
						if (Property->GetSize() == sizeof(PropertyValueType))
						{
							ObjectsAndValues.Add(TCachedValue_DEPRECATED<PropertyValueType>{ ObjectPtr, PropertyBindings->GetCurrentValue<PropertyValueType>(*ObjectPtr) });
						}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
						else
						{
							FMessageLog("Sequencer").Warning()
								->AddToken(FUObjectToken::Create(Player.GetEvaluationTemplate().GetSequence(MovieSceneSequenceID::Root)))
								->AddToken(FTextToken::Create(FText::Format(NSLOCTEXT("MovieScene", "IncompatibleDataWarning", "Property size mismatch for property '{0}'. Expected '{1}', found '{2}'. Recreate the track with the new property type."), FText::FromString(PropertyBindings->GetPropertyPath()), FText::FromString(TNameOf<PropertyValueType>::GetName()), FText::FromString(Property->GetCPPType()))));
						}
#endif
					}
				}
			}
		}

		TArray<TCachedValue_DEPRECATED<PropertyValueType>, TInlineAllocator<1>> ObjectsAndValues;
	};

	template<typename PropertyValueType, typename IntermediateType = PropertyValueType>
	struct TTemporarySetterType { typedef PropertyValueType Type; };

	/** Convert from an intermediate type to the type used for setting a property value. Called when resetting pre animated state */
	template<typename PropertyValueType, typename IntermediateType = PropertyValueType>
	typename TTemporarySetterType<PropertyValueType, IntermediateType>::Type
		ConvertFromIntermediateType(const IntermediateType& InIntermediateType, IMovieScenePlayer& Player)
	{
		return InIntermediateType;
	}

	/** Convert from an intermediate type to the type used for setting a property value. Called during token execution. */
	template<typename PropertyValueType, typename IntermediateType = PropertyValueType>
	typename TTemporarySetterType<PropertyValueType, IntermediateType>::Type
		ConvertFromIntermediateType(const IntermediateType& InIntermediateType, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player)
	{
		return InIntermediateType;
	}
	
	template<typename PropertyValueType, typename IntermediateType = PropertyValueType>
	IntermediateType ConvertToIntermediateType(PropertyValueType&& NewValue)
	{
		return Forward<PropertyValueType>(NewValue);
	}

	template<typename T>
	bool IsValueValid(const T& InValue)
	{
		return true;
	}

	/** Cached preanimated state for a given property */
	template<typename PropertyValueType, typename IntermediateType = PropertyValueType>
	struct TCachedState : IMovieScenePreAnimatedToken
	{
		TCachedState(typename TCallTraits<IntermediateType>::ParamType InValue, const FTrackInstancePropertyBindings& InBindings)
			: Value(MoveTempIfPossible(InValue))
			, Bindings(InBindings)
		{
		}

		virtual void RestoreState(UObject& Object, IMovieScenePlayer& Player) override
		{
			auto NewValue = PropertyTemplate::ConvertFromIntermediateType<PropertyValueType, IntermediateType>(Value, Player);
			if (IsValueValid(NewValue))
			{
				Bindings.CallFunction<PropertyValueType>(Object, NewValue);
			}
		}

		IntermediateType Value;
		FTrackInstancePropertyBindings Bindings;
	};

	template<typename PropertyValueType, typename IntermediateType = PropertyValueType>
	static IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object, FTrackInstancePropertyBindings& PropertyBindings)
	{
		return TCachedState<PropertyValueType, IntermediateType>(PropertyTemplate::ConvertToIntermediateType(PropertyBindings.GetCurrentValue<PropertyValueType>(Object)), PropertyBindings);
	}


	template<typename PropertyValueType>
	struct FTokenProducer : IMovieScenePreAnimatedTokenProducer
	{
		FTokenProducer(FTrackInstancePropertyBindings& InPropertyBindings) : PropertyBindings(InPropertyBindings) {}

		virtual IMovieScenePreAnimatedTokenPtr CacheExistingState(UObject& Object) const
		{
			return PropertyTemplate::CacheExistingState<PropertyValueType>(Object, PropertyBindings);
		}

		FTrackInstancePropertyBindings& PropertyBindings;
	};
}


template<typename PropertyValueType> struct
DEPRECATED(4.17, "Precaching of property values should no longer be necessary as it was only used to pass default values to curves on evaluation. Curves should now be checked for emptyness before attempting to animate an object.")
TCachedPropertyTrackExecutionToken : IMovieSceneExecutionToken
{
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		using namespace PropertyTemplate;

		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_PropertyTrack_TokenExecute)
		
		TCachedSectionData<PropertyValueType>& PropertyTrackData = PersistentData.GetSectionData<TCachedSectionData<PropertyValueType>>();
		FTrackInstancePropertyBindings* PropertyBindings = PropertyTrackData.PropertyBindings.Get();

		for (TCachedValue<PropertyValueType>& ObjectAndValue : PropertyTrackData.ObjectsAndValues)
		{
			if (UObject* ObjectPtr = ObjectAndValue.WeakObject.Get())
			{
				Player.SavePreAnimatedState(*ObjectPtr, PropertyTrackData.PropertyID, FTokenProducer<PropertyValueType>(*PropertyBindings));
				
				PropertyBindings->CallFunction<PropertyValueType>(*ObjectPtr, ObjectAndValue.Value);
			}
		}
	}
};

/** Execution token that simple stores a value, and sets it when executed */
template<typename PropertyValueType, typename IntermediateType = PropertyValueType>
struct TPropertyTrackExecutionToken : IMovieSceneExecutionToken
{
	TPropertyTrackExecutionToken(IntermediateType InValue)
		: Value(MoveTemp(InValue))
	{}

	/** Execute this token, operating on all objects referenced by 'Operand' */
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		using namespace PropertyTemplate;

		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_PropertyTrack_TokenExecute)
		
		FSectionData& PropertyTrackData = PersistentData.GetSectionData<FSectionData>();
		FTrackInstancePropertyBindings* PropertyBindings = PropertyTrackData.PropertyBindings.Get();

		auto NewValue = PropertyTemplate::ConvertFromIntermediateType<PropertyValueType, IntermediateType>(Value, Operand, PersistentData, Player);
		if (!IsValueValid(NewValue))
		{
			return;
		}

		for (TWeakObjectPtr<> WeakObject : Player.FindBoundObjects(Operand))
		{
			if (UObject* ObjectPtr = WeakObject.Get())
			{
				Player.SavePreAnimatedState(*ObjectPtr, PropertyTrackData.PropertyID, FTokenProducer<PropertyValueType>(*PropertyBindings));
				
				PropertyBindings->CallFunction<PropertyValueType>(*ObjectPtr, NewValue);
			}
		}
	}

	IntermediateType Value;
};

/** Blending actuator type that knows how to apply values of type PropertyType */
template<typename PropertyType>
struct TPropertyActuator : TMovieSceneBlendingActuator<PropertyType>
{
	PropertyTemplate::FSectionData PropertyData;
	TPropertyActuator(const PropertyTemplate::FSectionData& InPropertyData)
		: TMovieSceneBlendingActuator<PropertyType>(FMovieSceneBlendingActuatorID(InPropertyData.PropertyID))
		, PropertyData(InPropertyData)
	{}

	virtual PropertyType RetrieveCurrentValue(UObject* InObject, IMovieScenePlayer* Player) const
	{
		return PropertyData.PropertyBindings->GetCurrentValue<PropertyType>(*InObject);
	}

	virtual void Actuate(UObject* InObject, typename TCallTraits<PropertyType>::ParamType InFinalValue, const TBlendableTokenStack<PropertyType>& OriginalStack, const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		check(InObject);

		OriginalStack.SavePreAnimatedState(Player, *InObject, PropertyData.PropertyID, PropertyTemplate::FTokenProducer<PropertyType>(*PropertyData.PropertyBindings));
		PropertyData.PropertyBindings->CallFunction<PropertyType>(*InObject, InFinalValue);
	}
};

USTRUCT()
struct FMovieScenePropertySectionData
{
	GENERATED_BODY()

	UPROPERTY()
	FName PropertyName;

	UPROPERTY()
	FString PropertyPath;

	UPROPERTY()
	FName FunctionName;

	UPROPERTY()
	FName NotifyFunctionName;

	FMovieScenePropertySectionData()
	{}

	FMovieScenePropertySectionData(FName InPropertyName, FString InPropertyPath, FName InFunctionName = NAME_None, FName InNotifyFunctionName = NAME_None)
		: PropertyName(InPropertyName), PropertyPath(MoveTemp(InPropertyPath)), FunctionName(InFunctionName), NotifyFunctionName(InNotifyFunctionName)
	{}

	/** Helper function to create FSectionData for a property section */
	void SetupTrack(FPersistentEvaluationData& PersistentData) const
	{
		SetupTrack<PropertyTemplate::FSectionData>(PersistentData);
	}

	template<typename T>
	void SetupTrack(FPersistentEvaluationData& PersistentData) const
	{
		PersistentData.AddSectionData<T>().Initialize(PropertyName, PropertyPath, FunctionName, NotifyFunctionName);
	}

	template<typename T>
	DEPRECATED(4.17, "Precaching of property values should no longer be necessary as it was only used to pass default values to curves on evaluation. Curves should now be checked for emptyness before attempting to animate an object.")
	void SetupCachedTrack(FPersistentEvaluationData& PersistentData) const
	{
		typedef PropertyTemplate::TCachedSectionData<T> FSectionData;
		PersistentData.AddSectionData<FSectionData>().Initialize(PropertyName, PropertyPath, FunctionName, NotifyFunctionName);
	}

	template<typename T>
	DEPRECATED(4.17, "Precaching of property values should no longer be necessary as it was only used to pass default values to curves on evaluation. Curves should now be checked for emptyness before attempting to animate an object.")
	void SetupCachedFrame(const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
	{
		typedef PropertyTemplate::TCachedSectionData<T> FSectionData;
		PersistentData.GetSectionData<FSectionData>().SetupFrame(Operand, Player);
	}
};

USTRUCT()
struct MOVIESCENE_API FMovieScenePropertySectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()
	
	FMovieScenePropertySectionTemplate(){}
	FMovieScenePropertySectionTemplate(FName PropertyName, const FString& InPropertyPath);

protected:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	/** Setup is only called if derived classes enable RequiresSetupFlag */
	virtual void Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;

	/** Access an animation type identifier that uniquely represents the property this section animates */
	FMovieSceneAnimTypeID GetPropertyTypeID() const;

	/** Ensure that an actutor for this property type exists */
	template<typename PropertyType> FMovieSceneBlendingActuatorID EnsureActuator(FMovieSceneBlendingAccumulator& Accumulator) const
	{
		// Actuator type ID for this property
		FMovieSceneAnimTypeID UniquePropertyID = GetPropertyTypeID();
		FMovieSceneBlendingActuatorID ActuatorTypeID = FMovieSceneBlendingActuatorID(UniquePropertyID);
		if (!Accumulator.FindActuator<PropertyType>(ActuatorTypeID))
		{
			PropertyTemplate::FSectionData SectionData;
			SectionData.Initialize(PropertyData.PropertyName, PropertyData.PropertyPath, PropertyData.FunctionName, PropertyData.NotifyFunctionName);

			Accumulator.DefineActuator(ActuatorTypeID, MakeShared<TPropertyActuator<PropertyType>>(SectionData));
		}
		return ActuatorTypeID;
	}

	UPROPERTY()
	FMovieScenePropertySectionData PropertyData;
};