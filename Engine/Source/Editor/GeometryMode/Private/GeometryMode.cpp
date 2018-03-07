// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GeometryMode.h"
#include "Widgets/Text/STextBlock.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "GeometryEdMode.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "GeomModifier.h"

#define LOCTEXT_NAMESPACE "GeometryMode"

void SGeometryModeControls::SelectionChanged()
{
	// If the currently selected modifier is being disabled, change the selection to Edit
	for (int32 Idx = 0; Idx < ModifierControls.Num(); ++Idx)
	{
		if (ModifierControls[Idx]->IsChecked() && !GetGeometryModeTool()->GetModifier(Idx)->Supports())
		{
			if (GetGeometryModeTool()->GetNumModifiers() > 0)
			{
				GetGeometryModeTool()->SetCurrentModifier(GetGeometryModeTool()->GetModifier(0));
			}
		}
	}
}

void SGeometryModeControls::Construct(const FArguments& InArgs)
{
	FModeTool_GeometryModify* GeometryModeTool = GetGeometryModeTool();
	
	if (GetGeometryModeTool()->GetNumModifiers() > 0)
	{
		GetGeometryModeTool()->SetCurrentModifier(GetGeometryModeTool()->GetModifier(0));
	}

	CreateLayout();
}

void SGeometryModeControls::OnModifierStateChanged(ECheckBoxState NewCheckedState, UGeomModifier* Modifier)
{
	if (NewCheckedState == ECheckBoxState::Checked)
	{
		GetGeometryModeTool()->SetCurrentModifier(Modifier);

		TArray<UObject*> PropertyObjects;
		PropertyObjects.Add(GetGeometryModeTool()->GetCurrentModifier());
		PropertiesControl->SetObjects(PropertyObjects);
	}
}

ECheckBoxState SGeometryModeControls::IsModifierChecked(UGeomModifier* Modifier) const
{
	return (GetGeometryModeTool()->GetCurrentModifier() == Modifier)
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

bool SGeometryModeControls::IsModifierEnabled(UGeomModifier* Modifier) const
{
	return Modifier->Supports();
}

EVisibility SGeometryModeControls::IsPropertiesVisible() const
{
	if ((GetGeometryModeTool()->GetNumModifiers() > 0) && (GetGeometryModeTool()->GetCurrentModifier() != GetGeometryModeTool()->GetModifier(0)))
	{
		return EVisibility::Visible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

FReply SGeometryModeControls::OnApplyClicked()
{
	check(GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_Geometry));

	GetGeometryModeTool()->GetCurrentModifier()->Apply();

	return FReply::Handled();
}

FReply SGeometryModeControls::OnModifierClicked(UGeomModifier* Modifier)
{
	check(GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_Geometry));

	Modifier->Apply();

	return FReply::Handled();
}

void SGeometryModeControls::CreateLayout()
{
	this->ChildSlot
	[
		SNew(SScrollBox)
		+SScrollBox::Slot()
		.Padding(0.0f)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					CreateTopModifierButtons()
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3.0f)
				[
					SNew(SSeparator)
					.Orientation(Orient_Horizontal)
				]	
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					CreateModifierProperties()
				]	
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(3.0f)
				[
					SNew(SSeparator)
					.Orientation(Orient_Horizontal)
					.Visibility(this, &SGeometryModeControls::IsPropertiesVisible)
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					CreateBottomModifierButtons()
				]
			]
		]
	];
}

TSharedRef<SVerticalBox> SGeometryModeControls::CreateTopModifierButtons()
{
	FModeTool_GeometryModify* GeometryModeTool = GetGeometryModeTool();
	TSharedPtr<SVerticalBox> Vbox;
	const TSharedRef<SGridPanel> RadioButtonPanel = SNew(SGridPanel);

	// Loop through all geometry modifiers and create radio buttons for ones with the bPushButton set to false
	int32 CurrentModifierButtonCount = 0;
	for (FModeTool_GeometryModify::TModifierIterator Itor(GeometryModeTool->ModifierIterator()); Itor; ++Itor)
	{
		UGeomModifier* Modifier = *Itor;
		if (!Modifier->bPushButton)
		{
			RadioButtonPanel->AddSlot(CurrentModifierButtonCount%2, CurrentModifierButtonCount/2)
			.Padding( FMargin(20.0f, 5.0f) )
			[
				CreateSingleModifierRadioButton(Modifier)
			];

			++CurrentModifierButtonCount;
		}
	}

	// Add the Apply button
	SAssignNew(Vbox, SVerticalBox)
	+SVerticalBox::Slot()
	.AutoHeight()
	[
		RadioButtonPanel
	]
	+SVerticalBox::Slot()
	.AutoHeight()
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Center)
	[
		SNew(SButton)
		.Text(LOCTEXT("SGeometryModeDialog_Apply", "Apply"))
		.OnClicked(this, &SGeometryModeControls::OnApplyClicked)
	];

	return Vbox.ToSharedRef();
}

TSharedRef<SUniformGridPanel> SGeometryModeControls::CreateBottomModifierButtons()
{
	FModeTool_GeometryModify* GeometryModeTool = GetGeometryModeTool();
	TSharedRef<SUniformGridPanel> ButtonGrid = SNew(SUniformGridPanel).SlotPadding(5.0f);

	// The IDs of the buttons created in this function need to sequentially follow the IDs of the modifier radio buttons
	// So, this loop simply counts the number of radio buttons so we can use that as an offset
	int32 CurrentModifierButtonCount = 0;
	for (FModeTool_GeometryModify::TModifierConstIterator Itor(GeometryModeTool->ModifierConstIterator()); Itor; ++Itor)
	{
		const UGeomModifier* Modifier = *Itor;
		if (!Modifier->bPushButton)
		{
			++CurrentModifierButtonCount;
		}
	}

	// Loop through all geometry modifiers and create buttons for ones with the bPushButton set to true
	int32 PushButtonId = 0;
	for (FModeTool_GeometryModify::TModifierIterator Itor(GeometryModeTool->ModifierIterator()); Itor; ++Itor)
	{
		UGeomModifier* Modifier = *Itor;
		if (Modifier->bPushButton)
		{
			ButtonGrid->AddSlot(PushButtonId % 2, PushButtonId / 2)
			[
				CreateSingleModifierButton(Modifier)
			];

			++CurrentModifierButtonCount;
			++PushButtonId;
		}
	}

	return ButtonGrid;
}

TSharedRef<IDetailsView> SGeometryModeControls::CreateModifierProperties()
{
	FDetailsViewArgs Args;
	Args.bHideSelectionTip = true;
	Args.bAllowSearch = false;

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertiesControl = PropertyModule.CreateDetailView(Args);
	PropertiesControl->SetVisibility(TAttribute<EVisibility>(this, &SGeometryModeControls::IsPropertiesVisible));

	return PropertiesControl.ToSharedRef();
}

TSharedRef<SCheckBox> SGeometryModeControls::CreateSingleModifierRadioButton(UGeomModifier* Modifier)
{
	TSharedRef<SCheckBox> CheckBox =
	SNew(SCheckBox)
	.Style(FEditorStyle::Get(), "RadioButton")
	.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
	.IsChecked(this, &SGeometryModeControls::IsModifierChecked, Modifier)
	.IsEnabled(this, &SGeometryModeControls::IsModifierEnabled, Modifier)
	.OnCheckStateChanged(this, &SGeometryModeControls::OnModifierStateChanged, Modifier)
	.ToolTip(SNew(SToolTip).Text(Modifier->GetModifierTooltip()))
	[
		SNew(STextBlock).Text( Modifier->GetModifierDescription() )
	];

	ModifierControls.Add(CheckBox);

	return CheckBox;
}

TSharedRef<SButton> SGeometryModeControls::CreateSingleModifierButton(UGeomModifier* Modifier)
{
	TSharedRef<SButton> Widget =
	SNew(SButton)
	.Text( Modifier->GetModifierDescription() )
	.ToolTip(SNew(SToolTip).Text(Modifier->GetModifierTooltip()))
	.HAlign(HAlign_Center)
	.IsEnabled(this, &SGeometryModeControls::IsModifierEnabled, Modifier)
	.OnClicked(this, &SGeometryModeControls::OnModifierClicked, Modifier);

	return Widget;
}

FModeTool_GeometryModify* SGeometryModeControls::GetGeometryModeTool() const
{
	FEdModeGeometry* Mode = (FEdModeGeometry*)GLevelEditorModeTools().GetActiveMode( FBuiltinEditorModes::EM_Geometry );
	FModeTool* Tool = Mode? Mode->GetCurrentTool(): NULL;

	check(Tool);

	return (FModeTool_GeometryModify*)Tool;
}

void FGeometryMode::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{

}

void FGeometryMode::UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager)
{

}

void FGeometryMode::Init(const TSharedPtr< class IToolkitHost >& InitToolkitHost)
{
	GeomWidget = SNew(SGeometryModeControls);

	FModeToolkit::Init(InitToolkitHost);
}

FName FGeometryMode::GetToolkitFName() const
{
	return FName("GeometryMode");
}

FText FGeometryMode::GetBaseToolkitName() const
{
	return LOCTEXT( "ToolkitName", "Geometry Mode" );
}

class FEdMode* FGeometryMode::GetEditorMode() const
{
	return (FEdModeGeometry*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Geometry);
}

void FGeometryMode::SelectionChanged()
{
	GeomWidget->SelectionChanged();
}

TSharedPtr<SWidget> FGeometryMode::GetInlineContent() const
{
	return SNew(SGeometryModeControls);
}

#undef LOCTEXT_NAMESPACE
