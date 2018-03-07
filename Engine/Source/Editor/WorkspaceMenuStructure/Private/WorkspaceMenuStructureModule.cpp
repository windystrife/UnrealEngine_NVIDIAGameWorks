// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WorkspaceMenuStructureModule.h"
#include "Textures/SlateIcon.h"
#include "Framework/Docking/WorkspaceItem.h"
#include "WorkspaceMenuStructure.h"



#include "EditorStyleSet.h"

IMPLEMENT_MODULE( FWorkspaceMenuStructureModule, WorkspaceMenuStructure );

#define LOCTEXT_NAMESPACE "UnrealEditor"

class FWorkspaceMenuStructure : public IWorkspaceMenuStructure
{
public:
	virtual TSharedRef<FWorkspaceItem> GetStructureRoot() const override
	{
		return MenuRoot.ToSharedRef();
	}

	virtual TSharedRef<FWorkspaceItem> GetLevelEditorCategory() const override
	{
		return LevelEditorCategory.ToSharedRef();
	}

	virtual TSharedRef<FWorkspaceItem> GetLevelEditorViewportsCategory() const override
	{
		return LevelEditorViewportsCategory.ToSharedRef();
	}

	virtual TSharedRef<FWorkspaceItem> GetLevelEditorDetailsCategory() const override
	{
		return LevelEditorDetailsCategory.ToSharedRef();
	}

	virtual TSharedRef<FWorkspaceItem> GetLevelEditorModesCategory() const override
	{
		return LevelEditorModesCategory.ToSharedRef();
	}

	virtual TSharedRef<FWorkspaceItem> GetToolsCategory() const override
	{
		return ToolsCategory.ToSharedRef();
	}

	virtual TSharedRef<FWorkspaceItem> GetDeveloperToolsDebugCategory() const override
	{
		return DeveloperToolsDebugCategory.ToSharedRef();
	}

	virtual TSharedRef<FWorkspaceItem> GetDeveloperToolsLogCategory() const override
	{
		return DeveloperToolsLogCategory.ToSharedRef();
	}

	virtual TSharedRef<FWorkspaceItem> GetDeveloperToolsMiscCategory() const override
	{
		return DeveloperToolsMiscCategory.ToSharedRef();
	}

	virtual TSharedRef<FWorkspaceItem> GetAutomationToolsCategory() const override
	{
		return AutomationToolsCategory.ToSharedRef();
	}

	virtual TSharedRef<FWorkspaceItem> GetEditOptions() const override
	{
		return EditOptions.ToSharedRef();
	}

	void ResetLevelEditorCategory()
	{
		LevelEditorCategory->ClearItems();
		LevelEditorViewportsCategory = LevelEditorCategory->AddGroup(LOCTEXT( "WorkspaceMenu_LevelEditorViewportCategory", "Viewports" ), LOCTEXT( "WorkspaceMenu_LevelEditorViewportCategoryTooltip", "Open a Viewport tab." ), FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Viewports"), true);
		LevelEditorDetailsCategory = LevelEditorCategory->AddGroup(LOCTEXT("WorkspaceMenu_LevelEditorDetailCategory", "Details" ), LOCTEXT("WorkspaceMenu_LevelEditorDetailCategoryTooltip", "Open a Details tab." ), FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"), true );
		LevelEditorModesCategory = LevelEditorCategory->AddGroup(LOCTEXT("WorkspaceMenu_LevelEditorToolsCategory", "Tools" ), FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.EditorModes"), true );
	}

	void ResetToolsCategory()
	{
		ToolsCategory->ClearItems();

		// Developer tools sub menu
		DeveloperToolsCategory = ToolsCategory->AddGroup(LOCTEXT("WorkspaceMenu_DeveloperToolsCategory", "Developer Tools"), FSlateIcon(FEditorStyle::GetStyleSetName(), "DeveloperTools.MenuIcon"));

		// Developer tools sections
		DeveloperToolsDebugCategory = DeveloperToolsCategory->AddGroup(LOCTEXT("WorkspaceMenu_DeveloperToolsDebugCategory", "Debug"), FSlateIcon(), true);
		DeveloperToolsLogCategory = DeveloperToolsCategory->AddGroup(LOCTEXT("WorkspaceMenu_DeveloperToolsLogCategory", "Log"), FSlateIcon(), true);
		DeveloperToolsMiscCategory = DeveloperToolsCategory->AddGroup(LOCTEXT("WorkspaceMenu_DeveloperToolsMiscCategory", "Miscellaneous"), FSlateIcon(), true);
		
		// Automation tools sub menu
		AutomationToolsCategory = FWorkspaceItem::NewGroup(LOCTEXT("WorkspaceMenu_AutomationToolsCategory", "Automation Tools"), FSlateIcon(), true);
	}

public:
	FWorkspaceMenuStructure()
		: MenuRoot ( FWorkspaceItem::NewGroup(LOCTEXT( "WorkspaceMenu_Root", "Menu Root" )) )
		, LevelEditorCategory ( MenuRoot->AddGroup(LOCTEXT( "WorkspaceMenu_LevelEditorCategory", "Level Editor" ), FSlateIcon(), true) )
		, ToolsCategory ( MenuRoot->AddGroup(LOCTEXT( "WorkspaceMenu_ToolsCategory", "General" ), FSlateIcon(), true) )
		, EditOptions( FWorkspaceItem::NewGroup(LOCTEXT( "WorkspaceEdit_Options", "Edit Options" )) )
	{
		ResetLevelEditorCategory();
		ResetToolsCategory();
	}

	virtual ~FWorkspaceMenuStructure() {}

private:
	TSharedPtr<FWorkspaceItem> MenuRoot;
	
	TSharedPtr<FWorkspaceItem> LevelEditorCategory;
	TSharedPtr<FWorkspaceItem> LevelEditorViewportsCategory;
	TSharedPtr<FWorkspaceItem> LevelEditorDetailsCategory;
	TSharedPtr<FWorkspaceItem> LevelEditorModesCategory;

	TSharedPtr<FWorkspaceItem> ToolsCategory;
	TSharedPtr<FWorkspaceItem> DeveloperToolsCategory;
	TSharedPtr<FWorkspaceItem> DeveloperToolsDebugCategory;
	TSharedPtr<FWorkspaceItem> DeveloperToolsLogCategory;
	TSharedPtr<FWorkspaceItem> DeveloperToolsMiscCategory;
	
	TSharedPtr<FWorkspaceItem> AutomationToolsCategory;
	
	TSharedPtr<FWorkspaceItem> EditOptions;
};

void FWorkspaceMenuStructureModule::StartupModule()
{
	WorkspaceMenuStructure = MakeShareable(new FWorkspaceMenuStructure);
}

void FWorkspaceMenuStructureModule::ShutdownModule()
{
	WorkspaceMenuStructure.Reset();
}

const IWorkspaceMenuStructure& FWorkspaceMenuStructureModule::GetWorkspaceMenuStructure() const
{
	check(WorkspaceMenuStructure.IsValid());
	return *WorkspaceMenuStructure;
}

void FWorkspaceMenuStructureModule::ResetLevelEditorCategory()
{
	check(WorkspaceMenuStructure.IsValid());
	WorkspaceMenuStructure->ResetLevelEditorCategory();
}

void FWorkspaceMenuStructureModule::ResetToolsCategory()
{
	check(WorkspaceMenuStructure.IsValid());
	WorkspaceMenuStructure->ResetToolsCategory();
}

#undef LOCTEXT_NAMESPACE
