// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SProfilerWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SEventGraph.h"
#include "SlateOptMacros.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "EditorStyleSet.h"
#include "Widgets/SProfilerToolbar.h"
#include "Widgets/SFiltersAndPresets.h"
#include "Widgets/SMultiDumpBrowser.h"
#include "Widgets/SDataGraph.h"
#include "Widgets/SProfilerMiniView.h"
#include "Widgets/SProfilerGraphPanel.h"
#include "Widgets/SProfilerSettings.h"
#include "Widgets/Notifications/SNotificationList.h"

#if WITH_EDITOR
	#include "Runtime/Analytics/Analytics/Public/AnalyticsEventAttribute.h"
	#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
	#include "EngineAnalytics.h"
#endif // WITH_EDITOR


#define LOCTEXT_NAMESPACE "SProfilerWindow"


static FText GetTextForNotification( const EProfilerNotificationTypes NotificatonType, const ELoadingProgressStates ProgressState, const FString& Filename, const float ProgressPercent = 0.0f )
{
	FText Result;

	if( NotificatonType == EProfilerNotificationTypes::LoadingOfflineCapture )
	{
		if( ProgressState == ELoadingProgressStates::Started )
		{
			FFormatNamedArguments Args;
			Args.Add( TEXT("Filename"), FText::FromString( Filename ) );
			Result = FText::Format( LOCTEXT("DescF_OfflineCapture_Started", "Started loading a file ../../{Filename}"), Args );
		}
		else if( ProgressState == ELoadingProgressStates::InProgress )
		{
			FFormatNamedArguments Args;
			Args.Add( TEXT("Filename"), FText::FromString( Filename ) );
			Args.Add( TEXT("DataLoadingProgressPercent"), FText::AsPercent( ProgressPercent ) );
			Result = FText::Format( LOCTEXT("DescF_OfflineCapture_InProgress", "Loading a file ../../{Filename} {DataLoadingProgressPercent}"), Args );
		}
		else if( ProgressState == ELoadingProgressStates::Loaded )
		{
			FFormatNamedArguments Args;
			Args.Add( TEXT("Filename"), FText::FromString( Filename ) );
			Result = FText::Format( LOCTEXT("DescF_OfflineCapture_Loaded", "Capture file ../../{Filename} has been successfully loaded"), Args );
		}
		else if( ProgressState == ELoadingProgressStates::Failed )
		{
			FFormatNamedArguments Args;
			Args.Add( TEXT("Filename"), FText::FromString( Filename ) );
			Result = FText::Format( LOCTEXT("DescF_OfflineCapture_Failed", "Failed to load capture file ../../{Filename}"), Args );
		}
	}
	else if( NotificatonType == EProfilerNotificationTypes::SendingServiceSideCapture )
	{
		if( ProgressState == ELoadingProgressStates::Started )
		{
			FFormatNamedArguments Args;
			Args.Add( TEXT("Filename"), FText::FromString( Filename ) );
			Result = FText::Format( LOCTEXT("DescF_ServiceSideCapture_Started", "Started receiving a file ../../{Filename}"), Args );
		}
		else if( ProgressState == ELoadingProgressStates::InProgress )
		{
			FFormatNamedArguments Args;
			Args.Add( TEXT("Filename"), FText::FromString( Filename ) );
			Args.Add( TEXT("DataLoadingProgressPercent"), FText::AsPercent( ProgressPercent ) );
			Result = FText::Format( LOCTEXT("DescF_ServiceSideCapture_InProgress", "Receiving a file ../../{Filename} {DataLoadingProgressPercent}"), Args );
		}
		else if( ProgressState == ELoadingProgressStates::Loaded )
		{
			FFormatNamedArguments Args;
			Args.Add( TEXT("Filename"), FText::FromString( Filename ) );
			Result = FText::Format( LOCTEXT("DescF_ServiceSideCapture_Loaded", "Capture file ../../{Filename} has been successfully received"), Args );
		}
		else if( ProgressState == ELoadingProgressStates::Failed )
		{
			FFormatNamedArguments Args;
			Args.Add( TEXT("Filename"), FText::FromString( Filename ) );
			Result = FText::Format( LOCTEXT("DescF_ServiceSideCapture_Failed", "Failed to receive capture file ../../{Filename}"), Args );
		}
	}

	return Result;
}

SProfilerWindow::SProfilerWindow()
	: DurationActive(0.0f)
{}

SProfilerWindow::~SProfilerWindow()
{
	// Remove ourselves from the profiler manager.
	if( FProfilerManager::Get().IsValid() )
	{
		FProfilerManager::Get()->OnViewModeChanged().RemoveAll( this );
	}

#if WITH_EDITOR
	if (DurationActive > 0.0f && FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.Profiler"), FAnalyticsEventAttribute(TEXT("Duration"), DurationActive));
	}
#endif // WITH_EDITOR
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SProfilerWindow::Construct( const FArguments& InArgs )
{
	// TODO: Cleanup overlay code.
	ChildSlot
		[
			SNew(SOverlay)

			// Overlay slot for the main profiler window area, the first
			+ SOverlay::Slot()
				[
					SAssignNew(MainContentPanel, SVerticalBox)

					+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.Padding(0.0f)
								.HAlign(HAlign_Fill)
								.VAlign(VAlign_Fill)
								[
									SNew(SProfilerToolbar)
								]
						]

					/** Profiler Mini-view. */
					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 6.0f, 0.0f, 0.0f)
						[
							SNew(SBox)
								.HeightOverride(48.0f)
								.IsEnabled(this, &SProfilerWindow::IsProfilerEnabled)
								[
									SNew(SHorizontalBox)

									+ SHorizontalBox::Slot()
										.FillWidth(1.0f)
										.Padding(0.0f)
										.HAlign(HAlign_Fill)
										.VAlign(VAlign_Fill)
										[
											SAssignNew(ProfilerMiniView, SProfilerMiniView)
										]
								]
						]

					+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						.Padding(0.0f, 4.0f, 0.0f, 0.0f)
						[
							SNew(SSplitter)
								.Orientation(Orient_Horizontal)

							+ SSplitter::Slot()
								.Value(0.25f)
								[
									SNew(SSplitter)
									.Orientation(Orient_Vertical)
									+ SSplitter::Slot()
										.Value(0.25f)
										[
											SNew(SVerticalBox)
											+ SVerticalBox::Slot()
											.AutoHeight()
												[
													SNew(SHorizontalBox)
													+ SHorizontalBox::Slot()
													.AutoWidth()
														[
															SNew(SImage)
															.Image(FEditorStyle::GetBrush(TEXT("Profiler.Tab.FiltersAndPresets")))
														]

													+ SHorizontalBox::Slot()
														.AutoWidth()
														[
															SNew(STextBlock)
															.Text(LOCTEXT("MultiFileBrowser", "Stats dump browser"))
														]
												]
											+ SVerticalBox::Slot()
												.AutoHeight()
												[
													//SNew(STextBlock)
													//.Text(LOCTEXT("FileList", "File list here"))
													SAssignNew(MultiDumpBrowser, SMultiDumpBrowser)
												]
										]

									+ SSplitter::Slot()
										.Expose(FiltersAndPresetsSlot)
										[
											SNew(SVerticalBox)
											.IsEnabled(this, &SProfilerWindow::IsProfilerEnabled)

											// Header
											+ SVerticalBox::Slot()
												.AutoHeight()
												[
													SNew(SHorizontalBox)

													+ SHorizontalBox::Slot()
														.AutoWidth()
														[
															SNew(SImage)
																.Image(FEditorStyle::GetBrush(TEXT("Profiler.Tab.FiltersAndPresets")))
														]

													+ SHorizontalBox::Slot()
														.AutoWidth()
														[
															SNew(STextBlock)
																.Text( LOCTEXT("FiltersAndPresetsLabel", "Filters And Presets") )
														]
												]

											// Filters And Presets
											+ SVerticalBox::Slot()
												.FillHeight(1.0f)
												.Padding(0.0f, 2.0f, 0.0f, 0.0f)
												[
													SAssignNew(FiltersAndPresets, SFiltersAndPresets)
												]
										]
								]

							+ SSplitter::Slot()
								.Value(0.75f)
								[
									SNew(SSplitter)
										.Orientation(Orient_Vertical)
										//.PhysicalSplitterHandleSize( 2.0f )
										//.HitDetectionSplitterHandleSize( 4.0f )

										+ SSplitter::Slot()
											.Value(0.25f)
											[
												SNew(SVerticalBox)

												// Header
												+SVerticalBox::Slot()
													.AutoHeight()
														[
															SNew(SHorizontalBox)

															+ SHorizontalBox::Slot()
																.AutoWidth()
																[
																	SNew(SImage)
																		.Image(FEditorStyle::GetBrush(TEXT("Profiler.Tab.GraphView")))
																]

															+ SHorizontalBox::Slot()
																.AutoWidth()
																[
																	SNew(STextBlock)
																		.Text(LOCTEXT("GraphViewLabel", "Graph View"))
																]
														]
							
												// Graph View
												+ SVerticalBox::Slot()
													.FillHeight( 1.0f )
													.Padding(0.0f, 2.0f, 0.0f, 0.0f)
													[
														SAssignNew(GraphPanel, SProfilerGraphPanel)
													]
											]

										+ SSplitter::Slot()
											.Value(0.75f)
											[
												SAssignNew(EventGraphPanel, SVerticalBox)
											]
									]
							]
					]

			// session hint overlay
			+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SBorder)
						.BorderImage(FEditorStyle::GetBrush("NotificationList.ItemBackground"))
						.Padding(8.0f)
						.Visibility(this, &SProfilerWindow::IsSessionOverlayVissible)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("SelectSessionOverlayText", "Please select a session from the Session Browser or load a saved capture."))
						]
				]

			// notification area overlay
			+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
				.Padding(16.0f)
				[
					SAssignNew(NotificationList, SNotificationList)
				]

			// profiler settings overlay
			+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Expose(OverlaySettingsSlot)
		];

	ProfilerMiniView->OnSelectionBoxChanged().AddSP( GraphPanel.ToSharedRef(), &SProfilerGraphPanel::MiniView_OnSelectionBoxChanged );
	FProfilerManager::Get()->OnViewModeChanged().AddSP( this, &SProfilerWindow::ProfilerManager_OnViewModeChanged );
	GraphPanel->ProfilerMiniView = ProfilerMiniView;
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SProfilerWindow::ManageEventGraphTab( const FGuid ProfilerInstanceID, const bool bCreateFakeTab, const FString TabName )
{
	// TODO: Add support for multiple instances.
	if( bCreateFakeTab )
	{
		TSharedPtr<SEventGraph> EventGraphWidget;

		EventGraphPanel->ClearChildren();

		// Header
		EventGraphPanel->AddSlot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SImage)
				.Image( FEditorStyle::GetBrush( TEXT("Profiler.Tab.EventGraph") ) )
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text( FText::FromString(TabName) )
			]
		];

		EventGraphPanel->AddSlot()
		.AutoHeight()
		[
			SNew( SSpacer )
			.Size( FVector2D( 2.0f, 2.0f ) )
		];

		// Event graph
		EventGraphPanel->AddSlot()
		.FillHeight( 1.0f )
		[
			SAssignNew(EventGraphWidget,SEventGraph)
		];

		ActiveEventGraphs.Add( ProfilerInstanceID, EventGraphWidget.ToSharedRef() );

		// Register data graph with the new event graph tab.
		EventGraphWidget->OnEventGraphRestoredFromHistory().AddSP( GraphPanel->GetMainDataGraph().Get(), &SDataGraph::EventGraph_OnRestoredFromHistory );
	}
	else
	{
		ActiveEventGraphs.Remove( ProfilerInstanceID );
	}
}

void SProfilerWindow::UpdateEventGraph( const FGuid ProfilerInstanceID, const FEventGraphDataRef AverageEventGraph, const FEventGraphDataRef MaximumEventGraph, bool bInitial )
{
	const auto* EventGraph = ActiveEventGraphs.Find( ProfilerInstanceID );
	if( EventGraph )
	{
		(*EventGraph)->SetNewEventGraphState( AverageEventGraph, MaximumEventGraph, bInitial );
	}
}

EVisibility SProfilerWindow::IsSessionOverlayVissible() const
{
	if( FProfilerManager::Get()->HasValidSession() )
	{
		return EVisibility::Hidden;
	}

	return EVisibility::Visible;
}


bool SProfilerWindow::IsProfilerEnabled() const
{
	const bool bIsActive = FProfilerManager::Get()->IsConnected() || FProfilerManager::Get()->IsCaptureFileFullyProcessed();
	return bIsActive;
}


void SProfilerWindow::ManageLoadingProgressNotificationState( const FString& Filename, const EProfilerNotificationTypes NotificatonType, const ELoadingProgressStates ProgressState, const float DataLoadingProgress )
{
	const FString BaseFilename = FPaths::GetBaseFilename( Filename );

	if( ProgressState == ELoadingProgressStates::Started )
	{
		const bool bContains = ActiveNotifications.Contains( Filename );
		if( !bContains )
		{
			FNotificationInfo NotificationInfo( GetTextForNotification(NotificatonType,ProgressState,BaseFilename) );
			NotificationInfo.bFireAndForget = false;
			NotificationInfo.bUseLargeFont = false;

			// Add two buttons, one for cancel, one for loading the received file.
			if( NotificatonType == EProfilerNotificationTypes::SendingServiceSideCapture )
			{
				const FText CancelButtonText = LOCTEXT("CancelButton_Text", "Cancel");
				const FText CancelButtonTT = LOCTEXT("CancelButton_TTText", "Hides this notification");
				const FText LoadButtonText = LOCTEXT("LoadButton_Text", "Load file");
				const FText LoadButtonTT = LOCTEXT("LoadButton_TTText", "Loads the received file and hides this notification");

				NotificationInfo.ButtonDetails.Add( FNotificationButtonInfo( CancelButtonText, CancelButtonTT, FSimpleDelegate::CreateSP( this, &SProfilerWindow::SendingServiceSideCapture_Cancel, Filename ), SNotificationItem::CS_Success ) );
				NotificationInfo.ButtonDetails.Add( FNotificationButtonInfo( LoadButtonText, LoadButtonTT, FSimpleDelegate::CreateSP( this, &SProfilerWindow::SendingServiceSideCapture_Load, Filename ), SNotificationItem::CS_Success ) );
			}

			SNotificationItemWeak LoadingProgress = NotificationList->AddNotification(NotificationInfo);

			LoadingProgress.Pin()->SetCompletionState( SNotificationItem::CS_Pending );

			ActiveNotifications.Add( Filename, LoadingProgress );
		}
	}
	else if( ProgressState == ELoadingProgressStates::InProgress )
	{
		const SNotificationItemWeak* LoadingProgressPtr = ActiveNotifications.Find( Filename );
		if( LoadingProgressPtr )
		{
			TSharedPtr<SNotificationItem> LoadingProcessPinned = LoadingProgressPtr->Pin();
			LoadingProcessPinned->SetText( GetTextForNotification( NotificatonType, ProgressState, BaseFilename, DataLoadingProgress ) );
			LoadingProcessPinned->SetCompletionState( SNotificationItem::CS_Pending );
		}
	}
	else if( ProgressState == ELoadingProgressStates::Loaded )
	{
		const SNotificationItemWeak* LoadingProgressPtr = ActiveNotifications.Find( Filename );
		if( LoadingProgressPtr )
		{
			TSharedPtr<SNotificationItem> LoadingProcessPinned = LoadingProgressPtr->Pin();
			LoadingProcessPinned->SetText( GetTextForNotification( NotificatonType, ProgressState, BaseFilename ) );
			LoadingProcessPinned->SetCompletionState( SNotificationItem::CS_Success );
			
			// Notifications for received files are handled by ty user.
			if( NotificatonType == EProfilerNotificationTypes::LoadingOfflineCapture )
			{
				LoadingProcessPinned->ExpireAndFadeout();
				ActiveNotifications.Remove( Filename );
			}
		}
	}
	else if( ProgressState == ELoadingProgressStates::Failed )
	{
		const SNotificationItemWeak* LoadingProgressPtr = ActiveNotifications.Find( Filename );
		if( LoadingProgressPtr )
		{
			TSharedPtr<SNotificationItem> LoadingProcessPinned = LoadingProgressPtr->Pin();
			LoadingProcessPinned->SetText( GetTextForNotification(NotificatonType, ProgressState, BaseFilename) );
			LoadingProcessPinned->SetCompletionState(SNotificationItem::CS_Fail);

			LoadingProcessPinned->ExpireAndFadeout();
			ActiveNotifications.Remove( Filename );
		}
	}
	else if (ProgressState == ELoadingProgressStates::Cancelled)
	{
		const SNotificationItemWeak* LoadingProgressPtr = ActiveNotifications.Find( Filename );
		if ( LoadingProgressPtr )
		{
			TSharedPtr<SNotificationItem> LoadingProcessPinned = LoadingProgressPtr->Pin();
			LoadingProcessPinned->ExpireAndFadeout();
			ActiveNotifications.Remove( Filename );
		}
	}
}

void SProfilerWindow::SendingServiceSideCapture_Cancel( const FString Filename )
{
	const SNotificationItemWeak* LoadingProgressPtr = ActiveNotifications.Find( Filename );
	if( LoadingProgressPtr )
	{
		TSharedPtr<SNotificationItem> LoadingProcessPinned = LoadingProgressPtr->Pin(); 
		LoadingProcessPinned->ExpireAndFadeout();
		ActiveNotifications.Remove( Filename );
	}
}

void SProfilerWindow::SendingServiceSideCapture_Load( const FString Filename )
{
	const SNotificationItemWeak* LoadingProgressPtr = ActiveNotifications.Find( Filename );
	if( LoadingProgressPtr )
	{
		TSharedPtr<SNotificationItem> LoadingProcessPinned = LoadingProgressPtr->Pin();
		LoadingProcessPinned->ExpireAndFadeout();
		ActiveNotifications.Remove( Filename );

		// TODO: Move to a better place.
		const FString PathName = FPaths::ProfilingDir() + TEXT("UnrealStats/Received/");
		const FString StatFilepath = PathName + Filename;
		FProfilerManager::Get()->LoadProfilerCapture( StatFilepath );
	}
}

EActiveTimerReturnType SProfilerWindow::UpdateActiveDuration( double InCurrentTime, float InDeltaTime )
{
	DurationActive += InDeltaTime;

	// The profiler window will explicitly unregister this active timer when the mouse leaves
	return EActiveTimerReturnType::Continue;
}


void SProfilerWindow::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);

	if (!ActiveTimerHandle.IsValid())
	{
		ActiveTimerHandle = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SProfilerWindow::UpdateActiveDuration));
	}
}

void SProfilerWindow::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SCompoundWidget::OnMouseLeave(MouseEvent);

	auto PinnedActiveTimerHandle = ActiveTimerHandle.Pin();
	if (PinnedActiveTimerHandle.IsValid())
	{
		UnRegisterActiveTimer(PinnedActiveTimerHandle.ToSharedRef());
	}
	
}

FReply SProfilerWindow::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	return FProfilerManager::Get()->GetCommandList()->ProcessCommandBindings( InKeyEvent ) ? FReply::Handled() : FReply::Unhandled();
}

FReply SProfilerWindow::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FExternalDragOperation> DragDropOp = DragDropEvent.GetOperationAs<FExternalDragOperation>();
	if(DragDropOp.IsValid())
	{
		if( DragDropOp->HasFiles() )
		{
			const TArray<FString>& Files = DragDropOp->GetFiles();
			if( Files.Num() == 1 )
			{
				const FString DraggedFileExtension = FPaths::GetExtension( Files[0], true );
				if( DraggedFileExtension == FStatConstants::StatsFileExtension )
				{
					return FReply::Handled();

				}
				else if( DraggedFileExtension == FStatConstants::StatsFileRawExtension )
				{
					return FReply::Handled();
				}
			}
		}
	}

	return SCompoundWidget::OnDragOver(MyGeometry,DragDropEvent);
}

FReply SProfilerWindow::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FExternalDragOperation> DragDropOp = DragDropEvent.GetOperationAs<FExternalDragOperation>();
	if(DragDropOp.IsValid())
	{
		if( DragDropOp->HasFiles() )
		{
			// For now, only allow a single file.
			const TArray<FString>& Files = DragDropOp->GetFiles();
			if( Files.Num() == 1 )
			{
				const FString DraggedFileExtension = FPaths::GetExtension( Files[0], true );
				if( DraggedFileExtension == FStatConstants::StatsFileExtension )
				{
					// Enqueue load operation.
					FProfilerManager::Get()->LoadProfilerCapture( Files[0] );
					return FReply::Handled();
				}
				else if( DraggedFileExtension == FStatConstants::StatsFileRawExtension )
				{
					// Enqueue load operation.
					FProfilerManager::Get()->LoadRawStatsFile( Files[0] );
					return FReply::Handled();
				}
			}
		}
	}

	return SCompoundWidget::OnDrop(MyGeometry,DragDropEvent);
}

void SProfilerWindow::OpenProfilerSettings()
{
	MainContentPanel->SetEnabled( false );
	(*OverlaySettingsSlot)
	[
		SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush("NotificationList.ItemBackground") )
		.Padding( 8.0f )
		[
		 	SNew(SProfilerSettings)
		 	.OnClose( this, &SProfilerWindow::CloseProfilerSettings )
			.SettingPtr( &FProfilerManager::GetSettings() )
		]
	];
}

void SProfilerWindow::CloseProfilerSettings()
{
	// Close the profiler settings by simply replacing widget with a null one.
	(*OverlaySettingsSlot)
	[
		SNullWidget::NullWidget
	];
	MainContentPanel->SetEnabled( true );
}

void SProfilerWindow::ProfilerManager_OnViewModeChanged( EProfilerViewMode NewViewMode )
{
	if( NewViewMode == EProfilerViewMode::LineIndexBased )
	{
		EventGraphPanel->SetVisibility( EVisibility::Visible );
		EventGraphPanel->SetEnabled( true );

		(*FiltersAndPresetsSlot)
		[
			FiltersAndPresets.ToSharedRef()
		];
	}
	else if( NewViewMode == EProfilerViewMode::ThreadViewTimeBased )
	{
		EventGraphPanel->SetVisibility( EVisibility::Collapsed );
		EventGraphPanel->SetEnabled( false );

		(*FiltersAndPresetsSlot)
		[
			SNullWidget::NullWidget
		];
	}
}


#undef LOCTEXT_NAMESPACE
