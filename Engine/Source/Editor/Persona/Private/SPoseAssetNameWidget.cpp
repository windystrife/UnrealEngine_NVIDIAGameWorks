// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SPoseAssetNameWidget.h"
#define LOCTEXT_NAMESPACE "SPoseAssetNameWidget"

#define DEFAULTTEXT TEXT("None Selected")

void SPoseAssetNameWidget::Construct(const FArguments& InArgs)
{
	PoseAsset = InArgs._PoseAsset;
	OnSelectionChanged = InArgs._OnSelectionChanged;
	ChildSlot
	[
		SAssignNew(BasePoseComboBox, SComboBox< TSharedPtr<FString> >)
			.OptionsSource(&BasePoseComboList)
			.OnGenerateWidget(this, &SPoseAssetNameWidget::MakeBasePoseComboWidget)
			.OnSelectionChanged(this, &SPoseAssetNameWidget::SelectionChanged)
			.OnComboBoxOpening(this, &SPoseAssetNameWidget::OnBasePoseComboOpening)
			.ContentPadding(3)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &SPoseAssetNameWidget::GetBasePoseComboBoxContent)
				.ToolTipText(this, &SPoseAssetNameWidget::GetBasePoseComboBoxToolTip)
			]
	];

	RefreshBasePoseChanged();
}

TSharedRef<SWidget> SPoseAssetNameWidget::MakeBasePoseComboWidget(TSharedPtr<FString> InItem)
{
	return 	SNew(STextBlock).Text(FText::FromString(*InItem));
}

void SPoseAssetNameWidget::RefreshBasePoseChanged()
{
	if (PoseAsset.IsValid())
	{
		BasePoseComboList.Reset();
		// add pose names
		TArray<FSmartName> PoseNames = PoseAsset->GetPoseNames();
		if (PoseNames.Num() > 0)
		{
			// go through profile and see if it has mine
			for (const FSmartName& PoseName : PoseNames)
			{
				BasePoseComboList.Add(MakeShareable(new FString(PoseName.DisplayName.ToString())));
			}
		}
	}

	// if nothing in combo, make sure to add empty text
	if (BasePoseComboList.Num() == 0)
	{
		BasePoseComboList.Add(MakeShareable(new FString(DEFAULTTEXT)));
	}

	// should we trigger selection? Or The SetSelectedItem should trigger it?
	BasePoseComboBox->RefreshOptions();
	// test null item
	BasePoseComboBox->SetSelectedItem(BasePoseComboList[0]);
}

void SPoseAssetNameWidget::OnBasePoseComboOpening()
{
	// do I have to do this?
	TSharedPtr<FString> ComboStringPtr = BasePoseComboBox->GetSelectedItem();
	if (ComboStringPtr.IsValid())
	{
		BasePoseComboBox->SetSelectedItem(ComboStringPtr);
	}
}

FText SPoseAssetNameWidget::GetBasePoseComboBoxContent() const
{
	if (BasePoseComboList.Num() > 0)
	{
		return FText::FromString(*BasePoseComboBox->GetSelectedItem().Get());
	}

	return FText();
}

FText SPoseAssetNameWidget::GetBasePoseComboBoxToolTip() const
{
	return LOCTEXT("BasePoseComboToolTip", "Select Pose");
}

void SPoseAssetNameWidget::SetPoseAsset(UPoseAsset* NewPoseAsset)
{
	PoseAsset = NewPoseAsset;

	RefreshBasePoseChanged();
}

void SPoseAssetNameWidget::SelectionChanged(TSharedPtr<FString> PoseName, ESelectInfo::Type SelectionType)
{
	if (PoseName.IsValid() && *PoseName.Get() != DEFAULTTEXT)
	{
		OnSelectionChanged.ExecuteIfBound(PoseName, SelectionType);
	}
}
#undef LOCTEXT_NAMESPACE

