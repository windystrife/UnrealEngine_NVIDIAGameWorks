// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "INiagaraEditorTypeUtilities.h"
#include "AssetData.h"

class UNiagaraDataInterfaceCurveBase;

/** Niagara editor utilities for the UNiagaraDataInterfaceCurve type. */
class FNiagaraDataInterfaceCurveTypeEditorUtilitiesBase : public FNiagaraEditorTypeUtilities
{
public:
	//~ FNiagaraEditorTypeUtilities interface.
	virtual bool CanCreateDataInterfaceEditor() const override;
	virtual TSharedPtr<SWidget> CreateDataInterfaceEditor(UObject* DataInterface, FNotifyValueChanged DataInterfaceChangedHandler) const override;

protected:
	/** Gets the class name used for filtering the curve asset picker. */
	virtual FName GetSupportedAssetClassName() const = 0;

	/** Imports the selected curve asset into the supplied curve data interface. */
	virtual void ImportSelectedAsset(UObject* SelectedAsset, UNiagaraDataInterfaceCurveBase* CurveDataInterface) const = 0;

private:
	/** Gets the content for the import drop down menu. */
	TSharedRef<SWidget> GetImportMenuContent(TWeakObjectPtr<UNiagaraDataInterfaceCurveBase> CurveDataInterface, FNotifyValueChanged DataInterfaceChangedHandler);

	/** Handles an asset being selected in the import menu. */
	void CurveAssetSelected(const FAssetData& AssetData, TWeakObjectPtr<UNiagaraDataInterfaceCurveBase> CurveDataInterface, FNotifyValueChanged DataInterfaceChangedHandler);
};


/** Type editor utilities for float curve data interfaces. */
class FNiagaraDataInterfaceCurveTypeEditorUtilities : public FNiagaraDataInterfaceCurveTypeEditorUtilitiesBase
{
protected:
	//~ FNiagaraDataInterfaceCurveTypeEditorUtilitiesBase interface
	virtual FName GetSupportedAssetClassName() const override;
	virtual void ImportSelectedAsset(UObject* SelectedAsset, UNiagaraDataInterfaceCurveBase* CurveDataInterface) const override;
};


/** Type editor utilities for vector curve data interfaces. */
class FNiagaraDataInterfaceVectorCurveTypeEditorUtilities : public FNiagaraDataInterfaceCurveTypeEditorUtilitiesBase
{
protected:
	//~ FNiagaraDataInterfaceCurveTypeEditorUtilitiesBase interface
	virtual FName GetSupportedAssetClassName() const override;
	virtual void ImportSelectedAsset(UObject* SelectedAsset, UNiagaraDataInterfaceCurveBase* CurveDataInterface) const override;
};


/** Type editor utilities for color curve data interfaces. */
class FNiagaraDataInterfaceColorCurveTypeEditorUtilities : public FNiagaraDataInterfaceCurveTypeEditorUtilitiesBase
{
protected:
	//~ FNiagaraDataInterfaceCurveTypeEditorUtilitiesBase interface
	virtual FName GetSupportedAssetClassName() const override;
	virtual void ImportSelectedAsset(UObject* SelectedAsset, UNiagaraDataInterfaceCurveBase* CurveDataInterface) const override;
};