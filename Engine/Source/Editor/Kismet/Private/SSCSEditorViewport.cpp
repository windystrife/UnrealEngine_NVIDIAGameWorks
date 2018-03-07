// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSCSEditorViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "BlueprintEditorCommands.h"
#include "SSCSEditor.h"
#include "Slate/SceneViewport.h"
#include "SViewportToolBar.h"
#include "STransformViewportToolbar.h"
#include "EditorViewportCommands.h"
#include "SEditorViewportToolBarMenu.h"
#include "BlueprintEditorTabs.h"
#include "BlueprintEditorSettings.h"

/*-----------------------------------------------------------------------------
   SSCSEditorViewportToolBar
-----------------------------------------------------------------------------*/

class SSCSEditorViewportToolBar : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS( SSCSEditorViewportToolBar ){}
		SLATE_ARGUMENT(TWeakPtr<SSCSEditorViewport>, EditorViewport)
	SLATE_END_ARGS()

	/** Constructs this widget with the given parameters */
	void Construct(const FArguments& InArgs)
	{
		EditorViewport = InArgs._EditorViewport;

		static const FName DefaultForegroundName("DefaultForeground");

		this->ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			.ColorAndOpacity(this, &SViewportToolBar::OnGetColorAndOpacity)
			.ForegroundColor(FEditorStyle::GetSlateColor(DefaultForegroundName))
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 2.0f)
				[
					SNew(SEditorViewportToolbarMenu)
					.ParentToolBar(SharedThis(this))
					.Cursor(EMouseCursor::Default)
					.Image("EditorViewportToolBar.MenuDropdown")
					.OnGetMenuContent(this, &SSCSEditorViewportToolBar::GeneratePreviewMenu)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 2.0f)
				[
					SNew( SEditorViewportToolbarMenu )
					.ParentToolBar( SharedThis( this ) )
					.Cursor( EMouseCursor::Default )
					.Label(this, &SSCSEditorViewportToolBar::GetCameraMenuLabel)
					.LabelIcon(this, &SSCSEditorViewportToolBar::GetCameraMenuLabelIcon)
					.OnGetMenuContent(this, &SSCSEditorViewportToolBar::GenerateCameraMenu)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 2.0f)
				[
					SNew( SEditorViewportToolbarMenu )
					.ParentToolBar( SharedThis( this ) )
					.Cursor( EMouseCursor::Default )
					.Label(this, &SSCSEditorViewportToolBar::GetViewMenuLabel)
					.LabelIcon(this, &SSCSEditorViewportToolBar::GetViewMenuLabelIcon)
					.OnGetMenuContent(this, &SSCSEditorViewportToolBar::GenerateViewMenu)
				]
				+ SHorizontalBox::Slot()
				.Padding( 3.0f, 1.0f )
				.HAlign( HAlign_Right )
				[
					SNew(STransformViewportToolBar)
					.Viewport(EditorViewport.Pin().ToSharedRef())
					.CommandList(EditorViewport.Pin()->GetCommandList())
				]
			]
		];

		SViewportToolBar::Construct(SViewportToolBar::FArguments());
	}

	/** Creates the preview menu */
	TSharedRef<SWidget> GeneratePreviewMenu() const
	{
		TSharedPtr<const FUICommandList> CommandList = EditorViewport.IsValid()? EditorViewport.Pin()->GetCommandList(): NULL;

		const bool bInShouldCloseWindowAfterMenuSelection = true;

		FMenuBuilder PreviewOptionsMenuBuilder(bInShouldCloseWindowAfterMenuSelection, CommandList);
		{
			PreviewOptionsMenuBuilder.BeginSection("BlueprintEditorPreviewOptions", NSLOCTEXT("BlueprintEditor", "PreviewOptionsMenuHeader", "Preview Viewport Options"));
			{
				PreviewOptionsMenuBuilder.AddMenuEntry(FBlueprintEditorCommands::Get().ResetCamera);
				PreviewOptionsMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().ToggleRealTime);
				PreviewOptionsMenuBuilder.AddMenuEntry(FBlueprintEditorCommands::Get().ShowFloor);
				PreviewOptionsMenuBuilder.AddMenuEntry(FBlueprintEditorCommands::Get().ShowGrid);
			}
			PreviewOptionsMenuBuilder.EndSection();
		}

		return PreviewOptionsMenuBuilder.MakeWidget();
	}

	FText GetCameraMenuLabel() const
	{
		FText Label = NSLOCTEXT("BlueprintEditor", "CameraMenuTitle_Default", "Camera");

		if(EditorViewport.IsValid())
		{
			switch(EditorViewport.Pin()->GetViewportClient()->GetViewportType())
			{
			case LVT_Perspective:
				Label = NSLOCTEXT("BlueprintEditor", "CameraMenuTitle_Perspective", "Perspective");
				break;

			case LVT_OrthoXY:
				Label = NSLOCTEXT("BlueprintEditor", "CameraMenuTitle_Top", "Top");
				break;

			case LVT_OrthoYZ:
				Label = NSLOCTEXT("BlueprintEditor", "CameraMenuTitle_Left", "Left");
				break;

			case LVT_OrthoXZ:
				Label = NSLOCTEXT("BlueprintEditor", "CameraMenuTitle_Front", "Front");
				break;

			case LVT_OrthoNegativeXY:
				Label = NSLOCTEXT("BlueprintEditor", "CameraMenuTitle_Bottom", "Bottom");
				break;

			case LVT_OrthoNegativeYZ:
				Label = NSLOCTEXT("BlueprintEditor", "CameraMenuTitle_Right", "Right");
				break;

			case LVT_OrthoNegativeXZ:
				Label = NSLOCTEXT("BlueprintEditor", "CameraMenuTitle_Back", "Back");
				break;

			case LVT_OrthoFreelook:
				Label = NSLOCTEXT("BlueprintEditor", "CameraMenuTitle_OrthoFreelook", "Ortho");
				break;
			}
		}

		return Label;
	}

	const FSlateBrush* GetCameraMenuLabelIcon() const
	{
		static FName PerspectiveIconName("EditorViewport.Perspective");
		static FName TopIconName("EditorViewport.Top");
		static FName LeftIconName("EditorViewport.Left");
		static FName FrontIconName("EditorViewport.Front");
		static FName BottomIconName("EditorViewport.Bottom");
		static FName RightIconName("EditorViewport.Right");
		static FName BackIconName("EditorViewport.Back");

		FName Icon = NAME_None;

		if(EditorViewport.IsValid())
		{
			switch(EditorViewport.Pin()->GetViewportClient()->GetViewportType())
			{
			case LVT_Perspective:
				Icon = PerspectiveIconName;
				break;

			case LVT_OrthoXY:
				Icon = TopIconName;
				break;

			case LVT_OrthoYZ:
				Icon = LeftIconName;
				break;

			case LVT_OrthoXZ:
				Icon = FrontIconName;
				break;

			case LVT_OrthoNegativeXY:
				Icon = BottomIconName;
				break;

			case LVT_OrthoNegativeYZ:
				Icon = RightIconName;
				break;

			case LVT_OrthoNegativeXZ:
				Icon = BackIconName;
				break;
			}
		}

		return FEditorStyle::GetBrush(Icon);
	}

	TSharedRef<SWidget> GenerateCameraMenu() const
	{
		TSharedPtr<const FUICommandList> CommandList = EditorViewport.IsValid()? EditorViewport.Pin()->GetCommandList(): nullptr;

		const bool bInShouldCloseWindowAfterMenuSelection = true;
		FMenuBuilder CameraMenuBuilder(bInShouldCloseWindowAfterMenuSelection, CommandList);

		CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Perspective);

		CameraMenuBuilder.BeginSection("LevelViewportCameraType_Ortho", NSLOCTEXT("BlueprintEditor", "CameraTypeHeader_Ortho", "Orthographic"));
			CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Top);
			CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Bottom);
			CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Left);
			CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Right);
			CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Front);
			CameraMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().Back);
		CameraMenuBuilder.EndSection();

		return CameraMenuBuilder.MakeWidget();
	}

	FText GetViewMenuLabel() const
	{
		FText Label = NSLOCTEXT("BlueprintEditor", "ViewMenuTitle_Default", "View");

		if (EditorViewport.IsValid())
		{
			switch (EditorViewport.Pin()->GetViewportClient()->GetViewMode())
			{
			case VMI_Lit:
				Label = NSLOCTEXT("BlueprintEditor", "ViewMenuTitle_Lit", "Lit");
				break;

			case VMI_Unlit:
				Label = NSLOCTEXT("BlueprintEditor", "ViewMenuTitle_Unlit", "Unlit");
				break;

			case VMI_BrushWireframe:
				Label = NSLOCTEXT("BlueprintEditor", "ViewMenuTitle_Wireframe", "Wireframe");
				break;
			}
		}

		return Label;
	}

	const FSlateBrush* GetViewMenuLabelIcon() const
	{
		static FName LitModeIconName("EditorViewport.LitMode");
		static FName UnlitModeIconName("EditorViewport.UnlitMode");
		static FName WireframeModeIconName("EditorViewport.WireframeMode");

		FName Icon = NAME_None;

		if (EditorViewport.IsValid())
		{
			switch (EditorViewport.Pin()->GetViewportClient()->GetViewMode())
			{
			case VMI_Lit:
				Icon = LitModeIconName;
				break;

			case VMI_Unlit:
				Icon = UnlitModeIconName;
				break;

			case VMI_BrushWireframe:
				Icon = WireframeModeIconName;
				break;
			}
		}

		return FEditorStyle::GetBrush(Icon);
	}

	TSharedRef<SWidget> GenerateViewMenu() const
	{
		TSharedPtr<const FUICommandList> CommandList = EditorViewport.IsValid() ? EditorViewport.Pin()->GetCommandList() : nullptr;

		const bool bInShouldCloseWindowAfterMenuSelection = true;
		FMenuBuilder ViewMenuBuilder(bInShouldCloseWindowAfterMenuSelection, CommandList);

		ViewMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().LitMode, NAME_None, NSLOCTEXT("BlueprintEditor", "LitModeMenuOption", "Lit"));
		ViewMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().UnlitMode, NAME_None, NSLOCTEXT("BlueprintEditor", "UnlitModeMenuOption", "Unlit"));
		ViewMenuBuilder.AddMenuEntry(FEditorViewportCommands::Get().WireframeMode, NAME_None, NSLOCTEXT("BlueprintEditor", "WireframeModeMenuOption", "Wireframe"));

		return ViewMenuBuilder.MakeWidget();
	}

private:
	/** Reference to the parent viewport */
	TWeakPtr<SSCSEditorViewport> EditorViewport;
};


/*-----------------------------------------------------------------------------
   SSCSEditorViewport
-----------------------------------------------------------------------------*/

void SSCSEditorViewport::Construct(const FArguments& InArgs)
{
	bIsActiveTimerRegistered = false;

	// Save off the Blueprint editor reference, we'll need this later
	BlueprintEditorPtr = InArgs._BlueprintEditor;

	SEditorViewport::Construct( SEditorViewport::FArguments() );

	// Refresh the preview scene
	RequestRefresh(true);
}

SSCSEditorViewport::~SSCSEditorViewport()
{
	if(ViewportClient.IsValid())
	{
		// Reset this to ensure it's no longer in use after destruction
		ViewportClient->Viewport = NULL;
	}
}

bool SSCSEditorViewport::IsVisible() const
{
	// We consider the viewport to be visible if the reference is valid
	return ViewportWidget.IsValid() && SEditorViewport::IsVisible();
}

TSharedRef<FEditorViewportClient> SSCSEditorViewport::MakeEditorViewportClient()
{
	FPreviewScene* PreviewScene = BlueprintEditorPtr.Pin()->GetPreviewScene();

	// Construct a new viewport client instance.
	ViewportClient = MakeShareable(new FSCSEditorViewportClient(BlueprintEditorPtr, PreviewScene, SharedThis(this)));
	ViewportClient->SetRealtime(true);
	ViewportClient->bSetListenerPosition = false;
	ViewportClient->VisibilityDelegate.BindSP(this, &SSCSEditorViewport::IsVisible);

	return ViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SSCSEditorViewport::MakeViewportToolbar()
{
	return 
		SNew(SSCSEditorViewportToolBar)
		.EditorViewport(SharedThis(this))
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute());
}


void SSCSEditorViewport::BindCommands()
{
	FSCSEditorViewportCommands::Register(); // make sure the viewport specific commands have been registered

	TSharedPtr<FBlueprintEditor> BlueprintEditor = BlueprintEditorPtr.Pin();
	TSharedPtr<SSCSEditor> SCSEditorWidgetPtr = BlueprintEditor->GetSCSEditor();
	SSCSEditor* SCSEditorWidget = SCSEditorWidgetPtr.Get();
	// for mac, we have to bind a command that would override the BP-Editor's 
	// "NavigateToParentBackspace" command, because the delete key is the 
	// backspace key for that platform (and "NavigateToParentBackspace" does not 
	// make sense in the viewport window... it blocks the generic delete command)
	// 
	// NOTE: this needs to come before we map any other actions (so it is 
	// prioritized first)
	CommandList->MapAction(
		FSCSEditorViewportCommands::Get().DeleteComponent,
		FExecuteAction::CreateSP(SCSEditorWidget, &SSCSEditor::OnDeleteNodes),
		FCanExecuteAction::CreateSP(SCSEditorWidget, &SSCSEditor::CanDeleteNodes)
	);

	CommandList->Append(BlueprintEditor->GetSCSEditor()->CommandList.ToSharedRef());
	CommandList->Append(BlueprintEditor->GetToolkitCommands());
	SEditorViewport::BindCommands();

	const FBlueprintEditorCommands& Commands = FBlueprintEditorCommands::Get();

	BlueprintEditorPtr.Pin()->GetToolkitCommands()->MapAction(
		Commands.EnableSimulation,
		FExecuteAction::CreateSP(this, &SSCSEditorViewport::ToggleIsSimulateEnabled),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(ViewportClient.Get(), &FSCSEditorViewportClient::GetIsSimulateEnabled));

	// Toggle camera lock on/off
	CommandList->MapAction(
		Commands.ResetCamera,
		FExecuteAction::CreateSP(ViewportClient.Get(), &FSCSEditorViewportClient::ResetCamera) );

	CommandList->MapAction(
		Commands.ShowFloor,
		FExecuteAction::CreateSP(ViewportClient.Get(), &FSCSEditorViewportClient::ToggleShowFloor),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(ViewportClient.Get(), &FSCSEditorViewportClient::GetShowFloor));

	CommandList->MapAction(
		Commands.ShowGrid,
		FExecuteAction::CreateSP(ViewportClient.Get(), &FSCSEditorViewportClient::ToggleShowGrid),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(ViewportClient.Get(), &FSCSEditorViewportClient::GetShowGrid));
}

void SSCSEditorViewport::Invalidate()
{
	ViewportClient->Invalidate();
}

void SSCSEditorViewport::ToggleIsSimulateEnabled()
{
	// Make the viewport visible if the simulation is starting.
	if ( !ViewportClient->GetIsSimulateEnabled() )
	{
		if ( GetDefault<UBlueprintEditorSettings>()->bShowViewportOnSimulate )
		{
			BlueprintEditorPtr.Pin()->GetTabManager()->InvokeTab(FBlueprintEditorTabs::SCSViewportID);
		}
	}

	ViewportClient->ToggleIsSimulateEnabled();
}

void SSCSEditorViewport::EnablePreview(bool bEnable)
{
	if(bEnable)
	{
		// Restore the previously-saved realtime setting
		ViewportClient->RestoreRealtime();
	}
	else
	{
		// Disable and store the current realtime setting. This will bypass real-time rendering in the preview viewport (see UEditorEngine::UpdateSingleViewportClient).
		ViewportClient->SetRealtime(false, true);
	}
}

void SSCSEditorViewport::RequestRefresh(bool bResetCamera, bool bRefreshNow)
{
	if(bRefreshNow)
	{
		if(ViewportClient.IsValid())
		{
			ViewportClient->InvalidatePreview(bResetCamera);
		}
	}
	else
	{
		// Defer the update until the next tick. This way we don't accidentally spawn the preview actor in the middle of a transaction, for example.
		if (!bIsActiveTimerRegistered)
		{
			bIsActiveTimerRegistered = true;
			RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SSCSEditorViewport::DeferredUpdatePreview, bResetCamera));
		}
	}
}

void SSCSEditorViewport::OnComponentSelectionChanged()
{
	// When the component selection changes, make sure to invalidate hit proxies to sync with the current selection
	SceneViewport->Invalidate();
}

void SSCSEditorViewport::OnFocusViewportToSelection()
{
	ViewportClient->FocusViewportToSelection();
}

bool SSCSEditorViewport::GetIsSimulateEnabled()
{
	return ViewportClient->GetIsSimulateEnabled();
}

void SSCSEditorViewport::SetOwnerTab(TSharedRef<SDockTab> Tab)
{
	OwnerTab = Tab;
}

TSharedPtr<SDockTab> SSCSEditorViewport::GetOwnerTab() const
{
	return OwnerTab.Pin();
}

FReply SSCSEditorViewport::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<SSCSEditor> SCSEditor = BlueprintEditorPtr.Pin()->GetSCSEditor();
	return SCSEditor->TryHandleAssetDragDropOperation(DragDropEvent);
}

EActiveTimerReturnType SSCSEditorViewport::DeferredUpdatePreview(double InCurrentTime, float InDeltaTime, bool bResetCamera)
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->InvalidatePreview(bResetCamera);
	}

	bIsActiveTimerRegistered = false;
	return EActiveTimerReturnType::Stop;
}
