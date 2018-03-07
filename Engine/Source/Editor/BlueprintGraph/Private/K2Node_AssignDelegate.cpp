// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_AssignDelegate.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EditorStyleSettings.h"
#include "Editor/EditorEngine.h"
#include "ObjectEditorUtils.h"
#include "EditorCategoryUtils.h"

#define LOCTEXT_NAMESPACE "K2Node_AssignDelegate"

//------------------------------------------------------------------------------
UK2Node_AssignDelegate::UK2Node_AssignDelegate(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

//------------------------------------------------------------------------------
FText UK2Node_AssignDelegate::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
	{
		if (CachedListTitle.IsOutOfDate(this))
		{
			FText PropertyName;
			if (UProperty* Property = GetProperty())
			{
				bool const bShowFriendlyNames = GetDefault<UEditorStyleSettings>()->bShowFriendlyNames;
				PropertyName = FText::FromString(bShowFriendlyNames ? UEditorEngine::GetFriendlyName(Property) : Property->GetName());

				// FText::Format() is slow, so we cache this to save on performance
				CachedListTitle.SetCachedText(FText::Format(LOCTEXT("AssignDelegateTitle", "Assign {0}"), PropertyName), this);
			}
			else
			{
				return LOCTEXT("InvalidPropertyTitle", "Assign <invalid delegate>");
			}
		}
		return CachedListTitle;
	}
	
	return Super::GetNodeTitle(TitleType);
}

//------------------------------------------------------------------------------
FText UK2Node_AssignDelegate::GetMenuCategory() const
{
	FText MenuCategory = FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Delegates);
	if (UProperty* Property = GetProperty())
	{
		MenuCategory = FText::FromString(FObjectEditorUtils::GetCategory(Property));
	}
	return MenuCategory;
}

//------------------------------------------------------------------------------
bool UK2Node_AssignDelegate::IsCompatibleWithGraph(const UEdGraph* TargetGraph) const
{
	bool bIsCompatible = Super::IsCompatibleWithGraph(TargetGraph);

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	check(Blueprint != nullptr);
	UEdGraphSchema const* Schema = TargetGraph->GetSchema();

	EGraphType GraphType = Schema->GetGraphType(TargetGraph);
	bool const bAllowEvents = (GraphType == GT_Ubergraph) && FBlueprintEditorUtils::DoesSupportEventGraphs(Blueprint);
	
	return bAllowEvents && Super::IsCompatibleWithGraph(TargetGraph);
}

//------------------------------------------------------------------------------
void UK2Node_AssignDelegate::PostPlacedNewNode()
{
	UEdGraphPin* InDelegatePin = GetDelegatePin();
	if (InDelegatePin->LinkedTo.Num() == 0)
	{
		if (UMulticastDelegateProperty* DelegateProp = Cast<UMulticastDelegateProperty>(GetProperty()))
		{
			FVector2D const LocationOffset(-150.f, +150.f);

			FText DelegateName = FText::FromName(DelegateProp->GetFName());
			FText DesiredEventName = FText::Format(LOCTEXT("BindedEventName", "{0}_Event"), DelegateName);
			FName EventName = FBlueprintEditorUtils::FindUniqueKismetName(GetBlueprint(), DesiredEventName.ToString());

			UK2Node_CustomEvent* EventNode = UK2Node_CustomEvent::CreateFromFunction(
				FVector2D(NodePosX, NodePosY) + LocationOffset,
				GetGraph(), EventName.ToString(), DelegateProp->SignatureFunction, /*bSelectNewNode =*/false);

			if (EventNode != nullptr)
			{
				UEdGraphSchema_K2 const* K2Schema = GetDefault<UEdGraphSchema_K2>();
				UEdGraphPin* OutDelegatePin = EventNode->FindPinChecked(UK2Node_CustomEvent::DelegateOutputName);

				K2Schema->TryCreateConnection(OutDelegatePin, InDelegatePin);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
