// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SGraphPinDataTableRowName.h"

void SGraphPinDataTableRowName::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj, class UDataTable* InDataTable)
{
	DataTable = InDataTable;
	RefreshNameList();
	SGraphPinNameList::Construct(SGraphPinNameList::FArguments(), InGraphPinObj, NameList);
}

SGraphPinDataTableRowName::SGraphPinDataTableRowName()
{
}

SGraphPinDataTableRowName::~SGraphPinDataTableRowName()
{
}

void SGraphPinDataTableRowName::PreChange(const UDataTable* Changed, FDataTableEditorUtils::EDataTableChangeInfo Info)
{
}

void SGraphPinDataTableRowName::PostChange(const UDataTable* Changed, FDataTableEditorUtils::EDataTableChangeInfo Info)
{
	//FSoftObjectPath::InvalidateTag(); // Should be removed after UE-5615 is fixed
	if (Changed && (Changed == DataTable.Get()) && (FDataTableEditorUtils::EDataTableChangeInfo::RowList == Info))
	{
		RefreshNameList();
	}
}

void SGraphPinDataTableRowName::RefreshNameList()
{
	NameList.Empty();
	if (DataTable.IsValid())
	{
		auto Names = DataTable->GetRowNames();
		for (auto Name : Names)
		{
			TSharedPtr<FName> RowNameItem = MakeShareable(new FName(Name));
			NameList.Add(RowNameItem);
		}
	}
}
