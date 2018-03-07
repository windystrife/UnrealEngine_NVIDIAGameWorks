// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SProfilerToolbar.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Docking/TabManager.h"
#include "EditorStyleSet.h"
#include "ProfilerCommands.h"
#include "ProfilerManager.h"
#include "Widgets/SProfilerFPSChartPanel.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "Profiler"


SProfilerToolbar::SProfilerToolbar()
{
}


SProfilerToolbar::~SProfilerToolbar()
{
	// Remove ourselves from the profiler manager.
	if(FProfilerManager::Get().IsValid())
	{
		FProfilerManager::Get()->OnSessionInstancesUpdated().RemoveAll(this);
	}
}


void SProfilerToolbar::Construct(const FArguments& InArgs)
{
	CreateCommands();

	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder)
		{
			ToolbarBuilder.BeginSection("File");
			{
				ToolbarBuilder.AddToolBarButton(FProfilerCommands::Get().ProfilerManager_Load);
				ToolbarBuilder.AddToolBarButton(FProfilerCommands::Get().ProfilerManager_LoadMultiple);
				ToolbarBuilder.AddToolBarButton(FProfilerCommands::Get().ProfilerManager_Save);
			}
			ToolbarBuilder.EndSection();
			ToolbarBuilder.BeginSection("Capture");
			{
				ToolbarBuilder.AddToolBarButton(FProfilerCommands::Get().ToggleDataPreview);
				ToolbarBuilder.AddToolBarButton(FProfilerCommands::Get().ProfilerManager_ToggleLivePreview);
				ToolbarBuilder.AddToolBarButton(FProfilerCommands::Get().ToggleDataCapture);
			}
			ToolbarBuilder.EndSection();
			ToolbarBuilder.BeginSection("Profilers");
			{
				ToolbarBuilder.AddToolBarButton(FProfilerCommands::Get().StatsProfiler);
				//ToolbarBuilder.AddToolBarButton(FProfilerCommands::Get().MemoryProfiler);
				ToolbarBuilder.AddToolBarButton(FProfilerCommands::Get().FPSChart);
			}
			ToolbarBuilder.EndSection();
			ToolbarBuilder.BeginSection("Options");
			{
				ToolbarBuilder.AddToolBarButton(FProfilerCommands::Get().OpenSettings);
			}
			ToolbarBuilder.EndSection();
		}
	};

	TSharedPtr<FUICommandList> ProfilerCommandList = FProfilerManager::Get()->GetCommandList();
	FToolBarBuilder ToolbarBuilder(ProfilerCommandList.ToSharedRef(), FMultiBoxCustomization::None);
	Local::FillToolbar(ToolbarBuilder);

	// Create the tool bar!
	ChildSlot
	[
		SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.FillWidth(1.0)
		.Padding(0.0f)
		[
			SNew(SBorder)
			.Padding(0)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
			[
				ToolbarBuilder.MakeWidget()
			]
		]
	];
}


void SProfilerToolbar::ShowStats()
{
	// do nothing
}


void SProfilerToolbar::ShowMemory()
{
	// do nothing
}


void DisplayFPSChart(const TSharedRef<FFPSAnalyzer> InFPSAnalyzer)
{
	static bool HasRegisteredFPSChart = false;
	if (!HasRegisteredFPSChart)
	{
		TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("FPSChart_Layout")
			->AddArea
			(
			FTabManager::NewArea(720, 360)
			->Split
			(
			FTabManager::NewStack()
			->AddTab("FPSChart", ETabState::ClosedTab)
			)
			);

		FGlobalTabmanager::Get()->RestoreFrom(Layout, TSharedPtr<SWindow>());
		HasRegisteredFPSChart = true;
	}

	FGlobalTabmanager::Get()->InsertNewDocumentTab
	(
		"FPSChart", FTabManager::ESearchPreference::RequireClosedTab,
		SNew(SDockTab)
		.Label(LOCTEXT("Label_FPSHistogram", "FPS Histogram"))
		.TabRole(ETabRole::DocumentTab)
		[
			SNew(SProfilerFPSChartPanel)
			.FPSAnalyzer(InFPSAnalyzer)
		]
	);
}


void SProfilerToolbar::ShowFPSChart()
{
	const TSharedPtr<FProfilerSession> ProfilerSession = FProfilerManager::Get()->GetProfilerSession();
	if (ProfilerSession.IsValid())
	{
		DisplayFPSChart(ProfilerSession->FPSAnalyzer);
	}
}


void SProfilerToolbar::CreateCommands()
{
	TSharedPtr<FUICommandList> ProfilerCommandList = FProfilerManager::Get()->GetCommandList();
	const FProfilerCommands& Commands = FProfilerCommands::Get();

	// Save command
	ProfilerCommandList->MapAction(Commands.ProfilerManager_Save,
		FExecuteAction(),
		FCanExecuteAction::CreateRaw(this, &SProfilerToolbar::IsImplemented)
	);

	// Stats command
	ProfilerCommandList->MapAction(Commands.StatsProfiler,
		FExecuteAction::CreateRaw(this, &SProfilerToolbar::ShowStats),
		FCanExecuteAction(),
		FIsActionChecked::CreateRaw(this, &SProfilerToolbar::IsShowingStats)
	);

	// Memory command
	ProfilerCommandList->MapAction(Commands.MemoryProfiler,
		FExecuteAction::CreateRaw(this, &SProfilerToolbar::ShowMemory),
		FCanExecuteAction(),
		FIsActionChecked::CreateRaw(this, &SProfilerToolbar::IsShowingMemory)
		);

	// FPSChart command
	ProfilerCommandList->MapAction(Commands.FPSChart,
		FExecuteAction::CreateRaw(this, &SProfilerToolbar::ShowFPSChart),
		FCanExecuteAction()
		);
}


#undef LOCTEXT_NAMESPACE
