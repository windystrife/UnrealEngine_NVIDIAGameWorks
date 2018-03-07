// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/DragAndDrop.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SToolTip.h"

/** Class that handles the drag dropping of the specified stat in the profiler. */
class FStatIDDragDropOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FStatIDDragDropOp, FDragDropOperation)

	const TArray<int32>& GetStatIDs() const
	{
		return StatIDs;
	}

	const int32 GetSingleStatID() const
	{
		return IsSingleStatID() ? StatIDs[0] : -1;
	}

	const bool IsSingleStatID() const
	{
		return StatIDs.Num() == 1;
	}

	void ShowOK()
	{
		bShowOkIcon = true;
	}

	void ShowError()
	{
		bShowOkIcon = false;
	}

	static TSharedRef<FStatIDDragDropOp> NewGroup( const TArray<int32>& StatIDs, const FString GroupDesc )
	{
		TSharedRef<FStatIDDragDropOp> Operation = MakeShareable(new FStatIDDragDropOp());

		Operation->StatIDs.Append( StatIDs );
		Operation->Description = GroupDesc;
		Operation->bShowOkIcon = false;
		Operation->Construct();
		return Operation;
	}

	static TSharedRef<FStatIDDragDropOp> NewSingle( const int32 StatID, const FString StatDesc )
	{
		TSharedRef<FStatIDDragDropOp> Operation = MakeShareable(new FStatIDDragDropOp());

		Operation->StatIDs.Add( StatID );
		Operation->Description = StatDesc;
		Operation->bShowOkIcon = false;
		Operation->Construct();
		return Operation;
	}

private:
	/** Gets the widget that will serve as the decorator unless overridden. If you do not override, you will have no decorator. */
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		return 
			
		SNew(SToolTip)
		[		
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.Padding( 0.0f, 0.0f, 3.0f, 0.0f )
			[
				SNew( SImage )
				.Image( this, &FStatIDDragDropOp::GetIcon )
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign( VAlign_Center )
			.Padding( 0.0f, 0.0f, 3.0f, 0.0f )
			[
				SNew( SImage )
				.Visibility( !IsSingleStatID() ? EVisibility::Visible : EVisibility::Collapsed )
				.Image( GetIconForGroup() )
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign( VAlign_Center )
			[
				SNew(STextBlock)
				.Text( FText::FromString(Description) )
			]
		];
	}

	static const FSlateBrush* GetIconForGroup()
	{
		return FEditorStyle::GetBrush( TEXT( "Profiler.Misc.GenericGroup" ) );
	}

	const FSlateBrush* GetIcon() const
	{
		return bShowOkIcon ? FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK")) : FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.Error"));
	}

private:
	/** Array of the stat IDs. */
	TArray<int32> StatIDs;

	/** The display name for this stat or group. */
	FString Description;

	/** Whether to show OK or Error icon. */
	bool bShowOkIcon;	
};
