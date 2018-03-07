// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "K2Node_CustomEvent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_BaseMCDelegate.h"
#include "Kismet2/BlueprintEditorUtils.h"

#include "Kismet2/Kismet2NameValidators.h"
#include "Kismet2/CompilerResultsLog.h"
#include "BlueprintNodeSpawner.h"
#include "BlueprintEventNodeSpawner.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "FindInBlueprintManager.h"

#define LOCTEXT_NAMESPACE "K2Node_CustomEvent"

#define SNAP_GRID (16) // @todo ensure this is the same as SNodePanel::GetSnapGridSize()

/**
 * Attempts to find a CustomEvent node associated with the specified function.
 * 
 * @param  CustomEventFunc	The function you want to find an associated node for.
 * @return A pointer to the found node (NULL if a corresponding node wasn't found)
 */
static UK2Node_CustomEvent const* FindCustomEventNodeFromFunction(UFunction* CustomEventFunc)
{
	UK2Node_CustomEvent const* FoundEventNode = NULL;
	if (CustomEventFunc != NULL)
	{
		UObject const* const FuncOwner = CustomEventFunc->GetOuter();
		check(FuncOwner != NULL);

		// if the found function is a NOT a native function (it's user generated)
		if (FuncOwner->IsA(UBlueprintGeneratedClass::StaticClass()))
		{
			UBlueprintGeneratedClass* FuncClass = Cast<UBlueprintGeneratedClass>(CustomEventFunc->GetOuter());
			check(FuncClass != NULL);
			UBlueprint* FuncBlueprint = Cast<UBlueprint>(FuncClass->ClassGeneratedBy);
			check(FuncBlueprint != NULL);

			TArray<UK2Node_CustomEvent*> BpCustomEvents;
			FBlueprintEditorUtils::GetAllNodesOfClass<UK2Node_CustomEvent>(FuncBlueprint, BpCustomEvents);

			// look to see if the function that this is overriding is a custom-event
			for (UK2Node_CustomEvent const* const UserEvent : BpCustomEvents)
			{
				if (UserEvent->CustomFunctionName == CustomEventFunc->GetFName())
				{
					FoundEventNode = UserEvent;
					break;
				}
			}
		}
	}

	return FoundEventNode;
}

/**
 * Custom handler for validating CustomEvent renames
 */
class FCustomEventNameValidator : public FKismetNameValidator
{
public:
	FCustomEventNameValidator(UK2Node_CustomEvent const* CustomEventIn)
		: FKismetNameValidator(CustomEventIn->GetBlueprint(), CustomEventIn->CustomFunctionName)
		, CustomEvent(CustomEventIn)
	{
		check(CustomEvent != NULL);
	}

	// Begin INameValidatorInterface
	virtual EValidatorResult IsValid(FString const& Name, bool bOriginal = false) override
	{
		EValidatorResult NameValidity = FKismetNameValidator::IsValid(Name, bOriginal);
		if ((NameValidity == EValidatorResult::Ok) || (NameValidity == EValidatorResult::ExistingName))
		{
			UBlueprint* Blueprint = CustomEvent->GetBlueprint();
			check(Blueprint != NULL);

			UFunction* ParentFunction = FindField<UFunction>(Blueprint->ParentClass, *Name);
			// if this custom-event is overriding a function belonging to the blueprint's parent
			if (ParentFunction != NULL)
			{
				UK2Node_CustomEvent const* OverriddenEvent = FindCustomEventNodeFromFunction(ParentFunction);
				// if the function that we're overriding isn't another custom event,
				// then we can't name it this (only allow custom-event to override other custom-events)
				if (OverriddenEvent == NULL)
				{
					NameValidity = EValidatorResult::AlreadyInUse;
				}		
			}
		}
		return NameValidity;
	}
	// End INameValidatorInterface

private:
	UK2Node_CustomEvent const* CustomEvent;
};

UK2Node_CustomEvent::UK2Node_CustomEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bOverrideFunction = false;
	bIsEditable = true;
	bCanRenameNode = true;
	bCallInEditor = false;
}

void UK2Node_CustomEvent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		CachedNodeTitle.MarkDirty();

		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

		// Ensure that array type inputs and non-array type pass-by-reference inputs are also marked 'const' - since arrays
		// are implicitly passed by reference, and since events do not have return values/outputs, this equates to marking
		// the parameter as 'const Type&' in native code. Also note that since UHT already blocks non-const reference types
		// from being compiled into a MC delegate signature, any existing custom event param pins that were implicitly
		// created via "Assign" in the Blueprint editor's context menu will previously have had 'const' set for its pin type.
		//
		// This ensures that (a) we don't emit the "no reference will be returned" note on custom event nodes with array
		// inputs added by the user via the Details panel, and (b) we don't emit the "no reference will be returned" warning
		// on custom event nodes with struct/object inputs added by the user in the Details tab and also set to pass-by-reference.
		//
		// Blueprint details customization now sets 'bIsConst' in new custom event node placements - see OnRefCheckStateChanged().
		for (auto Pin : Pins)
		{
			if (Pin)
			{
				if (Pin->Direction == EGPD_Output
					&& !Pin->PinType.bIsConst
					&& !K2Schema->IsExecPin(*Pin)
					&& !K2Schema->IsDelegateCategory(Pin->PinType.PinCategory))
				{
					for (TSharedPtr<FUserPinInfo>& PinInfo : UserDefinedPins)
					{
						if (PinInfo->PinName == Pin->PinName)
						{
							Pin->PinType.bIsConst = PinInfo->PinType.bIsConst = (PinInfo->PinType.IsContainer() || PinInfo->PinType.bIsReference);
							break;
						}
					}
				}
			}
		}
	}
}

FText UK2Node_CustomEvent::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType != ENodeTitleType::FullTitle)
	{
		return FText::FromName(CustomFunctionName);
	}
	else if (CachedNodeTitle.IsOutOfDate(this))
	{
		FText RPCString = UK2Node_Event::GetLocalizedNetString(FunctionFlags, false);
		
		FFormatNamedArguments Args;
		Args.Add(TEXT("FunctionName"), FText::FromName(CustomFunctionName));
		Args.Add(TEXT("RPCString"), RPCString);

		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitle.SetCachedText(FText::Format(NSLOCTEXT("K2Node", "CustomEvent_Name", "{FunctionName}{RPCString}\nCustom Event"), Args), this);
	}

	return CachedNodeTitle;
}

bool UK2Node_CustomEvent::CanCreateUserDefinedPin(const FEdGraphPinType& InPinType, EEdGraphPinDirection InDesiredDirection, FText& OutErrorMessage)
{
	if (!IsEditable())
	{
		return false;
	}

	// Make sure that if this is an exec node we are allowed one.
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
	if(InDesiredDirection == EGPD_Input)
	{
		OutErrorMessage = NSLOCTEXT("K2Node", "AddInputPinError", "Cannot add input pins to custom event node!");
		return false;
	}
	else if (InPinType.PinCategory == Schema->PC_Exec && !CanModifyExecutionWires())
	{
		OutErrorMessage = LOCTEXT("MultipleExecPinError", "Cannot support more exec pins!");
		return false;
	}
	else
	{
		TArray<TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>> TypeTree;
		Schema->GetVariableTypeTree(TypeTree, ETypeTreeFilter::RootTypesOnly);

		bool bIsValid = false;
		for (TSharedPtr<UEdGraphSchema_K2::FPinTypeTreeInfo>& TypeInfo : TypeTree)
		{
			FEdGraphPinType CurrentType = TypeInfo->GetPinType(false);
			// only concerned with the list of categories
			if (CurrentType.PinCategory == InPinType.PinCategory)
			{
				bIsValid = true;
				break;
			}
		}

		if (!bIsValid)
		{
			OutErrorMessage = LOCTEXT("AddInputPinError", "Cannot add pins of this type to custom event node!");
			return false;
		}
	}

	return true;
}

UEdGraphPin* UK2Node_CustomEvent::CreatePinFromUserDefinition(const TSharedPtr<FUserPinInfo> NewPinInfo)
{
	UEdGraphPin* NewPin = CreatePin(EGPD_Output, NewPinInfo->PinType, NewPinInfo->PinName);
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	K2Schema->SetPinAutogeneratedDefaultValue(NewPin, NewPinInfo->PinDefaultValue);
	return NewPin;
}

bool UK2Node_CustomEvent::ModifyUserDefinedPinDefaultValue(TSharedPtr<FUserPinInfo> PinInfo, const FString& NewDefaultValue)
{
	if (Super::ModifyUserDefinedPinDefaultValue(PinInfo, NewDefaultValue))
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		K2Schema->HandleParameterDefaultValueChanged(this);

		return true;
	}
	return false;
}

void UK2Node_CustomEvent::RenameCustomEventCloseToName(int32 StartIndex)
{
	bool bFoundName = false;
	const FString& BaseName = CustomFunctionName.ToString();

	for (int32 NameIndex = StartIndex; !bFoundName; ++NameIndex)
	{
		const FString NewName = FString::Printf(TEXT("%s_%d"), *BaseName, NameIndex);
		if (Rename(*NewName, GetOuter(), REN_Test))
		{
			UBlueprint* Blueprint = GetBlueprint();
			CustomFunctionName = FName(NewName.GetCharArray().GetData());
			Rename(*NewName, GetOuter(), (Blueprint->bIsRegeneratingOnLoad ? REN_ForceNoResetLoaders : 0) | REN_DontCreateRedirectors);
			bFoundName = true;
		}
	}
}

void UK2Node_CustomEvent::OnRenameNode(const FString& NewName)
{
	CustomFunctionName = *NewName;
	CachedNodeTitle.MarkDirty();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprint());
}

TSharedPtr<class INameValidatorInterface> UK2Node_CustomEvent::MakeNameValidator() const
{
	return MakeShareable(new FCustomEventNameValidator(this));
}

bool UK2Node_CustomEvent::IsOverride() const
{
	UBlueprint* Blueprint = GetBlueprint();
	check(Blueprint != NULL);

	UFunction* ParentFunction = FindField<UFunction>(Blueprint->ParentClass, CustomFunctionName);
	UK2Node_CustomEvent const* OverriddenEvent = FindCustomEventNodeFromFunction(ParentFunction);

	return (OverriddenEvent != NULL);
}

uint32 UK2Node_CustomEvent::GetNetFlags() const
{
	uint32 NetFlags = (FunctionFlags & FUNC_NetFuncFlags);
	if (IsOverride())
	{
		UBlueprint* Blueprint = GetBlueprint();
		check(Blueprint != NULL);

		UFunction* ParentFunction = FindField<UFunction>(Blueprint->ParentClass, CustomFunctionName);
		check(ParentFunction != NULL);

		// inherited net flags take precedence 
		NetFlags = (ParentFunction->FunctionFlags & FUNC_NetFuncFlags);
	}

	// Sanitize NetFlags, only allow replication flags that can be supported by the online system
	// This mirrors logic in ProcessFunctionSpecifiers in HeaderParser.cpp. Basically if we want to 
	// replicate a function we need to know whether we're replicating on the client or the server.
	if (!(NetFlags & FUNC_Net))
	{
		NetFlags = 0;
	}

	return NetFlags;
}

void UK2Node_CustomEvent::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	UBlueprint* Blueprint = GetBlueprint();
	check(Blueprint != NULL);

	UFunction* ParentFunction = FindField<UFunction>(Blueprint->ParentClass, CustomFunctionName);
	// if this custom-event is overriding a function belonging to the blueprint's parent
	if (ParentFunction != NULL)
	{
		UObject const* const FuncOwner = ParentFunction->GetOuter();
		check(FuncOwner != NULL);

		// if this custom-event is attempting to override a native function, we can't allow that
		if (!FuncOwner->IsA(UBlueprintGeneratedClass::StaticClass()))
		{
			MessageLog.Error(*FString::Printf(*LOCTEXT("NativeFunctionConflict", "@@ name conflicts with a native '%s' function").ToString(), *FuncOwner->GetName()), this);
		}
		else 
		{
			UK2Node_CustomEvent const* OverriddenEvent = FindCustomEventNodeFromFunction(ParentFunction);
			// if the function that this is attempting to override is NOT another 
			// custom-event, then we want to error (a custom-event shouldn't override something different)
			if (OverriddenEvent == NULL)
			{
				MessageLog.Error(*FString::Printf(*LOCTEXT("NonCustomEventOverride", "@@ name conflicts with a '%s' function").ToString(), *FuncOwner->GetName()), this);
			}
			// else, we assume the user was attempting to override the parent's custom-event
			// the signatures could still be off, but FKismetCompilerContext::PrecompileFunction() should catch that
		}		
	}
}

void UK2Node_CustomEvent::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
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
		UBlueprintNodeSpawner* NodeSpawner = UBlueprintEventNodeSpawner::Create(GetClass(), FName());
		check(NodeSpawner != nullptr);

		auto SetupCustomEventNodeLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode)
		{
			UK2Node_CustomEvent* EventNode = CastChecked<UK2Node_CustomEvent>(NewNode);
			UBlueprint* Blueprint = EventNode->GetBlueprint();

			// in GetNodeTitle(), we use an empty CustomFunctionName to identify a menu entry
			if (!bIsTemplateNode)
			{
				EventNode->CustomFunctionName = FBlueprintEditorUtils::FindUniqueCustomEventName(Blueprint);
			}
			EventNode->bIsEditable = true;
		};

		NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(SetupCustomEventNodeLambda);
		ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
	}
}

void UK2Node_CustomEvent::ReconstructNode()
{
	CachedNodeTitle.MarkDirty();

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	const UEdGraphPin* DelegateOutPin = FindPin(DelegateOutputName);
	const UEdGraphPin* LinkedPin = ( DelegateOutPin && DelegateOutPin->LinkedTo.Num() && DelegateOutPin->LinkedTo[0] ) ? FBlueprintEditorUtils::FindFirstCompilerRelevantLinkedPin(DelegateOutPin->LinkedTo[0]) : nullptr;

	const UFunction* DelegateSignature = nullptr;

	if ( LinkedPin )
	{
		if ( const UK2Node_BaseMCDelegate* OtherNode = Cast<const UK2Node_BaseMCDelegate>(LinkedPin->GetOwningNode()) )
		{
			DelegateSignature = OtherNode->GetDelegateSignature();
		}
		else if ( LinkedPin->PinType.PinCategory == K2Schema->PC_Delegate )
		{
			DelegateSignature = FMemberReference::ResolveSimpleMemberReference<UFunction>(LinkedPin->PinType.PinSubCategoryMemberReference);
		}
	}
	
	const bool bUseDelegateSignature = (NULL == FindEventSignatureFunction()) && DelegateSignature;

	if (bUseDelegateSignature)
	{
		UserDefinedPins.Empty();
		for (TFieldIterator<UProperty> PropIt(DelegateSignature); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			const UProperty* Param = *PropIt;
			if (!Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm))
			{
				FEdGraphPinType PinType;
				K2Schema->ConvertPropertyToPinType(Param, /*out*/ PinType);

				FString NewPinName = Param->GetName();
				int32 Index = 1;
				while ((DelegateOutputName == NewPinName) || (K2Schema->PN_Then == NewPinName))
				{
					++Index;
					NewPinName += FString::FromInt(Index);
				}
				TSharedPtr<FUserPinInfo> NewPinInfo = MakeShareable( new FUserPinInfo() );
				NewPinInfo->PinName = NewPinName;
				NewPinInfo->PinType = PinType;
				NewPinInfo->DesiredPinDirection = EGPD_Output;
				UserDefinedPins.Add(NewPinInfo);
			}
		}
	}

	Super::ReconstructNode();
}

UK2Node_CustomEvent* UK2Node_CustomEvent::CreateFromFunction(FVector2D GraphPosition, UEdGraph* ParentGraph, const FString& Name, const UFunction* Function, bool bSelectNewNode/* = true*/)
{
	UK2Node_CustomEvent* CustomEventNode = NULL;
	if(ParentGraph && Function)
	{
		CustomEventNode = NewObject<UK2Node_CustomEvent>(ParentGraph);
		CustomEventNode->CustomFunctionName = FName(*Name);
		CustomEventNode->SetFlags(RF_Transactional);
		ParentGraph->Modify();
		ParentGraph->AddNode(CustomEventNode, true, bSelectNewNode);
		CustomEventNode->CreateNewGuid();
		CustomEventNode->PostPlacedNewNode();
		CustomEventNode->AllocateDefaultPins();

		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		for (TFieldIterator<UProperty> PropIt(Function); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			const UProperty* Param = *PropIt;
			if (!Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm))
			{
				FEdGraphPinType PinType;
				K2Schema->ConvertPropertyToPinType(Param, /*out*/ PinType);
				CustomEventNode->CreateUserDefinedPin(Param->GetName(), PinType, EGPD_Output);
			}
		}

		CustomEventNode->NodePosX = GraphPosition.X;
		CustomEventNode->NodePosY = GraphPosition.Y;
		CustomEventNode->SnapToGrid(SNAP_GRID);
	}

	return CustomEventNode;
}

bool UK2Node_CustomEvent::IsEditable() const
{
	const UEdGraphPin* DelegateOutPin = FindPin(DelegateOutputName);
	if(DelegateOutPin && DelegateOutPin->LinkedTo.Num())
	{
		return false;
	}
	return Super::IsEditable();
}

bool UK2Node_CustomEvent::IsUsedByAuthorityOnlyDelegate() const
{
	if(const UEdGraphPin* DelegateOutPin = FindPin(DelegateOutputName))
	{
		for(auto PinIter = DelegateOutPin->LinkedTo.CreateConstIterator(); PinIter; ++PinIter)
		{
			const UEdGraphPin* LinkedPin = *PinIter;
			const UK2Node_BaseMCDelegate* Node = LinkedPin ? Cast<const UK2Node_BaseMCDelegate>(LinkedPin->GetOwningNode()) : NULL;
			if(Node && Node->IsAuthorityOnly())
			{
				return true;
			}
		}
	}

	return false;
}

FText UK2Node_CustomEvent::GetTooltipText() const
{
	return LOCTEXT("AddCustomEvent_Tooltip", "An event with customizable name and parameters.");
}

FString UK2Node_CustomEvent::GetDocumentationLink() const
{
	// Use the main k2 node doc
	return UK2Node::GetDocumentationLink();
}

FString UK2Node_CustomEvent::GetDocumentationExcerptName() const
{
	return TEXT("UK2Node_CustomEvent");
}

FSlateIcon UK2Node_CustomEvent::GetIconAndTint(FLinearColor& OutColor) const
{
	return FSlateIcon("EditorStyle", bCallInEditor ? "GraphEditor.CallInEditorEvent_16x" : "GraphEditor.CustomEvent_16x");
}

void UK2Node_CustomEvent::AutowireNewNode(UEdGraphPin* FromPin)
{
	Super::AutowireNewNode(FromPin);

	if (auto DelegateOutPin = FindPin(DelegateOutputName))
	{
		if (DelegateOutPin->LinkedTo.Num())
		{
			ReconstructNode();
		}
	}
}

void UK2Node_CustomEvent::AddSearchMetaDataInfo(TArray<struct FSearchTagDataPair>& OutTaggedMetaData) const
{
	Super::AddSearchMetaDataInfo(OutTaggedMetaData);

	for (FSearchTagDataPair& SearchData : OutTaggedMetaData)
	{
		// Should always be the first item, but there is no guarantee
		if (SearchData.Key.CompareTo(FFindInBlueprintSearchTags::FiB_Name) == 0)
		{
			SearchData.Value = FText::FromString(FName::NameToDisplayString(CustomFunctionName.ToString(), false));
			break;
		}
	}
	OutTaggedMetaData.Add(FSearchTagDataPair(FFindInBlueprintSearchTags::FiB_NativeName, FText::FromName(CustomFunctionName)));
}

FText UK2Node_CustomEvent::GetKeywords() const
{
	FText ParentKeywords = Super::GetKeywords();

	FFormatNamedArguments Args;
	Args.Add(TEXT("ParentKeywords"), ParentKeywords);
	return FText::Format(LOCTEXT("CustomEventKeywords", "{ParentKeywords} Custom"), Args);
}

#undef LOCTEXT_NAMESPACE
