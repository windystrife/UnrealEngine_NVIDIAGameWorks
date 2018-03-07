// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlueprintComponentNodeSpawner.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_AddComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Styling/SlateIconFinder.h"
#include "BlueprintNodeTemplateCache.h"
#include "ComponentAssetBroker.h"
#include "ComponentTypeRegistry.h"

#define LOCTEXT_NAMESPACE "BlueprintComponenetNodeSpawner"

/*******************************************************************************
 * Static UBlueprintComponentNodeSpawner Helpers
 ******************************************************************************/

namespace BlueprintComponentNodeSpawnerImpl
{
	static FText GetDefaultMenuCategory(TSubclassOf<UActorComponent> const ComponentClass);
}

//------------------------------------------------------------------------------
static FText BlueprintComponentNodeSpawnerImpl::GetDefaultMenuCategory(TSubclassOf<UActorComponent> const ComponentClass)
{
	FText ClassGroup;
	TArray<FString> ClassGroupNames;
	ComponentClass->GetClassGroupNames(ClassGroupNames);

	static FText const DefaultClassGroup(LOCTEXT("DefaultClassGroup", "Common"));
	// 'Common' takes priority over other class groups
	if (ClassGroupNames.Contains(DefaultClassGroup.ToString()) || (ClassGroupNames.Num() == 0))
	{
		ClassGroup = DefaultClassGroup;
	}
	else
	{
		ClassGroup = FText::FromString(ClassGroupNames[0]);
	}
	return FText::Format(LOCTEXT("ComponentCategory", "Add Component|{0}"), ClassGroup);
}

/*******************************************************************************
 * UBlueprintComponentNodeSpawner
 ******************************************************************************/

//------------------------------------------------------------------------------
UBlueprintComponentNodeSpawner* UBlueprintComponentNodeSpawner::Create(const FComponentTypeEntry& Entry)
{
	UClass* ComponentClass = Entry.ComponentClass;
	if (ComponentClass == nullptr)
	{
		// unloaded class, must be blueprint created. Create an entry. We'll load the class when we spawn the node:

		UBlueprintComponentNodeSpawner* NodeSpawner = NewObject<UBlueprintComponentNodeSpawner>(GetTransientPackage());
		NodeSpawner->ComponentClass = nullptr;
		NodeSpawner->NodeClass = UK2Node_AddComponent::StaticClass();
		NodeSpawner->ComponentName = Entry.ComponentName;
		NodeSpawner->ComponentAssetName = Entry.ComponentAssetName;

		FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;
		FText const ComponentTypeName = FText::FromString(Entry.ComponentName);
		MenuSignature.MenuName = FText::Format(LOCTEXT("AddComponentMenuName", "Add {0}"), ComponentTypeName);
		MenuSignature.Category = LOCTEXT("BlueprintComponentCategory", "Custom");
		MenuSignature.Tooltip = FText::Format(LOCTEXT("AddComponentTooltip", "Spawn a {0}"), ComponentTypeName);
		// add at least one character, so that PrimeDefaultUiSpec() doesn't 
		// attempt to query the template node
		if (MenuSignature.Keywords.IsEmpty())
		{
			MenuSignature.Keywords = FText::FromString(TEXT(" "));
		}
		MenuSignature.Icon = FSlateIconFinder::FindIconForClass(nullptr);
		MenuSignature.DocLink  = TEXT("Shared/GraphNodes/Blueprint/UK2Node_AddComponent");
		MenuSignature.DocExcerptTag = TEXT("AddComponent");

		return NodeSpawner;
	}

	if (ComponentClass->HasAnyClassFlags(CLASS_Abstract) || !ComponentClass->HasMetaData(FBlueprintMetadata::MD_BlueprintSpawnableComponent))
	{
		// loaded class that is marked as abstract or not spawnable, don't create an entry:
		return nullptr;
	}

	if (ComponentClass->ClassWithin && ComponentClass->ClassWithin != UObject::StaticClass())
	{
		// we can't support 'Within' markup on components at this time (core needs to be aware of non-CDO archetypes
		// that have within markup, and BP system needs to property use RF_ArchetypeObject on template objects)
		return nullptr;
	}

	UClass* const AuthoritativeClass = ComponentClass->GetAuthoritativeClass();

	UBlueprintComponentNodeSpawner* NodeSpawner = NewObject<UBlueprintComponentNodeSpawner>(GetTransientPackage());
	NodeSpawner->ComponentClass = AuthoritativeClass;
	NodeSpawner->NodeClass      = UK2Node_AddComponent::StaticClass();

	FBlueprintActionUiSpec& MenuSignature = NodeSpawner->DefaultMenuSignature;
	FText const ComponentTypeName = AuthoritativeClass->GetDisplayNameText();
	MenuSignature.MenuName = FText::Format(LOCTEXT("AddComponentMenuName", "Add {0}"), ComponentTypeName);
	MenuSignature.Category = BlueprintComponentNodeSpawnerImpl::GetDefaultMenuCategory(AuthoritativeClass);
	MenuSignature.Tooltip  = FText::Format(LOCTEXT("AddComponentTooltip", "Spawn a {0}"), ComponentTypeName);
	MenuSignature.Keywords = AuthoritativeClass->GetMetaDataText(*FBlueprintMetadata::MD_FunctionKeywords.ToString(), TEXT("UObjectKeywords"), AuthoritativeClass->GetFullGroupName(false));
	// add at least one character, so that PrimeDefaultUiSpec() doesn't 
	// attempt to query the template node
	if (MenuSignature.Keywords.IsEmpty())
	{
		MenuSignature.Keywords = FText::FromString(TEXT(" "));
	}
	MenuSignature.Icon = FSlateIconFinder::FindIconForClass(AuthoritativeClass);
	MenuSignature.DocLink  = TEXT("Shared/GraphNodes/Blueprint/UK2Node_AddComponent");
	MenuSignature.DocExcerptTag = AuthoritativeClass->GetName();

	return NodeSpawner;
}

//------------------------------------------------------------------------------
UBlueprintComponentNodeSpawner::UBlueprintComponentNodeSpawner(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

//------------------------------------------------------------------------------
FBlueprintNodeSignature UBlueprintComponentNodeSpawner::GetSpawnerSignature() const
{
	FBlueprintNodeSignature SpawnerSignature(NodeClass);
	SpawnerSignature.AddSubObject(ComponentClass);
	return SpawnerSignature;
}

//------------------------------------------------------------------------------
// Evolved from a combination of FK2ActionMenuBuilder::CreateAddComponentAction()
// and FEdGraphSchemaAction_K2AddComponent::PerformAction().
UEdGraphNode* UBlueprintComponentNodeSpawner::Invoke(UEdGraph* ParentGraph, FBindingSet const& Bindings, FVector2D const Location) const
{
	UClass* ComponentType = ComponentClass;
	auto PostSpawnLambda = [ComponentType](UEdGraphNode* NewNode, bool bIsTemplateNode, FCustomizeNodeDelegate UserDelegate)
	{		
		UK2Node_AddComponent* AddCompNode = CastChecked<UK2Node_AddComponent>(NewNode);
		UBlueprint* Blueprint = AddCompNode->GetBlueprint();
		
		UFunction* AddComponentFunc = FindFieldChecked<UFunction>(AActor::StaticClass(), UK2Node_AddComponent::GetAddComponentFunctionName());
		AddCompNode->FunctionReference.SetFromField<UFunction>(AddComponentFunc, !bIsTemplateNode && FBlueprintEditorUtils::IsActorBased(Blueprint));

		AddCompNode->TemplateType = ComponentType;

		UserDelegate.ExecuteIfBound(NewNode, bIsTemplateNode);
	};

	FCustomizeNodeDelegate PostSpawnDelegate = FCustomizeNodeDelegate::CreateLambda(PostSpawnLambda, CustomizeNodeDelegate);
	// let SpawnNode() allocate default pins (so we can modify them)
	UK2Node_AddComponent* NewNode = Super::SpawnNode<UK2Node_AddComponent>(NodeClass, ParentGraph, FBindingSet(), Location, PostSpawnDelegate);
	if (NewNode->Pins.Num() == 0)
	{
		NewNode->AllocateDefaultPins();
	}

	// set the return type to be the type of the template
	UEdGraphPin* ReturnPin = NewNode->GetReturnValuePin();
	if (ReturnPin != nullptr)
	{
		if (ComponentClass != nullptr)
		{
			ReturnPin->PinType.PinSubCategoryObject = ComponentType;
		}
		else
		{
			ReturnPin->PinType.PinSubCategoryObject = UActorComponent::StaticClass();
		}
	}

	bool const bIsTemplateNode = FBlueprintNodeTemplateCache::IsTemplateOuter(ParentGraph);
	if (!bIsTemplateNode)
	{
		TSubclassOf<UActorComponent> Class = ComponentClass;
		if (Class == nullptr)
		{
			const ELoadFlags LoadFlags = LOAD_None;
			UBlueprint* LoadedObject = LoadObject<UBlueprint>(NULL, *ComponentAssetName, NULL, LoadFlags, NULL);
			if (LoadedObject == nullptr)
			{
				return nullptr;
			}

			Class = TSubclassOf<UActorComponent>(Cast<UBlueprintGeneratedClass>(LoadedObject->GeneratedClass));
			if (Class == nullptr)
			{
				return nullptr;
			}
		}

		UBlueprint* Blueprint = NewNode->GetBlueprint();

		FName DesiredComponentName = NewNode->MakeNewComponentTemplateName(Blueprint->GeneratedClass, Class);
		UActorComponent* ComponentTemplate = NewObject<UActorComponent>(Blueprint->GeneratedClass, Class, DesiredComponentName, RF_ArchetypeObject | RF_Public | RF_Transactional);

		Blueprint->ComponentTemplates.Add(ComponentTemplate);

		// set the name of the template as the default for the TemplateName param
		UEdGraphPin* TemplateNamePin = NewNode->GetTemplateNamePinChecked();
		if (TemplateNamePin != nullptr)
		{
			TemplateNamePin->DefaultValue = ComponentTemplate->GetName();
		}
		NewNode->ReconstructNode();
	}

	// apply bindings, after we've setup the template pin
	ApplyBindings(NewNode, Bindings);

	return NewNode;
}

//------------------------------------------------------------------------------
FBlueprintActionUiSpec UBlueprintComponentNodeSpawner::GetUiSpec(FBlueprintActionContext const& Context, FBindingSet const& Bindings) const
{
	UEdGraph* TargetGraph = (Context.Graphs.Num() > 0) ? Context.Graphs[0] : nullptr;
	FBlueprintActionUiSpec MenuSignature = PrimeDefaultUiSpec(TargetGraph);

	if (Bindings.Num() > 0)
	{
		FText AssetName;
		if (UObject* AssetBinding = Bindings.CreateConstIterator()->Get())
		{
			AssetName = FText::FromName(AssetBinding->GetFName());
		}

		FText const ComponentTypeName = FText::FromName(ComponentClass->GetFName());
		MenuSignature.MenuName = FText::Format(LOCTEXT("AddBoundComponentMenuName", "Add {0} (as {1})"), AssetName, ComponentTypeName);
		MenuSignature.Tooltip  = FText::Format(LOCTEXT("AddBoundComponentTooltip", "Spawn {0} using {1}"), ComponentTypeName, AssetName);
	}
	DynamicUiSignatureGetter.ExecuteIfBound(Context, Bindings, &MenuSignature);
	return MenuSignature;
}

//------------------------------------------------------------------------------
bool UBlueprintComponentNodeSpawner::IsBindingCompatible(UObject const* BindingCandidate) const
{
	bool bCanBindWith = false;
	if (BindingCandidate->IsAsset())
	{
		TArray< TSubclassOf<UActorComponent> > ComponentClasses = FComponentAssetBrokerage::GetComponentsForAsset(BindingCandidate);
		bCanBindWith = ComponentClasses.Contains(ComponentClass);
	}
	return bCanBindWith;
}

//------------------------------------------------------------------------------
bool UBlueprintComponentNodeSpawner::BindToNode(UEdGraphNode* Node, UObject* Binding) const
{
	bool bSuccessfulBinding = false;
	UK2Node_AddComponent* AddCompNode = CastChecked<UK2Node_AddComponent>(Node);

	UActorComponent* ComponentTemplate = AddCompNode->GetTemplateFromNode();
	if (ComponentTemplate != nullptr)
	{
		bSuccessfulBinding = FComponentAssetBrokerage::AssignAssetToComponent(ComponentTemplate, Binding);
		AddCompNode->ReconstructNode();
	}
	return bSuccessfulBinding;
}

//------------------------------------------------------------------------------
TSubclassOf<UActorComponent> UBlueprintComponentNodeSpawner::GetComponentClass() const
{
	return ComponentClass;
}

#undef LOCTEXT_NAMESPACE
