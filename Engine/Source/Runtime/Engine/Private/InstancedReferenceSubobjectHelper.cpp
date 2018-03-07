// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "InstancedReferenceSubobjectHelper.h"
	
UObject* FInstancedPropertyPath::Resolve(const UObject* Container) const
{
	UStruct* CurrentContainerType = Container->GetClass();

	const TArray<FPropertyLink>& PropChainRef = PropertyChain;
	auto GetProperty = [&CurrentContainerType, &PropChainRef](int32 ChainIndex)->UProperty*
	{
		const UProperty* SrcProperty = PropChainRef[ChainIndex].PropertyPtr;
		return FindField<UProperty>(CurrentContainerType, SrcProperty->GetFName());
	};

	const UProperty* CurrentProp = GetProperty(0);
	const uint8* ValuePtr = (CurrentProp) ? CurrentProp->ContainerPtrToValuePtr<uint8>(Container) : nullptr;

	for (int32 ChainIndex = 1; CurrentProp && ChainIndex < PropertyChain.Num(); ++ChainIndex)
	{
		if (const UArrayProperty* ArrayProperty = Cast<UArrayProperty>(CurrentProp))
		{
			check(PropertyChain[ChainIndex].PropertyPtr == ArrayProperty->Inner);

			int32 TargetIndex = PropertyChain[ChainIndex].ArrayIndex;
			check(TargetIndex != INDEX_NONE);

			FScriptArrayHelper ArrayHelper(ArrayProperty, ValuePtr);
			if (TargetIndex >= ArrayHelper.Num())
			{
				CurrentProp = nullptr;
				break;
			}

			CurrentProp = ArrayProperty->Inner;
			ValuePtr    = ArrayHelper.GetRawPtr(TargetIndex);
		}
		else if (const USetProperty* SetProperty = Cast<USetProperty>(CurrentProp))
		{
			check(PropertyChain[ChainIndex].PropertyPtr == SetProperty->ElementProp);

			int32 TargetIndex = PropertyChain[ChainIndex].ArrayIndex;
			check(TargetIndex != INDEX_NONE);

			FScriptSetHelper SetHelper(SetProperty, ValuePtr);
			if (TargetIndex >= SetHelper.Num())
			{
				CurrentProp = nullptr;
				break;
			}

			CurrentProp = SetProperty->ElementProp;
			ValuePtr    = SetHelper.GetElementPtr(TargetIndex);
		}
		else if (const UMapProperty* MapProperty = Cast<UMapProperty>(CurrentProp))
		{
			int32 TargetIndex = PropertyChain[ChainIndex].ArrayIndex;
			check(TargetIndex != INDEX_NONE);
				
			FScriptMapHelper MapHelper(MapProperty, ValuePtr);
			if(PropertyChain[ChainIndex].PropertyPtr == MapProperty->KeyProp)
			{
				ValuePtr = MapHelper.GetKeyPtr(TargetIndex);
			}
			else if(ensure(PropertyChain[ChainIndex].PropertyPtr == MapProperty->ValueProp))
			{
				ValuePtr = MapHelper.GetValuePtr(TargetIndex);
			}
			CurrentProp = PropertyChain[ChainIndex].PropertyPtr;
		}
		else if (ensure(PropertyChain[ChainIndex].ArrayIndex <= 0))
		{
			if (const UStructProperty* StructProperty = Cast<UStructProperty>(CurrentProp))
			{
				CurrentContainerType = StructProperty->Struct;
			}

			CurrentProp = GetProperty(ChainIndex);
			ValuePtr    = (CurrentProp) ? CurrentProp->ContainerPtrToValuePtr<uint8>(ValuePtr) : nullptr;
		}
	}

	const UObjectProperty* TargetPropety = Cast<UObjectProperty>(CurrentProp);
	if (TargetPropety && TargetPropety->HasAnyPropertyFlags(CPF_InstancedReference))
	{ 
		return TargetPropety->GetObjectPropertyValue(ValuePtr);
	}
	return nullptr;
}

void FFindInstancedReferenceSubobjectHelper::GetInstancedSubObjects_Inner(FInstancedPropertyPath& PropertyPath, const uint8* ContainerAddress, TFunctionRef<void(const FInstancedSubObjRef& Ref)> OutObjects)
{
	check(ContainerAddress);
	const UProperty* TargetProp = PropertyPath.Head();

	if (!TargetProp->HasAnyPropertyFlags(CPF_PersistentInstance | CPF_ContainsInstancedReference))
	{
		return;
	}

	if (const UArrayProperty* ArrayProperty = Cast<const UArrayProperty>(TargetProp))
	{
		if (const UStructProperty* InnerStructProperty = Cast<const UStructProperty>(ArrayProperty->Inner))
		{
			if (const UStruct* Struct = InnerStructProperty->Struct)
			{
				FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, ContainerAddress);
				for (int32 ElementIndex = 0; ElementIndex < ArrayHelper.Num(); ++ElementIndex)
				{
					const uint8* ValueAddress = ArrayHelper.GetRawPtr(ElementIndex);
					for (UProperty* StructProp = Struct->RefLink; StructProp; StructProp = StructProp->NextRef)
					{
						PropertyPath.Push(InnerStructProperty, ElementIndex);
						GetInstancedSubObjects_Inner(PropertyPath, ValueAddress, OutObjects);
						PropertyPath.Pop();
					}
				}
			}
		}
		else if (const UObjectProperty* InnerObjectProperty = Cast<const UObjectProperty>(ArrayProperty->Inner))
		{
			if (InnerObjectProperty->HasAllPropertyFlags(CPF_PersistentInstance))
			{
				ensure(InnerObjectProperty->HasAllPropertyFlags(CPF_InstancedReference));
				FScriptArrayHelper_InContainer ArrayHelper(ArrayProperty, ContainerAddress);
				for (int32 ElementIndex = 0; ElementIndex < ArrayHelper.Num(); ++ElementIndex)
				{
					if (UObject* ObjectValue = InnerObjectProperty->GetObjectPropertyValue(ArrayHelper.GetRawPtr(ElementIndex)))
					{
						PropertyPath.Push(InnerObjectProperty, ElementIndex);
						OutObjects(FInstancedSubObjRef(ObjectValue, PropertyPath));
						PropertyPath.Pop();
					}
				}
			}
		}
	}
	else if (const UMapProperty* MapProperty = Cast<const UMapProperty>(TargetProp))
	{
		if (const UStructProperty* KeyStructProperty = Cast<const UStructProperty>(MapProperty->KeyProp))
		{
			if (const UStruct* Struct = KeyStructProperty->Struct)
			{
				FScriptMapHelper MapHelper(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(ContainerAddress));
				for (int32 ElementIndex = 0; ElementIndex < MapHelper.Num(); ++ElementIndex)
				{
					if (MapHelper.IsValidIndex(ElementIndex))
					{
						const uint8* KeyAddress = MapHelper.GetKeyPtr(ElementIndex);
						for (UProperty* StructProp = Struct->RefLink; StructProp; StructProp = StructProp->NextRef)
						{
							PropertyPath.Push(KeyStructProperty, ElementIndex);
							GetInstancedSubObjects_Inner(PropertyPath, KeyAddress, OutObjects);
							PropertyPath.Pop();
						}
					}
				}
			}
		}
		else if (const UObjectProperty* KeyObjectProperty = Cast<const UObjectProperty>(MapProperty->KeyProp))
		{
			if (KeyObjectProperty->HasAllPropertyFlags(CPF_PersistentInstance))
			{
				ensure(KeyObjectProperty->HasAllPropertyFlags(CPF_InstancedReference));
				FScriptMapHelper MapHelper(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(ContainerAddress));
				for (int32 ElementIndex = 0; ElementIndex < MapHelper.Num(); ++ElementIndex)
				{
					if (MapHelper.IsValidIndex(ElementIndex))
					{
						const uint8* KeyAddress = MapHelper.GetKeyPtr(ElementIndex);
						if (UObject* ObjectValue = KeyObjectProperty->GetObjectPropertyValue(KeyAddress))
						{
							PropertyPath.Push(KeyObjectProperty, ElementIndex);
							OutObjects(FInstancedSubObjRef(ObjectValue, PropertyPath));
							PropertyPath.Pop();
						}
					}
				}
			}
		}

		if (const UStructProperty* ValueStructProperty = Cast<const UStructProperty>(MapProperty->ValueProp))
		{
			if (const UStruct* Struct = ValueStructProperty->Struct)
			{
				FScriptMapHelper MapHelper(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(ContainerAddress));
				for (int32 ElementIndex = 0; ElementIndex < MapHelper.Num(); ++ElementIndex)
				{
					if (MapHelper.IsValidIndex(ElementIndex))
					{
						// use of pair pointer is intentional, next call is going to offset from the pair entry:
						const uint8* ValueAddress = MapHelper.GetPairPtr(ElementIndex);
						for (UProperty* StructProp = Struct->RefLink; StructProp; StructProp = StructProp->NextRef)
						{
							PropertyPath.Push(ValueStructProperty, ElementIndex);
							GetInstancedSubObjects_Inner(PropertyPath, ValueAddress, OutObjects);
							PropertyPath.Pop();
						}
					}
				}
			}
		}
		else if (const UObjectProperty* ValueObjectProperty = Cast<const UObjectProperty>(MapProperty->ValueProp))
		{
			if (ValueObjectProperty->HasAllPropertyFlags(CPF_PersistentInstance))
			{
				ensure(ValueObjectProperty->HasAllPropertyFlags(CPF_InstancedReference));
				FScriptMapHelper MapHelper(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(ContainerAddress));
				for (int32 ElementIndex = 0; ElementIndex < MapHelper.Num(); ++ElementIndex)
				{
					if (MapHelper.IsValidIndex(ElementIndex))
					{
						const uint8* ValueAddress = MapHelper.GetValuePtr(ElementIndex);
						if (UObject* ObjectValue = ValueObjectProperty->GetObjectPropertyValue(ValueAddress))
						{
							PropertyPath.Push(ValueObjectProperty, ElementIndex);
							OutObjects(FInstancedSubObjRef(ObjectValue, PropertyPath));
							PropertyPath.Pop();
						}
					}
				}
			}
		}
	}
	else if (const USetProperty* SetProperty = Cast<const USetProperty>(TargetProp))
	{
		if (const UStructProperty* ElementStructProperty = Cast<const UStructProperty>(SetProperty->ElementProp))
		{
			if (const UStruct* Struct = ElementStructProperty->Struct)
			{
				FScriptSetHelper SetHelper(SetProperty, SetProperty->ContainerPtrToValuePtr<void>(ContainerAddress));
				for (int32 ElementIndex = 0; ElementIndex < SetHelper.Num(); ++ElementIndex)
				{
					if (SetHelper.IsValidIndex(ElementIndex))
					{
						const uint8* ElementAddress = SetHelper.GetElementPtr(ElementIndex);
						for (UProperty* StructProp = Struct->RefLink; StructProp; StructProp = StructProp->NextRef)
						{
							PropertyPath.Push(ElementStructProperty, ElementIndex);
							GetInstancedSubObjects_Inner(PropertyPath, ElementAddress, OutObjects);
							PropertyPath.Pop();
						}
					}
				}
			}
		}
		else if (const UObjectProperty* ElementObjectProperty = Cast<const UObjectProperty>(SetProperty->ElementProp))
		{
			if (ElementObjectProperty->HasAllPropertyFlags(CPF_PersistentInstance))
			{
				ensure(ElementObjectProperty->HasAllPropertyFlags(CPF_InstancedReference));
				FScriptSetHelper SetHelper(SetProperty, SetProperty->ContainerPtrToValuePtr<void>(ContainerAddress));
				for (int32 ElementIndex = 0; ElementIndex < SetHelper.Num(); ++ElementIndex)
				{
					if (SetHelper.IsValidIndex(ElementIndex))
					{
						const uint8* ElementAddress = SetHelper.GetElementPtr(ElementIndex);
						if (UObject* ObjectValue = ElementObjectProperty->GetObjectPropertyValue(ElementAddress))
						{
							PropertyPath.Push(ElementObjectProperty, ElementIndex);
							OutObjects(FInstancedSubObjRef(ObjectValue, PropertyPath));
							PropertyPath.Pop();
						}
					}
				}
			}
		}
	}
	else if (TargetProp->HasAllPropertyFlags(CPF_PersistentInstance))
	{
		ensure(TargetProp->HasAllPropertyFlags(CPF_InstancedReference));
		if (const UObjectProperty* ObjectProperty = Cast<const UObjectProperty>(TargetProp))
		{
			for (int32 ArrayIdx = 0; ArrayIdx < ObjectProperty->ArrayDim; ++ArrayIdx)
			{
				UObject* ObjectValue = ObjectProperty->GetObjectPropertyValue_InContainer(ContainerAddress, ArrayIdx);
				if (ObjectValue)
				{
					// don't need to push to PropertyPath, since this property is already at its head
					OutObjects(FInstancedSubObjRef(ObjectValue, PropertyPath));	
				}
			}
		}

		return;
	}
	else if (const UStructProperty* StructProperty = Cast<const UStructProperty>(TargetProp))
	{
		if (StructProperty->Struct)
		{
			for (int32 ArrayIdx = 0; ArrayIdx < StructProperty->ArrayDim; ++ArrayIdx)
			{
				const uint8* ValueAddress = StructProperty->ContainerPtrToValuePtr<uint8>(ContainerAddress, ArrayIdx);
				for (UProperty* StructProp = StructProperty->Struct->RefLink; StructProp; StructProp = StructProp->NextRef)
				{
					PropertyPath.Push(StructProp, ArrayIdx);
					GetInstancedSubObjects_Inner(PropertyPath, ValueAddress, OutObjects);
					PropertyPath.Pop();
				}
			}
		}
	}
}

void FFindInstancedReferenceSubobjectHelper::Duplicate(UObject* OldObject, UObject* NewObject, TMap<UObject*, UObject*>& ReferenceReplacementMap, TArray<UObject*>& DuplicatedObjects)
{
	if (OldObject->GetClass()->HasAnyClassFlags(CLASS_HasInstancedReference) &&
		NewObject->GetClass()->HasAnyClassFlags(CLASS_HasInstancedReference))
	{
		TArray<FInstancedSubObjRef> OldInstancedSubObjects;
		GetInstancedSubObjects(OldObject, OldInstancedSubObjects);
		if (OldInstancedSubObjects.Num() > 0)
		{
			TArray<FInstancedSubObjRef> NewInstancedSubObjects;
			GetInstancedSubObjects(NewObject, NewInstancedSubObjects);
			for (const FInstancedSubObjRef& Obj : NewInstancedSubObjects)
			{
				const bool bNewObjectHasOldOuter = (Obj->GetOuter() == OldObject);
				if (bNewObjectHasOldOuter)
				{
					const bool bKeptByOld = OldInstancedSubObjects.Contains(Obj);
					const bool bNotHandledYet = !ReferenceReplacementMap.Contains(Obj);
					if (bKeptByOld && bNotHandledYet)
					{
						UObject* NewEditInlineSubobject = StaticDuplicateObject(Obj, NewObject);
						ReferenceReplacementMap.Add(Obj, NewEditInlineSubobject);

						// NOTE: we cannot patch OldObject's linker table here, since we don't 
						//       know the relation between the  two objects (one could be of a 
						//       super class, and the other a child)

						// We also need to make sure to fixup any properties here
						DuplicatedObjects.Add(NewEditInlineSubobject);
					}
				}
			}
		}
	}
}
