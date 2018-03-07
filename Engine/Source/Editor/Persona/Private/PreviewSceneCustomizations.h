// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "IPersonaPreviewScene.h"
#include "IDetailCustomization.h"
#include "PropertyHandle.h"
#include "IEditableSkeleton.h"
#include "IPersonaToolkit.h"
#include "AnimationEditorPreviewScene.h"

struct FAssetData;
class FDetailWidgetRow;
class IDetailChildrenBuilder;
class IDetailLayoutBuilder;
class IPropertyUtilities;
class UPreviewMeshCollectionFactory;

class FPreviewSceneDescriptionCustomization : public IDetailCustomization
{
public:
	FPreviewSceneDescriptionCustomization(const FString& InSkeletonName, const TSharedRef<class IPersonaToolkit>& InPersonaToolkit);

	~FPreviewSceneDescriptionCustomization();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	EVisibility GetSaveButtonVisibility(TSharedRef<IPropertyHandle> AdditionalMeshesProperty) const;

	FReply OnSaveCollectionClicked(TSharedRef<IPropertyHandle> AdditionalMeshesProperty, IDetailLayoutBuilder* DetailLayoutBuilder);

	bool HandleShouldFilterAsset(const FAssetData& InAssetData, bool bCanUseDifferentSkeleton);

	bool HandleShouldFilterAdditionalMesh(const FAssetData& InAssetData, bool bCanUseDifferentSkeleton);

	void HandleAnimationModeChanged();

	void HandleAnimationChanged();

	void HandleMeshChanged(const FAssetData& InAssetData);

	void HandleAdditionalMeshesChanged(const FAssetData& InAssetData, IDetailLayoutBuilder* DetailLayoutBuilder);

	void HandleAllowDifferentSkeletonsCheckedStateChanged(ECheckBoxState CheckState);

	ECheckBoxState HandleAllowDifferentSkeletonsIsChecked() const;

private:
	/** Cached skeleton name to check for asset registry tags */
	FString SkeletonName;

	/** The persona toolkit we are associated with */
	TWeakPtr<class IPersonaToolkit> PersonaToolkit;

	/** Preview scene we will be editing */
	TWeakPtr<class FAnimationEditorPreviewScene> PreviewScene;

	/** Editable Skeleton scene we will be editing */
	TWeakPtr<class IEditableSkeleton> EditableSkeleton;

	/** Factory to use when creating mesh collections */
	UPreviewMeshCollectionFactory* FactoryToUse;

	/** This is list of class available to filter asset by. This list doesn't change once loaded, so only collect once */
	static TArray<FName> AvailableClassNameList;
};

class FPreviewMeshCollectionEntryCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FPreviewMeshCollectionEntryCustomization(nullptr));
	}

	FPreviewMeshCollectionEntryCustomization(const TSharedPtr<IPersonaPreviewScene>& InPreviewScene)
		: PreviewScene(InPreviewScene)
	{}

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override {}

private:
	bool HandleShouldFilterAsset(const FAssetData& InAssetData, FString SkeletonName);

	void HandleMeshChanged(const FAssetData& InAssetData);

	void HandleMeshesArrayChanged(TSharedPtr<IPropertyUtilities> PropertyUtilities);

private:
	/** Preview scene we will be editing */
	TWeakPtr<class IPersonaPreviewScene> PreviewScene;
};
