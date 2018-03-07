// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_CallFunction.h"
#include "K2Node_AddComponent.generated.h"

class UActorComponent;

UCLASS(MinimalAPI)
class UK2Node_AddComponent : public UK2Node_CallFunction
{
	GENERATED_UCLASS_BODY()

	/** Prefix used for component template object name. */
	BLUEPRINTGRAPH_API static const FString ComponentTemplateNamePrefix;

	UPROPERTY()
	uint32 bHasExposedVariable:1;

	/** The blueprint name we came from, so we can lookup the template after a paste */
	UPROPERTY()
	FString TemplateBlueprint;

	UPROPERTY()
	UClass* TemplateType;

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual void DestroyNode() override;
	virtual void PrepareForCopying() override;
	virtual void PostPasteNode() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FString GetDocumentationLink() const override;
	virtual FString GetDocumentationExcerptName() const override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	virtual void ReconstructNode() override;
	//~ End UEdGraphNode Interface

	//~ Begin K2Node Interface
	virtual void PostReconstructNode() override;
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	//~ End UK2Node Interface

	BLUEPRINTGRAPH_API void AllocateDefaultPinsWithoutExposedVariables();
	BLUEPRINTGRAPH_API void AllocatePinsForExposedVariables();

	UEdGraphPin* GetTemplateNamePinChecked() const
	{
		UEdGraphPin* FoundPin = GetTemplateNamePin();
		check(FoundPin != NULL);
		return FoundPin;
	}

	UEdGraphPin* GetRelativeTransformPin() const
	{
		return FindPinChecked(TEXT("RelativeTransform"));
	}

	UEdGraphPin* GetManualAttachmentPin() const
	{
		return FindPinChecked(TEXT("bManualAttachment"));
	}

	/** Tries to get a template object from this node. */
	BLUEPRINTGRAPH_API UActorComponent* GetTemplateFromNode() const;

	/** Helper method used to generate a new, unique component template name. */
	FName MakeNewComponentTemplateName(UObject* InOuter, UClass* InComponentClass);

	/** Helper method used to instantiate a new component template after duplication. */
	BLUEPRINTGRAPH_API void MakeNewComponentTemplate();

	/** Static name of function to call */
	static BLUEPRINTGRAPH_API FName GetAddComponentFunctionName();

private: 
	UEdGraphPin* GetTemplateNamePin() const
	{
		return FindPin(TEXT("TemplateName"));
	}

	UClass* GetSpawnedType() const;	
};



