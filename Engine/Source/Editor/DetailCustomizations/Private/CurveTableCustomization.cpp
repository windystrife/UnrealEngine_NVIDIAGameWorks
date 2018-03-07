// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CurveTableCustomization.h"

#include "Widgets/Input/SSearchBox.h"

TSharedRef<SWidget> FCurveTableCustomizationLayout::GetListContent()
{
	SAssignNew(RowNameComboListView, SListView<TSharedPtr<FString> >)
		.ListItemsSource(&RowNames)
		.OnSelectionChanged(this, &FCurveTableCustomizationLayout::OnSelectionChanged)
		.OnGenerateRow(this, &FCurveTableCustomizationLayout::HandleRowNameComboBoxGenarateWidget)
		.SelectionMode(ESelectionMode::Single);


	if (CurrentSelectedItem.IsValid())
	{
		RowNameComboListView->SetSelection(CurrentSelectedItem);
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSearchBox)
			.OnTextChanged(this, &FCurveTableCustomizationLayout::OnFilterTextChanged)
		]
	+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			RowNameComboListView.ToSharedRef()
		];
}
