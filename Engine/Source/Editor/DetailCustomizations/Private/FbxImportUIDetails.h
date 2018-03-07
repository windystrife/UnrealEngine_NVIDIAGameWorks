// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "Types/SlateEnums.h"
#include "IDetailCustomization.h"
#include "Factories/FbxImportUI.h"
#include "EditorUndoClient.h"

class IDetailLayoutBuilder;
class IDetailPropertyRow;
class IPropertyHandle;

class FFbxImportUIDetails : public IDetailCustomization, public FEditorUndoClient
{
public:
	~FFbxImportUIDetails();

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;
	/** End IDetailCustomization interface */

	void RefreshCustomDetail();

	/** FEditorUndoClient interface */
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	/** End FEditorUndoClient interface */

	void CollectChildPropertiesRecursive(TSharedPtr<IPropertyHandle> Node, TArray<TSharedPtr<IPropertyHandle>>& OutProperties);
	
	void ConstructBaseMaterialUI(TSharedPtr<IPropertyHandle> Handle, class IDetailCategoryBuilder& MaterialCategory);

	/** Checks whether a metadata string is valid for a given import type 
	* @param ImportType the type of mesh being imported
	* @param MetaData the metadata string to validate
	*/
	bool IsImportTypeMetaDataValid(EFBXImportType& ImportType, FString& MetaData);
	
	/** Called if the bAutoComputeLodDistances changes */
	void ImportAutoComputeLodDistancesChanged();

	/** Called if the bImportMaterials changes */
	void ImportMaterialsChanged();

	/** Called if the mesh mode (static / skeletal) changes */
	void MeshImportModeChanged();

	/** Called if the import mesh option for skeletal meshes is changed */
	void ImportMeshToggleChanged();

	/** Called when the base material is changed */
	void BaseMaterialChanged();

	/** Called user chooses base material properties */
	void OnBaseColor(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);
	void OnDiffuseTextureColor(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);
	void OnNormalTextureColor(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);
	void OnEmmisiveTextureColor(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);
	void OnEmissiveColor(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);
	void OnSpecularTextureColor(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo);
	
	TWeakObjectPtr<UFbxImportUI> ImportUI;		// The UI data object being customised
	IDetailLayoutBuilder* CachedDetailBuilder;	// The detail builder for this cusomtomisation

private:

	/** Use MakeInstance to create an instance of this class */
	FFbxImportUIDetails();

	/** Sets a custom widget for the StaticMeshLODGroup property */
	void SetStaticMeshLODGroupWidget(IDetailPropertyRow& PropertyRow, const TSharedPtr<IPropertyHandle>& Handle);

	/** Called when the StaticMeshLODGroup spinbox is changed */
	void OnLODGroupChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo, TWeakPtr<IPropertyHandle> HandlePtr);

	/** Called to determine the visibility of the VertexOverrideColor property */
	bool GetVertexOverrideColorEnabledState() const;

	/** LOD group options. */
	TArray<FName> LODGroupNames;
	TArray<TSharedPtr<FString>> LODGroupOptions;

	/** Cached StaticMeshLODGroup property handle */
	TSharedPtr<IPropertyHandle> StaticMeshLODGroupPropertyHandle;

	/** Cached VertexColorImportOption property handle */
	TSharedPtr<IPropertyHandle> VertexColorImportOptionHandle;

	TArray< TSharedPtr< FString > > BaseColorNames;
	TArray< TSharedPtr< FString > > BaseTextureNames;
};
