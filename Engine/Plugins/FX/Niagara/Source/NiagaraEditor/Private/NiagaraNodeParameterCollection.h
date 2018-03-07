// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphPin.h"
#include "NiagaraNodeWithDynamicPins.h"
#include "NiagaraNodeParameterCollection.generated.h"

class UEdGraphPin;
class UNiagaraParameterCollection;

/** A node that allows a user to get values from a parameter collection. */
UCLASS()
class UNiagaraNodeParameterCollection : public UNiagaraNodeWithDynamicPins
{
public:
	GENERATED_BODY()

	UNiagaraNodeParameterCollection();

	//~ UObject interface
	virtual void PostEditImport()override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)override;
	virtual void PostLoad()override;
	//~ UObject interface

	//~ UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const;
	virtual FText GetTooltipText() const;
	//~ UEdGraphNode Interface

	//~ UNiagaraNode Interface
	virtual void Compile(class FHlslNiagaraTranslator* Translator, TArray<int32>& Outputs);
	virtual UObject* GetReferencedAsset()const;
	virtual bool RefreshFromExternalChanges()override;
	//~ UNiagaraNode Interface

	//~ UNiagaraNodeWithDynamicPins Interface
	virtual bool CanRenamePin(const UEdGraphPin* GraphPinObj) const override;
	virtual bool CanMovePin(const UEdGraphPin* Pin) const override { return false; }
	virtual TSharedRef<SWidget> GenerateAddPinMenu(const FString& InWorkingPinName, SNiagaraGraphPinAdd* InPin) override;
protected:
	virtual void RemoveDynamicPin(UEdGraphPin* Pin)override;
public:
	//~ UNiagaraNodeWithDynamicPins Interface


	//virtual void BuildParameterMapHistory(const UEdGraphPin* ParameterMapPinFromNode, FNiagaraParameterMapHistory& OutHistory, bool bRecursive = true) override;

	UNiagaraParameterCollection* GetCollection()const { return Collection; }
	/**
	* A path to a collection asset.
	* This is used so that the nodes can be populated in the graph context
	* menu without having to load all of the actual collection assets.
	*/
	UPROPERTY(Transient)
	FName CollectionAssetObjectPath;

protected:

	virtual void OnNewTypedPinAdded(UEdGraphPin* NewPin);

	UPROPERTY(EditAnywhere, Category=Collection)
	UNiagaraParameterCollection* Collection;

	UPROPERTY()
	TArray<FNiagaraVariable> Variables;

	//Track change ID? Or just validate on load every time and on edit if already loaded.
	//UPROPERTY()
	//FGuid ChangeID;
};
