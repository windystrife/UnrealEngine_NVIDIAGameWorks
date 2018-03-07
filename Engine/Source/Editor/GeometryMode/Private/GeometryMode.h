// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Misc/NotifyHook.h"
#include "Toolkits/IToolkitHost.h"
#include "Toolkits/BaseToolkit.h"

class IDetailsView;
class SButton;
class SCheckBox;
class SUniformGridPanel;
class UGeomModifier;

/** Geometry Mode widget for controls */
class SGeometryModeControls : public SCompoundWidget, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS(SGeometryModeControls) {}
	SLATE_END_ARGS()

public:

	void SelectionChanged();

	/** SCompoundWidget functions */
	void Construct(const FArguments& InArgs);

protected:
	/** Called when a new modifier mode is selected */
	void OnModifierStateChanged(ECheckBoxState NewCheckedState, UGeomModifier* Modifier);

	/** Returns the state of a modifier radio button */
	ECheckBoxState IsModifierChecked(UGeomModifier* Modifier) const;

	/** Returns the enabled state of a modifier button */
	bool IsModifierEnabled(UGeomModifier* Modifier) const;

	/** Returns the visibility state of the properties control */
	EVisibility IsPropertiesVisible() const;

	/** Called when the Apply button is clicked */
	FReply OnApplyClicked();

	/** Called when a modifier button is clicked */
	FReply OnModifierClicked(UGeomModifier* Modifier);

private:
	/** Creates the geometry mode controls */
	void CreateLayout();

	/** Creates controls for the modifiers section */
	TSharedRef<SVerticalBox> CreateTopModifierButtons();
	
	/** Creates controls for the actions section */
	TSharedRef<SUniformGridPanel> CreateBottomModifierButtons();

	/** Creates controls for the modifier properties section */
	TSharedRef<class IDetailsView> CreateModifierProperties();

	/** Creates a modifier radio button */
	TSharedRef<SCheckBox> CreateSingleModifierRadioButton(UGeomModifier* Modifier);

	/** Creates an action button */
	TSharedRef<SButton> CreateSingleModifierButton(UGeomModifier* Modifier);

	/** Returns a reference to the geometry mode tool */
	class FModeTool_GeometryModify* GetGeometryModeTool() const;

	void MakeBuilderBrush( UClass* BrushBuilderClass );

	void OnAddVolume( UClass* VolumeClass );

private:
	/** Pointer to the parent window, so we know to destroy it when done */
	TWeakPtr<SWindow> ParentWindow;

	/** A list of the checkbox modifier controls */
	TArray< TSharedPtr<SCheckBox> > ModifierControls;

	/** The properties control */
	TSharedPtr<class IDetailsView> PropertiesControl;
};


/**
 * Mode Toolkit for the Geometry Tools
 */
class FGeometryMode : public FModeToolkit
{
public:
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	/** Initializes the geometry mode toolkit */
	virtual void Init(const TSharedPtr< class IToolkitHost >& InitToolkitHost) override;

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual class FEdMode* GetEditorMode() const override;
	virtual TSharedPtr<class SWidget> GetInlineContent() const override;

	/** Method called when the selection */
	virtual void SelectionChanged();

private:
	/** Geometry tools widget */
	TSharedPtr<class SGeometryModeControls> GeomWidget;
};
