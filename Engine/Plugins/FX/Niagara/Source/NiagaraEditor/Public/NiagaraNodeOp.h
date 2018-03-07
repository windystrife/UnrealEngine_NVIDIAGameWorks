// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#include "NiagaraEditorCommon.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SOverlay.h"
#include "NiagaraNode.h"
#include "NiagaraNodeOp.generated.h"

class SGraphNode;
class SGraphPin;
class SVerticalBox;

UCLASS(MinimalAPI)
class UNiagaraNodeOp : public UNiagaraNode
{
	GENERATED_UCLASS_BODY()

public:

	/** Name of operation */
	UPROPERTY()
	FName OpName;

	//~ Begin EdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	//~ End EdGraphNode Interface

	//~ Begin UNiagaraNode Interface
	virtual void Compile(class FHlslNiagaraTranslator* Translator, TArray<int32>& Outputs) override;
	virtual bool RefreshFromExternalChanges() override;
	virtual ENiagaraNumericOutputTypeSelectionMode GetNumericOutputTypeSelectionMode() const override;
	//~ End UNiagaraNode Interface
};



#if 0

class SGraphNodeNiagara : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeNiagara){}
	SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, class UNiagaraNode* InNode);

	//~ Begin SGraphNode Interface
	virtual void CreatePinWidgets() override;
	//~ End SGraphNode Interface

	//~ Begin SNodePanel::SNode Interface
	virtual void MoveTo(const FVector2D& NewPosition, FNodeSet& NodeFilter) override;
	//~ End SNodePanel::SNode Interface

	UNiagaraNode* GetNiagaraGraphNode() const { return NiagaraNode; }

protected:
	//~ Begin SGraphNode Interface
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;
	virtual void CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox) override;
	virtual void SetDefaultTitleAreaWidget(TSharedRef<SOverlay> DefaultTitleAreaWidget) override;
	virtual TSharedRef<SWidget> CreateNodeContentArea() override;
	//~ End SGraphNode Interface

	/** Creates a preview viewport if necessary */
	//TSharedRef<SWidget> CreatePreviewWidget();

	/** Returns visibility of Expression Preview viewport */
	//EVisibility ExpressionPreviewVisibility() const;

	/** Show/hide Expression Preview */
	//void OnExpressionPreviewChanged(const ECheckBoxState NewCheckedState);

	/** hidden == unchecked, shown == checked */
	//ECheckBoxState IsExpressionPreviewChecked() const;

	/** Up when shown, down when hidden */
	//const FSlateBrush* GetExpressionPreviewArrow() const;

private:
	/** Slate viewport for rendering preview via custom slate element */
	//TSharedPtr<FPreviewViewport> PreviewViewport;

	/** Cached material graph node pointer to avoid casting */
	UNiagaraNode* NiagaraNode;
};

#endif
