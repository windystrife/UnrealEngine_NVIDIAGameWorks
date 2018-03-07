// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlManipulator.h"
#include "SceneManagement.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ControlRigModule.h"
#include "Modules/ModuleManager.h"
#include "Materials/Material.h"

class FPropertyPath;

/** Begin code borrowed from Sequencer's FTrackInstancePropertyBindings */

namespace PropertyHelpers
{

struct FPropertyAddress
{
	UProperty* Property;
	void* Address;

	FPropertyAddress()
		: Property(nullptr)
		, Address(nullptr)
	{}
};

struct FPropertyAndIndex
{
	FPropertyAndIndex() : Property(nullptr), ArrayIndex(INDEX_NONE) {}

	UProperty* Property;
	int32 ArrayIndex;
};

FPropertyAndIndex FindPropertyAndArrayIndex(UStruct* InStruct, const FString& PropertyName)
{
	FPropertyAndIndex PropertyAndIndex;

	// Calculate the array index if possible
	int32 ArrayIndex = -1;
	if (PropertyName.Len() > 0 && PropertyName.GetCharArray()[PropertyName.Len() - 1] == ']')
	{
		int32 OpenIndex = 0;
		if (PropertyName.FindLastChar('[', OpenIndex))
		{
			FString TruncatedPropertyName(OpenIndex, *PropertyName);
			PropertyAndIndex.Property = FindField<UProperty>(InStruct, *TruncatedPropertyName);
			if (PropertyAndIndex.Property)
			{
				const int32 NumberLength = PropertyName.Len() - OpenIndex - 2;
				if (NumberLength > 0 && NumberLength <= 10)
				{
					TCHAR NumberBuffer[11];
					FMemory::Memcpy(NumberBuffer, &PropertyName[OpenIndex + 1], sizeof(TCHAR) * NumberLength);
					LexicalConversion::FromString(PropertyAndIndex.ArrayIndex, NumberBuffer);
				}
			}

			return PropertyAndIndex;
		}
	}

	PropertyAndIndex.Property = FindField<UProperty>(InStruct, *PropertyName);
	return PropertyAndIndex;
}

FPropertyAddress FindPropertyRecursive(void* BasePointer, UStruct* InStruct, TArray<FString>& InPropertyNames, uint32 Index, TArray<UProperty*>& InOutPropertyChain, FPropertyPath* InOutPropertyPath = nullptr)
{
#if WITH_EDITOR	
	check(InOutPropertyPath);
#endif

	FPropertyAndIndex PropertyAndIndex = FindPropertyAndArrayIndex(InStruct, *InPropertyNames[Index]);

	FPropertyAddress NewAddress;

	if (PropertyAndIndex.ArrayIndex != INDEX_NONE)
	{
		UArrayProperty* ArrayProp = CastChecked<UArrayProperty>(PropertyAndIndex.Property);

		FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(BasePointer));
		if (ArrayHelper.IsValidIndex(PropertyAndIndex.ArrayIndex))
		{
			UStructProperty* InnerStructProp = Cast<UStructProperty>(ArrayProp->Inner);
			if (InnerStructProp && InPropertyNames.IsValidIndex(Index + 1))
			{
				return FindPropertyRecursive(ArrayHelper.GetRawPtr(PropertyAndIndex.ArrayIndex), InnerStructProp->Struct, InPropertyNames, Index + 1, InOutPropertyChain, InOutPropertyPath);
			}
			else
			{
				NewAddress.Property = ArrayProp->Inner;
				NewAddress.Address = ArrayHelper.GetRawPtr(PropertyAndIndex.ArrayIndex);

				InOutPropertyChain.Add(NewAddress.Property);
#if WITH_EDITOR	
				InOutPropertyPath->AddProperty(FPropertyInfo(PropertyAndIndex.Property, PropertyAndIndex.ArrayIndex));
#endif
			}
		}
	}
	else if (UStructProperty* StructProp = Cast<UStructProperty>(PropertyAndIndex.Property))
	{
		NewAddress.Property = StructProp;
		NewAddress.Address = BasePointer;

		InOutPropertyChain.Add(NewAddress.Property);
#if WITH_EDITOR	
		InOutPropertyPath->AddProperty(FPropertyInfo(PropertyAndIndex.Property, PropertyAndIndex.ArrayIndex));
#endif
		if (InPropertyNames.IsValidIndex(Index + 1))
		{
			void* StructContainer = StructProp->ContainerPtrToValuePtr<void>(BasePointer);
			return FindPropertyRecursive(StructContainer, StructProp->Struct, InPropertyNames, Index + 1, InOutPropertyChain, InOutPropertyPath);
		}
		else
		{
			check(StructProp->GetName() == InPropertyNames[Index]);
		}
	}
	else if (PropertyAndIndex.Property)
	{
		NewAddress.Property = PropertyAndIndex.Property;
		NewAddress.Address = BasePointer;

		InOutPropertyChain.Add(NewAddress.Property);
#if WITH_EDITOR	
		InOutPropertyPath->AddProperty(FPropertyInfo(PropertyAndIndex.Property, PropertyAndIndex.ArrayIndex));
#endif
	}

	return NewAddress;
}

FPropertyAddress FindProperty(const UObject& InObject, const FString& InPropertyPath, TArray<UProperty*>& InOutPropertyChain, FPropertyPath* InOutPropertyPath = nullptr)
{
	TArray<FString> PropertyNames;

	InPropertyPath.ParseIntoArray(PropertyNames, TEXT("."), true);

	if (PropertyNames.Num() > 0)
	{
		return FindPropertyRecursive((void*)&InObject, InObject.GetClass(), PropertyNames, 0, InOutPropertyChain, InOutPropertyPath);
	}
	else
	{
		return FPropertyAddress();
	}
}

}

/** End code borrowed from Sequencer's FTrackInstancePropertyBindings */

void UControlManipulator::Initialize(UObject* InContainer)
{
	CacheProperty(InContainer);
}

void UControlManipulator::CacheProperty(const UObject* InContainer) const
{
	if (InContainer)
	{
		CachedPropertyChain.Reset();

#if WITH_EDITOR
		// In editor we need to cache the property path for use with sequencer keying
		CachedPropertyPath = FPropertyPath();
		PropertyHelpers::FPropertyAddress PropertyAddress = PropertyHelpers::FindProperty(*InContainer, *PropertyToManipulate.ToString(), CachedPropertyChain, &CachedPropertyPath);
#else
		PropertyHelpers::FPropertyAddress PropertyAddress = PropertyHelpers::FindProperty(*InContainer, *PropertyToManipulate.ToString(), CachedPropertyChain);
#endif
		CachedProperty = PropertyAddress.Property;
		CachedPropertyAddress = PropertyAddress.Address;
	}
	else
	{
		CachedProperty = nullptr;
		CachedPropertyAddress = nullptr;
		CachedPropertyChain.Reset();
#if WITH_EDITOR
		CachedPropertyPath = FPropertyPath();
#endif
	}
}

void UControlManipulator::SetLocation(const FVector& InLocation, UObject* InContainer)
{
#if WITH_EDITOR
	CacheProperty(InContainer);
#endif
	if (InContainer && CachedPropertyAddress && CachedProperty && CachedProperty->IsA<UStructProperty>())
	{
		const FName PropertyName = Cast<UStructProperty>(CachedProperty)->Struct->GetFName();
		if (PropertyName == NAME_Vector)
		{
			FVector& PropertyValue = *CachedProperty->ContainerPtrToValuePtr<FVector>(CachedPropertyAddress, 0);
			if (!PropertyValue.Equals(InLocation))
			{
				NotifyPreEditChangeProperty(InContainer);
				PropertyValue = InLocation;
				NotifyPostEditChangeProperty(InContainer);
			}
		}
		else if (PropertyName == NAME_Transform)
		{
			FTransform& PropertyValue = *CachedProperty->ContainerPtrToValuePtr<FTransform>(CachedPropertyAddress, 0);
			if (!PropertyValue.GetLocation().Equals(InLocation))
			{
				NotifyPreEditChangeProperty(InContainer);
				PropertyValue.SetLocation(InLocation);
				NotifyPostEditChangeProperty(InContainer);
			}
		}
	}
}

FVector UControlManipulator::GetLocation(const UObject* InContainer) const
{
#if WITH_EDITOR
	CacheProperty(InContainer);
#endif
	if (InContainer && CachedPropertyAddress && CachedProperty && CachedProperty->IsA<UStructProperty>())
	{
		const FName PropertyName = Cast<UStructProperty>(CachedProperty)->Struct->GetFName();
		if (PropertyName == NAME_Vector)
		{
			return *CachedProperty->ContainerPtrToValuePtr<FVector>(CachedPropertyAddress, 0);
		}
		else if (PropertyName == NAME_Transform)
		{
			return CachedProperty->ContainerPtrToValuePtr<FTransform>(CachedPropertyAddress, 0)->GetLocation();
		}
	}

	return FVector::ZeroVector;
}

void UControlManipulator::SetRotation(const FRotator& InRotation, UObject* InContainer)
{
#if WITH_EDITOR
	CacheProperty(InContainer);
#endif
	if (InContainer && CachedPropertyAddress && CachedProperty && CachedProperty->IsA<UStructProperty>())
	{
		const FName PropertyName = Cast<UStructProperty>(CachedProperty)->Struct->GetFName();
		if (PropertyName == NAME_Rotator)
		{
			FRotator& PropertyValue = *CachedProperty->ContainerPtrToValuePtr<FRotator>(CachedPropertyAddress, 0);
			if (!PropertyValue.Equals(InRotation))
			{
				NotifyPreEditChangeProperty(InContainer);
				PropertyValue = InRotation;
				NotifyPostEditChangeProperty(InContainer);
			}
		}
		else if (PropertyName == NAME_Transform)
		{
			FTransform& PropertyValue = *CachedProperty->ContainerPtrToValuePtr<FTransform>(CachedPropertyAddress, 0);
			FQuat RotationAsQuat = InRotation.Quaternion();
			if (!PropertyValue.GetRotation().Equals(RotationAsQuat))
			{
				NotifyPreEditChangeProperty(InContainer);
				PropertyValue.SetRotation(RotationAsQuat);
				NotifyPostEditChangeProperty(InContainer);
			}
		}
		else if (PropertyName == NAME_Quat)
		{
			FQuat& PropertyValue = *CachedProperty->ContainerPtrToValuePtr<FQuat>(InContainer, 0);
			FQuat RotationAsQuat = InRotation.Quaternion();
			if (!PropertyValue.Equals(RotationAsQuat))
			{
				NotifyPreEditChangeProperty(InContainer);
				PropertyValue = RotationAsQuat;
				NotifyPostEditChangeProperty(InContainer);
			}
		}
	}
}

FRotator UControlManipulator::GetRotation(const UObject* InContainer) const
{
#if WITH_EDITOR
	CacheProperty(InContainer);
#endif
	if (InContainer && CachedPropertyAddress && CachedProperty && CachedProperty->IsA<UStructProperty>())
	{
		const FName PropertyName = Cast<UStructProperty>(CachedProperty)->Struct->GetFName();
		if (PropertyName == NAME_Rotator)
		{
			return *CachedProperty->ContainerPtrToValuePtr<FRotator>(CachedPropertyAddress, 0);
		}
		else if (PropertyName == NAME_Transform)
		{
			return CachedProperty->ContainerPtrToValuePtr<FTransform>(CachedPropertyAddress, 0)->GetRotation().Rotator();
		}
		else if (PropertyName == NAME_Quat)
		{
			return CachedProperty->ContainerPtrToValuePtr<FQuat>(InContainer, 0)->Rotator();
		}
	}

	return FRotator::ZeroRotator;
}

void UControlManipulator::SetQuat(const FQuat& InQuat, UObject* InContainer)
{
#if WITH_EDITOR
	CacheProperty(InContainer);
#endif
	if (InContainer && CachedPropertyAddress && CachedProperty && CachedProperty->IsA<UStructProperty>())
	{
		const FName PropertyName = Cast<UStructProperty>(CachedProperty)->Struct->GetFName();
		if (PropertyName == NAME_Rotator)
		{
			FRotator& PropertyValue = *CachedProperty->ContainerPtrToValuePtr<FRotator>(CachedPropertyAddress, 0);
			FRotator QuatAsRotator = InQuat.Rotator();
			if (!PropertyValue.Equals(QuatAsRotator))
			{
				NotifyPreEditChangeProperty(InContainer);
				PropertyValue = QuatAsRotator;
				NotifyPostEditChangeProperty(InContainer);
			}
		}
		else if (PropertyName == NAME_Transform)
		{
			FTransform& PropertyValue = *CachedProperty->ContainerPtrToValuePtr<FTransform>(CachedPropertyAddress, 0);
			if (!PropertyValue.GetRotation().Equals(InQuat))
			{
				NotifyPreEditChangeProperty(InContainer);
				PropertyValue.SetRotation(InQuat);
				NotifyPostEditChangeProperty(InContainer);
			}
		}
		else if (PropertyName == NAME_Quat)
		{
			FQuat& PropertyValue = *CachedProperty->ContainerPtrToValuePtr<FQuat>(CachedPropertyAddress, 0);
			if (!PropertyValue.Equals(InQuat))
			{
				NotifyPreEditChangeProperty(InContainer);
				PropertyValue = InQuat;
				NotifyPostEditChangeProperty(InContainer);
			}
		}
	}
}

FQuat UControlManipulator::GetQuat(const UObject* InContainer) const
{
#if WITH_EDITOR
	CacheProperty(InContainer);
#endif
	if (InContainer && CachedPropertyAddress && CachedProperty && CachedProperty->IsA<UStructProperty>())
	{
		const FName PropertyName = Cast<UStructProperty>(CachedProperty)->Struct->GetFName();
		if (PropertyName == NAME_Rotator)
		{
			return CachedProperty->ContainerPtrToValuePtr<FRotator>(CachedPropertyAddress, 0)->Quaternion();
		}
		else if (PropertyName == NAME_Transform)
		{
			return CachedProperty->ContainerPtrToValuePtr<FTransform>(CachedPropertyAddress, 0)->GetRotation();
		}
		else if (PropertyName == NAME_Quat)
		{
			return *CachedProperty->ContainerPtrToValuePtr<FQuat>(CachedPropertyAddress, 0);
		}
	}

	return FQuat::Identity;
}

void UControlManipulator::SetScale(const FVector& InScale, UObject* InContainer)
{
#if WITH_EDITOR
	CacheProperty(InContainer);
#endif
	if (InContainer && CachedPropertyAddress && CachedProperty && CachedProperty->IsA<UStructProperty>())
	{
		const FName PropertyName = Cast<UStructProperty>(CachedProperty)->Struct->GetFName();
		if (PropertyName == NAME_Vector)
		{
			FVector& PropertyValue = *CachedProperty->ContainerPtrToValuePtr<FVector>(CachedPropertyAddress, 0);
			if (!PropertyValue.Equals(InScale))
			{
				NotifyPreEditChangeProperty(InContainer);
				PropertyValue = InScale;
				NotifyPostEditChangeProperty(InContainer);
			}
		}
		else if (PropertyName == NAME_Transform)
		{
			FTransform& PropertyValue = *CachedProperty->ContainerPtrToValuePtr<FTransform>(CachedPropertyAddress, 0);
			if (!PropertyValue.GetScale3D().Equals(InScale))
			{
				NotifyPreEditChangeProperty(InContainer);
				PropertyValue.SetScale3D(InScale);
				NotifyPostEditChangeProperty(InContainer);
			}
		}
	}
}

FVector UControlManipulator::GetScale(const UObject* InContainer) const
{
#if WITH_EDITOR
	CacheProperty(InContainer);
#endif
	if (InContainer && CachedPropertyAddress && CachedProperty && CachedProperty->IsA<UStructProperty>())
	{
		const FName PropertyName = Cast<UStructProperty>(CachedProperty)->Struct->GetFName();
		if (PropertyName == NAME_Vector)
		{
			return *CachedProperty->ContainerPtrToValuePtr<FVector>(CachedPropertyAddress, 0);
		}
		else if (PropertyName == NAME_Transform)
		{
			return CachedProperty->ContainerPtrToValuePtr<FTransform>(CachedPropertyAddress, 0)->GetScale3D();
		}
	}

	return FVector(1.0f);
}

void UControlManipulator::SetTransform(const FTransform& InTransform, UObject* InContainer)
{
#if WITH_EDITOR
	CacheProperty(InContainer);
#endif
	if (InContainer && CachedPropertyAddress && CachedProperty && CachedProperty->IsA<UStructProperty>() && Cast<UStructProperty>(CachedProperty)->Struct->GetFName() == NAME_Transform)
	{
		FTransform& PropertyValue = *CachedProperty->ContainerPtrToValuePtr<FTransform>(CachedPropertyAddress, 0);
		if (!PropertyValue.Equals(InTransform))
		{
			NotifyPreEditChangeProperty(InContainer);
			PropertyValue = InTransform;
			NotifyPostEditChangeProperty(InContainer);
		}
	}
	else
	{
		if (bUsesTranslation)
		{
			SetLocation(InTransform.GetLocation(), InContainer);
		}
		if (bUsesRotation)
		{
			SetRotation(InTransform.GetRotation().Rotator(), InContainer);
		}
		if (bUsesScale)
		{
			SetScale(InTransform.GetScale3D(), InContainer);
		}
	}
}

FTransform UControlManipulator::GetTransform(const UObject* InContainer) const
{
#if WITH_EDITOR
	CacheProperty(InContainer);
#endif
	if (InContainer && CachedPropertyAddress && CachedProperty && CachedProperty->IsA<UStructProperty>() && Cast<UStructProperty>(CachedProperty)->Struct->GetFName() == NAME_Transform)
	{
		return *CachedProperty->ContainerPtrToValuePtr<FTransform>(CachedPropertyAddress, 0);
	}
	else
	{
		return FTransform(
			bUsesRotation ? GetRotation(InContainer) : FRotator::ZeroRotator,
			bUsesTranslation ? GetLocation(InContainer) : FVector::ZeroVector,
			bUsesScale ? GetScale(InContainer) : FVector(1.0f));
	}
}

#if WITH_EDITOR

bool UControlManipulator::SupportsTransformComponent(ETransformComponent InComponent) const
{
	return (bUsesTranslation && InComponent == ETransformComponent::Translation) || (bUsesRotation && InComponent == ETransformComponent::Rotation) || (bUsesScale && InComponent == ETransformComponent::Scale);
}

#endif

void UControlManipulator::NotifyPreEditChangeProperty(UObject* InContainer)
{
#if WITH_EDITOR
	if (bNotifyListeners && CachedProperty && InContainer)
	{
		FEditPropertyChain EditPropertyChain;

		check(CachedPropertyChain.Num() > 0);
		for (UProperty* Property : CachedPropertyChain)
		{
			EditPropertyChain.AddTail(Property);
		}

		EditPropertyChain.SetActivePropertyNode(CachedProperty);
		if (CachedPropertyChain.Num() > 1)
		{
			EditPropertyChain.SetActiveMemberPropertyNode(CachedPropertyChain[0]);
		}

		InContainer->PreEditChange(EditPropertyChain);
	}
#endif
}

void UControlManipulator::NotifyPostEditChangeProperty(UObject* InContainer)
{
#if WITH_EDITOR
	if (bNotifyListeners && CachedProperty && InContainer)
	{
		FPropertyChangedEvent PropertyChangedEvent(CachedProperty);
		PropertyChangedEvent.ChangeType = bManipulating ? EPropertyChangeType::Interactive : EPropertyChangeType::ValueSet;
		InContainer->PostEditChangeProperty(PropertyChangedEvent);
	}
#endif
}

UColoredManipulator::UColoredManipulator()
	: Color(0.9f, 0.9f, 0.9f)
	, SelectedColor(0.728f, 0.364f, 0.003f)
{
}

USphereManipulator::USphereManipulator()
	: UColoredManipulator()
	, Radius(5.0f)
{
}

#if WITH_EDITOR

void UColoredManipulator::Draw(const FTransform& InTransform, const FSceneView* View, FPrimitiveDrawInterface* PDI, bool bIsSelected)
{
	// setup the material in case it was regenerated or we haven't been set up yet
	if (!ColorMaterial.IsValid())
	{
		FControlRigModule& ControlRigModule = FModuleManager::GetModuleChecked<FControlRigModule>("ControlRig");
		ColorMaterial = UMaterialInstanceDynamic::Create(ControlRigModule.ManipulatorMaterial, NULL);
	}
}

void USphereManipulator::Draw(const FTransform& InTransform, const FSceneView* View, FPrimitiveDrawInterface* PDI, bool bIsSelected)
{
	UColoredManipulator::Draw(InTransform, View, PDI, bIsSelected);

	if(ColorMaterial.IsValid())
	{ 
		ColorMaterial->SetVectorParameterValue("Color", FVector(bIsSelected ? SelectedColor : Color));
		DrawSphere(PDI, InTransform.GetLocation(), FRotator::ZeroRotator, FVector(Radius) * CurrentProximity, 64, 64, ColorMaterial->GetRenderProxy(false), SDPG_World);
	}
}

#endif

UBoxManipulator::UBoxManipulator()
	: UColoredManipulator()
	, BoxExtent(4.5f)
{
}

#if WITH_EDITOR

void UBoxManipulator::Draw(const FTransform& InTransform, const FSceneView* View, FPrimitiveDrawInterface* PDI, bool bIsSelected)
{
	UColoredManipulator::Draw(InTransform, View, PDI, bIsSelected);

	if (ColorMaterial.IsValid())
	{
		ColorMaterial->SetVectorParameterValue("Color", FVector(bIsSelected ? SelectedColor : Color));
		DrawBox(PDI, InTransform.ToMatrixWithScale(), BoxExtent * CurrentProximity, ColorMaterial->GetRenderProxy(false), SDPG_World);
	}
}
#endif