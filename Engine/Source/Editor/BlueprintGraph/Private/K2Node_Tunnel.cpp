// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_Tunnel.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Composite.h"
#include "K2Node_MacroInstance.h"
#include "Kismet2/BlueprintEditorUtils.h"


#define LOCTEXT_NAMESPACE "K2Node"

UK2Node_Tunnel::UK2Node_Tunnel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsEditable = true;
}

void UK2Node_Tunnel::DestroyNode()
{
	if (InputSinkNode != NULL)
	{
		InputSinkNode->OutputSourceNode = NULL;
	}

	if (OutputSourceNode != NULL)
	{
		OutputSourceNode->InputSinkNode = NULL;
	}

	//@TODO: Should we remove the pins provided by this node from the twinned node(s)?
	Super::DestroyNode();
}

void UK2Node_Tunnel::PostPasteNode()
{
	Super::PostPasteNode();
	//@TODO: The gateway<->gateway node pairing should be unique, so we need to disallow this paste somehow (or just let it be an error later on)!
}

FText UK2Node_Tunnel::GetTooltipText() const
{
	if (bCanHaveInputs && !bCanHaveOutputs)
	{
		return NSLOCTEXT("K2Node", "OutputTunnelTooltip", "Outputs from this graph");
	}
	else if (!bCanHaveInputs && bCanHaveOutputs)
	{
		return NSLOCTEXT("K2Node", "InputTunnelTooltip", "Inputs into this graph");
	}
	else
	{
		return NSLOCTEXT("K2Node", "TunnelConnectionTooltip", "Tunnel Connection");
	}
}

FText UK2Node_Tunnel::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (bCanHaveInputs && !bCanHaveOutputs)
	{
		return NSLOCTEXT("K2Node", "OutputTunnelTitle", "Outputs");
	}
	else if (!bCanHaveInputs && bCanHaveOutputs)
	{
		return NSLOCTEXT("K2Node", "InputTunnelTitle", "Inputs");
	}
	else
	{
		return NSLOCTEXT("K2Node", "TunnelConnectionTitle", "Tunnel Connection");
	}
}

FString UK2Node_Tunnel::CreateUniquePinName(FString InSourcePinName) const
{
	if (GetClass() == UK2Node_Tunnel::StaticClass())
	{
		// When dealing with a tunnel node that is not a sub class (macro/collapsed graph entry and result), attempt to find the paired node and find a valid name between the two
		TWeakObjectPtr<UK2Node_EditablePinBase> TunnelEntry;
		TWeakObjectPtr<UK2Node_EditablePinBase> TunnelResult;
		FBlueprintEditorUtils::GetEntryAndResultNodes(GetGraph(), TunnelEntry, TunnelResult);

		if (TunnelEntry.IsValid() && TunnelResult.IsValid())
		{
			FString PinName(InSourcePinName);

			int32 Index = 1;
			while (TunnelEntry.Get()->FindPin(PinName) != nullptr || TunnelResult.Get()->FindPin(PinName) != nullptr)
			{
				++Index;
				PinName = InSourcePinName + FString::FromInt(Index);
			}

			return PinName;
		}
	}

	return Super::CreateUniquePinName(InSourcePinName);
}

bool UK2Node_Tunnel::CanUserDeleteNode() const
{
	// Disallow deletion of tunnels that are inside a tunnel graph, but allow it on top level tunnels that have gotten there on accident
	//@TODO: Remove this code 
	if (UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNode(this))
	{
		const bool bIsExactlyTunnel = (GetClass() == UK2Node_Tunnel::StaticClass());
		const bool bIsTopLevelGraph = (GetGraph()->GetOuter() == Blueprint);
		const bool bIsLibraryProject = (Blueprint->BlueprintType == BPTYPE_MacroLibrary);
		const bool bIsLocalMacro = Blueprint->MacroGraphs.Contains(GetGraph());
		if (bIsExactlyTunnel && bIsTopLevelGraph && !bIsLibraryProject && !bIsLocalMacro)
		{
			return true;
		}
	}

	return false;
}

bool UK2Node_Tunnel::CanDuplicateNode() const
{
	// Disallow duplication on tunnels, but not more derived classes (they can override if they also want to disallow)
	const bool bIsExactlyTunnel = (GetClass() == UK2Node_Tunnel::StaticClass());
	return !bIsExactlyTunnel;
}

bool UK2Node_Tunnel::IsNodeSafeToIgnore() const
{
	// If there are no connections to anything on this node, it's safe to ignore
	int32 NumConnections = 0;
	for (int32 BoundaryPinIndex = 0; BoundaryPinIndex < Pins.Num(); ++BoundaryPinIndex)
	{
		UEdGraphPin* BoundaryPin = Pins[BoundaryPinIndex];
		NumConnections += BoundaryPin->LinkedTo.Num();
	}

	return NumConnections == 0;
}

bool UK2Node_Tunnel::DrawNodeAsExit() const
{
	return(bCanHaveInputs && !bCanHaveOutputs);
}

bool UK2Node_Tunnel::DrawNodeAsEntry() const
{
	return(!bCanHaveInputs && bCanHaveOutputs);
}

UK2Node_Tunnel* UK2Node_Tunnel::GetInputSink() const
{
	return InputSinkNode;
}

UK2Node_Tunnel* UK2Node_Tunnel::GetOutputSource() const
{
	return OutputSourceNode;
}

bool UK2Node_Tunnel::CanCreateUserDefinedPin(const FEdGraphPinType& InPinType, EEdGraphPinDirection InDesiredDirection, FText& OutErrorMessage)
{
	bool bResult = true;
	// Make sure that if this is an exec node we are allowed one.
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if (InPinType.PinCategory == Schema->PC_Exec && !CanModifyExecutionWires())
	{
		OutErrorMessage = LOCTEXT("MultipleExecPinError", "Cannot support more exec pins!");
		bResult = false;
	}
	else if(InDesiredDirection == EGPD_Input && !bCanHaveInputs)
	{
		OutErrorMessage = LOCTEXT("AddTunnelInputPinError", "Cannot add input pins to entry node!");
		bResult = false;
	}
	else if(InDesiredDirection == EGPD_Output && !bCanHaveOutputs)
	{
		OutErrorMessage = LOCTEXT("AddTunnelOutputPinError", "Cannot add output pins to entry node!");
		bResult = false;
	}
	return bResult;
}

UEdGraphPin* UK2Node_Tunnel::CreatePinFromUserDefinition(const TSharedPtr<FUserPinInfo> NewPinInfo)
{
	// Create the new pin
	EEdGraphPinDirection Direction = bCanHaveInputs ? EGPD_Input : EGPD_Output;

	// Let the user pick the pin direction if legal
	if ( (bCanHaveInputs && NewPinInfo->DesiredPinDirection == EGPD_Input) || (bCanHaveOutputs && NewPinInfo->DesiredPinDirection == EGPD_Output) )
	{
		Direction = NewPinInfo->DesiredPinDirection;
	}

	const UEdGraphSchema_K2* const Schema = GetDefault<UEdGraphSchema_K2>();
	UEdGraphPin* Result = CreatePin(Direction, NewPinInfo->PinType, NewPinInfo->PinName);
	Schema->SetPinAutogeneratedDefaultValue(Result, NewPinInfo->PinDefaultValue);

	// Make sure it mirrors onto the associated node
	UEdGraphNode* TargetNode = ((InputSinkNode != NULL) ? InputSinkNode : OutputSourceNode);
	if (Cast<UK2Node_Composite>(TargetNode) != NULL || Cast<UK2Node_MacroInstance>(TargetNode) != NULL)
	{
		UEdGraphPin* HasPinAlready = TargetNode->FindPin(Result->PinName);
		if (HasPinAlready == NULL)
		{
			TargetNode->CreatePin(UEdGraphPin::GetComplementaryDirection(Direction), NewPinInfo->PinType, NewPinInfo->PinName);
		}
	}
	else if (UK2Node_Tunnel* TunnelNode = Cast<UK2Node_Tunnel>(TargetNode))
	{
		TunnelNode->CreateUserDefinedPin(NewPinInfo->PinName, NewPinInfo->PinType, (Direction == EGPD_Input)? EGPD_Output : EGPD_Input);
	}

	//@TODO: Automatically update loaded macro instances when this node is changed too

	return Result;
}

bool UK2Node_Tunnel::ModifyUserDefinedPinDefaultValue(TSharedPtr<FUserPinInfo> PinInfo, const FString& NewDefaultValue)
{
	if (Super::ModifyUserDefinedPinDefaultValue(PinInfo, NewDefaultValue))
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		K2Schema->HandleParameterDefaultValueChanged(this);

		return true;
	}
	return false;
}

bool UK2Node_Tunnel::CanModifyExecutionWires()
{
	return true;
}

ERenamePinResult UK2Node_Tunnel::RenameUserDefinedPin(const FString& OldName, const FString& NewName, bool bTest)
{
	const ERenamePinResult ThisNodeResult = Super::RenameUserDefinedPin(OldName, NewName, bTest);
	if(ERenamePinResult::ERenamePinResult_NameCollision == ThisNodeResult)
	{
		return ERenamePinResult::ERenamePinResult_NameCollision;
	}

	// And do the same on the twinned pin
	ERenamePinResult TargetNodeResult = ERenamePinResult::ERenamePinResult_Success;
	UEdGraphNode* TargetNode = ((InputSinkNode != NULL) ? InputSinkNode : OutputSourceNode);
	if (UK2Node_Composite* CompositeNode = Cast<UK2Node_Composite>(TargetNode))
	{
		TargetNodeResult = CompositeNode->RenameUserDefinedPin(OldName, NewName, bTest);
	}
	if(ERenamePinResult::ERenamePinResult_NameCollision == TargetNodeResult)
	{
		return ERenamePinResult::ERenamePinResult_NameCollision;
	}

	return FMath::Min<ERenamePinResult>(ThisNodeResult, TargetNodeResult);
}

UObject* UK2Node_Tunnel::GetJumpTargetForDoubleClick() const
{
	// Try to select the other side of a tunnel node
	UEdGraphNode* TargetNode = GetOutputSource();
	if (TargetNode == NULL)
	{
		TargetNode = GetInputSink();
	}

	return TargetNode;
}

void UK2Node_Tunnel::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	Super::ReallocatePinsDuringReconstruction(OldPins);

	const UEdGraphSchema_K2* const Schema = GetDefault<UEdGraphSchema_K2>();

	// determine if all wildcard pins are unlinked.
	// if they are, we should revert them all back to wildcard status
	bool bAllWildcardsAreUnlinked = true;
	for (auto PinIt = Pins.CreateConstIterator(); PinIt; PinIt++)
	{
		// for each of the wildcard pins...
		UEdGraphPin* const Pin = *PinIt;
		if (Pin->PinType.PinCategory == Schema->PC_Wildcard)
		{
			// find it in the old pins array (where it might not be a wildcard)
			// and see if it's unlinked
			for (auto OldPinIt = OldPins.CreateConstIterator(); OldPinIt; OldPinIt++)
			{
				UEdGraphPin const* const OldPin = *OldPinIt;
				if (OldPin->PinName == Pin->PinName)
				{
					if (OldPin->LinkedTo.Num() > 0)
					{
						bAllWildcardsAreUnlinked = false;
						break;
					}
				}
			}
		}
	}

	if (bAllWildcardsAreUnlinked == false)
	{
		// Copy pin types from old pins for wildcard pins
		for (auto PinIt = Pins.CreateConstIterator(); PinIt; PinIt++)
		{
			UEdGraphPin* const Pin = *PinIt;
			if (Pin->PinType.PinCategory == Schema->PC_Wildcard)
			{
				// find it in the old pins and copy the type
				for (auto OldPinIt = OldPins.CreateConstIterator(); OldPinIt; OldPinIt++)
				{
					UEdGraphPin const* const OldPin = *OldPinIt;
					if (OldPin->PinName == Pin->PinName)
					{
						Pin->PinType = OldPin->PinType;
					}
				}
			}
		}
	}
	PostFixupAllWildcardPins(bAllWildcardsAreUnlinked);
}
#undef LOCTEXT_NAMESPACE
