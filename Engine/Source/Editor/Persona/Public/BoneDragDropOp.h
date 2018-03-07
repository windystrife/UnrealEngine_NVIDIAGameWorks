// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Input/DragAndDrop.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "EditorStyleSet.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"

class IEditableSkeleton;

//////////////////////////////////////////////////////////////////////////
// FBoneDragDropOp
class FBoneDragDropOp : public FDragDropOperation
{
public:	
	DRAG_DROP_OPERATOR_TYPE(FBoneDragDropOp, FDragDropOperation)
	
	TWeakPtr<class IEditableSkeleton> EditableSkeleton;
	FName BoneName;

	/** The widget decorator to use */
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		return SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("Graph.ConnectorFeedback.Border"))
			.Content()
			[		
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SImage)
					.Image(this, &FBoneDragDropOp::GetIcon)
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock) 
					.Text( this, &FBoneDragDropOp::GetHoverText )
				]
			];
	}

	FText GetHoverText() const
	{
		return FText::Format(NSLOCTEXT("BoneDragDropOp", "BoneHoverTextFmt", "Bone {0}"), FText::FromString(BoneName.GetPlainNameString()));
	}

	const FSlateBrush* GetIcon( ) const
	{
		return CurrentIconBrush;
	}

	void SetIcon(const FSlateBrush* InIcon)
	{
		CurrentIconBrush = InIcon;
	}

	static TSharedRef<FBoneDragDropOp> New(TSharedRef<class IEditableSkeleton> EditableSkeleton, const FName& InBoneName)
	{
		TSharedRef<FBoneDragDropOp> Operation = MakeShareable(new FBoneDragDropOp);
		Operation->BoneName = InBoneName;
		Operation->EditableSkeleton = EditableSkeleton;
		Operation->SetIcon( FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error")) );
		Operation->Construct();
		return Operation;
	}

private:
	const FSlateBrush* CurrentIconBrush;
};
