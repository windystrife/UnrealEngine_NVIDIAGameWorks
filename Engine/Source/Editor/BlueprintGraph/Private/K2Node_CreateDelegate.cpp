// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "K2Node_CreateDelegate.h"
#include "Engine/Blueprint.h"
#include "Engine/MemberReference.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_BaseMCDelegate.h"
#include "Kismet2/BlueprintEditorUtils.h"

#include "Kismet2/CompilerResultsLog.h"
#include "DelegateNodeHandlers.h"
#include "BlueprintNodeSpawner.h"
#include "EditorCategoryUtils.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "FindInBlueprintManager.h"

struct FK2Node_CreateDelegate_Helper
{
	static FString DelegateOutputName;
	static FString InputObjectName; // Deprecated, for fixup
};
FString FK2Node_CreateDelegate_Helper::DelegateOutputName(TEXT("OutputDelegate"));
FString FK2Node_CreateDelegate_Helper::InputObjectName(TEXT("InputObject"));

UK2Node_CreateDelegate::UK2Node_CreateDelegate(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_CreateDelegate::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	if (UEdGraphPin* ObjPin = CreatePin(EGPD_Input, K2Schema->PC_Object, FString(), UObject::StaticClass(), K2Schema->PN_Self))
	{
		ObjPin->PinFriendlyName = NSLOCTEXT("K2Node", "CreateDelegate_ObjectInputName", "Object");
	}

	if(UEdGraphPin* DelegatePin = CreatePin(EGPD_Output, K2Schema->PC_Delegate, FString(), nullptr, FK2Node_CreateDelegate_Helper::DelegateOutputName))
	{
		DelegatePin->PinFriendlyName = NSLOCTEXT("K2Node", "CreateDelegate_DelegateOutName", "Event");
	}

	Super::AllocateDefaultPins();
}

UK2Node::ERedirectType UK2Node_CreateDelegate::DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Handles remap of InputObject to Self, from 4.10 time frame
	if (OldPin->PinName == FK2Node_CreateDelegate_Helper::InputObjectName && NewPin->PinName == K2Schema->PN_Self)
	{
		return ERedirectType_Name;
	}

	return Super::DoPinsMatchForReconstruction(NewPin, NewPinIndex, OldPin, OldPinIndex);
}

bool UK2Node_CreateDelegate::IsValid(FString* OutMsg, bool bDontUseSkeletalClassForSelf) const
{
	FName FunctionName = GetFunctionName();
	if (FunctionName == NAME_None)
	{
		if (OutMsg)
		{
			*OutMsg = NSLOCTEXT("K2Node", "No_function_name", "No function/event specified.").ToString();
		}
		return false;
	}

	const UEdGraphPin* DelegatePin = GetDelegateOutPin();
	if(!DelegatePin)
	{
		if (OutMsg)
		{
			*OutMsg = NSLOCTEXT("K2Node", "No_delegate_out_pin", "Malformed node - there's no delegate output pin.").ToString();
		}
		return false;
	}

	const UFunction* Signature = GetDelegateSignature();
	if(!Signature)
	{
		if (OutMsg)
		{
			*OutMsg = NSLOCTEXT("K2Node", "Signature_not_found", "Unable to determine expected signature - is the delegate pin connected?").ToString();
		}
		return false;
	}

	for(int PinIter = 1; PinIter < DelegatePin->LinkedTo.Num(); PinIter++)
	{
		const UEdGraphPin* OtherPin = DelegatePin->LinkedTo[PinIter];
		const UFunction* OtherSignature = OtherPin ? 
			FMemberReference::ResolveSimpleMemberReference<UFunction>(OtherPin->PinType.PinSubCategoryMemberReference) : NULL;
		if(!OtherSignature || !Signature->IsSignatureCompatibleWith(OtherSignature))
		{
			if (OutMsg)
			{
				if (const UK2Node_BaseMCDelegate* DelegateNode = OtherPin ? Cast<const UK2Node_BaseMCDelegate>(OtherPin->GetOwningNode()) : nullptr)
				{
					const FString DelegateName = DelegateNode->GetPropertyName().ToString();

					*OutMsg = FString::Printf(*NSLOCTEXT("K2Node", "Bad_delegate_connection_named", "A connected delegate (%s) has an incompatible signature - has that delegate changed?").ToString(),
						*DelegateName);
				}
				else
				{
					*OutMsg = NSLOCTEXT("K2Node", "Bad_delegate_connection", "A connected delegate's signature is incompatible - has that delegate changed?").ToString();
				}
			}
			return false;
		}
	}

	UClass* ScopeClass = GetScopeClass(bDontUseSkeletalClassForSelf);
	if(!ScopeClass)
	{
		if (OutMsg)
		{
			FString SelfPinName = UEdGraphSchema_K2::PN_Self;
			if (UEdGraphPin* SelfPin = GetObjectInPin())
			{
				SelfPinName = SelfPin->PinFriendlyName.IsEmpty() ? SelfPin->PinFriendlyName.ToString() : SelfPin->PinName;
			}

			*OutMsg = FString::Printf(*NSLOCTEXT("K2Node", "Class_not_found", "Unable to determine context for the selected function/event: '%s' - make sure the target '%s' pin is properly set up.").ToString(),
				*FunctionName.ToString(), *SelfPinName);
		}
		return false;
	}

	FMemberReference MemberReference;
	MemberReference.SetDirect(SelectedFunctionName, SelectedFunctionGuid, ScopeClass, false);
	const UFunction* FoundFunction = MemberReference.ResolveMember<UFunction>((UClass*) NULL);
	if (!FoundFunction)
	{
		if (OutMsg)
		{
			*OutMsg = FString::Printf(*NSLOCTEXT("K2Node", "Function_not_found", "Unable to find the selected function/event: '%s' - has it been deleted?").ToString(),
				*FunctionName.ToString());

		}
		return false;
	}
	if (!Signature->IsSignatureCompatibleWith(FoundFunction))
	{
		if (OutMsg)
		{
			*OutMsg = FString::Printf(*NSLOCTEXT("K2Node", "Function_not_compatible", "The function/event '%s' does not match the necessary signature - has the delegate or function/event changed?").ToString(),
				*FunctionName.ToString());
		}
		return false;
	}
	if (!UEdGraphSchema_K2::FunctionCanBeUsedInDelegate(FoundFunction))
	{
		if (OutMsg)
		{
			*OutMsg = FString::Printf(*NSLOCTEXT("K2Node", "Function_cannot_be_used_in_delegate", "The selected function/event is not bindable - is the function/event pure or latent?").ToString(),
				*FunctionName.ToString());
		}
		return false;
	}

	if(!FoundFunction->HasAllFunctionFlags(FUNC_BlueprintAuthorityOnly))
	{
		for(int PinIter = 0; PinIter < DelegatePin->LinkedTo.Num(); PinIter++)
		{
			const UEdGraphPin* OtherPin = DelegatePin->LinkedTo[PinIter];
			const UK2Node_BaseMCDelegate* Node = OtherPin ? Cast<const UK2Node_BaseMCDelegate>(OtherPin->GetOwningNode()) : NULL;
			if (Node && Node->IsAuthorityOnly())
			{
				if(OutMsg)
				{
					*OutMsg = FString::Printf(*NSLOCTEXT("K2Node", "WrongDelegateAuthorityOnly", "The selected function/event ('%s') is not compatible with this delegate (the delegate is server-only) - try marking the function/event AuthorityOnly.").ToString(),
						*FunctionName.ToString());
				}
				return false;
			}
		}
	}

	return true;
}

void UK2Node_CreateDelegate::ValidationAfterFunctionsAreCreated(class FCompilerResultsLog& MessageLog, bool bFullCompile) const
{
	FString Msg;
	if(!IsValid(&Msg, bFullCompile))
	{
 		MessageLog.Error(*FString::Printf( TEXT("@@ %s %s"), *NSLOCTEXT("K2Node", "WrongDelegate", "Signature Error:").ToString(), *Msg), this);
	}
}

void UK2Node_CreateDelegate::HandleAnyChangeWithoutNotifying()
{
	const auto Blueprint = HasValidBlueprint() ? GetBlueprint() : nullptr;
	const auto SelfScopeClass = Blueprint ? Blueprint->SkeletonGeneratedClass : nullptr;
	const auto ParentClass = GetScopeClass();
	const bool bIsSelfScope = SelfScopeClass && ParentClass && ((SelfScopeClass->IsChildOf(ParentClass)) || (SelfScopeClass->ClassGeneratedBy == ParentClass->ClassGeneratedBy));

	FMemberReference FunctionReference;
	FunctionReference.SetDirect(SelectedFunctionName, SelectedFunctionGuid, GetScopeClass(), bIsSelfScope);

	if (FunctionReference.ResolveMember<UFunction>(SelfScopeClass))
	{
		SelectedFunctionName = FunctionReference.GetMemberName();
		SelectedFunctionGuid = FunctionReference.GetMemberGuid();

		if (!SelectedFunctionGuid.IsValid())
		{
			UBlueprint::GetGuidFromClassByFieldName<UFunction>(ParentClass, SelectedFunctionName, SelectedFunctionGuid);
		}
	}

	if(!IsValid())
	{
		// do not clear the name, so we can keep it around as a hint/guide for 
		// users (so they can better determine what went wrong)
		if (const UEdGraphPin* DelegatePin = GetDelegateOutPin())
		{
			if (DelegatePin->LinkedTo.Num() == 0)
			{
				// ok to clear if they've disconnected the delegate pin
				SelectedFunctionName = NAME_None;
			}
		}
		SelectedFunctionGuid.Invalidate();
	}
}

void UK2Node_CreateDelegate::HandleAnyChange(UEdGraph* & OutGraph, UBlueprint* & OutBlueprint)
{
	const FName OldSelectedFunctionName = GetFunctionName();
	HandleAnyChangeWithoutNotifying();
	if (OldSelectedFunctionName != GetFunctionName())
	{
		OutGraph = GetGraph();
		OutBlueprint = GetBlueprint();
	}
	else
	{
		OutGraph = nullptr;
		OutBlueprint = nullptr;
	}
}

void UK2Node_CreateDelegate::HandleAnyChange(bool bForceModify)
{
	const FName OldSelectedFunctionName = GetFunctionName();
	HandleAnyChangeWithoutNotifying();
	if (bForceModify || (OldSelectedFunctionName != GetFunctionName()))
	{
		if(UEdGraph* Graph = GetGraph())
		{
			Graph->NotifyGraphChanged();
		}

		UBlueprint* Blueprint = GetBlueprint();
		if(Blueprint && !Blueprint->bBeingCompiled)
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
			Blueprint->BroadcastChanged();
		}
	}
	else if (GetFunctionName() == NAME_None)
	{
		if(UEdGraph* Graph = GetGraph())
		{
			Graph->NotifyGraphChanged();
		}
	}
}

void UK2Node_CreateDelegate::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);
	UBlueprint* Blueprint = GetBlueprint();
	if(Blueprint && !Blueprint->bBeingCompiled)
	{
		HandleAnyChange();
	}
	else
	{
		HandleAnyChangeWithoutNotifying();
	}
}

void UK2Node_CreateDelegate::PinTypeChanged(UEdGraphPin* Pin)
{
	Super::PinTypeChanged(Pin);

	HandleAnyChangeWithoutNotifying();
}

void UK2Node_CreateDelegate::NodeConnectionListChanged()
{
	Super::NodeConnectionListChanged();
	UBlueprint* Blueprint = GetBlueprint();
	if(Blueprint && !Blueprint->bBeingCompiled)
	{
		HandleAnyChange();
	}
	else
	{
		HandleAnyChangeWithoutNotifying();
	}
}

void UK2Node_CreateDelegate::PostReconstructNode()
{
	Super::PostReconstructNode();
	HandleAnyChange();
}

UFunction* UK2Node_CreateDelegate::GetDelegateSignature() const
{
	UEdGraphPin* Pin = GetDelegateOutPin();
	check(Pin);
	if(Pin->LinkedTo.Num())
	{
		if(UEdGraphPin* ResultPin = Pin->LinkedTo[0])
		{
			if (UEdGraphSchema_K2::PC_Delegate == ResultPin->PinType.PinCategory)
			{
				return FMemberReference::ResolveSimpleMemberReference<UFunction>(ResultPin->PinType.PinSubCategoryMemberReference);
			}
		}
	}
	return nullptr;
}

UClass* UK2Node_CreateDelegate::GetScopeClass(bool bDontUseSkeletalClassForSelf/* = false*/) const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	UEdGraphPin* Pin = FindPin(K2Schema->PN_Self);
	if (Pin == nullptr)
	{
		// The BlueprintNodeTemplateCache creates nodes but doesn't call allocate default pins.
		// SMyBlueprint::OnDeleteGraph calls this function on every UK2Node_CreateDelegate. Each of 
		// these systems is violating some first principles, so I've settled on this null check.
		return nullptr;
	}
	check(Pin->LinkedTo.Num() <= 1);
	bool bUseSelf = false;
	if(Pin->LinkedTo.Num() == 0)
	{
		bUseSelf = true;
	}
	else
	{
		if(UEdGraphPin* ResultPin = Pin->LinkedTo[0])
		{
			ensure(K2Schema->PC_Object == ResultPin->PinType.PinCategory);
			if (K2Schema->PN_Self == ResultPin->PinType.PinSubCategory)
			{
				bUseSelf = true;
			}
			if(UClass* TrueScopeClass = Cast<UClass>(ResultPin->PinType.PinSubCategoryObject.Get()))
			{
				if(UBlueprint* ScopeClassBlueprint = Cast<UBlueprint>(TrueScopeClass->ClassGeneratedBy))
				{
					if(ScopeClassBlueprint->SkeletonGeneratedClass)
					{
						return ScopeClassBlueprint->SkeletonGeneratedClass;
					}
				}
				return TrueScopeClass;
			}
		}
	}

	if (bUseSelf && HasValidBlueprint())
	{
		if (UBlueprint* ScopeClassBlueprint = GetBlueprint())
		{
			return bDontUseSkeletalClassForSelf ? ScopeClassBlueprint->GeneratedClass : ScopeClassBlueprint->SkeletonGeneratedClass;
		}
	}

	return nullptr;
}

FName UK2Node_CreateDelegate::GetFunctionName() const
{
	return SelectedFunctionName;
}

UEdGraphPin* UK2Node_CreateDelegate::GetDelegateOutPin() const
{
	return FindPin(FK2Node_CreateDelegate_Helper::DelegateOutputName);
}

UEdGraphPin* UK2Node_CreateDelegate::GetObjectInPin() const
{
	return FindPin(UEdGraphSchema_K2::PN_Self);
}

FText UK2Node_CreateDelegate::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NSLOCTEXT("K2Node", "CreateDelegate", "Create Event");
}

UObject* UK2Node_CreateDelegate::GetJumpTargetForDoubleClick() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	UClass* ScopeClass = GetScopeClass();

	if (UBlueprint* ScopeClassBlueprint = (ScopeClass != nullptr) ? Cast<UBlueprint>(ScopeClass->ClassGeneratedBy) : nullptr)
	{
		if (UEdGraph* FoundGraph = FindObject<UEdGraph>(ScopeClassBlueprint, *GetFunctionName().ToString()))
		{
			if (!FBlueprintEditorUtils::IsGraphIntermediate(FoundGraph))
			{
				return FoundGraph;
			}
		}
		for (auto UbergraphIt = ScopeClassBlueprint->UbergraphPages.CreateIterator(); UbergraphIt; ++UbergraphIt)
		{
			UEdGraph* Graph = (*UbergraphIt);
			if (!FBlueprintEditorUtils::IsGraphIntermediate(Graph))
			{
				TArray<UK2Node_Event*> EventNodes;
				Graph->GetNodesOfClass(EventNodes);
				for (UK2Node_Event* EventNode : EventNodes)
				{
					if (GetFunctionName() == EventNode->GetFunctionName())
					{
						return EventNode;
					}
				}
			}
		}
	}

	return GetDelegateSignature();
}

void UK2Node_CreateDelegate::AddSearchMetaDataInfo(TArray<struct FSearchTagDataPair>& OutTaggedMetaData) const
{
	Super::AddSearchMetaDataInfo(OutTaggedMetaData);

	const FName FunctionName = GetFunctionName();
	if (!FunctionName.IsNone())
	{
		OutTaggedMetaData.Add(FSearchTagDataPair(FFindInBlueprintSearchTags::FiB_NativeName, FText::FromName(FunctionName)));
	}
}

FNodeHandlingFunctor* UK2Node_CreateDelegate::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_CreateDelegate(CompilerContext);
}

void UK2Node_CreateDelegate::SetFunction(FName Name)
{
	SelectedFunctionName = Name;
	SelectedFunctionGuid.Invalidate();
}

FText UK2Node_CreateDelegate::GetMenuCategory() const
{
	return FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Delegates);
}

void UK2Node_CreateDelegate::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* NodeClass = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(NodeClass))
	{
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(NodeClass);
		check(NodeSpawner);
		ActionRegistrar.AddBlueprintAction(NodeClass, NodeSpawner);
	}
}
