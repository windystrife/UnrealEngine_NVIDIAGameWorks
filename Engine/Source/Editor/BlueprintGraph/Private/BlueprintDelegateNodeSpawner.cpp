// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlueprintDelegateNodeSpawner.h"
#include "K2Node_Variable.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EditorStyleSettings.h"
#include "Editor/EditorEngine.h"
#include "ObjectEditorUtils.h"
#include "EditorCategoryUtils.h"

#define LOCTEXT_NAMESPACE "BlueprintDelegateNodeSpawner"

/*******************************************************************************
 * Static UBlueprintDelegateNodeSpawner Helpers
 ******************************************************************************/

namespace BlueprintDelegateNodeSpawnerImpl
{
	static FText GetDefaultMenuName(UMulticastDelegateProperty const* Delegate);
	static FText GetDefaultMenuCategory(UMulticastDelegateProperty const* Delegate);
	static FSlateIcon GetDefaultMenuIcon(UMulticastDelegateProperty const* Delegate, FLinearColor& ColorOut);
}

//------------------------------------------------------------------------------
static FText BlueprintDelegateNodeSpawnerImpl::GetDefaultMenuName(UMulticastDelegateProperty const* Delegate)
{
	bool const bShowFriendlyNames = GetDefault<UEditorStyleSettings>()->bShowFriendlyNames;
	return bShowFriendlyNames ? FText::FromString(UEditorEngine::GetFriendlyName(Delegate)) : FText::FromName(Delegate->GetFName());
}

//------------------------------------------------------------------------------
static FText BlueprintDelegateNodeSpawnerImpl::GetDefaultMenuCategory(UMulticastDelegateProperty const* Delegate)
{
	FText DelegateCategory = FText::FromString(FObjectEditorUtils::GetCategory(Delegate));
	if (DelegateCategory.IsEmpty())
	{
		DelegateCategory = FEditorCategoryUtils::GetCommonCategory(FCommonEditorCategory::Delegates);
	}
	return DelegateCategory;
}

//------------------------------------------------------------------------------
static FSlateIcon BlueprintDelegateNodeSpawnerImpl::GetDefaultMenuIcon(UMulticastDelegateProperty const* Delegate, FLinearColor& ColorOut)
{
	FName    const PropertyName = Delegate->GetFName();
	UStruct* const PropertyOwner = CastChecked<UStruct>(Delegate->GetOuterUField());

	return UK2Node_Variable::GetVariableIconAndColor(PropertyOwner, PropertyName, ColorOut);
}

/*******************************************************************************
 * UBlueprintDelegateNodeSpawner
 ******************************************************************************/

//------------------------------------------------------------------------------
UBlueprintDelegateNodeSpawner* UBlueprintDelegateNodeSpawner::Create(TSubclassOf<UK2Node_BaseMCDelegate> NodeClass, UMulticastDelegateProperty const* const Property, UObject* Outer/* = nullptr*/)
{
	check(Property != nullptr);
	if (Outer == nullptr)
	{
		Outer = GetTransientPackage();
	}

	//--------------------------------------
	// Constructing the Spawner
	//--------------------------------------

	UBlueprintDelegateNodeSpawner* NodeSpawner = NewObject<UBlueprintDelegateNodeSpawner>(Outer);
	NodeSpawner->Field     = Property;
	NodeSpawner->NodeClass = NodeClass;

	//--------------------------------------
	// Default UI Signature
	//--------------------------------------

	FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;
	//MenuSignature.MenuName, will be pulled from the node template
	MenuSignature.Category = BlueprintDelegateNodeSpawnerImpl::GetDefaultMenuCategory(Property);
	//MenuSignature.Tooltip,  will be pulled from the node template
	//MenuSignature.Keywords, will be pulled from the node template
	MenuSignature.Icon = BlueprintDelegateNodeSpawnerImpl::GetDefaultMenuIcon(Property, MenuSignature.IconTint);

	//--------------------------------------
	// Post-Spawn Setup
	//--------------------------------------

	auto SetDelegateLambda = [](UEdGraphNode* NewNode, UField const* InField)
	{
		UMulticastDelegateProperty const* MCDProperty = Cast<UMulticastDelegateProperty>(InField);

		UK2Node_BaseMCDelegate* DelegateNode = Cast<UK2Node_BaseMCDelegate>(NewNode);
		if ((DelegateNode != nullptr) && (MCDProperty != nullptr))
		{
			UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(NewNode);
			UClass* OwnerClass = MCDProperty->GetOwnerClass();
			bool const bIsSelfContext = Blueprint->SkeletonGeneratedClass->IsChildOf(OwnerClass);

			DelegateNode->SetFromProperty(MCDProperty, bIsSelfContext);
		}
	};
	NodeSpawner->SetNodeFieldDelegate = FSetNodeFieldDelegate::CreateStatic(SetDelegateLambda);

	return NodeSpawner;
}

//------------------------------------------------------------------------------
UBlueprintDelegateNodeSpawner::UBlueprintDelegateNodeSpawner(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

//------------------------------------------------------------------------------
UMulticastDelegateProperty const* UBlueprintDelegateNodeSpawner::GetDelegateProperty() const
{
	return Cast<UMulticastDelegateProperty>(GetField());
}

#undef LOCTEXT_NAMESPACE
