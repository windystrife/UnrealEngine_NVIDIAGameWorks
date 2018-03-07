// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlueprintVariableNodeSpawner.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EditorStyleSettings.h"
#include "Editor/EditorEngine.h"
#include "ObjectEditorUtils.h"
#include "EditorCategoryUtils.h"

#define LOCTEXT_NAMESPACE "BlueprintVariableNodeSpawner"

/*******************************************************************************
 * UBlueprintVariableNodeSpawner
 ******************************************************************************/

//------------------------------------------------------------------------------
UBlueprintVariableNodeSpawner* UBlueprintVariableNodeSpawner::CreateFromMemberOrParam(TSubclassOf<UK2Node_Variable> NodeClass, UProperty const* VarProperty, UEdGraph* VarContext, UObject* Outer/*= nullptr*/)
{
	check(VarProperty != nullptr);
	if (Outer == nullptr)
	{
		Outer = GetTransientPackage();
	}

	//--------------------------------------
	// Constructing the Spawner
	//--------------------------------------

	UBlueprintVariableNodeSpawner* NodeSpawner = NewObject<UBlueprintVariableNodeSpawner>(Outer);
	NodeSpawner->NodeClass = NodeClass;
	NodeSpawner->Field = VarProperty;
	NodeSpawner->LocalVarOuter = VarContext;

	//--------------------------------------
	// Default UI Signature
	//--------------------------------------

	FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;
	FString const VarSubCategory = FObjectEditorUtils::GetCategory(VarProperty);
	MenuSignature.Category = FEditorCategoryUtils::BuildCategoryString(FCommonEditorCategory::Variables, FText::FromString(VarSubCategory));

	FText const VarName = NodeSpawner->GetVariableName();
	// @TODO: NodeClass could be modified post Create()
	if (NodeClass->IsChildOf(UK2Node_VariableGet::StaticClass()))
	{
		MenuSignature.MenuName = FText::Format(LOCTEXT("GetterMenuName", "Get {0}"), VarName);
		MenuSignature.Tooltip  = UK2Node_VariableGet::GetPropertyTooltip(VarProperty);
	}
	else if (NodeClass->IsChildOf(UK2Node_VariableSet::StaticClass()))
	{
		MenuSignature.MenuName = FText::Format(LOCTEXT("SetterMenuName", "Set {0}"), VarName);
		MenuSignature.Tooltip  = UK2Node_VariableSet::GetPropertyTooltip(VarProperty);
	}
	// add at least one character, so that PrimeDefaultUiSpec() doesn't 
	// attempt to query the template node
	//
	// @TODO: maybe UPROPERTY() fields should have keyword metadata like functions
	if (MenuSignature.Keywords.IsEmpty())
	{
		// want to set it to something so we won't end up back in this condition
		MenuSignature.Keywords = FText::FromString(TEXT(" "));
	}
	MenuSignature.Icon = UK2Node_Variable::GetVarIconFromPinType(NodeSpawner->GetVarType(), MenuSignature.IconTint);

	//--------------------------------------
	// Post-Spawn Setup
	//--------------------------------------

	auto MemberVarSetupLambda = [](UEdGraphNode* NewNode, UField const* InField)
	{
		if (UProperty const* Property = Cast<UProperty>(InField))
		{
			UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForNodeChecked(NewNode);
			UClass* OwnerClass = Property->GetOwnerClass();

			// We need to use a generated class instead of a skeleton class for IsChildOf, so if the OwnerClass has a Blueprint, grab the GeneratedClass
			const bool bOwnerClassIsSelfContext = (Blueprint->SkeletonGeneratedClass->GetAuthoritativeClass() == OwnerClass) || Blueprint->SkeletonGeneratedClass->IsChildOf(OwnerClass);
			const bool bIsFunctionVariable = Property->GetOuter()->IsA(UFunction::StaticClass());

			UK2Node_Variable* VarNode = CastChecked<UK2Node_Variable>(NewNode);
			VarNode->SetFromProperty(Property, bOwnerClassIsSelfContext && !bIsFunctionVariable);
		}
	};
	NodeSpawner->SetNodeFieldDelegate = FSetNodeFieldDelegate::CreateStatic(MemberVarSetupLambda);

	return NodeSpawner;
}

//------------------------------------------------------------------------------
UBlueprintVariableNodeSpawner* UBlueprintVariableNodeSpawner::CreateFromLocal(TSubclassOf<UK2Node_Variable> NodeClass, UEdGraph* VarContext, FBPVariableDescription const& VarDesc, UProperty* VarProperty, UObject* Outer/*= nullptr*/)
{
	check(VarContext != nullptr);
	if (Outer == nullptr)
	{
		Outer = GetTransientPackage();
	}

	//--------------------------------------
	// Constructing the Spawner
	//--------------------------------------

	// @TODO: consider splitting out local variable spawners (since they don't 
	//        conform to UBlueprintFieldNodeSpawner
	UBlueprintVariableNodeSpawner* NodeSpawner = NewObject<UBlueprintVariableNodeSpawner>(Outer);
	NodeSpawner->NodeClass     = NodeClass;
	NodeSpawner->LocalVarOuter = VarContext;
	NodeSpawner->LocalVarDesc  = VarDesc;
	NodeSpawner->Field = VarProperty;

	//--------------------------------------
	// Default UI Signature
	//--------------------------------------

	FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;
	MenuSignature.Category = FEditorCategoryUtils::BuildCategoryString(FCommonEditorCategory::Variables, VarDesc.Category);

	FText const VarName = NodeSpawner->GetVariableName();
	// @TODO: NodeClass could be modified post Create()
	if (NodeClass->IsChildOf(UK2Node_VariableGet::StaticClass()))
	{
		MenuSignature.MenuName = FText::Format(LOCTEXT("LocalGetterMenuName", "Get {0}"), VarName);
		MenuSignature.Tooltip  = UK2Node_VariableGet::GetBlueprintVarTooltip(VarDesc);
	}
	else if (NodeClass->IsChildOf(UK2Node_VariableSet::StaticClass()))
	{
		MenuSignature.MenuName = FText::Format(LOCTEXT("LocalSetterMenuName", "Set {0}"), VarName);
		MenuSignature.Tooltip  = UK2Node_VariableSet::GetBlueprintVarTooltip(VarDesc);
	}
	// add at least one character, so that PrimeDefaultUiSpec() doesn't 
	// attempt to query the template node
	if (MenuSignature.Keywords.IsEmpty())
	{
		// want to set it to something so we won't end up back in this condition
		MenuSignature.Keywords = FText::FromString(TEXT(" "));
	}
	MenuSignature.Icon = UK2Node_Variable::GetVarIconFromPinType(NodeSpawner->GetVarType(), MenuSignature.IconTint);

	return NodeSpawner;
}

//------------------------------------------------------------------------------
UBlueprintVariableNodeSpawner::UBlueprintVariableNodeSpawner(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

//------------------------------------------------------------------------------
void UBlueprintVariableNodeSpawner::Prime()
{
	// we expect that you don't need a node template to construct menu entries
	// from this, so we choose not to pre-cache one here
}

//------------------------------------------------------------------------------
FBlueprintNodeSignature UBlueprintVariableNodeSpawner::GetSpawnerSignature() const
{
	FBlueprintNodeSignature SpawnerSignature(NodeClass);
	if (IsUserLocalVariable())
	{
		SpawnerSignature.AddSubObject(LocalVarOuter);
		static const FName LocalVarSignatureKey(TEXT("LocalVarName"));
		SpawnerSignature.AddNamedValue(LocalVarSignatureKey, LocalVarDesc.VarName.ToString());
	}
	
	return SpawnerSignature;
}

//------------------------------------------------------------------------------
FBlueprintActionUiSpec UBlueprintVariableNodeSpawner::GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const
{
	UEdGraph* TargetGraph = (Context.Graphs.Num() > 0) ? Context.Graphs[0] : nullptr;
	FBlueprintActionUiSpec MenuSignature = PrimeDefaultUiSpec(TargetGraph);

	if (UProperty const* WrappedVariable = GetVarProperty())
	{
		checkSlow(Context.Blueprints.Num() > 0);
		UBlueprint const* TargetBlueprint = Context.Blueprints[0];

		// @TODO: this is duplicated in a couple places, move it to some shared resource
		UClass const* TargetClass = (TargetBlueprint->GeneratedClass != nullptr) ? TargetBlueprint->GeneratedClass : TargetBlueprint->ParentClass;
		for (UEdGraphPin* Pin : Context.Pins)
		{
			if ((Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object) && 
				 Pin->PinType.PinSubCategoryObject.IsValid())
			{
				TargetClass = CastChecked<UClass>(Pin->PinType.PinSubCategoryObject.Get());
			}
		}

		auto OwnerClass = WrappedVariable->GetOwnerClass();
		const bool bIsOwneClassValid = OwnerClass && (!Cast<const UBlueprintGeneratedClass>(OwnerClass) || OwnerClass->ClassGeneratedBy); //todo: more general validation
		UClass const* VariableClass = bIsOwneClassValid ? OwnerClass->GetAuthoritativeClass() : NULL;
		if (VariableClass && !TargetClass->IsChildOf(VariableClass))
		{
			MenuSignature.Category = FEditorCategoryUtils::BuildCategoryString(FCommonEditorCategory::Class,
				FText::FromString(VariableClass->GetDisplayNameText().ToString()));
		}
	}
	DynamicUiSignatureGetter.ExecuteIfBound(Context, Bindings, &MenuSignature);
	return MenuSignature;
}

//------------------------------------------------------------------------------
UEdGraphNode* UBlueprintVariableNodeSpawner::Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const
{
	UEdGraphNode* NewNode = nullptr;
	// @TODO: consider splitting out local variable spawners (since they don't 
	//        conform to UBlueprintFieldNodeSpawner
	if (IsLocalVariable())
	{
		auto LocalVarSetupLambda = [](UEdGraphNode* InNewNode, bool bIsTemplateNode, FName VarName, UObject const* VarOuter, FGuid VarGuid, FCustomizeNodeDelegate UserDelegate)
		{
			UK2Node_Variable* VarNode = CastChecked<UK2Node_Variable>(InNewNode);
			VarNode->VariableReference.SetLocalMember(VarName, VarOuter->GetName(), VarGuid);
			UserDelegate.ExecuteIfBound(InNewNode, bIsTemplateNode);
		};

		FCustomizeNodeDelegate PostSpawnDelegate = CustomizeNodeDelegate;
		if (UObject const* LocalVariableOuter = GetVarOuter())
		{
			PostSpawnDelegate = FCustomizeNodeDelegate::CreateStatic(LocalVarSetupLambda, IsUserLocalVariable() ? LocalVarDesc.VarName : Field->GetFName(), LocalVariableOuter, LocalVarDesc.VarGuid, CustomizeNodeDelegate);
		}

		NewNode = UBlueprintNodeSpawner::SpawnNode<UEdGraphNode>(NodeClass, ParentGraph, Bindings, Location, PostSpawnDelegate);
	}
	else
	{
		NewNode = Super::Invoke(ParentGraph, Bindings, Location);
	}

	return NewNode;
}

//------------------------------------------------------------------------------
bool UBlueprintVariableNodeSpawner::IsUserLocalVariable() const
{
	return (LocalVarDesc.VarName != NAME_None);
}

//------------------------------------------------------------------------------
bool UBlueprintVariableNodeSpawner::IsLocalVariable() const
{
	return (LocalVarDesc.VarName != NAME_None) || (LocalVarOuter != nullptr);
}

//------------------------------------------------------------------------------
UObject const* UBlueprintVariableNodeSpawner::GetVarOuter() const
{
	UObject const* VarOuter = nullptr;
	if (IsLocalVariable())
	{
		VarOuter = LocalVarOuter;
	}
	else if (UProperty const* MemberVariable = GetVarProperty())
	{
		VarOuter = MemberVariable->GetOuter();
	}	
	return VarOuter;
}

//------------------------------------------------------------------------------
UProperty const* UBlueprintVariableNodeSpawner::GetVarProperty() const
{
	// Get() does IsValid() checks for us
	return Cast<UProperty>(GetField());
}

//------------------------------------------------------------------------------
FEdGraphPinType UBlueprintVariableNodeSpawner::GetVarType() const
{
	FEdGraphPinType VarType;
	if (IsUserLocalVariable())
	{
		VarType = LocalVarDesc.VarType;
	}
	else if (UProperty const* VarProperty = GetVarProperty())
	{
		UEdGraphSchema_K2 const* K2Schema = GetDefault<UEdGraphSchema_K2>();
		K2Schema->ConvertPropertyToPinType(VarProperty, VarType);
	}
	return VarType;
}

//------------------------------------------------------------------------------
FText UBlueprintVariableNodeSpawner::GetVariableName() const
{
	FText VarName;

	bool bShowFriendlyNames = GetDefault<UEditorStyleSettings>()->bShowFriendlyNames;
	if (IsUserLocalVariable())
	{
		VarName = bShowFriendlyNames ? FText::FromString(LocalVarDesc.FriendlyName) : FText::FromName(LocalVarDesc.VarName);
	}
	else if (UProperty const* MemberVariable = GetVarProperty())
	{
		VarName = bShowFriendlyNames ? FText::FromString(UEditorEngine::GetFriendlyName(MemberVariable)) : FText::FromName(MemberVariable->GetFName());
	}
	return VarName;
}

#undef LOCTEXT_NAMESPACE
