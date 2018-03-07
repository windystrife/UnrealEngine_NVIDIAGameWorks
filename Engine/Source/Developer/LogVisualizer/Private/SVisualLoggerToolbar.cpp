// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SVisualLoggerToolbar.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LogVisualizerStyle.h"
#include "VisualLoggerCommands.h"
#define LOCTEXT_NAMESPACE "SVisualLoggerToolbar"

/* SVisualLoggerToolbar interface
*****************************************************************************/
void SVisualLoggerToolbar::Construct(const FArguments& InArgs, const TSharedRef<FUICommandList>& InCommandList)
{
	ChildSlot
		[
			MakeToolbar(InCommandList)
		];
}


/* SVisualLoggerToolbar implementation
*****************************************************************************/
TSharedRef<SWidget> SVisualLoggerToolbar::MakeToolbar(const TSharedRef<FUICommandList>& CommandList)
{
	FToolBarBuilder ToolBarBuilder(CommandList, FMultiBoxCustomization::None);

	ToolBarBuilder.BeginSection("Debugger");
	{
		ToolBarBuilder.AddToolBarButton(FVisualLoggerCommands::Get().StartRecording, NAME_None, LOCTEXT("StartLogger", "Start"), LOCTEXT("StartDebuggerTooltip", "Starts recording and collecting visual logs"), FSlateIcon(FLogVisualizerStyle::Get().GetStyleSetName(), TEXT("Toolbar.Record")));
		ToolBarBuilder.AddToolBarButton(FVisualLoggerCommands::Get().StopRecording, NAME_None, LOCTEXT("StopLogger", "Stop"), TAttribute<FText>(), FSlateIcon(FLogVisualizerStyle::Get().GetStyleSetName(), TEXT("Toolbar.Stop")));
		ToolBarBuilder.AddToolBarButton(FVisualLoggerCommands::Get().Pause, NAME_None, LOCTEXT("PauseLogger", "Pause"), TAttribute<FText>(), FSlateIcon(FLogVisualizerStyle::Get().GetStyleSetName(), TEXT("Toolbar.Pause")));
		ToolBarBuilder.AddToolBarButton(FVisualLoggerCommands::Get().Resume, NAME_None, LOCTEXT("ResumeLogger", "Resume"), TAttribute<FText>(), FSlateIcon(FLogVisualizerStyle::Get().GetStyleSetName(), TEXT("Toolbar.Resume")));

		ToolBarBuilder.AddSeparator();
		ToolBarBuilder.AddToolBarButton(FVisualLoggerCommands::Get().LoadFromVLog, NAME_None, LOCTEXT("Load", "Load"), TAttribute<FText>(), FSlateIcon(FLogVisualizerStyle::Get().GetStyleSetName(), TEXT("Toolbar.Load")));
		ToolBarBuilder.AddToolBarButton(FVisualLoggerCommands::Get().SaveToVLog, NAME_None, LOCTEXT("SaveLogs", "Save"), LOCTEXT("SaveLogsTooltip", "Save selected logs/rows to file."), FSlateIcon(FLogVisualizerStyle::Get().GetStyleSetName(), TEXT("Toolbar.Save")));
		ToolBarBuilder.AddToolBarButton(FVisualLoggerCommands::Get().SaveAllToVLog, NAME_None, LOCTEXT("SaveAllLogs", "Save All"), TAttribute<FText>(), FSlateIcon(FLogVisualizerStyle::Get().GetStyleSetName(), TEXT("Toolbar.SaveAll")));

		ToolBarBuilder.AddSeparator();
		ToolBarBuilder.AddToolBarButton(FVisualLoggerCommands::Get().FreeCamera, NAME_None, LOCTEXT("FreeCamera", "Camera"), TAttribute<FText>(), FSlateIcon(FLogVisualizerStyle::Get().GetStyleSetName(), TEXT("Toolbar.Camera")));
		ToolBarBuilder.AddToolBarButton(FVisualLoggerCommands::Get().ResetData, NAME_None, LOCTEXT("ResetData", "Clear"), TAttribute<FText>(), FSlateIcon(FLogVisualizerStyle::Get().GetStyleSetName(), TEXT("Toolbar.Remove")));
		ToolBarBuilder.AddToolBarButton(FVisualLoggerCommands::Get().ToggleGraphs, NAME_None, LOCTEXT("ToggleGraphs", "Graphs"), TAttribute<FText>(), FSlateIcon(FLogVisualizerStyle::Get().GetStyleSetName(), TEXT("Toolbar.Graphs")));
	}

	return ToolBarBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
