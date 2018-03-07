// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraSystemViewport.h"
#include "Widgets/Layout/SBox.h"
#include "Editor/UnrealEdEngine.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "UnrealEdGlobals.h"
#include "NiagaraComponent.h"
#include "ComponentReregisterContext.h"
#include "NiagaraEditorModule.h"
#include "Slate/SceneViewport.h"
#include "NiagaraSystem.h"
#include "Widgets/Docking/SDockTab.h"
#include "Engine/TextureCube.h"
#include "SNiagaraSystemViewportToolBar.h"
#include "NiagaraEditorCommands.h"
#include "EditorViewportCommands.h"
#include "AdvancedPreviewScene.h"

/** Viewport Client for the preview viewport */
class FNiagaraSystemViewportClient : public FEditorViewportClient
{
public:
	FNiagaraSystemViewportClient(FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SNiagaraSystemViewport>& InNiagaraEditorViewport);
	
	// FEditorViewportClient interface
	virtual FLinearColor GetBackgroundColor() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas) override;
	virtual bool ShouldOrbitCamera() const override;
	virtual FSceneView* CalcSceneView(FSceneViewFamily* ViewFamily, const EStereoscopicPass StereoPass = eSSP_FULL) override;
	
	void SetShowGrid(bool bShowGrid);
};

FNiagaraSystemViewportClient::FNiagaraSystemViewportClient(FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SNiagaraSystemViewport>& InNiagaraEditorViewport)
	: FEditorViewportClient(nullptr, &InPreviewScene, StaticCastSharedRef<SEditorViewport>(InNiagaraEditorViewport))
{
	// Setup defaults for the common draw helper.
	DrawHelper.bDrawPivot = false;
	DrawHelper.bDrawWorldBox = false;
	DrawHelper.bDrawKillZ = false;
	DrawHelper.bDrawGrid = false;
	DrawHelper.GridColorAxis = FColor(80,80,80);
	DrawHelper.GridColorMajor = FColor(72,72,72);
	DrawHelper.GridColorMinor = FColor(64,64,64);
	DrawHelper.PerspectiveGridSize = HALF_WORLD_MAX1;
	
	SetViewMode(VMI_Lit);
	
	EngineShowFlags.DisableAdvancedFeatures();
	EngineShowFlags.SetSnap(0);
	
	OverrideNearClipPlane(1.0f);
	bUsingOrbitCamera = true;
}


void FNiagaraSystemViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);
	
	// Tick the preview scene world.
	if (!GIntraFrameDebuggingGameThread)
	{
		PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	}
}


void FNiagaraSystemViewportClient::Draw(FViewport* InViewport,FCanvas* Canvas)
{
	FEditorViewportClient::Draw(InViewport, Canvas);
}

bool FNiagaraSystemViewportClient::ShouldOrbitCamera() const
{
	return true;
}


FLinearColor FNiagaraSystemViewportClient::GetBackgroundColor() const
{
	FLinearColor BackgroundColor = FLinearColor::Black;
	return BackgroundColor;
}

FSceneView* FNiagaraSystemViewportClient::CalcSceneView(FSceneViewFamily* ViewFamily, const EStereoscopicPass StereoPass)
{
	FSceneView* SceneView = FEditorViewportClient::CalcSceneView(ViewFamily);
	FFinalPostProcessSettings::FCubemapEntry& CubemapEntry = *new(SceneView->FinalPostProcessSettings.ContributingCubemaps) FFinalPostProcessSettings::FCubemapEntry;
	CubemapEntry.AmbientCubemap = GUnrealEd->GetThumbnailManager()->AmbientCubemap;
	CubemapEntry.AmbientCubemapTintMulScaleValue = FLinearColor::White;
	return SceneView;
}

void FNiagaraSystemViewportClient::SetShowGrid(bool bShowGrid)
{
	DrawHelper.bDrawGrid = bShowGrid;
}

void SNiagaraSystemViewport::Construct(const FArguments& InArgs)
{
	bShowGrid = false;
	bShowBackground = false;
	PreviewComponent = nullptr;
	AdvancedPreviewScene = MakeShareable(new FAdvancedPreviewScene(FPreviewScene::ConstructionValues()));
	
	SEditorViewport::Construct( SEditorViewport::FArguments() );
}

SNiagaraSystemViewport::~SNiagaraSystemViewport()
{
	if (SystemViewportClient.IsValid())
	{
		SystemViewportClient->Viewport = NULL;
	}
}

void SNiagaraSystemViewport::AddReferencedObjects( FReferenceCollector& Collector )
{
	if (PreviewComponent != nullptr)
	{
		Collector.AddReferencedObject(PreviewComponent);
	}
}

void SNiagaraSystemViewport::RefreshViewport()
{
	//reregister the preview components, so if the preview material changed it will be propagated to the render thread
	PreviewComponent->MarkRenderStateDirty();
	SceneViewport->InvalidateDisplay();
}

void SNiagaraSystemViewport::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	SEditorViewport::Tick( AllottedGeometry, InCurrentTime, InDeltaTime );
}

void SNiagaraSystemViewport::SetPreviewComponent(UNiagaraComponent* NiagaraComponent)
{
	if (PreviewComponent != nullptr)
	{
		AdvancedPreviewScene->RemoveComponent(PreviewComponent);
	}
	PreviewComponent = NiagaraComponent;

	if (PreviewComponent != nullptr)
	{
		AdvancedPreviewScene->AddComponent(PreviewComponent, PreviewComponent->GetRelativeTransform());
	}
}


void SNiagaraSystemViewport::ToggleRealtime()
{
	SystemViewportClient->ToggleRealtime();
}

/*
TSharedRef<FUICommandList> SNiagaraSystemViewport::GetSystemEditorCommands() const
{
	check(SystemEditorPtr.IsValid());
	return SystemEditorPtr.GetToolkitCommands();
}
*/

void SNiagaraSystemViewport::OnAddedToTab( const TSharedRef<SDockTab>& OwnerTab )
{
	ParentTab = OwnerTab;
}

bool SNiagaraSystemViewport::IsVisible() const
{
	return ViewportWidget.IsValid() && (!ParentTab.IsValid() || ParentTab.Pin()->IsForeground()) && SEditorViewport::IsVisible() ;
}

void SNiagaraSystemViewport::BindCommands()
{
	SEditorViewport::BindCommands();

	// Unbind the CycleTransformGizmos since niagara currently doesn't use the gizmos and it prevents resetting the system with
	// spacebar when the viewport is focused.
	CommandList->UnmapAction(FEditorViewportCommands::Get().CycleTransformGizmos);

	const FNiagaraEditorCommands& Commands = FNiagaraEditorCommands::Get();
	
	// Add the commands to the toolkit command list so that the toolbar buttons can find them
	
	CommandList->MapAction(
		Commands.TogglePreviewGrid,
		FExecuteAction::CreateSP( this, &SNiagaraSystemViewport::TogglePreviewGrid ),
								  FCanExecuteAction(),
								  FIsActionChecked::CreateSP( this, &SNiagaraSystemViewport::IsTogglePreviewGridChecked ) );
	
	CommandList->MapAction(
		Commands.TogglePreviewBackground,
		FExecuteAction::CreateSP( this, &SNiagaraSystemViewport::TogglePreviewBackground ),
								  FCanExecuteAction(),
								  FIsActionChecked::CreateSP( this, &SNiagaraSystemViewport::IsTogglePreviewBackgroundChecked ) );
								  
}

void SNiagaraSystemViewport::OnFocusViewportToSelection()
{
	if( PreviewComponent )
	{
		SystemViewportClient->FocusViewportOnBox( PreviewComponent->Bounds.GetBox() );
	}
}

void SNiagaraSystemViewport::TogglePreviewGrid()
{
	bShowGrid = !bShowGrid;
	SystemViewportClient->SetShowGrid(bShowGrid);
	RefreshViewport();
}

bool SNiagaraSystemViewport::IsTogglePreviewGridChecked() const
{
	return bShowGrid;
}

void SNiagaraSystemViewport::TogglePreviewBackground()
{
	bShowBackground = !bShowBackground;
	// @todo DB: Set the background mesh for the preview viewport.
	RefreshViewport();
}

bool SNiagaraSystemViewport::IsTogglePreviewBackgroundChecked() const
{
	return bShowBackground;
}

TSharedRef<FEditorViewportClient> SNiagaraSystemViewport::MakeEditorViewportClient() 
{
	SystemViewportClient = MakeShareable( new FNiagaraSystemViewportClient(*AdvancedPreviewScene.Get(), SharedThis(this)) );
	
	SystemViewportClient->SetViewLocation( FVector::ZeroVector );
	SystemViewportClient->SetViewRotation( FRotator::ZeroRotator );
	SystemViewportClient->SetViewLocationForOrbiting( FVector::ZeroVector );
	SystemViewportClient->bSetListenerPosition = false;
	
	SystemViewportClient->SetRealtime( true );
	SystemViewportClient->VisibilityDelegate.BindSP( this, &SNiagaraSystemViewport::IsVisible );
	
	return SystemViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SNiagaraSystemViewport::MakeViewportToolbar()
{
	//return SNew(SNiagaraSystemViewportToolBar)
	//.Viewport(SharedThis(this));
	return SNew(SBox);
}

EVisibility SNiagaraSystemViewport::OnGetViewportContentVisibility() const
{
	EVisibility BaseVisibility = SEditorViewport::OnGetViewportContentVisibility();
	if (BaseVisibility != EVisibility::Visible)
	{
		return BaseVisibility;
	}
	return IsVisible() ? EVisibility::Visible : EVisibility::Collapsed;
}

void SNiagaraSystemViewport::PopulateViewportOverlays(TSharedRef<class SOverlay> Overlay)
{
	Overlay->AddSlot()
		.VAlign(VAlign_Top)
		[
			SNew(SNiagaraSystemViewportToolBar, SharedThis(this))
		];

}


TSharedRef<class SEditorViewport> SNiagaraSystemViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SNiagaraSystemViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void SNiagaraSystemViewport::OnFloatingButtonClicked()
{
}