// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelViewportTabContent.h"
#include "Misc/ConfigCacheIni.h"
#include "Framework/Docking/LayoutService.h"
#include "Modules/ModuleManager.h"
#include "LevelEditor.h"
#include "LevelViewportLayout2x2.h"
#include "LevelViewportLayoutOnePane.h"
#include "LevelViewportLayoutTwoPanes.h"
#include "LevelViewportLayoutThreePanes.h"
#include "LevelViewportLayoutFourPanes.h"
#include "Widgets/Docking/SDockTab.h"


// FLevelViewportTabContent ///////////////////////////

TSharedPtr< class FLevelViewportLayout > FLevelViewportTabContent::ConstructViewportLayoutByTypeName(const FName& TypeName, bool bSwitchingLayouts)
{
	TSharedPtr< class FLevelViewportLayout > ViewportLayout;

	// The items in these ifs should match the names in namespace LevelViewportConfigurationNames
	if (TypeName == LevelViewportConfigurationNames::FourPanes2x2) ViewportLayout = MakeShareable(new FLevelViewportLayout2x2);
	else if (TypeName == LevelViewportConfigurationNames::TwoPanesVert) ViewportLayout = MakeShareable(new FLevelViewportLayoutTwoPanesVert);
	else if (TypeName == LevelViewportConfigurationNames::TwoPanesHoriz) ViewportLayout = MakeShareable(new FLevelViewportLayoutTwoPanesHoriz);
	else if (TypeName == LevelViewportConfigurationNames::ThreePanesLeft) ViewportLayout = MakeShareable(new FLevelViewportLayoutThreePanesLeft);
	else if (TypeName == LevelViewportConfigurationNames::ThreePanesRight) ViewportLayout = MakeShareable(new FLevelViewportLayoutThreePanesRight);
	else if (TypeName == LevelViewportConfigurationNames::ThreePanesTop) ViewportLayout = MakeShareable(new FLevelViewportLayoutThreePanesTop);
	else if (TypeName == LevelViewportConfigurationNames::ThreePanesBottom) ViewportLayout = MakeShareable(new FLevelViewportLayoutThreePanesBottom);
	else if (TypeName == LevelViewportConfigurationNames::FourPanesLeft) ViewportLayout = MakeShareable(new FLevelViewportLayoutFourPanesLeft);
	else if (TypeName == LevelViewportConfigurationNames::FourPanesRight) ViewportLayout = MakeShareable(new FLevelViewportLayoutFourPanesRight);
	else if (TypeName == LevelViewportConfigurationNames::FourPanesBottom) ViewportLayout = MakeShareable(new FLevelViewportLayoutFourPanesBottom);
	else if (TypeName == LevelViewportConfigurationNames::FourPanesTop) ViewportLayout = MakeShareable(new FLevelViewportLayoutFourPanesTop);
	else if (TypeName == LevelViewportConfigurationNames::OnePane) ViewportLayout = MakeShareable(new FLevelViewportLayoutOnePane);

	if (!ensure(ViewportLayout.IsValid()))
	{
		ViewportLayout = MakeShareable(new FLevelViewportLayoutOnePane);
	}
	ViewportLayout->SetIsReplacement(bSwitchingLayouts);
	return ViewportLayout;
}

void FLevelViewportTabContent::Initialize(TSharedPtr<ILevelEditor> InParentLevelEditor, TSharedPtr<SDockTab> InParentTab, const FString& InLayoutString)
{
	ParentTab = InParentTab;
	ParentLevelEditor = InParentLevelEditor;
	LayoutString = InLayoutString;

	InParentTab->SetOnPersistVisualState( SDockTab::FOnPersistVisualState::CreateSP(this, &FLevelViewportTabContent::SaveConfig) );

	const FString& IniSection = FLayoutSaveRestore::GetAdditionalLayoutConfigIni();

	FString LayoutTypeString;
	if(LayoutString.IsEmpty() ||
		!GConfig->GetString(*IniSection, *(InLayoutString + TEXT(".LayoutType")), LayoutTypeString, GEditorPerProjectIni))
	{
		LayoutTypeString = LevelViewportConfigurationNames::FourPanes2x2.ToString();
	}
	FName LayoutType(*LayoutTypeString);
	SetViewportConfiguration(LayoutType);
}

bool FLevelViewportTabContent::IsVisible() const
{
	if (ActiveLevelViewportLayout.IsValid())
	{
		return ActiveLevelViewportLayout->IsVisible();
	}
	return false;
}

const TMap< FName, TSharedPtr< IViewportLayoutEntity > >* FLevelViewportTabContent::GetViewports() const
{
	if (ActiveLevelViewportLayout.IsValid())
	{
		return &ActiveLevelViewportLayout->GetViewports();
	}
	return NULL;
}

void FLevelViewportTabContent::SetViewportConfiguration(const FName& ConfigurationName)
{
	bool bSwitchingLayouts = ActiveLevelViewportLayout.IsValid();

	if (bSwitchingLayouts)
	{
		SaveConfig();
		ActiveLevelViewportLayout.Reset();
	}

	ActiveLevelViewportLayout = ConstructViewportLayoutByTypeName(ConfigurationName, bSwitchingLayouts);
	check (ActiveLevelViewportLayout.IsValid());

	UpdateViewportTabWidget();
}

void FLevelViewportTabContent::SaveConfig() const
{
	if (ActiveLevelViewportLayout.IsValid())
	{
		if (!LayoutString.IsEmpty())
		{
			FString LayoutTypeString = ActiveLevelViewportLayout->GetLayoutTypeName().ToString();

			const FString& IniSection = FLayoutSaveRestore::GetAdditionalLayoutConfigIni();
			GConfig->SetString(*IniSection, *(LayoutString + TEXT(".LayoutType")), *LayoutTypeString, GEditorPerProjectIni);
		}

		ActiveLevelViewportLayout->SaveLayoutString(LayoutString);
	}
}

void FLevelViewportTabContent::RefreshViewportConfiguration()
{
	check(ActiveLevelViewportLayout.IsValid());

	FName ConfigurationName = ActiveLevelViewportLayout->GetLayoutTypeName();
	for (auto& Pair : ActiveLevelViewportLayout->GetViewports())
	{
		if (Pair.Value->AsWidget()->HasFocusedDescendants())
		{
			PreviouslyFocusedViewport = Pair.Key;
			break;
		}
	}

	ActiveLevelViewportLayout.Reset();

	bool bSwitchingLayouts = false;
	ActiveLevelViewportLayout = ConstructViewportLayoutByTypeName(ConfigurationName, bSwitchingLayouts);
	check(ActiveLevelViewportLayout.IsValid());

	UpdateViewportTabWidget();
}


bool FLevelViewportTabContent::IsViewportConfigurationSet(const FName& ConfigurationName) const
{
	if (ActiveLevelViewportLayout.IsValid())
	{
		return ActiveLevelViewportLayout->GetLayoutTypeName() == ConfigurationName;
	}
	return false;
}

bool FLevelViewportTabContent::BelongsToTab(TSharedRef<class SDockTab> InParentTab) const
{
	TSharedPtr<SDockTab> ParentTabPinned = ParentTab.Pin();
	return ParentTabPinned == InParentTab;
}

void FLevelViewportTabContent::UpdateViewportTabWidget()
{
	TSharedPtr<SDockTab> ParentTabPinned = ParentTab.Pin();
	if (ParentTabPinned.IsValid() && ActiveLevelViewportLayout.IsValid())
	{
		TSharedRef<SWidget> LayoutWidget = ActiveLevelViewportLayout->BuildViewportLayout(ParentTabPinned, SharedThis(this), LayoutString, ParentLevelEditor);
		ParentTabPinned->SetContent(LayoutWidget);

		if (PreviouslyFocusedViewport.IsSet())
		{
			TSharedPtr<IViewportLayoutEntity> ViewportToFocus = ActiveLevelViewportLayout->GetViewports().FindRef(PreviouslyFocusedViewport.GetValue());
			if (ViewportToFocus.IsValid())
			{
				ViewportToFocus->SetKeyboardFocus();
			}
			PreviouslyFocusedViewport = TOptional<FName>();
		}
	}
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.OnTabContentChanged().Broadcast();
}
