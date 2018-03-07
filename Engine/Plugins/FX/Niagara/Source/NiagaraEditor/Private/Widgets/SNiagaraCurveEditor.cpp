// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraCurveEditor.h"
#include "NiagaraSystemViewModel.h"
#include "SCurveEditor.h"
#include "SOverlay.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "RichCurveEditorCommands.h"
#include "Widgets/Input/SNumericDropDown.h"
#include "SCurveEditor.h"
#include "SOverlay.h"

#define LOCTEXT_NAMESPACE "NiagaraCurveEditor"

void SNiagaraCurveEditor::Construct(const FArguments& InArgs, TSharedRef<FNiagaraSystemViewModel> InSystemViewModel)
{
	SystemViewModel = InSystemViewModel;
	SystemViewModel->OnCurveOwnerChanged().AddRaw(this, &SNiagaraCurveEditor::OnCurveOwnerChanged);
	InputSnap = .1f;
	OutputSnap = .1f;

	SAssignNew(CurveEditor, SCurveEditor)
		.ShowCurveSelector(true);
	CurveEditor->SetCurveOwner(&SystemViewModel->GetCurveOwner());

	TSharedPtr<SOverlay> OverlayWidget;
	this->ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			ConstructToolBar(CurveEditor->GetCommands())
		]
		+ SVerticalBox::Slot()
		[
			CurveEditor.ToSharedRef()
		]
	];
}

SNiagaraCurveEditor::~SNiagaraCurveEditor()
{
	SystemViewModel->OnCurveOwnerChanged().RemoveAll(this);
}

TSharedRef<SWidget> SNiagaraCurveEditor::ConstructToolBar(TSharedPtr<FUICommandList> CurveEditorCommandList)
{
	FToolBarBuilder ToolBarBuilder(CurveEditorCommandList, FMultiBoxCustomization::None, TSharedPtr<FExtender>(), Orient_Horizontal, true);

	// TODO: Move this to a shared location since it's 99% the same as the sequencer curve toolbar.
	ToolBarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateSP(this, &SNiagaraCurveEditor::MakeCurveEditorViewOptionsMenu, CurveEditorCommandList),
		LOCTEXT("CurveEditorViewOptions", "View Options"),
		LOCTEXT("CurveEditorViewOptionsToolTip", "View Options"),
		TAttribute<FSlateIcon>(),
		true);

	TArray<SNumericDropDown<float>::FNamedValue> SnapValues;
	SnapValues.Add(SNumericDropDown<float>::FNamedValue(0.001f, LOCTEXT("Snap_OneThousandth", "0.001"), LOCTEXT("SnapDescription_OneThousandth", "Set snap to 1/1000th")));
	SnapValues.Add(SNumericDropDown<float>::FNamedValue(0.01f, LOCTEXT("Snap_OneHundredth", "0.01"), LOCTEXT("SnapDescription_OneHundredth", "Set snap to 1/100th")));
	SnapValues.Add(SNumericDropDown<float>::FNamedValue(0.1f, LOCTEXT("Snap_OneTenth", "0.1"), LOCTEXT("SnapDescription_OneTenth", "Set snap to 1/10th")));
	SnapValues.Add(SNumericDropDown<float>::FNamedValue(1.0f, LOCTEXT("Snap_One", "1"), LOCTEXT("SnapDescription_One", "Set snap to 1")));
	SnapValues.Add(SNumericDropDown<float>::FNamedValue(10.0f, LOCTEXT("Snap_Ten", "10"), LOCTEXT("SnapDescription_Ten", "Set snap to 10")));
	SnapValues.Add(SNumericDropDown<float>::FNamedValue(100.0f, LOCTEXT("Snap_OneHundred", "100"), LOCTEXT("SnapDescription_OneHundred", "Set snap to 100")));

	TSharedRef<SWidget> InputSnapWidget =
		SNew(SNumericDropDown<float>)
		.DropDownValues(SnapValues)
		.LabelText(LOCTEXT("InputSnapLabel", "Input Snap"))
		.Value(this, &SNiagaraCurveEditor::GetInputSnap)
		.OnValueChanged(this, &SNiagaraCurveEditor::SetInputSnap);

	TSharedRef<SWidget> OutputSnapWidget =
		SNew(SNumericDropDown<float>)
		.DropDownValues(SnapValues)
		.LabelText(LOCTEXT("OutputSnapLabel", "Output Snap"))
		.Value(this, &SNiagaraCurveEditor::GetOutputSnap)
		.OnValueChanged(this, &SNiagaraCurveEditor::SetOutputSnap);

	ToolBarBuilder.BeginSection("Snap");
	{
		ToolBarBuilder.AddToolBarButton(FRichCurveEditorCommands::Get().ToggleInputSnapping);
		ToolBarBuilder.AddWidget(InputSnapWidget);
		ToolBarBuilder.AddToolBarButton(FRichCurveEditorCommands::Get().ToggleOutputSnapping);
		ToolBarBuilder.AddWidget(OutputSnapWidget);
	}
	ToolBarBuilder.EndSection();

	ToolBarBuilder.BeginSection("Curve");
	{
		ToolBarBuilder.AddToolBarButton(FRichCurveEditorCommands::Get().ZoomToFitHorizontal);
		ToolBarBuilder.AddToolBarButton(FRichCurveEditorCommands::Get().ZoomToFitVertical);
		ToolBarBuilder.AddToolBarButton(FRichCurveEditorCommands::Get().ZoomToFit);
	}
	ToolBarBuilder.EndSection();

	ToolBarBuilder.BeginSection("Interpolation");
	{
		ToolBarBuilder.AddToolBarButton(FRichCurveEditorCommands::Get().InterpolationCubicAuto);
		ToolBarBuilder.AddToolBarButton(FRichCurveEditorCommands::Get().InterpolationCubicUser);
		ToolBarBuilder.AddToolBarButton(FRichCurveEditorCommands::Get().InterpolationCubicBreak);
		ToolBarBuilder.AddToolBarButton(FRichCurveEditorCommands::Get().InterpolationLinear);
		ToolBarBuilder.AddToolBarButton(FRichCurveEditorCommands::Get().InterpolationConstant);
	}
	ToolBarBuilder.EndSection();

	ToolBarBuilder.BeginSection("Tangents");
	{
		ToolBarBuilder.AddToolBarButton(FRichCurveEditorCommands::Get().FlattenTangents);
		ToolBarBuilder.AddToolBarButton(FRichCurveEditorCommands::Get().StraightenTangents);
	}
	ToolBarBuilder.EndSection();

	ToolBarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateSP(this, &SNiagaraCurveEditor::MakeCurveEditorCurveOptionsMenu, CurveEditorCommandList),
		LOCTEXT("CurveEditorCurveOptions", "Curves Options"),
		LOCTEXT("CurveEditorCurveOptionsToolTip", "Curve Options"),
		TAttribute<FSlateIcon>(),
		true);

	return ToolBarBuilder.MakeWidget();
}

TSharedRef<SWidget> SNiagaraCurveEditor::MakeCurveEditorViewOptionsMenu(TSharedPtr<FUICommandList> CurveEditorCommandList)
{
	FMenuBuilder MenuBuilder(true, CurveEditorCommandList);

	MenuBuilder.BeginSection("CurveVisibility", LOCTEXT("CurveEditorMenuCurveVisibilityHeader", "Curve Visibility"));
	{
		MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().SetAllCurveVisibility);
		MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().SetSelectedCurveVisibility);
		MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().SetAnimatedCurveVisibility);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("TangentVisibility", LOCTEXT("CurveEditorMenuTangentVisibilityHeader", "Tangent Visibility"));
	{
		MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().SetAllTangentsVisibility);
		MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().SetSelectedKeysTangentVisibility);
		MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().SetNoTangentsVisibility);
	}
	MenuBuilder.EndSection();

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().ToggleAutoFrameCurveEditor);
	MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().ToggleShowCurveEditorCurveToolTips);

	return MenuBuilder.MakeWidget();
}
TSharedRef<SWidget> SNiagaraCurveEditor::MakeCurveEditorCurveOptionsMenu(TSharedPtr<FUICommandList> CurveEditorCommandList)
{
	struct FExtrapolationMenus
	{
		static void MakePreInfinityExtrapSubMenu(FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.BeginSection("Pre-Infinity Extrapolation", LOCTEXT("CurveEditorMenuPreInfinityExtrapHeader", "Extrapolation"));
			{
				MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().SetPreInfinityExtrapCycle);
				MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().SetPreInfinityExtrapCycleWithOffset);
				MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().SetPreInfinityExtrapOscillate);
				MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().SetPreInfinityExtrapLinear);
				MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().SetPreInfinityExtrapConstant);
			}
			MenuBuilder.EndSection();
		}

		static void MakePostInfinityExtrapSubMenu(FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.BeginSection("Post-Infinity Extrapolation", LOCTEXT("CurveEditorMenuPostInfinityExtrapHeader", "Extrapolation"));
			{
				MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().SetPostInfinityExtrapCycle);
				MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().SetPostInfinityExtrapCycleWithOffset);
				MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().SetPostInfinityExtrapOscillate);
				MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().SetPostInfinityExtrapLinear);
				MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().SetPostInfinityExtrapConstant);
			}
			MenuBuilder.EndSection();
		}
	};

	FMenuBuilder MenuBuilder(true, CurveEditorCommandList);

	MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().BakeCurve);
	MenuBuilder.AddMenuEntry(FRichCurveEditorCommands::Get().ReduceCurve);

	MenuBuilder.AddSubMenu(
		LOCTEXT("PreInfinitySubMenu", "Pre-Infinity"),
		LOCTEXT("PreInfinitySubMenuToolTip", "Pre-Infinity Extrapolation"),
		FNewMenuDelegate::CreateStatic(&FExtrapolationMenus::MakePreInfinityExtrapSubMenu));

	MenuBuilder.AddSubMenu(
		LOCTEXT("PostInfinitySubMenu", "Post-Infinity"),
		LOCTEXT("PostInfinitySubMenuToolTip", "Post-Infinity Extrapolation"),
		FNewMenuDelegate::CreateStatic(&FExtrapolationMenus::MakePostInfinityExtrapSubMenu));

	return MenuBuilder.MakeWidget();
}


float SNiagaraCurveEditor::GetInputSnap() const
{
	return InputSnap;
}

void SNiagaraCurveEditor::SetInputSnap(float Value)
{
	InputSnap = Value;
}

float SNiagaraCurveEditor::GetOutputSnap() const
{
	return OutputSnap;
}

void SNiagaraCurveEditor::SetOutputSnap(float Value)
{
	OutputSnap = Value;
}

void SNiagaraCurveEditor::OnCurveOwnerChanged()
{
	CurveEditor->SetCurveOwner(&SystemViewModel->GetCurveOwner());
}

#undef LOCTEXT_NAMESPACE
