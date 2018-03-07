// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/WidgetComponent.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineGlobals.h"
#include "MaterialShared.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"
#include "Widgets/SWindow.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Input/HittestGrid.h"
#include "SceneManagement.h"
#include "DynamicMeshBuilder.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/BodySetup.h"
#include "Slate/SGameLayerManager.h"
#include "Slate/WidgetRenderer.h"
#include "Slate/SWorldWidgetScreenLayer.h"
#include "SViewport.h"

DECLARE_CYCLE_STAT(TEXT("3DHitTesting"), STAT_Slate3DHitTesting, STATGROUP_Slate);

class FWorldWidgetScreenLayer : public IGameLayer
{
public:
	FWorldWidgetScreenLayer(const FLocalPlayerContext& PlayerContext)
	{
		OwningPlayer = PlayerContext;
	}

	virtual ~FWorldWidgetScreenLayer()
	{
		// empty virtual destructor to help clang warning
	}

	void AddComponent(UWidgetComponent* Component)
	{
		if ( Component )
		{
			Components.AddUnique(Component);
			if ( ScreenLayer.IsValid() )
			{
				if ( UUserWidget* UserWidget = Component->GetUserWidgetObject() )
				{
					ScreenLayer.Pin()->AddComponent(Component, UserWidget->TakeWidget());
				}
			}
		}
	}

	void RemoveComponent(UWidgetComponent* Component)
	{
		if ( Component )
		{
			Components.RemoveSwap(Component);

			if ( ScreenLayer.IsValid() )
			{
				ScreenLayer.Pin()->RemoveComponent(Component);
			}
		}
	}

	virtual TSharedRef<SWidget> AsWidget() override
	{
		if ( ScreenLayer.IsValid() )
		{
			return ScreenLayer.Pin().ToSharedRef();
		}

		TSharedRef<SWorldWidgetScreenLayer> NewScreenLayer = SNew(SWorldWidgetScreenLayer, OwningPlayer);
		ScreenLayer = NewScreenLayer;

		// Add all the pending user widgets to the surface
		for ( TWeakObjectPtr<UWidgetComponent>& WeakComponent : Components )
		{
			if ( UWidgetComponent* Component = WeakComponent.Get() )
			{
				if ( UUserWidget* UserWidget = Component->GetUserWidgetObject() )
				{
					NewScreenLayer->AddComponent(Component, UserWidget->TakeWidget());
				}
			}
		}

		return NewScreenLayer;
	}

private:
	FLocalPlayerContext OwningPlayer;
	TWeakPtr<SWorldWidgetScreenLayer> ScreenLayer;
	TArray<TWeakObjectPtr<UWidgetComponent>> Components;
};




/**
* The hit tester used by all Widget Component objects.
*/
class FWidget3DHitTester : public ICustomHitTestPath
{
public:
	FWidget3DHitTester( UWorld* InWorld )
		: World( InWorld )
		, CachedFrame(-1)
	{}

	// ICustomHitTestPath implementation
	virtual TArray<FWidgetAndPointer> GetBubblePathAndVirtualCursors(const FGeometry& InGeometry, FVector2D DesktopSpaceCoordinate, bool bIgnoreEnabledStatus) const override
	{
		SCOPE_CYCLE_COUNTER(STAT_Slate3DHitTesting);

		if( World.IsValid() /*&& ensure( World->IsGameWorld() )*/ )
		{
			UWorld* SafeWorld = World.Get();
			if ( SafeWorld )
			{
				ULocalPlayer* const TargetPlayer = GEngine->GetLocalPlayerFromControllerId(SafeWorld, 0);

				if( TargetPlayer && TargetPlayer->PlayerController )
				{
					FVector2D LocalMouseCoordinate = InGeometry.AbsoluteToLocal(DesktopSpaceCoordinate) * InGeometry.Scale;

					if ( UPrimitiveComponent* HitComponent = GetHitResultAtScreenPositionAndCache(TargetPlayer->PlayerController, LocalMouseCoordinate) )
					{
						if ( UWidgetComponent* WidgetComponent = Cast<UWidgetComponent>(HitComponent) )
						{
							if ( WidgetComponent->GetReceiveHardwareInput() )
							{
								if ( WidgetComponent->GetDrawSize().X != 0 && WidgetComponent->GetDrawSize().Y != 0 )
								{
									// Get the "forward" vector based on the current rotation system.
									const FVector ForwardVector = WidgetComponent->GetForwardVector();

									// Make sure the player is interacting with the front of the widget
									if ( FVector::DotProduct(ForwardVector, CachedHitResult.ImpactPoint - CachedHitResult.TraceStart) < 0.f )
									{
										return WidgetComponent->GetHitWidgetPath(CachedHitResult.Location, bIgnoreEnabledStatus);
									}
								}
							}
						}
					}
				}
			}
		}

		return TArray<FWidgetAndPointer>();
	}

	virtual void ArrangeChildren( FArrangedChildren& ArrangedChildren ) const override
	{
		for( TWeakObjectPtr<UWidgetComponent> Component : RegisteredComponents )
		{
			UWidgetComponent* WidgetComponent = Component.Get();
			// Check if visible;
			if ( WidgetComponent && WidgetComponent->GetSlateWindow().IsValid() )
			{
				FGeometry WidgetGeom;

				ArrangedChildren.AddWidget( FArrangedWidget( WidgetComponent->GetSlateWindow().ToSharedRef(), WidgetGeom.MakeChild( WidgetComponent->GetDrawSize(), FSlateLayoutTransform() ) ) );
			}
		}
	}

	virtual TSharedPtr<struct FVirtualPointerPosition> TranslateMouseCoordinateFor3DChild( const TSharedRef<SWidget>& ChildWidget, const FGeometry& ViewportGeometry, const FVector2D& ScreenSpaceMouseCoordinate, const FVector2D& LastScreenSpaceMouseCoordinate ) const override
	{
		if ( World.IsValid() && ensure(World->IsGameWorld()) )
		{
			ULocalPlayer* const TargetPlayer = GEngine->GetLocalPlayerFromControllerId(World.Get(), 0);
			if ( TargetPlayer && TargetPlayer->PlayerController )
			{
				FVector2D LocalMouseCoordinate = ViewportGeometry.AbsoluteToLocal(ScreenSpaceMouseCoordinate) * ViewportGeometry.Scale;

				// Check for a hit against any widget components in the world
				for ( TWeakObjectPtr<UWidgetComponent> Component : RegisteredComponents )
				{
					UWidgetComponent* WidgetComponent = Component.Get();
					// Check if visible;
					if ( WidgetComponent && WidgetComponent->GetSlateWindow() == ChildWidget )
					{
						if ( UPrimitiveComponent* HitComponent = GetHitResultAtScreenPositionAndCache(TargetPlayer->PlayerController, LocalMouseCoordinate) )
						{
							if ( WidgetComponent->GetReceiveHardwareInput() )
							{
								if ( WidgetComponent->GetDrawSize().X != 0 && WidgetComponent->GetDrawSize().Y != 0 )
								{
									if ( WidgetComponent == HitComponent )
									{
										TSharedPtr<FVirtualPointerPosition> VirtualCursorPos = MakeShareable(new FVirtualPointerPosition);

										FVector2D LocalHitLocation;
										WidgetComponent->GetLocalHitLocation(CachedHitResult.Location, LocalHitLocation);

										VirtualCursorPos->CurrentCursorPosition = LocalHitLocation;
										VirtualCursorPos->LastCursorPosition = LocalHitLocation;

										return VirtualCursorPos;
									}
								}
							}
						}
					}
				}
			}
		}

		return nullptr;
	}
	// End ICustomHitTestPath

	UPrimitiveComponent* GetHitResultAtScreenPositionAndCache(APlayerController* PlayerController, FVector2D ScreenPosition) const
	{
		UPrimitiveComponent* HitComponent = nullptr;

		if ( GFrameNumber != CachedFrame || CachedScreenPosition != ScreenPosition )
		{
			CachedFrame = GFrameNumber;
			CachedScreenPosition = ScreenPosition;

			if ( PlayerController )
			{
				if ( PlayerController->GetHitResultAtScreenPosition(ScreenPosition, ECC_Visibility, true, CachedHitResult) )
				{
					return CachedHitResult.Component.Get();
				}
			}
		}
		else
		{
			return CachedHitResult.Component.Get();
		}

		return nullptr;
	}

	void RegisterWidgetComponent( UWidgetComponent* InComponent )
	{
		RegisteredComponents.AddUnique( InComponent );
	}

	void UnregisterWidgetComponent( UWidgetComponent* InComponent )
	{
		RegisteredComponents.RemoveSingleSwap( InComponent );
	}

	uint32 GetNumRegisteredComponents() const { return RegisteredComponents.Num(); }
	
	UWorld* GetWorld() const { return World.Get(); }

private:
	TArray< TWeakObjectPtr<UWidgetComponent> > RegisteredComponents;
	TWeakObjectPtr<UWorld> World;

	mutable int64 CachedFrame;
	mutable FVector2D CachedScreenPosition;
	mutable FHitResult CachedHitResult;
};



/** Represents a billboard sprite to the scene manager. */
class FWidget3DSceneProxy : public FPrimitiveSceneProxy
{
public:
	/** Initialization constructor. */
	FWidget3DSceneProxy( UWidgetComponent* InComponent, ISlate3DRenderer& InRenderer )
		: FPrimitiveSceneProxy( InComponent )
		, Pivot( InComponent->GetPivot() )
		, Renderer( InRenderer )
		, RenderTarget( InComponent->GetRenderTarget() )
		, MaterialInstance( InComponent->GetMaterialInstance() )
		, BodySetup( InComponent->GetBodySetup() )
		, BlendMode( InComponent->GetBlendMode() )
		, GeometryMode(InComponent->GetGeometryMode())
		, ArcAngle(FMath::DegreesToRadians(InComponent->GetCylinderArcAngle()))
	{
		bWillEverBeLit = false;

		MaterialRelevance = MaterialInstance->GetRelevance(GetScene().GetFeatureLevel());
	}

	// FPrimitiveSceneProxy interface.
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
#if WITH_EDITOR
		const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

		auto WireframeMaterialInstance = new FColoredMaterialRenderProxy(
			GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy(IsSelected()) : nullptr,
			FLinearColor(0, 0.5f, 1.f)
			);

		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);

		FMaterialRenderProxy* ParentMaterialProxy = nullptr;
		if ( bWireframe )
		{
			ParentMaterialProxy = WireframeMaterialInstance;
		}
		else
		{
			ParentMaterialProxy = MaterialInstance->GetRenderProxy(IsSelected());
		}
#else
		FMaterialRenderProxy* ParentMaterialProxy = MaterialInstance->GetRenderProxy(IsSelected());
#endif

		//FSpriteTextureOverrideRenderProxy* TextureOverrideMaterialProxy = new FSpriteTextureOverrideRenderProxy(ParentMaterialProxy,

		const FMatrix& ViewportLocalToWorld = GetLocalToWorld();
	
		if( RenderTarget )
		{
			FTextureResource* TextureResource = RenderTarget->Resource;
			if ( TextureResource )
			{
				if (GeometryMode == EWidgetGeometryMode::Plane)
				{
					float U = -RenderTarget->SizeX * Pivot.X;
					float V = -RenderTarget->SizeY * Pivot.Y;
					float UL = RenderTarget->SizeX * (1.0f - Pivot.X);
					float VL = RenderTarget->SizeY * (1.0f - Pivot.Y);

					int32 VertexIndices[4];

					for ( int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++ )
					{
						FDynamicMeshBuilder MeshBuilder;

						if ( VisibilityMap & ( 1 << ViewIndex ) )
						{
							VertexIndices[0] = MeshBuilder.AddVertex(-FVector(0, U, V ),  FVector2D(0, 0), FVector(0, -1, 0), FVector(0, 0, -1), FVector(1, 0, 0), FColor::White);
							VertexIndices[1] = MeshBuilder.AddVertex(-FVector(0, U, VL),  FVector2D(0, 1), FVector(0, -1, 0), FVector(0, 0, -1), FVector(1, 0, 0), FColor::White);
							VertexIndices[2] = MeshBuilder.AddVertex(-FVector(0, UL, VL), FVector2D(1, 1), FVector(0, -1, 0), FVector(0, 0, -1), FVector(1, 0, 0), FColor::White);
							VertexIndices[3] = MeshBuilder.AddVertex(-FVector(0, UL, V),  FVector2D(1, 0), FVector(0, -1, 0), FVector(0, 0, -1), FVector(1, 0, 0), FColor::White);

							MeshBuilder.AddTriangle(VertexIndices[0], VertexIndices[1], VertexIndices[2]);
							MeshBuilder.AddTriangle(VertexIndices[0], VertexIndices[2], VertexIndices[3]);

							MeshBuilder.GetMesh(ViewportLocalToWorld, ParentMaterialProxy, SDPG_World, false, true, ViewIndex, Collector);
						}
					}
				}
				else
				{
					ensure(GeometryMode == EWidgetGeometryMode::Cylinder);

					const int32 NumSegments = FMath::Lerp(4, 32, ArcAngle/PI);


					const float Radius = RenderTarget->SizeX / ArcAngle;
					const float Apothem = Radius * FMath::Cos(0.5f*ArcAngle);
					const float ChordLength = 2.0f * Radius * FMath::Sin(0.5f*ArcAngle);
					
					const float PivotOffsetX = ChordLength * (0.5-Pivot.X);
					const float V = -RenderTarget->SizeY * Pivot.Y;
					const float VL = RenderTarget->SizeY * (1.0f - Pivot.Y);

					int32 VertexIndices[4];

					for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
					{
						FDynamicMeshBuilder MeshBuilder;

						if (VisibilityMap & (1 << ViewIndex))
						{
							const float RadiansPerStep = ArcAngle / NumSegments;

							FVector LastTangentX;
							FVector LastTangentY;
							FVector LastTangentZ;

							for (int32 Segment = 0; Segment < NumSegments; Segment++ )
							{
								const float Angle = -ArcAngle / 2 + Segment * RadiansPerStep;
								const float NextAngle = Angle + RadiansPerStep;
								
								// Polar to Cartesian
								const float X0 = Radius * FMath::Cos(Angle) - Apothem;
								const float Y0 = Radius * FMath::Sin(Angle);
								const float X1 = Radius * FMath::Cos(NextAngle) - Apothem;
								const float Y1 = Radius * FMath::Sin(NextAngle);

								const float U0 = static_cast<float>(Segment) / NumSegments;
								const float U1 = static_cast<float>(Segment+1) / NumSegments;

								const FVector Vertex0 = -FVector(X0, PivotOffsetX + Y0, V);
								const FVector Vertex1 = -FVector(X0, PivotOffsetX + Y0, VL);
								const FVector Vertex2 = -FVector(X1, PivotOffsetX + Y1, VL);
								const FVector Vertex3 = -FVector(X1, PivotOffsetX + Y1, V);

								FVector TangentX = Vertex3 - Vertex0;
								TangentX.Normalize();
								FVector TangentY = Vertex1 - Vertex0;
								TangentY.Normalize();
								FVector TangentZ = FVector::CrossProduct(TangentX, TangentY);

								if (Segment == 0)
								{
									LastTangentX = TangentX;
									LastTangentY = TangentY;
									LastTangentZ = TangentZ;
								}

								VertexIndices[0] = MeshBuilder.AddVertex(Vertex0, FVector2D(U0, 0), LastTangentX, LastTangentY, LastTangentZ, FColor::White);
								VertexIndices[1] = MeshBuilder.AddVertex(Vertex1, FVector2D(U0, 1), LastTangentX, LastTangentY, LastTangentZ, FColor::White);
								VertexIndices[2] = MeshBuilder.AddVertex(Vertex2, FVector2D(U1, 1), TangentX, TangentY, TangentZ, FColor::White);
								VertexIndices[3] = MeshBuilder.AddVertex(Vertex3, FVector2D(U1, 0), TangentX, TangentY, TangentZ, FColor::White);

								MeshBuilder.AddTriangle(VertexIndices[0], VertexIndices[1], VertexIndices[2]);
								MeshBuilder.AddTriangle(VertexIndices[0], VertexIndices[2], VertexIndices[3]);

								LastTangentX = TangentX;
								LastTangentY = TangentY;
								LastTangentZ = TangentZ;
							}
							MeshBuilder.GetMesh(ViewportLocalToWorld, ParentMaterialProxy, SDPG_World, false, true, ViewIndex, Collector);
						}
					}
				}
			}
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		for ( int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++ )
		{
			if ( VisibilityMap & ( 1 << ViewIndex ) )
			{
				RenderCollision(BodySetup, Collector, ViewIndex, ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
				RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
			}
		}
#endif
	}

	void RenderCollision(UBodySetup* InBodySetup, FMeshElementCollector& Collector, int32 ViewIndex, const FEngineShowFlags& EngineShowFlags, const FBoxSphereBounds& InBounds, bool bRenderInEditor) const
	{
		if ( InBodySetup )
		{
			bool bDrawCollision = EngineShowFlags.Collision && IsCollisionEnabled();

			if ( bDrawCollision && AllowDebugViewmodes() )
			{
				// Draw simple collision as wireframe if 'show collision', collision is enabled, and we are not using the complex as the simple
				const bool bDrawSimpleWireframeCollision = InBodySetup->CollisionTraceFlag != ECollisionTraceFlag::CTF_UseComplexAsSimple;

				if ( FMath::Abs(GetLocalToWorld().Determinant()) < SMALL_NUMBER )
				{
					// Catch this here or otherwise GeomTransform below will assert
					// This spams so commented out
					//UE_LOG(LogStaticMesh, Log, TEXT("Zero scaling not supported (%s)"), *StaticMesh->GetPathName());
				}
				else
				{
					const bool bDrawSolid = !bDrawSimpleWireframeCollision;
					const bool bProxyIsSelected = IsSelected();

					if ( bDrawSolid )
					{
						// Make a material for drawing solid collision stuff
						auto SolidMaterialInstance = new FColoredMaterialRenderProxy(
							GEngine->ShadedLevelColorationUnlitMaterial->GetRenderProxy(IsSelected(), IsHovered()),
							WireframeColor
							);

						Collector.RegisterOneFrameMaterialProxy(SolidMaterialInstance);

						FTransform GeomTransform(GetLocalToWorld());
						InBodySetup->AggGeom.GetAggGeom(GeomTransform, WireframeColor.ToFColor(true), SolidMaterialInstance, false, true, UseEditorDepthTest(), ViewIndex, Collector);
					}
					// wireframe
					else
					{
						FColor CollisionColor = FColor(157, 149, 223, 255);
						FTransform GeomTransform(GetLocalToWorld());
						InBodySetup->AggGeom.GetAggGeom(GeomTransform, GetSelectionColor(CollisionColor, bProxyIsSelected, IsHovered()).ToFColor(true), nullptr, false, false, UseEditorDepthTest(), ViewIndex, Collector);
					}
				}
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		bool bVisible = true;

		FPrimitiveViewRelevance Result;

		MaterialRelevance.SetPrimitiveViewRelevance(Result);

		Result.bDrawRelevance = IsShown(View) && bVisible && View->Family->EngineShowFlags.WidgetComponents;
		Result.bDynamicRelevance = true;
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bEditorPrimitiveRelevance = false;

		return Result;
	}

	virtual void GetLightRelevance(const FLightSceneProxy* LightSceneProxy, bool& bDynamic, bool& bRelevant, bool& bLightMapped, bool& bShadowMapped) const override
	{
		bDynamic = false;
		bRelevant = false;
		bLightMapped = false;
		bShadowMapped = false;
	}

	virtual void OnTransformChanged() override
	{
		Origin = GetLocalToWorld().GetOrigin();
	}

	virtual bool CanBeOccluded() const override
	{
		return !MaterialRelevance.bDisableDepthTest;
	}

	virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + GetAllocatedSize()); }

	uint32 GetAllocatedSize(void) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

private:
	FVector Origin;
	FVector2D Pivot;
	ISlate3DRenderer& Renderer;
	UTextureRenderTarget2D* RenderTarget;
	UMaterialInstanceDynamic* MaterialInstance;
	FMaterialRelevance MaterialRelevance;
	UBodySetup* BodySetup;
	EWidgetBlendMode BlendMode;
	EWidgetGeometryMode GeometryMode;
	float ArcAngle;
};






UWidgetComponent::UWidgetComponent( const FObjectInitializer& PCIP )
	: Super( PCIP )
	, DrawSize( FIntPoint( 500, 500 ) )
	, bManuallyRedraw(false)
	, bRedrawRequested(true)
	, RedrawTime(0)
	, LastWidgetRenderTime(0)
	, bReceiveHardwareInput(false)
	, bWindowFocusable(true)
	, BackgroundColor( FLinearColor::Transparent )
	, TintColorAndOpacity( FLinearColor::White )
	, OpacityFromTexture( 1.0f )
	, BlendMode( EWidgetBlendMode::Masked )
	, bIsTwoSided( false )
	, TickWhenOffscreen( false )
	, SharedLayerName(TEXT("WidgetComponentScreenLayer"))
	, LayerZOrder(-100)
	, GeometryMode(EWidgetGeometryMode::Plane)
	, CylinderArcAngle( 180.0f )
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;

	RelativeRotation = FRotator::ZeroRotator;

	BodyInstance.SetCollisionProfileName(FName(TEXT("UI")));

	// Translucent material instances
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TranslucentMaterial_Finder( TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent") );
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TranslucentMaterial_OneSided_Finder(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent_OneSided"));
	TranslucentMaterial = TranslucentMaterial_Finder.Object;
	TranslucentMaterial_OneSided = TranslucentMaterial_OneSided_Finder.Object;

	// Opaque material instances
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OpaqueMaterial_Finder( TEXT( "/Engine/EngineMaterials/Widget3DPassThrough_Opaque" ) );
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OpaqueMaterial_OneSided_Finder(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque_OneSided"));
	OpaqueMaterial = OpaqueMaterial_Finder.Object;
	OpaqueMaterial_OneSided = OpaqueMaterial_OneSided_Finder.Object;

	// Masked material instances
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaskedMaterial_Finder(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Masked"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaskedMaterial_OneSided_Finder(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Masked_OneSided"));
	MaskedMaterial = MaskedMaterial_Finder.Object;
	MaskedMaterial_OneSided = MaskedMaterial_OneSided_Finder.Object;

	LastLocalHitLocation = FVector2D::ZeroVector;
	//bGenerateOverlapEvents = false;
	bUseEditorCompositing = false;

	Space = EWidgetSpace::World;
	TimingPolicy = EWidgetTimingPolicy::RealTime;
	Pivot = FVector2D(0.5, 0.5);

	bAddedToScreen = false;
}

void UWidgetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseResources();
	Super::EndPlay(EndPlayReason);
}

FPrimitiveSceneProxy* UWidgetComponent::CreateSceneProxy()
{
	// Always clear the material instance in case we're going from 3D to 2D.
	if ( MaterialInstance )
	{
		MaterialInstance = nullptr;
	}

	if (Space == EWidgetSpace::Screen)
	{
		return nullptr;
	}

	if (WidgetRenderer.IsValid() && CurrentSlateWidget.IsValid())
	{
		// Create a new MID for the current base material
		{
			UMaterialInterface* BaseMaterial = GetMaterial(0);

			MaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);

			UpdateMaterialInstanceParameters();
		}

		RequestRedraw();
		LastWidgetRenderTime = 0;

		return new FWidget3DSceneProxy(this, *WidgetRenderer->GetSlateRenderer());
	}

#if WITH_EDITOR
	// make something so we can see this component in the editor
	class FWidgetBoxProxy : public FPrimitiveSceneProxy
	{
	public:
		FWidgetBoxProxy(const UWidgetComponent* InComponent)
			: FPrimitiveSceneProxy(InComponent)
			, BoxExtents(1.f, InComponent->GetDrawSize().X / 2.0f, InComponent->GetDrawSize().Y / 2.0f)
		{
			bWillEverBeLit = false;
		}

		virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_BoxSceneProxy_GetDynamicMeshElements);

			const FMatrix& LocalToWorld = GetLocalToWorld();

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if (VisibilityMap & (1 << ViewIndex))
				{
					const FSceneView* View = Views[ViewIndex];

					const FLinearColor DrawColor = GetViewSelectionColor(FColor::White, *View, IsSelected(), IsHovered(), false, IsIndividuallySelected());

					FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
					DrawOrientedWireBox(PDI, LocalToWorld.GetOrigin(), LocalToWorld.GetScaledAxis(EAxis::X), LocalToWorld.GetScaledAxis(EAxis::Y), LocalToWorld.GetScaledAxis(EAxis::Z), BoxExtents, DrawColor, SDPG_World);
				}
			}
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
		{
			FPrimitiveViewRelevance Result;
			if (!View->bIsGameView)
			{
				// Should we draw this because collision drawing is enabled, and we have collision
				const bool bShowForCollision = View->Family->EngineShowFlags.Collision && IsCollisionEnabled();
				Result.bDrawRelevance = IsShown(View) || bShowForCollision;
				Result.bDynamicRelevance = true;
				Result.bShadowRelevance = IsShadowCast(View);
				Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
			}
			return Result;
		}
		virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + GetAllocatedSize()); }
		uint32 GetAllocatedSize(void) const { return(FPrimitiveSceneProxy::GetAllocatedSize()); }

	private:
		const FVector	BoxExtents;
	};

	return new FWidgetBoxProxy(this);
#else
	return nullptr;
#endif
}

FBoxSphereBounds UWidgetComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	if ( Space != EWidgetSpace::Screen )
	{
		const FVector Origin = FVector(.5f,
			-( DrawSize.X * 0.5f ) + ( DrawSize.X * Pivot.X ),
			-( DrawSize.Y * 0.5f ) + ( DrawSize.Y * Pivot.Y ));

		const FVector BoxExtent = FVector(1.f, DrawSize.X / 2.0f, DrawSize.Y / 2.0f);

		FBoxSphereBounds NewBounds(Origin, BoxExtent, DrawSize.Size() / 2.0f);
		NewBounds = NewBounds.TransformBy(LocalToWorld);

		NewBounds.BoxExtent *= BoundsScale;
		NewBounds.SphereRadius *= BoundsScale;

		return NewBounds;
	}
	else
	{
		return FBoxSphereBounds(ForceInit).TransformBy(LocalToWorld);
	}
}

UBodySetup* UWidgetComponent::GetBodySetup() 
{
	UpdateBodySetup();
	return BodySetup;
}

FCollisionShape UWidgetComponent::GetCollisionShape(float Inflation) const
{
	if ( Space != EWidgetSpace::Screen )
	{
		FVector BoxHalfExtent = ( FVector(0.01f, DrawSize.X * 0.5f, DrawSize.Y * 0.5f) * GetComponentTransform().GetScale3D() ) + Inflation;

		if ( Inflation < 0.0f )
		{
			// Don't shrink below zero size.
			BoxHalfExtent = BoxHalfExtent.ComponentMax(FVector::ZeroVector);
		}

		return FCollisionShape::MakeBox(BoxHalfExtent);
	}
	else
	{
		return FCollisionShape::MakeBox(FVector::ZeroVector);
	}
}

void UWidgetComponent::OnRegister()
{
	Super::OnRegister();

#if !UE_SERVER
	if ( !IsRunningDedicatedServer() )
	{
		if ( Space != EWidgetSpace::Screen )
		{
			if ( CanReceiveHardwareInput() && GetWorld()->IsGameWorld() )
			{
				TSharedPtr<SViewport> GameViewportWidget = GEngine->GetGameViewportWidget();
				RegisterHitTesterWithViewport(GameViewportWidget);
			}

			if ( !WidgetRenderer.IsValid() && !GUsingNullRHI )
			{
				WidgetRenderer = MakeShareable(new FWidgetRenderer());
			}
		}

		BodySetup = nullptr;

		InitWidget();
	}
#endif // !UE_SERVER
}

bool UWidgetComponent::CanReceiveHardwareInput() const
{
	return bReceiveHardwareInput && GeometryMode == EWidgetGeometryMode::Plane;
}

void UWidgetComponent::RegisterHitTesterWithViewport(TSharedPtr<SViewport> ViewportWidget)
{
#if !UE_SERVER
	if ( ViewportWidget.IsValid() )
	{
		TSharedPtr<ICustomHitTestPath> CustomHitTestPath = ViewportWidget->GetCustomHitTestPath();
		if ( !CustomHitTestPath.IsValid() )
		{
			CustomHitTestPath = MakeShareable(new FWidget3DHitTester(GetWorld()));
			ViewportWidget->SetCustomHitTestPath(CustomHitTestPath);
		}

		TSharedPtr<FWidget3DHitTester> Widget3DHitTester = StaticCastSharedPtr<FWidget3DHitTester>(CustomHitTestPath);
		if ( Widget3DHitTester->GetWorld() == GetWorld() )
		{
			Widget3DHitTester->RegisterWidgetComponent(this);
		}
	}
#endif

}

void UWidgetComponent::UnregisterHitTesterWithViewport(TSharedPtr<SViewport> ViewportWidget)
{
#if !UE_SERVER
	if ( CanReceiveHardwareInput() )
	{
		TSharedPtr<ICustomHitTestPath> CustomHitTestPath = ViewportWidget->GetCustomHitTestPath();
		if ( CustomHitTestPath.IsValid() )
		{
			TSharedPtr<FWidget3DHitTester> WidgetHitTestPath = StaticCastSharedPtr<FWidget3DHitTester>(CustomHitTestPath);

			WidgetHitTestPath->UnregisterWidgetComponent(this);

			if ( WidgetHitTestPath->GetNumRegisteredComponents() == 0 )
			{
				ViewportWidget->SetCustomHitTestPath(nullptr);
			}
		}
	}
#endif
}

void UWidgetComponent::OnUnregister()
{
#if !UE_SERVER
	if ( GetWorld()->IsGameWorld() )
	{
		TSharedPtr<SViewport> GameViewportWidget = GEngine->GetGameViewportWidget();
		if ( GameViewportWidget.IsValid() )
		{
			UnregisterHitTesterWithViewport(GameViewportWidget);
		}
	}
#endif

#if WITH_EDITOR
	if (!GetWorld()->IsGameWorld())
	{
		ReleaseResources();
	}
#endif

	Super::OnUnregister();
}

void UWidgetComponent::DestroyComponent(bool bPromoteChildren/*= false*/)
{
	Super::DestroyComponent(bPromoteChildren);

	ReleaseResources();
}

void UWidgetComponent::ReleaseResources()
{
	if ( Widget )
	{
		RemoveWidgetFromScreen();
		Widget = nullptr;
	}

	WidgetRenderer.Reset();

	UnregisterWindow();
}

void UWidgetComponent::RegisterWindow()
{
	if ( SlateWindow.IsValid() )
	{
		if (!CanReceiveHardwareInput() && FSlateApplication::IsInitialized() )
		{
			FSlateApplication::Get().RegisterVirtualWindow(SlateWindow.ToSharedRef());
		}
	}
}

void UWidgetComponent::UnregisterWindow()
{
	if ( SlateWindow.IsValid() )
	{
		if ( !CanReceiveHardwareInput() && FSlateApplication::IsInitialized() )
		{
			FSlateApplication::Get().UnregisterVirtualWindow(SlateWindow.ToSharedRef());
		}

		SlateWindow.Reset();
	}
}

void UWidgetComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		UpdateWidget();

		if ( Widget == nullptr && !SlateWidget.IsValid() )
		{
			return;
		}

	    if ( Space != EWidgetSpace::Screen )
	    {
			if ( ShouldDrawWidget() )
		    {
				// Calculate the actual delta time since we last drew, this handles the case where we're ticking when
				// the world is paused, this also takes care of the case where the widget component is rendering at
				// a different rate than the rest of the world.
				const float DeltaTimeFromLastDraw = LastWidgetRenderTime == 0 ? 0 : (GetCurrentTime() - LastWidgetRenderTime);
			    DrawWidgetToRenderTarget(DeltaTimeFromLastDraw);
		    }
	    }
	    else
	    {
			if ( ( Widget && !Widget->IsDesignTime() ) || SlateWidget.IsValid() )
		    {
				UWorld* ThisWorld = GetWorld();

				ULocalPlayer* TargetPlayer = GetOwnerPlayer();
				APlayerController* PlayerController = TargetPlayer ? TargetPlayer->PlayerController : nullptr;

				if ( TargetPlayer && PlayerController && IsVisible() )
				{
					if ( !bAddedToScreen )
					{
						if ( ThisWorld->IsGameWorld() )
						{
							if ( UGameViewportClient* ViewportClient = ThisWorld->GetGameViewport() )
							{
								TSharedPtr<IGameLayerManager> LayerManager = ViewportClient->GetGameLayerManager();
								if ( LayerManager.IsValid() )
								{
									TSharedPtr<FWorldWidgetScreenLayer> ScreenLayer;

									FLocalPlayerContext PlayerContext(TargetPlayer, ThisWorld);

									TSharedPtr<IGameLayer> Layer = LayerManager->FindLayerForPlayer(TargetPlayer, SharedLayerName);
									if ( !Layer.IsValid() )
									{
										TSharedRef<FWorldWidgetScreenLayer> NewScreenLayer = MakeShareable(new FWorldWidgetScreenLayer(PlayerContext));
										LayerManager->AddLayerForPlayer(TargetPlayer, SharedLayerName, NewScreenLayer, LayerZOrder);
										ScreenLayer = NewScreenLayer;
									}
									else
									{
										ScreenLayer = StaticCastSharedPtr<FWorldWidgetScreenLayer>(Layer);
									}
								
									bAddedToScreen = true;
								
									Widget->SetPlayerContext(PlayerContext);
									ScreenLayer->AddComponent(this);
								}
							}
						}
					}
				}
				else if ( bAddedToScreen )
				{
					RemoveWidgetFromScreen();
				}
			}
		}
	}
#endif // !UE_SERVER
}

bool UWidgetComponent::ShouldDrawWidget() const
{
	const float RenderTimeThreshold = .5f;
	if ( IsVisible() )
	{
		// If we don't tick when off-screen, don't bother ticking if it hasn't been rendered recently
		if ( TickWhenOffscreen || GetWorld()->TimeSince(LastRenderTime) <= RenderTimeThreshold )
		{
			if ( ( GetCurrentTime() - LastWidgetRenderTime) >= RedrawTime )
			{
				return bManuallyRedraw ? bRedrawRequested : true;
			}
		}
	}

	return false;
}

void UWidgetComponent::DrawWidgetToRenderTarget(float DeltaTime)
{
	if ( GUsingNullRHI )
	{
		return;
	}

	if ( !SlateWindow.IsValid() )
	{
		return;
	}

	const int32 MaxAllowedDrawSize = GetMax2DTextureDimension();
	if ( DrawSize.X <= 0 || DrawSize.Y <= 0 || DrawSize.X > MaxAllowedDrawSize || DrawSize.Y > MaxAllowedDrawSize )
	{
		return;
	}

	CurrentDrawSize = DrawSize;

	const float DrawScale = 1.0f;

	if ( bDrawAtDesiredSize )
	{
		SlateWindow->SlatePrepass(DrawScale);

		FVector2D DesiredSize = SlateWindow->GetDesiredSize();
		DesiredSize.X = FMath::RoundToInt(DesiredSize.X);
		DesiredSize.Y = FMath::RoundToInt(DesiredSize.Y);
		CurrentDrawSize = DesiredSize.IntPoint();

		WidgetRenderer->SetIsPrepassNeeded(false);
	}
	else
	{
		WidgetRenderer->SetIsPrepassNeeded(true);
	}

	if ( CurrentDrawSize != DrawSize )
	{
		DrawSize = CurrentDrawSize;
		UpdateBodySetup(true);
		RecreatePhysicsState();
	}

	UpdateRenderTarget(CurrentDrawSize);

	// The render target could be null if the current draw size is zero
	if(RenderTarget)
	{
		bRedrawRequested = false;

		WidgetRenderer->DrawWindow(
			RenderTarget,
			SlateWindow->GetHittestGrid(),
			SlateWindow.ToSharedRef(),
			DrawScale,
			CurrentDrawSize,
			DeltaTime);

		LastWidgetRenderTime = GetCurrentTime();
	}
}

float UWidgetComponent::ComputeComponentWidth() const
{
	switch (GeometryMode)
	{
		default:
		case EWidgetGeometryMode::Plane:
			return DrawSize.X;
		break;

		case EWidgetGeometryMode::Cylinder:
			const float ArcAngleRadians = FMath::DegreesToRadians(GetCylinderArcAngle());
			const float Radius = GetDrawSize().X / ArcAngleRadians;
			// Chord length is 2*R*Sin(Theta/2)
			return 2.0f * Radius * FMath::Sin(0.5f*ArcAngleRadians);
		break;
	}
}

double UWidgetComponent::GetCurrentTime() const
{
	return (TimingPolicy == EWidgetTimingPolicy::RealTime) ? FApp::GetCurrentTime() : static_cast<double>(GetWorld()->GetTimeSeconds());
}

void UWidgetComponent::RemoveWidgetFromScreen()
{
#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		bAddedToScreen = false;

		if ( UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport() )
		{
			TSharedPtr<IGameLayerManager> LayerManager = ViewportClient->GetGameLayerManager();
			if ( LayerManager.IsValid() )
			{
				ULocalPlayer* TargetPlayer = GetOwnerPlayer();

				TSharedPtr<IGameLayer> Layer = LayerManager->FindLayerForPlayer(TargetPlayer, SharedLayerName);
				if ( Layer.IsValid() )
				{
					TSharedPtr<FWorldWidgetScreenLayer> ScreenLayer = StaticCastSharedPtr<FWorldWidgetScreenLayer>(Layer);
					ScreenLayer->RemoveComponent(this);
				}
			}
		}
	}
#endif // !UE_SERVER
}

class FWidgetComponentInstanceData : public FSceneComponentInstanceData
{
public:
	FWidgetComponentInstanceData( const UWidgetComponent* SourceComponent )
		: FSceneComponentInstanceData(SourceComponent)
		, WidgetClass ( SourceComponent->GetWidgetClass() )
		, RenderTarget( SourceComponent->GetRenderTarget() )
	{}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		FSceneComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);
		CastChecked<UWidgetComponent>(Component)->ApplyComponentInstanceData(this);
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		FSceneComponentInstanceData::AddReferencedObjects(Collector);

		UClass* WidgetUClass = *WidgetClass;
		Collector.AddReferencedObject(WidgetUClass);
		Collector.AddReferencedObject(RenderTarget);
	}

public:
	TSubclassOf<UUserWidget> WidgetClass;
	UTextureRenderTarget2D* RenderTarget;
};

FActorComponentInstanceData* UWidgetComponent::GetComponentInstanceData() const
{
	return new FWidgetComponentInstanceData( this );
}

void UWidgetComponent::ApplyComponentInstanceData(FWidgetComponentInstanceData* WidgetInstanceData)
{
	check(WidgetInstanceData);

	// Note: ApplyComponentInstanceData is called while the component is registered so the rendering thread is already using this component
	// That means all component state that is modified here must be mirrored on the scene proxy, which will be recreated to receive the changes later due to MarkRenderStateDirty.

	if (GetWidgetClass() != WidgetClass)
	{
		return;
	}

	RenderTarget = WidgetInstanceData->RenderTarget;
	if ( MaterialInstance && RenderTarget )
	{
		MaterialInstance->SetTextureParameterValue("SlateUI", RenderTarget);
	}

	MarkRenderStateDirty();
}

void UWidgetComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (MaterialInstance)
	{
		OutMaterials.AddUnique(MaterialInstance);
	}
}

#if WITH_EDITOR

bool UWidgetComponent::CanEditChange(const UProperty* InProperty) const
{
	if ( InProperty )
	{
		FString PropertyName = InProperty->GetName();

		if ( PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWidgetComponent, GeometryMode) ||
			 PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWidgetComponent, TimingPolicy) ||
			 PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWidgetComponent, bWindowFocusable) ||
			 PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWidgetComponent, bManuallyRedraw) ||
			 PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWidgetComponent, RedrawTime) ||
			 PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWidgetComponent, BackgroundColor) ||
			 PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWidgetComponent, TintColorAndOpacity) ||
			 PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWidgetComponent, OpacityFromTexture) ||
			 PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWidgetComponent, BlendMode) ||
			 PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWidgetComponent, bIsTwoSided) ||
			 PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWidgetComponent, TickWhenOffscreen) )
		{
			return Space != EWidgetSpace::Screen;
		}

		if ( PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWidgetComponent, bReceiveHardwareInput) )
		{
			return Space != EWidgetSpace::Screen && GeometryMode == EWidgetGeometryMode::Plane;
		}

		if ( PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UWidgetComponent, CylinderArcAngle) )
		{
			return GeometryMode == EWidgetGeometryMode::Cylinder;
		}
	}

	return Super::CanEditChange(InProperty);
}

void UWidgetComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* Property = PropertyChangedEvent.MemberProperty;

	if( Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive )
	{
		static FName DrawSizeName("DrawSize");
		static FName PivotName("Pivot");
		static FName WidgetClassName("WidgetClass");
		static FName IsOpaqueName("bIsOpaque");
		static FName IsTwoSidedName("bIsTwoSided");
		static FName BackgroundColorName("BackgroundColor");
		static FName TintColorAndOpacityName("TintColorAndOpacity");
		static FName OpacityFromTextureName("OpacityFromTexture");
		static FName ParabolaDistortionName(TEXT("ParabolaDistortion"));
		static FName BlendModeName( TEXT( "BlendMode" ) );
		static FName GeometryModeName( TEXT("GeometryMode") );
		static FName CylinderArcAngleName( TEXT("CylinderArcAngle") );

		auto PropertyName = Property->GetFName();

		if( PropertyName == WidgetClassName )
		{
			Widget = nullptr;

			UpdateWidget();
			MarkRenderStateDirty();
		}
		else if ( PropertyName == DrawSizeName || PropertyName == PivotName || PropertyName == GeometryModeName || PropertyName == CylinderArcAngleName )
		{
			MarkRenderStateDirty();
			UpdateBodySetup(true);
			RecreatePhysicsState();
		}
		else if ( PropertyName == IsOpaqueName || PropertyName == IsTwoSidedName || PropertyName == BlendModeName )
		{
			MarkRenderStateDirty();
		}
		else if( PropertyName == BackgroundColorName || PropertyName == ParabolaDistortionName )
		{
			MarkRenderStateDirty();
		}
		else if( PropertyName == TintColorAndOpacityName || PropertyName == OpacityFromTextureName )
		{
			MarkRenderStateDirty();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif

void UWidgetComponent::InitWidget()
{
	// Don't do any work if Slate is not initialized
	if ( FSlateApplication::IsInitialized() )
	{
		if ( WidgetClass && Widget == nullptr && GetWorld() )
		{
			Widget = CreateWidget<UUserWidget>(GetWorld(), WidgetClass);
		}
		
#if WITH_EDITOR
		if ( Widget && !GetWorld()->IsGameWorld() && !bEditTimeUsable )
		{
			if( !GEnableVREditorHacks )
			{
				// Prevent native ticking of editor component previews
				Widget->SetDesignerFlags(EWidgetDesignFlags::Designing);
			}
		}
#endif
	}
}

void UWidgetComponent::SetOwnerPlayer(ULocalPlayer* LocalPlayer)
{
	if ( OwnerPlayer != LocalPlayer )
	{
		RemoveWidgetFromScreen();
		OwnerPlayer = LocalPlayer;
	}
}

ULocalPlayer* UWidgetComponent::GetOwnerPlayer() const
{
	return OwnerPlayer ? OwnerPlayer : GEngine->GetLocalPlayerFromControllerId(GetWorld(), 0);
}

void UWidgetComponent::SetWidget(UUserWidget* InWidget)
{
	if (InWidget != nullptr)
	{
		SetSlateWidget(nullptr);
	}

	if (Widget)
	{
		RemoveWidgetFromScreen();
	}

	Widget = InWidget;

	UpdateWidget();
}

void UWidgetComponent::SetSlateWidget(const TSharedPtr<SWidget>& InSlateWidget)
{
	if (Widget != nullptr)
	{
		SetWidget(nullptr);
	}

	if (SlateWidget.IsValid())
	{
		RemoveWidgetFromScreen();
		SlateWidget.Reset();
	}

	SlateWidget = InSlateWidget;

	UpdateWidget();
}

void UWidgetComponent::UpdateWidget()
{
	// Don't do any work if Slate is not initialized
	if ( FSlateApplication::IsInitialized() )
	{
		if ( Space != EWidgetSpace::Screen )
		{
			TSharedPtr<SWidget> NewSlateWidget;
			if (Widget)
			{
				NewSlateWidget = Widget->TakeWidget();
			}

			bool bNeededNewWindow = false;
			if ( !SlateWindow.IsValid() )
			{
				SlateWindow = SNew(SVirtualWindow).Size(DrawSize);
				SlateWindow->SetIsFocusable(bWindowFocusable);
				RegisterWindow();

				bNeededNewWindow = true;
			}

			SlateWindow->Resize(DrawSize);

			bool bWidgetChanged = false;
			if ( NewSlateWidget.IsValid() )
			{
				if ( NewSlateWidget != CurrentSlateWidget || bNeededNewWindow )
				{
					CurrentSlateWidget = NewSlateWidget;
					SlateWindow->SetContent(NewSlateWidget.ToSharedRef());
					bWidgetChanged = true;
				}
			}
			else if( SlateWidget.IsValid() )
			{
				if ( SlateWidget != CurrentSlateWidget || bNeededNewWindow )
				{
					CurrentSlateWidget = SlateWidget;
					SlateWindow->SetContent(SlateWidget.ToSharedRef());
					bWidgetChanged = true;
				}
			}
			else
			{
				if (CurrentSlateWidget != SNullWidget::NullWidget)
				{
					CurrentSlateWidget = SNullWidget::NullWidget;
					bWidgetChanged = true;
				}
				SlateWindow->SetContent( SNullWidget::NullWidget );
			}

			if (bNeededNewWindow || bWidgetChanged)
			{
				MarkRenderStateDirty();
			}
		}
		else
		{
			UnregisterWindow();
		}
	}
}

void UWidgetComponent::UpdateRenderTarget(FIntPoint DesiredRenderTargetSize)
{
	bool bWidgetRenderStateDirty = false;
	bool bClearColorChanged = false;

	FLinearColor ActualBackgroundColor = BackgroundColor;
	switch ( BlendMode )
	{
	case EWidgetBlendMode::Opaque:
		ActualBackgroundColor.A = 1.0f;
		break;
	case EWidgetBlendMode::Masked:
		ActualBackgroundColor.A = 0.0f;
		break;
	}

	if ( DesiredRenderTargetSize.X != 0 && DesiredRenderTargetSize.Y != 0 )
	{
		if ( RenderTarget == nullptr )
		{
			RenderTarget = NewObject<UTextureRenderTarget2D>(this);
			RenderTarget->ClearColor = ActualBackgroundColor;

			bClearColorChanged = bWidgetRenderStateDirty = true;

			RenderTarget->InitCustomFormat(DesiredRenderTargetSize.X, DesiredRenderTargetSize.Y, PF_B8G8R8A8, false);

			if ( MaterialInstance )
			{
				MaterialInstance->SetTextureParameterValue("SlateUI", RenderTarget);
			}
		}
		else
		{
			// Update the format
			if ( RenderTarget->SizeX != DesiredRenderTargetSize.X || RenderTarget->SizeY != DesiredRenderTargetSize.Y )
			{
				RenderTarget->InitCustomFormat(DesiredRenderTargetSize.X, DesiredRenderTargetSize.Y, PF_B8G8R8A8, false);
				bWidgetRenderStateDirty = true;
			}

			// Update the clear color
			if ( RenderTarget->ClearColor != ActualBackgroundColor )
			{
				RenderTarget->ClearColor = ActualBackgroundColor;
				bClearColorChanged = bWidgetRenderStateDirty = true;
			}

			if ( bWidgetRenderStateDirty )
			{
				RenderTarget->UpdateResourceImmediate();
			}
		}
	}

	if ( RenderTarget )
	{
		// If the clear color of the render target changed, update the BackColor of the material to match
		if ( bClearColorChanged && MaterialInstance )
		{
			MaterialInstance->SetVectorParameterValue("BackColor", RenderTarget->ClearColor);
		}

		if ( bWidgetRenderStateDirty )
		{
			MarkRenderStateDirty();
		}
	}
}

void UWidgetComponent::UpdateBodySetup( bool bDrawSizeChanged )
{
	if (Space == EWidgetSpace::Screen)
	{
		// We do not have a body setup in screen space
		BodySetup = nullptr;
	}
	else if ( !BodySetup || bDrawSizeChanged )
	{
		BodySetup = NewObject<UBodySetup>(this);
		BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		BodySetup->AggGeom.BoxElems.Add(FKBoxElem());

		FKBoxElem* BoxElem = BodySetup->AggGeom.BoxElems.GetData();

		FVector Origin = FVector(.5f,
			-( DrawSize.X * 0.5f ) + ( DrawSize.X * Pivot.X ),
			-( DrawSize.Y * 0.5f ) + ( DrawSize.Y * Pivot.Y ));
			const float Width = ComputeComponentWidth();
			const float Height = DrawSize.Y;
			Origin = FVector(.5f,
				-( Width * 0.5f ) + ( Width * Pivot.X ),
				-( Height * 0.5f ) + ( Height * Pivot.Y ));
			
		BoxElem->X = 0.01f;
		BoxElem->Y = DrawSize.X;
		BoxElem->Z = DrawSize.Y;

		BoxElem->SetTransform(FTransform::Identity);
		BoxElem->Center = Origin;
	}
}

void UWidgetComponent::GetLocalHitLocation(FVector WorldHitLocation, FVector2D& OutLocalWidgetHitLocation) const
{
	ensureMsgf(GeometryMode == EWidgetGeometryMode::Plane, TEXT("Method does not support non-planar widgets."));

	// Find the hit location on the component
	FVector ComponentHitLocation = GetComponentTransform().InverseTransformPosition(WorldHitLocation);

	// Convert the 3D position of component space, into the 2D equivalent
	OutLocalWidgetHitLocation = FVector2D(-ComponentHitLocation.Y, -ComponentHitLocation.Z);

	// Offset the position by the pivot to get the position in widget space.
	OutLocalWidgetHitLocation.X += CurrentDrawSize.X * Pivot.X;
	OutLocalWidgetHitLocation.Y += CurrentDrawSize.Y * Pivot.Y;

	// Apply the parabola distortion
	FVector2D NormalizedLocation = OutLocalWidgetHitLocation / CurrentDrawSize;

	OutLocalWidgetHitLocation.Y = CurrentDrawSize.Y * NormalizedLocation.Y;
}


TOptional<float> FindLineSphereIntersection(const FVector& Start, const FVector& Dir, float Radius)
{
	// Solution exist at two possible locations:
	// (Start + Dir * t) (dot) (Start + Dir * t) = Radius^2
	// Dir(dot)Dir*t^2 + 2*Start(dot)Dir + Start(dot)Start - Radius^2 = 0
	//
	// Recognize quadratic form with:
	const float a = FVector::DotProduct(Dir,Dir);
	const float b = 2 * FVector::DotProduct(Start,Dir);
	const float c = FVector::DotProduct(Start,Start) - Radius*Radius;

	const float Discriminant = b*b - 4 * a * c;
	
	if (Discriminant >= 0)
	{
		const float SqrtDiscr = FMath::Sqrt(Discriminant);
		const float Soln1 = (-b + SqrtDiscr) / (2 * a);

		return Soln1;
	}
	else
	{
		return TOptional<float>();
	}
}

TTuple<FVector, FVector2D> UWidgetComponent::GetCylinderHitLocation(FVector WorldHitLocation, FVector WorldHitDirection) const
{
	// Turn this on to see a visualiztion of cylindrical collision testing.
	static const bool bDrawCollisionDebug = false;

	ensure(GeometryMode == EWidgetGeometryMode::Cylinder);
		

	FTransform ToWorld = GetComponentToWorld();

	const FVector HitLocation_ComponentSpace = GetComponentTransform().InverseTransformPosition(WorldHitLocation);
	const FVector HitDirection_ComponentSpace = GetComponentTransform().InverseTransformVector(WorldHitDirection);


	const float ArcAngleRadians = FMath::DegreesToRadians(GetCylinderArcAngle());
	const float Radius = GetDrawSize().X / ArcAngleRadians;
	const float Apothem = Radius * FMath::Cos(0.5f*ArcAngleRadians);
	const float ChordLength = 2.0f * Radius * FMath::Sin(0.5f*ArcAngleRadians);

	const float PivotOffsetX = ChordLength * (0.5-Pivot.X);

	if (bDrawCollisionDebug)
	{
		// Draw component-space axes
		UKismetSystemLibrary::DrawDebugArrow((UWidgetComponent*)(this), ToWorld.TransformPosition(FVector::ZeroVector), ToWorld.TransformPosition(FVector(50.f, 0, 0)), 2.0f, FLinearColor::Red);
		UKismetSystemLibrary::DrawDebugArrow((UWidgetComponent*)(this), ToWorld.TransformPosition(FVector::ZeroVector), ToWorld.TransformPosition(FVector(0, 50.f, 0)), 2.0f, FLinearColor::Green);
		UKismetSystemLibrary::DrawDebugArrow((UWidgetComponent*)(this), ToWorld.TransformPosition(FVector::ZeroVector), ToWorld.TransformPosition(FVector(0, 0, 50.f)), 2.0f, FLinearColor::Blue);

		// Draw the imaginary circle which we use to describe the cylinder.
		// Note that we transform all the hit locations into a space where the circle's origin is at (0,0).
		UKismetSystemLibrary::DrawDebugCircle((UWidgetComponent*)(this), ToWorld.TransformPosition(FVector::ZeroVector), ToWorld.GetScale3D().X*Radius, 64, FLinearColor::Green,
			0, 1.0f, FVector(0, 1, 0), FVector(1, 0, 0));
		UKismetSystemLibrary::DrawDebugLine((UWidgetComponent*)(this), ToWorld.TransformPosition(FVector(-Apothem, -Radius, 0.0f)), ToWorld.TransformPosition(FVector(-Apothem, +Radius, 0.0f)), FLinearColor::Green);
	}

	const FVector HitLocation_CircleSpace( -Apothem, HitLocation_ComponentSpace.Y + PivotOffsetX, 0.0f );
	const FVector HitDirection_CircleSpace( HitDirection_ComponentSpace.X, HitDirection_ComponentSpace.Y, 0.0f );

	// DRAW HIT DIRECTION
	if (bDrawCollisionDebug)
	{
		UKismetSystemLibrary::DrawDebugCircle((UWidgetComponent*)(this), ToWorld.TransformPosition(FVector(HitLocation_CircleSpace.X, HitLocation_CircleSpace.Y,0)), 2.0f);
		FVector HitDirection_CircleSpace_Normalized = HitDirection_CircleSpace;
		HitDirection_CircleSpace_Normalized.Normalize();
		HitDirection_CircleSpace_Normalized *= 40;
		UKismetSystemLibrary::DrawDebugLine(
			(UWidgetComponent*)(this),
			ToWorld.TransformPosition(FVector(HitLocation_CircleSpace.X, HitLocation_CircleSpace.Y, 0.0f)),
			ToWorld.TransformPosition(FVector(HitLocation_CircleSpace.X + HitDirection_CircleSpace_Normalized.X, HitLocation_CircleSpace.Y + HitDirection_CircleSpace_Normalized.Y, 0.0f)),
			FLinearColor::White, 0, 0.1f);
	}

	// Perform a ray vs. circle intersection test (effectively in 2D because Z coordinate is always 0)
	const TOptional<float> Solution = FindLineSphereIntersection(HitLocation_CircleSpace, HitDirection_CircleSpace, Radius);
	if (Solution.IsSet())
	{
		const float Time = Solution.GetValue();

		const FVector TrueHitLocation_CircleSpace = HitLocation_CircleSpace + HitDirection_CircleSpace * Time;
		if (bDrawCollisionDebug)
		{
			UKismetSystemLibrary::DrawDebugLine((UWidgetComponent*)(this),
				ToWorld.TransformPosition(FVector(HitLocation_CircleSpace.X, HitLocation_CircleSpace.Y, 0.0f)),
				ToWorld.TransformPosition(FVector(TrueHitLocation_CircleSpace.X, TrueHitLocation_CircleSpace.Y, 0.0f)),
				FLinearColor(1, 0, 1, 1), 0, 0.5f);
		 }
			
		// Determine the widget-space X hit coordinate.
		const float Endpoint1 = FMath::Fmod(FMath::Atan2(-0.5f*ChordLength, -Apothem) + 2*PI, 2*PI);
		const float Endpoint2 = FMath::Fmod(FMath::Atan2(+0.5f*ChordLength, -Apothem) + 2*PI, 2*PI);
		const float HitAngleRads = FMath::Fmod(FMath::Atan2(TrueHitLocation_CircleSpace.Y, TrueHitLocation_CircleSpace.X) + 2*PI, 2*PI);
		const float HitAngleZeroToOne = (HitAngleRads - FMath::Min(Endpoint1, Endpoint2)) / FMath::Abs(Endpoint2 - Endpoint1);


		// Determine the widget-space Y hit coordinate
		const FVector CylinderHitLocation_ComponentSpace = HitLocation_ComponentSpace + HitDirection_ComponentSpace*Time;
		const float YHitLocation = (-CylinderHitLocation_ComponentSpace.Z + CurrentDrawSize.Y*Pivot.Y);

		const FVector2D WidgetSpaceHitCoord = FVector2D(HitAngleZeroToOne * CurrentDrawSize.X, YHitLocation);
			
		return MakeTuple(GetComponentTransform().TransformPosition(CylinderHitLocation_ComponentSpace), WidgetSpaceHitCoord);
	}
	else
	{
		return MakeTuple(FVector::ZeroVector, FVector2D::ZeroVector);
	}
}

UUserWidget* UWidgetComponent::GetUserWidgetObject() const
{
	return Widget;
}

UTextureRenderTarget2D* UWidgetComponent::GetRenderTarget() const
{
	return RenderTarget;
}

UMaterialInstanceDynamic* UWidgetComponent::GetMaterialInstance() const
{
	return MaterialInstance;
}

const TSharedPtr<SWidget>& UWidgetComponent::GetSlateWidget() const
{
	return SlateWidget;
}

TArray<FWidgetAndPointer> UWidgetComponent::GetHitWidgetPath(FVector WorldHitLocation, bool bIgnoreEnabledStatus, float CursorRadius)
{
	ensure(GeometryMode == EWidgetGeometryMode::Plane);

	FVector2D LocalHitLocation;
	GetLocalHitLocation(WorldHitLocation, LocalHitLocation);

	return GetHitWidgetPath(LocalHitLocation, bIgnoreEnabledStatus, CursorRadius);
}


TArray<FWidgetAndPointer> UWidgetComponent::GetHitWidgetPath(FVector2D WidgetSpaceHitCoordinate, bool bIgnoreEnabledStatus, float CursorRadius /*= 0.0f*/)
{
	TSharedRef<FVirtualPointerPosition> VirtualMouseCoordinate = MakeShareable(new FVirtualPointerPosition);

	const FVector2D& LocalHitLocation = WidgetSpaceHitCoordinate;

	VirtualMouseCoordinate->CurrentCursorPosition = LocalHitLocation;
	VirtualMouseCoordinate->LastCursorPosition = LastLocalHitLocation;

	// Cache the location of the hit
	LastLocalHitLocation = LocalHitLocation;

	TArray<FWidgetAndPointer> ArrangedWidgets;
	if ( SlateWindow.IsValid() )
	{
		ArrangedWidgets = SlateWindow->GetHittestGrid()->GetBubblePath( LocalHitLocation, CursorRadius, bIgnoreEnabledStatus );

		for( FWidgetAndPointer& ArrangedWidget : ArrangedWidgets )
		{
			ArrangedWidget.PointerPosition = VirtualMouseCoordinate;
		}
	}

	return ArrangedWidgets;
}

TSharedPtr<SWindow> UWidgetComponent::GetSlateWindow() const
{
	return SlateWindow;
}

FVector2D UWidgetComponent::GetDrawSize() const
{
	return DrawSize;
}

void UWidgetComponent::SetDrawSize(FVector2D Size)
{
	FIntPoint NewDrawSize((int32)Size.X, (int32)Size.Y);

	if ( NewDrawSize != DrawSize )
	{
		DrawSize = NewDrawSize;
		MarkRenderStateDirty();
		UpdateBodySetup( true );
		RecreatePhysicsState();
	}
}

void UWidgetComponent::RequestRedraw()
{
	bRedrawRequested = true;
}

void UWidgetComponent::SetBlendMode( const EWidgetBlendMode NewBlendMode )
{
	if( NewBlendMode != this->BlendMode )
	{
		this->BlendMode = NewBlendMode;
		if( IsRegistered() )
		{
			MarkRenderStateDirty();
		}
	}
}

void UWidgetComponent::SetTwoSided( const bool bWantTwoSided )
{
	if( bWantTwoSided != this->bIsTwoSided )
	{
		this->bIsTwoSided = bWantTwoSided;
		if( IsRegistered() )
		{
			MarkRenderStateDirty();
		}
	}
}

void UWidgetComponent::SetBackgroundColor( const FLinearColor NewBackgroundColor )
{
	if( NewBackgroundColor != this->BackgroundColor)
	{
		this->BackgroundColor = NewBackgroundColor;
		MarkRenderStateDirty();
	}
}

void UWidgetComponent::SetTintColorAndOpacity( const FLinearColor NewTintColorAndOpacity )
{
	if( NewTintColorAndOpacity != this->TintColorAndOpacity )
	{
		this->TintColorAndOpacity = NewTintColorAndOpacity;
		UpdateMaterialInstanceParameters();
	}
}

void UWidgetComponent::SetOpacityFromTexture( const float NewOpacityFromTexture )
{
	if( NewOpacityFromTexture != this->OpacityFromTexture )
	{
		this->OpacityFromTexture = NewOpacityFromTexture;
		UpdateMaterialInstanceParameters();
	}
}

TSharedPtr< SWindow > UWidgetComponent::GetVirtualWindow() const
{
	return StaticCastSharedPtr<SWindow>(SlateWindow);
}

UMaterialInterface* UWidgetComponent::GetMaterial(int32 MaterialIndex) const
{
	if ( OverrideMaterials.IsValidIndex(MaterialIndex) && ( OverrideMaterials[MaterialIndex] != nullptr ) )
	{
		return OverrideMaterials[MaterialIndex];
	}
	else
	{
		switch ( BlendMode )
		{
		case EWidgetBlendMode::Opaque:
			return bIsTwoSided ? OpaqueMaterial : OpaqueMaterial_OneSided;
			break;
		case EWidgetBlendMode::Masked:
			return bIsTwoSided ? MaskedMaterial : MaskedMaterial_OneSided;
			break;
		case EWidgetBlendMode::Transparent:
			return bIsTwoSided ? TranslucentMaterial : TranslucentMaterial_OneSided;
			break;
		}
	}

	return nullptr;
}

int32 UWidgetComponent::GetNumMaterials() const
{
	return FMath::Max<int32>(OverrideMaterials.Num(), 1);
}

void UWidgetComponent::UpdateMaterialInstanceParameters()
{
	if ( MaterialInstance )
	{
		MaterialInstance->SetTextureParameterValue("SlateUI", RenderTarget);
		MaterialInstance->SetVectorParameterValue("TintColorAndOpacity", TintColorAndOpacity);
		MaterialInstance->SetScalarParameterValue("OpacityFromTexture", OpacityFromTexture);
	}
}

void UWidgetComponent::SetWidgetClass(TSubclassOf<UUserWidget> InWidgetClass)
{
	if (WidgetClass != InWidgetClass)
	{
		WidgetClass = InWidgetClass;

		if(HasBegunPlay())
		{
			if (WidgetClass)
			{
				UUserWidget* NewWidget = CreateWidget<UUserWidget>(GetWorld(), WidgetClass);
				SetWidget(NewWidget);
			}
			else
			{
				SetWidget(nullptr);
			}
		}
	}
}
