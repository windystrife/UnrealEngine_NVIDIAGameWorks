// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Styling/SlateColor.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "Editor/LandscapeEditor/Private/LandscapeEdMode.h"
#include "LandscapeFileFormatInterface.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyHandle.h"
#include "Editor/LandscapeEditor/Private/LandscapeEditorDetailCustomization_Base.h"

class IDetailLayoutBuilder;

/**
 * Slate widgets customizer for the "New Landscape" tool
 */

class FLandscapeEditorDetailCustomization_NewLandscape : public FLandscapeEditorDetailCustomization_Base
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

public:
	static void SetScale(float NewValue, ETextCommit::Type, TSharedRef<IPropertyHandle> PropertyHandle);

	static TSharedRef<SWidget> GetSectionSizeMenu(TSharedRef<IPropertyHandle> PropertyHandle);
	static void OnChangeSectionSize(TSharedRef<IPropertyHandle> PropertyHandle, int32 NewSize);
	static FText GetSectionSize(TSharedRef<IPropertyHandle> PropertyHandle);

	static TSharedRef<SWidget> GetSectionsPerComponentMenu(TSharedRef<IPropertyHandle> PropertyHandle);
	static void OnChangeSectionsPerComponent(TSharedRef<IPropertyHandle> PropertyHandle, int32 NewSize);
	static FText GetSectionsPerComponent(TSharedRef<IPropertyHandle> PropertyHandle);

	TOptional<int32> GetLandscapeResolutionX() const;
	void OnChangeLandscapeResolutionX(int32 NewValue);
	void OnCommitLandscapeResolutionX(int32 NewValue, ETextCommit::Type CommitInfo);

	TOptional<int32> GetLandscapeResolutionY() const;
	void OnChangeLandscapeResolutionY(int32 NewValue);
	void OnCommitLandscapeResolutionY(int32 NewValue, ETextCommit::Type CommitInfo);

	TOptional<int32> GetMinLandscapeResolution() const;
	TOptional<int32> GetMaxLandscapeResolution() const;

	FText GetTotalComponentCount() const;

	FReply OnCreateButtonClicked();
	FReply OnFillWorldButtonClicked();

	static EVisibility GetVisibilityOnlyInNewLandscapeMode(ENewLandscapePreviewMode::Type value);
	ECheckBoxState NewLandscapeModeIsChecked(ENewLandscapePreviewMode::Type value) const;
	void OnNewLandscapeModeChanged(ECheckBoxState NewCheckedState, ENewLandscapePreviewMode::Type value);

	// Import
	static EVisibility GetHeightmapErrorVisibility(TSharedRef<IPropertyHandle> PropertyHandle_HeightmapImportResult);
	static FSlateColor GetHeightmapErrorColor(TSharedRef<IPropertyHandle> PropertyHandle_HeightmapImportResult);
	static void SetImportHeightmapFilenameString(const FText& NewValue, ETextCommit::Type CommitInfo, TSharedRef<IPropertyHandle> PropertyHandle_HeightmapFilename);
	void OnImportHeightmapFilenameChanged();
	static FReply OnImportHeightmapFilenameButtonClicked(TSharedRef<IPropertyHandle> PropertyHandle_HeightmapFilename);

	TSharedRef<SWidget> GetImportLandscapeResolutionMenu();
	void OnChangeImportLandscapeResolution(int32 NewConfigIndex);
	FText GetImportLandscapeResolution() const;

	bool GetImportButtonIsEnabled() const;
	FReply OnFitImportDataButtonClicked();
	void ChooseBestComponentSizeForImport(FEdModeLandscape* LandscapeEdMode);

	FText GetOverallResolutionTooltip() const;

	// Import layers
	EVisibility GetMaterialTipVisibility() const;

protected:
	static const int32 SectionSizes[];
	static const int32 NumSections[];

	TArray<FLandscapeFileResolution> ImportResolutions;
};

class FLandscapeEditorStructCustomization_FLandscapeImportLayer : public FLandscapeEditorStructCustomization_Base
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

public:
	static FReply OnLayerFilenameButtonClicked(TSharedRef<IPropertyHandle> PropertyHandle_LayerFilename);
	static bool ShouldFilterLayerInfo(const struct FAssetData& AssetData, FName LayerName);

	static EVisibility GetImportLayerCreateVisibility(TSharedRef<IPropertyHandle> PropertyHandle_LayerInfo);
	static TSharedRef<SWidget> OnGetImportLayerCreateMenu(TSharedRef<IPropertyHandle> PropertyHandle_LayerInfo, FName LayerName);
	static void OnImportLayerCreateClicked(TSharedRef<IPropertyHandle> PropertyHandle_LayerInfo, FName LayerName, bool bNoWeightBlend);

	static EVisibility GetErrorVisibility(TSharedRef<IPropertyHandle> PropertyHandle_ImportResult);
	static FSlateColor GetErrorColor(TSharedRef<IPropertyHandle> PropertyHandle_ImportResult);
	static FText GetErrorText(TSharedRef<IPropertyHandle> PropertyHandle_ErrorMessage);
};
