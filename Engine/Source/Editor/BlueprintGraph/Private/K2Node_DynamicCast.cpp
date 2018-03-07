// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "K2Node_DynamicCast.h"
#include "UObject/Interface.h"
#include "Engine/Blueprint.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EdGraphSchema_K2.h"

#include "BlueprintEditorSettings.h"
#include "Kismet2/CompilerResultsLog.h"
#include "DynamicCastHandler.h"
#include "EditorCategoryUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "K2Node_DynamicCast"

namespace UK2Node_DynamicCastImpl
{
	static const FString CastSuccessPinName("bSuccess");
}

UK2Node_DynamicCast::UK2Node_DynamicCast(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bIsPureCast(false)
{
}

void UK2Node_DynamicCast::AllocateDefaultPins()
{
	const bool bReferenceObsoleteClass = TargetType && TargetType->HasAnyClassFlags(CLASS_NewerVersionExists);
	if (bReferenceObsoleteClass)
	{
		Message_Error(FString::Printf(TEXT("Node '%s' references obsolete class '%s'"), *GetPathName(), *TargetType->GetPathName()));
	}
	ensure(!bReferenceObsoleteClass);

	const UEdGraphSchema_K2* K2Schema = Cast<UEdGraphSchema_K2>(GetSchema());
	check(K2Schema != nullptr);
	if (!K2Schema->DoesGraphSupportImpureFunctions(GetGraph()))
	{
		bIsPureCast = true;
	}

	if (!bIsPureCast)
	{
		// Input - Execution Pin
		CreatePin(EGPD_Input, K2Schema->PC_Exec, FString(), nullptr, K2Schema->PN_Execute);

		// Output - Execution Pins
		CreatePin(EGPD_Output, K2Schema->PC_Exec, FString(), nullptr, K2Schema->PN_CastSucceeded);
		CreatePin(EGPD_Output, K2Schema->PC_Exec, FString(), nullptr, K2Schema->PN_CastFailed);
	}

	// Input - Source type Pin
	CreatePin(EGPD_Input, K2Schema->PC_Wildcard, FString(), UObject::StaticClass(), K2Schema->PN_ObjectToCast);

	// Output - Data Pin
	if (TargetType != NULL)
	{
		FString CastResultPinName = K2Schema->PN_CastedValuePrefix + TargetType->GetDisplayNameText().ToString();
		if (TargetType->IsChildOf(UInterface::StaticClass()))
		{
			CreatePin(EGPD_Output, K2Schema->PC_Interface, FString(), *TargetType, CastResultPinName);
		}
		else 
		{
			CreatePin(EGPD_Output, K2Schema->PC_Object, FString(), *TargetType, CastResultPinName);
		}
	}

	UEdGraphPin* BoolSuccessPin = CreatePin(EGPD_Output, K2Schema->PC_Boolean, FString(), nullptr, UK2Node_DynamicCastImpl::CastSuccessPinName);
	BoolSuccessPin->bHidden = !bIsPureCast;

	Super::AllocateDefaultPins();
}

FLinearColor UK2Node_DynamicCast::GetNodeTitleColor() const
{
	return FLinearColor(0.0f, 0.55f, 0.62f);
}

FSlateIcon UK2Node_DynamicCast::GetIconAndTint(FLinearColor& OutColor) const
{
	static FSlateIcon Icon("EditorStyle", "GraphEditor.Cast_16x");
	return Icon;
}

FText UK2Node_DynamicCast::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TargetType == nullptr)
	{
		return NSLOCTEXT("K2Node_DynamicCast", "BadCastNode", "Bad cast node");
	}
	else if (CachedNodeTitle.IsOutOfDate(this))
	{
		// If casting to BP class, use BP name not class name (ie. remove the _C)
		FString TargetName;
		UBlueprint* CastToBP = UBlueprint::GetBlueprintFromClass(TargetType);
		if (CastToBP != NULL)
		{
			TargetName = CastToBP->GetName();
		}
		else
		{
			TargetName = TargetType->GetName();
		}

		FFormatNamedArguments Args;
		Args.Add(TEXT("TargetName"), FText::FromString(TargetName));

		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitle.SetCachedText(FText::Format(NSLOCTEXT("K2Node_DynamicCast", "CastTo", "Cast To {TargetName}"), Args), this);
	}
	return CachedNodeTitle;
}

void UK2Node_DynamicCast::GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const
{
	Super::GetContextMenuActions(Context);

	if (!Context.bIsDebugging)
	{
		Context.MenuBuilder->BeginSection("K2NodeDynamicCast", LOCTEXT("DynamicCastHeader", "Cast"));
		{
			FText MenuEntryTitle = LOCTEXT("MakePureTitle", "Convert to pure cast");
			FText MenuEntryTooltip = LOCTEXT("MakePureTooltip", "Removes the execution pins to make the node more versatile (NOTE: the cast could still fail, resulting in an invalid output).");

			bool bCanTogglePurity = true;
			auto CanExecutePurityToggle = [](bool const bInCanTogglePurity)->bool
			{
				return bInCanTogglePurity;
			};

			if (bIsPureCast)
			{
				MenuEntryTitle = LOCTEXT("MakeImpureTitle", "Convert to impure cast");
				MenuEntryTooltip = LOCTEXT("MakeImpureTooltip", "Adds in branching execution pins so that you can separatly handle when the cast fails/succeeds.");

				const UEdGraphSchema_K2* K2Schema = Cast<UEdGraphSchema_K2>(GetSchema());
				check(K2Schema != nullptr);

				bCanTogglePurity = K2Schema->DoesGraphSupportImpureFunctions(GetGraph());
				if (!bCanTogglePurity)
				{
					MenuEntryTooltip = LOCTEXT("CannotMakeImpureTooltip", "This graph does not support impure calls (and you should therefore test the cast's result for validity).");
				}
			}

			Context.MenuBuilder->AddMenuEntry(
				MenuEntryTitle,
				MenuEntryTooltip,
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateUObject(this, &UK2Node_DynamicCast::TogglePurity),
					FCanExecuteAction::CreateStatic(CanExecutePurityToggle, bCanTogglePurity),
					FIsActionChecked()
				)
			);
		}
		Context.MenuBuilder->EndSection();
	}
}

void UK2Node_DynamicCast::PostReconstructNode()
{
	Super::PostReconstructNode();
	// update the pin name (to "Interface" if an interface is connected)
	NotifyPinConnectionListChanged(GetCastSourcePin());
}

void UK2Node_DynamicCast::PostPlacedNewNode()
{
	Super::PostPlacedNewNode();

	const UBlueprintEditorSettings* BlueprintSettings = GetDefault<UBlueprintEditorSettings>();
	SetPurity(BlueprintSettings->bFavorPureCastNodes);
}

UEdGraphPin* UK2Node_DynamicCast::GetValidCastPin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* Pin = FindPin(K2Schema->PN_CastSucceeded);
	check((Pin != nullptr) || bIsPureCast);
	check((Pin == nullptr) || (Pin->Direction == EGPD_Output));
	return Pin;
}

UEdGraphPin* UK2Node_DynamicCast::GetInvalidCastPin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* Pin = FindPin(K2Schema->PN_CastFailed);
	check((Pin != nullptr) || bIsPureCast);
	check((Pin == nullptr) || (Pin->Direction == EGPD_Output));
	return Pin;
}

UEdGraphPin* UK2Node_DynamicCast::GetCastResultPin() const
{
	if(TargetType != nullptr)
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

		for (int32 PinIdx = 0; PinIdx < Pins.Num(); PinIdx++)
		{
			if (Pins[PinIdx]->PinType.PinSubCategoryObject == *TargetType
				&& Pins[PinIdx]->Direction == EGPD_Output
				&& Pins[PinIdx]->PinName.StartsWith(K2Schema->PN_CastedValuePrefix))
			{
				return Pins[PinIdx];
			}
		}
	}
		
	return nullptr;
}

UEdGraphPin* UK2Node_DynamicCast::GetCastSourcePin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraphPin* Pin = FindPin(K2Schema->PN_ObjectToCast);
	check(Pin != NULL);
	check(Pin->Direction == EGPD_Input);
	return Pin;
}

UEdGraphPin* UK2Node_DynamicCast::GetBoolSuccessPin() const
{
	UEdGraphPin* Pin = FindPin(UK2Node_DynamicCastImpl::CastSuccessPinName);
	check((Pin == nullptr) || (Pin->Direction == EGPD_Output));
	return Pin;
}

void UK2Node_DynamicCast::SetPurity(bool bNewPurity)
{
	if (bNewPurity != bIsPureCast)
	{
		bIsPureCast = bNewPurity;

		bool const bHasBeenConstructed = (Pins.Num() > 0);
		if (bHasBeenConstructed)
		{
			ReconstructNode();
		}
	}
}

void UK2Node_DynamicCast::TogglePurity()
{
	const FText TransactionTitle = bIsPureCast ? LOCTEXT("TogglePurityToImpure", "Convert to Impure Cast") : LOCTEXT("TogglePurityToPure", "Convert to Pure Cast");
	const FScopedTransaction Transaction( TransactionTitle );
	Modify();

	SetPurity(!bIsPureCast);
}

UK2Node::ERedirectType UK2Node_DynamicCast::DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex) const
{
	ERedirectType RedirectType = Super::DoPinsMatchForReconstruction(NewPin, NewPinIndex, OldPin, OldPinIndex);
	if((ERedirectType_None == RedirectType) && (NULL != NewPin) && (NULL != OldPin))
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

		const bool bProperPrefix = 
			NewPin->PinName.StartsWith(K2Schema->PN_CastedValuePrefix, ESearchCase::CaseSensitive) && 
			OldPin->PinName.StartsWith(K2Schema->PN_CastedValuePrefix, ESearchCase::CaseSensitive);

		const bool bClassMatch = NewPin->PinType.PinSubCategoryObject.IsValid() &&
			(NewPin->PinType.PinSubCategoryObject == OldPin->PinType.PinSubCategoryObject);

		if(bProperPrefix && bClassMatch)
		{
			RedirectType = ERedirectType_Name;
		}
	}
	return RedirectType;
}

FNodeHandlingFunctor* UK2Node_DynamicCast::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_DynamicCast(CompilerContext, KCST_DynamicCast);
}

bool UK2Node_DynamicCast::HasExternalDependencies(TArray<class UStruct*>* OptionalOutput) const
{
	const UBlueprint* SourceBlueprint = GetBlueprint();
	UClass* SourceClass = *TargetType;
	const bool bResult = (SourceClass != NULL) && (SourceClass->ClassGeneratedBy != SourceBlueprint);
	if (bResult && OptionalOutput)
	{
		OptionalOutput->AddUnique(SourceClass);
	}
	const bool bSuperResult = Super::HasExternalDependencies(OptionalOutput);
	return bSuperResult || bResult;
}

FText UK2Node_DynamicCast::GetMenuCategory() const
{
	static FNodeTextCache CachedCategory;
	if (CachedCategory.IsOutOfDate(this))
	{
		// FText::Format() is slow, so we cache this to save on performance
		CachedCategory.SetCachedText(FEditorCategoryUtils::BuildCategoryString(FCommonEditorCategory::Utilities, LOCTEXT("ActionMenuCategory", "Casting")), this);
	}
	return CachedCategory;
}

FBlueprintNodeSignature UK2Node_DynamicCast::GetSignature() const
{
	FBlueprintNodeSignature NodeSignature = Super::GetSignature();
	NodeSignature.AddSubObject(TargetType);

	return NodeSignature;
}

bool UK2Node_DynamicCast::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	bool bIsDisallowed = Super::IsConnectionDisallowed(MyPin, OtherPin, OutReason);

	if (MyPin == GetCastSourcePin())
	{
		const FEdGraphPinType& OtherPinType = OtherPin->PinType;
		const FText OtherPinName = OtherPin->PinFriendlyName.IsEmpty() ? FText::FromString(OtherPin->PinName) : OtherPin->PinFriendlyName;

		if (OtherPinType.IsContainer())
		{
			bIsDisallowed = true;
			OutReason = LOCTEXT("CannotContainerCast", "You cannot cast containers of objects.").ToString();
		}
		else if (TargetType == nullptr)
		{
			bIsDisallowed = true;
			OutReason = LOCTEXT("InvalidTargetType", "This cast has an invalid target type (was the class deleted without a redirect?).").ToString();
		}
		else if ((OtherPinType.PinCategory == UEdGraphSchema_K2::PC_Interface) || TargetType->HasAnyClassFlags(CLASS_Interface))
		{
			// allow all interface casts
		}
		else if (OtherPinType.PinCategory == UEdGraphSchema_K2::PC_Object)
		{
			// let's handle wasted cast inputs with warnings in ValidateNodeDuringCompilation() instead
		}
		else
		{
			bIsDisallowed = true;
			OutReason = LOCTEXT("NonObjectCast", "You can only cast objects/interfaces.").ToString();
		}
	}
	return bIsDisallowed;
}

void UK2Node_DynamicCast::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	if (Pin == GetCastSourcePin())
	{
		Pin->PinFriendlyName = FText::GetEmpty();

		FEdGraphPinType& InputPinType = Pin->PinType;
		if (Pin->LinkedTo.Num() == 0)
		{
			InputPinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
			InputPinType.PinSubCategory.Empty();
			InputPinType.PinSubCategoryObject = nullptr;
		}
		else
		{
			const FEdGraphPinType& ConnectedPinType = Pin->LinkedTo[0]->PinType;
			if (ConnectedPinType.PinCategory == UEdGraphSchema_K2::PC_Interface)
			{
				Pin->PinFriendlyName = LOCTEXT("InterfaceInputName", "Interface");
				InputPinType.PinCategory = UEdGraphSchema_K2::PC_Interface;
				InputPinType.PinSubCategoryObject = ConnectedPinType.PinSubCategoryObject;
			}
			else if (ConnectedPinType.PinCategory == UEdGraphSchema_K2::PC_Object)
			{
				InputPinType.PinCategory = UEdGraphSchema_K2::PC_Object;
				InputPinType.PinSubCategoryObject = UObject::StaticClass();
			}
		}
	}
}

void UK2Node_DynamicCast::ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins)
{
	Super::ReallocatePinsDuringReconstruction(OldPins);

	// Update exec pins if we converted from impure to pure
	ReconnectPureExecPins(OldPins);
}

void UK2Node_DynamicCast::ValidateNodeDuringCompilation(FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	UEdGraphPin* SourcePin = GetCastSourcePin();
	if (SourcePin->LinkedTo.Num() > 0)
	{
		UClass* SourceType = *TargetType;
		if (SourceType == nullptr)
		{
			return;
		}
		SourceType = SourceType->GetAuthoritativeClass();

		for (UEdGraphPin* CastInput : SourcePin->LinkedTo)
		{
			const FEdGraphPinType& SourcePinType = CastInput->PinType;
			if (SourcePinType.PinCategory != UEdGraphSchema_K2::PC_Object)
			{
				// all other types should have been rejected by IsConnectionDisallowed()
				continue;
			}

			UClass* SourceClass = Cast<UClass>(SourcePinType.PinSubCategoryObject.Get());
			if ((SourceClass == nullptr) && (SourcePinType.PinSubCategory == UEdGraphSchema_K2::PSC_Self))
			{
				if (UK2Node* K2Node = Cast<UK2Node>(CastInput->GetOwningNode()))
				{
					SourceClass = K2Node->GetBlueprint()->GeneratedClass;
				}
			}

			if (SourceClass == nullptr)
			{
				const FString SourcePinName = CastInput->PinFriendlyName.IsEmpty() ? CastInput->PinName : CastInput->PinFriendlyName.ToString();

				FText const ErrorFormat = LOCTEXT("BadCastInput", "'%s' does not have a clear object type (invalid input into @@).");
				MessageLog.Error( *FString::Printf(*ErrorFormat.ToString(), *SourcePinName), this );

				continue;
			}
			SourceClass = SourceClass->GetAuthoritativeClass();

			if (SourceClass == SourceType)
			{
				const FString SourcePinName = CastInput->PinFriendlyName.IsEmpty() ? CastInput->PinName : CastInput->PinFriendlyName.ToString();

				FText const WarningFormat = LOCTEXT("EqualObjectCast", "'%s' is already a '%s', you don't need @@.");
				MessageLog.Note( *FString::Printf(*WarningFormat.ToString(), *SourcePinName, *TargetType->GetDisplayNameText().ToString()), this );
			}
			else if (SourceClass->IsChildOf(SourceType))
			{
				const FString SourcePinName = CastInput->PinFriendlyName.IsEmpty() ? CastInput->PinName : CastInput->PinFriendlyName.ToString();

				FText const WarningFormat = LOCTEXT("UnneededObjectCast", "'%s' is already a '%s' (which inherits from '%s'), so you don't need @@.");
				MessageLog.Note( *FString::Printf(*WarningFormat.ToString(), *SourcePinName, *SourceClass->GetDisplayNameText().ToString(), *TargetType->GetDisplayNameText().ToString()), this );
			}
			else if (!SourceType->IsChildOf(SourceClass) && !FKismetEditorUtilities::IsClassABlueprintInterface(SourceType))
			{
				FText const WarningFormat = LOCTEXT("DisallowedObjectCast", "'%s' does not inherit from '%s' (@@ would always fail).");
				MessageLog.Warning( *FString::Printf(*WarningFormat.ToString(), *TargetType->GetDisplayNameText().ToString(), *SourceClass->GetDisplayNameText().ToString()), this );
			}
		}
	}
}


bool UK2Node_DynamicCast::ReconnectPureExecPins(TArray<UEdGraphPin*>& OldPins)
{
	if (bIsPureCast)
	{
		// look for an old exec pin
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
		UEdGraphPin* PinExec = nullptr;
		for (UEdGraphPin* Pin : OldPins)
		{
			if (Pin->PinName == K2Schema->PN_Execute)
			{
				PinExec = Pin;
				break;
			}
		}
		if (PinExec)
		{
			// look for old then pin
			UEdGraphPin* PinThen = nullptr;
			for (UEdGraphPin* Pin : OldPins)
			{
				if (Pin->PinName == K2Schema->PN_Then)
				{
					PinThen = Pin;
					break;
				}
			}
			if (PinThen)
			{
				// reconnect all incoming links to old exec pin to the far end of the old then pin.
				if (PinThen->LinkedTo.Num() > 0)
				{
					UEdGraphPin* PinThenLinked = PinThen->LinkedTo[0];
					while (PinExec->LinkedTo.Num() > 0)
					{
						UEdGraphPin* PinExecLinked = PinExec->LinkedTo[0];
						PinExecLinked->BreakLinkTo(PinExec);
						PinExecLinked->MakeLinkTo(PinThenLinked);
					}
					return true;
				}
			}
		}
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
