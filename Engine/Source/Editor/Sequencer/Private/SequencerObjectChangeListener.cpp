// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SequencerObjectChangeListener.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Editor.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IPropertyChangeListener.h"
#include "MovieSceneSequence.h"

DEFINE_LOG_CATEGORY_STATIC(LogSequencerTools, Log, All);

FSequencerObjectChangeListener::FSequencerObjectChangeListener( TSharedRef<ISequencer> InSequencer )
	: Sequencer( InSequencer )
{
	FCoreUObjectDelegates::OnPreObjectPropertyChanged.AddRaw(this, &FSequencerObjectChangeListener::OnObjectPreEditChange);
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FSequencerObjectChangeListener::OnObjectPostEditChange);
	//GEditor->OnPreActorMoved.AddRaw(this, &FSequencerObjectChangeListener::OnActorPreEditMove);
	GEditor->OnActorMoved().AddRaw( this, &FSequencerObjectChangeListener::OnActorPostEditMove );
}

FSequencerObjectChangeListener::~FSequencerObjectChangeListener()
{
	FCoreUObjectDelegates::OnPreObjectPropertyChanged.RemoveAll(this);
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
	GEditor->OnActorMoved().RemoveAll( this );
}

void FSequencerObjectChangeListener::OnPropertyChanged(const TArray<UObject*>& ChangedObjects, const IPropertyHandle& PropertyHandle) const
{
	if (Sequencer.IsValid() && !Sequencer.Pin()->IsAllowedToChange())
	{
		return;
	}

	BroadcastPropertyChanged(FKeyPropertyParams(ChangedObjects, PropertyHandle, ESequencerKeyMode::AutoKey));

	for (UObject* Object : ChangedObjects)
	{
		if (Object)
		{
			const FOnObjectPropertyChanged* Event = ObjectToPropertyChangedEvent.Find(Object);
			if (Event)
			{
				Event->Broadcast(*Object);
			}
		}
	}
}

void FSequencerObjectChangeListener::BroadcastPropertyChanged( FKeyPropertyParams KeyPropertyParams ) const
{
	if (Sequencer.IsValid() && !Sequencer.Pin()->IsAllowedToChange())
	{
		return;
	}

	// Filter out objects that actually have the property path that will be keyable. 
	// Otherwise, this might try to key objects that don't have the requested property.
	// For example, a property changed for the FieldOfView property will be sent for 
	// both the CameraActor and the CameraComponent.
	TArray<UObject*> KeyableObjects;
	FOnAnimatablePropertyChanged Delegate;
	UProperty* Property = nullptr;
	FPropertyPath PropertyPath;
	for (auto ObjectToKey : KeyPropertyParams.ObjectsToKey)
	{
		if (KeyPropertyParams.PropertyPath.GetNumProperties() > 0)
		{
			for (TFieldIterator<UProperty> PropertyIterator(ObjectToKey->GetClass()); PropertyIterator; ++PropertyIterator)
			{
				UProperty* CheckProperty = *PropertyIterator;
				if (CheckProperty == KeyPropertyParams.PropertyPath.GetRootProperty().Property.Get())
				{
					if (CanKeyProperty_Internal(FCanKeyPropertyParams(ObjectToKey->GetClass(), KeyPropertyParams.PropertyPath), Delegate, Property, PropertyPath))
					{
						KeyableObjects.Add(ObjectToKey);
						break;
					}
				}
			}
		}
	}

	if (!KeyableObjects.Num())
	{
		return;
	}

	if (Delegate.IsBound() && PropertyPath.GetNumProperties() > 0 && Property != nullptr)
	{
		// If the property path is not truncated, then we are keying the leafmost property anyways, so set to NAME_None
		// Otherwise always set to leafmost property of the non-truncated property path, so we correctly pick up struct members
		const bool bTruncatedPropertyPath = PropertyPath.GetNumProperties() != KeyPropertyParams.PropertyPath.GetNumProperties();
		FName StructPropertyNameToKey = !bTruncatedPropertyPath ? NAME_None : KeyPropertyParams.PropertyPath.GetLeafMostProperty().Property->GetFName();
		FPropertyChangedParams Params(KeyableObjects, PropertyPath, StructPropertyNameToKey, KeyPropertyParams.KeyMode);
		Delegate.Broadcast(Params);
	}
}

bool FSequencerObjectChangeListener::IsObjectValidForListening( UObject* Object ) const
{
	// @todo Sequencer - Pre/PostEditChange is sometimes called for inner objects of other objects (like actors with components)
	// We only want the outer object so assume it's an actor for now
	if (Sequencer.IsValid())
	{
		TSharedRef<ISequencer> PinnedSequencer = Sequencer.Pin().ToSharedRef();
		return (PinnedSequencer->GetFocusedMovieSceneSequence() && PinnedSequencer->GetFocusedMovieSceneSequence()->CanAnimateObject(*Object));
	}

	return false;
}

FOnAnimatablePropertyChanged& FSequencerObjectChangeListener::GetOnAnimatablePropertyChanged( FAnimatedPropertyKey PropertyKey )
{
	return PropertyChangedEventMap.FindOrAdd( PropertyKey );
}

FOnPropagateObjectChanges& FSequencerObjectChangeListener::GetOnPropagateObjectChanges()
{
	return OnPropagateObjectChanges;
}

FOnObjectPropertyChanged& FSequencerObjectChangeListener::GetOnAnyPropertyChanged(UObject& Object)
{
	return ObjectToPropertyChangedEvent.FindOrAdd(&Object);
}

void FSequencerObjectChangeListener::ReportObjectDestroyed(UObject& Object)
{
	ObjectToPropertyChangedEvent.Remove(&Object);
}

FName GetFunctionName(FAnimatedPropertyKey PropertyKey, const FString& InPropertyVarName)
{
	FString PropertyVarName = InPropertyVarName;

	// If this is a bool property, strip off the 'b' so that the "Set" functions to be 
	// found are, for example, "SetHidden" instead of "SetbHidden"
	if (PropertyKey.PropertyTypeName == "BoolProperty")
	{
		PropertyVarName.RemoveFromStart("b", ESearchCase::CaseSensitive);
	}

	static const FString Set(TEXT("Set"));

	const FString FunctionString = Set + PropertyVarName;

	FName FunctionName = FName(*FunctionString);

	return FunctionName;
}

bool IsHiddenFunction(const UStruct& PropertyStructure, FAnimatedPropertyKey PropertyKey, const FString& InPropertyVarName)
{
	FName FunctionName = GetFunctionName(PropertyKey, InPropertyVarName);

	static const FName HideFunctionsName(TEXT("HideFunctions"));
	bool bIsHiddenFunction = false;
	TArray<FString> HideFunctions;
	if (const UClass* Class = Cast<const UClass>(&PropertyStructure))
	{
		Class->GetHideFunctions(HideFunctions);
	}

	return HideFunctions.Contains(FunctionName.ToString());
}

const FOnAnimatablePropertyChanged* FSequencerObjectChangeListener::FindPropertySetter(const UStruct& PropertyStructure, FAnimatedPropertyKey PropertyKey, const UProperty& Property) const
{
	const FOnAnimatablePropertyChanged* DelegatePtr = PropertyChangedEventMap.Find(PropertyKey);
	if (DelegatePtr != nullptr)
	{
		FString PropertyVarName = Property.GetName();

		// If this is a bool property, strip off the 'b' so that the "Set" functions to be 
		// found are, for example, "SetHidden" instead of "SetbHidden"
		if (PropertyKey.PropertyTypeName == "BoolProperty")
		{
			PropertyVarName.RemoveFromStart("b", ESearchCase::CaseSensitive);
		}

		static const FString Set(TEXT("Set"));

		const FString FunctionString = Set + PropertyVarName;

		FName FunctionName = FName(*FunctionString);

		static const FName DeprecatedFunctionName(TEXT("DeprecatedFunction"));
		UFunction* Function = nullptr;
		if (const UClass* Class = Cast<const UClass>(&PropertyStructure))
		{
			Function = Class->FindFunctionByName(FunctionName);
		}
		bool bFoundValidFunction = false;
		if (Function && !Function->HasMetaData(DeprecatedFunctionName))
		{
			bFoundValidFunction = true;
		}

		bool bFoundValidInterp = false;
		bool bFoundEditDefaultsOnly = false;
		bool bFoundEdit = false;

		if (Property.HasAnyPropertyFlags(CPF_Interp))
		{
			bFoundValidInterp = true;
		}

		// @TODO: should we early out of our property path iteration if we find an "edit defaults only" property?
		if (Property.HasAnyPropertyFlags(CPF_DisableEditOnInstance))
		{
			bFoundEditDefaultsOnly = true;
		}
		if (Property.HasAnyPropertyFlags(CPF_Edit))
		{
			bFoundEdit = true;
		}

		const bool bIsHiddenFunction = IsHiddenFunction(PropertyStructure, FAnimatedPropertyKey::FromProperty(&Property), Property.GetName());

		// Valid if there's a setter function and the property is editable. Also valid if there's an interp keyword.
		if (((bFoundValidFunction && bFoundEdit && !bFoundEditDefaultsOnly) || bFoundValidInterp) && !bIsHiddenFunction)
		{
			return DelegatePtr;
		}
	}

	return nullptr;
}

bool FSequencerObjectChangeListener::CanKeyProperty(FCanKeyPropertyParams CanKeyPropertyParams) const
{
	FOnAnimatablePropertyChanged Delegate;
	UProperty* Property = nullptr;
	FPropertyPath PropertyPath;
	return CanKeyProperty_Internal(CanKeyPropertyParams, Delegate, Property, PropertyPath);
}

bool FSequencerObjectChangeListener::CanKeyProperty_Internal(FCanKeyPropertyParams CanKeyPropertyParams, FOnAnimatablePropertyChanged& InOutDelegate, UProperty*& InOutProperty, FPropertyPath& InOutPropertyPath) const
{
	if (CanKeyPropertyParams.PropertyPath.GetNumProperties() == 0)
	{
		return false;
	}

	// iterate over the property path trying to find keyable properties
	InOutPropertyPath = FPropertyPath();
	for (int32 Index = 0; Index < CanKeyPropertyParams.PropertyPath.GetNumProperties(); ++Index)
	{
		const FPropertyInfo& PropertyInfo = CanKeyPropertyParams.PropertyPath.GetPropertyInfo(Index);

		// Add this to our 'potentially truncated' path
		InOutPropertyPath.AddProperty(PropertyInfo);

		UProperty* Property = CanKeyPropertyParams.PropertyPath.GetPropertyInfo(Index).Property.Get();
		if (Property)
		{
			const UStruct* PropertyContainer = CanKeyPropertyParams.FindPropertyContainer(Property);
			if (PropertyContainer)
			{
				{
					FAnimatedPropertyKey PropertyKey = FAnimatedPropertyKey::FromProperty(Property);
					const FOnAnimatablePropertyChanged* DelegatePtr = FindPropertySetter(*PropertyContainer, PropertyKey, *Property);
					if (DelegatePtr != nullptr)
					{
						InOutProperty = Property;
						InOutDelegate = *DelegatePtr;
						return true;
					}
				}

				if (UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property))
				{
					UClass* ClassType = ObjectProperty->PropertyClass ? ObjectProperty->PropertyClass->GetSuperClass() : nullptr;
					while (ClassType)
					{
						FAnimatedPropertyKey PropertyKey = FAnimatedPropertyKey::FromObjectType(ClassType);
						const FOnAnimatablePropertyChanged* DelegatePtr = FindPropertySetter(*PropertyContainer, PropertyKey, *Property);
						if (DelegatePtr != nullptr)
						{
							InOutProperty = Property;
							InOutDelegate = *DelegatePtr;
							return true;
						}
						ClassType = ClassType->GetSuperClass();
					}
				}
			}
		}
	}

	return false;
}

void FSequencerObjectChangeListener::KeyProperty(FKeyPropertyParams KeyPropertyParams) const
{
	BroadcastPropertyChanged(KeyPropertyParams);
}

void FSequencerObjectChangeListener::OnObjectPreEditChange( UObject* Object, const FEditPropertyChain& PropertyChain )
{
	if (Sequencer.IsValid() && !Sequencer.Pin()->IsAllowedToChange())
	{
		return;
	}

	// We only care if we are not attempting to change properties of a CDO (which cannot be animated)
	if( Sequencer.IsValid() && !Object->HasAnyFlags(RF_ClassDefaultObject) && PropertyChain.GetActiveMemberNode() )
	{
		// Register with the property editor module that we'd like to know about animated float properties that change
		FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>( "PropertyEditor" );

		// Sometimes due to edit inline new the object is not actually the object that contains the property
		if ( IsObjectValidForListening(Object) && Object->GetClass()->HasProperty(PropertyChain.GetActiveMemberNode()->GetValue()) )
		{
			TSharedPtr<IPropertyChangeListener> PropertyChangeListener = ActivePropertyChangeListeners.FindRef( Object );

			if( !PropertyChangeListener.IsValid() )
			{
				PropertyChangeListener = PropertyEditor.CreatePropertyChangeListener();
				
				ActivePropertyChangeListeners.Add( Object, PropertyChangeListener );

				PropertyChangeListener->GetOnPropertyChangedDelegate().AddRaw( this, &FSequencerObjectChangeListener::OnPropertyChanged );

				FPropertyListenerSettings Settings;
				// Ignore array and object properties
				Settings.bIgnoreArrayProperties = true;
				Settings.bIgnoreObjectProperties = false;
				// Property flags which must be on the property
				Settings.RequiredPropertyFlags = 0;
				// Property flags which cannot be on the property
				Settings.DisallowedPropertyFlags = CPF_EditConst;

				PropertyChangeListener->SetObject( *Object, Settings );
			}
		}
	}

	// Call add key/track before the property changes so that pre-animated state can be saved off.
	for( FEditPropertyChain::TIterator It(PropertyChain.GetHead()); It; ++It )
	{
		UProperty* Prop = *It;

		TArray<UObject*> Objects;
		Objects.Add(Object);

		FPropertyPath PropertyPath;
		PropertyPath.AddProperty(FPropertyInfo(Prop));
		BroadcastPropertyChanged(FKeyPropertyParams(Objects, PropertyPath, ESequencerKeyMode::AutoKey));
	}
}

void FSequencerObjectChangeListener::OnObjectPostEditChange( UObject* Object, FPropertyChangedEvent& PropertyChangedEvent )
{
	if( Object && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive )
	{
		bool bIsObjectValid = IsObjectValidForListening( Object );

		bool bShouldPropagateChanges = bIsObjectValid;

		// We only care if we are not attempting to change properties of a CDO (which cannot be animated)
		if( Sequencer.IsValid() && bIsObjectValid && !Object->HasAnyFlags(RF_ClassDefaultObject) )
		{
			TSharedPtr< IPropertyChangeListener > Listener;
			ActivePropertyChangeListeners.RemoveAndCopyValue( Object, Listener );

			if( Listener.IsValid() )
			{
				check( Listener.IsUnique() );
					
				// Don't recache new values, the listener will be destroyed after this call
				const bool bRecacheNewValues = false;

				const bool bFoundChanges = Listener->ScanForChanges( bRecacheNewValues );

				// If the listener did not find any changes we care about, propagate changes to puppets
				// @todo Sequencer - We might need to check per changed property
				bShouldPropagateChanges = !bFoundChanges;
			}
		}

		if( bShouldPropagateChanges )
		{
			OnPropagateObjectChanges.Broadcast( Object );
		}
	}
}


void FSequencerObjectChangeListener::OnActorPostEditMove( AActor* Actor )
{
	// @todo sequencer actors: Currently this only fires on a "final" move.  For our purposes we probably
	// want to get an update every single movement, even while dragging an object.
	FPropertyChangedEvent PropertyChangedEvent(nullptr);
	OnObjectPostEditChange( Actor, PropertyChangedEvent );
}

void FSequencerObjectChangeListener::TriggerAllPropertiesChanged(UObject* Object)
{
	if( Object )
	{
		// @todo Sequencer - Pre/PostEditChange is sometimes called for inner objects of other objects (like actors with components)
		// We only want the outer object so assume it's an actor for now
		bool bObjectIsActor = Object->IsA( AActor::StaticClass() );

		// Default to propagating changes to objects only if they are actors
		// if this change is handled by auto-key we will not propagate changes
		bool bShouldPropagateChanges = bObjectIsActor;

		// We only care if we are not attempting to change properties of a CDO (which cannot be animated)
		if( Sequencer.IsValid() && bObjectIsActor && !Object->HasAnyFlags(RF_ClassDefaultObject) )
		{
			TSharedPtr<IPropertyChangeListener> PropertyChangeListener = ActivePropertyChangeListeners.FindRef( Object );
			
			if( !PropertyChangeListener.IsValid() )
			{
				// Register with the property editor module that we'd like to know about animated float properties that change
				FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>( "PropertyEditor" );

				PropertyChangeListener = PropertyEditor.CreatePropertyChangeListener();
				
				PropertyChangeListener->GetOnPropertyChangedDelegate().AddRaw( this, &FSequencerObjectChangeListener::OnPropertyChanged );

				FPropertyListenerSettings Settings;
				// Ignore array and object properties
				Settings.bIgnoreArrayProperties = true;
				Settings.bIgnoreObjectProperties = true;
				// Property flags which must be on the property
				Settings.RequiredPropertyFlags = 0;
				// Property flags which cannot be on the property
				Settings.DisallowedPropertyFlags = CPF_EditConst;

				PropertyChangeListener->SetObject( *Object, Settings );
			}

			PropertyChangeListener->TriggerAllPropertiesChangedDelegate();
		}
	}
}
