// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "K2Node_VariableGet.h"
#include "UObject/UObjectHash.h"
#include "UObject/PropertyPortFlags.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetCompilerMisc.h"
#include "KismetCompiler.h"
#include "ScopedTransaction.h"

//////////////////////////////////////////////////////////////////////////
// FKCHandler_VariableGet

#define LOCTEXT_NAMESPACE "K2Node"

class FKCHandler_VariableGet : public FNodeHandlingFunctor
{
public:
	FKCHandler_VariableGet(FKismetCompilerContext& InCompilerContext)
		: FNodeHandlingFunctor(InCompilerContext)
	{
	}

	virtual void RegisterNet(FKismetFunctionContext& Context, UEdGraphPin* Net) override
	{
		// This net is a variable read
		ResolveAndRegisterScopedTerm(Context, Net, Context.VariableReferences);
	}

	virtual void RegisterNets(FKismetFunctionContext& Context, UEdGraphNode* Node) override
	{
		UK2Node_Variable* VarNode = Cast<UK2Node_Variable>(Node);
		if (VarNode)
		{
			VarNode->CheckForErrors(CompilerContext.GetSchema(), Context.MessageLog);

			// Report an error that the local variable could not be found
			if(VarNode->VariableReference.IsLocalScope() && VarNode->GetPropertyForVariable() == NULL)
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("VariableName"), FText::FromName(VarNode->VariableReference.GetMemberName()));

				if(VarNode->VariableReference.GetMemberScopeName() != Context.Function->GetName())
				{
					Args.Add(TEXT("ScopeName"), FText::FromString(VarNode->VariableReference.GetMemberScopeName()));
					CompilerContext.MessageLog.Warning(*FText::Format(LOCTEXT("LocalVariableNotFoundInScope_Error", "Unable to find local variable with name '{VariableName}' for @@, scope expected: @@, scope found: {ScopeName}"), Args).ToString(), Node, Node->GetGraph());
				}
				else
				{
					CompilerContext.MessageLog.Warning(*FText::Format(LOCTEXT("LocalVariableNotFound_Error", "Unable to find local variable with name '{VariableName}' for @@"), Args).ToString(), Node);
				}
			}
		}

		FNodeHandlingFunctor::RegisterNets(Context, Node);
	}
};

namespace K2Node_VariableGetImpl
{
	/**
	 * Shared utility method for retrieving a UK2Node_VariableGet's bare tooltip.
	 * 
	 * @param  VarName	The name of the variable that the node represents.
	 * @return A formatted text string, describing what the VariableGet node does.
	 */
	static FText GetBaseTooltip(FName VarName);
}

static FText K2Node_VariableGetImpl::GetBaseTooltip(FName VarName)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("VarName"), FText::FromName(VarName));

	return FText::Format(LOCTEXT("GetVariableTooltip", "Read the value of variable {VarName}"), Args);

}

UK2Node_VariableGet::UK2Node_VariableGet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bIsPureGet(true)
{
}

void UK2Node_VariableGet::CreateNonPurePins(TArray<UEdGraphPin*>* InOldPinsPtr)
{
	const UEdGraphSchema_K2* K2Schema = Cast<UEdGraphSchema_K2>(GetSchema());
	check(K2Schema != nullptr);
	if (!K2Schema->DoesGraphSupportImpureFunctions(GetGraph()))
	{
		bIsPureGet = true;
	}

	if (!bIsPureGet)
	{
		FEdGraphPinType PinType;
		UProperty* VariableProperty = GetPropertyForVariable();

		// We need the pin's type, to both see if it's an array and if it is of the correct types to remain an impure node
		if (VariableProperty)
		{
			K2Schema->ConvertPropertyToPinType(GetPropertyForVariable(), PinType);
		}
		// If there is no property and we are given some old pins to look at, find the old value pin and use the type there
		// This allows nodes to be pasted into other BPs without access to the property
		else if(InOldPinsPtr)
		{
			// find old variable pin and use the type.
			const FString PinName = GetVarNameString();
			for(auto Iter = InOldPinsPtr->CreateConstIterator(); Iter; ++Iter)
			{
				if(const UEdGraphPin* Pin = *Iter)
				{
					if(PinName == Pin->PinName)
					{
						PinType = Pin->PinType;
						break;
					}
				}
			}

		}

		if (IsValidTypeForNonPure(PinType))
		{
			// Input - Execution Pin
			CreatePin(EGPD_Input, K2Schema->PC_Exec, FString(), nullptr, K2Schema->PN_Execute);

			// Output - Execution Pins
			UEdGraphPin* ValidPin = CreatePin(EGPD_Output, K2Schema->PC_Exec, FString(), nullptr, K2Schema->PN_Then);
			ValidPin->PinFriendlyName = LOCTEXT("Valid", "Is Valid");

			UEdGraphPin* InvalidPin = CreatePin(EGPD_Output, K2Schema->PC_Exec, FString(), nullptr, K2Schema->PN_Else);
			InvalidPin->PinFriendlyName = LOCTEXT("Invalid", "Is Not Valid");
		}
		else
		{
			bIsPureGet = true;
		}
	}
}

void UK2Node_VariableGet::AllocateDefaultPins()
{
	if(GetVarName() != NAME_None)
	{
		CreateNonPurePins(nullptr);

		if(CreatePinForVariable(EGPD_Output))
		{
			CreatePinForSelf();
		}
	}

	Super::AllocateDefaultPins();
}

void UK2Node_VariableGet::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	if(GetVarName() != NAME_None)
	{
		CreateNonPurePins(&OldPins);

		if(!CreatePinForVariable(EGPD_Output))
		{
			if(!RecreatePinForVariable(EGPD_Output, OldPins))
			{
				return;
			}
		}
		CreatePinForSelf();

		RestoreSplitPins(OldPins);
	}
}

FText UK2Node_VariableGet::GetPropertyTooltip(UProperty const* VariableProperty)
{
	FName VarName = NAME_None;
	if (VariableProperty != nullptr)
	{
		VarName = VariableProperty->GetFName();

		UClass* SourceClass = VariableProperty->GetOwnerClass();
		// discover if the variable property is a non blueprint user variable
		bool const bIsNativeVariable = (SourceClass != nullptr) && (SourceClass->ClassGeneratedBy == nullptr);
		FName const TooltipMetaKey(TEXT("tooltip"));

		FText SubTooltip;
		if (bIsNativeVariable)
		{
			FText const PropertyTooltip = VariableProperty->GetToolTipText();
			if (!PropertyTooltip.IsEmpty())
			{
				// See if the native property has a tooltip
				SubTooltip = PropertyTooltip;
				FString TooltipName = FString::Printf(TEXT("%s.%s"), *VarName.ToString(), *TooltipMetaKey.ToString());
				FText::FindText(*VariableProperty->GetFullGroupName(true), *TooltipName, SubTooltip);
			}
		}
		else if (SourceClass)
		{
			if (UBlueprint* VarBlueprint = Cast<UBlueprint>(SourceClass->ClassGeneratedBy))
			{
				FString UserTooltipData;
				if (FBlueprintEditorUtils::GetBlueprintVariableMetaData(VarBlueprint, VarName, VariableProperty->GetOwnerStruct(), TooltipMetaKey, UserTooltipData))
				{
					SubTooltip = FText::FromString(UserTooltipData);
				}
			}
		}

		if (!SubTooltip.IsEmpty())
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("VarName"), FText::FromName(VarName));
			Args.Add(TEXT("PropertyTooltip"), SubTooltip);

			return FText::Format(LOCTEXT("GetVariableProperty_Tooltip", "Read the value of variable {VarName}\n{PropertyTooltip}"), Args);
		}
	}
	return K2Node_VariableGetImpl::GetBaseTooltip(VarName);
}

FText UK2Node_VariableGet::GetBlueprintVarTooltip(FBPVariableDescription const& VarDesc)
{
	FName const TooltipMetaKey(TEXT("tooltip"));
	int32 const MetaIndex = VarDesc.FindMetaDataEntryIndexForKey(TooltipMetaKey);
	bool const bHasTooltipData = (MetaIndex != INDEX_NONE);

	if (bHasTooltipData)
	{
		FString UserTooltipData = VarDesc.GetMetaData(TooltipMetaKey);

		FFormatNamedArguments Args;
		Args.Add(TEXT("VarName"), FText::FromName(VarDesc.VarName));
		Args.Add(TEXT("UserTooltip"), FText::FromString(UserTooltipData));

		return FText::Format(LOCTEXT("GetBlueprintVariable_Tooltip", "Read the value of variable {VarName}\n{UserTooltip}"), Args);
	}
	return K2Node_VariableGetImpl::GetBaseTooltip(VarDesc.VarName);
}

FText UK2Node_VariableGet::GetTooltipText() const
{
	if (CachedTooltip.IsOutOfDate(this))
	{
		if (UProperty* Property = GetPropertyForVariable())
		{
			CachedTooltip.SetCachedText(GetPropertyTooltip(Property), this);
		}
		else if (FBPVariableDescription const* VarDesc = GetBlueprintVarDescription())
		{
			CachedTooltip.SetCachedText(GetBlueprintVarTooltip(*VarDesc), this);
		}
		else
		{
			CachedTooltip.SetCachedText(K2Node_VariableGetImpl::GetBaseTooltip(GetVarName()), this);
		}
	}
	return CachedTooltip;
}

FText UK2Node_VariableGet::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	// If there is only one variable being read, the title can be made the variable name
	FString OutputPinName;
	int32 NumOutputsFound = 0;

	for (int32 PinIndex = 0; PinIndex < Pins.Num(); ++PinIndex)
	{
		UEdGraphPin* Pin = Pins[PinIndex];

		// The following code is to attempt to log info related to UE-19729
		if (TitleType == ENodeTitleType::ListView)
		{
			if (UEdGraph* Graph = Cast<UEdGraph>(GetOuter()))
			{
				FString VariableName = GetVarNameString();
				FString BlueprintPath = FBlueprintEditorUtils::FindBlueprintForGraph(Graph)->GetPathName();
				FString SetupStyle = bIsPureGet? TEXT("pure") : TEXT("validated");
				FString VariableResolves = (VariableReference.ResolveMember<UProperty>(GetBlueprintClassFromNode()) != nullptr)? TEXT("resolves") : TEXT("does not resolve");
				checkf(Pin, TEXT("Get node for variable '%s' in Blueprint '%s' which is setup as %s and has %d pins. Variable %s"), *VariableName, *BlueprintPath, *SetupStyle, Pins.Num(), *VariableResolves);
			}
		}

		if (Pin->Direction == EGPD_Output)
		{
			++NumOutputsFound;
			OutputPinName = Pin->PinName;
		}
	}

	if (NumOutputsFound != 1)
	{
		return LOCTEXT("Get", "Get");
	}
	else if (CachedNodeTitle.IsOutOfDate(this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("PinName"), FText::FromString(OutputPinName));
		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("GetPinName", "Get {PinName}"), Args), this);
	}
	return CachedNodeTitle;
}

FNodeHandlingFunctor* UK2Node_VariableGet::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_VariableGet(CompilerContext);
}

bool UK2Node_VariableGet::IsValidTypeForNonPure(const FEdGraphPinType& InPinType)
{
	return !InPinType.IsContainer() && (InPinType.PinCategory == UObject::StaticClass()->GetName() ||InPinType.PinCategory == UClass::StaticClass()->GetName());
}

void UK2Node_VariableGet::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	Super::GetContextMenuActions(Context);

	const UEdGraphPin* ValuePin = GetValuePin();
	if (ValuePin && IsValidTypeForNonPure(ValuePin->PinType))
	{
		Context.MenuBuilder->BeginSection("K2NodeVariableGet", LOCTEXT("VariableGetHeader", "Variable Get"));
		{
			FText MenuEntryTitle;
			FText MenuEntryTooltip;

			bool bCanTogglePurity = true;
			auto CanExecutePurityToggle = [](bool const bInCanTogglePurity)->bool
			{
				return bInCanTogglePurity;
			};

			if (bIsPureGet)
			{
				MenuEntryTitle   = LOCTEXT("ConvertToImpureGetTitle",   "Convert to Validated Get");
				MenuEntryTooltip = LOCTEXT("ConvertToImpureGetTooltip", "Adds in branching execution pins so that you can separately handle when the returned value is valid/invalid.");

				const UEdGraphSchema_K2* K2Schema = Cast<UEdGraphSchema_K2>(GetSchema());
				check(K2Schema != nullptr);

				bCanTogglePurity = K2Schema->DoesGraphSupportImpureFunctions(GetGraph());
				if (!bCanTogglePurity)
				{
					MenuEntryTooltip = LOCTEXT("CannotMakeImpureGetTooltip", "This graph does not support impure calls!");
				}
			}
			else
			{
				MenuEntryTitle   = LOCTEXT("ConvertToPureGetTitle",   "Convert to pure Get");
				MenuEntryTooltip = LOCTEXT("ConvertToPureGetTooltip", "Removes the execution pins to make the node more versatile.");
			}

			Context.MenuBuilder->AddMenuEntry(
				MenuEntryTitle,
				MenuEntryTooltip,
				FSlateIcon(),
				FUIAction(
				FExecuteAction::CreateUObject(this, &UK2Node_VariableGet::TogglePurity),
				FCanExecuteAction::CreateStatic(CanExecutePurityToggle, bCanTogglePurity && !Context.bIsDebugging),
				FIsActionChecked()
				)
				);
		}
		Context.MenuBuilder->EndSection();
	}
}

void UK2Node_VariableGet::TogglePurity()
{
	FText TransactionTitle;
	if(!bIsPureGet)
	{
		TransactionTitle = LOCTEXT("TogglePureGet", "Convert to Pure Get");
	}
	else
	{
		TransactionTitle = LOCTEXT("ToggleImpureGet", "Convert to Impure Get");
	}
	const FScopedTransaction Transaction( TransactionTitle );
	Modify();

	SetPurity(!bIsPureGet);
}

void UK2Node_VariableGet::SetPurity(bool bNewPurity)
{
	if (bNewPurity != bIsPureGet)
	{
		bIsPureGet = bNewPurity;

		bool const bHasBeenConstructed = (Pins.Num() > 0);
		if (bHasBeenConstructed)
		{
			ReconstructNode();
		}
	}
}

void UK2Node_VariableGet::ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	// Some expansions, such as timelines, will create gets for non-blueprint visible properties, and we don't want to validate against that
	if (!IsIntermediateNode())
	{
		if (UProperty* Property = GetPropertyForVariable())
		{
			const FBlueprintEditorUtils::EPropertyReadableState PropertyReadableState = FBlueprintEditorUtils::IsPropertyReadableInBlueprint(GetBlueprint(), Property);

			if (PropertyReadableState != FBlueprintEditorUtils::EPropertyReadableState::Readable)
			{
				FFormatNamedArguments Args;
				if (UObject* Class = Property->GetOuter())
				{
					Args.Add(TEXT("VariableName"), FText::AsCultureInvariant(FString::Printf(TEXT("%s.%s"), *Class->GetName(), *Property->GetName())));
				}
				else
				{
					Args.Add(TEXT("VariableName"), FText::AsCultureInvariant(Property->GetName()));
				}

				if (PropertyReadableState == FBlueprintEditorUtils::EPropertyReadableState::NotBlueprintVisible)
				{
					// DEPRECATED(4.17) ... make this an error
					MessageLog.Warning(*FText::Format(LOCTEXT("UnableToGet_NotVisible", "{VariableName} is not blueprint visible (BlueprintReadOnly or BlueprintReadWrite). Please fix mark up or cease accessing as this will be made an error in a future release. @@"), Args).ToString(), this);
				}
				else if (PropertyReadableState == FBlueprintEditorUtils::EPropertyReadableState::Private)
				{
					// DEPRECATED(4.17) ... make this an error
					MessageLog.Warning(*FText::Format(LOCTEXT("UnableToGet_ReadOnly", "{VariableName} is private and not accessible in this context. Please fix mark up or cease accessing as this will be an error in a future release. @@"), Args).ToString(), this);
				}
				else
				{
					check(false);
				}
			}
		}
	}
}

void UK2Node_VariableGet::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	UProperty* VariableProperty = GetPropertyForVariable();

	UK2Node_VariableGet* VariableGetNode = this;

	// Do not attempt to expand the node when not a pure get nor when there is no property. Normal compilation error detection will detect the missing property.
	if (!bIsPureGet && VariableProperty)
	{
		UEdGraphPin* ValuePin = GetValuePin();

		// Impure Get nodes convert into three nodes:
		// 1. A pure Get node
		// 2. An IsValid node
		// 3. A Branch node (only impure part)
		
		// Create the pure Get node
		VariableGetNode = CompilerContext.SpawnIntermediateNode<UK2Node_VariableGet>(this, SourceGraph);
		VariableGetNode->VariableReference = VariableReference;
		VariableGetNode->AllocateDefaultPins();
		CompilerContext.MessageLog.NotifyIntermediateObjectCreation(VariableGetNode, this);

		// Move pin links from Get node we are expanding, to the new pure one we've created
		CompilerContext.MovePinLinksToIntermediate(*ValuePin, *VariableGetNode->GetValuePin());
		if (!VariableReference.IsLocalScope())
		{
			CompilerContext.MovePinLinksToIntermediate(*FindPin(UEdGraphSchema_K2::PN_Self), *VariableGetNode->FindPin(UEdGraphSchema_K2::PN_Self));
		}

		// Create the IsValid node
		UK2Node_CallFunction* IsValidFunction = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);

		// Based on if the type is an "Object" or a "Class" changes which function to use
		if (ValuePin->PinType.PinCategory == UObject::StaticClass()->GetName())
		{
			IsValidFunction->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_MEMBER_NAME_CHECKED(UKismetSystemLibrary, IsValid)));
		}
		else if (ValuePin->PinType.PinCategory == UClass::StaticClass()->GetName())
		{
			IsValidFunction->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_MEMBER_NAME_CHECKED(UKismetSystemLibrary, IsValidClass)));
		}
		IsValidFunction->AllocateDefaultPins();
		CompilerContext.MessageLog.NotifyIntermediateObjectCreation(IsValidFunction, this);

		// Connect the value pin from the new Get node to the IsValid
		UEdGraphPin* ObjectPin = IsValidFunction->Pins[1];
		check(ObjectPin->Direction == EGPD_Input);
		ObjectPin->MakeLinkTo(VariableGetNode->GetValuePin());

		// Create the Branch node
		UK2Node_IfThenElse* BranchNode = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
		BranchNode->AllocateDefaultPins();
		CompilerContext.MessageLog.NotifyIntermediateObjectCreation(BranchNode, this);

		// Connect the bool output pin from IsValid node to the Branch node
		UEdGraphPin* BoolPin = IsValidFunction->Pins[2];
		check(BoolPin->Direction == EGPD_Output);
		BoolPin->MakeLinkTo(BranchNode->GetConditionPin());

		// Connect the Branch node to the input of the impure Get node
		CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *BranchNode->GetExecPin());

		// Move the two Branch pins to the Branch node
		CompilerContext.MovePinLinksToIntermediate(*FindPin(UEdGraphSchema_K2::PN_Then), *BranchNode->FindPin(UEdGraphSchema_K2::PN_Then));
		CompilerContext.MovePinLinksToIntermediate(*FindPin(UEdGraphSchema_K2::PN_Else), *BranchNode->FindPin(UEdGraphSchema_K2::PN_Else));

		BreakAllNodeLinks();
	}

	// If property has a BlueprintGetter accessor, then replace the variable get node with a call function
	if (VariableProperty)
	{
		const FString& GetFunctionName = VariableProperty->GetMetaData(FBlueprintMetadata::MD_PropertyGetFunction);
		if (!GetFunctionName.IsEmpty())
		{
			UClass* OwnerClass = VariableProperty->GetOwnerClass();
			UFunction* GetFunction = OwnerClass->FindFunctionByName(*GetFunctionName);
			check(GetFunction);

			UK2Node_CallFunction* CallFuncNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
			CallFuncNode->SetFromFunction(GetFunction);
			CallFuncNode->AllocateDefaultPins();

			const UEdGraphSchema_K2* K2Schema = CompilerContext.GetSchema();

			// Move Self pin connections
			CompilerContext.MovePinLinksToIntermediate(*K2Schema->FindSelfPin(*this, EGPD_Input), *K2Schema->FindSelfPin(*CallFuncNode, EGPD_Input));

			// Move Value pin connections
			CompilerContext.MovePinLinksToIntermediate(*GetValuePin(), *CallFuncNode->GetReturnValuePin());
		}
	}
}

void UK2Node_VariableGet::Serialize(FArchive& Ar)
{
	// The following code is to attempt to log info related to UE-19729
	if (Ar.IsSaving() && Ar.IsPersistent())
	{
		uint32 PortFlagsToSkip = PPF_Duplicate | PPF_DuplicateForPIE;
		if (!(Ar.GetPortFlags() & PortFlagsToSkip))
		{
			if (UEdGraph* Graph = Cast<UEdGraph>(GetOuter()))
			{
				if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph))
				{
					if (!Blueprint->bBeingCompiled)
					{
						// The following line may spur the crash noted in UE-19729 and will confirm that the crash happens before the FiB gather.
						GetNodeTitle(ENodeTitleType::ListView);
					}
				}
			}
		}
	}
	Super::Serialize(Ar);
}


#undef LOCTEXT_NAMESPACE
