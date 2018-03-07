// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ComponentVisualizer.h"
#include "GameFramework/Actor.h"
#include "ActorEditorUtils.h"

IMPLEMENT_HIT_PROXY(HComponentVisProxy, HHitProxy);


FComponentVisualizer::FPropertyNameAndIndex FComponentVisualizer::GetComponentPropertyName(const UActorComponent* Component)
{
	if (Component)
	{
		const AActor* CompOwner = Component->GetOwner();
		if (CompOwner)
		{
			// Iterate over UObject* fields of this actor
			UClass* ActorClass = CompOwner->GetClass();
			for (TFieldIterator<UObjectProperty> It(ActorClass); It; ++It)
			{
				// See if this property points to the component in question
				UObjectProperty* ObjectProp = *It;
				for (int32 Index = 0; Index < ObjectProp->ArrayDim; ++Index)
				{
					UObject* Object = ObjectProp->GetObjectPropertyValue(ObjectProp->ContainerPtrToValuePtr<void>(CompOwner, Index));
					if (Object == Component)
					{
						// It does! Return this name
						return FPropertyNameAndIndex(ObjectProp->GetFName(), Index);
					}
				}
			}

			// If nothing found, look in TArray<UObject*> fields
			for (TFieldIterator<UArrayProperty> It(ActorClass); It; ++It)
			{
				UArrayProperty* ArrayProp = *It;
				if (UObjectProperty* InnerProp = Cast<UObjectProperty>(It->Inner))
				{
					FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(CompOwner));
					for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
					{
						UObject* Object = InnerProp->GetObjectPropertyValue(ArrayHelper.GetRawPtr(Index));
						if (Object == Component)
						{
							return FPropertyNameAndIndex(ArrayProp->GetFName(), Index);
						}
					}
				}
			}
		}
	}

	// Didn't find actor property referencing this component
	return FPropertyNameAndIndex();
}

UActorComponent* FComponentVisualizer::GetComponentFromPropertyName(const AActor* CompOwner, const FPropertyNameAndIndex& Property)
{
	UActorComponent* ResultComp = NULL;
	if(CompOwner && Property.IsValid())
	{
		UClass* ActorClass = CompOwner->GetClass();
		UProperty* Prop = FindField<UProperty>(ActorClass, Property.Name);
		if (UObjectProperty* ObjectProp = Cast<UObjectProperty>(Prop))
		{
			UObject* Object = ObjectProp->GetObjectPropertyValue(ObjectProp->ContainerPtrToValuePtr<void>(CompOwner, Property.Index));
			ResultComp = Cast<UActorComponent>(Object);
		}
		else if (UArrayProperty* ArrayProp = Cast<UArrayProperty>(Prop))
		{
			if (UObjectProperty* InnerProp = Cast<UObjectProperty>(ArrayProp->Inner))
			{
				FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(CompOwner));
				UObject* Object = InnerProp->GetObjectPropertyValue(ArrayHelper.GetRawPtr(Property.Index));
				ResultComp = Cast<UActorComponent>(Object);
			}
		}
	}

	return ResultComp;
}

void FComponentVisualizer::NotifyPropertyModified(UActorComponent* Component, UProperty* Property)
{
	TArray<UProperty*> Properties;
	Properties.Add(Property);
	NotifyPropertiesModified(Component, Properties);
}

void FComponentVisualizer::NotifyPropertiesModified(UActorComponent* Component, const TArray<UProperty*>& Properties)
{
	if (Component == nullptr)
	{
		return;
	}

	for (UProperty* Property : Properties)
	{
		FPropertyChangedEvent PropertyChangedEvent(Property);
		Component->PostEditChangeProperty(PropertyChangedEvent);
	}

	AActor* Owner = Component->GetOwner();

	if (Owner && FActorEditorUtils::IsAPreviewOrInactiveActor(Owner))
	{
		// The component belongs to the preview actor in the BP editor, so we need to propagate the property change to the archetype.
		// Before this, we exploit the fact that the archetype and the preview actor have the old and new value of the property, respectively.
		// So we can go through all archetype instances, and if they hold the (old) archetype value, update it to the new value.

		// Get archetype
		UActorComponent* Archetype = Cast<UActorComponent>(Component->GetArchetype());
		check(Archetype);

		// Get all archetype instances (the preview actor passed in should be amongst them)
		TArray<UObject*> ArchetypeInstances;
		Archetype->GetArchetypeInstances(ArchetypeInstances);
		check(ArchetypeInstances.Contains(Component));

		// Identify which of the modified properties are at their default values in the archetype instances,
		// and thus need the new value to be propagated to them
		struct FInstanceDefaultProperties
		{
			UActorComponent* ArchetypeInstance;
			TArray<UProperty*, TInlineAllocator<8>> Properties;
		};

		TArray<FInstanceDefaultProperties> InstanceDefaultProperties;
		InstanceDefaultProperties.Reserve(ArchetypeInstances.Num());

		for (UObject* ArchetypeInstance : ArchetypeInstances)
		{
			UActorComponent* InstanceComp = Cast<UActorComponent>(ArchetypeInstance);
			if (InstanceComp != Component)
			{
				FInstanceDefaultProperties Entry;
				for (UProperty* Property : Properties)
				{
					uint8* ArchetypePtr = Property->ContainerPtrToValuePtr<uint8>(Archetype);
					uint8* InstancePtr = Property->ContainerPtrToValuePtr<uint8>(InstanceComp);
					if (Property->Identical(ArchetypePtr, InstancePtr))
					{
						Entry.Properties.Add(Property);
					}
				}

				if (Entry.Properties.Num() > 0)
				{
					Entry.ArchetypeInstance = InstanceComp;
					InstanceDefaultProperties.Add(MoveTemp(Entry));
				}
			}
		}

		// Propagate all modified properties to the archetype
		Archetype->SetFlags(RF_Transactional);
		Archetype->Modify();

		if (Archetype->GetOwner())
		{
			Archetype->GetOwner()->Modify();
		}

		for (UProperty* Property : Properties)
		{
			uint8* ArchetypePtr = Property->ContainerPtrToValuePtr<uint8>(Archetype);
			uint8* PreviewPtr = Property->ContainerPtrToValuePtr<uint8>(Component);
			Property->CopyCompleteValue(ArchetypePtr, PreviewPtr);

			FPropertyChangedEvent PropertyChangedEvent(Property);
			Archetype->PostEditChangeProperty(PropertyChangedEvent);
		}

		// Apply changes to each archetype instance
		for (const auto& Instance : InstanceDefaultProperties)
		{
			Instance.ArchetypeInstance->SetFlags(RF_Transactional);
			Instance.ArchetypeInstance->Modify();

			AActor* InstanceOwner = Instance.ArchetypeInstance->GetOwner();

			if (InstanceOwner)
			{
				InstanceOwner->Modify();
			}

			for (UProperty* Property : Instance.Properties)
			{
				uint8* InstancePtr = Property->ContainerPtrToValuePtr<uint8>(Instance.ArchetypeInstance);
				uint8* PreviewPtr = Property->ContainerPtrToValuePtr<uint8>(Component);
				Property->CopyCompleteValue(InstancePtr, PreviewPtr);

				FPropertyChangedEvent PropertyChangedEvent(Property);
				Instance.ArchetypeInstance->PostEditChangeProperty(PropertyChangedEvent);
			}

			// Rerun construction script on instance
			if (InstanceOwner)
			{
				InstanceOwner->PostEditMove(false);
			}
		}
	}

	// Rerun construction script on preview actor
	Owner->PostEditMove(false);
}
