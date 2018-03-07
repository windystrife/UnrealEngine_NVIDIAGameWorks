// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "SListView.h"
#include "SComboBox.h"

class USkeletalMesh;
class UClothingAsset;
struct FClothParameterMask_PhysMesh;

struct FClothingAssetListItem
{
	TWeakObjectPtr<UClothingAsset> ClothingAsset;
};

struct FClothingMaskListItem
{
	FClothingMaskListItem()
		: LodIndex(INDEX_NONE)
		, MaskIndex(INDEX_NONE)
	{}

	FClothParameterMask_PhysMesh* GetMask();

	TWeakObjectPtr<UClothingAsset> ClothingAsset;
	int32 LodIndex;
	int32 MaskIndex;
};

typedef SListView<TSharedPtr<FClothingAssetListItem>> SAssetList;
typedef SListView<TSharedPtr<FClothingMaskListItem>> SMaskList;

DECLARE_DELEGATE_ThreeParams(FOnClothAssetSelectionChanged, TWeakObjectPtr<UClothingAsset>, int32, int32);

class SClothAssetSelector : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SClothAssetSelector) {}
		SLATE_EVENT(FOnClothAssetSelectionChanged, OnSelectionChanged)
	SLATE_END_ARGS()

	~SClothAssetSelector();

	void Construct(const FArguments& InArgs, USkeletalMesh* InMesh);

	TWeakObjectPtr<UClothingAsset> GetSelectedAsset() const;
	int32 GetSelectedLod() const;
	int32 GetSelectedMask() const;

protected:

	FReply OnImportApexFileClicked();

	EVisibility GetAssetHeaderButtonTextVisibility() const;
	EVisibility GetMaskHeaderButtonTextVisibility() const;

	TSharedRef<SWidget> OnGetLodMenu();
	FText GetLodButtonText() const;

	TSharedRef<ITableRow> OnGenerateWidgetForClothingAssetItem(TSharedPtr<FClothingAssetListItem> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	void OnAssetListSelectionChanged(TSharedPtr<FClothingAssetListItem> InSelectedItem, ESelectInfo::Type InSelectInfo);

	TSharedRef<ITableRow> OnGenerateWidgetForMaskItem(TSharedPtr<FClothingMaskListItem> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	void OnMaskSelectionChanged(TSharedPtr<FClothingMaskListItem> InSelectedItem, ESelectInfo::Type InSelectInfo);

	// Mask manipulation
	FReply AddNewMask();
	bool CanAddNewMask() const;

	void OnRefresh();

	void RefreshAssetList();
	void RefreshMaskList();

	void OnClothingLodSelected(int32 InNewLod);

	// Setters for the list selections so we can handle list selections changing properly
	void SetSelectedAsset(TWeakObjectPtr<UClothingAsset> InSelectedAsset);
	void SetSelectedLod(int32 InLodIndex, bool bRefreshMasks = true);
	void SetSelectedMask(int32 InMaskIndex);

	USkeletalMesh* Mesh;

	TSharedPtr<SButton> ImportApexButton;
	TSharedPtr<SButton> NewMaskButton;
	TSharedPtr<SAssetList> AssetList;
	TSharedPtr<SMaskList> MaskList;

	TSharedPtr<SHorizontalBox> AssetHeaderBox;
	TSharedPtr<SHorizontalBox> MaskHeaderBox;

	TArray<TSharedPtr<FClothingAssetListItem>> AssetListItems;
	TArray<TSharedPtr<FClothingMaskListItem>> MaskListItems;

	// Currently selected clothing asset, Lod Index and Mask index
	TWeakObjectPtr<UClothingAsset> SelectedAsset;
	int32 SelectedLod;
	int32 SelectedMask;

	FOnClothAssetSelectionChanged OnSelectionChanged;

	// Handle for mesh event callback when clothing changes.
	FDelegateHandle MeshClothingChangedHandle;
};