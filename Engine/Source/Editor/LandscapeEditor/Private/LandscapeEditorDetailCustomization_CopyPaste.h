// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"
#include "PropertyHandle.h"
#include "Editor/LandscapeEditor/Private/LandscapeEditorDetailCustomization_Base.h"

class IDetailLayoutBuilder;

/**
 * Slate widgets customizer for the "Copy/Paste" tool
 */

class FLandscapeEditorDetailCustomization_CopyPaste : public FLandscapeEditorDetailCustomization_Base
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

public:
	static FReply OnCopyToGizmoButtonClicked();
	static FReply OnFitGizmoToSelectionButtonClicked();
	static FReply OnFitHeightsToGizmoButtonClicked();
	static FReply OnClearGizmoDataButtonClicked();
	static FReply OnGizmoHeightmapFilenameButtonClicked(TSharedRef<IPropertyHandle> HeightmapPropertyHandle);

	bool GetGizmoImportButtonIsEnabled() const;
	FReply OnGizmoImportButtonClicked();
	FReply OnGizmoExportButtonClicked();
};

class FLandscapeEditorStructCustomization_FGizmoImportLayer : public FLandscapeEditorStructCustomization_Base
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

public:
	static FReply OnGizmoImportLayerFilenameButtonClicked(TSharedRef<IPropertyHandle> PropertyHandle_LayerFilename);
};
