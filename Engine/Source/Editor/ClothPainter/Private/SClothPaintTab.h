// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetEditorToolkit.h"

class UClothingAsset;
class SClothPaintWidget;
class SClothAssetSelector;
class IDetailsView;
class ISkeletalMeshEditor;
class IPersonaToolkit;

class CLOTHPAINTER_API SClothPaintTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SClothPaintTab)
	{}	
	SLATE_ARGUMENT(TWeakPtr<class FAssetEditorToolkit>, InHostingApp)
	SLATE_END_ARGS()

	SClothPaintTab();
	~SClothPaintTab();

	/** SWidget functions */
	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	/** Toggles cloth paint mode */
	void TogglePaintMode();

	/** Gets whether cloth paint mode is active */
	bool IsPaintModeActive() const;

protected:

	/** Called as the tool selection changes to enable/disable painting */
	void UpdatePaintTools();

	/** Called from the selector when the asset selection changes (Asset, LOD, Mask) */
	void OnAssetSelectionChanged(TWeakObjectPtr<UClothingAsset> InAssetPtr, int32 InLodIndex, int32 InMaskIndex);

	/** Whether or not the asset config section is enabled for editing */
	bool IsAssetDetailsPanelEnabled();

	/** Helpers for getting editor objects */
	ISkeletalMeshEditor* GetSkeletalMeshEditor() const;
	TSharedRef<IPersonaToolkit> GetPersonaToolkit() const;

	TWeakPtr<class FAssetEditorToolkit> HostingApp;
	
	TSharedPtr<SClothAssetSelector> SelectorWidget;
	TSharedPtr<SClothPaintWidget> ModeWidget;
	TSharedPtr<SVerticalBox> ContentBox;
	TSharedPtr<IDetailsView> DetailsView;

	bool bModeApplied;
	bool bPaintModeEnabled;
};