// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UnrealType.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "K2Node_Event.h"
#include "K2Node_ComponentBoundEvent.generated.h"

class UDynamicBlueprintBinding;
class UEdGraph;

UCLASS(MinimalAPI)
class UK2Node_ComponentBoundEvent : public UK2Node_Event
{
	GENERATED_UCLASS_BODY()

		/** Delegate property name that this event is associated with */
		UPROPERTY()
		FName DelegatePropertyName;

	/** Delegate property's owner class that this event is associated with */
	UPROPERTY()
		UClass* DelegateOwnerClass;

	/** Name of property in Blueprint class that pointer to component we want to bind to */
	UPROPERTY()
		FName ComponentPropertyName;

	//~ Begin UObject Interface
	virtual bool Modify(bool bAlwaysMarkDirty = true) override;
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	//~ Begin UEdGraphNode Interface
	virtual void ReconstructNode() override;
	virtual bool CanPasteHere(const UEdGraph* TargetGraph) const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FString GetDocumentationLink() const override;
	virtual FString GetDocumentationExcerptName() const override;
	//~ End UEdGraphNode Interface

	//~ Begin K2Node Interface
	virtual bool NodeCausesStructuralBlueprintChange() const override { return true; }
	virtual UClass* GetDynamicBindingClass() const override;
	virtual void RegisterDynamicBinding(UDynamicBlueprintBinding* BindingObject) const override;
	virtual void HandleVariableRenamed(UBlueprint* InBlueprint, UClass* InVariableClass, UEdGraph* InGraph, const FName& InOldVarName, const FName& InNewVarName) override;
	//~ End K2Node Interface

	virtual bool IsUsedByAuthorityOnlyDelegate() const override;

	/** Return the delegate property that this event is bound to */
	BLUEPRINTGRAPH_API UMulticastDelegateProperty* GetTargetDelegateProperty() const;

	BLUEPRINTGRAPH_API void InitializeComponentBoundEventParams(UObjectProperty const* InComponentProperty, const UMulticastDelegateProperty* InDelegateProperty);

private:
	/** Cached display name for the delegate property */
	UPROPERTY()
		FText DelegatePropertyDisplayName;

	/** Constructing FText strings can be costly, so we cache the node's title */
	FNodeTextCache CachedNodeTitle;
};

