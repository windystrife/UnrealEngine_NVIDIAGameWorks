// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphPin.h"
#include "NiagaraNode.h"
#include "SInlineEditableTextBlock.h"

/** A graph pin widget for allowing a pin to have an editable name for a pin. */
template< class BaseClass >
class TNiagaraGraphPinEditableName : public BaseClass
{
public:
	SLATE_BEGIN_ARGS(TNiagaraGraphPinEditableName<BaseClass>) {}
	SLATE_END_ARGS()

	FORCENOINLINE void Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
	{
		bPendingRename = false;
		BaseClass::Construct(BaseClass::FArguments(), InGraphPinObj);
	}

protected:
	FText GetParentPinLabel() const
	{
		return BaseClass::GetPinLabel();
	}

	EVisibility GetParentPinVisibility() const
	{
		return BaseClass::GetPinLabelVisibility();
	}

	FSlateColor GetParentPinTextColor() const
	{
		return BaseClass::GetPinTextColor();
	}

	bool OnVerifyTextChanged(const FText& InName, FText& OutErrorMessage) const
	{
		UNiagaraNode* ParentNode = Cast<UNiagaraNode>(this->GraphPinObj->GetOwningNode());
		if (ParentNode)
		{
			return ParentNode->VerifyEditablePinName(InName, OutErrorMessage, this->GraphPinObj);
		}
		return false;
	}

	void OnTextCommitted(const FText& InText, ETextCommit::Type InCommitType)
	{
		UNiagaraNode* ParentNode = Cast<UNiagaraNode>(this->GraphPinObj->GetOwningNode());
		if (ParentNode != nullptr)
		{
			ParentNode->CommitEditablePinName(InText, this->GraphPinObj);
		}
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		if (bPendingRename && CreatedTextBlock.IsValid())
		{
			CreatedTextBlock->EnterEditingMode();
			bPendingRename = false;
		}
		BaseClass::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	}

	virtual TSharedRef<SWidget> GetLabelWidget(const FName& InLabelStyle) override
	{
		UNiagaraNode* ParentNode = Cast<UNiagaraNode>(this->GraphPinObj->GetOwningNode());
		if (ParentNode && ParentNode->IsPinNameEditable(this->GraphPinObj))
		{
			CreatedTextBlock = SNew(SInlineEditableTextBlock)
				.Style(&FEditorStyle::Get().GetWidgetStyle<FInlineEditableTextBlockStyle>("Graph.Node.InlineEditablePinName"))
				.Text(this, &TNiagaraGraphPinEditableName<BaseClass>::GetParentPinLabel)
				.Visibility(this, &TNiagaraGraphPinEditableName<BaseClass>::GetParentPinVisibility)
				.ColorAndOpacity(this, &TNiagaraGraphPinEditableName<BaseClass>::GetParentPinTextColor)
				.OnVerifyTextChanged(this, &TNiagaraGraphPinEditableName<BaseClass>::OnVerifyTextChanged)
				.OnTextCommitted(this, &TNiagaraGraphPinEditableName<BaseClass>::OnTextCommitted);

			TSharedRef<SWidget> EditableTextBlock = CreatedTextBlock.ToSharedRef();

			if (ParentNode->IsPinNameEditableUponCreation(this->GraphPinObj))
			{
				bPendingRename = true;
			}
			return EditableTextBlock;
		}
		else
		{
			return BaseClass::GetLabelWidget(InLabelStyle);
		}
	}

	bool bPendingRename;
	TSharedPtr<SInlineEditableTextBlock> CreatedTextBlock;
};