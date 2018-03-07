// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"

class UBlastMesh;
class SButton;
class FReply;

DECLARE_DELEGATE_OneParam(FOnDepthFilterChanged, int32);

/** GUI Widget for depth level filtering  */
class SBlastDepthFilter : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SBlastDepthFilter)
	{
	}
		SLATE_ATTRIBUTE(FText, Text)

		SLATE_ATTRIBUTE(bool, IsMultipleSelection)

		SLATE_EVENT(FOnDepthFilterChanged, OnDepthFilterChanged)
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs);
	
	void SetBlastMesh(UBlastMesh* InBlastMesh);
	void Refresh();

	const TArray<int32>& GetSelectedDepths() const;
	void SetSelectedDepths(const TArray<int32>& InPreviewDepthDepth);

private:

	FOnDepthFilterChanged OnDepthFilterChanged;

	FReply OnButtonClicked(int32 i);
	
private:

	TAttribute<FText>									Text;

	TAttribute<bool>									IsMultipleSelection;
	
	TSharedPtr<SWidget>									AllDepthsWidget;
	TArray<TSharedPtr<SButton>>							AllDepthsButtons;
	TSharedPtr<SButton>									ShowAllDepthsButton;
	TArray<TSharedPtr<SButton>>							FixedDepthsButtons;
	TSharedPtr<SButton>									LeavesButton;

	/** List of depths. */
	TArray<TSharedPtr<FString>>							FilterDepths;

	TArray<int32>										SelectedDepths;

	UBlastMesh*											BlastMesh;

};
