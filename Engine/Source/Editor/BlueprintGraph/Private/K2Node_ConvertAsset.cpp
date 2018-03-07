// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "K2Node_ConvertAsset.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_ClassDynamicCast.h"
#include "KismetCompiler.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"

#define LOCTEXT_NAMESPACE "K2Node_ConvertAsset"

namespace UK2Node_ConvertAssetImpl
{
	static const FString InputPinName("Input");
	static const FString OutputPinName("Output");
}

UClass* UK2Node_ConvertAsset::GetTargetClass() const
{
	UEdGraphPin* InputPin = FindPin(UK2Node_ConvertAssetImpl::InputPinName);
	bool bIsConnected = InputPin && InputPin->LinkedTo.Num() && InputPin->LinkedTo[0];
	UEdGraphPin* SourcePin = bIsConnected ? InputPin->LinkedTo[0] : nullptr;
	return SourcePin ? Cast<UClass>(SourcePin->PinType.PinSubCategoryObject.Get()) : nullptr;
}

bool UK2Node_ConvertAsset::IsAssetClassType() const
{
	// get first input, return if class asset
	UEdGraphPin* InputPin = FindPin(UK2Node_ConvertAssetImpl::InputPinName);
	bool bIsConnected = InputPin && InputPin->LinkedTo.Num() && InputPin->LinkedTo[0];
	UEdGraphPin* SourcePin = bIsConnected ? InputPin->LinkedTo[0] : nullptr;
	return SourcePin ? (SourcePin->PinType.PinCategory == UEdGraphSchema_K2::PC_SoftClass || SourcePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Class) : false;
}

bool UK2Node_ConvertAsset::IsConvertToAsset() const
{
	// get first input, return if class asset
	UEdGraphPin* InputPin = FindPin(UK2Node_ConvertAssetImpl::InputPinName);
	bool bIsConnected = InputPin && InputPin->LinkedTo.Num() && InputPin->LinkedTo[0];
	UEdGraphPin* SourcePin = bIsConnected ? InputPin->LinkedTo[0] : nullptr;
	return SourcePin ? (SourcePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Class || SourcePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object) : false;
}

bool UK2Node_ConvertAsset::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	UEdGraphPin* InputPin = FindPin(UK2Node_ConvertAssetImpl::InputPinName);
	if (InputPin && OtherPin && (InputPin == MyPin) && (MyPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard))
	{
		if ((OtherPin->PinType.PinCategory != UEdGraphSchema_K2::PC_SoftObject) &&
			(OtherPin->PinType.PinCategory != UEdGraphSchema_K2::PC_SoftClass) &&
			(OtherPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Object) &&
			(OtherPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Class))
		{
			return true;
		}
	}
	return false;
}

void UK2Node_ConvertAsset::RefreshPinTypes()
{
	const UEdGraphSchema_K2* K2Schema = CastChecked<UEdGraphSchema_K2>(GetSchema());
	UEdGraphPin* InputPin = FindPin(UK2Node_ConvertAssetImpl::InputPinName);
	UEdGraphPin* OutputPin = FindPin(UK2Node_ConvertAssetImpl::OutputPinName);
	ensure(InputPin && OutputPin);
	if (InputPin && OutputPin)
	{
		const bool bIsConnected = InputPin->LinkedTo.Num() > 0;
		UClass* TargetType = bIsConnected ? GetTargetClass() : nullptr;
		const bool bIsAssetClass = bIsConnected ? IsAssetClassType() : false;
		const bool bIsConvertToAsset = bIsConnected ? IsConvertToAsset() : false;

		FString InputCategory = UEdGraphSchema_K2::PC_Wildcard;
		FString OutputCategory = UEdGraphSchema_K2::PC_Wildcard;
		if (bIsConnected)
		{
			if (bIsConvertToAsset)
			{
				InputCategory = (bIsAssetClass ? UEdGraphSchema_K2::PC_Class : UEdGraphSchema_K2::PC_Object);
				OutputCategory = (bIsAssetClass ? UEdGraphSchema_K2::PC_SoftClass : UEdGraphSchema_K2::PC_SoftObject);
			}
			else
			{
				InputCategory = (bIsAssetClass ? UEdGraphSchema_K2::PC_SoftClass : UEdGraphSchema_K2::PC_SoftObject);
				OutputCategory = (bIsAssetClass ? UEdGraphSchema_K2::PC_Class : UEdGraphSchema_K2::PC_Object);
			}
		}
			
		InputPin->PinType = FEdGraphPinType(InputCategory, FString(), TargetType, EPinContainerType::None, false, FEdGraphTerminalType() );
		OutputPin->PinType = FEdGraphPinType(OutputCategory, FString(), TargetType, EPinContainerType::None, false, FEdGraphTerminalType() );

		PinTypeChanged(InputPin);
		PinTypeChanged(OutputPin);

		if (OutputPin->LinkedTo.Num())
		{
			TArray<UEdGraphPin*> PinsToUnlink = OutputPin->LinkedTo;

			UClass const* CallingContext = NULL;
			if (UBlueprint const* Blueprint = GetBlueprint())
			{
				CallingContext = Blueprint->GeneratedClass;
				if (CallingContext == NULL)
				{
					CallingContext = Blueprint->ParentClass;
				}
			}

			for (UEdGraphPin* TargetPin : PinsToUnlink)
			{
				if (TargetPin && !K2Schema->ArePinsCompatible(OutputPin, TargetPin, CallingContext))
				{
					OutputPin->BreakLinkTo(TargetPin);
				}
			}
		}
	}
}

void UK2Node_ConvertAsset::PostReconstructNode()
{
	RefreshPinTypes();

	Super::PostReconstructNode();
}

void UK2Node_ConvertAsset::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);
	if (Pin && (UK2Node_ConvertAssetImpl::InputPinName == Pin->PinName))
	{
		RefreshPinTypes();

		GetGraph()->NotifyGraphChanged();
	}
}

void UK2Node_ConvertAsset::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, FString(), nullptr, UK2Node_ConvertAssetImpl::InputPinName);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Wildcard, FString(), nullptr, UK2Node_ConvertAssetImpl::OutputPinName);
}

UK2Node::ERedirectType UK2Node_ConvertAsset::DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const
{
	// Names changed, only thing that matters is the direction
	if (NewPin->Direction == OldPin->Direction)
	{
		return UK2Node::ERedirectType_Name;
	}
	return UK2Node::ERedirectType_None;
}

void UK2Node_ConvertAsset::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	// actions get registered under specific object-keys; the idea is that 
	// actions might have to be updated (or deleted) if their object-key is  
	// mutated (or removed)... here we use the node's class (so if the node 
	// type disappears, then the action should go with it)
	UClass* ActionKey = GetClass();
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
		check(NodeSpawner != nullptr);

		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

void UK2Node_ConvertAsset::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);
	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	
	UClass* TargetType = GetTargetClass();
	if (TargetType && Schema && (2 == Pins.Num()))
	{
		const bool bIsAssetClass = IsAssetClassType();
		UClass *TargetClass = GetTargetClass();
		bool bIsErrorFree = true;

		if (IsConvertToAsset())
		{
			//Create Convert Function
			UK2Node_CallFunction* ConvertToObjectFunc = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
			FName ConvertFunctionName = bIsAssetClass
				? GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, Conv_ClassToSoftClassReference)
				: GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, Conv_ObjectToSoftObjectReference);

			ConvertToObjectFunc->FunctionReference.SetExternalMember(ConvertFunctionName, UKismetSystemLibrary::StaticClass());
			ConvertToObjectFunc->AllocateDefaultPins();

			//Connect input to convert
			UEdGraphPin* InputPin = FindPin(UK2Node_ConvertAssetImpl::InputPinName);
			const FString ConvertInputName = bIsAssetClass
				? FString(TEXT("Class"))
				: FString(TEXT("Object"));
			UEdGraphPin* ConvertInput = ConvertToObjectFunc->FindPin(ConvertInputName);
			bIsErrorFree = InputPin && ConvertInput && CompilerContext.MovePinLinksToIntermediate(*InputPin, *ConvertInput).CanSafeConnect();

			if (UEdGraphPin* ConvertOutput = ConvertToObjectFunc->GetReturnValuePin())
			{
				// Force the convert output pin to the right type. This is only safe because all asset ptrs are type-compatible, the cast is done at resolution time
				ConvertOutput->PinType.PinSubCategoryObject = TargetClass;

				UEdGraphPin* OutputPin = FindPin(UK2Node_ConvertAssetImpl::OutputPinName);
				bIsErrorFree &= OutputPin && CompilerContext.MovePinLinksToIntermediate(*OutputPin, *ConvertOutput).CanSafeConnect();
			}
			else
			{
				bIsErrorFree = false;
			}
		}
		else
		{
			//Create Convert Function
			UK2Node_CallFunction* ConvertToObjectFunc = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
			FName ConvertFunctionName = bIsAssetClass
				? GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, Conv_SoftClassReferenceToClass)
				: GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, Conv_SoftObjectReferenceToObject);

			ConvertToObjectFunc->FunctionReference.SetExternalMember(ConvertFunctionName, UKismetSystemLibrary::StaticClass());
			ConvertToObjectFunc->AllocateDefaultPins();

			//Connect input to convert
			UEdGraphPin* InputPin = FindPin(UK2Node_ConvertAssetImpl::InputPinName);
			const FString ConvertInputName = bIsAssetClass
				? FString(TEXT("SoftClass"))
				: FString(TEXT("SoftObject"));
			UEdGraphPin* ConvertInput = ConvertToObjectFunc->FindPin(ConvertInputName);
			bIsErrorFree = InputPin && ConvertInput && CompilerContext.MovePinLinksToIntermediate(*InputPin, *ConvertInput).CanSafeConnect();

			UEdGraphPin* ConvertOutput = ConvertToObjectFunc->GetReturnValuePin();
			UEdGraphPin* InnerOutput = nullptr;
			if (UObject::StaticClass() != TargetType)
			{
				//Create Cast Node
				UK2Node_DynamicCast* CastNode = bIsAssetClass
					? CompilerContext.SpawnIntermediateNode<UK2Node_ClassDynamicCast>(this, SourceGraph)
					: CompilerContext.SpawnIntermediateNode<UK2Node_DynamicCast>(this, SourceGraph);
				CastNode->SetPurity(true);
				CastNode->TargetType = TargetType;
				CastNode->AllocateDefaultPins();

				// Connect Object/Class to Cast
				UEdGraphPin* CastInput = CastNode->GetCastSourcePin();
				bIsErrorFree &= ConvertOutput && CastInput && Schema->TryCreateConnection(ConvertOutput, CastInput);

				// Connect output to cast
				InnerOutput = CastNode->GetCastResultPin();
			}
			else
			{
				InnerOutput = ConvertOutput;
			}

			UEdGraphPin* OutputPin = FindPin(UK2Node_ConvertAssetImpl::OutputPinName);
			bIsErrorFree &= OutputPin && InnerOutput && CompilerContext.MovePinLinksToIntermediate(*OutputPin, *InnerOutput).CanSafeConnect();
		}

		if (!bIsErrorFree)
		{
			CompilerContext.MessageLog.Error(*LOCTEXT("InternalConnectionError", "K2Node_ConvertAsset: Internal connection error. @@").ToString(), this);
		}

		BreakAllNodeLinks();
	}
}

FText UK2Node_ConvertAsset::GetCompactNodeTitle() const
{
	return FText::FromString(TEXT("\x2022"));
}

FText UK2Node_ConvertAsset::GetMenuCategory() const
{
	return FText(LOCTEXT("UK2Node_LoadAssetGetMenuCategory", "Utilities"));
}

FText UK2Node_ConvertAsset::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText(LOCTEXT("UK2Node_ConvertAssetGetNodeTitle", "Resolve Soft Reference"));
}

FText UK2Node_ConvertAsset::GetKeywords() const
{
	// Return old name here
	return FText(LOCTEXT("UK2Node_ConvertAssetGetKeywords", "Resolve Asset ID"));
}

FText UK2Node_ConvertAsset::GetTooltipText() const
{
	return FText(LOCTEXT("UK2Node_ConvertAssetGetTooltipText", "Resolves a Soft Reference or Soft Class Reference into an object/class or vice versa. If the object isn't already loaded it returns none."));
}

#undef LOCTEXT_NAMESPACE
