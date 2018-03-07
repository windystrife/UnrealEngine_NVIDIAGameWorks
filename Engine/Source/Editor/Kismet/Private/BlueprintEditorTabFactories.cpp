// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorTabFactories.h"
#include "Widgets/Text/STextBlock.h"
#include "Editor.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Notifications/SErrorText.h"
#include "EditorStyleSet.h"

#include "BlueprintEditorSharedTabFactories.h"
#include "BlueprintEditorTabs.h"
#include "STimelineEditor.h"
#include "Debugging/SKismetDebuggingView.h"
#include "SKismetInspector.h"
#include "SSCSEditor.h"
#include "SSCSEditorViewport.h"
#include "SBlueprintPalette.h"
#include "FindInBlueprints.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "SMyBlueprint.h"
#include "SReplaceNodeReferences.h"
#include "Widgets/Input/SHyperlink.h"
#include "BlueprintEditorSettings.h"

#define LOCTEXT_NAMESPACE "BlueprintEditor"

void FGraphEditorSummoner::OnTabActivated(TSharedPtr<SDockTab> Tab) const
{
	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());
	BlueprintEditorPtr.Pin()->OnGraphEditorFocused(GraphEditor);
}

void FGraphEditorSummoner::OnTabBackgrounded(TSharedPtr<SDockTab> Tab) const
{
	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());
	BlueprintEditorPtr.Pin()->OnGraphEditorBackgrounded(GraphEditor);
}

void FGraphEditorSummoner::OnTabRefreshed(TSharedPtr<SDockTab> Tab) const
{
	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());
	GraphEditor->NotifyGraphChanged();
}

void FGraphEditorSummoner::SaveState(TSharedPtr<SDockTab> Tab, TSharedPtr<FTabPayload> Payload) const
{
	TSharedRef<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(Tab->GetContent());

	FVector2D ViewLocation;
	float ZoomAmount;
	GraphEditor->GetViewLocation(ViewLocation, ZoomAmount);

	UEdGraph* Graph = Payload->IsValid() ? FTabPayload_UObject::CastChecked<UEdGraph>(Payload) : nullptr;

	if (Graph && BlueprintEditorPtr.Pin()->IsGraphInCurrentBlueprint(Graph))
	{
		// Don't save references to external graphs.
		BlueprintEditorPtr.Pin()->GetBlueprintObj()->LastEditedDocuments.Add(FEditedDocumentInfo(Graph, ViewLocation, ZoomAmount));
	}
}

FGraphEditorSummoner::FGraphEditorSummoner(TSharedPtr<class FBlueprintEditor> InBlueprintEditorPtr, FOnCreateGraphEditorWidget CreateGraphEditorWidgetCallback) : FDocumentTabFactoryForObjects<UEdGraph>(FBlueprintEditorTabs::GraphEditorID, InBlueprintEditorPtr)
, BlueprintEditorPtr(InBlueprintEditorPtr)
, OnCreateGraphEditorWidget(CreateGraphEditorWidgetCallback)
{

}

TSharedRef<SWidget> FGraphEditorSummoner::CreateTabBodyForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const
{
	check(Info.TabInfo.IsValid());
	return OnCreateGraphEditorWidget.Execute(Info.TabInfo.ToSharedRef(), DocumentID);
}

const FSlateBrush* FGraphEditorSummoner::GetTabIconForObject(const FWorkflowTabSpawnInfo& Info, UEdGraph* DocumentID) const
{
	return FBlueprintEditor::GetGlyphForGraph(DocumentID, false);
}

TSharedRef<FGenericTabHistory> FGraphEditorSummoner::CreateTabHistoryNode(TSharedPtr<FTabPayload> Payload)
{
	return MakeShareable(new FGraphTabHistory(SharedThis(this), Payload));
}

void FTimelineEditorSummoner::OnTabRefreshed(TSharedPtr<SDockTab> Tab) const
{
	TSharedRef<STimelineEditor> TimelineEditor = StaticCastSharedRef<STimelineEditor>(Tab->GetContent());
	TimelineEditor->OnTimelineChanged();
}

FTimelineEditorSummoner::FTimelineEditorSummoner(TSharedPtr<class FBlueprintEditor> InBlueprintEditorPtr)
	: FDocumentTabFactoryForObjects<UTimelineTemplate>(FBlueprintEditorTabs::TimelineEditorID, InBlueprintEditorPtr)
, BlueprintEditorPtr(InBlueprintEditorPtr)
{

}

TSharedRef<SWidget> FTimelineEditorSummoner::CreateTabBodyForObject(const FWorkflowTabSpawnInfo& Info, UTimelineTemplate* DocumentID) const
{
	return SNew(STimelineEditor, BlueprintEditorPtr.Pin(), DocumentID);
}

const FSlateBrush* FTimelineEditorSummoner::GetTabIconForObject(const FWorkflowTabSpawnInfo& Info, UTimelineTemplate* DocumentID) const
{
	return FEditorStyle::GetBrush("GraphEditor.Timeline_16x");
}

void FTimelineEditorSummoner::SaveState(TSharedPtr<SDockTab> Tab, TSharedPtr<FTabPayload> Payload) const
{
	UTimelineTemplate* Timeline = FTabPayload_UObject::CastChecked<UTimelineTemplate>(Payload);
	BlueprintEditorPtr.Pin()->GetBlueprintObj()->LastEditedDocuments.Add(FEditedDocumentInfo(Timeline));
}

TAttribute<FText> FTimelineEditorSummoner::ConstructTabNameForObject(UTimelineTemplate* DocumentID) const
{
	return TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateStatic<UObject*>(&FLocalKismetCallbacks::GetObjectName, DocumentID));
}

FDebugInfoSummoner::FDebugInfoSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp) : FWorkflowTabFactory(FBlueprintEditorTabs::DebugID, InHostingApp)
{
	TabLabel = LOCTEXT("DebugTabTitle", "Debug");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "DebugTools.TabIcon");

	EnableTabPadding();
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("DebugView", "Debug");
	ViewMenuTooltip = LOCTEXT("DebugView_ToolTip", "Shows the debugging view");
}

TSharedRef<SWidget> FDebugInfoSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = StaticCastSharedPtr<FBlueprintEditor>(HostingApp.Pin());

	return BlueprintEditorPtr->GetDebuggingView();
}

FDefaultsEditorSummoner::FDefaultsEditorSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
	: FWorkflowTabFactory(FBlueprintEditorTabs::DefaultEditorID, InHostingApp)
{
	TabLabel = LOCTEXT("ClassDefaultsTabTitle", "Class Defaults");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "Kismet.Tabs.BlueprintDefaults");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("DefaultEditorView", "Defaults");
	ViewMenuTooltip = LOCTEXT("DefaultEditorView_ToolTip", "Shows the default editor view");
}

TSharedRef<SWidget> FDefaultsEditorSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = StaticCastSharedPtr<FBlueprintEditor>(HostingApp.Pin());

	TSharedRef<SWidget> Message = CreateOptionalDataOnlyMessage();

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0,0,0,1))
		[
			Message
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			BlueprintEditorPtr->GetDefaultEditor()
		];
}

TSharedRef<SWidget> FDefaultsEditorSummoner::CreateOptionalDataOnlyMessage() const
{
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = StaticCastSharedPtr<FBlueprintEditor>(HostingApp.Pin());

	TSharedRef<SWidget> Message = SNullWidget::NullWidget;
	if ( UBlueprint* Blueprint = BlueprintEditorPtr->GetBlueprintObj() )
	{
		if ( FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint) )
		{
			Message = SNew(SBorder)
				.Padding(FMargin(5))
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SWrapBox)
					.UseAllottedWidth(true)

					+ SWrapBox::Slot()
					[
						SNew(STextBlock)
						.Font(FEditorStyle::GetFontStyle("BoldFont"))
						.Text(LOCTEXT("DataOnlyMessage_Part1", "NOTE: This is a data only blueprint, so only the default values are shown.  It does not have any script or variables.  If you want to add some, "))
					]

					+ SWrapBox::Slot()
					[
						SNew(SHyperlink)
						.Style(FEditorStyle::Get(), "Common.GotoBlueprintHyperlink")
						.OnNavigate(this, &FDefaultsEditorSummoner::OnChangeBlueprintToNotDataOnly)
						.Text(LOCTEXT("FullEditor", "Open Full Blueprint Editor"))
						.ToolTipText(LOCTEXT("FullEditorToolTip", "This opens the blueprint in the full editor."))
					]
				];
		}
	}

	return Message;
}

void FDefaultsEditorSummoner::OnChangeBlueprintToNotDataOnly()
{
	UBlueprint* Blueprint = nullptr;

	{
		TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = StaticCastSharedPtr<FBlueprintEditor>(HostingApp.Pin());
		Blueprint = BlueprintEditorPtr->GetBlueprintObj();
		if ( Blueprint )
		{
			BlueprintEditorPtr->CloseWindow();
		}
	}

	if ( Blueprint )
	{
		Blueprint->bForceFullEditor = true;

		GEditor->EditObject(Blueprint);
	}
}

FConstructionScriptEditorSummoner::FConstructionScriptEditorSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
	: FWorkflowTabFactory(FBlueprintEditorTabs::ConstructionScriptEditorID, InHostingApp)
{
	TabLabel = LOCTEXT("ComponentsTabLabel", "Components");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "Kismet.Tabs.Components");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("ComponentsView", "Components");
	ViewMenuTooltip = LOCTEXT("ComponentsView_ToolTip", "Show the components view");
}

TSharedRef<SWidget> FConstructionScriptEditorSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = StaticCastSharedPtr<FBlueprintEditor>(HostingApp.Pin());

	return BlueprintEditorPtr->GetSCSEditor().ToSharedRef();
}

FSCSViewportSummoner::FSCSViewportSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
	: FWorkflowTabFactory(FBlueprintEditorTabs::SCSViewportID, InHostingApp)
{
	TabLabel = LOCTEXT("SCSViewportTabLabel", "Viewport");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Viewports");

	bIsSingleton = true;
	TabRole = ETabRole::DocumentTab;

	ViewMenuDescription = LOCTEXT("SCSViewportView", "Viewport");
	ViewMenuTooltip = LOCTEXT("SCSViewportView_ToolTip", "Show the viewport view");
}

TSharedRef<SWidget> FSCSViewportSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = StaticCastSharedPtr<FBlueprintEditor>(HostingApp.Pin());

	TSharedPtr<SWidget> Result;
	if (BlueprintEditorPtr->CanAccessComponentsMode())
	{
		Result = BlueprintEditorPtr->GetSCSViewport();
	}

	if (Result.IsValid())
	{
		return Result.ToSharedRef();
	}
	else
	{
		return SNew(SErrorText)
			.BackgroundColor(FLinearColor::Transparent)
			.ErrorText(LOCTEXT("SCSViewportView_Unavailable", "Viewport is not available for this Blueprint."));
	}
}

TSharedRef<SDockTab> FSCSViewportSummoner::SpawnTab(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedRef<SDockTab> Tab = FWorkflowTabFactory::SpawnTab(Info);

	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = StaticCastSharedPtr<FBlueprintEditor>(HostingApp.Pin());
	BlueprintEditorPtr->GetSCSViewport()->SetOwnerTab(Tab);

	return Tab;
}

FPaletteSummoner::FPaletteSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
	: FWorkflowTabFactory(FBlueprintEditorTabs::PaletteID, InHostingApp)
{
	TabLabel = LOCTEXT("PaletteTabTitle", "Palette");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "Kismet.Tabs.Palette");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("PaletteView", "Palette");
	ViewMenuTooltip = LOCTEXT("PaletteView_ToolTip", "Show palette of all functions and variables");
}

TSharedRef<SWidget> FPaletteSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = StaticCastSharedPtr<FBlueprintEditor>(HostingApp.Pin());

	return BlueprintEditorPtr->GetPalette();
}

FMyBlueprintSummoner::FMyBlueprintSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
	: FWorkflowTabFactory(FBlueprintEditorTabs::MyBlueprintID, InHostingApp)
{
	TabLabel = LOCTEXT("MyBlueprintTabLabel", "My Blueprint");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.BlueprintCore");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("MyBlueprintTabView", "My Blueprint");
	ViewMenuTooltip = LOCTEXT("MyBlueprintTabView_ToolTip", "Show the my blueprint view");
}

TSharedRef<SWidget> FMyBlueprintSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = StaticCastSharedPtr<FBlueprintEditor>(HostingApp.Pin());

	return BlueprintEditorPtr->GetMyBlueprintWidget().ToSharedRef();
}

FReplaceNodeReferencesSummoner::FReplaceNodeReferencesSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
	: FWorkflowTabFactory(FBlueprintEditorTabs::ReplaceNodeReferencesID, InHostingApp)
{
	TabLabel = LOCTEXT("ReplaceNodeReferences", "Replace References");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "ClassIcon.BlueprintCore");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("ReplaceNodeReferences", "Replace References");
	ViewMenuTooltip = LOCTEXT("ReplaceNodeReferences_Tooltip", "Show Replace References");
}

TSharedRef<SWidget> FReplaceNodeReferencesSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = StaticCastSharedPtr<FBlueprintEditor>(HostingApp.Pin());

	return BlueprintEditorPtr->GetReplaceReferencesWidget().ToSharedRef();
}

FCompilerResultsSummoner::FCompilerResultsSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
	: FWorkflowTabFactory(FBlueprintEditorTabs::CompilerResultsID, InHostingApp)
{
	TabLabel = LOCTEXT("CompilerResultsTabTitle", "Compiler Results");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "Kismet.Tabs.CompilerResults");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("CompilerResultsView", "Compiler Results");
	ViewMenuTooltip = LOCTEXT("CompilerResultsView_ToolTip", "Show compiler results of all functions and variables");
}

TSharedRef<SWidget> FCompilerResultsSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = StaticCastSharedPtr<FBlueprintEditor>(HostingApp.Pin());

	return BlueprintEditorPtr->GetCompilerResults();
}

FFindResultsSummoner::FFindResultsSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp)
	: FWorkflowTabFactory(FBlueprintEditorTabs::FindResultsID, InHostingApp)
{
	TabLabel = LOCTEXT("FindResultsTabTitle", "Find Results");
	TabIcon = FSlateIcon(FEditorStyle::GetStyleSetName(), "Kismet.Tabs.FindResults");

	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("FindResultsView", "Find Results");

	if (GetDefault<UBlueprintEditorSettings>()->bHostFindInBlueprintsInGlobalTab)
	{
		ViewMenuTooltip = LOCTEXT("FindResultsView_ToolTip", "Show find results for searching in this blueprint");
	}
	else
	{
		ViewMenuTooltip = LOCTEXT("FindResultsViewAllBlueprints_ToolTip", "Show find results for searching in this blueprint or all blueprints");
	}
}

TSharedRef<SWidget> FFindResultsSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	TSharedPtr<FBlueprintEditor> BlueprintEditorPtr = StaticCastSharedPtr<FBlueprintEditor>(HostingApp.Pin());

	return BlueprintEditorPtr->GetFindResults();
}

void FGraphTabHistory::EvokeHistory(TSharedPtr<FTabInfo> InTabInfo)
{
	FWorkflowTabSpawnInfo SpawnInfo;
	SpawnInfo.Payload = Payload;
	SpawnInfo.TabInfo = InTabInfo;

	TSharedRef< SGraphEditor > GraphEditorRef = StaticCastSharedRef< SGraphEditor >(FactoryPtr.Pin()->CreateTabBody(SpawnInfo));
	GraphEditor = GraphEditorRef;

	FactoryPtr.Pin()->UpdateTab(InTabInfo->GetTab().Pin(), SpawnInfo, GraphEditorRef);
}

void FGraphTabHistory::SaveHistory()
{
	if (IsHistoryValid())
	{
		check(GraphEditor.IsValid());
		GraphEditor.Pin()->GetViewLocation(SavedLocation, SavedZoomAmount);
	}
}

void FGraphTabHistory::RestoreHistory()
{
	if (IsHistoryValid())
	{
		check(GraphEditor.IsValid());
		GraphEditor.Pin()->SetViewLocation(SavedLocation, SavedZoomAmount);
	}
}


#undef LOCTEXT_NAMESPACE
