// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_EditablePinBase.h"
#include "UObject/UnrealType.h"
#include "Misc/FeedbackContext.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/KismetDebugUtilities.h"

FArchive& operator<<(FArchive& Ar, FUserPinInfo& Info)
{
	Ar << Info.PinName;

	if (Ar.UE4Ver() >= VER_UE4_SERIALIZE_PINTYPE_CONST)
	{
		Info.PinType.Serialize(Ar);
		Ar << Info.DesiredPinDirection;
	}
	else
	{
		bool bIsArray = (Info.PinType.ContainerType == EPinContainerType::Array);
		Ar << bIsArray;

		bool bIsReference = Info.PinType.bIsReference;
		Ar << bIsReference;

		if (Ar.IsLoading())
		{
			Info.PinType.ContainerType = (bIsArray ? EPinContainerType::Array : EPinContainerType::None);
			Info.PinType.bIsReference = bIsReference;
		}

		Ar << Info.PinType.PinCategory;
		Ar << Info.PinType.PinSubCategory;

		Ar << Info.PinType.PinSubCategoryObject;
	}

	Ar << Info.PinDefaultValue;

	return Ar;
}


UK2Node_EditablePinBase::UK2Node_EditablePinBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_EditablePinBase::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	// Add in pins based on the user defined pins in this node
	for(int32 i = 0; i < UserDefinedPins.Num(); i++)
	{
		FText DummyErrorMsg;
		if (!IsEditable() || CanCreateUserDefinedPin(UserDefinedPins[i]->PinType, UserDefinedPins[i]->DesiredPinDirection, DummyErrorMsg))
		{
			CreatePinFromUserDefinition(UserDefinedPins[i]);
		}
	}
}

UEdGraphPin* UK2Node_EditablePinBase::CreateUserDefinedPin(const FString& InPinName, const FEdGraphPinType& InPinType, EEdGraphPinDirection InDesiredDirection, bool bUseUniqueName)
{
	// Sanitize the name, if needed
	const FString NewPinName = bUseUniqueName ? CreateUniquePinName(InPinName) : InPinName;

	// First, add this pin to the user-defined pins
	TSharedPtr<FUserPinInfo> NewPinInfo = MakeShareable( new FUserPinInfo() );
	NewPinInfo->PinName = NewPinName;
	NewPinInfo->PinType = InPinType;
	NewPinInfo->DesiredPinDirection = InDesiredDirection;
	UserDefinedPins.Add(NewPinInfo);

	// Then, add the pin to the actual Pins array
	UEdGraphPin* NewPin = CreatePinFromUserDefinition(NewPinInfo);

	return NewPin;
}

void UK2Node_EditablePinBase::RemoveUserDefinedPin(TSharedPtr<FUserPinInfo> PinToRemove)
{
	RemoveUserDefinedPinByName(PinToRemove->PinName);
}

void UK2Node_EditablePinBase::RemoveUserDefinedPinByName(const FString& PinName)
{
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin->PinName == PinName)
		{
			Pin->Modify();

			Pins.Remove(Pin);
			Pin->MarkPendingKill();

			if (UBlueprint* Blueprint = GetBlueprint())
			{
				FKismetDebugUtilities::RemovePinWatch(Blueprint, Pin);
			}

			break;
		}
	}

	// Remove the description from the user-defined pins array
	UserDefinedPins.RemoveAll([&](const TSharedPtr<FUserPinInfo>& UDPin)
	{
		return UDPin.IsValid() && (UDPin->PinName == PinName);
	});
}

void UK2Node_EditablePinBase::ExportCustomProperties(FOutputDevice& Out, uint32 Indent)
{
	Super::ExportCustomProperties(Out, Indent);

	for (int32 PinIndex = 0; PinIndex < UserDefinedPins.Num(); ++PinIndex)
	{
		const FUserPinInfo& PinInfo = *UserDefinedPins[PinIndex].Get();

		Out.Logf( TEXT("%sCustomProperties UserDefinedPin "), FCString::Spc(Indent));
		Out.Logf( TEXT("Name=\"%s\" "), *PinInfo.PinName);
		Out.Logf( TEXT("IsReference=%s "), (PinInfo.PinType.bIsReference ? TEXT("1") : TEXT("0")));

		if (UEnum* PinContainerType = FindObject<UEnum>(ANY_PACKAGE, TEXT("EPinContainerType")))
		{ 
			const FString ValueName = PinContainerType->GetNameStringByValue((uint8)PinInfo.PinType.ContainerType);
			if (!ValueName.IsEmpty())
			{
				Out.Logf(TEXT("PinContainerType=\"%s\" "), *ValueName);
			}
		}

		if (UEnum* PinDirEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EEdGraphPinDirection")))
		{ 
			const FString ValueName = PinDirEnum->GetNameStringByValue(PinInfo.DesiredPinDirection);
			if (!ValueName.IsEmpty())
			{
				Out.Logf(TEXT("PinDir=\"%s\" "), *ValueName);
			}
		}
		
		if (PinInfo.PinType.PinCategory.Len() > 0)
		{
			Out.Logf( TEXT("Category=%s "), *PinInfo.PinType.PinCategory);
		}

		if (PinInfo.PinType.PinSubCategory.Len() > 0)
		{
			Out.Logf( TEXT("SubCategory=%s "), *PinInfo.PinType.PinSubCategory);
		}

		if (PinInfo.PinType.PinSubCategoryObject.IsValid())
		{
			Out.Logf( TEXT("SubCategoryObject=%s "), *PinInfo.PinType.PinSubCategoryObject.Get()->GetPathName());
		}

		if (PinInfo.PinDefaultValue.Len() > 0)
		{
			Out.Logf( TEXT("DefaultValue=%s "), *PinInfo.PinDefaultValue);
		}

		Out.Logf( TEXT("\r\n"));
	}
}

void UK2Node_EditablePinBase::ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn)
{
	if (FParse::Command(&SourceText, TEXT("UserDefinedPin")))
	{
		TSharedPtr<FUserPinInfo> PinInfo = MakeShareable( new FUserPinInfo() );

		if (!FParse::Value(SourceText, TEXT("Name="), PinInfo->PinName))
		{
			Warn->Logf( *NSLOCTEXT( "Core", "SyntaxError", "Syntax Error" ).ToString() );
			return;
		}

		int32 BoolAsInt = 0;
		if (FParse::Value(SourceText, TEXT("IsArray="), BoolAsInt))
		{
			PinInfo->PinType.ContainerType = (BoolAsInt != 0 ? EPinContainerType::Array : EPinContainerType::None);
		}

		if (UEnum* PinContainerType = FindObject<UEnum>(ANY_PACKAGE, TEXT("EPinContainerType")))
		{
			FString DesiredContainerType;
			if (FParse::Value(SourceText, TEXT("PinContainerType="), DesiredContainerType))
			{
				const int32 DesiredContainerTypeVal = PinContainerType->GetValueByName(*DesiredContainerType);
				if (DesiredContainerTypeVal != INDEX_NONE)
				{
					PinInfo->PinType.ContainerType = (EPinContainerType)DesiredContainerTypeVal;
				}
			}
		}

		if (FParse::Value(SourceText, TEXT("IsReference="), BoolAsInt))
		{
			PinInfo->PinType.bIsReference = (BoolAsInt != 0);
		}

		if (UEnum* PinDirEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EEdGraphPinDirection")))
		{
			FString DesiredDirection;
			if (FParse::Value(SourceText, TEXT("PinDir="), DesiredDirection))
			{
				const int32 DesiredDirectionVal = PinDirEnum->GetValueByName(*DesiredDirection);
				if (DesiredDirectionVal != INDEX_NONE)
				{
					PinInfo->DesiredPinDirection = (EEdGraphPinDirection)DesiredDirectionVal;
				}
			}
		}

		FParse::Value(SourceText, TEXT("Category="), PinInfo->PinType.PinCategory);
		FParse::Value(SourceText, TEXT("SubCategory="), PinInfo->PinType.PinSubCategory);

		FString ObjectPathName;
		if (FParse::Value(SourceText, TEXT("SubCategoryObject="), ObjectPathName))
		{
			PinInfo->PinType.PinSubCategoryObject = FindObject<UObject>(ANY_PACKAGE, *ObjectPathName);
			if (!PinInfo->PinType.PinSubCategoryObject.IsValid())
			{
				Warn->Logf( *NSLOCTEXT( "Core", "UnableToFindObject", "Unable to find object" ).ToString() );
				return;
			}
		}

		FParse::Value(SourceText, TEXT("DefaultValue="), PinInfo->PinDefaultValue);

		UserDefinedPins.Add(PinInfo);
	}
	else
	{
		Super::ImportCustomProperties(SourceText, Warn);
	}
}

void UK2Node_EditablePinBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	TArray<FUserPinInfo> SerializedItems;
	if (Ar.IsLoading())
	{
		Ar << SerializedItems;

		UserDefinedPins.Empty(SerializedItems.Num());
		for (int32 Index = 0; Index < SerializedItems.Num(); ++Index)
		{
			TSharedPtr<FUserPinInfo> PinInfo = MakeShareable(new FUserPinInfo(SerializedItems[Index]));

			// Ensure that the UserDefinedPin's "desired direction" matches the direction of 
			// the EdGraphPin that it corresponds to. Somehow it is possible for these to get 
			// out of sync, and we're not entirely sure how/why.
			//
			// @TODO: Determine how these get out of sync and fix that up so we can guard this 
			//        with a version check, and not have to do this for updated assets
			if (UEdGraphPin* NodePin = FindPin(PinInfo->PinName))
			{
				// NOTE: the second FindPin call here to keep us from altering a pin with the same 
				//       name but different direction (in case there is two)
				if (PinInfo->DesiredPinDirection != NodePin->Direction && FindPin(PinInfo->PinName, PinInfo->DesiredPinDirection) == nullptr)
				{
					PinInfo->DesiredPinDirection = NodePin->Direction;
				}
			}

			UserDefinedPins.Add(PinInfo);
		}
	}
	else if(Ar.IsSaving())
	{
		SerializedItems.Empty(UserDefinedPins.Num());

		for (int32 Index = 0; Index < UserDefinedPins.Num(); ++Index)
		{
			SerializedItems.Add(*(UserDefinedPins[Index].Get()));
		}

		Ar << SerializedItems;
	}
	else
	{
		// We want to avoid destroying and recreating FUserPinInfo, because that will invalidate 
		// any WeakPtrs to those entries:
		for(TSharedPtr<FUserPinInfo>& PinInfo : UserDefinedPins )
		{
			Ar << *PinInfo;
		}
	}
}

void UK2Node_EditablePinBase::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UK2Node_EditablePinBase* This = CastChecked<UK2Node_EditablePinBase>(InThis);
	for (int32 Index = 0; Index < This->UserDefinedPins.Num(); ++Index)
	{
		FUserPinInfo PinInfo = *This->UserDefinedPins[Index].Get();
		UObject* PinSubCategoryObject = PinInfo.PinType.PinSubCategoryObject.Get();
		Collector.AddReferencedObject(PinSubCategoryObject, This);
	}
	Super::AddReferencedObjects( This, Collector );
}

void UK2Node_EditablePinBase::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	static bool bRecursivelyChangingDefaultValue = false;

	// Only do this if we're editable and not already calling this code
	if (!bIsEditable || bRecursivelyChangingDefaultValue)
	{
		return;
	}

	// See if this is a user defined pin
	for (int32 Index = 0; Index < UserDefinedPins.Num(); ++Index)
	{
		TSharedPtr<FUserPinInfo> PinInfo = UserDefinedPins[Index];
		if (Pin->PinName == PinInfo->PinName && Pin->Direction == PinInfo->DesiredPinDirection)
		{
			FString DefaultsString = Pin->GetDefaultAsString();

			if (DefaultsString != PinInfo->PinDefaultValue)
			{
				// Make sure this doesn't get called recursively
				TGuardValue<bool> CircularGuard(bRecursivelyChangingDefaultValue, true);
				ModifyUserDefinedPinDefaultValue(PinInfo, Pin->GetDefaultAsString());
			}
		}
	}
}

bool UK2Node_EditablePinBase::ModifyUserDefinedPinDefaultValue(TSharedPtr<FUserPinInfo> PinInfo, const FString& InDefaultValue)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	FString NewDefaultValue = InDefaultValue;

	// Find and modify the current pin
	if (UEdGraphPin* OldPin = FindPin(PinInfo->PinName))
	{
		FString SavedDefaultValue = OldPin->DefaultValue;
		
		K2Schema->SetPinAutogeneratedDefaultValue(OldPin, NewDefaultValue);

		// Validate the new default value
		FString ErrorString = K2Schema->IsCurrentPinDefaultValid(OldPin);

		if (!ErrorString.IsEmpty())
		{
			NewDefaultValue = SavedDefaultValue;
			K2Schema->SetPinAutogeneratedDefaultValue(OldPin, SavedDefaultValue);

			return false;
		}
	}

	PinInfo->PinDefaultValue = NewDefaultValue;
	return true;
}

bool UK2Node_EditablePinBase::CreateUserDefinedPinsForFunctionEntryExit(const UFunction* Function, bool bForFunctionEntry)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Create the inputs and outputs
	bool bAllPinsGood = true;
	for ( TFieldIterator<UProperty> PropIt(Function); PropIt && ( PropIt->PropertyFlags & CPF_Parm ); ++PropIt )
	{
		UProperty* Param = *PropIt;

		const bool bIsFunctionInput = !Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm);

		if ( bIsFunctionInput == bForFunctionEntry )
		{
			const EEdGraphPinDirection Direction = bForFunctionEntry ? EGPD_Output : EGPD_Input;

			FEdGraphPinType PinType;
			K2Schema->ConvertPropertyToPinType(Param, /*out*/ PinType);

			const bool bPinGood = CreateUserDefinedPin(Param->GetName(), PinType, Direction) != NULL;

			bAllPinsGood = bAllPinsGood && bPinGood;
		}
	}

	return bAllPinsGood;
}
