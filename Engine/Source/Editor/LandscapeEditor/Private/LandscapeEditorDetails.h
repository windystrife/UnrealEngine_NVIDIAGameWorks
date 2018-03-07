// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Textures/SlateIcon.h"
#include "Widgets/SWidget.h"
#include "UnrealClient.h"
#include "IDetailCustomization.h"
#include "Editor/LandscapeEditor/Private/LandscapeEditorDetailCustomization_Base.h"

class FLandscapeEditorDetailCustomization_AlphaBrush;
class FLandscapeEditorDetailCustomization_CopyPaste;
class FLandscapeEditorDetailCustomization_MiscTools;
class FLandscapeEditorDetailCustomization_NewLandscape;
class FLandscapeEditorDetailCustomization_ResizeLandscape;
class FLandscapeEditorDetailCustomization_TargetLayers;
class FUICommandList;
class IDetailLayoutBuilder;
class ULandscapeInfo;

class FLandscapeEditorDetails : public FLandscapeEditorDetailCustomization_Base
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

protected:
	static FText GetLocalizedName(FString Name);

	static EVisibility GetTargetLandscapeSelectorVisibility();
	static FText GetTargetLandscapeName();
	static TSharedRef<SWidget> GetTargetLandscapeMenu();
	static void OnChangeTargetLandscape(TWeakObjectPtr<ULandscapeInfo> LandscapeInfo);

	FText GetCurrentToolName() const;
	FSlateIcon GetCurrentToolIcon() const;
	TSharedRef<SWidget> GetToolSelector();
	bool GetToolSelectorIsVisible() const;
	EVisibility GetToolSelectorVisibility() const;

	FText GetCurrentBrushName() const;
	FSlateIcon GetCurrentBrushIcon() const;
	TSharedRef<SWidget> GetBrushSelector();
	bool GetBrushSelectorIsVisible() const;

	FText GetCurrentBrushFalloffName() const;
	FSlateIcon GetCurrentBrushFalloffIcon() const;
	TSharedRef<SWidget> GetBrushFalloffSelector();
	bool GetBrushFalloffSelectorIsVisible() const;

	TSharedPtr<FUICommandList> CommandList;

	TSharedPtr<FLandscapeEditorDetailCustomization_NewLandscape> Customization_NewLandscape;
	TSharedPtr<FLandscapeEditorDetailCustomization_ResizeLandscape> Customization_ResizeLandscape;
	TSharedPtr<FLandscapeEditorDetailCustomization_CopyPaste> Customization_CopyPaste;
	TSharedPtr<FLandscapeEditorDetailCustomization_MiscTools> Customization_MiscTools;
	TSharedPtr<FLandscapeEditorDetailCustomization_AlphaBrush> Customization_AlphaBrush;
	TSharedPtr<FLandscapeEditorDetailCustomization_TargetLayers> Customization_TargetLayers;
};
