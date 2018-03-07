// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "K2Node_FormatText.h"
#include "UObject/Package.h"
#include "Kismet/KismetSystemLibrary.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphSchema_K2_Actions.h"
#include "K2Node_CallFunction.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MakeStruct.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet/KismetTextLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "KismetCompiler.h"
#include "ScopedTransaction.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"

#define LOCTEXT_NAMESPACE "K2Node_FormatText"

/////////////////////////////////////////////////////
// UK2Node_FormatText

struct FFormatTextNodeHelper
{
	static const FString& GetFormatPinName()
	{
		static const FString FormatPinName(TEXT("Format"));
		return FormatPinName;
	}
};

UK2Node_FormatText::UK2Node_FormatText(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CachedFormatPin(NULL)
{
	NodeTooltip = LOCTEXT("NodeTooltip", "Builds a formatted string using available format argument values.\n  \u2022 Use {} to denote format arguments.\n  \u2022 Argument types may be Byte, Integer, Float, Text, or ETextGender.");
}

void UK2Node_FormatText::AllocateDefaultPins()
{
	Super::AllocateDefaultPins();

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	CachedFormatPin = CreatePin(EGPD_Input, K2Schema->PC_Text, FString(), nullptr, FFormatTextNodeHelper::GetFormatPinName());
	CreatePin(EGPD_Output, K2Schema->PC_Text, FString(), nullptr, TEXT("Result"));

	for (const FString& PinName : PinNames)
	{
		CreatePin(EGPD_Input, K2Schema->PC_Wildcard, FString(), nullptr, PinName);
	}
}

void UK2Node_FormatText::SynchronizeArgumentPinType(UEdGraphPin* Pin)
{
	const UEdGraphPin* FormatPin = GetFormatPin();
	if (Pin != FormatPin && Pin->Direction == EGPD_Input)
	{
		const UEdGraphSchema_K2* K2Schema = Cast<const UEdGraphSchema_K2>(GetSchema());

		bool bPinTypeChanged = false;
		if (Pin->LinkedTo.Num() == 0)
		{
			static const FEdGraphPinType WildcardPinType = FEdGraphPinType(K2Schema->PC_Wildcard, FString(), nullptr, EPinContainerType::None, false, FEdGraphTerminalType());

			// Ensure wildcard
			if (Pin->PinType != WildcardPinType)
			{
				Pin->PinType = WildcardPinType;
				bPinTypeChanged = true;
			}
		}
		else
		{
			UEdGraphPin* ArgumentSourcePin = Pin->LinkedTo[0];

			// Take the type of the connected pin
			if (Pin->PinType != ArgumentSourcePin->PinType)
			{
				Pin->PinType = ArgumentSourcePin->PinType;
				bPinTypeChanged = true;
			}
		}

		if (bPinTypeChanged)
		{
			// Let the graph know to refresh
			GetGraph()->NotifyGraphChanged();

			UBlueprint* Blueprint = GetBlueprint();
			if (!Blueprint->bBeingCompiled)
			{
				FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
				Blueprint->BroadcastChanged();
			}
		}
	}
}

FText UK2Node_FormatText::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("FormatText_Title", "Format Text");
}

FText UK2Node_FormatText::GetPinDisplayName(const UEdGraphPin* Pin) const
{
	return FText::FromString(Pin->PinName);
}

FString UK2Node_FormatText::GetUniquePinName()
{
	FString NewPinName;
	int32 i = 0;
	while (true)
	{
		NewPinName = FString::FromInt(i++);
		if (!FindPin(NewPinName))
		{
			break;
		}
	}
	return NewPinName;
}

void UK2Node_FormatText::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property  ? PropertyChangedEvent.Property->GetFName() : NAME_None);
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UK2Node_FormatText, PinNames))
	{
		ReconstructNode();
		GetGraph()->NotifyGraphChanged();
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UK2Node_FormatText::PinConnectionListChanged(UEdGraphPin* Pin)
{
	UEdGraphPin* FormatPin = GetFormatPin();

	Modify();

	// Clear all pins.
	if(Pin == FormatPin && !FormatPin->DefaultTextValue.IsEmpty())
	{
		PinNames.Empty();
		GetSchema()->TrySetDefaultText(*FormatPin, FText::GetEmpty());

		for(auto It = Pins.CreateConstIterator(); It; ++It)
		{
			UEdGraphPin* CheckPin = *It;
			if(CheckPin != FormatPin && CheckPin->Direction == EGPD_Input)
			{
				CheckPin->Modify();
				CheckPin->MarkPendingKill();
				Pins.Remove(CheckPin);
				--It;
			}
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
	}

	// Potentially update an argument pin type
	SynchronizeArgumentPinType(Pin);
}

void UK2Node_FormatText::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	const UEdGraphPin* FormatPin = GetFormatPin();
	if(Pin == FormatPin && FormatPin->LinkedTo.Num() == 0)
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		
		TArray< FString > ArgumentParams;
		FText::GetFormatPatternParameters(FormatPin->DefaultTextValue, ArgumentParams);

		PinNames.Empty();

		for (const FString& Param : ArgumentParams)
		{
			if(!FindArgumentPin(Param))
			{
				CreatePin(EGPD_Input, K2Schema->PC_Wildcard, FString(), nullptr, Param);
			}
			PinNames.Add(Param);
		}

		for(auto It = Pins.CreateConstIterator(); It; ++It)
		{
			UEdGraphPin* CheckPin = *It;
			if(CheckPin != FormatPin && CheckPin->Direction == EGPD_Input)
			{
				int Index = 0;
				if(!ArgumentParams.Find(CheckPin->PinName, Index))
				{
					CheckPin->MarkPendingKill();
					Pins.Remove(CheckPin);
					--It;
				}
			}
		}

		GetGraph()->NotifyGraphChanged();
	}
}

void UK2Node_FormatText::PinTypeChanged(UEdGraphPin* Pin)
{
	// Potentially update an argument pin type
	SynchronizeArgumentPinType(Pin);

	Super::PinTypeChanged(Pin);
}

FText UK2Node_FormatText::GetTooltipText() const
{
	return NodeTooltip;
}

UEdGraphPin* FindOutputStructPinChecked(UEdGraphNode* Node)
{
	check(NULL != Node);
	UEdGraphPin* OutputPin = NULL;
	for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
	{
		UEdGraphPin* Pin = Node->Pins[PinIndex];
		if (Pin && (EGPD_Output == Pin->Direction))
		{
			OutputPin = Pin;
			break;
		}
	}
	check(NULL != OutputPin);
	return OutputPin;
}

void UK2Node_FormatText::PostReconstructNode()
{
	Super::PostReconstructNode();

	// We need to upgrade any non-connected argument pins with valid literal text data to use a "Make Literal Text" node as an input (argument pins used to be PC_Text and they're now PC_Wildcard)
	if (!IsTemplate())
	{
		// Make sure we're not dealing with a menu node
		UEdGraph* OuterGraph = GetGraph();
		if (OuterGraph && OuterGraph->Schema)
		{
			int32 NumPinsFixedUp = 0;

			const UEdGraphPin* FormatPin = GetFormatPin();
			for (UEdGraphPin* CurrentPin : Pins)
			{
				if (CurrentPin != FormatPin && CurrentPin->Direction == EGPD_Input && CurrentPin->LinkedTo.Num() == 0 && !CurrentPin->DefaultTextValue.IsEmpty())
				{
					// Create a new "Make Literal Text" function and add it to the graph
					UK2Node_CallFunction* MakeLiteralText = nullptr;
					{
						UK2Node_CallFunction* MakeLiteralTextTemplate = NewObject<UK2Node_CallFunction>(GetGraph());
						MakeLiteralTextTemplate->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_MEMBER_NAME_CHECKED(UKismetSystemLibrary, MakeLiteralText)));

						const FVector2D SpawnLocation = FVector2D(NodePosX - 300, NodePosY + (60 * (NumPinsFixedUp + 1)));
						MakeLiteralText = FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_CallFunction>(GetGraph(), MakeLiteralTextTemplate, SpawnLocation, /*bSelectNewNode*/false);
					}

					// Set the new value and clear it on this pin to avoid it ever attempting this upgrade again (eg, if the "Make Literal Text" node was disconnected)
					UEdGraphPin* LiteralValuePin = MakeLiteralText->FindPinChecked(TEXT("Value"));
					LiteralValuePin->DefaultTextValue = CurrentPin->DefaultTextValue; // Note: Uses assignment rather than TrySetDefaultText to ensure we keep the existing localization identity
					CurrentPin->DefaultTextValue = FText::GetEmpty();

					// Connect the new node to the existing pin
					UEdGraphPin* LiteralReturnValuePin = MakeLiteralText->FindPinChecked(TEXT("ReturnValue"));
					GetSchema()->TryCreateConnection(LiteralReturnValuePin, CurrentPin);

					++NumPinsFixedUp;
				}

				// Potentially update an argument pin type
				SynchronizeArgumentPinType(CurrentPin);
			}

			if (NumPinsFixedUp > 0)
			{
				GetGraph()->NotifyGraphChanged();
				FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
			}
		}
	}
}

void UK2Node_FormatText::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	/**
		At the end of this, the UK2Node_FormatText will not be a part of the Blueprint, it merely handles connecting
		the other nodes into the Blueprint.
	*/

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();

	// Create a "Make Array" node to compile the list of arguments into an array for the Format function being called
	UK2Node_MakeArray* MakeArrayNode = CompilerContext.SpawnIntermediateNode<UK2Node_MakeArray>(this, SourceGraph);
	MakeArrayNode->AllocateDefaultPins();
	CompilerContext.MessageLog.NotifyIntermediateObjectCreation(MakeArrayNode, this);

	UEdGraphPin* ArrayOut = MakeArrayNode->GetOutputPin();

	// This is the node that does all the Format work.
	UK2Node_CallFunction* CallFormatFunction = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallFormatFunction->SetFromFunction(UKismetTextLibrary::StaticClass()->FindFunctionByName(GET_MEMBER_NAME_CHECKED(UKismetTextLibrary, Format)));
	CallFormatFunction->AllocateDefaultPins();
	CompilerContext.MessageLog.NotifyIntermediateObjectCreation(CallFormatFunction, this);

	// Connect the output of the "Make Array" pin to the function's "InArgs" pin
	ArrayOut->MakeLinkTo(CallFormatFunction->FindPinChecked(TEXT("InArgs")));

	// This will set the "Make Array" node's type, only works if one pin is connected.
	MakeArrayNode->PinConnectionListChanged(ArrayOut);

	// For each argument, we will need to add in a "Make Struct" node.
	for(int32 ArgIdx = 0; ArgIdx < PinNames.Num(); ++ArgIdx)
	{
		UEdGraphPin* ArgumentPin = FindArgumentPin(PinNames[ArgIdx]);

		static UScriptStruct* FormatArgumentDataStruct = FindObjectChecked<UScriptStruct>(FindObjectChecked<UPackage>(nullptr, TEXT("/Script/Engine")), TEXT("FormatArgumentData"));

		// Spawn a "Make Struct" node to create the struct needed for formatting the text.
		UK2Node_MakeStruct* MakeFormatArgumentDataStruct = CompilerContext.SpawnIntermediateNode<UK2Node_MakeStruct>(this, SourceGraph);
		MakeFormatArgumentDataStruct->StructType = FormatArgumentDataStruct;
		MakeFormatArgumentDataStruct->AllocateDefaultPins();
		MakeFormatArgumentDataStruct->bMadeAfterOverridePinRemoval = true;
		CompilerContext.MessageLog.NotifyIntermediateObjectCreation(MakeFormatArgumentDataStruct, this);

		// Set the struct's "ArgumentName" pin literal to be the argument pin's name.
		MakeFormatArgumentDataStruct->GetSchema()->TrySetDefaultValue(*MakeFormatArgumentDataStruct->FindPinChecked(GET_MEMBER_NAME_STRING_CHECKED(FFormatArgumentData, ArgumentName)), ArgumentPin->PinName);

		UEdGraphPin* ArgumentTypePin = MakeFormatArgumentDataStruct->FindPinChecked(GET_MEMBER_NAME_STRING_CHECKED(FFormatArgumentData, ArgumentValueType));

		// Move the connection of the argument pin to the correct argument value pin, and also set the correct argument type based on the pin that was hooked up.
		if (ArgumentPin->LinkedTo.Num() > 0)
		{
			const FString& ArgumentPinCategory = ArgumentPin->PinType.PinCategory;

			const UEdGraphSchema_K2* K2Schema = Cast<const UEdGraphSchema_K2>(GetSchema());
			if (ArgumentPinCategory == K2Schema->PC_Int)
			{
				MakeFormatArgumentDataStruct->GetSchema()->TrySetDefaultValue(*ArgumentTypePin, TEXT("Int"));
				CompilerContext.MovePinLinksToIntermediate(*ArgumentPin, *MakeFormatArgumentDataStruct->FindPinChecked(GET_MEMBER_NAME_STRING_CHECKED(FFormatArgumentData, ArgumentValueInt)));
			}
			else if (ArgumentPinCategory == K2Schema->PC_Float)
			{
				MakeFormatArgumentDataStruct->GetSchema()->TrySetDefaultValue(*ArgumentTypePin, TEXT("Float"));
				CompilerContext.MovePinLinksToIntermediate(*ArgumentPin, *MakeFormatArgumentDataStruct->FindPinChecked(GET_MEMBER_NAME_STRING_CHECKED(FFormatArgumentData, ArgumentValueFloat)));
			}
			else if (ArgumentPinCategory == K2Schema->PC_Text)
			{
				MakeFormatArgumentDataStruct->GetSchema()->TrySetDefaultValue(*ArgumentTypePin, TEXT("Text"));
				CompilerContext.MovePinLinksToIntermediate(*ArgumentPin, *MakeFormatArgumentDataStruct->FindPinChecked(GET_MEMBER_NAME_STRING_CHECKED(FFormatArgumentData, ArgumentValue)));
			}
			else if (ArgumentPinCategory == K2Schema->PC_Byte && !ArgumentPin->PinType.PinSubCategoryObject.IsValid())
			{
				MakeFormatArgumentDataStruct->GetSchema()->TrySetDefaultValue(*ArgumentTypePin, TEXT("Int"));

				// Need a manual cast from byte -> int
				UK2Node_CallFunction* CallByteToIntFunction = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
				CallByteToIntFunction->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(GET_MEMBER_NAME_CHECKED(UKismetMathLibrary, Conv_ByteToInt)));
				CallByteToIntFunction->AllocateDefaultPins();
				CompilerContext.MessageLog.NotifyIntermediateObjectCreation(CallByteToIntFunction, this);

				// Move the byte output pin to the input pin of the conversion node
				CompilerContext.MovePinLinksToIntermediate(*ArgumentPin, *CallByteToIntFunction->FindPinChecked(TEXT("InByte")));

				// Connect the int output pin to the argument value
				CallByteToIntFunction->FindPinChecked(TEXT("ReturnValue"))->MakeLinkTo(MakeFormatArgumentDataStruct->FindPinChecked(GET_MEMBER_NAME_STRING_CHECKED(FFormatArgumentData, ArgumentValueInt)));
			}
			else if (ArgumentPinCategory == K2Schema->PC_Byte || ArgumentPinCategory == K2Schema->PC_Enum)
			{
				static UEnum* TextGenderEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("ETextGender"), /*ExactClass*/true);
				if (ArgumentPin->PinType.PinSubCategoryObject == TextGenderEnum)
				{
					MakeFormatArgumentDataStruct->GetSchema()->TrySetDefaultValue(*ArgumentTypePin, TEXT("Gender"));
					CompilerContext.MovePinLinksToIntermediate(*ArgumentPin, *MakeFormatArgumentDataStruct->FindPinChecked(GET_MEMBER_NAME_STRING_CHECKED(FFormatArgumentData, ArgumentValueGender)));
				}
			}
			else
			{
				// Unexpected pin type!
				CompilerContext.MessageLog.Error(*FText::Format(LOCTEXT("Error_UnexpectedPinType", "Pin '{0}' has an unexpected type: {1}"), FText::FromString(PinNames[ArgIdx]), FText::FromString(ArgumentPinCategory)).ToString());
			}
		}
		else
		{
			// No connected pin - just default to an empty text
			MakeFormatArgumentDataStruct->GetSchema()->TrySetDefaultValue(*ArgumentTypePin, TEXT("Text"));
			MakeFormatArgumentDataStruct->GetSchema()->TrySetDefaultText(*MakeFormatArgumentDataStruct->FindPinChecked(GET_MEMBER_NAME_STRING_CHECKED(FFormatArgumentData, ArgumentValue)), FText::GetEmpty());
		}

		// The "Make Array" node already has one pin available, so don't create one for ArgIdx == 0
		if(ArgIdx > 0)
		{
			MakeArrayNode->AddInputPin();
		}

		// Find the input pin on the "Make Array" node by index.
		FString PinName = FString::Printf(TEXT("[%d]"), ArgIdx);
		UEdGraphPin* InputPin = MakeArrayNode->FindPinChecked(PinName);

		// Find the output for the pin's "Make Struct" node and link it to the corresponding pin on the "Make Array" node.
		FindOutputStructPinChecked(MakeFormatArgumentDataStruct)->MakeLinkTo(InputPin);
	}

	// Move connection of FormatText's "Result" pin to the call function's return value pin.
	CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(TEXT("Result")), *CallFormatFunction->GetReturnValuePin());
	// Move connection of FormatText's "Format" pin to the call function's "InPattern" pin
	CompilerContext.MovePinLinksToIntermediate(*GetFormatPin(), *CallFormatFunction->FindPinChecked(TEXT("InPattern")));

	BreakAllNodeLinks();
}

UEdGraphPin* UK2Node_FormatText::FindArgumentPin(const FString& InPinName) const
{
	const UEdGraphPin* FormatPin = GetFormatPin();
	for(int32 PinIdx=0; PinIdx<Pins.Num(); PinIdx++)
	{
		if( Pins[PinIdx] != FormatPin && Pins[PinIdx]->Direction != EGPD_Output && Pins[PinIdx]->PinName.Equals(InPinName) )
		{
			return Pins[PinIdx];
		}
	}

	return NULL;
}

UK2Node::ERedirectType UK2Node_FormatText::DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const
{
	ERedirectType RedirectType = ERedirectType_None;

	// if the pin names do match
	if (FCString::Strcmp(*(NewPin->PinName), *(OldPin->PinName)) == 0)
	{
		// Make sure we're not dealing with a menu node
		UEdGraph* OuterGraph = GetGraph();
		if( OuterGraph && OuterGraph->Schema )
		{
			const UEdGraphSchema_K2* K2Schema = Cast<const UEdGraphSchema_K2>(GetSchema());
			if( !K2Schema || K2Schema->IsSelfPin(*NewPin) || K2Schema->ArePinTypesCompatible(OldPin->PinType, NewPin->PinType) )
			{
				RedirectType = ERedirectType_Name;
			}
			else
			{
				RedirectType = ERedirectType_None;
			}
		}
	}
	else
	{
		// try looking for a redirect if it's a K2 node
		if (UK2Node* Node = Cast<UK2Node>(NewPin->GetOwningNode()))
		{	
			// if you don't have matching pin, now check if there is any redirect param set
			TArray<FString> OldPinNames;
			GetRedirectPinNames(*OldPin, OldPinNames);

			FName NewPinName;
			RedirectType = ShouldRedirectParam(OldPinNames, /*out*/ NewPinName, Node);

			// make sure they match
			if ((RedirectType != ERedirectType_None) && FCString::Stricmp(*(NewPin->PinName), *(NewPinName.ToString())) != 0)
			{
				RedirectType = ERedirectType_None;
			}
		}
	}

	return RedirectType;
}

bool UK2Node_FormatText::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	// Argument input pins may only be connected to Byte, Integer, Float, Text, and ETextGender pins...
	const UEdGraphPin* FormatPin = GetFormatPin();
	if (MyPin != FormatPin && MyPin->Direction == EGPD_Input)
	{
		const UEdGraphSchema_K2* K2Schema = Cast<const UEdGraphSchema_K2>(GetSchema());
		const FString& OtherPinCategory = OtherPin->PinType.PinCategory;

		bool bIsValidType = false;
		if (OtherPinCategory == K2Schema->PC_Int || OtherPinCategory == K2Schema->PC_Float || OtherPinCategory == K2Schema->PC_Text || (OtherPinCategory == K2Schema->PC_Byte && !OtherPin->PinType.PinSubCategoryObject.IsValid()))
		{
			bIsValidType = true;
		}
		else if (OtherPinCategory == K2Schema->PC_Byte || OtherPinCategory == K2Schema->PC_Enum)
		{
			static UEnum* TextGenderEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("ETextGender"), /*ExactClass*/true);
			if (OtherPin->PinType.PinSubCategoryObject == TextGenderEnum)
			{
				bIsValidType = true;
			}
		}

		if (!bIsValidType)
		{
			OutReason = LOCTEXT("Error_InvalidArgumentType", "Format arguments may only be Byte, Integer, Float, Text, or ETextGender.").ToString();
			return true;
		}
	}

	return Super::IsConnectionDisallowed(MyPin, OtherPin, OutReason);
}

FText UK2Node_FormatText::GetArgumentName(int32 InIndex) const
{
	if(InIndex < PinNames.Num())
	{
		return FText::FromString(PinNames[InIndex]);
	}
	return FText::GetEmpty();
}

void UK2Node_FormatText::AddArgumentPin()
{
	const FScopedTransaction Transaction( NSLOCTEXT("Kismet", "AddArgumentPin", "Add Argument Pin") );
	Modify();

	const UEdGraphSchema_K2* K2Schema = Cast<const UEdGraphSchema_K2>(GetSchema());
	FString PinName = GetUniquePinName();
	CreatePin(EGPD_Input, K2Schema->PC_Wildcard, FString(), nullptr, PinName);
	PinNames.Add(PinName);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
	GetGraph()->NotifyGraphChanged();
}

void UK2Node_FormatText::RemoveArgument(int32 InIndex)
{
	const FScopedTransaction Transaction( NSLOCTEXT("Kismet", "RemoveArgumentPin", "Remove Argument Pin") );
	Modify();

	if (UEdGraphPin* ArgumentPin = FindArgumentPin(PinNames[InIndex]))
	{
		Pins.Remove(ArgumentPin);
		ArgumentPin->MarkPendingKill();
	}
	PinNames.RemoveAt(InIndex);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
	GetGraph()->NotifyGraphChanged();
}

void UK2Node_FormatText::SetArgumentName(int32 InIndex, FString InName)
{
	PinNames[InIndex] = InName;

	ReconstructNode();

	FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprint());
}

void UK2Node_FormatText::SwapArguments(int32 InIndexA, int32 InIndexB)
{
	check(InIndexA < PinNames.Num());
	check(InIndexB < PinNames.Num());
	PinNames.Swap(InIndexA, InIndexB);

	ReconstructNode();
	GetGraph()->NotifyGraphChanged();

	FBlueprintEditorUtils::MarkBlueprintAsModified(GetBlueprint());
}

UEdGraphPin* UK2Node_FormatText::GetFormatPin() const
{
	if (!CachedFormatPin)
	{
		const_cast<UK2Node_FormatText*>(this)->CachedFormatPin = FindPinChecked(FFormatTextNodeHelper::GetFormatPinName());
	}
	return CachedFormatPin;
}

void UK2Node_FormatText::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
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

FText UK2Node_FormatText::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Text);
}

#undef LOCTEXT_NAMESPACE
