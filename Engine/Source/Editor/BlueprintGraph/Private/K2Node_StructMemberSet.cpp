// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_StructMemberSet.h"
#include "EdGraphSchema_K2.h"
#include "StructMemberNodeHandlers.h"

//////////////////////////////////////////////////////////////////////////
// UK2Node_StructMemberSet

#define LOCTEXT_NAMESPACE "K2Node"

UK2Node_StructMemberSet::UK2Node_StructMemberSet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_StructMemberSet::PreEditChange(UProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);

	if (PropertyThatWillChange && PropertyThatWillChange->GetFName() == GET_MEMBER_NAME_CHECKED(FOptionalPinFromProperty, bShowPin))
	{
		FOptionalPinManager::CacheShownPins(ShowPinForProperties, OldShownPins);
	}
}

void UK2Node_StructMemberSet::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None);

	if (PropertyName == GET_MEMBER_NAME_CHECKED(FOptionalPinFromProperty, bShowPin))
	{
		FOptionalPinManager::EvaluateOldShownPins(ShowPinForProperties, OldShownPins, this);
		GetSchema()->ReconstructNode(*this);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UK2Node_StructMemberSet::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	// Add the execution sequencing pin
	CreatePin(EGPD_Input, Schema->PC_Exec, FString(), nullptr, Schema->PN_Execute);
	CreatePin(EGPD_Output, Schema->PC_Exec, FString(), nullptr, Schema->PN_Then);

	// Display any currently visible optional pins
	{
		FStructOperationOptionalPinManager OptionalPinManager;
		OptionalPinManager.RebuildPropertyList(ShowPinForProperties, StructType);
		OptionalPinManager.CreateVisiblePins(ShowPinForProperties, StructType, EGPD_Input, this);
	}
}

FText UK2Node_StructMemberSet::GetTooltipText() const
{
	if (CachedTooltip.IsOutOfDate(this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("VariableName"), FText::FromName(VariableReference.GetMemberName()));
		// FText::Format() is slow, so we cache this to save on performance
		CachedTooltip.SetCachedText(FText::Format(LOCTEXT("K2Node_StructMemberSet_Tooltip", "Set member variables of {VariableName}"), Args), this);
	}
	return CachedTooltip;
}

FText UK2Node_StructMemberSet::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (CachedNodeTitle.IsOutOfDate(this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("VariableName"), FText::FromName(VariableReference.GetMemberName()));
		// FText::Format() is slow, so we cache this to save on performance
		CachedNodeTitle.SetCachedText(FText::Format(LOCTEXT("SetMembersInVariable", "Set members in {VariableName}"), Args), this);
	}
	return CachedNodeTitle;
}

UK2Node::ERedirectType UK2Node_StructMemberSet::DoPinsMatchForReconstruction(const UEdGraphPin* NewPin, int32 NewPinIndex, const UEdGraphPin* OldPin, int32 OldPinIndex)  const
{
	return UK2Node::DoPinsMatchForReconstruction(NewPin, NewPinIndex, OldPin, OldPinIndex);
}

FNodeHandlingFunctor* UK2Node_StructMemberSet::CreateNodeHandler(FKismetCompilerContext& CompilerContext) const
{
	return new FKCHandler_StructMemberVariableSet(CompilerContext);
}

#undef LOCTEXT_NAMESPACE
