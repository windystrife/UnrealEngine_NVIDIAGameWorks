// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SBlastMeshEditorViewport.h"
#include "SBlastMeshEditorViewportToolBar.h"
#include "BlastMesh.h"
#include "BlastImportUI.h"
#include "BlastMeshEditor.h"
#include "BlastMeshEditorCommands.h"
#include "ViewportBlastMeshComponent.h"
#include "BlastMeshEditorModule.h"
#include "BlastFractureSettings.h"

#include "EditorModes.h"
#include "ComponentReregisterContext.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "UObject/Package.h"
#include "Slate/SceneViewport.h"
#include "UObject/UObjectHash.h"
#include "Widgets/Docking/SDockableTab.h"
#include "EditorViewportCommands.h"
#include "CanvasTypes.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "STransformViewportToolbar.h"
#include "SEditorViewportViewMenu.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SSpinBox.h"
#include "EditorStyleSet.h"
#include "SceneView.h"

/////////////////////////////////////////////////////////////////////////
// FBlastMeshEditorViewportClient

class FBlastMeshEditorViewportClient : public FEditorViewportClient, public TSharedFromThis<FBlastMeshEditorViewportClient>
{
protected:
	/** Skeletal Mesh Component used for preview */
	TWeakObjectPtr<UViewportBlastMeshComponent> PreviewBlastComp;

public:
	FBlastMeshEditorViewportClient(TWeakPtr<IBlastMeshEditor> InBlastMeshEditor, FPreviewScene& InPreviewScene, const TSharedRef<SBlastMeshEditorViewport>& InBlastMeshEditorViewport);
	//FBlastMeshEditorViewportClient(TWeakPtr<IBlastMeshEditor> InBlastMeshEditor, FPreviewScene& InPreviewScene, TWeakPtr<SBlastMeshEditorViewport>& InBlastMeshEditorViewport);

	// FEditorViewportClient interface
	virtual void Tick(float DeltaTime) override;
	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI) override;
	FLinearColor GetBackgroundColor() const override { return FLinearColor::Black; }
	virtual bool InputKey(FViewport* Viewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed = 1.f, bool bGamepad = false) override;
	virtual bool InputAxis(FViewport* Viewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples = 1, bool bGamepad = false) override;
	virtual void ProcessClick(class FSceneView& View, class HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;
	virtual void MouseMove(FViewport* Viewport, int32 x, int32 y) override;

	void RenderCollisionMesh(FPrimitiveDrawInterface* PDI, uint32_t ChunkIndex, const FColor& Color);

	void ResetCamera();

	/** Function to set the mesh component used for preview */
	void SetPreviewComponent( UViewportBlastMeshComponent* InPreviewBlastComp );
	
	inline void SetExplodeAmount(float InExplodeAmount)
	{
		ExplodeAmount = InExplodeAmount;
	}

	void ToggleFractureVisualization()
	{
		bShowFractureVisualization = !bShowFractureVisualization;
		Invalidate();
	}

	bool IsToggledFractureVisualization() const
	{
		return bShowFractureVisualization;
	}

	void ToggleAABBView()
	{
		bShowAABB = !bShowAABB;
		Invalidate();
	}

	bool IsToggledAABBView() const
	{
		return bShowAABB;
	}

	void ToggleCollisionMeshView()
	{
		bShowCollisionMesh = !bShowCollisionMesh;
		Invalidate();
	}

	bool IsToggledCollisionMeshView() const
	{
		return bShowCollisionMesh;
	}

	void ToggleVoronoiSitesView()
	{
		bShowVoronoiSites = !bShowVoronoiSites;
		Invalidate();
	}

	bool IsToggledVoronoiSitesView() const
	{
		return bShowVoronoiSites;
	}

private:
	/** Pointer back to the BlastMesh editor tool that owns us */
	TWeakPtr<IBlastMeshEditor> BlastMeshEditorPtr;
	TWeakPtr<SBlastMeshEditorViewport> BlastMeshEditorViewportPtr;

	FBoxSphereBounds MeshBounds;
	int32 MouseX, MouseY;
	float ExplodeAmount;
	bool bShowFractureVisualization = true;
	bool bShowAABB = true;
	bool bShowCollisionMesh = false;
	bool bShowVoronoiSites = false;

	const FColor Blue = FColor::Blue;
	const FColor Green = FColor::Green;
	const FColor Orange = FColor::Orange;
	const FColor White = FColor::White;
};

FBlastMeshEditorViewportClient::FBlastMeshEditorViewportClient(TWeakPtr<IBlastMeshEditor> InBlastMeshEditor, FPreviewScene& InPreviewScene, const TSharedRef<SBlastMeshEditorViewport>& InBlastMeshEditorViewport)
	: FEditorViewportClient(nullptr, &InPreviewScene, StaticCastSharedRef<SEditorViewport>(InBlastMeshEditorViewport))
//FBlastMeshEditorViewportClient::FBlastMeshEditorViewportClient(TWeakPtr<IBlastMeshEditor> InBlastMeshEditor, FPreviewScene& InPreviewScene, TWeakPtr<SBlastMeshEditorViewport>& InBlastMeshEditorViewport)
	//: FEditorViewportClient(nullptr, &InPreviewScene, StaticCastSharedRef<SEditorViewport>(InBlastMeshEditorViewport.Pin().ToSharedRef()))
	, BlastMeshEditorPtr(InBlastMeshEditor)
	, BlastMeshEditorViewportPtr(InBlastMeshEditorViewport)
{
	SetViewMode(VMI_Lit);

	OverrideNearClipPlane(1.0f);
	bUsingOrbitCamera = true;

	// Setup defaults for the common draw helper.
	DrawHelper.bDrawPivot = false;
	DrawHelper.bDrawWorldBox = false;
	DrawHelper.bDrawKillZ = false;
	DrawHelper.bDrawGrid = true;
	DrawHelper.GridColorAxis = FColor(70, 70, 70);
	DrawHelper.GridColorMajor = FColor(40, 40, 40);
	DrawHelper.GridColorMinor =  FColor(20, 20, 20);
	DrawHelper.PerspectiveGridSize = HALF_WORLD_MAX1;
}

void FBlastMeshEditorViewportClient::ResetCamera()
{
	UBlastMesh* BlastMesh = BlastMeshEditorPtr.Pin()->GetBlastMesh();
	if (BlastMesh && BlastMesh->Mesh)
	{
		// If we have a thumbnail transform, we will favor that over the camera position as the user may have customized this for a nice view
		// If we have neither a custom thumbnail nor a valid camera position, then we'll just use the default thumbnail transform 
		const USceneThumbnailInfo* const AssetThumbnailInfo = Cast<USceneThumbnailInfo>(BlastMesh->Mesh->ThumbnailInfo);
		const USceneThumbnailInfo* const DefaultThumbnailInfo = USceneThumbnailInfo::StaticClass()->GetDefaultObject<USceneThumbnailInfo>();

		// Prefer the asset thumbnail if available
		const USceneThumbnailInfo* const ThumbnailInfo = (AssetThumbnailInfo) ? AssetThumbnailInfo : DefaultThumbnailInfo;
		check(ThumbnailInfo);

		FRotator ThumbnailAngle;
		ThumbnailAngle.Pitch = ThumbnailInfo->OrbitPitch;
		ThumbnailAngle.Yaw = ThumbnailInfo->OrbitYaw;
		ThumbnailAngle.Roll = 0;
		//const float ThumbnailDistance = ThumbnailInfo->OrbitZoom;
		//const float AutoViewportOrbitCameraTranslate = 256.0f;
		const float CameraY = BlastMesh->Mesh->GetImportedBounds().SphereRadius / (75.0f * PI / 360.0f);
		SetCameraSetup(
			FVector::ZeroVector,
			ThumbnailAngle,
			FVector(0.0f, -CameraY/* + ThumbnailDistance - AutoViewportOrbitCameraTranslate*/, 0.0f),
			BlastMesh->Mesh->GetImportedBounds().Origin,
			-FVector(0, CameraY, 0),
			FRotator(0, 90.f, 0)
		);
	}
}

void FBlastMeshEditorViewportClient::SetPreviewComponent(UViewportBlastMeshComponent* InPreviewBlastComp)
{
	PreviewBlastComp = InPreviewBlastComp;

	if (InPreviewBlastComp)
	{
		MeshBounds = InPreviewBlastComp->GetBlastMesh()->Mesh->GetImportedBounds();
	}
}

void FBlastMeshEditorViewportClient::ProcessClick(class FSceneView& View, class HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	bool bKeepSelection = Viewport->KeyState(EKeys::LeftControl) || Viewport->KeyState(EKeys::RightControl);
	bool bSelectionChanged = false;
	auto BME = BlastMeshEditorPtr.Pin();
	auto BMEV = BlastMeshEditorViewportPtr.Pin();

	UViewportBlastMeshComponent* Comp = PreviewBlastComp.Get();
	if (Key == EKeys::LeftMouseButton && Event == EInputEvent::IE_Released && Comp)
	{
		FVector2D ScreenPos(HitX, HitY);
		FVector ClickOrigin, ViewDir;
		View.DeprojectFVector2D(ScreenPos, ClickOrigin, ViewDir);

		FVector ClickedChunkHitLoc;
		FVector ClickedChunkHitNorm;
		int32 ClickedChunk = Comp->GetChunkWorldHit(ClickOrigin, ClickOrigin + ViewDir * 100000.0f, ClickedChunkHitLoc, ClickedChunkHitNorm);

		if (BMEV->IsSelectedBlastVectorMode(EBlastViewportControlMode::None))
		{
			TSet<int32>& SelectedChunkIndices = BME->GetSelectedChunkIndices();

			if (ClickedChunk >= 0)
			{
				if (!SelectedChunkIndices.Contains(ClickedChunk))
				{
					if (!bKeepSelection) { SelectedChunkIndices.Empty(); }

					SelectedChunkIndices.Add(ClickedChunk);
					bSelectionChanged = true;
				}
				else
				{
					SelectedChunkIndices.Remove(ClickedChunk);
					bSelectionChanged = true;
				}
			}
			else if (!bKeepSelection)
			{
				SelectedChunkIndices.Empty();
				bSelectionChanged = true;
			}
		}
		else
		{
			if (ClickedChunk >= 0)
			{
				FVector Displacement = ExplodeAmount * Comp->ChunkDisplacements[ClickedChunk];
				const FTransform& ComponentSpaceTransform = Comp->GetBlastMesh()->GetComponentSpaceInitialBoneTransform(Comp->GetBlastMesh()->ChunkIndexToBoneIndex[ClickedChunk]);
				FVector ClickLocation = ClickedChunkHitLoc - ComponentSpaceTransform.GetRotation().RotateVector(Displacement);
				UBlastMesh* BlastMesh = BlastMeshEditorPtr.Pin()->GetBlastMesh();
				if (BMEV->IsSelectedBlastVectorMode(EBlastViewportControlMode::Normal))
				{
					BMEV->UpdateBlastVectorValue(BlastMesh->GetComponentSpaceInitialBoneTransform(ClickedChunk).TransformVector(ClickedChunkHitNorm), ClickLocation);
				}
				else
				{
					BMEV->UpdateBlastVectorValue(BlastMesh->GetComponentSpaceInitialBoneTransform(ClickedChunk).TransformVector(ClickLocation), ClickLocation);
				}
			}
		}
	}

	if (bSelectionChanged)
	{
		BME->UpdateChunkSelection();
	}
}

void FBlastMeshEditorViewportClient::MouseMove(FViewport* Viewport, int32 x, int32 y)
{
	MouseX = x;
	MouseY = y;
	RedrawAllViewportsIntoThisScene();
}

void FBlastMeshEditorViewportClient::Tick(float DeltaTime)
{
	FEditorViewportClient::Tick(DeltaTime);

	if (PreviewScene)
	{
		PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaTime);
	}
}

void FBlastMeshEditorViewportClient::Draw( const FSceneView* View,FPrimitiveDrawInterface* PDI )
{
	FEditorViewportClient::Draw(View, PDI);

	const bool DrawChunkMarker = true;

	UViewportBlastMeshComponent* Comp = PreviewBlastComp.Get();
	
	if (Comp && Comp->GetBlastMesh() != NULL)
	{
		FDynamicColoredMaterialRenderProxy* SelectedColorInstance = new FDynamicColoredMaterialRenderProxy(GEngine->GeomMaterial->GetRenderProxy(false), FColor(255, 0, 0, 128));
		PDI->RegisterDynamicResource(SelectedColorInstance);

		//Draw chunks and its Voronoi sites
		auto BME = BlastMeshEditorPtr.Pin();
		auto BMEV = BlastMeshEditorViewportPtr.Pin();
		const TSet<int32>& SelectedChunkIndices = BME->GetSelectedChunkIndices();
		auto& ChunkModels = BME->GetChunkEditorModels();

		for (int32 i = 0; i < ChunkModels.Num(); ++i)
		{
			int32 ChunkIndex = ChunkModels[i]->ChunkIndex;
			if (bShowCollisionMesh && ChunkModels[ChunkIndex]->bVisible)
			{
				const FColor Color = SelectedChunkIndices.Contains(ChunkIndex) ? Orange : White;
				RenderCollisionMesh(PDI, ChunkIndex, Color);
			}
			if (SelectedChunkIndices.Contains(ChunkIndex))
			{
				if (bShowAABB && ChunkModels[ChunkIndex]->bVisible)
				{
					FBox Bounds = Comp->GetChunkWorldBounds(ChunkIndex);
					DrawWireBox(PDI, Bounds, Blue, SDPG_World);
				}

				if (bShowVoronoiSites)
				{
					auto Sites = ChunkModels[ChunkIndex]->VoronoiSites;
					if (Sites.IsValid())
					{
						int32 BoneIndex = Comp->GetBlastMesh()->ChunkIndexToBoneIndex[ChunkIndex];
						auto tr = Comp->GetBlastMesh()->GetComponentSpaceInitialBoneTransform(BoneIndex).Inverse();
						FVector Displacement = ExplodeAmount * Comp->ChunkDisplacements[ChunkIndex];
						for (auto& Site : *Sites)
						{
							DrawWireStar(PDI, tr.TransformPosition(Site + Displacement), 3, Green, 0);
						}
					}
				}
			}
		}

		//Draw control mode vis
		FVector2D ScreenPos(MouseX, MouseY);
		FVector MouseOrigin, ViewDir;
		View->DeprojectFVector2D(ScreenPos, MouseOrigin, ViewDir);

		FVector MouseChunkHitLoc;
		FVector MouseChunkHitNorm;
		int32 HooveredChunk = Comp->GetChunkWorldHit(MouseOrigin, MouseOrigin + ViewDir * 100000.0f, MouseChunkHitLoc, MouseChunkHitNorm);
		if (HooveredChunk >= 0)
		{
			auto Values = BMEV->GetBlastVectorValueInScreenSpace();
			switch (BMEV->GetBlastVectorMode())
			{
			case EBlastViewportControlMode::Normal:
				{
					FQuat Rot = FQuat::FindBetweenVectors(FVector(1.f, 0.f, 0.f), MouseChunkHitNorm);
					DrawDirectionalArrow(PDI, FTransform(Rot, MouseChunkHitLoc).ToMatrixNoScale(), FColor::Blue, 10, 1, 255, .5f);
				}
				break;
			case EBlastViewportControlMode::TwoPoint:
				if (Values.Num())
				{
					FVector PrevPos = Values[0];
					FQuat Rot = FQuat::FindBetweenVectors(FVector(1.f, 0.f, 0.f), MouseChunkHitLoc - PrevPos);
					DrawWireSphereAutoSides(PDI, PrevPos, FColor::Red, 1.f, 1);
					DrawDirectionalArrow(PDI, FTransform(Rot, PrevPos).ToMatrixNoScale(), FColor::Blue, FVector::Distance(MouseChunkHitLoc, PrevPos), 1, 255, .5f);
				}
				break;
			case EBlastViewportControlMode::ThreePoint:
				if (Values.Num())
				{
					FVector Pos1 = Values[0];
					FVector Pos2 = Values.Num() == 1 ? MouseChunkHitLoc : Values[1];
					FQuat Rot = FQuat::FindBetweenVectors(FVector(1.f, 0.f, 0.f), Pos2 - Pos1);
					DrawDirectionalArrow(PDI, FTransform(Rot, Pos1).ToMatrixNoScale(), FColor::Blue, FVector::Distance(Pos1, Pos2), 0.f, 255, .5f);
					if (Values.Num() == 2)
					{
						DrawWireSphereAutoSides(PDI, Pos1 + (MouseChunkHitLoc - Pos1).ProjectOnTo(Pos2 - Pos1), FColor::Red, 1.f, 255);
					}
					else
					{
						DrawWireSphereAutoSides(PDI, Pos1, FColor::Red, 1.f, 0);
					}
				}
				break;
			}
			DrawWireSphereAutoSides(PDI, MouseChunkHitLoc, FColor::Red, 1.f, 0);
		}

		//Draw fracture method vis
		if (bShowFractureVisualization)
		{
			auto FS = BME->GetFractureSettings();
			if (FS != nullptr)
			{
				FVector Origin, Normal, Scale;
				int32_t BoneIndex = Comp->GetBlastMesh()->ChunkIndexToBoneIndex[0];
				FVector Displacement(0);
				if (BME->GetSelectedChunkIndices().Num() == 1)
				{
					for (int32_t ChunkIndex : BME->GetSelectedChunkIndices())
					{
						BoneIndex = Comp->GetBlastMesh()->ChunkIndexToBoneIndex[ChunkIndex];
						Displacement = ExplodeAmount * Comp->ChunkDisplacements[ChunkIndex];
					}
				}
				auto Tr = Comp->GetBlastMesh()->GetComponentSpaceInitialBoneTransform(BoneIndex).Inverse();
				Displacement = Tr.TransformPosition(Displacement);
				switch (FS->FractureMethod)
				{
				case EBlastFractureMethod::VoronoiInSphere:
					Origin = Tr.TransformPosition(FS->InSphereFracture->Origin) + Displacement;
					Scale = FVector(FS->InSphereFracture->Radius);
					DrawSphere(PDI, Origin, FRotator(0.f), Scale, 32, 32, SelectedColorInstance, 0);
					DrawWireSphereAutoSides(PDI, Origin, FColor::Red, Scale.X, 0);
					break;
				case EBlastFractureMethod::VoronoiRemoveInSphere:
					Origin = Tr.TransformPosition(FS->RemoveInSphere->Origin) + Displacement;
					Scale = FVector(FS->RemoveInSphere->Radius);
					DrawSphere(PDI, Origin, FRotator(0.f), Scale, 32, 32, SelectedColorInstance, 0);
					DrawWireSphereAutoSides(PDI, Origin, FColor::Red, Scale.X, 0);
					break;
				case EBlastFractureMethod::VoronoiRadial:
					Normal = Tr.TransformVector(FS->RadialFracture->Normal);
					Origin = Tr.TransformPosition(FS->RadialFracture->Origin) + Displacement;
					Scale = FVector(FS->RadialFracture->Radius);
					{
						FVector tangent, cotangent;
						Normal.FindBestAxisVectors(tangent, cotangent);
						DrawCylinder(PDI, Origin, tangent, cotangent, Normal, Scale.X, 0.2f * Scale.X, 32, SelectedColorInstance, 0);
						DrawWireCylinder(PDI, Origin, tangent, cotangent, Normal, FColor::Red, Scale.X, 0.2f * Scale.X, 32, 0);
					}
					break;
				case EBlastFractureMethod::Cut:
					Normal = Tr.TransformVector(FS->CutFracture->Normal);
					Origin = Tr.TransformPosition(FS->CutFracture->Point) + Displacement;
					Scale = FVector(2 * MeshBounds.SphereRadius, 2 * MeshBounds.SphereRadius, 0);
				case EBlastFractureMethod::Cutout:
					if (FS->FractureMethod == EBlastFractureMethod::Cutout)
					{
						Normal = Tr.TransformVector(FS->CutoutFracture->Normal);
						Origin = Tr.TransformPosition(FS->CutoutFracture->Origin) + Displacement;
						Scale = FVector(FS->CutoutFracture->Size.X, FS->CutoutFracture->Size.Y, FS->CutoutFracture->RotationZ);
					}
					{
						FTransform ScaleTr; ScaleTr.SetScale3D(FVector(Scale.X, Scale.Y, 1.f) * 0.5f);
						FTransform YawTr(FQuat(FVector(0.f, 0.f, 1.f), FMath::DegreesToRadians(Scale.Z)));
						FQuat Rot = FQuat::FindBetweenVectors(FVector(0.f, 0.f, 1.f), Normal);
						FTransform ClickedTr(Rot, Origin);
						DrawPlane10x10(PDI, (ScaleTr * YawTr * ClickedTr).ToMatrixWithScale(), 1.f, FVector2D(0, 0), FVector2D(1, 1), SelectedColorInstance, 0);
						Rot = FQuat::FindBetweenVectors(FVector(0.f, 0.f, 1.f), -Normal);
						ClickedTr = FTransform(Rot, Origin);
						DrawPlane10x10(PDI, (ScaleTr * YawTr * ClickedTr).ToMatrixWithScale(), 1.f, FVector2D(0, 0), FVector2D(1, 1), SelectedColorInstance, 0);
					}
					break;
				}
			}
		}
	}
}

void FBlastMeshEditorViewportClient::RenderCollisionMesh(FPrimitiveDrawInterface* PDI, uint32_t ChunkIndex, const FColor& Color)
{
	UBlastMesh* BlastMesh = BlastMeshEditorPtr.Pin()->GetBlastMesh();
	//FPhATSharedData::EPhATRenderMode CollisionViewMode = SharedData->GetCurrentCollisionViewMode();

	//for (int32 i = 0; i < PhysicsAsset->SkeletalBodySetups.Num(); ++i)
	{
		FName ChunkBone = BlastMesh->GetChunkIndexToBoneName()[ChunkIndex];
		int32 BodyIndex = BlastMesh->PhysicsAsset->FindBodyIndex(ChunkBone);
		int32 BoneIndex = BlastMesh->ChunkIndexToBoneIndex[ChunkIndex];

		// If we found a bone for it, draw the collision.
		// The logic is as follows; always render in the ViewMode requested when not in hit mode - but if we are in hit mode and the right editing mode, render as solid
		if (BoneIndex != INDEX_NONE)
		{
			FTransform BoneTM = PreviewBlastComp.Get()->GetBoneTransform(BoneIndex);
			float Scale = BoneTM.GetScale3D().GetAbsMax();
			FVector VectorScale(Scale);
			BoneTM.RemoveScaling();

			FKAggregateGeom* AggGeom = &BlastMesh->PhysicsAsset->SkeletalBodySetups[BodyIndex]->AggGeom;

			for (int32 j = 0; j < AggGeom->SphereElems.Num(); ++j)
			{
				//if (bHitTest)
				//{
				//	PDI->SetHitProxy(new HPhATEdBoneProxy(i, KPT_Sphere, j));
				//}

				FTransform ElemTM = BoneTM * AggGeom->SphereElems[j].GetTransform();// Comp->GetPrimitiveTransform(BoneTM, BodyIndex, KPT_Sphere, j, Scale);

				//solids are drawn if it's the ViewMode and we're not doing a hit, or if it's hitAndBodyMode
				//if ((CollisionViewMode == FPhATSharedData::PRM_Solid && !bHitTest) || bHitTestAndBodyMode)
				//{
				//	UMaterialInterface*    PrimMaterial = GetPrimitiveMaterial(i, KPT_Sphere, j, bHitTestAndBodyMode);
				//	AggGeom->SphereElems[j].DrawElemSolid(PDI, ElemTM, VectorScale, PrimMaterial->GetRenderProxy(0));
				//}

				//wires are never used during hit
				//if (!bHitTest)
				{
					//if (CollisionViewMode == FPhATSharedData::PRM_Solid || CollisionViewMode == FPhATSharedData::PRM_Wireframe)
					{
						AggGeom->SphereElems[j].DrawElemWire(PDI, ElemTM, VectorScale, Color);
					}
				}

				//if (bHitTest)
				//{
				//	PDI->SetHitProxy(NULL);
				//}

			}

			for (int32 j = 0; j < AggGeom->BoxElems.Num(); ++j)
			{
				FTransform ElemTM = BoneTM * AggGeom->BoxElems[j].GetTransform();
				AggGeom->BoxElems[j].DrawElemWire(PDI, ElemTM, VectorScale, Color);
			}

			for (int32 j = 0; j < AggGeom->SphylElems.Num(); ++j)
			{
				FTransform ElemTM = BoneTM * AggGeom->SphylElems[j].GetTransform();
				AggGeom->SphylElems[j].DrawElemWire(PDI, ElemTM, VectorScale, Color);
			}

			for (int32 j = 0; j < AggGeom->ConvexElems.Num(); ++j)
			{
				FTransform ElemTM = BoneTM *  AggGeom->ConvexElems[j].GetTransform();
				AggGeom->ConvexElems[j].DrawElemWire(PDI, ElemTM, Scale, Color);
			}

			//if (!bHitTest && SharedData->bShowCOM && Bodies.IsValidIndex(i))
			//{
			//	Bodies[i]->DrawCOMPosition(PDI, COMRenderSize, SharedData->COMRenderColor);
			//}
		}
	}
}

bool FBlastMeshEditorViewportClient::InputKey(FViewport* Viewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed /*= 1.f*/, bool bGamepad /*= false*/)
{
	bool bHandled = false;
	if (!bHandled)
	{
		bHandled |= FEditorViewportClient::InputKey(Viewport, ControllerId, Key, Event, AmountDepressed, false);
	}

	if (!bHandled)
	{
		bHandled |= static_cast<FAdvancedPreviewScene*>(PreviewScene)->HandleInputKey(Viewport, ControllerId, Key, Event, AmountDepressed, bGamepad);
		if (bHandled)
			Invalidate();
	}

	return bHandled;
}


bool FBlastMeshEditorViewportClient::InputAxis(FViewport* Viewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples /*= 1*/, bool bGamepad /*= false*/)
{
	bool bHandled = FEditorViewportClient::InputAxis(Viewport, ControllerId, Key, Delta, DeltaTime, NumSamples, bGamepad);

	if (!bHandled)
	{
		bHandled |= static_cast<FAdvancedPreviewScene*>(PreviewScene)->HandleViewportInput(Viewport, ControllerId, Key, Delta, DeltaTime, NumSamples, bGamepad);
		if (bHandled)
			Invalidate();
	}

	return bHandled;
}

//////////////////////////////////////////////////////////////////////////
// SBlastMeshEditorViewport

void SBlastMeshEditorViewport::Construct(const FArguments& InArgs)
{
	BlastMeshEditorPtr = InArgs._BlastMeshEditor;
	CurrentViewMode = VMI_Lit;

	PreviewScene = MakeShared<FAdvancedPreviewScene>(FPreviewScene::ConstructionValues());

	if (InArgs._ObjectToEdit->Mesh)
	{
		auto MeshBounds = InArgs._ObjectToEdit->Mesh->GetImportedBounds();
		PreviewScene->SetFloorOffset(-MeshBounds.Origin.Z + MeshBounds.BoxExtent.Z);
	}

	SEditorViewport::Construct(SEditorViewport::FArguments());

	//PreviewComponent = NewObject<UViewportBlastMeshComponent>(NewActor, "BlastMesh", RF_Transactional);
	PreviewComponent = NewObject<UViewportBlastMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient );

	SetPreviewMesh(InArgs._ObjectToEdit);

	ExplodeAmount = 0.1f;

	BlastVectorHandle = FBlastVector::OnVisualModification.AddRaw(this, &SBlastMeshEditorViewport::HandleBlastVector);
	ResetBlastVectorMode();
}

SBlastMeshEditorViewport::~SBlastMeshEditorViewport()
{
	if (PreviewComponent)
	{
		PreviewScene->RemoveComponent(PreviewComponent);
		PreviewComponent = NULL;
	}
	if (EditorViewportClient.IsValid())
	{
		EditorViewportClient->Viewport = NULL;
	}
	if (BlastVectorHandle.IsValid())
	{
		FBlastVector::OnVisualModification.Remove(BlastVectorHandle);
		BlastVectorHandle.Reset();
	}
}

void SBlastMeshEditorViewport::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject(PreviewComponent);
	Collector.AddReferencedObject(BlastMesh);
}

void SBlastMeshEditorViewport::NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, class FEditPropertyChain* PropertyThatChanged)
{
	for( FEditPropertyChain::TIterator It(PropertyThatChanged->GetHead()); It; ++It )
	{
		if (*It->GetName() == FName(TEXT("Materials")))
		{
			if (PreviewComponent != nullptr)
			{
				PreviewComponent->MarkRenderStateDirty();
				break;
			}
		}
	}
}

void SBlastMeshEditorViewport::RefreshViewport()
{
	// Update chunk visibilities
	if (BlastMesh != nullptr && PreviewComponent != nullptr && PreviewComponent->IsRegistered() && BlastMesh->GetLoadedAsset())
	{
		auto BME = BlastMeshEditorPtr.Pin();
		float MaxDownardDisplacment = 0;
		const uint32_t chunkCount = PreviewComponent->GetBlastAsset()->GetChunkCount();
		if (PreviewComponent->ChunkDisplacements.Num() != chunkCount)
		{
			PreviewComponent->BuildChunkDisplacements();
		}
		auto& ChunkModels = BME->GetChunkEditorModels();
		if (ChunkModels.Num())
		{
			for (uint32 ChunkIndex = 0; ChunkIndex < chunkCount; ++ChunkIndex)
			{
				const bool bChunkVisible = ChunkModels[ChunkIndex]->bVisible;
				PreviewComponent->SetChunkVisible(ChunkIndex, bChunkVisible);
				if (bChunkVisible)
				{
					FVector ChunkRestPos = BlastMesh->GetComponentSpaceInitialBoneTransform(BlastMesh->ChunkIndexToBoneIndex[ChunkIndex]).GetTranslation();
					FVector Displacement = ExplodeAmount * PreviewComponent->ChunkDisplacements[ChunkIndex];
					PreviewComponent->SetChunkLocation(ChunkIndex, ChunkRestPos + Displacement);
					MaxDownardDisplacment = FMath::Min(MaxDownardDisplacment, Displacement.Z);
				}
			}
		}
		PreviewComponent->BoundsScale = 100;

		PreviewComponent->ForceBoneTransformUpdate();

		auto MeshBounds = BlastMesh->Mesh->GetImportedBounds();
		PreviewScene->SetFloorOffset(-MeshBounds.Origin.Z + MeshBounds.BoxExtent.Z - MaxDownardDisplacment);
	}
	else
	{
		PreviewScene->SetFloorOffset(0);
	}

	EditorViewportClient->SetExplodeAmount(ExplodeAmount);
	RedrawViewport();
}

void SBlastMeshEditorViewport::RedrawViewport()
{
	// Invalidate the viewport's display.
	SceneViewport->InvalidateDisplay();
}

void SBlastMeshEditorViewport::ResetCamera()
{
	EditorViewportClient->ResetCamera();
}

void SBlastMeshEditorViewport::HandleBlastVector(const FBlastVector* Vector)
{
	ResetBlastVectorMode();
	BlastVectorMode = Vector->DefaultControlMode;
	SceneViewport->Invalidate();
	BlastVector = const_cast<FBlastVector*>(Vector);
}

void SBlastMeshEditorViewport::UpdateBlastVectorValue(FVector NewValue, FVector NewValueInScreenspace)
{
	if (BlastVector->DefaultControlMode == EBlastViewportControlMode::Normal)
	{
		NewValue.Normalize();
	}
	switch (BlastVectorMode)
	{
	case EBlastViewportControlMode::Normal:
	case EBlastViewportControlMode::Point:
		*BlastVector = NewValue;
		ResetBlastVectorMode(true);
		break;
	case EBlastViewportControlMode::TwoPoint:
		if (BlastVectorPreviouslyClickedValues.Num() >= 1)
		{
			*BlastVector = NewValue - BlastVectorPreviouslyClickedValues.Last();
			ResetBlastVectorMode(true);
		}
		else
		{
			BlastVectorPreviouslyClickedValues.Push(NewValue);
			BlastVectorPreviouslyClickedValuesInScreenSpace.Push(NewValueInScreenspace);
		}
		break;
	case EBlastViewportControlMode::ThreePoint:
		if (BlastVectorPreviouslyClickedValues.Num() >= 2)
		{
			if (BlastVector->DefaultControlMode == EBlastViewportControlMode::Normal)
			{
				*BlastVector = (NewValue - BlastVectorPreviouslyClickedValues[0]).ProjectOnTo(
					BlastVectorPreviouslyClickedValues[1] - BlastVectorPreviouslyClickedValues[0]);
			}
			else
			{
				*BlastVector = BlastVectorPreviouslyClickedValues[0] + (NewValue - BlastVectorPreviouslyClickedValues[0]).ProjectOnTo(
					BlastVectorPreviouslyClickedValues[1] - BlastVectorPreviouslyClickedValues[0]);
			}
			ResetBlastVectorMode(true);
		}
		else
		{
			BlastVectorPreviouslyClickedValues.Push(NewValue);
			BlastVectorPreviouslyClickedValuesInScreenSpace.Push(NewValueInScreenspace);
		}
		break;
	}
}

void SBlastMeshEditorViewport::ResetBlastVectorMode(bool ToDefault)
{
	if (ToDefault && BlastVector && BlastVector->DefaultBlastVectorActivation)
	{
		BlastVector->DefaultBlastVectorActivation->Activate();
		if (!IsBlastVectorModeSelectable(BlastVectorMode))
		{
			BlastVectorMode = BlastVector->DefaultControlMode;
		}
	}
	else
	{
		if (BlastVector)
		{
			BlastVector->IsActive = false;
		}
		BlastVectorMode = EBlastViewportControlMode::None;
	}
	BlastVectorPreviouslyClickedValues.Empty();
	BlastVectorPreviouslyClickedValuesInScreenSpace.Empty();
	RedrawViewport();
}

const TArray<FVector>& SBlastMeshEditorViewport::GetBlastVectorValueInScreenSpace() const
{
	return BlastVectorPreviouslyClickedValuesInScreenSpace;
}

void SBlastMeshEditorViewport::SetBlastVectorMode(EBlastViewportControlMode Mode)
{
	ResetBlastVectorMode();
	BlastVectorMode = Mode;
}

EBlastViewportControlMode SBlastMeshEditorViewport::GetBlastVectorMode() const
{
	return BlastVectorMode;
}

bool SBlastMeshEditorViewport::IsSelectedBlastVectorMode(EBlastViewportControlMode Mode) const
{
	return BlastVectorMode == Mode;
}

bool SBlastMeshEditorViewport::IsBlastVectorModeSelectable(EBlastViewportControlMode Mode) const
{
	if (Mode == EBlastViewportControlMode::None || BlastVector == nullptr)
	{
		return false;
	}
	if (BlastVector->DefaultControlMode == EBlastViewportControlMode::Normal)
	{
		return Mode == EBlastViewportControlMode::Normal || Mode == EBlastViewportControlMode::TwoPoint;
	}
	else
	{
		return Mode != EBlastViewportControlMode::Normal;
	}
}

void SBlastMeshEditorViewport::SetPreviewMesh(UBlastMesh* InBlastMesh)
{
	BlastMesh = InBlastMesh;

	if (InBlastMesh == nullptr || PreviewComponent == nullptr)
	{
		EditorViewportClient->SetPreviewComponent(nullptr);
		return;
	}

	FComponentReregisterContext ReregisterContext( PreviewComponent );

	PreviewComponent->SetBlastMesh(InBlastMesh);
	
	PreviewScene->AddComponent( PreviewComponent, FTransform::Identity);

	EditorViewportClient->SetPreviewComponent(PreviewComponent);

	PreviewComponent->InitAllActors();
}

void SBlastMeshEditorViewport::UpdatePreviewMesh(UBlastMesh* InBlastMesh)
{
	if (PreviewComponent)
	{
		PreviewScene->RemoveComponent(PreviewComponent);
		PreviewComponent = NULL;
	}

	//PreviewComponent = NewObject<UViewportBlastMeshComponent>(NewActor, "BlastMesh", RF_Transactional);
	if (InBlastMesh)
	{
		PreviewComponent = NewObject<UViewportBlastMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	}

	SetPreviewMesh(InBlastMesh);
}


TSharedRef<class SEditorViewport> SBlastMeshEditorViewport::GetViewportWidget()
{
	return SharedThis(this);
}


TSharedPtr<FExtender> SBlastMeshEditorViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void SBlastMeshEditorViewport::OnFloatingButtonClicked()
{

}

bool SBlastMeshEditorViewport::IsVisible() const
{
	return ViewportWidget.IsValid() && (!ParentTab.IsValid() || ParentTab.Pin()->IsForeground());
}

void SBlastMeshEditorViewport::SetExplodeAmount(float InExplodeAmount)
{
	float NewExplodeAmount = 0.0f;
	if (InExplodeAmount >= 0.0f)
	{
		NewExplodeAmount = InExplodeAmount;
	}

	if (NewExplodeAmount != ExplodeAmount)
	{
		ExplodeAmount = NewExplodeAmount;
		RefreshViewport();
	}
}

void SBlastMeshEditorViewport::SetViewModeWireframe()
{
	if(CurrentViewMode != VMI_Wireframe)
	{
		CurrentViewMode = VMI_Wireframe;
	}
	else
	{
		CurrentViewMode = VMI_Lit;
	}

	EditorViewportClient->SetViewMode(CurrentViewMode);
	SceneViewport->Invalidate();

}

bool SBlastMeshEditorViewport::IsInViewModeWireframeChecked() const
{
	return CurrentViewMode == VMI_Wireframe;
}

EVisibility SBlastMeshEditorViewport::GetTransformToolbarVisibility() const
{
	//return IsSelectedBlastVectorMode(EBlastViewportControlMode::None) ? EVisibility::Visible : EVisibility::Collapsed;
	return  EVisibility::Visible;
}

void SBlastMeshEditorViewport::BindCommands()
{
	SEditorViewport::BindCommands();

	const FBlastMeshEditorCommands& Commands = FBlastMeshEditorCommands::Get();

	TSharedRef<FBlastMeshEditorViewportClient> EditorViewportClientRef = EditorViewportClient.ToSharedRef();

	CommandList->MapAction(
		Commands.ToggleFractureVisualization,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FBlastMeshEditorViewportClient::ToggleFractureVisualization),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FBlastMeshEditorViewportClient::IsToggledFractureVisualization));

	CommandList->MapAction(
		Commands.ToggleAABBView,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FBlastMeshEditorViewportClient::ToggleAABBView),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FBlastMeshEditorViewportClient::IsToggledAABBView));

	CommandList->MapAction(
		Commands.ToggleCollisionMeshView,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FBlastMeshEditorViewportClient::ToggleCollisionMeshView),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FBlastMeshEditorViewportClient::IsToggledCollisionMeshView));

	CommandList->MapAction(
		Commands.ToggleVoronoiSitesView,
		FExecuteAction::CreateSP(EditorViewportClientRef, &FBlastMeshEditorViewportClient::ToggleVoronoiSitesView),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(EditorViewportClientRef, &FBlastMeshEditorViewportClient::IsToggledVoronoiSitesView));

	CommandList->MapAction(
		Commands.BlastVectorNormal,
		FExecuteAction::CreateSP(this, &SBlastMeshEditorViewport::SetBlastVectorMode, EBlastViewportControlMode::Normal),
		FCanExecuteAction::CreateSP(this, &SBlastMeshEditorViewport::IsBlastVectorModeSelectable, EBlastViewportControlMode::Normal),
		FIsActionChecked::CreateSP(this, &SBlastMeshEditorViewport::IsSelectedBlastVectorMode, EBlastViewportControlMode::Normal),
		FIsActionButtonVisible::CreateLambda([this]() {return !IsSelectedBlastVectorMode(EBlastViewportControlMode::None); }));

	CommandList->MapAction(
		Commands.BlastVectorPoint,
		FExecuteAction::CreateSP(this, &SBlastMeshEditorViewport::SetBlastVectorMode, EBlastViewportControlMode::Point),
		FCanExecuteAction::CreateSP(this, &SBlastMeshEditorViewport::IsBlastVectorModeSelectable, EBlastViewportControlMode::Point),
		FIsActionChecked::CreateSP(this, &SBlastMeshEditorViewport::IsSelectedBlastVectorMode, EBlastViewportControlMode::Point),
		FIsActionButtonVisible::CreateLambda([this]() {return !IsSelectedBlastVectorMode(EBlastViewportControlMode::None); }));

	CommandList->MapAction(
		Commands.BlastVectorTwoPoint,
		FExecuteAction::CreateSP(this, &SBlastMeshEditorViewport::SetBlastVectorMode, EBlastViewportControlMode::TwoPoint),
		FCanExecuteAction::CreateSP(this, &SBlastMeshEditorViewport::IsBlastVectorModeSelectable, EBlastViewportControlMode::TwoPoint),
		FIsActionChecked::CreateSP(this, &SBlastMeshEditorViewport::IsSelectedBlastVectorMode, EBlastViewportControlMode::TwoPoint),
		FIsActionButtonVisible::CreateLambda([this]() {return !IsSelectedBlastVectorMode(EBlastViewportControlMode::None); }));

	CommandList->MapAction(
		Commands.BlastVectorThreePoint,
		FExecuteAction::CreateSP(this, &SBlastMeshEditorViewport::SetBlastVectorMode, EBlastViewportControlMode::ThreePoint),
		FCanExecuteAction::CreateSP(this, &SBlastMeshEditorViewport::IsBlastVectorModeSelectable, EBlastViewportControlMode::ThreePoint),
		FIsActionChecked::CreateSP(this, &SBlastMeshEditorViewport::IsSelectedBlastVectorMode, EBlastViewportControlMode::ThreePoint),
		FIsActionButtonVisible::CreateLambda([this]() {return !IsSelectedBlastVectorMode(EBlastViewportControlMode::None); }));

	CommandList->MapAction(
		Commands.BlastVectorExit,
		FExecuteAction::CreateSP(this, &SBlastMeshEditorViewport::SetBlastVectorMode, EBlastViewportControlMode::None),
		FCanExecuteAction(),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateLambda([this]() {return !IsSelectedBlastVectorMode(EBlastViewportControlMode::None); }));
}

TSharedRef<FEditorViewportClient> SBlastMeshEditorViewport::MakeEditorViewportClient()
{
	check(PreviewScene.IsValid());
	EditorViewportClient = MakeShareable(new FBlastMeshEditorViewportClient(BlastMeshEditorPtr, *PreviewScene, SharedThis(this)));

	EditorViewportClient->bSetListenerPosition = false;

	EditorViewportClient->SetRealtime( false );
	EditorViewportClient->VisibilityDelegate.BindSP( this, &SBlastMeshEditorViewport::IsVisible );

	return EditorViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SBlastMeshEditorViewport::MakeViewportToolbar()
{
	return SNew(SBlastMeshEditorViewportToolbar, SharedThis(this));
}
