// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StaticMeshEditorViewportClient.h"
#include "EngineGlobals.h"
#include "RawIndexBuffer.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "Engine/StaticMesh.h"
#include "Editor.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Canvas.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "Engine/StaticMeshSocket.h"
#include "Utils.h"
#include "IStaticMeshEditor.h"

#include "StaticMeshResources.h"
#include "RawMesh.h"
#include "DistanceFieldAtlas.h"
#include "SEditorViewport.h"
#include "AdvancedPreviewScene.h"
#include "SStaticMeshEditorViewport.h"

#include "Runtime/Analytics/Analytics/Public/Interfaces/IAnalyticsProvider.h"
#include "EngineAnalytics.h"
#include "AI/Navigation/NavCollision.h"
#include "PhysicsEngine/BodySetup.h"

#include "Engine/AssetUserData.h"

#include "Editor/EditorPerProjectUserSettings.h"
#include "AssetViewerSettings.h"

#define LOCTEXT_NAMESPACE "FStaticMeshEditorViewportClient"

#define HITPROXY_SOCKET	1

namespace {
	static const float LightRotSpeed = 0.22f;
	static const float StaticMeshEditor_RotateSpeed = 0.01f;
	static const float	StaticMeshEditor_TranslateSpeed = 0.25f;
	static const float GridSize = 2048.0f;
	static const int32 CellSize = 16;
	static const float AutoViewportOrbitCameraTranslate = 256.0f;

	static float AmbientCubemapIntensity = 0.4f;
}

FStaticMeshEditorViewportClient::FStaticMeshEditorViewportClient(TWeakPtr<IStaticMeshEditor> InStaticMeshEditor, const TSharedRef<SStaticMeshEditorViewport>& InStaticMeshEditorViewport, const TSharedRef<FAdvancedPreviewScene>& InPreviewScene, UStaticMesh* InPreviewStaticMesh, UStaticMeshComponent* InPreviewStaticMeshComponent)
	: FEditorViewportClient(nullptr, &InPreviewScene.Get(), StaticCastSharedRef<SEditorViewport>(InStaticMeshEditorViewport))
	, StaticMeshEditorPtr(InStaticMeshEditor)
	, StaticMeshEditorViewportPtr(InStaticMeshEditorViewport)
{
	SimplygonLogo = LoadObject<UTexture2D>(NULL, TEXT("/Engine/EditorResources/SimplygonLogo.SimplygonLogo"), NULL, LOAD_None, NULL);

	// Setup defaults for the common draw helper.
	DrawHelper.bDrawPivot = false;
	DrawHelper.bDrawWorldBox = false;
	DrawHelper.bDrawKillZ = false;
	DrawHelper.bDrawGrid = true;
	DrawHelper.GridColorAxis = FColor(160,160,160);
	DrawHelper.GridColorMajor = FColor(144,144,144);
	DrawHelper.GridColorMinor = FColor(128,128,128);
	DrawHelper.PerspectiveGridSize = GridSize;
	DrawHelper.NumCells = DrawHelper.PerspectiveGridSize / (CellSize * 2);

	SetViewMode(VMI_Lit);

	WidgetMode = FWidget::WM_None;

	EngineShowFlags.SetSeparateTranslucency(true);
	EngineShowFlags.SetSnap(0);
	EngineShowFlags.SetCompositeEditorPrimitives(true);
	OverrideNearClipPlane(1.0f);
	bUsingOrbitCamera = true;

	bShowSimpleCollision = false;
	bShowComplexCollision = false;
	bShowSockets = true;
	bDrawUVs = false;
	bDrawNormals = false;
	bDrawTangents = false;
	bDrawBinormals = false;
	bShowPivot = false;
	bDrawAdditionalData = true;
	bDrawVertices = false;

	bManipulating = false;

	AdvancedPreviewScene = static_cast<FAdvancedPreviewScene*>(PreviewScene);

	SetPreviewMesh(InPreviewStaticMesh, InPreviewStaticMeshComponent);

	// Register delegate to update the show flags when the post processing is turned on or off
	UAssetViewerSettings::Get()->OnAssetViewerSettingsChanged().AddRaw(this, &FStaticMeshEditorViewportClient::OnAssetViewerSettingsChanged);
	// Set correct flags according to current profile settings
	SetAdvancedShowFlagsForScene(UAssetViewerSettings::Get()->Profiles[GetMutableDefault<UEditorPerProjectUserSettings>()->AssetViewerProfileIndex].bPostProcessingEnabled);
}

FStaticMeshEditorViewportClient::~FStaticMeshEditorViewportClient()
{
	UAssetViewerSettings::Get()->OnAssetViewerSettingsChanged().RemoveAll(this);
}

void FStaticMeshEditorViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	// Tick the preview scene world.
	if (!GIntraFrameDebuggingGameThread)
	{
		PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	}
}

/**
 * A hit proxy class for the wireframe collision geometry
 */
struct HSMECollisionProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	IStaticMeshEditor::FPrimData	PrimData;

	HSMECollisionProxy(const IStaticMeshEditor::FPrimData& InPrimData) :
		HHitProxy(HPP_UI),
		PrimData(InPrimData) {}

	HSMECollisionProxy(EAggCollisionShape::Type InPrimType, int32 InPrimIndex) :
		HHitProxy(HPP_UI),
		PrimData(InPrimType, InPrimIndex) {}
};
IMPLEMENT_HIT_PROXY(HSMECollisionProxy, HHitProxy);

/**
 * A hit proxy class for sockets.
 */
struct HSMESocketProxy : public HHitProxy
{
	DECLARE_HIT_PROXY( );

	int32							SocketIndex;

	HSMESocketProxy(int32 InSocketIndex) :
		HHitProxy( HPP_UI ), 
		SocketIndex( InSocketIndex ) {}
};
IMPLEMENT_HIT_PROXY(HSMESocketProxy, HHitProxy);

/**
 * A hit proxy class for vertices.
 */
struct HSMEVertexProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	uint32		Index;

	HSMEVertexProxy(uint32 InIndex)
		: HHitProxy( HPP_UI )
		, Index( InIndex )
	{}
};
IMPLEMENT_HIT_PROXY(HSMEVertexProxy, HHitProxy);

bool FStaticMeshEditorViewportClient::InputWidgetDelta( FViewport* InViewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale )
{
	bool bHandled = false;
	if (bManipulating)
	{
		if (CurrentAxis != EAxisList::None)
		{
			UStaticMeshSocket* SelectedSocket = StaticMeshEditorPtr.Pin()->GetSelectedSocket();
			if(SelectedSocket)
			{
				UProperty* ChangedProperty = NULL;
				const FWidget::EWidgetMode MoveMode = GetWidgetMode();
				if(MoveMode == FWidget::WM_Rotate)
				{
					ChangedProperty = FindField<UProperty>( UStaticMeshSocket::StaticClass(), "RelativeRotation" );
					SelectedSocket->PreEditChange(ChangedProperty);

					FRotator CurrentRot = SelectedSocket->RelativeRotation;
					FRotator SocketWinding, SocketRotRemainder;
					CurrentRot.GetWindingAndRemainder(SocketWinding, SocketRotRemainder);

					const FQuat ActorQ = SocketRotRemainder.Quaternion();
					const FQuat DeltaQ = Rot.Quaternion();
					const FQuat ResultQ = DeltaQ * ActorQ;
					const FRotator NewSocketRotRem = FRotator( ResultQ );
					FRotator DeltaRot = NewSocketRotRem - SocketRotRemainder;
					DeltaRot.Normalize();

					SelectedSocket->RelativeRotation += DeltaRot;
					SelectedSocket->RelativeRotation = SelectedSocket->RelativeRotation.Clamp();
				}
				else if(MoveMode == FWidget::WM_Translate)
				{
					ChangedProperty = FindField<UProperty>( UStaticMeshSocket::StaticClass(), "RelativeLocation" );
					SelectedSocket->PreEditChange(ChangedProperty);

					//FRotationMatrix SocketRotTM( SelectedSocket->RelativeRotation );
					//FVector SocketMove = SocketRotTM.TransformVector( Drag );

					SelectedSocket->RelativeLocation += Drag;
				}
				if ( ChangedProperty )
				{			
					FPropertyChangedEvent PropertyChangedEvent( ChangedProperty );
					SelectedSocket->PostEditChangeProperty(PropertyChangedEvent);
				}

				StaticMeshEditorPtr.Pin()->GetStaticMesh()->MarkPackageDirty();
			}
			else
			{
				const bool bSelectedPrim = StaticMeshEditorPtr.Pin()->HasSelectedPrims();
				if (bSelectedPrim && CurrentAxis != EAxisList::None)
				{
					const FWidget::EWidgetMode MoveMode = GetWidgetMode();
					if (MoveMode == FWidget::WM_Rotate)
					{
						StaticMeshEditorPtr.Pin()->RotateSelectedPrims(Rot);
					}
					else if (MoveMode == FWidget::WM_Scale)
					{
						StaticMeshEditorPtr.Pin()->ScaleSelectedPrims(Scale);
					}
					else if (MoveMode == FWidget::WM_Translate)
					{
						StaticMeshEditorPtr.Pin()->TranslateSelectedPrims(Drag);
					}

					StaticMeshEditorPtr.Pin()->GetStaticMesh()->MarkPackageDirty();
				}
			}
		}

		Invalidate();		
		bHandled = true;
	}

	return bHandled;
}

void FStaticMeshEditorViewportClient::TrackingStarted( const struct FInputEventState& InInputState, bool bIsDraggingWidget, bool bNudge )
{
	if( !bManipulating && bIsDraggingWidget )
	{
		const UStaticMeshSocket* SelectedSocket = StaticMeshEditorPtr.Pin()->GetSelectedSocket();
		if (SelectedSocket)
		{
			FText TransText;
			if( GetWidgetMode() == FWidget::WM_Rotate )
			{
				TransText = LOCTEXT("FStaticMeshEditorViewportClient_RotateSocket", "Rotate Socket");
			}
			else if (GetWidgetMode() == FWidget::WM_Translate)
			{
				if( InInputState.IsLeftMouseButtonPressed() && (Widget->GetCurrentAxis() & EAxisList::XYZ) )
				{
					const bool bAltDown = InInputState.IsAltButtonPressed();
					if ( bAltDown )
					{
						// Rather than moving/rotating the selected socket, copy it and move the copy instead
						StaticMeshEditorPtr.Pin()->DuplicateSelectedSocket();
					}
				}

				TransText = LOCTEXT("FStaticMeshEditorViewportClient_TranslateSocket", "Translate Socket");
			}

			if (!TransText.IsEmpty())
			{
				GEditor->BeginTransaction(TransText);
			}
		}
		
		const bool bSelectedPrim = StaticMeshEditorPtr.Pin()->HasSelectedPrims();
		if (bSelectedPrim)
		{
			FText TransText;
			if (GetWidgetMode() == FWidget::WM_Rotate)
			{
				TransText = LOCTEXT("FStaticMeshEditorViewportClient_RotateCollision", "Rotate Collision");
			}
			else if (GetWidgetMode() == FWidget::WM_Scale)
			{
				TransText = LOCTEXT("FStaticMeshEditorViewportClient_ScaleCollision", "Scale Collision");
			}
			else if (GetWidgetMode() == FWidget::WM_Translate)
			{
				if (InInputState.IsLeftMouseButtonPressed() && (Widget->GetCurrentAxis() & EAxisList::XYZ))
				{
					const bool bAltDown = InInputState.IsAltButtonPressed();
					if (bAltDown)
					{
						// Rather than moving/rotating the selected primitives, copy them and move the copies instead
						StaticMeshEditorPtr.Pin()->DuplicateSelectedPrims(NULL);
					}
				}

				TransText = LOCTEXT("FStaticMeshEditorViewportClient_TranslateCollision", "Translate Collision");
			}
			if (!TransText.IsEmpty())
			{
				GEditor->BeginTransaction(TransText);
				if (StaticMesh->BodySetup)
				{
					StaticMesh->BodySetup->Modify();
				}
			}
		}

		bManipulating = true;
	}

}

FWidget::EWidgetMode FStaticMeshEditorViewportClient::GetWidgetMode() const
{
	const UStaticMeshSocket* SelectedSocket = StaticMeshEditorPtr.Pin()->GetSelectedSocket();
	if( SelectedSocket )
	{
		return WidgetMode;
	}

	const bool bSelectedPrim = StaticMeshEditorPtr.Pin()->HasSelectedPrims();
	if (bSelectedPrim)
	{
		return WidgetMode;
	}

	return FWidget::WM_None;
}

void FStaticMeshEditorViewportClient::SetWidgetMode(FWidget::EWidgetMode NewMode)
{
	WidgetMode = NewMode;
	Invalidate();
}

bool FStaticMeshEditorViewportClient::CanSetWidgetMode(FWidget::EWidgetMode NewMode) const
{
	if (!Widget->IsDragging())
	{
		const bool bSelectedPrim = StaticMeshEditorPtr.Pin()->HasSelectedPrims();
		if (bSelectedPrim)
		{
			return true;
		}
		else if (NewMode != FWidget::WM_Scale)	// Sockets don't support scaling
		{
			const UStaticMeshSocket* SelectedSocket = StaticMeshEditorPtr.Pin()->GetSelectedSocket();
			if (SelectedSocket)
			{
				return true;
			}
		}
	}
	return false;
}

bool FStaticMeshEditorViewportClient::CanCycleWidgetMode() const
{
	if (!Widget->IsDragging())
	{
		const UStaticMeshSocket* SelectedSocket = StaticMeshEditorPtr.Pin()->GetSelectedSocket();
		const bool bSelectedPrim = StaticMeshEditorPtr.Pin()->HasSelectedPrims();
		if ((SelectedSocket || bSelectedPrim))
		{
			return true;
		}
	}
	return false;
}

void FStaticMeshEditorViewportClient::TrackingStopped()
{
	if( bManipulating )
	{
		bManipulating = false;
		GEditor->EndTransaction();
	}
}

FVector FStaticMeshEditorViewportClient::GetWidgetLocation() const
{
	const UStaticMeshSocket* SelectedSocket = StaticMeshEditorPtr.Pin()->GetSelectedSocket();
	if( SelectedSocket )
	{
		FMatrix SocketTM;
		SelectedSocket->GetSocketMatrix(SocketTM, StaticMeshComponent);

		return SocketTM.GetOrigin();
	}

	FTransform PrimTransform = FTransform::Identity;
	const bool bSelectedPrim = StaticMeshEditorPtr.Pin()->GetLastSelectedPrimTransform(PrimTransform);
	if (bSelectedPrim)
	{
		return PrimTransform.GetLocation();
	}

	return FVector::ZeroVector;
}

FMatrix FStaticMeshEditorViewportClient::GetWidgetCoordSystem() const 
{
	const UStaticMeshSocket* SelectedSocket = StaticMeshEditorPtr.Pin()->GetSelectedSocket();
	if( SelectedSocket )
	{
		//FMatrix SocketTM;
		//SelectedSocket->GetSocketMatrix(SocketTM, StaticMeshComponent);

		return FRotationMatrix( SelectedSocket->RelativeRotation );
	}

	FTransform PrimTransform = FTransform::Identity;
	const bool bSelectedPrim = StaticMeshEditorPtr.Pin()->GetLastSelectedPrimTransform(PrimTransform);
	if (bSelectedPrim)
	{
		return FRotationMatrix(PrimTransform.Rotator());
	}

	return FMatrix::Identity;
}

bool FStaticMeshEditorViewportClient::ShouldOrbitCamera() const
{
	if (GetDefault<ULevelEditorViewportSettings>()->bUseUE3OrbitControls)
	{
		// this editor orbits always if ue3 orbit controls are enabled
		return true;
	}

	return FEditorViewportClient::ShouldOrbitCamera();
}

void FStaticMeshEditorViewportClient::Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	FEditorViewportClient::Draw(View, PDI);

	TSharedPtr<IStaticMeshEditor> StaticMeshEditor = StaticMeshEditorPtr.Pin();

	if(!StaticMesh->RenderData->LODResources.IsValidIndex(StaticMeshEditor->GetCurrentLODIndex()))
	{
		// Guard against corrupted meshes
		return;
	}

	// Draw simple shapes if we are showing simple, or showing complex but using simple as complex
	if (StaticMesh->BodySetup && (bShowSimpleCollision || (bShowComplexCollision && StaticMesh->BodySetup->CollisionTraceFlag == ECollisionTraceFlag::CTF_UseSimpleAsComplex)))
	{
		// Ensure physics mesh is created before we try and draw it
		StaticMesh->BodySetup->CreatePhysicsMeshes();

		const FColor SelectedColor(20, 220, 20);
		const FColor UnselectedColor(0, 125, 0);

		const FVector VectorScaleOne(1.0f);

		// Draw bodies
		FKAggregateGeom* AggGeom = &StaticMesh->BodySetup->AggGeom;

		for (int32 i = 0; i < AggGeom->SphereElems.Num(); ++i)
		{
			HSMECollisionProxy* HitProxy = new HSMECollisionProxy(EAggCollisionShape::Sphere, i);
			PDI->SetHitProxy(HitProxy);

			const FColor CollisionColor = StaticMeshEditor->IsSelectedPrim(HitProxy->PrimData) ? SelectedColor : UnselectedColor;
			const FKSphereElem& SphereElem = AggGeom->SphereElems[i];
			const FTransform ElemTM = SphereElem.GetTransform();
			SphereElem.DrawElemWire(PDI, ElemTM, VectorScaleOne, CollisionColor);

			PDI->SetHitProxy(NULL);
		}

		for (int32 i = 0; i < AggGeom->BoxElems.Num(); ++i)
		{
			HSMECollisionProxy* HitProxy = new HSMECollisionProxy(EAggCollisionShape::Box, i);
			PDI->SetHitProxy(HitProxy);

			const FColor CollisionColor = StaticMeshEditor->IsSelectedPrim(HitProxy->PrimData) ? SelectedColor : UnselectedColor;
			const FKBoxElem& BoxElem = AggGeom->BoxElems[i];
			const FTransform ElemTM = BoxElem.GetTransform();
			BoxElem.DrawElemWire(PDI, ElemTM, VectorScaleOne, CollisionColor);

			PDI->SetHitProxy(NULL);
		}

		for (int32 i = 0; i < AggGeom->SphylElems.Num(); ++i)
		{
			HSMECollisionProxy* HitProxy = new HSMECollisionProxy(EAggCollisionShape::Sphyl, i);
			PDI->SetHitProxy(HitProxy);

			const FColor CollisionColor = StaticMeshEditor->IsSelectedPrim(HitProxy->PrimData) ? SelectedColor : UnselectedColor;
			const FKSphylElem& SphylElem = AggGeom->SphylElems[i];
			const FTransform ElemTM = SphylElem.GetTransform();
			SphylElem.DrawElemWire(PDI, ElemTM, VectorScaleOne, CollisionColor);

			PDI->SetHitProxy(NULL);
		}

		for (int32 i = 0; i < AggGeom->ConvexElems.Num(); ++i)
		{
			HSMECollisionProxy* HitProxy = new HSMECollisionProxy(EAggCollisionShape::Convex, i);
			PDI->SetHitProxy(HitProxy);

			const FColor CollisionColor = StaticMeshEditor->IsSelectedPrim(HitProxy->PrimData) ? SelectedColor : UnselectedColor;
			const FKConvexElem& ConvexElem = AggGeom->ConvexElems[i];
			const FTransform ElemTM = ConvexElem.GetTransform();
			ConvexElem.DrawElemWire(PDI, ElemTM, 1.f, CollisionColor);

			PDI->SetHitProxy(NULL);
		}
	}

	if( bShowSockets )
	{
		const FColor SocketColor = FColor(255, 128, 128);

		for(int32 i=0; i < StaticMesh->Sockets.Num(); i++)
		{
			UStaticMeshSocket* Socket = StaticMesh->Sockets[i];
			if(Socket)
			{
				FMatrix SocketTM;
				Socket->GetSocketMatrix(SocketTM, StaticMeshComponent);
				PDI->SetHitProxy( new HSMESocketProxy(i) );
				DrawWireDiamond(PDI, SocketTM, 5.f, SocketColor, SDPG_Foreground);
				PDI->SetHitProxy( NULL );
			}
		}
	}

	// Draw any edges that are currently selected by the user
	if( SelectedEdgeIndices.Num() > 0 )
	{
		for(int32 VertexIndex = 0; VertexIndex < SelectedEdgeVertices.Num(); VertexIndex += 2)
		{
			FVector EdgeVertices[ 2 ];
			EdgeVertices[ 0 ] = SelectedEdgeVertices[VertexIndex];
			EdgeVertices[ 1 ] = SelectedEdgeVertices[VertexIndex + 1];

			PDI->DrawLine(
				StaticMeshComponent->GetComponentTransform().TransformPosition( EdgeVertices[ 0 ] ),
				StaticMeshComponent->GetComponentTransform().TransformPosition( EdgeVertices[ 1 ] ),
				FColor( 255, 255, 0 ),
				SDPG_World );
		}
	}


	if( bDrawNormals || bDrawTangents || bDrawBinormals || bDrawVertices )
	{
		FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[StaticMeshEditor->GetCurrentLODIndex()];
		FIndexArrayView Indices = LODModel.IndexBuffer.GetArrayView();
		uint32 NumIndices = Indices.Num();

		FMatrix LocalToWorldInverseTranspose = StaticMeshComponent->GetComponentTransform().ToMatrixWithScale().InverseFast().GetTransposed();
		for (uint32 i = 0; i < NumIndices; i++)
		{
			const FVector& VertexPos = LODModel.PositionVertexBuffer.VertexPosition( Indices[i] );

			const FVector WorldPos = StaticMeshComponent->GetComponentTransform().TransformPosition( VertexPos );
			const FVector& Normal = LODModel.VertexBuffer.VertexTangentZ( Indices[i] ); 
			const FVector& Binormal = LODModel.VertexBuffer.VertexTangentY( Indices[i] ); 
			const FVector& Tangent = LODModel.VertexBuffer.VertexTangentX( Indices[i] ); 

			const float Len = 5.0f;
			const float BoxLen = 2.0f;
			const FVector Box(BoxLen);

			if( bDrawNormals )
			{
				PDI->DrawLine( WorldPos, WorldPos+LocalToWorldInverseTranspose.TransformVector( Normal ).GetSafeNormal() * Len, FLinearColor( 0.0f, 1.0f, 0.0f), SDPG_World );
			}

			if( bDrawTangents )
			{
				PDI->DrawLine( WorldPos, WorldPos+LocalToWorldInverseTranspose.TransformVector( Tangent ).GetSafeNormal() * Len, FLinearColor( 1.0f, 0.0f, 0.0f), SDPG_World );
			}

			if( bDrawBinormals )
			{
				PDI->DrawLine( WorldPos, WorldPos+LocalToWorldInverseTranspose.TransformVector( Binormal ).GetSafeNormal() * Len, FLinearColor( 0.0f, 0.0f, 1.0f), SDPG_World );
			}

			if( bDrawVertices )
			{								
				PDI->SetHitProxy(new HSMEVertexProxy(i));
				DrawWireBox( PDI, FBox(VertexPos - Box, VertexPos + Box), FLinearColor(0.0f, 1.0f, 0.0f), SDPG_World );
				PDI->SetHitProxy(NULL);								
			}
		}	
	}


	if( bShowPivot )
	{
		FUnrealEdUtils::DrawWidget(View, PDI, StaticMeshComponent->GetComponentTransform().ToMatrixWithScale(), 0, 0, EAxisList::All, EWidgetMovementMode::WMM_Translate, false);
	}

	if( bDrawAdditionalData )
	{
		const TArray<UAssetUserData*>* UserDataArray = StaticMesh->GetAssetUserDataArray();
		if (UserDataArray != NULL)
		{
			for (int32 AdditionalDataIndex = 0; AdditionalDataIndex < UserDataArray->Num(); ++AdditionalDataIndex)
			{
				if ((*UserDataArray)[AdditionalDataIndex] != NULL)
				{
					(*UserDataArray)[AdditionalDataIndex]->Draw(PDI, View);
				}
			}
		}

		// The simple nav geometry is only used by dynamic obstacles for now
		if (StaticMesh->NavCollision && StaticMesh->NavCollision->bIsDynamicObstacle)
		{
			// Draw the static mesh's body setup (simple collision)
			FTransform GeomTransform(StaticMeshComponent->GetComponentTransform());
			FColor NavCollisionColor = FColor(118, 84, 255, 255);
			StaticMesh->NavCollision->DrawSimpleGeom(PDI, GeomTransform, FColorList::LimeGreen);
		}
	}
}

static void DrawAngles(FCanvas* Canvas, int32 XPos, int32 YPos, EAxisList::Type ManipAxis, FWidget::EWidgetMode MoveMode, const FRotator& Rotation, const FVector& Translation)
{
	FString OutputString(TEXT(""));
	if (MoveMode == FWidget::WM_Rotate && Rotation.IsZero() == false)
	{
		//Only one value moves at a time
		const FVector EulerAngles = Rotation.Euler();
		if (ManipAxis == EAxisList::X)
		{
			OutputString += FString::Printf(TEXT("Roll: %0.2f"), EulerAngles.X);
		}
		else if (ManipAxis == EAxisList::Y)
		{
			OutputString += FString::Printf(TEXT("Pitch: %0.2f"), EulerAngles.Y);
		}
		else if (ManipAxis == EAxisList::Z)
		{
			OutputString += FString::Printf(TEXT("Yaw: %0.2f"), EulerAngles.Z);
		}
	}
	else if (MoveMode == FWidget::WM_Translate && Translation.IsZero() == false)
	{
		//Only one value moves at a time
		if (ManipAxis == EAxisList::X)
		{
			OutputString += FString::Printf(TEXT(" %0.2f"), Translation.X);
		}
		else if (ManipAxis == EAxisList::Y)
		{
			OutputString += FString::Printf(TEXT(" %0.2f"), Translation.Y);
		}
		else if (ManipAxis == EAxisList::Z)
		{
			OutputString += FString::Printf(TEXT(" %0.2f"), Translation.Z);
		}
	}

	if (OutputString.Len() > 0)
	{
		FCanvasTextItem TextItem( FVector2D(XPos, YPos), FText::FromString( OutputString ), GEngine->GetSmallFont(), FLinearColor::White );
		Canvas->DrawItem( TextItem );
	}
}

void FStaticMeshEditorViewportClient::DrawCanvas( FViewport& InViewport, FSceneView& View, FCanvas& Canvas )
{
#ifdef TODO_STATICMESH
	if ( StaticMesh->bHasBeenSimplified && SimplygonLogo && SimplygonLogo->Resource )
	{
		const float LogoSizeX = 64.0f;
		const float LogoSizeY = 40.65f;
		const float Padding = 6.0f;
		const float LogoX = Viewport->GetSizeXY().X - Padding - LogoSizeX;
		const float LogoY = Viewport->GetSizeXY().Y - Padding - LogoSizeY;

		Canvas->DrawTile(
			LogoX,
			LogoY,
			LogoSizeX,
			LogoSizeY,
			0.0f,
			0.0f,
			1.0f,
			1.0f,
			FLinearColor::White,
			SimplygonLogo->Resource,
			SE_BLEND_Opaque );
	}
#endif // #if TODO_STATICMESH

	auto StaticMeshEditor = StaticMeshEditorPtr.Pin();
	auto StaticMeshEditorViewport = StaticMeshEditorViewportPtr.Pin();
	if (!StaticMeshEditor.IsValid() || !StaticMeshEditorViewport.IsValid())
	{
		return;
	}

	const int32 HalfX = Viewport->GetSizeXY().X/2;
	const int32 HalfY = Viewport->GetSizeXY().Y/2;

	// Draw socket names if desired.
	if( bShowSockets )
	{
		for(int32 i=0; i<StaticMesh->Sockets.Num(); i++)
		{
			UStaticMeshSocket* Socket = StaticMesh->Sockets[i];
			if(Socket!=NULL)
			{
				FMatrix SocketTM;
				Socket->GetSocketMatrix(SocketTM, StaticMeshComponent);
				const FVector SocketPos	= SocketTM.GetOrigin();
				const FPlane proj		= View.Project( SocketPos );
				if(proj.W > 0.f)
				{
					const int32 XPos = HalfX + ( HalfX * proj.X );
					const int32 YPos = HalfY + ( HalfY * (proj.Y * -1) );

					FCanvasTextItem TextItem( FVector2D( XPos, YPos ), FText::FromString( Socket->SocketName.ToString() ), GEngine->GetSmallFont(), FLinearColor(FColor(255,196,196)) );
					Canvas.DrawItem( TextItem );	

					const UStaticMeshSocket* SelectedSocket = StaticMeshEditor->GetSelectedSocket();
					if (bManipulating && SelectedSocket == Socket)
					{
						//Figure out the text height
						FTextSizingParameters Parameters(GEngine->GetSmallFont(), 1.0f, 1.0f);
						UCanvas::CanvasStringSize(Parameters, *Socket->SocketName.ToString());
						int32 YL = FMath::TruncToInt(Parameters.DrawYL);

						DrawAngles(&Canvas, XPos, YPos + YL, 
							Widget->GetCurrentAxis(), 
							GetWidgetMode(),
							Socket->RelativeRotation,
							Socket->RelativeLocation);
					}
				}
			}
		}
	}

	TArray<SStaticMeshEditorViewport::FOverlayTextItem> TextItems;

	int32 CurrentLODLevel = StaticMeshEditor->GetCurrentLODLevel();
	if (CurrentLODLevel == 0)
	{
		CurrentLODLevel = ComputeStaticMeshLOD(StaticMesh->RenderData.Get(), StaticMeshComponent->Bounds.Origin, StaticMeshComponent->Bounds.SphereRadius, View, StaticMesh->MinLOD);
	}
	else
	{
		CurrentLODLevel -= 1;
	}

	TextItems.Add(SStaticMeshEditorViewport::FOverlayTextItem(
		FText::Format(NSLOCTEXT("UnrealEd", "LOD_F", "LOD:  {0}"), FText::AsNumber(CurrentLODLevel))));

	float CurrentScreenSize = ComputeBoundsScreenSize(StaticMeshComponent->Bounds.Origin, StaticMeshComponent->Bounds.SphereRadius, View);
	FNumberFormattingOptions FormatOptions;
	FormatOptions.MinimumFractionalDigits = 3;
	FormatOptions.MaximumFractionalDigits = 6;
	FormatOptions.MaximumIntegralDigits = 6;
	TextItems.Add(SStaticMeshEditorViewport::FOverlayTextItem(
		FText::Format(NSLOCTEXT("UnrealEd", "ScreenSize_F", "Current Screen Size:  {0}"), FText::AsNumber(CurrentScreenSize, &FormatOptions))));

	TextItems.Add(SStaticMeshEditorViewport::FOverlayTextItem(
		FText::Format(NSLOCTEXT("UnrealEd", "Triangles_F", "Triangles:  {0}"), FText::AsNumber(StaticMeshEditorPtr.Pin()->GetNumTriangles(CurrentLODLevel)))));

	TextItems.Add(SStaticMeshEditorViewport::FOverlayTextItem(
		FText::Format(NSLOCTEXT("UnrealEd", "Vertices_F", "Vertices:  {0}"), FText::AsNumber(StaticMeshEditorPtr.Pin()->GetNumVertices(CurrentLODLevel)))));

	TextItems.Add(SStaticMeshEditorViewport::FOverlayTextItem(
		FText::Format(NSLOCTEXT("UnrealEd", "UVChannels_F", "UV Channels:  {0}"), FText::AsNumber(StaticMeshEditorPtr.Pin()->GetNumUVChannels(CurrentLODLevel)))));

	if( StaticMesh->RenderData->LODResources.Num() > 0 )
	{
		if (StaticMesh->RenderData->LODResources[0].DistanceFieldData != nullptr )
		{
			const FDistanceFieldVolumeData& VolumeData = *(StaticMesh->RenderData->LODResources[0].DistanceFieldData);

			if (VolumeData.Size.GetMax() > 0)
			{
				static const auto CVarEightBit = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DistanceFieldBuild.EightBit"));
				const bool bEightBitFixedPoint = CVarEightBit->GetValueOnAnyThread() != 0;
				const int32 FormatSize = GPixelFormats[bEightBitFixedPoint ? PF_G8 : PF_R16F].BlockBytes;

				float MemoryMb = (VolumeData.Size.X * VolumeData.Size.Y * VolumeData.Size.Z * FormatSize + VolumeData.CompressedDistanceFieldVolume.Num() * VolumeData.CompressedDistanceFieldVolume.GetTypeSize()) / (1024.0f * 1024.0f);

				FNumberFormattingOptions NumberOptions;
				NumberOptions.MinimumFractionalDigits = 2;
				NumberOptions.MaximumFractionalDigits = 2;

				if (VolumeData.bMeshWasClosed)
				{
					TextItems.Add(SStaticMeshEditorViewport::FOverlayTextItem(
						FText::Format(NSLOCTEXT("UnrealEd", "DistanceFieldRes_F", "Distance Field:  {0}x{1}x{2} = {3}Mb"), FText::AsNumber(VolumeData.Size.X), FText::AsNumber(VolumeData.Size.Y), FText::AsNumber(VolumeData.Size.Z), FText::AsNumber(MemoryMb, &NumberOptions))));
				}
				else
				{
					TextItems.Add(SStaticMeshEditorViewport::FOverlayTextItem(
						NSLOCTEXT("UnrealEd", "DistanceFieldClosed_F", "Distance Field:  Mesh was not closed and material was one-sided")));
				}
			}
		}
	}

	TextItems.Add(SStaticMeshEditorViewport::FOverlayTextItem(
		FText::Format(NSLOCTEXT("UnrealEd", "ApproxSize_F", "Approx Size: {0}x{1}x{2}"),
		FText::AsNumber(int32(StaticMesh->GetBounds().BoxExtent.X * 2.0f)), // x2 as artists wanted length not radius
		FText::AsNumber(int32(StaticMesh->GetBounds().BoxExtent.Y * 2.0f)),
		FText::AsNumber(int32(StaticMesh->GetBounds().BoxExtent.Z * 2.0f)))));

	// Show the number of collision primitives
	if(StaticMesh->BodySetup)
	{
		TextItems.Add(SStaticMeshEditorViewport::FOverlayTextItem(
			FText::Format(NSLOCTEXT("UnrealEd", "NumPrimitives_F", "Num Collision Primitives:  {0}"), FText::AsNumber(StaticMesh->BodySetup->AggGeom.GetElementCount()))));
	}

	if (StaticMeshComponent && StaticMeshComponent->SectionIndexPreview != INDEX_NONE)
	{
		TextItems.Add(SStaticMeshEditorViewport::FOverlayTextItem(NSLOCTEXT("UnrealEd", "MeshSectionsHiddenWarning",  "Mesh Sections Hidden")));
	}

	if (StaticMesh->FlexAsset)
	{
		TextItems.Add(SStaticMeshEditorViewport::FOverlayTextItem(
			FText::Format(FText::FromString(TEXT("Flex Num Particles: {0}")), FText::AsNumber(StaticMesh->FlexAsset->Particles.Num()))));

		TextItems.Add(SStaticMeshEditorViewport::FOverlayTextItem(
			FText::Format(FText::FromString(TEXT("Flex Num Shapes: {0}")), FText::AsNumber(StaticMesh->FlexAsset->ShapeCenters.Num()))));

		TextItems.Add(SStaticMeshEditorViewport::FOverlayTextItem(
			FText::Format(FText::FromString(TEXT("Flex Num Springs: {0}")), FText::AsNumber(StaticMesh->FlexAsset->SpringCoefficients.Num()))));
	}

	StaticMeshEditorViewport->PopulateOverlayText(TextItems);

	if(bDrawUVs && StaticMesh->RenderData->LODResources.Num() > 0)
	{
		const int32 YPos = 160;
		DrawUVsForMesh(Viewport, &Canvas, YPos);
	}
}

void FStaticMeshEditorViewportClient::DrawUVsForMesh(FViewport* InViewport, FCanvas* InCanvas, int32 InTextYPos )
{
	//use the overridden LOD level
	const uint32 LODLevel = FMath::Clamp(StaticMeshComponent->ForcedLodModel - 1, 0, StaticMesh->RenderData->LODResources.Num() - 1);

	int32 UVChannel = StaticMeshEditorPtr.Pin()->GetCurrentUVChannel();

	DrawUVs(InViewport, InCanvas, InTextYPos, LODLevel, UVChannel, SelectedEdgeTexCoords[UVChannel], StaticMeshComponent->GetStaticMesh()->RenderData.Get(), NULL);
}

void FStaticMeshEditorViewportClient::MouseMove(FViewport* InViewport,int32 x, int32 y)
{
	FEditorViewportClient::MouseMove(InViewport,x,y);
}

bool FStaticMeshEditorViewportClient::InputKey(FViewport* InViewport, int32 ControllerId, FKey Key, EInputEvent Event,float AmountDepressed, bool Gamepad)
{
	bool bHandled = FEditorViewportClient::InputKey(InViewport, ControllerId, Key, Event, AmountDepressed, false);

	// Handle viewport screenshot.
	bHandled |= InputTakeScreenshot( InViewport, Key, Event );

	bHandled |= AdvancedPreviewScene->HandleInputKey(InViewport, ControllerId, Key, Event, AmountDepressed, Gamepad);

	return bHandled;
}

bool FStaticMeshEditorViewportClient::InputAxis(FViewport* InViewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad)
{
	bool bResult = true;
	
	if (!bDisableInput)
	{
		bResult = AdvancedPreviewScene->HandleViewportInput(InViewport, ControllerId, Key, Delta, DeltaTime, NumSamples, bGamepad);
		if (bResult)
		{
			Invalidate();
		}
		else
		{
			bResult = FEditorViewportClient::InputAxis(InViewport, ControllerId, Key, Delta, DeltaTime, NumSamples, bGamepad);
		}
	}

	return bResult;
}

void FStaticMeshEditorViewportClient::ProcessClick(class FSceneView& InView, class HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	const bool bCtrlDown = Viewport->KeyState(EKeys::LeftControl) || Viewport->KeyState(EKeys::RightControl);

	bool ClearSelectedSockets = true;
	bool ClearSelectedPrims = true;
	bool ClearSelectedEdges = true;

	if( HitProxy )
	{
		if(HitProxy->IsA( HSMESocketProxy::StaticGetType() ) )
		{
			HSMESocketProxy* SocketProxy = (HSMESocketProxy*)HitProxy;

			UStaticMeshSocket* Socket = NULL;

			if(SocketProxy->SocketIndex < StaticMesh->Sockets.Num())
			{
				Socket = StaticMesh->Sockets[SocketProxy->SocketIndex];
			}

			if(Socket)
			{
				StaticMeshEditorPtr.Pin()->SetSelectedSocket(Socket);
			}

			ClearSelectedSockets = false;
		}
		else if (HitProxy->IsA(HSMECollisionProxy::StaticGetType()) && StaticMesh->BodySetup)
		{
			HSMECollisionProxy* CollisionProxy = (HSMECollisionProxy*)HitProxy;			

			if (StaticMeshEditorPtr.Pin()->IsSelectedPrim(CollisionProxy->PrimData))
			{
				if (!bCtrlDown)
				{
					StaticMeshEditorPtr.Pin()->AddSelectedPrim(CollisionProxy->PrimData, true);
				}
				else
				{
					StaticMeshEditorPtr.Pin()->RemoveSelectedPrim(CollisionProxy->PrimData);
				}
			}
			else
			{
				StaticMeshEditorPtr.Pin()->AddSelectedPrim(CollisionProxy->PrimData, !bCtrlDown);
			}

			// Force the widget to translate, if not already set
			if (WidgetMode == FWidget::WM_None)
			{
				WidgetMode = FWidget::WM_Translate;
			}

			ClearSelectedPrims = false;
		}
		else if (IsShowSocketsChecked() && HitProxy->IsA(HSMEVertexProxy::StaticGetType()))
		{
			UStaticMeshSocket* Socket = StaticMeshEditorPtr.Pin()->GetSelectedSocket();

			if (Socket)
			{
				HSMEVertexProxy* VertexProxy = (HSMEVertexProxy*)HitProxy;
				TSharedPtr<IStaticMeshEditor> StaticMeshEditor = StaticMeshEditorPtr.Pin();
				if (StaticMeshEditor.IsValid())
				{
					FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[StaticMeshEditor->GetCurrentLODIndex()];
					FIndexArrayView Indices = LODModel.IndexBuffer.GetArrayView();
					const uint32 Index = Indices[VertexProxy->Index];

					Socket->RelativeLocation = LODModel.PositionVertexBuffer.VertexPosition(Index);
					Socket->RelativeRotation = FRotationMatrix::MakeFromYZ(LODModel.VertexBuffer.VertexTangentZ(Index), LODModel.VertexBuffer.VertexTangentX(Index)).Rotator();

					ClearSelectedSockets = false;
				}
			}
		}
	}
	else
	{
		const bool bShiftDown = Viewport->KeyState(EKeys::LeftShift) || Viewport->KeyState(EKeys::RightShift);

		if(!bCtrlDown && !bShiftDown)
		{
			SelectedEdgeIndices.Empty();
		}

		// Check to see if we clicked on a mesh edge
		if( StaticMeshComponent != NULL && Viewport->GetSizeXY().X > 0 && Viewport->GetSizeXY().Y > 0 )
		{
			FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues( Viewport, GetScene(), EngineShowFlags ));
			FSceneView* View = CalcSceneView(&ViewFamily);
			FViewportClick ViewportClick(View, this, Key, Event, HitX, HitY);

			const FVector ClickLineStart( ViewportClick.GetOrigin() );
			const FVector ClickLineEnd( ViewportClick.GetOrigin() + ViewportClick.GetDirection() * HALF_WORLD_MAX );

			// Don't bother doing a line check as there is only one mesh in the SME and it makes fuzzy selection difficult
			// 	FHitResult CheckResult( 1.0f );
			// 	if( StaticMeshComponent->LineCheck(
			// 			CheckResult,	// In/Out: Result
			// 			ClickLineEnd,	// Target
			// 			ClickLineStart,	// Source
			// 			FVector::ZeroVector,	// Extend
			// 			TRACE_ComplexCollision ) )	// Trace flags
			{
				// @todo: Should be in screen space ideally
				const float WorldSpaceMinClickDistance = 100.0f;

				float ClosestEdgeDistance = FLT_MAX;
				TArray< int32 > ClosestEdgeIndices;
				FVector ClosestEdgeVertices[ 2 ];

				const uint32 LODLevel = FMath::Clamp( StaticMeshComponent->ForcedLodModel - 1, 0, StaticMeshComponent->GetStaticMesh()->GetNumLODs() - 1 );
				FRawMesh RawMesh;
				StaticMeshComponent->GetStaticMesh()->SourceModels[LODLevel].RawMeshBulkData->LoadRawMesh(RawMesh);

				const int32 RawEdgeCount = RawMesh.WedgeIndices.Num() - 1; 
				const int32 NumFaces = RawMesh.WedgeIndices.Num() / 3;
				int32 NumBackFacingTriangles = 0;
				for(int32 FaceIndex = 0; FaceIndex < NumFaces; ++FaceIndex)
				{
					// We disable edge selection where all adjoining triangles are back face culled and the 
					// material is not two-sided. This prevents edges that are back-face culled from being selected.
					bool bIsBackFacing = false;
					bool bIsTwoSided = false;
					UMaterialInterface* Material = StaticMeshComponent->GetMaterial(RawMesh.FaceMaterialIndices[FaceIndex]);
					if (Material && Material->GetMaterial())
					{
						bIsTwoSided = Material->IsTwoSided();
					}
					if(!bIsTwoSided)
					{
						// Check whether triangle if back facing 
						const FVector A = RawMesh.GetWedgePosition( FaceIndex * 3);
						const FVector B = RawMesh.GetWedgePosition( FaceIndex * 3 + 1);
						const FVector C = RawMesh.GetWedgePosition( FaceIndex * 3 + 2);
								
						// Compute the per-triangle normal
						const FVector BA = A - B;
						const FVector CA = A - C;
						const FVector TriangleNormal = (CA ^ BA).GetSafeNormal();

						// Transform the view position from world to component space
						const FVector ComponentSpaceViewOrigin = StaticMeshComponent->GetComponentTransform().InverseTransformPosition( View->ViewMatrices.GetViewOrigin());
								
						// Determine which side of the triangle's plane that the view position lies on.
						bIsBackFacing = (FVector::PointPlaneDist( ComponentSpaceViewOrigin,  A, TriangleNormal)  < 0.0f);
					}
						
					for( int32 VertIndex = 0; VertIndex < 3; ++VertIndex )
					{
						const int32 EdgeIndex = FaceIndex * 3 + VertIndex;
						const int32 EdgeIndex2 = FaceIndex * 3 + ((VertIndex + 1) % 3);

						FVector EdgeVertices[ 2 ];
						EdgeVertices[0]	= RawMesh.GetWedgePosition(EdgeIndex);
						EdgeVertices[1] = RawMesh.GetWedgePosition(EdgeIndex2);

						// First check to see if this edge is already in our "closest to click" list.
						// Most edges are shared by two faces in our raw triangle data set, so we want
						// to select (or deselect) both of these edges that the user clicks on (what
						// appears to be) a single edge
						if( ClosestEdgeIndices.Num() > 0 &&
							( ( EdgeVertices[ 0 ].Equals( ClosestEdgeVertices[ 0 ] ) && EdgeVertices[ 1 ].Equals( ClosestEdgeVertices[ 1 ] ) ) ||
							( EdgeVertices[ 0 ].Equals( ClosestEdgeVertices[ 1 ] ) && EdgeVertices[ 1 ].Equals( ClosestEdgeVertices[ 0 ] ) ) ) )
						{
							// Edge overlaps the closest edge we have so far, so just add it to the list
							ClosestEdgeIndices.Add( EdgeIndex );
							// Increment the number of back facing triangles if the adjoining triangle 
							// is back facing and isn't two-sided
							if(bIsBackFacing && !bIsTwoSided)
							{
								++NumBackFacingTriangles;
							}
						}
						else
						{
							FVector WorldSpaceEdgeStart( StaticMeshComponent->GetComponentTransform().TransformPosition( EdgeVertices[ 0 ] ) );
							FVector WorldSpaceEdgeEnd( StaticMeshComponent->GetComponentTransform().TransformPosition( EdgeVertices[ 1 ] ) );

							// Determine the mesh edge that's closest to the ray cast through the eye towards the click location
							FVector ClosestPointToEdgeOnClickLine;
							FVector ClosestPointToClickLineOnEdge;
							FMath::SegmentDistToSegment(
								ClickLineStart,
								ClickLineEnd,
								WorldSpaceEdgeStart,
								WorldSpaceEdgeEnd,
								ClosestPointToEdgeOnClickLine,
								ClosestPointToClickLineOnEdge );

							// Compute the minimum distance (squared)
							const float MinDistanceToEdgeSquared = ( ClosestPointToClickLineOnEdge - ClosestPointToEdgeOnClickLine ).SizeSquared();

							if( MinDistanceToEdgeSquared <= WorldSpaceMinClickDistance )
							{
								if( MinDistanceToEdgeSquared <= ClosestEdgeDistance )
								{
									// This is the closest edge to the click line that we've found so far!
									ClosestEdgeDistance = MinDistanceToEdgeSquared;
									ClosestEdgeVertices[ 0 ] = EdgeVertices[ 0 ];
									ClosestEdgeVertices[ 1 ] = EdgeVertices[ 1 ];

									ClosestEdgeIndices.Reset();
									ClosestEdgeIndices.Add( EdgeIndex );

									// Reset the number of back facing triangles.
									NumBackFacingTriangles = (bIsBackFacing && !bIsTwoSided) ? 1 : 0;
								}
							}
						}
					}
				}

				// Did the user click on an edge? Edges must also have at least one adjoining triangle 
				// which isn't back face culled (for one-sided materials)
				if( ClosestEdgeIndices.Num() > 0 && ClosestEdgeIndices.Num() > NumBackFacingTriangles)
				{
					for( int32 CurIndex = 0; CurIndex < ClosestEdgeIndices.Num(); ++CurIndex )
					{
						const int32 CurEdgeIndex = ClosestEdgeIndices[ CurIndex ];

						if( bCtrlDown )
						{
							// Toggle selection
							if( SelectedEdgeIndices.Contains( CurEdgeIndex ) )
							{
								SelectedEdgeIndices.Remove( CurEdgeIndex );
							}
							else
							{
								SelectedEdgeIndices.Add( CurEdgeIndex );
							}
						}
						else
						{
							// Append to selection
							SelectedEdgeIndices.Add( CurEdgeIndex );
						}
					}

					// Reset cached vertices and uv coordinates.
					SelectedEdgeVertices.Reset();
					for(int32 TexCoordIndex = 0; TexCoordIndex < MAX_STATIC_TEXCOORDS; ++TexCoordIndex)
					{
						SelectedEdgeTexCoords[TexCoordIndex].Reset();
					}

					for(FSelectedEdgeSet::TIterator SelectionIt( SelectedEdgeIndices ); SelectionIt; ++SelectionIt)
					{
						const uint32 EdgeIndex = *SelectionIt;
						const uint32 FaceIndex = EdgeIndex / 3;

						const uint32 WedgeIndex = FaceIndex * 3 + (EdgeIndex % 3);
						const uint32 WedgeIndex2 = FaceIndex * 3 + ((EdgeIndex + 1) % 3);

						// Cache edge vertices in local space.
						FVector EdgeVertices[ 2 ];
						EdgeVertices[ 0 ] = RawMesh.GetWedgePosition(WedgeIndex);
						EdgeVertices[ 1 ] = RawMesh.GetWedgePosition(WedgeIndex2);

						SelectedEdgeVertices.Add(EdgeVertices[0]);
						SelectedEdgeVertices.Add(EdgeVertices[1]);

						// Cache UV
						for(int32 TexCoordIndex = 0; TexCoordIndex < MAX_STATIC_TEXCOORDS; ++TexCoordIndex)
						{
							if( RawMesh.WedgeTexCoords[TexCoordIndex].Num() > 0)
							{
								FVector2D UVIndex1, UVIndex2;
								UVIndex1 = RawMesh.WedgeTexCoords[TexCoordIndex][WedgeIndex];
								UVIndex2 = RawMesh.WedgeTexCoords[TexCoordIndex][WedgeIndex2];
								SelectedEdgeTexCoords[TexCoordIndex].Add(UVIndex1);
								SelectedEdgeTexCoords[TexCoordIndex].Add(UVIndex2);
							}
						}
					}

					ClearSelectedEdges = false;
				}
			}
		}
	}

	if (ClearSelectedSockets && StaticMeshEditorPtr.Pin()->GetSelectedSocket())
	{
		StaticMeshEditorPtr.Pin()->SetSelectedSocket(NULL);
	}
	if (ClearSelectedPrims)
	{
		StaticMeshEditorPtr.Pin()->ClearSelectedPrims();
	}
	if (ClearSelectedEdges)
	{
		SelectedEdgeIndices.Empty();
		SelectedEdgeVertices.Empty();
		for (int32 TexCoordIndex = 0; TexCoordIndex < MAX_STATIC_TEXCOORDS; ++TexCoordIndex)
		{
			SelectedEdgeTexCoords[TexCoordIndex].Empty();
		}
	}

	Invalidate();
}

void FStaticMeshEditorViewportClient::PerspectiveCameraMoved()
{
	FEditorViewportClient::PerspectiveCameraMoved();

	// If in the process of transitioning to a new location, don't update the orbit camera position.
	// On the final update of the transition, we will get here with IsPlaying()==false, and the editor camera position will
	// be correctly updated.
	if (GetViewTransform().IsPlaying())
	{
		return;
	}

	// The static mesh editor saves the camera position in terms of an orbit camera, so ensure 
	// that orbit mode is enabled before we store the current transform information.
	const bool bWasOrbit = bUsingOrbitCamera;
	const FVector OldCameraLocation = GetViewLocation();
	const FRotator OldCameraRotation = GetViewRotation();
	ToggleOrbitCamera(true);

	const FVector OrbitPoint = GetLookAtLocation();
	const FVector OrbitZoom = GetViewLocation() - OrbitPoint;
	StaticMesh->EditorCameraPosition = FAssetEditorOrbitCameraPosition(
		OrbitPoint,
		OrbitZoom,
		GetViewRotation()
		);

	ToggleOrbitCamera(bWasOrbit);
}

void FStaticMeshEditorViewportClient::OnAssetViewerSettingsChanged(const FName& InPropertyName)
{
	if (InPropertyName == GET_MEMBER_NAME_CHECKED(FPreviewSceneProfile, bPostProcessingEnabled) || InPropertyName == NAME_None)
	{
		UAssetViewerSettings* Settings = UAssetViewerSettings::Get();
		const int32 ProfileIndex = AdvancedPreviewScene->GetCurrentProfileIndex();
		if (Settings->Profiles.IsValidIndex(ProfileIndex))
		{
			SetAdvancedShowFlagsForScene(Settings->Profiles[ProfileIndex].bPostProcessingEnabled);
		}		
	}
}

void FStaticMeshEditorViewportClient::SetAdvancedShowFlagsForScene(const bool bAdvancedShowFlags)
{	
	if (bAdvancedShowFlags)
	{
		EngineShowFlags.EnableAdvancedFeatures();
	}
	else
	{
		EngineShowFlags.DisableAdvancedFeatures();
	}
}

void FStaticMeshEditorViewportClient::SetFloorAndEnvironmentVisibility(const bool bVisible)
{
	AdvancedPreviewScene->SetFloorVisibility(bVisible, true);
	AdvancedPreviewScene->SetEnvironmentVisibility(bVisible, true);
}

void FStaticMeshEditorViewportClient::SetPreviewMesh(UStaticMesh* InStaticMesh, UStaticMeshComponent* InStaticMeshComponent, bool bResetCamera)
{
	StaticMesh = InStaticMesh;
	StaticMeshComponent = InStaticMeshComponent;

	if(StaticMeshComponent != nullptr)
	{
		StaticMeshComponent->bDrawMeshCollisionIfSimple = bShowSimpleCollision;
		StaticMeshComponent->bDrawMeshCollisionIfComplex = bShowComplexCollision;
		StaticMeshComponent->MarkRenderStateDirty();
	}
	
	if (bResetCamera)
	{
		// If we have a thumbnail transform, we will favor that over the camera position as the user may have customized this for a nice view
		// If we have neither a custom thumbnail nor a valid camera position, then we'll just use the default thumbnail transform 
		const USceneThumbnailInfo* const AssetThumbnailInfo = Cast<USceneThumbnailInfo>(StaticMesh->ThumbnailInfo);
		const USceneThumbnailInfo* const DefaultThumbnailInfo = USceneThumbnailInfo::StaticClass()->GetDefaultObject<USceneThumbnailInfo>();

		// Prefer the asset thumbnail if available
		const USceneThumbnailInfo* const ThumbnailInfo = (AssetThumbnailInfo) ? AssetThumbnailInfo : DefaultThumbnailInfo;
		check(ThumbnailInfo);

		FRotator ThumbnailAngle;
		ThumbnailAngle.Pitch = ThumbnailInfo->OrbitPitch;
		ThumbnailAngle.Yaw = ThumbnailInfo->OrbitYaw;
		ThumbnailAngle.Roll = 0;
		const float ThumbnailDistance = ThumbnailInfo->OrbitZoom;

		const float CameraY = StaticMesh->GetBounds().SphereRadius / (75.0f * PI / 360.0f);
		SetCameraSetup(
			FVector::ZeroVector,
			ThumbnailAngle,
			FVector(0.0f, CameraY + ThumbnailDistance - AutoViewportOrbitCameraTranslate, 0.0f),
			StaticMesh->GetBounds().Origin,
			-FVector(0, CameraY, 0),
			FRotator(0, 90.f, 0)
			);

		if (!AssetThumbnailInfo && StaticMesh->EditorCameraPosition.bIsSet)
		{
			// The static mesh editor saves the camera position in terms of an orbit camera, so ensure 
			// that orbit mode is enabled before we set the new transform information
			const bool bWasOrbit = bUsingOrbitCamera;
			ToggleOrbitCamera(true);

			SetViewRotation(StaticMesh->EditorCameraPosition.CamOrbitRotation);
			SetViewLocation(StaticMesh->EditorCameraPosition.CamOrbitPoint + StaticMesh->EditorCameraPosition.CamOrbitZoom);
			SetLookAtLocation(StaticMesh->EditorCameraPosition.CamOrbitPoint);

			ToggleOrbitCamera(bWasOrbit);
		}
	}
}

void FStaticMeshEditorViewportClient::ToggleDrawUVOverlay()
{
	SetDrawUVOverlay(!bDrawUVs);
}

void FStaticMeshEditorViewportClient::SetDrawUVOverlay(bool bShouldDraw)
{
	bDrawUVs = bShouldDraw;
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.Toolbar"), TEXT("bDrawUVs"), bDrawUVs ? TEXT("True") : TEXT("False"));
	}
	Invalidate();
}

bool FStaticMeshEditorViewportClient::IsDrawUVOverlayChecked() const
{
	return bDrawUVs;
}

void FStaticMeshEditorViewportClient::ToggleShowNormals()
{
	bDrawNormals = !bDrawNormals;
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.Toolbar"), TEXT("bDrawNormals"), bDrawNormals ? TEXT("True") : TEXT("False"));
	}
	Invalidate();
}

bool FStaticMeshEditorViewportClient::IsShowNormalsChecked() const
{
	return bDrawNormals;
}

void FStaticMeshEditorViewportClient::ToggleShowTangents()
{
	bDrawTangents = !bDrawTangents;
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.Toolbar"), TEXT("bDrawTangents"), bDrawTangents ? TEXT("True") : TEXT("False"));
	}
	Invalidate();
}

bool FStaticMeshEditorViewportClient::IsShowTangentsChecked() const
{
	return bDrawTangents;
}

void FStaticMeshEditorViewportClient::ToggleShowBinormals()
{
	bDrawBinormals = !bDrawBinormals;
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.Toolbar"), TEXT("bDrawBinormals"), bDrawBinormals ? TEXT("True") : TEXT("False"));
	}
	Invalidate();
}

bool FStaticMeshEditorViewportClient::IsShowBinormalsChecked() const
{
	return bDrawBinormals;
}

void FStaticMeshEditorViewportClient::ToggleDrawVertices()
{
	bDrawVertices = !bDrawVertices;
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.Toolbar"), TEXT("bDrawVertices"), bDrawVertices ? TEXT("True") : TEXT("False"));
	}
	Invalidate();
}

bool FStaticMeshEditorViewportClient::IsDrawVerticesChecked() const
{
	return bDrawVertices;
}

void FStaticMeshEditorViewportClient::ToggleShowSimpleCollision()
{
	bShowSimpleCollision = !bShowSimpleCollision;

	if (StaticMeshComponent != nullptr)
	{
		// Have to set this flag in case we are using 'use complex as simple'
		StaticMeshComponent->bDrawMeshCollisionIfSimple = bShowSimpleCollision;
		StaticMeshComponent->MarkRenderStateDirty();
	}

	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.Toolbar"), TEXT("bShowCollision"), (bShowSimpleCollision || bShowComplexCollision) ? TEXT("True") : TEXT("False"));
	}
	StaticMeshEditorPtr.Pin()->ClearSelectedPrims();
	Invalidate();
}

bool FStaticMeshEditorViewportClient::IsShowSimpleCollisionChecked() const
{
	return bShowSimpleCollision;
}

void FStaticMeshEditorViewportClient::ToggleShowComplexCollision()
{
	bShowComplexCollision = !bShowComplexCollision;

	if (StaticMeshComponent != nullptr)
	{
		StaticMeshComponent->bDrawMeshCollisionIfComplex = bShowComplexCollision;
		StaticMeshComponent->MarkRenderStateDirty();
	}

	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.Toolbar"), TEXT("bShowCollision"), (bShowSimpleCollision || bShowComplexCollision) ? TEXT("True") : TEXT("False"));
	}
	Invalidate();
}

bool FStaticMeshEditorViewportClient::IsShowComplexCollisionChecked() const
{
	return bShowComplexCollision;
}

void FStaticMeshEditorViewportClient::ToggleShowSockets()
{
	bShowSockets = !bShowSockets;
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.Toolbar"), TEXT("bShowSockets"), bShowSockets ? TEXT("True") : TEXT("False"));
	}
	Invalidate();
}
bool FStaticMeshEditorViewportClient::IsShowSocketsChecked() const
{
	return bShowSockets;
}

void FStaticMeshEditorViewportClient::ToggleShowPivot()
{
	bShowPivot = !bShowPivot;
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.Toolbar"), TEXT("bShowPivot"), bShowPivot ? TEXT("True") : TEXT("False"));
	}
	Invalidate();
}

bool FStaticMeshEditorViewportClient::IsShowPivotChecked() const
{
	return bShowPivot;
}

void FStaticMeshEditorViewportClient::ToggleDrawAdditionalData()
{
	bDrawAdditionalData = !bDrawAdditionalData;
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.StaticMesh.Toolbar"), TEXT("bDrawAdditionalData"), bDrawAdditionalData ? TEXT("True") : TEXT("False"));
	}
	Invalidate();
}

bool FStaticMeshEditorViewportClient::IsDrawAdditionalDataChecked() const
{
	return bDrawAdditionalData;
}

TSet< int32 >& FStaticMeshEditorViewportClient::GetSelectedEdges()
{ 
	return SelectedEdgeIndices;
}

void FStaticMeshEditorViewportClient::OnSocketSelectionChanged( UStaticMeshSocket* SelectedSocket )
{
	if (SelectedSocket)
	{
		SelectedEdgeIndices.Empty();

		if (WidgetMode == FWidget::WM_None || WidgetMode == FWidget::WM_Scale)
		{
			WidgetMode = FWidget::WM_Translate;
		}
	}

	Invalidate();
}

void FStaticMeshEditorViewportClient::ResetCamera()
{
	FocusViewportOnBox( StaticMeshComponent->Bounds.GetBox() );
	Invalidate();
}
#undef LOCTEXT_NAMESPACE 
