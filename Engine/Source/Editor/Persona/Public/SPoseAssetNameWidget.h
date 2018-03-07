// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "Animation/PoseAsset.h"

//////////////////////////////////////////////////////////////////////////
// SBoneMappingBase

class PERSONA_API SPoseAssetNameWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPoseAssetNameWidget)
		: _PoseAsset()
		, _OnSelectionChanged()
	{}

	SLATE_ARGUMENT(UPoseAsset*, PoseAsset)
	SLATE_EVENT(SComboBox<TSharedPtr<FString>>::FOnSelectionChanged, OnSelectionChanged)
	SLATE_END_ARGS()

	/**
	* Slate construction function
	*
	* @param InArgs - Arguments passed from Slate
	*
	*/
	void Construct( const FArguments& InArgs );
	void SetPoseAsset(UPoseAsset* NewPoseAsset);
private:
	TSharedPtr<class SComboBox< TSharedPtr<FString> > > BasePoseComboBox;
	TArray< TSharedPtr< FString > >						BasePoseComboList;

	TSharedRef<SWidget> MakeBasePoseComboWidget(TSharedPtr<FString> InItem);
	FText GetBasePoseComboBoxContent() const;
	FText GetBasePoseComboBoxToolTip() const;
	void OnBasePoseComboOpening();
	void RefreshBasePoseChanged();

	TWeakObjectPtr<UPoseAsset> PoseAsset;

	SComboBox<TSharedPtr<FString>>::FOnSelectionChanged OnSelectionChanged;

	void SelectionChanged(TSharedPtr<FString> PoseName, ESelectInfo::Type SelectionType);
};
