// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "AssetData.h"
#include "PropertyCustomizationHelpers.h"

class URig;

/** Called when an asset is selected in the asset view */
DECLARE_DELEGATE_OneParam(FOnAssetSelected, const struct FAssetData& /*AssetData*/);

/**
 * A widget used to pick rig 
 */
class SRigPicker : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnSetReference, UObject*)

	SLATE_BEGIN_ARGS( SRigPicker )
		: _InitialObject(NULL)
	{}
		SLATE_ARGUMENT( FAssetData, InitialObject )
		SLATE_EVENT( FOnShouldFilterAsset, OnShouldFilterAsset )
		SLATE_EVENT( FOnSetReference, OnSetReference )
		SLATE_EVENT( FSimpleDelegate, OnClose )
	SLATE_END_ARGS()

	/**
	 * Construct the widget.
	 * @param	InArgs				Arguments for widget construction
	 * @param	InPropertyHandle	The property handle that this widget will operate on.
	 */
	void Construct( const FArguments& InArgs );

private:
	/** 
	 * Delegate for handling selection in the asset browser.
	 * @param	AssetData	The chosen asset data
	 */
	void OnAssetSelected( const FAssetData& AssetData );
	void OnSelectDefault();
	void OnClear();
	bool OnShouldFilterAsset(const struct FAssetData& AssetData);

	/** 
	 * Set the value of the asset referenced by this property editor.
	 * Will set the underlying property handle if there is one.
	 * @param	AssetData	The asset data for the object to set the reference to
	 */
	void SetValue( const FAssetData& AssetData );
private:
	FAssetData CurrentObject;

	/** Delegate for filtering valid assets */
	FOnShouldFilterAsset ShouldFilterAsset;

	/** Delegate to call when our object value should be set */
	FOnSetReference OnSetReference;

	/** Delegate for closing the containing menu */
	FSimpleDelegate OnClose;

	static URig* EngineHumanoidRig;
};
