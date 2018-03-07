// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SCompoundWidget.h"
#include "ClothingAssetFactoryInterface.h"
#include "IDetailCustomization.h"
#include "DeclarativeSyntaxSupport.h"
#include "Engine/SkeletalMesh.h"

DECLARE_DELEGATE_OneParam(FOnCreateClothingRequested, FSkeletalMeshClothBuildParams&);

class FClothCreateSettingsCustomization : public IDetailCustomization
{
public:

	FClothCreateSettingsCustomization(TWeakObjectPtr<USkeletalMesh> InMeshPtr, bool bInIsSubImport)
		: bIsSubImport(bInIsSubImport)
		, MeshPtr(InMeshPtr)
		, ParamsStruct(nullptr)
	{};

	static TSharedRef<IDetailCustomization> MakeInstance(TWeakObjectPtr<USkeletalMesh> MeshPtr, bool bIsSubImport)
	{
		return MakeShareable(new FClothCreateSettingsCustomization(MeshPtr, bIsSubImport));
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

protected:

	TSharedRef<SWidget> OnGetTargetAssetMenu();
	FText GetTargetAssetText() const;
	void OnAssetSelected(int32 InMeshClothingIndex);

	TSharedRef<SWidget> OnGetTargetLodMenu();
	FText GetTargetLodText() const;
	void OnLodSelected(int32 InLodIndex);
	bool CanSelectLod() const;

	bool bIsSubImport;
	TWeakObjectPtr<USkeletalMesh> MeshPtr;
	FSkeletalMeshClothBuildParams* ParamsStruct;
};

class PERSONA_API SCreateClothingSettingsPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCreateClothingSettingsPanel)
		: _LodIndex(INDEX_NONE)
		, _SectionIndex(INDEX_NONE)
		, _bIsSubImport(false)
	{}

		// Name of the mesh we're operating on
		SLATE_ARGUMENT(FString, MeshName)
		// Mesh LOD index we want to target
		SLATE_ARGUMENT(int32, LodIndex)
		// Mesh section index we want to targe
		SLATE_ARGUMENT(int32, SectionIndex)
		// Weak ptr to the mesh we're building for
		SLATE_ARGUMENT(TWeakObjectPtr<USkeletalMesh>, Mesh)
		// Whether this window is for a sub import (importing a LOD or replacing a LOD)
		SLATE_ARGUMENT(bool, bIsSubImport)
		// Callback to handle create request
		SLATE_EVENT(FOnCreateClothingRequested, OnCreateRequested)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	// Params struct to hold request data
	FSkeletalMeshClothBuildParams BuildParams;

	// Whether or not to show sub params (lod imports)
	bool bIsSubImport;

	// Create button functionality
	FText GetCreateButtonTooltip() const;
	bool CanCreateClothing() const;

	// Handlers for panel buttons
	FReply OnCreateClicked();

	// Called when the create button is clicked, so external caller can handle the request
	FOnCreateClothingRequested OnCreateDelegate;

};