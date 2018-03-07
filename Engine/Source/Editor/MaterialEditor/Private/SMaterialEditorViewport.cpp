// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SMaterialEditorViewport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SViewport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "EditorStyleSet.h"
#include "Components/MeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Editor/UnrealEdEngine.h"
#include "MaterialEditor/MaterialEditorMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Selection.h"
#include "Editor.h"
#include "Dialogs/Dialogs.h"
#include "UnrealEdGlobals.h"
#include "MaterialEditorActions.h"
#include "Slate/SceneViewport.h"
#include "MaterialEditor.h"
#include "SMaterialEditorViewportToolBar.h"
#include "Widgets/Docking/SDockTab.h"
#include "Engine/TextureCube.h"
#include "ComponentAssetBroker.h"
#include "SlateMaterialBrush.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "AdvancedPreviewScene.h"
#include "AssetViewerSettings.h"


#define LOCTEXT_NAMESPACE "MaterialEditor"

/** Viewport Client for the preview viewport */
class FMaterialEditorViewportClient : public FEditorViewportClient
{
public:
	FMaterialEditorViewportClient(TWeakPtr<IMaterialEditor> InMaterialEditor, FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SMaterialEditor3DPreviewViewport>& InMaterialEditorViewport);

	// FEditorViewportClient interface
	virtual bool InputKey(FViewport* InViewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed = 1.f, bool bGamepad = false) override;
	virtual FLinearColor GetBackgroundColor() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas) override;
	virtual bool ShouldOrbitCamera() const override;
	
	void SetShowGrid(bool bShowGrid);

	/**
	* Focuses the viewport to the center of the bounding box/sphere ensuring that the entire bounds are in view
	*
	* @param Bounds   The bounds to focus
	* @param bInstant Whether or not to focus the viewport instantly or over time
	*/
	void FocusViewportOnBounds(const FBoxSphereBounds Bounds, bool bInstant = false);

private:

	/** Pointer back to the material editor tool that owns us */
	TWeakPtr<IMaterialEditor> MaterialEditorPtr;

	/** Preview Scene - uses advanced preview settings */
	class FAdvancedPreviewScene* AdvancedPreviewScene;
};

FMaterialEditorViewportClient::FMaterialEditorViewportClient(TWeakPtr<IMaterialEditor> InMaterialEditor, FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SMaterialEditor3DPreviewViewport>& InMaterialEditorViewport)
	: FEditorViewportClient(nullptr, &InPreviewScene, StaticCastSharedRef<SEditorViewport>(InMaterialEditorViewport))
	, MaterialEditorPtr(InMaterialEditor)
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
	EngineShowFlags.SetSeparateTranslucency(true);

	OverrideNearClipPlane(1.0f);
	bUsingOrbitCamera = true;

	// Don't want to display the widget in this viewport
	Widget->SetDefaultVisibility(false);

	AdvancedPreviewScene = &InPreviewScene;

}



void FMaterialEditorViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	// Tick the preview scene world.
	if (!GIntraFrameDebuggingGameThread)
	{
		PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	}
}


void FMaterialEditorViewportClient::Draw(FViewport* InViewport,FCanvas* Canvas)
{
	FEditorViewportClient::Draw(InViewport, Canvas);
	MaterialEditorPtr.Pin()->DrawMessages(InViewport, Canvas);
}

bool FMaterialEditorViewportClient::ShouldOrbitCamera() const
{
	// Should always orbit around the preview object to keep it in view.
	return true;
}

bool FMaterialEditorViewportClient::InputKey(FViewport* InViewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed, bool bGamepad)
{
	bool bHandled = FEditorViewportClient::InputKey(InViewport, ControllerId, Key, Event, AmountDepressed, false);

	// Handle viewport screenshot.
	bHandled |= InputTakeScreenshot(InViewport, Key, Event);

	bHandled |= AdvancedPreviewScene->HandleInputKey(InViewport, ControllerId, Key, Event, AmountDepressed, bGamepad);

	return bHandled;
}

FLinearColor FMaterialEditorViewportClient::GetBackgroundColor() const
{
	FLinearColor BackgroundColor = FLinearColor::Black;
	if( MaterialEditorPtr.IsValid() )
	{
		UMaterialInterface* MaterialInterface = MaterialEditorPtr.Pin()->GetMaterialInterface();
		if(MaterialInterface)
		{
			const EBlendMode PreviewBlendMode = (EBlendMode)MaterialInterface->GetBlendMode();
			if(PreviewBlendMode == BLEND_Modulate)
			{
				BackgroundColor = FLinearColor::White;
			}
			else if(PreviewBlendMode == BLEND_Translucent || PreviewBlendMode == BLEND_AlphaComposite)
			{
				BackgroundColor = FColor(64, 64, 64);
			}
		}
	}
	return BackgroundColor;
}

void FMaterialEditorViewportClient::SetShowGrid(bool bShowGrid)
{
	DrawHelper.bDrawGrid = bShowGrid;
}

void FMaterialEditorViewportClient::FocusViewportOnBounds(const FBoxSphereBounds Bounds, bool bInstant /*= false*/)
{
	const FVector Position = Bounds.Origin;
	float Radius = Bounds.SphereRadius;

	float AspectToUse = AspectRatio;
	FIntPoint ViewportSize = Viewport->GetSizeXY();
	if (!bUseControllingActorViewInfo && ViewportSize.X > 0 && ViewportSize.Y > 0)
	{
		AspectToUse = Viewport->GetDesiredAspectRatio();
	}

	const bool bEnable = false;
	ToggleOrbitCamera(bEnable);

	/**
	* We need to make sure we are fitting the sphere into the viewport completely, so if the height of the viewport is less
	* than the width of the viewport, we scale the radius by the aspect ratio in order to compensate for the fact that we have
	* less visible vertically than horizontally.
	*/
	if (AspectToUse > 1.0f)
	{
		Radius *= AspectToUse;
	}

	/**
	* Now that we have a adjusted radius, we are taking half of the viewport's FOV,
	* converting it to radians, and then figuring out the camera's distance from the center
	* of the bounding sphere using some simple trig.  Once we have the distance, we back up
	* along the camera's forward vector from the center of the sphere, and set our new view location.
	*/
	const float HalfFOVRadians = FMath::DegreesToRadians(ViewFOV / 2.0f);
	const float DistanceFromSphere = Radius / FMath::Sin(HalfFOVRadians);
	FViewportCameraTransform& ViewTransform = GetViewTransform();
	FVector CameraOffsetVector = ViewTransform.GetRotation().Vector() * -DistanceFromSphere;

	ViewTransform.SetLookAt(Position);
	ViewTransform.TransitionToLocation(Position + CameraOffsetVector, EditorViewportWidget, bInstant);

	// Tell the viewport to redraw itself.
	Invalidate();
}

void SMaterialEditor3DPreviewViewport::Construct(const FArguments& InArgs)
{
	MaterialEditorPtr = InArgs._MaterialEditor;
	AdvancedPreviewScene = MakeShareable(new FAdvancedPreviewScene(FPreviewScene::ConstructionValues()));

	bShowGrid = false;
	bShowBackground = false;
	PreviewPrimType = TPT_None;

	SEditorViewport::Construct( SEditorViewport::FArguments() );

	PreviewMaterial = nullptr;
	PreviewMeshComponent = nullptr;

	UMaterialInterface* Material = MaterialEditorPtr.Pin()->GetMaterialInterface();
	if (Material)
	{
		SetPreviewMaterial(Material);
	}

	SetPreviewAsset( GUnrealEd->GetThumbnailManager()->EditorSphere );
}

SMaterialEditor3DPreviewViewport::~SMaterialEditor3DPreviewViewport()
{
	UAssetViewerSettings::Get()->OnAssetViewerSettingsChanged().RemoveAll(this);
	if (PreviewMeshComponent != nullptr)
	{
		PreviewMeshComponent->OverrideMaterials.Empty();
	}

	if (EditorViewportClient.IsValid())
	{
		EditorViewportClient->Viewport = NULL;
	}
}

void SMaterialEditor3DPreviewViewport::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( PreviewMeshComponent );
	Collector.AddReferencedObject( PreviewMaterial );
}

void SMaterialEditor3DPreviewViewport::RefreshViewport()
{
	// reregister the preview components, so if the preview material changed it will be propagated to the render thread
	if (PreviewMeshComponent != nullptr)
	{
		PreviewMeshComponent->MarkRenderStateDirty();
	}
	SceneViewport->InvalidateDisplay();

	if (EditorViewportClient.IsValid())
	{
		UAssetViewerSettings* Settings = UAssetViewerSettings::Get();
		const int32 ProfileIndex = AdvancedPreviewScene->GetCurrentProfileIndex();
		if (Settings->Profiles.IsValidIndex(ProfileIndex) &&
			Settings->Profiles[ProfileIndex].bRotateLightingRig
			&& !EditorViewportClient->IsRealtime())
		{
			EditorViewportClient->SetRealtime(true);
		}
	}
}

bool SMaterialEditor3DPreviewViewport::SetPreviewAsset(UObject* InAsset)
{
	if (!MaterialEditorPtr.Pin()->ApproveSetPreviewAsset(InAsset))
	{
		return false;
	}

	// Unregister the current component
	if (PreviewMeshComponent != nullptr)
	{
		AdvancedPreviewScene->RemoveComponent(PreviewMeshComponent);
		PreviewMeshComponent = nullptr;
	}

	FTransform Transform = FTransform::Identity;

	if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(InAsset))
	{
		// Special case handling for static meshes, to use more accurate bounds via a subclass
		UStaticMeshComponent* NewSMComponent = NewObject<UMaterialEditorMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
		NewSMComponent->SetStaticMesh(StaticMesh);

		PreviewMeshComponent = NewSMComponent;

		// Update the toolbar state implicitly through PreviewPrimType.
		if (StaticMesh == GUnrealEd->GetThumbnailManager()->EditorCylinder)
		{
			PreviewPrimType = TPT_Cylinder;
		}
		else if (StaticMesh == GUnrealEd->GetThumbnailManager()->EditorCube)
		{
			PreviewPrimType = TPT_Cube;
		}
		else if (StaticMesh == GUnrealEd->GetThumbnailManager()->EditorSphere)
		{
			PreviewPrimType = TPT_Sphere;
		}
		else if (StaticMesh == GUnrealEd->GetThumbnailManager()->EditorPlane)
		{
			PreviewPrimType = TPT_Plane;
		}
		else
		{
			PreviewPrimType = TPT_None;
		}
	}
	else if (InAsset != nullptr)
	{
		// Fall back to the component asset broker
		if (TSubclassOf<UActorComponent> ComponentClass = FComponentAssetBrokerage::GetPrimaryComponentForAsset(InAsset->GetClass()))
		{
			if (ComponentClass->IsChildOf(UMeshComponent::StaticClass()))
			{
				PreviewMeshComponent = NewObject<UMeshComponent>(GetTransientPackage(), ComponentClass, NAME_None, RF_Transient);

				FComponentAssetBrokerage::AssignAssetToComponent(PreviewMeshComponent, InAsset);

				PreviewPrimType = TPT_None;
			}
		}
	}

	// Add the new component to the scene
	if (PreviewMeshComponent != nullptr)
	{
		AdvancedPreviewScene->AddComponent(PreviewMeshComponent, Transform);
		AdvancedPreviewScene->SetFloorOffset(-PreviewMeshComponent->Bounds.Origin.Z + PreviewMeshComponent->Bounds.BoxExtent.Z);

	}

	// Make sure the preview material is applied to the component
	SetPreviewMaterial(PreviewMaterial);

	return (PreviewMeshComponent != nullptr);
}

bool SMaterialEditor3DPreviewViewport::SetPreviewAssetByName(const TCHAR* InAssetName)
{
	bool bSuccess = false;
	if ((InAssetName != nullptr) && (*InAssetName != 0))
	{
		if (UObject* Asset = LoadObject<UObject>(nullptr, InAssetName))
		{
			bSuccess = SetPreviewAsset(Asset);
		}
	}
	return bSuccess;
}

void SMaterialEditor3DPreviewViewport::SetPreviewMaterial(UMaterialInterface* InMaterialInterface)
{
	PreviewMaterial = InMaterialInterface;

	if (PreviewMeshComponent != nullptr)
	{
		PreviewMeshComponent->OverrideMaterials.Empty();
		PreviewMeshComponent->OverrideMaterials.Add(PreviewMaterial);
	}
}

void SMaterialEditor3DPreviewViewport::OnAddedToTab( const TSharedRef<SDockTab>& OwnerTab )
{
	ParentTab = OwnerTab;
}

bool SMaterialEditor3DPreviewViewport::IsVisible() const
{
	return ViewportWidget.IsValid() && (!ParentTab.IsValid() || ParentTab.Pin()->IsForeground()) && SEditorViewport::IsVisible() ;
}

void SMaterialEditor3DPreviewViewport::BindCommands()
{
	SEditorViewport::BindCommands();

	const FMaterialEditorCommands& Commands = FMaterialEditorCommands::Get();

	check(MaterialEditorPtr.IsValid());
	CommandList->Append(MaterialEditorPtr.Pin()->GetToolkitCommands());

	// Add the commands to the toolkit command list so that the toolbar buttons can find them
	CommandList->MapAction(
		Commands.SetCylinderPreview,
		FExecuteAction::CreateSP( this, &SMaterialEditor3DPreviewViewport::OnSetPreviewPrimitive, TPT_Cylinder, false ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SMaterialEditor3DPreviewViewport::IsPreviewPrimitiveChecked, TPT_Cylinder ) );

	CommandList->MapAction(
		Commands.SetSpherePreview,
		FExecuteAction::CreateSP( this, &SMaterialEditor3DPreviewViewport::OnSetPreviewPrimitive, TPT_Sphere, false ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SMaterialEditor3DPreviewViewport::IsPreviewPrimitiveChecked, TPT_Sphere ) );

	CommandList->MapAction(
		Commands.SetPlanePreview,
		FExecuteAction::CreateSP( this, &SMaterialEditor3DPreviewViewport::OnSetPreviewPrimitive, TPT_Plane, false ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SMaterialEditor3DPreviewViewport::IsPreviewPrimitiveChecked, TPT_Plane ) );

	CommandList->MapAction(
		Commands.SetCubePreview,
		FExecuteAction::CreateSP( this, &SMaterialEditor3DPreviewViewport::OnSetPreviewPrimitive, TPT_Cube, false ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SMaterialEditor3DPreviewViewport::IsPreviewPrimitiveChecked, TPT_Cube ) );

	CommandList->MapAction(
		Commands.SetPreviewMeshFromSelection,
		FExecuteAction::CreateSP( this, &SMaterialEditor3DPreviewViewport::OnSetPreviewMeshFromSelection ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SMaterialEditor3DPreviewViewport::IsPreviewMeshFromSelectionChecked ) );

	CommandList->MapAction(
		Commands.TogglePreviewGrid,
		FExecuteAction::CreateSP( this, &SMaterialEditor3DPreviewViewport::TogglePreviewGrid ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SMaterialEditor3DPreviewViewport::IsTogglePreviewGridChecked ) );

	CommandList->MapAction(
		Commands.TogglePreviewBackground,
		FExecuteAction::CreateSP( this, &SMaterialEditor3DPreviewViewport::TogglePreviewBackground ),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP( this, &SMaterialEditor3DPreviewViewport::IsTogglePreviewBackgroundChecked ) );
}

void SMaterialEditor3DPreviewViewport::OnFocusViewportToSelection()
{
	if( PreviewMeshComponent != nullptr )
	{
		EditorViewportClient->FocusViewportOnBounds( PreviewMeshComponent->Bounds );
	}
}

void SMaterialEditor3DPreviewViewport::OnSetPreviewPrimitive(EThumbnailPrimType PrimType, bool bInitialLoad)
{
	if (SceneViewport.IsValid())
	{
		UStaticMesh* Primitive = nullptr;
		switch (PrimType)
		{
		case TPT_Cylinder: Primitive = GUnrealEd->GetThumbnailManager()->EditorCylinder; break;
		case TPT_Sphere: Primitive = GUnrealEd->GetThumbnailManager()->EditorSphere; break;
		case TPT_Plane: Primitive = GUnrealEd->GetThumbnailManager()->EditorPlane; break;
		case TPT_Cube: Primitive = GUnrealEd->GetThumbnailManager()->EditorCube; break;
		}

		if (Primitive != nullptr)
		{
			SetPreviewAsset(Primitive);
			
			// Clear the thumbnail preview mesh
			if (UMaterialInterface* MaterialInterface = MaterialEditorPtr.Pin()->GetMaterialInterface())
			{
				MaterialInterface->PreviewMesh = nullptr;
				FMaterialEditor::UpdateThumbnailInfoPreviewMesh(MaterialInterface);
				if (!bInitialLoad)
				{
					MaterialInterface->MarkPackageDirty();
				}
			}
			
			RefreshViewport();
		}
	}
}

bool SMaterialEditor3DPreviewViewport::IsPreviewPrimitiveChecked(EThumbnailPrimType PrimType) const
{
	return PreviewPrimType == PrimType;
}

void SMaterialEditor3DPreviewViewport::OnSetPreviewMeshFromSelection()
{
	bool bFoundPreviewMesh = false;
	FEditorDelegates::LoadSelectedAssetsIfNeeded.Broadcast();

	UMaterialInterface* MaterialInterface = MaterialEditorPtr.Pin()->GetMaterialInterface();

	// Look for a selected asset that can be converted to a mesh component
	for (FSelectionIterator SelectionIt(*GEditor->GetSelectedObjects()); SelectionIt && !bFoundPreviewMesh; ++SelectionIt)
	{
		UObject* TestAsset = *SelectionIt;
		if (TestAsset->IsAsset())
		{
			if (TSubclassOf<UActorComponent> ComponentClass = FComponentAssetBrokerage::GetPrimaryComponentForAsset(TestAsset->GetClass()))
			{
				if (ComponentClass->IsChildOf(UMeshComponent::StaticClass()))
				{
					if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(TestAsset))
					{
						// Special case handling for skeletal meshes, sets the material to be usable with them
						if (MaterialInterface->GetMaterial())
						{
							bool bNeedsRecompile = false;
							MaterialInterface->GetMaterial()->SetMaterialUsage(bNeedsRecompile, MATUSAGE_SkeletalMesh);
						}
					}

					SetPreviewAsset(TestAsset);
					MaterialInterface->PreviewMesh = TestAsset->GetPathName();
					bFoundPreviewMesh = true;
				}
			}
		}
	}

	if (bFoundPreviewMesh)
	{
		FMaterialEditor::UpdateThumbnailInfoPreviewMesh(MaterialInterface);

		MaterialInterface->MarkPackageDirty();
		RefreshViewport();
	}
	else
	{
		FSuppressableWarningDialog::FSetupInfo Info(NSLOCTEXT("UnrealEd", "Warning_NoPreviewMeshFound_Message", "You need to select a mesh-based asset in the content browser to preview it."),
			NSLOCTEXT("UnrealEd", "Warning_NoPreviewMeshFound", "Warning: No Preview Mesh Found"), "Warning_NoPreviewMeshFound");
		Info.ConfirmText = NSLOCTEXT("UnrealEd", "Warning_NoPreviewMeshFound_Confirm", "Continue");
		
		FSuppressableWarningDialog NoPreviewMeshWarning( Info );
		NoPreviewMeshWarning.ShowModal();
	}
}

bool SMaterialEditor3DPreviewViewport::IsPreviewMeshFromSelectionChecked() const
{
	return (PreviewPrimType == TPT_None && PreviewMeshComponent != nullptr);
}

void SMaterialEditor3DPreviewViewport::TogglePreviewGrid()
{
	bShowGrid = !bShowGrid;
	EditorViewportClient->SetShowGrid(bShowGrid);
	RefreshViewport();
}

bool SMaterialEditor3DPreviewViewport::IsTogglePreviewGridChecked() const
{
	return bShowGrid;
}

void SMaterialEditor3DPreviewViewport::TogglePreviewBackground()
{
	bShowBackground = !bShowBackground;
	// @todo DB: Set the background mesh for the preview viewport.
	RefreshViewport();
}

bool SMaterialEditor3DPreviewViewport::IsTogglePreviewBackgroundChecked() const
{
	return bShowBackground;
}


void SMaterialEditor3DPreviewViewport::OnAssetViewerSettingsChanged(const FName& InPropertyName)
{
	if (InPropertyName == GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, bRotateLightingRig) || InPropertyName == NAME_None)
	{
		UAssetViewerSettings* Settings = UAssetViewerSettings::Get();
		const int32 ProfileIndex = AdvancedPreviewScene->GetCurrentProfileIndex();
		if (Settings->Profiles.IsValidIndex(ProfileIndex) &&
			Settings->Profiles[ProfileIndex].bRotateLightingRig
			&& !EditorViewportClient->IsRealtime())
		{
			EditorViewportClient->SetRealtime(true);
		}
	}
}

TSharedRef<class SEditorViewport> SMaterialEditor3DPreviewViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SMaterialEditor3DPreviewViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void SMaterialEditor3DPreviewViewport::OnFloatingButtonClicked()
{
}

TSharedRef<FEditorViewportClient> SMaterialEditor3DPreviewViewport::MakeEditorViewportClient() 
{
	EditorViewportClient = MakeShareable( new FMaterialEditorViewportClient(MaterialEditorPtr, *AdvancedPreviewScene.Get(), SharedThis(this)) );
	UAssetViewerSettings::Get()->OnAssetViewerSettingsChanged().AddRaw(this, &SMaterialEditor3DPreviewViewport::OnAssetViewerSettingsChanged);
	EditorViewportClient->SetViewLocation( FVector::ZeroVector );
	EditorViewportClient->SetViewRotation( FRotator(0.0f, -90.0f, 0.0f) );
	EditorViewportClient->SetViewLocationForOrbiting( FVector::ZeroVector );
	EditorViewportClient->bSetListenerPosition = false;
	EditorViewportClient->EngineShowFlags.EnableAdvancedFeatures();
	EditorViewportClient->EngineShowFlags.SetLighting(true);
	EditorViewportClient->EngineShowFlags.SetIndirectLightingCache(true);
	EditorViewportClient->EngineShowFlags.SetPostProcessing(true);
	EditorViewportClient->Invalidate();
	EditorViewportClient->VisibilityDelegate.BindSP( this, &SMaterialEditor3DPreviewViewport::IsVisible );

	return EditorViewportClient.ToSharedRef();
}

void SMaterialEditor3DPreviewViewport::PopulateViewportOverlays(TSharedRef<class SOverlay> Overlay)
{
	Overlay->AddSlot()
		.VAlign(VAlign_Top)
		[
			SNew(SMaterialEditorViewportToolBar, SharedThis(this))
		];

	Overlay->AddSlot()
		.VAlign(VAlign_Bottom)
		[
			SNew(SMaterialEditorViewportPreviewShapeToolBar, SharedThis(this))
		];
}

EVisibility SMaterialEditor3DPreviewViewport::OnGetViewportContentVisibility() const
{
	EVisibility BaseVisibility = SEditorViewport::OnGetViewportContentVisibility();
	if (BaseVisibility != EVisibility::Visible)
	{
		return BaseVisibility;
	}
	return IsVisible() ? EVisibility::Visible : EVisibility::Collapsed;
}

class SMaterialEditorUIPreviewZoomer : public SPanel
{
public:

	class FMaterialPreviewPanelSlot : public TSupportsOneChildMixin<FMaterialPreviewPanelSlot>
	{
	public:
		FMaterialPreviewPanelSlot()
			: TSupportsOneChildMixin<FMaterialPreviewPanelSlot>()
		{
		}
	};

	SLATE_BEGIN_ARGS(SMaterialEditorUIPreviewViewport){}
	SLATE_END_ARGS()

	SMaterialEditorUIPreviewZoomer()
	{
	}

	void Construct( const FArguments& InArgs, UMaterialInterface* InPreviewMaterial );

	virtual void OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;
	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FChildren* GetChildren() override;
	virtual FReply OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	bool ZoomBy( const float Amount );
	float GetZoomLevel() const;

	void SetPreviewSize( const FVector2D PreviewSize );
	void SetPreviewMaterial(UMaterialInterface* InPreviewMaterial);
private:
	mutable FVector2D CachedSize;
	float ZoomLevel;

	FMaterialPreviewPanelSlot ChildSlot;
	TSharedPtr<FSlateMaterialBrush> PreviewBrush;
	TSharedPtr<SImage> ImageWidget;
};


void SMaterialEditorUIPreviewZoomer::Construct( const FArguments& InArgs, UMaterialInterface* InPreviewMaterial )
{
	PreviewBrush = MakeShareable( new FSlateMaterialBrush( *InPreviewMaterial, FVector2D(250,250) ) );

	ChildSlot
	[
		SAssignNew( ImageWidget, SImage )
		.Image( PreviewBrush.Get() )
	];

	ZoomLevel = 1.0f;
}

void SMaterialEditorUIPreviewZoomer::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const 
{
	CachedSize = AllottedGeometry.GetLocalSize();

	const TSharedRef<SWidget>& ChildWidget = ChildSlot.GetWidget();
	if( ChildWidget->GetVisibility() != EVisibility::Collapsed )
	{
		const FVector2D& WidgetDesiredSize = ChildWidget->GetDesiredSize();

		ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(ChildWidget, FVector2D::ZeroVector, WidgetDesiredSize * ZoomLevel));
	}
}

FVector2D SMaterialEditorUIPreviewZoomer::ComputeDesiredSize(float) const
{
	FVector2D ThisDesiredSize = FVector2D::ZeroVector;

	const TSharedRef<SWidget>& ChildWidget = ChildSlot.GetWidget();
	if( ChildWidget->GetVisibility() != EVisibility::Collapsed )
	{
		ThisDesiredSize = ChildWidget->GetDesiredSize() * ZoomLevel;
	}

	return ThisDesiredSize;
}

FChildren* SMaterialEditorUIPreviewZoomer::GetChildren()
{
	return &ChildSlot;
}

FReply SMaterialEditorUIPreviewZoomer::OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	 ZoomBy(MouseEvent.GetWheelDelta());

	 return FReply::Handled();
}

int32 SMaterialEditorUIPreviewZoomer::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = SPanel::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	return LayerId;
}

bool SMaterialEditorUIPreviewZoomer::ZoomBy( const float Amount )
{
	static const float MinZoomLevel = 0.2f;
	static const float MaxZoomLevel = 4.0f;

	const float PrevZoomLevel = ZoomLevel;
	ZoomLevel = FMath::Clamp(ZoomLevel + (Amount * 0.05f), MinZoomLevel, MaxZoomLevel);
	return ZoomLevel != PrevZoomLevel;
}

float SMaterialEditorUIPreviewZoomer::GetZoomLevel() const
{
	return ZoomLevel;
}


void SMaterialEditorUIPreviewZoomer::SetPreviewSize( const FVector2D PreviewSize )
{
	PreviewBrush->ImageSize = PreviewSize;
}

void SMaterialEditorUIPreviewZoomer::SetPreviewMaterial(UMaterialInterface* InPreviewMaterial)
{
	PreviewBrush = MakeShareable(new FSlateMaterialBrush(*InPreviewMaterial, PreviewBrush->ImageSize));
	ImageWidget->SetImage(PreviewBrush.Get());
}

void SMaterialEditorUIPreviewViewport::Construct( const FArguments& InArgs, UMaterialInterface* PreviewMaterial )
{

	ChildSlot
	[
		SNew( SVerticalBox )
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew( SBorder )
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Top)
			.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
			[
				SNew( SHorizontalBox )
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding( 3.f )
				.AutoWidth()
				[
					SNew( STextBlock )
					.Text( LOCTEXT("PreviewSize", "Preview Size" ) )
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding( 3.f )
				.MaxWidth( 75 )
				[
					SNew( SNumericEntryBox<int32> )
					.AllowSpin( true )
					.MinValue(1)
					.MaxSliderValue( 4096 )
					.OnValueChanged( this, &SMaterialEditorUIPreviewViewport::OnPreviewXChanged )
					.OnValueCommitted( this, &SMaterialEditorUIPreviewViewport::OnPreviewXCommitted )
					.Value( this, &SMaterialEditorUIPreviewViewport::OnGetPreviewXValue )
					.MinDesiredValueWidth( 75 )
					.Label()
					[	
						SNew( SBox )
						.VAlign( VAlign_Center )
						[
							SNew( STextBlock )
							.Text( LOCTEXT("PreviewSize_X", "X") )
						]
					]
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding( 3.f )
				.MaxWidth( 75 )
				[
					SNew( SNumericEntryBox<int32> )
					.AllowSpin( true )
					.MinValue(1)
					.MaxSliderValue( 4096 )
					.MinDesiredValueWidth( 75 )
					.OnValueChanged( this, &SMaterialEditorUIPreviewViewport::OnPreviewYChanged )
					.OnValueCommitted( this, &SMaterialEditorUIPreviewViewport::OnPreviewYCommitted )
					.Value( this, &SMaterialEditorUIPreviewViewport::OnGetPreviewYValue )
					.Label()
					[	
						SNew( SBox )
						.VAlign( VAlign_Center )
						[
							SNew( STextBlock )
							.Text( LOCTEXT("PreviewSize_Y", "Y") )
						]
					]
				]
			]
		]
		+ SVerticalBox::Slot()
		[
			SNew( SBorder )
			.HAlign( HAlign_Center )
			.VAlign( VAlign_Center )
			.BorderImage( FEditorStyle::GetBrush("BlackBrush") )
			[
				SAssignNew( PreviewZoomer, SMaterialEditorUIPreviewZoomer, PreviewMaterial )
			]
		]
	];

	PreviewSize = FIntPoint(250,250);
	PreviewZoomer->SetPreviewSize( FVector2D(PreviewSize) );
}

void SMaterialEditorUIPreviewViewport::SetPreviewMaterial(UMaterialInterface* InMaterialInterface)
{
	PreviewZoomer->SetPreviewMaterial(InMaterialInterface);
}

void SMaterialEditorUIPreviewViewport::OnPreviewXChanged( int32 NewValue )
{
	PreviewSize.X = NewValue;
	PreviewZoomer->SetPreviewSize( FVector2D( PreviewSize ) );

}

void SMaterialEditorUIPreviewViewport::OnPreviewXCommitted( int32 NewValue, ETextCommit::Type )
{
	OnPreviewXChanged( NewValue );
}


void SMaterialEditorUIPreviewViewport::OnPreviewYChanged( int32 NewValue )
{
	PreviewSize.Y = NewValue;
	PreviewZoomer->SetPreviewSize( FVector2D( PreviewSize ) );

}

void SMaterialEditorUIPreviewViewport::OnPreviewYCommitted( int32 NewValue, ETextCommit::Type )
{
	OnPreviewYChanged( NewValue );
}

#undef LOCTEXT_NAMESPACE
