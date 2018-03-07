// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/UnrealType.h"
#include "UObject/ObjectKey.h"
#include "Curves/KeyHandle.h"

class AActor;
class UCameraComponent;
class UMovieSceneSection;
class USceneComponent;
struct FRichCurve;
enum class EMovieSceneKeyInterpolation : uint8;

class MOVIESCENE_API MovieSceneHelpers
{
public:

	/**
	 * Gets the sections that were traversed over between the current time and the previous time, including overlapping sections
	 */
	static TArray<UMovieSceneSection*> GetAllTraversedSections( const TArray<UMovieSceneSection*>& Sections, float CurrentTime, float PreviousTime );

	/**
	 * Gets the sections that were traversed over between the current time and the previous time, excluding overlapping sections (highest wins)
	 */
	static TArray<UMovieSceneSection*> GetTraversedSections( const TArray<UMovieSceneSection*>& Sections, float CurrentTime, float PreviousTime );

	/**
	 * Finds a section that exists at a given time
	 *
	 * @param Time	The time to find a section at
	 * @return The found section or null
	 */
	static UMovieSceneSection* FindSectionAtTime( const TArray<UMovieSceneSection*>& Sections, float Time );

	/**
	 * Finds the nearest section to the given time
	 *
	 * @param Time	The time to find a section at
	 * @return The found section or null
	 */
	static UMovieSceneSection* FindNearestSectionAtTime( const TArray<UMovieSceneSection*>& Sections, float Time );

	/*
	 * Fix up consecutive sections so that there are no gaps
	 * 
	 * @param Sections All the sections
	 * @param Section The section that was modified 
	 * @param bDelete Was this a deletion?
	 */
	static void FixupConsecutiveSections(TArray<UMovieSceneSection*>& Sections, UMovieSceneSection& Section, bool bDelete);

	/*
 	 * Sort consecutive sections so that they are in order based on start time
 	 */
	static void SortConsecutiveSections(TArray<UMovieSceneSection*>& Sections);

	/**
	 * Get the scene component from the runtime object
	 *
	 * @param Object The object to get the scene component for
	 * @return The found scene component
	 */	
	static USceneComponent* SceneComponentFromRuntimeObject(UObject* Object);

	/**
	 * Get the active camera component from the actor 
	 *
	 * @param InActor The actor to look for the camera component on
	 * @return The active camera component
	 */
	static UCameraComponent* CameraComponentFromActor(const AActor* InActor);

	/**
	 * Find and return camera component from the runtime object
	 *
	 * @param Object The object to get the camera component for
	 * @return The found camera component
	 */	
	static UCameraComponent* CameraComponentFromRuntimeObject(UObject* RuntimeObject);

	/**
	 * Set the runtime object movable
	 *
	 * @param Object The object to set the mobility for
	 * @param Mobility The mobility of the runtime object
	 */
	static void SetRuntimeObjectMobility(UObject* Object, EComponentMobility::Type ComponentMobility = EComponentMobility::Movable);

	/*
	 * Set the key interpolation
	 *
	 * @param InCurve The curve that contains the key handle to set
	 * @param InKeyHandle The key handle to set
	 * @param InInterpolation The interpolation to set
	 */
	static void SetKeyInterpolation(FRichCurve& InCurve, FKeyHandle InKeyHandle, EMovieSceneKeyInterpolation InKeyInterpolation);
};

/**
 * Manages bindings to keyed properties for a track instance. 
 * Calls UFunctions to set the value on runtime objects
 */
class MOVIESCENE_API FTrackInstancePropertyBindings
{
public:
	FTrackInstancePropertyBindings( FName InPropertyName, const FString& InPropertyPath, const FName& InFunctionName = FName(), const FName& InNotifyFunctionName = FName());

	/**
	 * Calls the setter function for a specific runtime object or if the setter function does not exist, the property is set directly
	 *
	 * @param InRuntimeObject The runtime object whose function to call
	 * @param PropertyValue The new value to assign to the property
	 */
	template <typename ValueType>
	void CallFunction( UObject& InRuntimeObject, typename TCallTraits<ValueType>::ParamType PropertyValue )
	{
		FPropertyAndFunction PropAndFunction = FindOrAdd(InRuntimeObject);
		if (UFunction* SetterFunction = PropAndFunction.SetterFunction.Get())
		{
			// ProcessEvent should really be taking const void*
			InRuntimeObject.ProcessEvent(SetterFunction, (void*)&PropertyValue);
		}
		else if (ValueType* Val = PropAndFunction.GetPropertyAddress<ValueType>())
		{
			*Val = MoveTempIfPossible(PropertyValue);
		}

		if (UFunction* NotifyFunction = PropAndFunction.NotifyFunction.Get())
		{
			InRuntimeObject.ProcessEvent(NotifyFunction, nullptr);
		}
	}

	/**
	 * Calls the setter function for a specific runtime object or if the setter function does not exist, the property is set directly
	 *
	 * @param InRuntimeObject The runtime object whose function to call
	 * @param PropertyValue The new value to assign to the property
	 */
	void CallFunctionForEnum( UObject& InRuntimeObject, int64 PropertyValue );

	/**
	 * Rebuilds the property and function mappings for a single runtime object, and adds them to the cache
	 *
	 * @param InRuntimeObject	The object to cache mappings for
	 */
	void CacheBinding( const UObject& InRuntimeObject );

	/**
	 * Gets the UProperty that is bound to the track instance
	 *
	 * @param Object	The Object that owns the property
	 * @return			The property on the object if it exists
	 */
	UProperty* GetProperty(const UObject& Object) const;

	/**
	 * Gets the current value of a property on an object
	 *
	 * @param Object	The object to get the property from
	 * @return ValueType	The current value
	 */
	template <typename ValueType>
	ValueType GetCurrentValue(const UObject& Object)
	{
		FPropertyAndFunction PropAndFunction = FindOrAdd(Object);

		const ValueType* Val = PropAndFunction.GetPropertyAddress<ValueType>();
		return Val ? *Val : ValueType();
	}

	/**
	 * Gets the current value of a property on an object
	 *
	 * @param Object	The object to get the property from
	 * @return ValueType	The current value
	 */
	int64 GetCurrentValueForEnum(const UObject& Object);

	/**
	 * Sets the current value of a property on an object
	 *
	 * @param Object	The object to set the property on
	 * @param InValue   The value to set
	 */
	template <typename ValueType>
	void SetCurrentValue(UObject& Object, typename TCallTraits<ValueType>::ParamType InValue)
	{
		FPropertyAndFunction PropAndFunction = FindOrAdd(Object);

		if(ValueType* Val = PropAndFunction.GetPropertyAddress<ValueType>())
		{
			*Val = InValue;

			if (UFunction* NotifyFunction = PropAndFunction.NotifyFunction.Get())
			{
				Object.ProcessEvent(NotifyFunction, nullptr);
			}
		}
	}

	/** @return the property path that this binding was initialized from */
	const FString& GetPropertyPath() const
	{
		return PropertyPath;
	}

	/** @return the property name that this binding was initialized from */
	const FName& GetPropertyName() const
	{
		return PropertyName;
	}

	template<typename ValueType>
	DEPRECATED(4.15, "Please use GetCurrentValue(const UObject&)")
	ValueType GetCurrentValue(const UObject* Object) { check(Object) return GetCurrentValue<ValueType>(*Object); }
	template <typename ValueType>
	DEPRECATED(4.15, "Please use CallFunction(UObject&)")
	void CallFunction( UObject* InRuntimeObject, ValueType* PropertyValue ) { CallFunction<ValueType>(*InRuntimeObject, *PropertyValue); }
	DEPRECATED(4.15, "UpdateBindings is no longer necessary")
	void UpdateBindings( const TArray<TWeakObjectPtr<UObject>>& InRuntimeObjects ){}
	DEPRECATED(4.15, "UpdateBindings is no longer necessary")
	void UpdateBinding( const TWeakObjectPtr<UObject>& InRuntimeObject ) {}

private:

	struct FPropertyAddress
	{
		TWeakObjectPtr<UProperty> Property;
		void* Address;

		UProperty* GetProperty() const
		{
			UProperty* PropertyPtr = Property.Get();
			if (PropertyPtr && Address && !PropertyPtr->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
			{
				return PropertyPtr;
			}
			return nullptr;
		}

		FPropertyAddress()
			: Property(nullptr)
			, Address(nullptr)
		{}
	};

	struct FPropertyAndFunction
	{
		FPropertyAddress PropertyAddress;
		TWeakObjectPtr<UFunction> SetterFunction;
		TWeakObjectPtr<UFunction> NotifyFunction;

		template<typename ValueType>
		ValueType* GetPropertyAddress() const
		{
			UProperty* PropertyPtr = PropertyAddress.GetProperty();
			return PropertyPtr ? PropertyPtr->ContainerPtrToValuePtr<ValueType>(PropertyAddress.Address) : nullptr;
		}

		FPropertyAndFunction()
			: PropertyAddress()
			, SetterFunction( nullptr )
			, NotifyFunction( nullptr )
		{}
	};

	static FPropertyAddress FindPropertyRecursive(void* BasePointer, UStruct* InStruct, TArray<FString>& InPropertyNames, uint32 Index);
	static FPropertyAddress FindProperty(const UObject& Object, const FString& InPropertyPath);

	/** Find or add the FPropertyAndFunction for the specified object */
	FPropertyAndFunction FindOrAdd(const UObject& InObject)
	{
		FObjectKey ObjectKey(&InObject);

		const FPropertyAndFunction* PropAndFunction = RuntimeObjectToFunctionMap.Find(ObjectKey);
		if (PropAndFunction && (PropAndFunction->SetterFunction.IsValid() || PropAndFunction->PropertyAddress.Property.IsValid()))
		{
			return *PropAndFunction;
		}

		CacheBinding(InObject);
		return RuntimeObjectToFunctionMap.FindRef(ObjectKey);
	}

private:
	/** Mapping of objects to bound functions that will be called to update data on the track */
	TMap< FObjectKey, FPropertyAndFunction > RuntimeObjectToFunctionMap;

	/** Path to the property we are bound to */
	FString PropertyPath;

	/** Name of the function to call to set values */
	FName FunctionName;

	/** Name of a function to call when a value has been set */
	FName NotifyFunctionName;

	/** Actual name of the property we are bound to */
	FName PropertyName;

};

/** Explicit specializations for bools */
template<> MOVIESCENE_API void FTrackInstancePropertyBindings::CallFunction<bool>(UObject& InRuntimeObject, TCallTraits<bool>::ParamType PropertyValue);
template<> MOVIESCENE_API bool FTrackInstancePropertyBindings::GetCurrentValue<bool>(const UObject& Object);
template<> MOVIESCENE_API void FTrackInstancePropertyBindings::SetCurrentValue<bool>(UObject& Object, TCallTraits<bool>::ParamType InValue);
