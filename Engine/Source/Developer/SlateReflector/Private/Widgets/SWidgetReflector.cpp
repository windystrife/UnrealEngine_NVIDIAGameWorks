// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SWidgetReflector.h"
#include "Rendering/DrawElements.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Framework/Docking/TabManager.h"
#include "Models/WidgetReflectorNode.h"
#include "Widgets/SWidgetReflectorTreeWidgetItem.h"
#include "Widgets/SWidgetReflectorToolTipWidget.h"
#include "ISlateReflectorModule.h"
#include "Stats/SlateStats.h"
#include "Widgets/SInvalidationPanel.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SWidgetSnapshotVisualizer.h"
#include "WidgetSnapshotService.h"
#include "Widgets/Input/SNumericDropDown.h"

#if SLATE_REFLECTOR_HAS_DESKTOP_PLATFORM
#include "DesktopPlatformModule.h"
#endif // SLATE_REFLECTOR_HAS_DESKTOP_PLATFORM

#if SLATE_REFLECTOR_HAS_SESSION_SERVICES
#include "ISessionManager.h"
#include "ISessionServicesModule.h"
#endif // SLATE_REFLECTOR_HAS_SESSION_SERVICES

#define LOCTEXT_NAMESPACE "SWidgetReflector"
#define WITH_EVENT_LOGGING 0

#if SLATE_STATS
extern SLATECORE_API int32 GSlateStatsFlatEnable;
extern SLATECORE_API int32 GSlateStatsFlatLogOutput;
extern SLATECORE_API int32 GSlateStatsHierarchyTrigger;
extern SLATECORE_API float GSlateStatsFlatIntervalWindowSec;
#endif
static const int32 MaxLoggedEvents = 100;

/**
 * Widget reflector user widget.
 */

/* Local helpers
 *****************************************************************************/

struct FLoggedEvent
{
	FLoggedEvent( const FInputEvent& InEvent, const FReplyBase& InReply )
		: Event(InEvent)
		, Handler(InReply.GetHandler())
		, EventText(InEvent.ToText())
		, HandlerText(InReply.GetHandler().IsValid() ? FText::FromString(InReply.GetHandler()->ToString()) : LOCTEXT("NullHandler", "null"))
	{ }

	FText ToText()
	{
		return FText::Format(LOCTEXT("LoggedEvent","{0}  |  {1}"), EventText, HandlerText);
	}
	
	FInputEvent Event;
	TWeakPtr<SWidget> Handler;
	FText EventText;
	FText HandlerText;
};

namespace WidgetReflectorImpl
{

/** Information about a potential widget snapshot target */
struct FWidgetSnapshotTarget
{
	/** Display name of the target (used in the UI) */
	FText DisplayName;

	/** Instance ID of the target */
	FGuid InstanceId;
};

/** Different UI modes the widget reflector can be in */
enum class EWidgetReflectorUIMode : uint8
{
	Live,
	Snapshot,
};

namespace WidgetReflectorTabID
{
	static const FName WidgetHierarchy = "WidgetReflector.WidgetHierarchyTab";
	static const FName SlateStats = "WidgetReflector.SlateStatsTab";
	static const FName SnapshotWidgetPicker = "WidgetReflector.SnapshotWidgetPickerTab";
}

/**
 * Widget reflector implementation
 */
class SWidgetReflector : public ::SWidgetReflector
{
	// The reflector uses a tree that observes FWidgetReflectorNodeBase objects.
	typedef STreeView<TSharedRef<FWidgetReflectorNodeBase>> SReflectorTree;

public:

	~SWidgetReflector();

private:

	virtual void Construct( const FArguments& InArgs ) override;

	TSharedRef<SDockTab> SpawnWidgetHierarchyTab(const FSpawnTabArgs& Args);
#if SLATE_STATS
	TSharedRef<SDockTab> SpawnSlateStatsTab(const FSpawnTabArgs& Args);
#endif
	TSharedRef<SDockTab> SpawnSnapshotWidgetPicker(const FSpawnTabArgs& Args);

	void OnTabSpawned(const FName& TabIdentifier, const TSharedRef<SDockTab>& SpawnedTab);

	void CloseTab(const FName& TabIdentifier);

	void SetUIMode(const EWidgetReflectorUIMode InNewMode);

	// SCompoundWidget overrides
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	// IWidgetReflector interface

	virtual void OnEventProcessed( const FInputEvent& Event, const FReplyBase& InReply ) override;

	virtual bool IsInPickingMode() const override
	{
		return bIsPicking;
	}

	virtual bool IsShowingFocus() const override
	{
		return bShowFocus;
	}

	virtual bool IsVisualizingLayoutUnderCursor() const override
	{
		return bIsPicking;
	}

	virtual void OnWidgetPicked() override
	{
		bIsPicking = false;
	}

	virtual bool ReflectorNeedsToDrawIn( TSharedRef<SWindow> ThisWindow ) const override;

	virtual void SetSourceAccessDelegate( FAccessSourceCode InDelegate ) override
	{
		SourceAccessDelegate = InDelegate;
	}

	virtual void SetAssetAccessDelegate(FAccessAsset InDelegate) override
	{
		AsseetAccessDelegate = InDelegate;
	}

	virtual void SetWidgetsToVisualize( const FWidgetPath& InWidgetsToVisualize ) override;
	virtual int32 Visualize( const FWidgetPath& InWidgetsToVisualize, FSlateWindowElementList& OutDrawElements, int32 LayerId ) override;
	virtual int32 VisualizeCursorAndKeys(FSlateWindowElementList& OutDrawElements, int32 LayerId) const override;

	/**
	 * Generates a tool tip for the given reflector tree node.
	 *
	 * @param InReflectorNode The node to generate the tool tip for.
	 * @return The tool tip widget.
	 */
	TSharedRef<SToolTip> GenerateToolTipForReflectorNode( TSharedRef<FWidgetReflectorNodeBase> InReflectorNode );

	/**
	 * Mark the provided reflector nodes such that they stand out in the tree and are visible.
	 *
	 * @param WidgetPathToObserve The nodes to mark.
	 */
	void VisualizeAsTree( const TArray< TSharedRef<FWidgetReflectorNodeBase> >& WidgetPathToVisualize );

	/**
	 * Draw the widget path to the picked widget as the widgets' outlines.
	 *
	 * @param InWidgetsToVisualize A widget path whose widgets' outlines to draw.
	 * @param OutDrawElements A list of draw elements; we will add the output outlines into it.
	 * @param LayerId The maximum layer achieved in OutDrawElements so far.
	 * @return The maximum layer ID we achieved while painting.
	 */
	int32 VisualizePickAsRectangles( const FWidgetPath& InWidgetsToVisualize, FSlateWindowElementList& OutDrawElements, int32 LayerId );

	/**
	 * Draw an outline for the specified nodes.
	 *
	 * @param InNodesToDraw A widget path whose widgets' outlines to draw.
	 * @param WindowGeometry The geometry of the window in which to draw.
	 * @param OutDrawElements A list of draw elements; we will add the output outlines into it.
	 * @param LayerId the maximum layer achieved in OutDrawElements so far.
	 * @return The maximum layer ID we achieved while painting.
	 */
	int32 VisualizeSelectedNodesAsRectangles( const TArray<TSharedRef<FWidgetReflectorNodeBase>>& InNodesToDraw, const TSharedRef<SWindow>& VisualizeInWindow, FSlateWindowElementList& OutDrawElements, int32 LayerId );

	/** Callback for changing the application scale slider. */
	void HandleAppScaleSliderChanged( float NewValue )
	{
		FSlateApplication::Get().SetApplicationScale(NewValue);
	}

	FReply HandleDisplayTextureAtlases();
	FReply HandleDisplayFontAtlases();

	/** Callback for getting the value of the application scale slider. */
	float HandleAppScaleSliderValue() const
	{
		return FSlateApplication::Get().GetApplicationScale();
	}

	/** Callback for checked state changes of the focus check box. */
	void HandleFocusCheckBoxCheckedStateChanged( ECheckBoxState NewValue );

	/** Callback for getting the checked state of the focus check box. */
	ECheckBoxState HandleFocusCheckBoxIsChecked() const
	{
		return bShowFocus ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	/** Callback for getting the text of the frame rate text block. */
	FString HandleFrameRateText() const;

	/** Callback for clicking the pick button. */
	FReply HandlePickButtonClicked()
	{
		bIsPicking = !bIsPicking;

		if (bIsPicking)
		{
			bShowFocus = false;
			SetUIMode(EWidgetReflectorUIMode::Live);
			SInvalidationPanel::SetEnableWidgetCaching(false);
		}
		else
		{
			SInvalidationPanel::SetEnableWidgetCaching(true);
		}

		return FReply::Handled();
	}

	/** Callback for getting the color of the pick button text. */
	FSlateColor HandlePickButtonColorAndOpacity() const
	{
		static const FName SelectionColor("SelectionColor");

		return bIsPicking
			? FCoreStyle::Get().GetSlateColor(SelectionColor)
			: FLinearColor::White;
	}

	/** Callback for getting the text of the pick button. */
	FText HandlePickButtonText() const;

	/** Callback to see whether the "Snapshot Target" combo should be enabled */
	bool IsSnapshotTargetComboEnabled() const;

	/** Callback to see whether the "Take Snapshot" button should be enabled */
	bool IsTakeSnapshotButtonEnabled() const;

	/** Callback for clicking the "Take Snapshot" button. */
	FReply HandleTakeSnapshotButtonClicked();

	/** Takes a snapshot of the current state of the snapshot target. */
	void TakeSnapshot();

	/** Used as a callback for the "snapshot pending" notification item buttons, called when we should give up on a snapshot request */
	void OnCancelPendingRemoteSnapshot();

	/** Callback for when a remote widget snapshot is available */
	void HandleRemoteSnapshotReceived(const TArray<uint8>& InSnapshotData);

#if SLATE_REFLECTOR_HAS_DESKTOP_PLATFORM
	/** Callback for clicking the "Load Snapshot" button. */
	FReply HandleLoadSnapshotButtonClicked();
#endif // SLATE_REFLECTOR_HAS_DESKTOP_PLATFORM

	/** Called to update the list of available snapshot targets */
	void UpdateAvailableSnapshotTargets();

	/** Called to update the currently selected snapshot target (after the list has been refreshed) */
	void UpdateSelectedSnapshotTarget();

	/** Called when the list of available snapshot targets changes */
	void OnAvailableSnapshotTargetsChanged();

	/** Get the display name of the currently selected snapshot target */
	FText GetSelectedSnapshotTargetDisplayName() const;

	/** Generate a row widget for the available targets combo box */
	TSharedRef<SWidget> HandleGenerateAvailableSnapshotComboItemWidget(TSharedPtr<FWidgetSnapshotTarget> InItem) const;
	
	/** Update the selected target when the combo box selection is changed */
	void HandleAvailableSnapshotComboSelectionChanged(TSharedPtr<FWidgetSnapshotTarget> InItem, ESelectInfo::Type InSeletionInfo);

	/** Callback for generating a row in the reflector tree view. */
	TSharedRef<ITableRow> HandleReflectorTreeGenerateRow( TSharedRef<FWidgetReflectorNodeBase> InReflectorNode, const TSharedRef<STableViewBase>& OwnerTable );

	/** Callback for getting the child items of the given reflector tree node. */
	void HandleReflectorTreeGetChildren( TSharedRef<FWidgetReflectorNodeBase> InReflectorNode, TArray<TSharedRef<FWidgetReflectorNodeBase>>& OutChildren );

	/** Callback for when the selection in the reflector tree has changed. */
	void HandleReflectorTreeSelectionChanged( TSharedPtr<FWidgetReflectorNodeBase>, ESelectInfo::Type /*SelectInfo*/ );

	TSharedRef<ITableRow> GenerateEventLogRow( TSharedRef<FLoggedEvent> InReflectorNode, const TSharedRef<STableViewBase>& OwnerTable );

	EWidgetReflectorUIMode CurrentUIMode;

	TSharedPtr<FTabManager> TabManager;
	TMap<FName, TWeakPtr<SDockTab>> SpawnedTabs;

	TArray< TSharedRef<FLoggedEvent> > LoggedEvents;
	TSharedPtr< SListView< TSharedRef< FLoggedEvent > > > EventListView;
	TSharedPtr<SReflectorTree> ReflectorTree;

	TArray<TSharedRef<FWidgetReflectorNodeBase>> SelectedNodes;
	TArray<TSharedRef<FWidgetReflectorNodeBase>> ReflectorTreeRoot;
	TArray<TSharedRef<FWidgetReflectorNodeBase>> PickedPath;

	/** When working with a snapshotted tree, this will contain the snapshot hierarchy and screenshot info */
	FWidgetSnapshotData SnapshotData;
	TSharedPtr<SWidgetSnapshotVisualizer> WidgetSnapshotVisualizer;

	/** List of available snapshot targets, as well as the one we currently have selected */
	TSharedPtr<SComboBox<TSharedPtr<FWidgetSnapshotTarget>>> AvailableSnapshotTargetsComboBox;
	TArray<TSharedPtr<FWidgetSnapshotTarget>> AvailableSnapshotTargets;
	FGuid SelectedSnapshotTargetInstanceId;
	TSharedPtr<FWidgetSnapshotService> WidgetSnapshotService;
	TWeakPtr<SNotificationItem> WidgetSnapshotNotificationPtr;
	FGuid RemoteSnapshotRequestId;

	SSplitter::FSlot* WidgetInfoLocation;

	FAccessSourceCode SourceAccessDelegate;
	FAccessAsset AsseetAccessDelegate;

	bool bShowFocus;
	bool bIsPicking;

#if SLATE_STATS
	// STATS
	TSharedPtr<SBorder> StatsBorder;
	TArray< TSharedRef<class FStatItem> > StatsItems;
	TSharedPtr< SListView< TSharedRef<FStatItem> > > StatsList;

	TSharedRef<SWidget> MakeStatViewer();
	void UpdateStats();
	TSharedRef<ITableRow> GenerateStatRow(TSharedRef<FStatItem> StatItem, const TSharedRef<STableViewBase>& OwnerTable);
#endif


private:
	// DEMO MODE
	bool bEnableDemoMode;
	double LastMouseClickTime;
	FVector2D CursorPingPosition;

	float SnapshotDelay;
	bool bIsPendingDelayedSnapshot;
	double TimeOfScheduledSnapshot;
};


SWidgetReflector::~SWidgetReflector()
{
	TabManager->UnregisterTabSpawner(WidgetReflectorTabID::WidgetHierarchy);
	TabManager->UnregisterTabSpawner(WidgetReflectorTabID::SlateStats);
	TabManager->UnregisterTabSpawner(WidgetReflectorTabID::SnapshotWidgetPicker);
}

void SWidgetReflector::Construct( const FArguments& InArgs )
{
	LoggedEvents.Reserve(MaxLoggedEvents);

	CurrentUIMode = EWidgetReflectorUIMode::Live;

	bShowFocus = false;
	bIsPicking = false;

	bEnableDemoMode = false;
	LastMouseClickTime = -1.0;
	CursorPingPosition = FVector2D::ZeroVector;

	SnapshotDelay = 0.0f;
	bIsPendingDelayedSnapshot = false;
	TimeOfScheduledSnapshot = -1.0;

	WidgetSnapshotService = InArgs._WidgetSnapshotService;

#if SLATE_REFLECTOR_HAS_SESSION_SERVICES
	{
		TSharedRef<ISessionManager> SessionManager = FModuleManager::LoadModuleChecked<ISessionServicesModule>("SessionServices").GetSessionManager();
		SessionManager->OnSessionsUpdated().AddSP(this, &SWidgetReflector::OnAvailableSnapshotTargetsChanged);
	}
#endif // SLATE_REFLECTOR_HAS_SESSION_SERVICES
	SelectedSnapshotTargetInstanceId = FApp::GetInstanceId();
	UpdateAvailableSnapshotTargets();

#if SLATE_STATS
	const FName TabLayoutName = "WidgetReflector_Layout_v1";
#else
	const FName TabLayoutName = "WidgetReflector_Layout_NoStats_v1";
#endif

	TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(TabLayoutName)
	->AddArea
	(
		FTabManager::NewPrimaryArea()
		->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->SetHideTabWell(true)
			->SetSizeCoefficient(0.7f)
			->AddTab(WidgetReflectorTabID::WidgetHierarchy, ETabState::OpenedTab)
		)
		->Split
		(
			FTabManager::NewStack()
			->SetHideTabWell(true)
			->SetSizeCoefficient(0.3f)
#if SLATE_STATS
			->AddTab(WidgetReflectorTabID::SlateStats, ETabState::ClosedTab)
#endif
			->AddTab(WidgetReflectorTabID::SnapshotWidgetPicker, ETabState::ClosedTab)
		)
	);

	auto RegisterTrackedTabSpawner = [this](const FName& TabId, const FOnSpawnTab& OnSpawnTab) -> FTabSpawnerEntry&
	{
		return TabManager->RegisterTabSpawner(TabId, FOnSpawnTab::CreateLambda([this, OnSpawnTab](const FSpawnTabArgs& Args) -> TSharedRef<SDockTab>
		{
			TSharedRef<SDockTab> SpawnedTab = OnSpawnTab.Execute(Args);
			OnTabSpawned(Args.GetTabId().TabType, SpawnedTab);
			return SpawnedTab;
		}));
	};

	check(InArgs._ParentTab.IsValid());
	TabManager = FGlobalTabmanager::Get()->NewTabManager(InArgs._ParentTab.ToSharedRef());

	RegisterTrackedTabSpawner(WidgetReflectorTabID::WidgetHierarchy, FOnSpawnTab::CreateSP(this, &SWidgetReflector::SpawnWidgetHierarchyTab))
		.SetDisplayName(LOCTEXT("WidgetHierarchyTab", "Widget Hierarchy"));

#if SLATE_STATS
	RegisterTrackedTabSpawner(WidgetReflectorTabID::SlateStats, FOnSpawnTab::CreateSP(this, &SWidgetReflector::SpawnSlateStatsTab))
		.SetDisplayName(LOCTEXT("SlateStatsTab", "Slate Stats"));
#endif

	RegisterTrackedTabSpawner(WidgetReflectorTabID::SnapshotWidgetPicker, FOnSpawnTab::CreateSP(this, &SWidgetReflector::SpawnSnapshotWidgetPicker))
		.SetDisplayName(LOCTEXT("SnapshotWidgetPickerTab", "Snapshot Widget Picker"));

	this->ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.BorderBackgroundColor(FLinearColor::Gray) // Darken the outer border
		[
			SNew(SVerticalBox)
				
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f))
			[
				SNew(SHorizontalBox)
					
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AppScale", "Application Scale: "))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.MinDesiredWidth(100)
					.MaxDesiredWidth(250)
					[
						SNew(SSpinBox<float>)
						.Value(this, &SWidgetReflector::HandleAppScaleSliderValue)
						.MinValue(0.50f)
						.MaxValue(3.0f)
						.Delta(0.01f)
						.OnValueChanged(this, &SWidgetReflector::HandleAppScaleSliderChanged)
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5.0f, 0.0f))
				[
					SNew(SCheckBox)
					.Style(FCoreStyle::Get(), "ToggleButtonCheckbox")
					.IsChecked_Lambda([&]()
					{
						return SInvalidationPanel::GetEnableWidgetCaching() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([&](const ECheckBoxState NewState)
					{
						SInvalidationPanel::SetEnableWidgetCaching(( NewState == ECheckBoxState::Checked ) ? true : false);
					})
					[
						SNew(SBox)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.Padding(FMargin(4.0, 2.0))
						[
							SNew(STextBlock)
							.Text(LOCTEXT("EnableWidgetCaching", "Widget Caching"))
						]
					]
				]

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5.0f, 0.0f))
				[
					SNew(SCheckBox)
					.Style(FCoreStyle::Get(), "ToggleButtonCheckbox")
					.IsChecked_Lambda([&]()
					{
						return SInvalidationPanel::IsInvalidationDebuggingEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([&](const ECheckBoxState NewState)
					{
						SInvalidationPanel::EnableInvalidationDebugging(( NewState == ECheckBoxState::Checked ) ? true : false);
					})
					[
						SNew(SBox)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.Padding(FMargin(4.0, 2.0))
						[
							SNew(STextBlock)
							.Text(LOCTEXT("InvalidationDebugging", "Invalidation Debugging"))
						]
					]
				]
#endif

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5.0f, 0.0f))
				[
					SNew(SCheckBox)
					.Style(FCoreStyle::Get(), "ToggleButtonCheckbox")
					.IsChecked_Lambda([&]()
					{
						return bEnableDemoMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([&](const ECheckBoxState NewState)
					{
						bEnableDemoMode = (NewState == ECheckBoxState::Checked) ? true : false;
					})
					[
						SNew(SBox)
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.Padding(FMargin(4.0, 2.0))
						[
							SNew(STextBlock)
							.Text(LOCTEXT("EnableDemoMode", "Demo Mode"))
						]
					]
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5.0f, 0.0f))
				[
					SNew(SCheckBox)
					.Style( FCoreStyle::Get(), "ToggleButtonCheckbox" )
#if !SLATE_STATS
					.IsEnabled(false)
#endif
					.IsChecked_Static([]
					{
#if SLATE_STATS
						return GSlateStatsFlatEnable == 0 ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
#else
						return ECheckBoxState::Unchecked;
#endif
					})
					.OnCheckStateChanged_Lambda( [=]( const ECheckBoxState NewState )
					{
#if SLATE_STATS
						GSlateStatsFlatEnable = (NewState == ECheckBoxState::Checked) ? 1 : 0;
						if (GSlateStatsFlatEnable)
						{
							TabManager->InvokeTab(WidgetReflectorTabID::SlateStats);
						}
						else
						{
							CloseTab(WidgetReflectorTabID::SlateStats);
						}
#endif
					})
					.ToolTip
					(
						SNew(SToolTip)
						[
							SNew(STextBlock)
							.WrapTextAt(200.0f)
#if SLATE_STATS
							.Text( LOCTEXT("ToggleStatsTooltip", "Enables flat stats view.") )
#else
							.Text( LOCTEXT("ToggleStatsUnavailableTooltip", "To enable slate stats, compile with SLATE_STATS defined to one (see SlateStats.h).") )
#endif
						]
					)
					[
						SNew(SBox)
						.VAlign( VAlign_Center )
						.HAlign( HAlign_Center )
						.Padding(FMargin(4.0, 2.0))
						[
							SNew(STextBlock)	
							.Text( LOCTEXT("ToggleStats", "Toggle Stats") )
						]
					]
				]

				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SSpacer)
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5.0f, 0.0f))
				[
					SNew(SButton)
					.Text(LOCTEXT("DisplayTextureAtlases", "Display Texture Atlases"))
					.OnClicked(this, &SWidgetReflector::HandleDisplayTextureAtlases)
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5.0f, 0.0f))
				[
					SNew(SButton)
					.Text(LOCTEXT("DisplayFontAtlases", "Display Font Atlases"))
					.OnClicked(this, &SWidgetReflector::HandleDisplayFontAtlases)
				]
			]

			+SVerticalBox::Slot()
			[
				TabManager->RestoreFrom(Layout, nullptr).ToSharedRef()
			]
		]
	];

#if SLATE_STATS
	if (GSlateStatsFlatEnable)
	{
		TabManager->InvokeTab(WidgetReflectorTabID::SlateStats);
	}
	else
	{
		CloseTab(WidgetReflectorTabID::SlateStats);
	}
#endif
}


TSharedRef<SDockTab> SWidgetReflector::SpawnWidgetHierarchyTab(const FSpawnTabArgs& Args)
{
	TArray<SNumericDropDown<float>::FNamedValue> NamedValuesForSnapshotDelay;
	NamedValuesForSnapshotDelay.Add(SNumericDropDown<float>::FNamedValue(0.0f, LOCTEXT("NoDelayValueName", "None"), LOCTEXT("NoDelayValueDescription", "Snapshot will be taken immediately upon clickng to take the snapshot.")));

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("WidgetHierarchyTab", "Widget Hierarchy"))
		//.OnCanCloseTab_Lambda([]() { return false; }) // Can't prevent this as it stops the editor from being able to close while the widget reflector is open
		[
			SNew(SVerticalBox)
				
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 2.0f))
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5.0f, 0.0f))
				[
					// Check box that controls LIVE MODE
					SNew(SCheckBox)
					.IsChecked(this, &SWidgetReflector::HandleFocusCheckBoxIsChecked)
					.OnCheckStateChanged(this, &SWidgetReflector::HandleFocusCheckBoxCheckedStateChanged)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ShowFocus", "Show Focus"))
					]
                ]
				
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5.0f, 0.0f))
				[
					// Check box that controls PICKING A WIDGET TO INSPECT
					SNew(SButton)
					.IsEnabled_Lambda([this]() { return !bIsPendingDelayedSnapshot; })
					.OnClicked(this, &SWidgetReflector::HandlePickButtonClicked)
					.ButtonColorAndOpacity(this, &SWidgetReflector::HandlePickButtonColorAndOpacity)
					[
						SNew(STextBlock)
						.Text(this, &SWidgetReflector::HandlePickButtonText)
					]
				]

				+SHorizontalBox::Slot()
				[
					SNew(SSpacer)
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5.0f, 0.0f))
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						// Button that controls taking a snapshot of the current window(s)
						SNew(SButton)
						.IsEnabled(this, &SWidgetReflector::IsTakeSnapshotButtonEnabled)
						.OnClicked(this, &SWidgetReflector::HandleTakeSnapshotButtonClicked)
						[
							SNew(STextBlock)
							.Text_Lambda([this]() { return bIsPendingDelayedSnapshot ? LOCTEXT("CancelSnapshotButtonText", "Cancel Snapshot") : LOCTEXT("TakeSnapshotButtonText", "Take Snapshot"); })
						]
					]

					+SHorizontalBox::Slot()
					.Padding(FMargin(4.0f, 0.0f))
					.AutoWidth()
					[
						SNew(SNumericDropDown<float>)
						.LabelText(LOCTEXT("DelayLabel", "Delay:"))
						.bShowNamedValue(true)
						.DropDownValues(NamedValuesForSnapshotDelay)
						.IsEnabled_Lambda([this]() { return !bIsPendingDelayedSnapshot; })
						.Value_Lambda([this]() { return SnapshotDelay; })
						.OnValueChanged_Lambda([this](const float InValue) { SnapshotDelay = FMath::Max(0.0f, InValue); })
					]

					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						// Button that controls the target for the snapshot operation
						SAssignNew(AvailableSnapshotTargetsComboBox, SComboBox<TSharedPtr<FWidgetSnapshotTarget>>)
						.IsEnabled(this, &SWidgetReflector::IsSnapshotTargetComboEnabled)
						.ToolTipText(LOCTEXT("ChooseSnapshotTargetToolTipText", "Choose Snapshot Target"))
						.OptionsSource(&AvailableSnapshotTargets)
						.OnGenerateWidget(this, &SWidgetReflector::HandleGenerateAvailableSnapshotComboItemWidget)
						.OnSelectionChanged(this, &SWidgetReflector::HandleAvailableSnapshotComboSelectionChanged)
						[
							SNew(STextBlock)
							.Text(this, &SWidgetReflector::GetSelectedSnapshotTargetDisplayName)
						]
					]
				]

#if SLATE_REFLECTOR_HAS_DESKTOP_PLATFORM
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5.0f, 0.0f))
				[
					// Button that controls loading a saved snapshot
					SNew(SButton)
					.IsEnabled_Lambda([this]() { return !bIsPendingDelayedSnapshot; })
					.OnClicked(this, &SWidgetReflector::HandleLoadSnapshotButtonClicked)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("LoadSnapshotButtonText", "Load Snapshot"))
					]
				]
#endif // SLATE_REFLECTOR_HAS_DESKTOP_PLATFORM
			]

			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				[
					// The tree view that shows all the info that we capture.
					SAssignNew(ReflectorTree, SReflectorTree)
					.ItemHeight(24.0f)
					.TreeItemsSource(&ReflectorTreeRoot)
					.OnGenerateRow(this, &SWidgetReflector::HandleReflectorTreeGenerateRow)
					.OnGetChildren(this, &SWidgetReflector::HandleReflectorTreeGetChildren)
					.OnSelectionChanged(this, &SWidgetReflector::HandleReflectorTreeSelectionChanged)
					.HeaderRow
					(
						SNew(SHeaderRow)

						+SHeaderRow::Column(SReflectorTreeWidgetItem::NAME_WidgetName)
						.DefaultLabel(LOCTEXT("WidgetName", "Widget Name"))
						.FillWidth(0.65f)

						+SHeaderRow::Column(SReflectorTreeWidgetItem::NAME_ForegroundColor)
						.FixedWidth(24.0f)
						.VAlignHeader(VAlign_Center)
						.HeaderContent()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("ForegroundColor", "FG"))
							.ToolTipText(LOCTEXT("ForegroundColorToolTip", "Foreground Color"))
						]

						+SHeaderRow::Column(SReflectorTreeWidgetItem::NAME_Visibility)
						.FixedWidth(125.0f)
						.HAlignHeader(HAlign_Center)
						.VAlignHeader(VAlign_Center)
						.HeaderContent()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Visibility", "Visibility" ))
							.ToolTipText(LOCTEXT("VisibilityTooltip", "Visibility"))
						]

						+ SHeaderRow::Column("Focusable")
						.DefaultLabel(LOCTEXT("Focusable", "Focusable?"))
						.FixedWidth(125.0f)
						.HAlignHeader(HAlign_Center)
						.VAlignHeader(VAlign_Center)
						.HeaderContent()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("Focusable", "Focusable?"))
							.ToolTipText(LOCTEXT("FocusableTooltip", "Focusability (Note that for hit-test directional navigation to work it must be Focusable and \"Visible\"!)"))
						]

						+SHeaderRow::Column(SReflectorTreeWidgetItem::NAME_Clipping)
						.DefaultLabel(LOCTEXT("Clipping", "Clipping" ))
						.FixedWidth(100.0f)

						+SHeaderRow::Column(SReflectorTreeWidgetItem::NAME_WidgetInfo)
						.DefaultLabel(LOCTEXT("WidgetInfo", "Widget Info" ))
						.FillWidth(0.25f)

						+SHeaderRow::Column(SReflectorTreeWidgetItem::NAME_Address)
						.DefaultLabel( LOCTEXT("Address", "Address") )
						.FixedWidth(140.0f)
					)
				]
			]
		];

	UpdateSelectedSnapshotTarget();

	return SpawnedTab;
}


#if SLATE_STATS

TSharedRef<SDockTab> SWidgetReflector::SpawnSlateStatsTab(const FSpawnTabArgs& Args)
{
	auto OnTabClosed = [](TSharedRef<SDockTab>)
	{
		// Tab closed - disable stats
		GSlateStatsFlatEnable = 0;
	};

	return SNew(SDockTab)
		.Label(LOCTEXT("SlateStatsTab", "Slate Stats"))
		.OnTabClosed_Lambda(OnTabClosed)
		[
			SNew(SVerticalBox)
				
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 2.0f))
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5.0f, 0.0f))
				[
					SNew(SCheckBox)
					.Style( FCoreStyle::Get(), "ToggleButtonCheckbox" )
					.IsChecked_Static([]
					{
						return GSlateStatsFlatLogOutput == 0 ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
					})
					.OnCheckStateChanged_Static([]( const ECheckBoxState NewState )
					{
						GSlateStatsFlatLogOutput = (NewState == ECheckBoxState::Checked) ? 1 : 0;
					})
					.ToolTip
					(
						SNew(SToolTip)
						[
							SNew(STextBlock)
							.WrapTextAt(200.0f)
							.Text( LOCTEXT("LogStatsTooltip", "Enables outputting stats to the log at the given interval.") )
						]
					)
					[
						SNew(SBox)
						.VAlign( VAlign_Center )
						.HAlign( HAlign_Center )
						[
							SNew(STextBlock)	
							.Text( LOCTEXT("ToggleLogStats", "Log Stats") )
						]
					]
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5.0f, 0.0f))
				[
					SNew(SButton)
					.OnClicked_Static([]
					{
						GSlateStatsHierarchyTrigger = 1;
						return FReply::Handled();
					})
					.ToolTip
					(
						SNew(SToolTip)
						[
							SNew(STextBlock)
							.WrapTextAt(200.0f)
							.Text( LOCTEXT("CaptureStatsHierarchyTooltip", "When clicked, the next rendered frame will capture hierarchical stats and save them to file in the Saved/ folder with the following name: SlateHierarchyStats-<timestamp>.csv") )
						]
					)
					[
						SNew(STextBlock)
							.Text(LOCTEXT("CaptureHierarchy", "Capture Hierarchy"))
					]
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.ToolTip
					(
						SNew(SToolTip)
						[
							SNew(STextBlock)
							.WrapTextAt(200.0f)
							.Text( LOCTEXT("StatsSamplingIntervalLabelTooltip", "the interval (in seconds) to integrate stats before updating the averages.") )
						]
					)
					.Text(LOCTEXT("StatsSampleWindow", "Sampling Interval: "))
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.MinDesiredWidth(100)
					.MaxDesiredWidth(250)
					[
						SNew(SSpinBox<float>)
						.ToolTip
						(
							SNew(SToolTip)
							[
								SNew(STextBlock)
								.WrapTextAt(200.0f)
								.Text( LOCTEXT("StatsSamplingIntervalTooltip", "the interval (in seconds) to integrate stats before updating the stats.") )
							]
						)
						.Value_Static([]
						{
							return GSlateStatsFlatIntervalWindowSec;
						})
						.MinValue(0.1f)
						.MaxValue(15.0f)
						.Delta(0.1f)
						.OnValueChanged_Static([](float NewValue)
						{
							GSlateStatsFlatIntervalWindowSec = NewValue;
						})
					]
				]
			]

#if WITH_EVENT_LOGGING
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SBorder)
				.Padding(0)
				.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				[
					SAssignNew(EventListView, SListView<TSharedRef<FLoggedEvent>>)
					.ListItemsSource( &LoggedEvents )
					.OnGenerateRow(this, &SWidgetReflector::GenerateEventLogRow)
				]
			]
#endif //WITH_EVENT_LOGGING

			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				MakeStatViewer()
			]
		];
}

#endif // #if SLATE_STATS


TSharedRef<SDockTab> SWidgetReflector::SpawnSnapshotWidgetPicker(const FSpawnTabArgs& Args)
{
	auto OnTabClosed = [this](TSharedRef<SDockTab>)
	{
		// Tab closed - leave snapshot mode
		SetUIMode(EWidgetReflectorUIMode::Live);
	};

	auto OnWidgetPathPicked = [this](const TArray<TSharedRef<FWidgetReflectorNodeBase>>& PickedWidgetPath)
	{
		VisualizeAsTree(PickedWidgetPath);
	};

	return SNew(SDockTab)
		.Label(LOCTEXT("SnapshotWidgetPickerTab", "Snapshot Widget Picker"))
		.OnTabClosed_Lambda(OnTabClosed)
		[
			SAssignNew(WidgetSnapshotVisualizer, SWidgetSnapshotVisualizer)
			.SnapshotData(&SnapshotData)
			.OnWidgetPathPicked_Lambda(OnWidgetPathPicked)
		];
}


void SWidgetReflector::OnTabSpawned(const FName& TabIdentifier, const TSharedRef<SDockTab>& SpawnedTab)
{
	TWeakPtr<SDockTab>* const ExistingTab = SpawnedTabs.Find(TabIdentifier);
	if (!ExistingTab)
	{
		SpawnedTabs.Add(TabIdentifier, SpawnedTab);
	}
	else
	{
		check(!ExistingTab->IsValid());
		*ExistingTab = SpawnedTab;
	}
}


void SWidgetReflector::CloseTab(const FName& TabIdentifier)
{
	TWeakPtr<SDockTab>* const ExistingTab = SpawnedTabs.Find(TabIdentifier);
	if (ExistingTab)
	{
		TSharedPtr<SDockTab> ExistingTabPin = ExistingTab->Pin();
		if (ExistingTabPin.IsValid())
		{
			ExistingTabPin->RequestCloseTab();
		}
	}
}


void SWidgetReflector::SetUIMode(const EWidgetReflectorUIMode InNewMode)
{
	if (CurrentUIMode != InNewMode)
	{
		CurrentUIMode = InNewMode;

		SelectedNodes.Reset();
		ReflectorTreeRoot.Reset();
		PickedPath.Reset();
		ReflectorTree->RequestTreeRefresh();

		if (CurrentUIMode == EWidgetReflectorUIMode::Snapshot)
		{
			TabManager->InvokeTab(WidgetReflectorTabID::SnapshotWidgetPicker);
		}
		else
		{
			SnapshotData.ClearSnapshot();

			if (WidgetSnapshotVisualizer.IsValid())
			{
				WidgetSnapshotVisualizer->SnapshotDataUpdated();
			}

			CloseTab(WidgetReflectorTabID::SnapshotWidgetPicker);
		}
	}
}


/* SCompoundWidget overrides
 *****************************************************************************/

void SWidgetReflector::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
#if SLATE_STATS
	UpdateStats();
#endif

	if (bIsPendingDelayedSnapshot && FSlateApplication::Get().GetCurrentTime() > TimeOfScheduledSnapshot)
	{
		// TakeSnapshot leads to the widget being ticked indirectly recursively,
		// so the recursion of this tick mustn't trigger a recursive snapshot.
		// Immediately clear the pending snapshot flag.
		bIsPendingDelayedSnapshot = false;
		TimeOfScheduledSnapshot = -1.0;

		TakeSnapshot();
	}
}

void SWidgetReflector::OnEventProcessed( const FInputEvent& Event, const FReplyBase& InReply )
{
	if ( Event.IsPointerEvent() )
	{
		const FPointerEvent& PtrEvent = static_cast<const FPointerEvent&>(Event);
		if (PtrEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			LastMouseClickTime = FSlateApplication::Get().GetCurrentTime();
			CursorPingPosition = PtrEvent.GetScreenSpacePosition();
		}
	}

	#if WITH_EVENT_LOGGING
		if (LoggedEvents.Num() >= MaxLoggedEvents)
		{
			LoggedEvents.Empty();
		}

		LoggedEvents.Add(MakeShareable(new FLoggedEvent(Event, InReply)));
		EventListView->RequestListRefresh();
		EventListView->RequestScrollIntoView(LoggedEvents.Last());
	#endif //WITH_EVENT_LOGGING
}


/* IWidgetReflector overrides
 *****************************************************************************/

bool SWidgetReflector::ReflectorNeedsToDrawIn( TSharedRef<SWindow> ThisWindow ) const
{
	return ((SelectedNodes.Num() > 0) && (ReflectorTreeRoot.Num() > 0) && (ReflectorTreeRoot[0]->GetLiveWidget() == ThisWindow));
}


void SWidgetReflector::SetWidgetsToVisualize( const FWidgetPath& InWidgetsToVisualize )
{
	ReflectorTreeRoot.Empty();

	if (InWidgetsToVisualize.IsValid())
	{
		ReflectorTreeRoot.Add(FWidgetReflectorNodeUtils::NewLiveNodeTreeFrom(InWidgetsToVisualize.Widgets[0]));
		PickedPath.Empty();

		FWidgetReflectorNodeUtils::FindLiveWidgetPath(ReflectorTreeRoot, InWidgetsToVisualize, PickedPath);
		VisualizeAsTree(PickedPath);
	}

	ReflectorTree->RequestTreeRefresh();
}


int32 SWidgetReflector::Visualize( const FWidgetPath& InWidgetsToVisualize, FSlateWindowElementList& OutDrawElements, int32 LayerId )
{
	const bool bAttemptingToVisualizeReflector = InWidgetsToVisualize.ContainsWidget(ReflectorTree.ToSharedRef());

	if (!InWidgetsToVisualize.IsValid() && SelectedNodes.Num() > 0 && ReflectorTreeRoot.Num() > 0)
	{
		TSharedPtr<SWidget> WindowWidget = ReflectorTreeRoot[0]->GetLiveWidget();
		if (WindowWidget.IsValid())
		{
			TSharedPtr<SWindow> Window = StaticCastSharedPtr<SWindow>(WindowWidget);
			return VisualizeSelectedNodesAsRectangles(SelectedNodes, Window.ToSharedRef(), OutDrawElements, LayerId);
		}
	}

	if (!bAttemptingToVisualizeReflector)
	{
		SetWidgetsToVisualize(InWidgetsToVisualize);
		return VisualizePickAsRectangles(InWidgetsToVisualize, OutDrawElements, LayerId);
	}

	return LayerId;
}

int32 SWidgetReflector::VisualizeCursorAndKeys(FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	if (bEnableDemoMode)
	{
		static const float ClickFadeTime = 0.5f;
		static const float PingScaleAmount = 3.0f;
		static const FName CursorPingBrush("DemoRecording.CursorPing");
		const TSharedPtr<SWindow> WindowBeingDrawn = OutDrawElements.GetWindow();

		// Normalized animation value for the cursor ping between 0 and 1.
		const float AnimAmount = (FSlateApplication::Get().GetCurrentTime() - LastMouseClickTime) / ClickFadeTime;

		if (WindowBeingDrawn.IsValid() && AnimAmount <= 1.0f)
		{
			const FVector2D CursorPosDesktopSpace = CursorPingPosition;
			const FVector2D CursorSize = FSlateApplication::Get().GetCursorSize();
			const FVector2D PingSize = CursorSize*PingScaleAmount*FCurveHandle::ApplyEasing(AnimAmount, ECurveEaseFunction::QuadOut);
			const FLinearColor PingColor = FLinearColor(1.0f, 0.0f, 1.0f, 1.0f - FCurveHandle::ApplyEasing(AnimAmount, ECurveEaseFunction::QuadIn));

			FGeometry CursorHighlightGeometry = FGeometry::MakeRoot(PingSize, FSlateLayoutTransform(CursorPosDesktopSpace - PingSize / 2));
			CursorHighlightGeometry.AppendTransform(Inverse(WindowBeingDrawn->GetLocalToScreenTransform()));

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId++,
				CursorHighlightGeometry.ToPaintGeometry(),
				FCoreStyle::Get().GetBrush(CursorPingBrush),
				ESlateDrawEffect::None,
				PingColor
				);
		}
	}
	
	return LayerId;
}


/* SWidgetReflector implementation
 *****************************************************************************/

TSharedRef<SToolTip> SWidgetReflector::GenerateToolTipForReflectorNode( TSharedRef<FWidgetReflectorNodeBase> InReflectorNode )
{
	return SNew(SToolTip)
		[
			SNew(SReflectorToolTipWidget)
				.WidgetInfoToVisualize(InReflectorNode)
		];
}


void SWidgetReflector::VisualizeAsTree( const TArray<TSharedRef<FWidgetReflectorNodeBase>>& WidgetPathToVisualize )
{
	const FLinearColor TopmostWidgetColor(1.0f, 0.0f, 0.0f);
	const FLinearColor LeafmostWidgetColor(0.0f, 1.0f, 0.0f);

	for (int32 WidgetIndex = 0; WidgetIndex<WidgetPathToVisualize.Num(); ++WidgetIndex)
	{
		const auto& CurWidget = WidgetPathToVisualize[WidgetIndex];

		// Tint the item based on depth in picked path
		const float ColorFactor = static_cast<float>(WidgetIndex)/WidgetPathToVisualize.Num();
		CurWidget->SetTint(FMath::Lerp(TopmostWidgetColor, LeafmostWidgetColor, ColorFactor));

		// Make sure the user can see the picked path in the tree.
		ReflectorTree->SetItemExpansion(CurWidget, true);
	}

	ReflectorTree->RequestScrollIntoView(WidgetPathToVisualize.Last());
	ReflectorTree->SetSelection(WidgetPathToVisualize.Last());
}


int32 SWidgetReflector::VisualizePickAsRectangles( const FWidgetPath& InWidgetsToVisualize, FSlateWindowElementList& OutDrawElements, int32 LayerId)
{
	const FLinearColor TopmostWidgetColor(1.0f, 0.0f, 0.0f);
	const FLinearColor LeafmostWidgetColor(0.0f, 1.0f, 0.0f);

	for (int32 WidgetIndex = 0; WidgetIndex < InWidgetsToVisualize.Widgets.Num(); ++WidgetIndex)
	{
		const FArrangedWidget& WidgetGeometry = InWidgetsToVisualize.Widgets[WidgetIndex];
		const float ColorFactor = static_cast<float>(WidgetIndex)/InWidgetsToVisualize.Widgets.Num();
		const FLinearColor Tint(1.0f - ColorFactor, ColorFactor, 0.0f, 1.0f);

		// The FGeometry we get is from a WidgetPath, so it's rooted in desktop space.
		// We need to APPEND a transform to the Geometry to essentially undo this root transform
		// and get us back into Window Space.
		// This is nonstandard so we have to go through some hoops and a specially exposed method 
		// in FPaintGeometry to allow appending layout transforms.
		FPaintGeometry WindowSpaceGeometry = WidgetGeometry.Geometry.ToPaintGeometry();
		WindowSpaceGeometry.AppendTransform(TransformCast<FSlateLayoutTransform>(Inverse(InWidgetsToVisualize.TopLevelWindow->GetPositionInScreen())));

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId,
			WindowSpaceGeometry,
			FCoreStyle::Get().GetBrush(TEXT("Debug.Border")),
			ESlateDrawEffect::None,
			FMath::Lerp(TopmostWidgetColor, LeafmostWidgetColor, ColorFactor)
		);
	}

	return LayerId;
}


int32 SWidgetReflector::VisualizeSelectedNodesAsRectangles( const TArray<TSharedRef<FWidgetReflectorNodeBase>>& InNodesToDraw, const TSharedRef<SWindow>& VisualizeInWindow, FSlateWindowElementList& OutDrawElements, int32 LayerId )
{
	for (int32 NodeIndex = 0; NodeIndex < InNodesToDraw.Num(); ++NodeIndex)
	{
		const auto& NodeToDraw = InNodesToDraw[NodeIndex];
		const FLinearColor Tint(0.0f, 1.0f, 0.0f);

		// The FGeometry we get is from a WidgetPath, so it's rooted in desktop space.
		// We need to APPEND a transform to the Geometry to essentially undo this root transform
		// and get us back into Window Space.
		// This is nonstandard so we have to go through some hoops and a specially exposed method 
		// in FPaintGeometry to allow appending layout transforms.
		FPaintGeometry WindowSpaceGeometry(NodeToDraw->GetAccumulatedLayoutTransform(), NodeToDraw->GetAccumulatedRenderTransform(), NodeToDraw->GetLocalSize(), NodeToDraw->GetGeometry().HasRenderTransform());
		WindowSpaceGeometry.AppendTransform(TransformCast<FSlateLayoutTransform>(Inverse(VisualizeInWindow->GetPositionInScreen())));

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId,
			WindowSpaceGeometry,
			FCoreStyle::Get().GetBrush(TEXT("Debug.Border")),
			ESlateDrawEffect::None,
			NodeToDraw->GetTint()
		);
	}

	return LayerId;
}


/* SWidgetReflector callbacks
 *****************************************************************************/

FReply SWidgetReflector::HandleDisplayTextureAtlases()
{
	static const FName SlateReflectorModuleName("SlateReflector");
	FModuleManager::LoadModuleChecked<ISlateReflectorModule>(SlateReflectorModuleName).DisplayTextureAtlasVisualizer();
	return FReply::Handled();
}

FReply SWidgetReflector::HandleDisplayFontAtlases()
{
	static const FName SlateReflectorModuleName("SlateReflector");
	FModuleManager::LoadModuleChecked<ISlateReflectorModule>(SlateReflectorModuleName).DisplayFontAtlasVisualizer();
	return FReply::Handled();
}

void SWidgetReflector::HandleFocusCheckBoxCheckedStateChanged( ECheckBoxState NewValue )
{
	bShowFocus = NewValue != ECheckBoxState::Unchecked;

	if (bShowFocus)
	{
		bIsPicking = false;
	}
}


FString SWidgetReflector::HandleFrameRateText() const
{
	FString MyString;
#if 0 // the new stats system does not support this
	MyString = FString::Printf(TEXT("FPS: %0.2f (%0.2f ms)"), (float)( 1.0f / FPSCounter.GetAverage()), (float)FPSCounter.GetAverage() * 1000.0f);
#endif

	return MyString;
}


FText SWidgetReflector::HandlePickButtonText() const
{
	static const FText NotPicking = LOCTEXT("PickLiveWidget", "Pick Live Widget");
	static const FText Picking = LOCTEXT("PickingWidget", "Picking (Esc to Stop)");

	return bIsPicking ? Picking : NotPicking;
}


bool SWidgetReflector::IsSnapshotTargetComboEnabled() const
{
	if (bIsPendingDelayedSnapshot)
	{
		return false;
	}

#if SLATE_REFLECTOR_HAS_SESSION_SERVICES
	return !RemoteSnapshotRequestId.IsValid();
#else
	return false;
#endif
}


bool SWidgetReflector::IsTakeSnapshotButtonEnabled() const
{
	return SelectedSnapshotTargetInstanceId.IsValid() && !RemoteSnapshotRequestId.IsValid();
}


FReply SWidgetReflector::HandleTakeSnapshotButtonClicked()
{
	if (!bIsPendingDelayedSnapshot)
	{
		if (SnapshotDelay > 0.0f)
		{
			bIsPendingDelayedSnapshot = true;
			TimeOfScheduledSnapshot = FSlateApplication::Get().GetCurrentTime() + SnapshotDelay;
		}
		else
		{
			TakeSnapshot();
		}
	}
	else
	{
		bIsPendingDelayedSnapshot = false;
		TimeOfScheduledSnapshot = -1.0f;
	}

	return FReply::Handled();
}

void SWidgetReflector::TakeSnapshot()
{
	// Local snapshot?
	if (SelectedSnapshotTargetInstanceId == FApp::GetInstanceId())
	{
		SetUIMode(EWidgetReflectorUIMode::Snapshot);

		// Take a snapshot of any window(s) that are currently open
		SnapshotData.TakeSnapshot();

		// Rebuild the reflector tree from the snapshot data
		ReflectorTreeRoot = SnapshotData.GetWindowsRef();
		ReflectorTree->RequestTreeRefresh();

		WidgetSnapshotVisualizer->SnapshotDataUpdated();
	}
	else
	{
		// Remote snapshot - these can take a while, show a progress message
		FNotificationInfo Info(LOCTEXT("RemoteWidgetSnapshotPendingNotificationText", "Waiting for Remote Widget Snapshot Data"));

		// Add the buttons with text, tooltip and callback
		Info.ButtonDetails.Add(FNotificationButtonInfo(
			LOCTEXT("CancelPendingSnapshotButtonText", "Cancel"),
			LOCTEXT("CancelPendingSnapshotButtonToolTipText", "Cancel the pending widget snapshot request."),
			FSimpleDelegate::CreateSP(this, &SWidgetReflector::OnCancelPendingRemoteSnapshot)
		));

		// We will be keeping track of this ourselves
		Info.bFireAndForget = false;

		// Launch notification
		WidgetSnapshotNotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);

		if (WidgetSnapshotNotificationPtr.IsValid())
		{
			WidgetSnapshotNotificationPtr.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
		}

		RemoteSnapshotRequestId = WidgetSnapshotService->RequestSnapshot(SelectedSnapshotTargetInstanceId, FWidgetSnapshotService::FOnWidgetSnapshotResponse::CreateSP(this, &SWidgetReflector::HandleRemoteSnapshotReceived));

		if (!RemoteSnapshotRequestId.IsValid())
		{
			TSharedPtr<SNotificationItem> WidgetSnapshotNotificationPin = WidgetSnapshotNotificationPtr.Pin();

			if (WidgetSnapshotNotificationPin.IsValid())
			{
				WidgetSnapshotNotificationPin->SetText(LOCTEXT("RemoteWidgetSnapshotFailedNotificationText", "Remote Widget Snapshot Failed"));
				WidgetSnapshotNotificationPin->SetCompletionState(SNotificationItem::CS_Fail);
				WidgetSnapshotNotificationPin->ExpireAndFadeout();

				WidgetSnapshotNotificationPtr.Reset();
			}
		}
	}
}

void SWidgetReflector::OnCancelPendingRemoteSnapshot()
{
	TSharedPtr<SNotificationItem> WidgetSnapshotNotificationPin = WidgetSnapshotNotificationPtr.Pin();

	if (WidgetSnapshotNotificationPin.IsValid())
	{
		WidgetSnapshotNotificationPin->SetText(LOCTEXT("RemoteWidgetSnapshotAbortedNotificationText", "Aborted Remote Widget Snapshot"));
		WidgetSnapshotNotificationPin->SetCompletionState(SNotificationItem::CS_Fail);
		WidgetSnapshotNotificationPin->ExpireAndFadeout();

		WidgetSnapshotNotificationPtr.Reset();
	}

	WidgetSnapshotService->AbortSnapshotRequest(RemoteSnapshotRequestId);
	RemoteSnapshotRequestId = FGuid();
}


void SWidgetReflector::HandleRemoteSnapshotReceived(const TArray<uint8>& InSnapshotData)
{
	{
		TSharedPtr<SNotificationItem> WidgetSnapshotNotificationPin = WidgetSnapshotNotificationPtr.Pin();

		if (WidgetSnapshotNotificationPin.IsValid())
		{
			WidgetSnapshotNotificationPin->SetText(LOCTEXT("RemoteWidgetSnapshotReceivedNotificationText", "Remote Widget Snapshot Data Received"));
			WidgetSnapshotNotificationPin->SetCompletionState(SNotificationItem::CS_Success);
			WidgetSnapshotNotificationPin->ExpireAndFadeout();

			WidgetSnapshotNotificationPtr.Reset();
		}
	}

	RemoteSnapshotRequestId = FGuid();

	SetUIMode(EWidgetReflectorUIMode::Snapshot);

	// Load up the remote data
	SnapshotData.LoadSnapshotFromBuffer(InSnapshotData);

	// Rebuild the reflector tree from the snapshot data
	ReflectorTreeRoot = SnapshotData.GetWindowsRef();
	ReflectorTree->RequestTreeRefresh();

	WidgetSnapshotVisualizer->SnapshotDataUpdated();
}


#if SLATE_REFLECTOR_HAS_DESKTOP_PLATFORM

FReply SWidgetReflector::HandleLoadSnapshotButtonClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	if (DesktopPlatform)
	{
		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(SharedThis(this));

		TArray<FString> OpenFilenames;
		const bool bOpened = DesktopPlatform->OpenFileDialog(
			(ParentWindow.IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr,
			LOCTEXT("LoadSnapshotDialogTitle", "Load Widget Snapshot").ToString(),
			FPaths::GameAgnosticSavedDir(),
			TEXT(""),
			TEXT("Slate Widget Snapshot (*.widgetsnapshot)|*.widgetsnapshot"),
			EFileDialogFlags::None,
			OpenFilenames
			);

		if (bOpened && SnapshotData.LoadSnapshotFromFile(OpenFilenames[0]))
		{
			SetUIMode(EWidgetReflectorUIMode::Snapshot);

			// Rebuild the reflector tree from the snapshot data
			ReflectorTreeRoot = SnapshotData.GetWindowsRef();
			ReflectorTree->RequestTreeRefresh();

			WidgetSnapshotVisualizer->SnapshotDataUpdated();
		}
	}

	return FReply::Handled();
}

#endif // SLATE_REFLECTOR_HAS_DESKTOP_PLATFORM


void SWidgetReflector::UpdateAvailableSnapshotTargets()
{
	AvailableSnapshotTargets.Reset();

#if SLATE_REFLECTOR_HAS_SESSION_SERVICES
	{
		TSharedRef<ISessionManager> SessionManager = FModuleManager::LoadModuleChecked<ISessionServicesModule>("SessionServices").GetSessionManager();

		TArray<TSharedPtr<ISessionInfo>> AvailableSessions;
		SessionManager->GetSessions(AvailableSessions);

		for (const auto& AvailableSession : AvailableSessions)
		{
			// Only allow sessions belonging to the current user
			if (AvailableSession->GetSessionOwner() != FApp::GetSessionOwner())
			{
				continue;
			}

			TArray<TSharedPtr<ISessionInstanceInfo>> AvailableInstances;
			AvailableSession->GetInstances(AvailableInstances);

			for (const auto& AvailableInstance : AvailableInstances)
			{
				FWidgetSnapshotTarget SnapshotTarget;
				SnapshotTarget.DisplayName = FText::Format(LOCTEXT("SnapshotTargetDisplayNameFmt", "{0} ({1})"), FText::FromString(AvailableInstance->GetInstanceName()), FText::FromString(AvailableInstance->GetPlatformName()));
				SnapshotTarget.InstanceId = AvailableInstance->GetInstanceId();

				AvailableSnapshotTargets.Add(MakeShareable(new FWidgetSnapshotTarget(SnapshotTarget)));
			}
		}
	}
#else
	{
		// No session services, just add an entry that lets us snapshot ourself
		FWidgetSnapshotTarget SnapshotTarget;
		SnapshotTarget.DisplayName = FText::FromString(FApp::GetInstanceName());
		SnapshotTarget.InstanceId = FApp::GetInstanceId();

		AvailableSnapshotTargets.Add(MakeShareable(new FWidgetSnapshotTarget(SnapshotTarget)));
	}
#endif
}


void SWidgetReflector::UpdateSelectedSnapshotTarget()
{
	if (AvailableSnapshotTargetsComboBox.IsValid())
	{
		const TSharedPtr<FWidgetSnapshotTarget>* FoundSnapshotTarget = AvailableSnapshotTargets.FindByPredicate([this](const TSharedPtr<FWidgetSnapshotTarget>& InAvailableSnapshotTarget) -> bool
		{
			return InAvailableSnapshotTarget->InstanceId == SelectedSnapshotTargetInstanceId;
		});

		if (FoundSnapshotTarget)
		{
			AvailableSnapshotTargetsComboBox->SetSelectedItem(*FoundSnapshotTarget);
		}
		else if (AvailableSnapshotTargets.Num() > 0)
		{
			SelectedSnapshotTargetInstanceId = AvailableSnapshotTargets[0]->InstanceId;
			AvailableSnapshotTargetsComboBox->SetSelectedItem(AvailableSnapshotTargets[0]);
		}
		else
		{
			SelectedSnapshotTargetInstanceId = FGuid();
			AvailableSnapshotTargetsComboBox->SetSelectedItem(nullptr);
		}
	}
}


void SWidgetReflector::OnAvailableSnapshotTargetsChanged()
{
	UpdateAvailableSnapshotTargets();
	UpdateSelectedSnapshotTarget();
}


FText SWidgetReflector::GetSelectedSnapshotTargetDisplayName() const
{
	if (AvailableSnapshotTargetsComboBox.IsValid())
	{
		TSharedPtr<FWidgetSnapshotTarget> SelectedSnapshotTarget = AvailableSnapshotTargetsComboBox->GetSelectedItem();
		if (SelectedSnapshotTarget.IsValid())
		{
			return SelectedSnapshotTarget->DisplayName;
		}
	}

	return FText::GetEmpty();
}


TSharedRef<SWidget> SWidgetReflector::HandleGenerateAvailableSnapshotComboItemWidget(TSharedPtr<FWidgetSnapshotTarget> InItem) const
{
	return SNew(STextBlock)
		.Text(InItem->DisplayName);
}


void SWidgetReflector::HandleAvailableSnapshotComboSelectionChanged(TSharedPtr<FWidgetSnapshotTarget> InItem, ESelectInfo::Type InSeletionInfo)
{
	if (InItem.IsValid())
	{
		SelectedSnapshotTargetInstanceId = InItem->InstanceId;
	}
	else
	{
		SelectedSnapshotTargetInstanceId = FGuid();
	}
}


TSharedRef<ITableRow> SWidgetReflector::HandleReflectorTreeGenerateRow( TSharedRef<FWidgetReflectorNodeBase> InReflectorNode, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(SReflectorTreeWidgetItem, OwnerTable)
		.WidgetInfoToVisualize(InReflectorNode)
		.ToolTip(GenerateToolTipForReflectorNode(InReflectorNode))
		.SourceCodeAccessor(SourceAccessDelegate)
		.AssetAccessor(AsseetAccessDelegate);
}


void SWidgetReflector::HandleReflectorTreeGetChildren(TSharedRef<FWidgetReflectorNodeBase> InReflectorNode, TArray<TSharedRef<FWidgetReflectorNodeBase>>& OutChildren)
{
	OutChildren = InReflectorNode->GetChildNodes();
}


void SWidgetReflector::HandleReflectorTreeSelectionChanged( TSharedPtr<FWidgetReflectorNodeBase>, ESelectInfo::Type /*SelectInfo*/ )
{
	SelectedNodes = ReflectorTree->GetSelectedItems();

	if (CurrentUIMode == EWidgetReflectorUIMode::Snapshot)
	{
		WidgetSnapshotVisualizer->SetSelectedWidgets(SelectedNodes);
	}
}


TSharedRef<ITableRow> SWidgetReflector::GenerateEventLogRow( TSharedRef<FLoggedEvent> InLoggedEvent, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(STableRow<TSharedRef<FLoggedEvent>>, OwnerTable)
	[
		SNew(STextBlock)
		.Text(InLoggedEvent->ToText())
	];
}

#if SLATE_STATS

//
// STATS
//

class FStatItem
{
public:
	
	FStatItem(FSlateStatCycleCounter* InCounter)
	: Counter(InCounter)
	{
		UpdateValues();
	}

	FText GetStatName() const { return StatName; }
	FText GetInclusiveAvgMsText() const { return InclusiveAvgMsText; }
	float GetInclusiveAvgMs() const { return InclusiveAvgMs; }
	void UpdateValues()
	{
		StatName = FText::FromName(Counter->GetName());
		InclusiveAvgMsText = FText::AsNumber(Counter->GetLastComputedAverageInclusiveTime(), &FNumberFormattingOptions().SetMinimumIntegralDigits(1).SetMinimumFractionalDigits(3).SetMaximumFractionalDigits(3));
		InclusiveAvgMs = (float)(Counter->GetLastComputedAverageInclusiveTime());
	}
private:
	FSlateStatCycleCounter* Counter;
	FText StatName;
	FText InclusiveAvgMsText;
	float InclusiveAvgMs;
};

static const FName ColumnId_StatName("StatName");
static const FName ColumnId_InclusiveAvgMs("InclusiveAvgMs");
static const FName ColumnId_InclusiveAvgMsGraph("InclusiveAvgMsGraph");

TSharedRef<SWidget> SWidgetReflector::MakeStatViewer()
{
	// the list of registered counters must remain constant throughout program execution.
	// As long as all counters are declared globally this will be true.
	for (auto Stat : FSlateStatCycleCounter::GetRegisteredCounters())
	{
		StatsItems.Add(MakeShareable(new FStatItem(Stat)));
	}

	return
		SAssignNew(StatsBorder, SBorder)
		.Padding(0)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Visibility_Lambda([]
		{
			return GSlateStatsFlatEnable > 0 ? EVisibility::Visible : EVisibility::Collapsed;
		})
		[
			// STATS LIST
			SAssignNew( StatsList, SListView< TSharedRef<FStatItem> > )
			.OnGenerateRow( this, &SWidgetReflector::GenerateStatRow )
			.ListItemsSource( &StatsItems )
			.HeaderRow
			(
				// STATS HEADER
				SNew(SHeaderRow)
				
				+SHeaderRow::Column(ColumnId_StatName)
				.FillWidth( 5.0f )
				.HAlignCell(HAlign_Right)
				.DefaultLabel( LOCTEXT("Stats_StatNameColumn", "Statistic") )

				+SHeaderRow::Column(ColumnId_InclusiveAvgMs)
				.FixedWidth(80.0f)
				.HAlignCell(HAlign_Right)
				.DefaultLabel( LOCTEXT("Stats_InclusiveAvgMsColumn", "AvgTime (ms)") )

				+SHeaderRow::Column(ColumnId_InclusiveAvgMsGraph)
				.FillWidth( 7.0f )
				.DefaultLabel( LOCTEXT("Stats_InclusiveAvgMsGraphColumn", " ") )
			)
		];
}

void SWidgetReflector::UpdateStats()
{
	if (FSlateStatCycleCounter::AverageInclusiveTimesWereUpdatedThisFrame())
	{
		for (auto& StatsItem : StatsItems)
		{
			StatsItem->UpdateValues();
		}
		//StatsList->RequestListRefresh();
	}
}


class SStatTableRow : public SMultiColumnTableRow< TSharedRef<FSlateStatCycleCounter> >
{
	
public:
	SLATE_BEGIN_ARGS( SStatTableRow )
	{}
	SLATE_END_ARGS();

	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, const TSharedPtr<FStatItem>& InStatItem )
	{
		StatItem = InStatItem;
		FSuperRowType::Construct( FTableRowArgs(), OwnerTable );
	}

	float GetValue() const { return 6.0f; }

	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override;

private:
	TSharedPtr<FStatItem> StatItem;
};

TSharedRef<SWidget> SStatTableRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	if (ColumnName == ColumnId_StatName)
	{
		// STAT NAME
		return
			SNew(STextBlock)
			.Text_Lambda([=] { return StatItem->GetStatName(); });
	}
	else if (ColumnName == ColumnId_InclusiveAvgMs)
	{
		// STAT NUMBER
		return
			SNew(SBox)
			.Padding(FMargin(5, 0))
			[
				SNew(STextBlock)
				.TextStyle(FCoreStyle::Get(), "MonospacedText")
				.Text_Lambda([=] { return StatItem->GetInclusiveAvgMsText(); })
			];
	}
	else if (ColumnName == ColumnId_InclusiveAvgMsGraph)
	{
		// BAR GRAPH
		return
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0, 1)
			.FillWidth(TAttribute<float>::Create(TAttribute<float>::FGetter::CreateLambda([=] { return StatItem->GetInclusiveAvgMs(); })))
			[
				SNew(SImage)
				.Image(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.ColorAndOpacity_Lambda([=] { return FMath::Lerp(FLinearColor::Green, FLinearColor::Red, StatItem->GetInclusiveAvgMs() / 30.0f); })
			]
			+ SHorizontalBox::Slot()
			.Padding(0, 1)
			.FillWidth(TAttribute<float>::Create(TAttribute<float>::FGetter::CreateLambda([=] { return 60.0f - StatItem->GetInclusiveAvgMs(); })))
			;
	}
	else
	{
		return SNew(SSpacer);
	}
}

TSharedRef<ITableRow> SWidgetReflector::GenerateStatRow( TSharedRef<FStatItem> StatItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(SStatTableRow, OwnerTable, StatItem);
}

#endif // SLATE_STATS

} // namespace WidgetReflectorImpl

TSharedRef<SWidgetReflector> SWidgetReflector::New()
{
  return MakeShareable( new WidgetReflectorImpl::SWidgetReflector() );
}

#undef LOCTEXT_NAMESPACE


