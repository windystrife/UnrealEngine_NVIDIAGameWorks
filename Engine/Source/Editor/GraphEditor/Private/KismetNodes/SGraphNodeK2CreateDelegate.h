// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "KismetNodes/SGraphNodeK2Base.h"

class ITableRow;
class SComboButton;
class STableViewBase;
class SVerticalBox;
class UK2Node;

class SGraphNodeK2CreateDelegate : public SGraphNodeK2Base
{
public:
	SLATE_BEGIN_ARGS(SGraphNodeK2CreateDelegate) {}
	SLATE_END_ARGS()

		struct FFunctionItemData
	{
		FName Name;
		FString Description;
	};
	TArray<TSharedPtr<FFunctionItemData>> FunctionDataItems;
	TWeakPtr<SComboButton> SelectFunctionWidget;
public:
	virtual ~SGraphNodeK2CreateDelegate();
	void Construct(const FArguments& InArgs, UK2Node* InNode);
	virtual void CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox) override;

protected:
	static FString FunctionDescription(const UFunction* Function, const bool bOnlyDescribeSignature = false, const int32 CharacterLimit = 32);

	FText GetCurrentFunctionDescription() const;
	TSharedRef<ITableRow> HandleGenerateRowFunction(TSharedPtr<FFunctionItemData> FunctionItemData, const TSharedRef<STableViewBase>& OwnerTable);
	void OnFunctionSelected(TSharedPtr<FFunctionItemData> FunctionItemData, ESelectInfo::Type SelectInfo);
};

