// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInstanceConstant.h"
#include "EngineUtils.h"
#include "EditorViewportClient.h"
#include "LandscapeToolInterface.h"
#include "LandscapeProxy.h"
#include "LandscapeGizmoActor.h"
#include "LandscapeEdMode.h"
#include "LandscapeEditorObject.h"

#include "LandscapeRender.h"

#include "LevelUtils.h"

// 
// FLandscapeBrush
//
bool GInLandscapeBrushTransaction = false;

void FLandscapeBrush::BeginStroke(float LandscapeX, float LandscapeY, FLandscapeTool* CurrentTool)
{
	if (!GInLandscapeBrushTransaction)
	{
		GEditor->BeginTransaction(FText::Format(NSLOCTEXT("UnrealEd", "LandscapeMode_EditTransaction", "Landscape Editing: {0}"), CurrentTool->GetDisplayName()));
		GInLandscapeBrushTransaction = true;
	}
}

void FLandscapeBrush::EndStroke()
{
	if (ensure(GInLandscapeBrushTransaction))
	{
		GEditor->EndTransaction();
		GInLandscapeBrushTransaction = false;
	}
}

// 
// FLandscapeBrushCircle
//

class FLandscapeBrushCircle : public FLandscapeBrush
{
	TSet<ULandscapeComponent*> BrushMaterialComponents;
	TArray<UMaterialInstanceDynamic*> BrushMaterialFreeInstances;

protected:
	FVector2D LastMousePosition;
	UMaterialInterface* BrushMaterial;
	TMap<ULandscapeComponent*, UMaterialInstanceDynamic*> BrushMaterialInstanceMap;

	virtual float CalculateFalloff(float Distance, float Radius, float Falloff) = 0;

	/** Protected so that only subclasses can create instances of this class. */
	FLandscapeBrushCircle(FEdModeLandscape* InEdMode, UMaterialInterface* InBrushMaterial)
		: LastMousePosition(0, 0)
		, BrushMaterial(LandscapeTool::CreateMaterialInstance(InBrushMaterial))
		, EdMode(InEdMode)
	{
	}

public:
	FEdModeLandscape* EdMode;

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(BrushMaterial);

		// Allow any currently unused material instances to be GC'd
		BrushMaterialFreeInstances.Empty();

		Collector.AddReferencedObjects(BrushMaterialComponents);
		Collector.AddReferencedObjects(BrushMaterialInstanceMap);

		// If a user tool removes any components then we will have bad (null) entries in our TSet/TMap, remove them
		// We can't just call .Remove(nullptr) because the entries were hashed as non-null values so a hash lookup of nullptr won't find them
		for (auto It = BrushMaterialComponents.CreateIterator(); It; ++It)
		{
			if (*It == nullptr)
			{
				It.RemoveCurrent();
			}
		}
		for (auto It = BrushMaterialInstanceMap.CreateIterator(); It; ++It)
		{
			if (It->Key == nullptr || It->Value == nullptr)
			{
				It.RemoveCurrent();
			}
		}
	}

	virtual void LeaveBrush() override
	{
		for (ULandscapeComponent* Component : BrushMaterialComponents)
		{
			if (Component)
			{
				Component->EditToolRenderData.ToolMaterial = nullptr;
				Component->UpdateEditToolRenderData();
			}
		}

		TArray<UMaterialInstanceDynamic*> BrushMaterialInstances;
		BrushMaterialInstanceMap.GenerateValueArray(BrushMaterialInstances);
		BrushMaterialFreeInstances += BrushMaterialInstances;
		BrushMaterialInstanceMap.Empty();
		BrushMaterialComponents.Empty();
	}

	virtual void BeginStroke(float LandscapeX, float LandscapeY, FLandscapeTool* CurrentTool) override
	{
		FLandscapeBrush::BeginStroke(LandscapeX, LandscapeY, CurrentTool);
		LastMousePosition = FVector2D(LandscapeX, LandscapeY);
	}

	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override
	{
		ULandscapeInfo* LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo.Get();
		ALandscapeProxy* Proxy = LandscapeInfo->GetLandscapeProxy();

		const float ScaleXY = FMath::Abs(LandscapeInfo->DrawScale.X);
		const float TotalRadius = EdMode->UISettings->BrushRadius / ScaleXY;
		const float Radius = (1.0f - EdMode->UISettings->BrushFalloff) * TotalRadius;
		const float Falloff = EdMode->UISettings->BrushFalloff * TotalRadius;

		FIntRect Bounds;
		Bounds.Min.X = FMath::FloorToInt(LastMousePosition.X - TotalRadius);
		Bounds.Min.Y = FMath::FloorToInt(LastMousePosition.Y - TotalRadius);
		Bounds.Max.X = FMath::CeilToInt( LastMousePosition.X + TotalRadius);
		Bounds.Max.Y = FMath::CeilToInt( LastMousePosition.Y + TotalRadius);

		TSet<ULandscapeComponent*> NewComponents;

		if (!ViewportClient->IsMovingCamera())
		{
			// GetComponentsInRegion expects an inclusive max
			LandscapeInfo->GetComponentsInRegion(Bounds.Min.X, Bounds.Min.Y, Bounds.Max.X - 1, Bounds.Max.Y - 1, NewComponents);
		}

		// Remove the material from any old components that are no longer in the region
		TSet<ULandscapeComponent*> RemovedComponents = BrushMaterialComponents.Difference(NewComponents);
		for (ULandscapeComponent* RemovedComponent : RemovedComponents)
		{
			BrushMaterialFreeInstances.Push(BrushMaterialInstanceMap.FindAndRemoveChecked(RemovedComponent));

			RemovedComponent->EditToolRenderData.ToolMaterial = nullptr;
			RemovedComponent->UpdateEditToolRenderData();
		}

		// Set brush material for components in new region
		TSet<ULandscapeComponent*> AddedComponents = NewComponents.Difference(BrushMaterialComponents);
		for (ULandscapeComponent* AddedComponent : AddedComponents)
		{
			UMaterialInstanceDynamic* BrushMaterialInstance = nullptr;
			if (BrushMaterialFreeInstances.Num() > 0)
			{
				BrushMaterialInstance = BrushMaterialFreeInstances.Pop();
			}
			else
			{
				BrushMaterialInstance = UMaterialInstanceDynamic::Create(BrushMaterial, nullptr);
			}
			BrushMaterialInstanceMap.Add(AddedComponent, BrushMaterialInstance);
			AddedComponent->EditToolRenderData.ToolMaterial = BrushMaterialInstance;
			AddedComponent->UpdateEditToolRenderData();
		}

		BrushMaterialComponents = MoveTemp(NewComponents);

		// Set params for brush material.
		FVector WorldLocation = Proxy->LandscapeActorToWorld().TransformPosition(FVector(LastMousePosition.X, LastMousePosition.Y, 0));

		for (const auto& BrushMaterialInstancePair : BrushMaterialInstanceMap)
		{
			ULandscapeComponent* const Component = BrushMaterialInstancePair.Key;
			UMaterialInstanceDynamic* const MaterialInstance = BrushMaterialInstancePair.Value;

				// Painting can cause the EditToolRenderData to be destructed, so update it if necessary
			if (!AddedComponents.Contains(Component))
			{
				if (Component->EditToolRenderData.ToolMaterial == nullptr)
				{
					Component->EditToolRenderData.ToolMaterial = MaterialInstance;
					Component->UpdateEditToolRenderData();
				}
			}

			MaterialInstance->SetScalarParameterValue(FName(TEXT("LocalRadius")), Radius);
			MaterialInstance->SetScalarParameterValue(FName(TEXT("LocalFalloff")), Falloff);
			MaterialInstance->SetVectorParameterValue(FName(TEXT("WorldPosition")), FLinearColor(WorldLocation.X, WorldLocation.Y, WorldLocation.Z, ScaleXY));

			bool bCanPaint = true;

			const ALandscapeProxy* LandscapeProxy = Component->GetLandscapeProxy();
			const ULandscapeLayerInfoObject* LayerInfo = EdMode->CurrentToolTarget.LayerInfo.Get();

			if (EdMode->CurrentToolTarget.TargetType == ELandscapeToolTargetType::Weightmap &&
				EdMode->UISettings->PaintingRestriction != ELandscapeLayerPaintingRestriction::None)
			{
				if (EdMode->UISettings->PaintingRestriction == ELandscapeLayerPaintingRestriction::UseComponentWhitelist &&
					!Component->LayerWhitelist.Contains(LayerInfo))
				{
					bCanPaint = false;
				}
				else
				{
					bool bExisting = Component->WeightmapLayerAllocations.ContainsByPredicate([LayerInfo](const FWeightmapLayerAllocationInfo& Allocation) { return Allocation.LayerInfo == LayerInfo; });
					if (!bExisting)
					{
						if (EdMode->UISettings->PaintingRestriction == ELandscapeLayerPaintingRestriction::ExistingOnly ||
							(EdMode->UISettings->PaintingRestriction == ELandscapeLayerPaintingRestriction::UseMaxLayers &&
							 LandscapeProxy->MaxPaintedLayersPerComponent > 0 && Component->WeightmapLayerAllocations.Num() >= LandscapeProxy->MaxPaintedLayersPerComponent))
						{
							bCanPaint = false;
						}
					}
				}
			}

			MaterialInstance->SetScalarParameterValue("CanPaint", bCanPaint ? 1.0f : 0.0f);
		}
	}

	virtual void MouseMove(float LandscapeX, float LandscapeY) override
	{
		LastMousePosition = FVector2D(LandscapeX, LandscapeY);
	}

	virtual FLandscapeBrushData ApplyBrush(const TArray<FLandscapeToolInteractorPosition>& InInteractorPositions) override
	{
		ULandscapeInfo* LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo.Get();
		const float ScaleXY = FMath::Abs(LandscapeInfo->DrawScale.X);
		const float TotalRadius = EdMode->UISettings->BrushRadius / ScaleXY;
		const float Radius = (1.0f - EdMode->UISettings->BrushFalloff) * TotalRadius;
		const float Falloff = EdMode->UISettings->BrushFalloff * TotalRadius;

		// Cap number of mouse positions to a sensible number
		TArray<FLandscapeToolInteractorPosition> InteractorPositions;
		if (InInteractorPositions.Num() > 10)
		{
			for (int32 i = 0; i < 10; ++i)
			{
				// Scale so we include the first and last of the input positions
				InteractorPositions.Add(InInteractorPositions[(i * (InInteractorPositions.Num() - 1)) / 9]);
			}
		}
		else
		{
			InteractorPositions = InInteractorPositions;
		}

		FIntRect Bounds;
		for (const FLandscapeToolInteractorPosition& InteractorPosition : InteractorPositions)
		{
			FIntRect SpotBounds;
			SpotBounds.Min.X = FMath::FloorToInt(InteractorPosition.Position.X - TotalRadius);
			SpotBounds.Min.Y = FMath::FloorToInt(InteractorPosition.Position.Y - TotalRadius);
			SpotBounds.Max.X = FMath::CeilToInt( InteractorPosition.Position.X + TotalRadius);
			SpotBounds.Max.Y = FMath::CeilToInt( InteractorPosition.Position.Y + TotalRadius);

			if (Bounds.IsEmpty())
			{
				Bounds = SpotBounds;
			}
			else
			{
				Bounds.Min = Bounds.Min.ComponentMin(SpotBounds.Min);
				Bounds.Max = Bounds.Max.ComponentMax(SpotBounds.Max);
			}
		}

		// Clamp to landscape bounds
		int32 MinX, MaxX, MinY, MaxY;
		if (!ensure(LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY)))
		{
			// Landscape has no components somehow
			return FLandscapeBrushData();
		}
		Bounds.Clip(FIntRect(MinX, MinY, MaxX + 1, MaxY + 1));

		FLandscapeBrushData BrushData(Bounds);

		for (const FLandscapeToolInteractorPosition& InteractorPosition : InteractorPositions)
		{
			FIntRect SpotBounds;
			SpotBounds.Min.X = FMath::Max(FMath::FloorToInt(InteractorPosition.Position.X - TotalRadius), Bounds.Min.X);
			SpotBounds.Min.Y = FMath::Max(FMath::FloorToInt(InteractorPosition.Position.Y - TotalRadius), Bounds.Min.Y);
			SpotBounds.Max.X = FMath::Min(FMath::CeilToInt( InteractorPosition.Position.X + TotalRadius), Bounds.Max.X);
			SpotBounds.Max.Y = FMath::Min(FMath::CeilToInt( InteractorPosition.Position.Y + TotalRadius), Bounds.Max.Y);

			for (int32 Y = SpotBounds.Min.Y; Y < SpotBounds.Max.Y; Y++)
			{
				float* Scanline = BrushData.GetDataPtr(FIntPoint(0, Y));

				for (int32 X = SpotBounds.Min.X; X < SpotBounds.Max.X; X++)
				{
					float PrevAmount = Scanline[X];
					if (PrevAmount < 1.0f)
					{
						// Distance from mouse
						float MouseDist = FMath::Sqrt(FMath::Square(InteractorPosition.Position.X - (float)X) + FMath::Square(InteractorPosition.Position.Y - (float)Y));

						float PaintAmount = CalculateFalloff(MouseDist, Radius, Falloff);

						if (PaintAmount > 0.0f)
						{
							if (EdMode->CurrentTool && EdMode->CurrentTool->GetToolType() != ELandscapeToolType::Mask
								&& EdMode->UISettings->bUseSelectedRegion && LandscapeInfo->SelectedRegion.Num() > 0)
							{
								float MaskValue = LandscapeInfo->SelectedRegion.FindRef(FIntPoint(X, Y));
								if (EdMode->UISettings->bUseNegativeMask)
								{
									MaskValue = 1.0f - MaskValue;
								}
								PaintAmount *= MaskValue;
							}

							if (PaintAmount > PrevAmount)
							{
								// Set the brush value for this vertex
								Scanline[X] = PaintAmount;
							}
						}
					}
				}
			}
		}

		return BrushData;
	}
};

// 
// FLandscapeBrushComponent
//

class FLandscapeBrushComponent : public FLandscapeBrush
{
	TSet<ULandscapeComponent*> BrushMaterialComponents;

	virtual const TCHAR* GetBrushName() override { return TEXT("Component"); }
	virtual FText GetDisplayName() override { return NSLOCTEXT("UnrealEd", "LandscapeMode_Brush_Component", "Component"); };

protected:
	FVector2D LastMousePosition;
	UMaterialInterface* BrushMaterial;
public:
	FEdModeLandscape* EdMode;

	FLandscapeBrushComponent(FEdModeLandscape* InEdMode)
		: BrushMaterial(nullptr)
		, EdMode(InEdMode)
	{
		UMaterial* BaseBrushMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/EditorLandscapeResources/SelectBrushMaterial.SelectBrushMaterial"));
		BrushMaterial = LandscapeTool::CreateMaterialInstance(BaseBrushMaterial);
	}

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObjects(BrushMaterialComponents);
		Collector.AddReferencedObject(BrushMaterial);
	}

	virtual ELandscapeBrushType GetBrushType() override { return ELandscapeBrushType::Component; }

	virtual void LeaveBrush() override
	{
		for (TSet<ULandscapeComponent*>::TIterator It(BrushMaterialComponents); It; ++It)
		{
			if ((*It) != nullptr)
			{
				(*It)->EditToolRenderData.ToolMaterial = nullptr;
				(*It)->UpdateEditToolRenderData();
			}
		}
		BrushMaterialComponents.Empty();
	}

	virtual void BeginStroke(float LandscapeX, float LandscapeY, FLandscapeTool* CurrentTool) override
	{
		FLandscapeBrush::BeginStroke(LandscapeX, LandscapeY, CurrentTool);
		LastMousePosition = FVector2D(LandscapeX, LandscapeY);
	}

	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override
	{
		TSet<ULandscapeComponent*> NewComponents;

		if (!ViewportClient->IsMovingCamera())
		{
			ULandscapeInfo* LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo.Get();
			if (LandscapeInfo && LandscapeInfo->ComponentSizeQuads > 0)
			{
				const int32 BrushSize = FMath::Max(EdMode->UISettings->BrushComponentSize, 0);

				const float BrushOriginX = LastMousePosition.X / LandscapeInfo->ComponentSizeQuads - (BrushSize - 1) / 2.0f;
				const float BrushOriginY = LastMousePosition.Y / LandscapeInfo->ComponentSizeQuads - (BrushSize - 1) / 2.0f;
				const int32 ComponentIndexX = FMath::FloorToInt(BrushOriginX);
				const int32 ComponentIndexY = FMath::FloorToInt(BrushOriginY);

				for (int32 YIndex = 0; YIndex < BrushSize; ++YIndex)
				{
					for (int32 XIndex = 0; XIndex < BrushSize; ++XIndex)
					{
						ULandscapeComponent* Component = LandscapeInfo->XYtoComponentMap.FindRef(FIntPoint((ComponentIndexX + XIndex), (ComponentIndexY + YIndex)));
						if (Component && FLevelUtils::IsLevelVisible(Component->GetLandscapeProxy()->GetLevel()))
						{
							// For MoveToLevel
							if (EdMode->CurrentTool->GetToolName() == FName("MoveToLevel"))
							{
								if (Component->GetLandscapeProxy() && !Component->GetLandscapeProxy()->GetLevel()->IsCurrentLevel())
								{
									NewComponents.Add(Component);
								}
							}
							else
							{
								NewComponents.Add(Component);
							}
						}
					}
				}

				// Set brush material for components in new region
				for (ULandscapeComponent* NewComponent : NewComponents)
				{
					NewComponent->EditToolRenderData.ToolMaterial = BrushMaterial;
					NewComponent->UpdateEditToolRenderData();
				}
			}
		}

		// Remove the material from any old components that are no longer in the region
		TSet<ULandscapeComponent*> RemovedComponents = BrushMaterialComponents.Difference(NewComponents);
		for (ULandscapeComponent* RemovedComponent : RemovedComponents)
		{
			if (RemovedComponent != nullptr)
			{
				RemovedComponent->EditToolRenderData.ToolMaterial = nullptr;
				RemovedComponent->UpdateEditToolRenderData();
			}
		}

		BrushMaterialComponents = MoveTemp(NewComponents);
	}

	virtual void MouseMove(float LandscapeX, float LandscapeY) override
	{
		LastMousePosition = FVector2D(LandscapeX, LandscapeY);
	}

	virtual FLandscapeBrushData ApplyBrush(const TArray<FLandscapeToolInteractorPosition>& InteractorPositions) override
	{
		// Selection Brush only works for 
		ULandscapeInfo* LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo.Get();

		FIntRect Bounds;

		// The add component tool needs the raw bounds of the brush rather than the bounds of the actually existing components under the brush
		if (EdMode->CurrentTool->GetToolName() == FName("AddComponent"))
		{
			const int32 BrushSize = FMath::Max(EdMode->UISettings->BrushComponentSize, 0);

			const float BrushOriginX = LastMousePosition.X / LandscapeInfo->ComponentSizeQuads - (BrushSize - 1) / 2.0f;
			const float BrushOriginY = LastMousePosition.Y / LandscapeInfo->ComponentSizeQuads - (BrushSize - 1) / 2.0f;
			const int32 ComponentIndexX = FMath::FloorToInt(BrushOriginX);
			const int32 ComponentIndexY = FMath::FloorToInt(BrushOriginY);

			Bounds.Min.X = (ComponentIndexX) * LandscapeInfo->ComponentSizeQuads;
			Bounds.Min.Y = (ComponentIndexY) * LandscapeInfo->ComponentSizeQuads;
			Bounds.Max.X = (ComponentIndexX + BrushSize) * LandscapeInfo->ComponentSizeQuads + 1;
			Bounds.Max.Y = (ComponentIndexY + BrushSize) * LandscapeInfo->ComponentSizeQuads + 1;
		}
		else
		{
			if (BrushMaterialComponents.Num() == 0)
			{
				return FLandscapeBrushData();
			}

			// Get extent for all components
			Bounds.Min.X = INT_MAX;
			Bounds.Min.Y = INT_MAX;
			Bounds.Max.X = INT_MIN;
			Bounds.Max.Y = INT_MIN;

			for (ULandscapeComponent* Component : BrushMaterialComponents)
			{
				if (ensure(Component))
				{
					Component->GetComponentExtent(Bounds.Min.X, Bounds.Min.Y, Bounds.Max.X, Bounds.Max.Y);
				}
			}

			// GetComponentExtent returns an inclusive max bound
			Bounds.Max += FIntPoint(1, 1);
		}

		FLandscapeBrushData BrushData(Bounds);

		for (int32 Y = Bounds.Min.Y; Y < Bounds.Max.Y; Y++)
		{
			float* Scanline = BrushData.GetDataPtr(FIntPoint(0, Y));

			for (int32 X = Bounds.Min.X; X < Bounds.Max.X; X++)
			{
				float PaintAmount = 1.0f;
				if (EdMode->CurrentTool && EdMode->CurrentTool->GetToolType() != ELandscapeToolType::Mask
					&& EdMode->UISettings->bUseSelectedRegion && LandscapeInfo->SelectedRegion.Num() > 0)
				{
					float MaskValue = LandscapeInfo->SelectedRegion.FindRef(FIntPoint(X, Y));
					if (EdMode->UISettings->bUseNegativeMask)
					{
						MaskValue = 1.0f - MaskValue;
					}
					PaintAmount *= MaskValue;
				}

				// Set the brush value for this vertex
				Scanline[X] = PaintAmount;
			}
		}

		return BrushData;
	}
};

// 
// FLandscapeBrushGizmo
//

class FLandscapeBrushGizmo : public FLandscapeBrush
{
	TSet<ULandscapeComponent*> BrushMaterialComponents;

	const TCHAR* GetBrushName() override { return TEXT("Gizmo"); }
	virtual FText GetDisplayName() override { return NSLOCTEXT("UnrealEd", "LandscapeMode_Brush_Gizmo", "Gizmo"); };

protected:
	UMaterialInstanceDynamic* BrushMaterial;
public:
	FEdModeLandscape* EdMode;

	FLandscapeBrushGizmo(FEdModeLandscape* InEdMode)
		: BrushMaterial(nullptr)
		, EdMode(InEdMode)
	{
		UMaterialInterface* GizmoMaterial = LoadObject<UMaterialInstanceConstant>(nullptr, TEXT("/Engine/EditorLandscapeResources/MaskBrushMaterial_Gizmo.MaskBrushMaterial_Gizmo"));
		BrushMaterial = UMaterialInstanceDynamic::Create(LandscapeTool::CreateMaterialInstance(GizmoMaterial), nullptr);
	}

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObjects(BrushMaterialComponents);
		Collector.AddReferencedObject(BrushMaterial);
	}

	virtual ELandscapeBrushType GetBrushType() override { return ELandscapeBrushType::Gizmo; }

	virtual void EnterBrush() override
	{
		// Make sure gizmo actor is selected
		ALandscapeGizmoActiveActor* Gizmo = EdMode->CurrentGizmoActor.Get();
		if (Gizmo)
		{
			GEditor->SelectNone(false, true);
			GEditor->SelectActor(Gizmo, true, false, true);
		}
	}

	virtual void LeaveBrush() override
	{
		for (TSet<ULandscapeComponent*>::TIterator It(BrushMaterialComponents); It; ++It)
		{
			if ((*It) != nullptr)
			{
				(*It)->EditToolRenderData.ToolMaterial = nullptr;
				(*It)->UpdateEditToolRenderData();
			}
		}
		BrushMaterialComponents.Empty();
	}

	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override
	{
		if (GLandscapeEditRenderMode & ELandscapeEditRenderMode::Gizmo || GLandscapeEditRenderMode & ELandscapeEditRenderMode::Select)
		{
			ALandscapeGizmoActiveActor* Gizmo = EdMode->CurrentGizmoActor.Get();

			if (Gizmo && Gizmo->TargetLandscapeInfo && (Gizmo->TargetLandscapeInfo == EdMode->CurrentToolTarget.LandscapeInfo.Get()) && Gizmo->GizmoTexture && Gizmo->GetRootComponent())
			{
				ULandscapeInfo* LandscapeInfo = Gizmo->TargetLandscapeInfo;
				if (LandscapeInfo && LandscapeInfo->GetLandscapeProxy())
				{
					float ScaleXY = FMath::Abs(LandscapeInfo->DrawScale.X);
					FMatrix LToW = LandscapeInfo->GetLandscapeProxy()->LandscapeActorToWorld().ToMatrixWithScale();
					FMatrix WToL = LToW.InverseFast();

					UTexture2D* DataTexture = Gizmo->GizmoTexture;
					FIntRect Bounds(MAX_int32, MAX_int32, MIN_int32, MIN_int32);
					FVector LocalPos[4];
					//FMatrix WorldToLocal = Proxy->LocalToWorld().Inverse();
					for (int32 i = 0; i < 4; ++i)
					{
						//LocalPos[i] = WorldToLocal.TransformPosition(Gizmo->FrustumVerts[i]);
						LocalPos[i] = WToL.TransformPosition(Gizmo->FrustumVerts[i]);
						Bounds.Min.X = FMath::Min(Bounds.Min.X, (int32)LocalPos[i].X);
						Bounds.Min.Y = FMath::Min(Bounds.Min.Y, (int32)LocalPos[i].Y);
						Bounds.Max.X = FMath::Max(Bounds.Max.X, (int32)LocalPos[i].X);
						Bounds.Max.Y = FMath::Max(Bounds.Max.Y, (int32)LocalPos[i].Y);
					}

					// GetComponentsInRegion expects an inclusive max
					TSet<ULandscapeComponent*> NewComponents;
					LandscapeInfo->GetComponentsInRegion(Bounds.Min.X, Bounds.Min.Y, Bounds.Max.X - 1, Bounds.Max.Y - 1, NewComponents);

					float SquaredScaleXY = FMath::Square(ScaleXY);
					FLinearColor AlphaScaleBias(
						SquaredScaleXY / (Gizmo->GetWidth() * DataTexture->GetSizeX()),
						SquaredScaleXY / (Gizmo->GetHeight() * DataTexture->GetSizeY()),
						Gizmo->TextureScale.X,
						Gizmo->TextureScale.Y
						);
					BrushMaterial->SetVectorParameterValue(FName(TEXT("AlphaScaleBias")), AlphaScaleBias);

					float Angle = (-EdMode->CurrentGizmoActor->GetActorRotation().Euler().Z) * PI / 180.0f;
					FLinearColor LandscapeLocation(EdMode->CurrentGizmoActor->GetActorLocation().X, EdMode->CurrentGizmoActor->GetActorLocation().Y, EdMode->CurrentGizmoActor->GetActorLocation().Z, Angle);
					BrushMaterial->SetVectorParameterValue(FName(TEXT("LandscapeLocation")), LandscapeLocation);
					BrushMaterial->SetTextureParameterValue(FName(TEXT("AlphaTexture")), DataTexture);

					// Set brush material for components in new region
					for (ULandscapeComponent* NewComponent : NewComponents)
					{
						NewComponent->EditToolRenderData.GizmoMaterial = ((Gizmo->DataType != LGT_None) && (GLandscapeEditRenderMode & ELandscapeEditRenderMode::Gizmo) ? BrushMaterial : nullptr);
						NewComponent->UpdateEditToolRenderData();
					}

					// Remove the material from any old components that are no longer in the region
					TSet<ULandscapeComponent*> RemovedComponents = BrushMaterialComponents.Difference(NewComponents);
					for (ULandscapeComponent* RemovedComponent : RemovedComponents)
					{
						if (RemovedComponent != nullptr)
						{
							RemovedComponent->EditToolRenderData.GizmoMaterial = nullptr;
							RemovedComponent->UpdateEditToolRenderData();
						}
					}

					BrushMaterialComponents = MoveTemp(NewComponents);
				}
			}
		}
	}

	virtual void MouseMove(float LandscapeX, float LandscapeY) override
	{
	}

	virtual TOptional<bool> InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent) override
	{

		if (InKey == EKeys::LeftMouseButton && InEvent == IE_Pressed)
		{
			int32 HitX = InViewport->GetMouseX();
			int32 HitY = InViewport->GetMouseY();
			HHitProxy* HitProxy = InViewport->GetHitProxy(HitX, HitY);

			HActor* ActorHitProxy = HitProxyCast<HActor>(HitProxy);
			if (ActorHitProxy && ActorHitProxy->Actor->IsA<ALandscapeGizmoActor>())
			{
				// don't treat clicks on a landscape gizmo as a tool invocation
				return TOptional<bool>(false);
			}
		}

		// default behaviour
		return TOptional<bool>();
	}

	virtual FLandscapeBrushData ApplyBrush(const TArray<FLandscapeToolInteractorPosition>& InteractorPositions) override
	{
		// Selection Brush only works for 
		ALandscapeGizmoActiveActor* Gizmo = EdMode->CurrentGizmoActor.Get();
		ULandscapeInfo* LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo.Get();

		if (!Gizmo || !Gizmo->GetRootComponent())
		{
			return FLandscapeBrushData();
		}

		if (BrushMaterialComponents.Num() == 0)
		{
			return FLandscapeBrushData();
		}

		Gizmo->TargetLandscapeInfo = LandscapeInfo;
		float ScaleXY = FMath::Abs(LandscapeInfo->DrawScale.X);

		// Get extent for all components
		FIntRect Bounds;
		Bounds.Min.X = INT_MAX;
		Bounds.Min.Y = INT_MAX;
		Bounds.Max.X = INT_MIN;
		Bounds.Max.Y = INT_MIN;

		for (ULandscapeComponent* Component : BrushMaterialComponents)
		{
			if (ensure(Component))
			{
				Component->GetComponentExtent(Bounds.Min.X, Bounds.Min.Y, Bounds.Max.X, Bounds.Max.Y);
			}
		}

		FLandscapeBrushData BrushData(Bounds);

		//FMatrix LandscapeToGizmoLocal = Landscape->LocalToWorld() * Gizmo->WorldToLocal();
		const float LW = Gizmo->GetWidth() / (2 * ScaleXY);
		const float LH = Gizmo->GetHeight() / (2 * ScaleXY);

		FMatrix WToL = LandscapeInfo->GetLandscapeProxy()->LandscapeActorToWorld().ToMatrixWithScale().InverseFast();
		FVector BaseLocation = WToL.TransformPosition(Gizmo->GetActorLocation());
		FMatrix LandscapeToGizmoLocal =
			(FTranslationMatrix(FVector(-LW + 0.5, -LH + 0.5, 0)) * FRotationTranslationMatrix(FRotator(0, Gizmo->GetActorRotation().Yaw, 0), FVector(BaseLocation.X, BaseLocation.Y, 0))).InverseFast();

		float W = Gizmo->GetWidth() / ScaleXY; //Gizmo->GetWidth() / (Gizmo->DrawScale * Gizmo->DrawScale3D.X);
		float H = Gizmo->GetHeight() / ScaleXY; //Gizmo->GetHeight() / (Gizmo->DrawScale * Gizmo->DrawScale3D.Y);

		for (int32 Y = Bounds.Min.Y; Y < Bounds.Max.Y; Y++)
		{
			float* Scanline = BrushData.GetDataPtr(FIntPoint(0, Y));

			for (int32 X = Bounds.Min.X; X < Bounds.Max.X; X++)
			{
				FVector GizmoLocal = LandscapeToGizmoLocal.TransformPosition(FVector(X, Y, 0));
				if (GizmoLocal.X < W && GizmoLocal.X > 0 && GizmoLocal.Y < H && GizmoLocal.Y > 0)
				{
					float PaintAmount = 1.0f;
					// Transform in 0,0 origin LW radius
					if (EdMode->UISettings->bSmoothGizmoBrush)
					{
						FVector TransformedLocal(FMath::Abs(GizmoLocal.X - LW), FMath::Abs(GizmoLocal.Y - LH) * (W / H), 0);
						float FalloffRadius = LW * EdMode->UISettings->BrushFalloff;
						float SquareRadius = LW - FalloffRadius;
						float Cos = FMath::Abs(TransformedLocal.X) / TransformedLocal.Size2D();
						float Sin = FMath::Abs(TransformedLocal.Y) / TransformedLocal.Size2D();
						float RatioX = FalloffRadius > 0.0f ? 1.0f - FMath::Clamp((FMath::Abs(TransformedLocal.X) - Cos*SquareRadius) / FalloffRadius, 0.0f, 1.0f) : 1.0f;
						float RatioY = FalloffRadius > 0.0f ? 1.0f - FMath::Clamp((FMath::Abs(TransformedLocal.Y) - Sin*SquareRadius) / FalloffRadius, 0.0f, 1.0f) : 1.0f;
						float Ratio = TransformedLocal.Size2D() > SquareRadius ? RatioX * RatioY : 1.0f; //TransformedLocal.X / LW * TransformedLocal.Y / LW;
						PaintAmount = Ratio*Ratio*(3 - 2 * Ratio); //FMath::Lerp(SquareFalloff, RectFalloff*RectFalloff, Ratio);
					}

					if (PaintAmount)
					{
						if (EdMode->CurrentTool && EdMode->CurrentTool->GetToolType() != ELandscapeToolType::Mask
							&& EdMode->UISettings->bUseSelectedRegion && LandscapeInfo->SelectedRegion.Num() > 0)
						{
							float MaskValue = LandscapeInfo->SelectedRegion.FindRef(FIntPoint(X, Y));
							if (EdMode->UISettings->bUseNegativeMask)
							{
								MaskValue = 1.0f - MaskValue;
							}
							PaintAmount *= MaskValue;
						}

						// Set the brush value for this vertex
						Scanline[X] = PaintAmount;
					}
				}
			}
		}

		return BrushData;
	}
};

// 
// FLandscapeBrushSplines
//
class FLandscapeBrushSplines : public FLandscapeBrush
{
public:
	const TCHAR* GetBrushName() override { return TEXT("Splines"); }
	virtual FText GetDisplayName() override { return NSLOCTEXT("UnrealEd", "LandscapeMode_Brush_Splines", "Splines"); };

	FEdModeLandscape* EdMode;

	FLandscapeBrushSplines(FEdModeLandscape* InEdMode)
		: EdMode(InEdMode)
	{
	}

	virtual ~FLandscapeBrushSplines()
	{
	}

	virtual ELandscapeBrushType GetBrushType() override { return ELandscapeBrushType::Splines; }

	virtual void MouseMove(float LandscapeX, float LandscapeY) override
	{
	}

	virtual FLandscapeBrushData ApplyBrush(const TArray<FLandscapeToolInteractorPosition>& InteractorPositions) override
	{
		return FLandscapeBrushData();
	}
};

// 
// FLandscapeBrushDummy
//
class FLandscapeBrushDummy : public FLandscapeBrush
{
public:
	const TCHAR* GetBrushName() override { return TEXT("None"); }
	virtual FText GetDisplayName() override { return NSLOCTEXT("UnrealEd", "LandscapeMode_Brush_None", "None"); };

	FEdModeLandscape* EdMode;

	FLandscapeBrushDummy(FEdModeLandscape* InEdMode)
		: EdMode(InEdMode)
	{
	}

	virtual ~FLandscapeBrushDummy()
	{
	}

	virtual ELandscapeBrushType GetBrushType() override { return ELandscapeBrushType::Normal; }

	virtual void MouseMove(float LandscapeX, float LandscapeY) override
	{
	}

	virtual FLandscapeBrushData ApplyBrush(const TArray<FLandscapeToolInteractorPosition>& InteractorPositions) override
	{
		return FLandscapeBrushData();
	}
};

class FLandscapeBrushCircle_Linear : public FLandscapeBrushCircle
{
protected:
	/** Protected so only subclasses can create instances. */
	FLandscapeBrushCircle_Linear(FEdModeLandscape* InEdMode, UMaterialInterface* InBrushMaterial)
		: FLandscapeBrushCircle(InEdMode, InBrushMaterial)
	{
	}

	virtual float CalculateFalloff(float Distance, float Radius, float Falloff) override
	{
		return Distance < Radius ? 1.0f :
			Falloff > 0.0f ? FMath::Max<float>(0.0f, 1.0f - (Distance - Radius) / Falloff) :
			0.0f;
	}

public:
	static FLandscapeBrushCircle_Linear* Create(FEdModeLandscape* InEdMode)
	{
		UMaterialInstanceConstant* CircleBrushMaterial_Linear = LoadObject<UMaterialInstanceConstant>(nullptr, TEXT("/Engine/EditorLandscapeResources/CircleBrushMaterial_Linear.CircleBrushMaterial_Linear"), nullptr, LOAD_None, nullptr);
		return new FLandscapeBrushCircle_Linear(InEdMode, CircleBrushMaterial_Linear);
	}
	virtual const TCHAR* GetBrushName() override { return TEXT("Circle_Linear"); }
	virtual FText GetDisplayName() override { return NSLOCTEXT("UnrealEd", "LandscapeMode_Brush_Falloff_Linear", "Linear falloff"); };

};

class FLandscapeBrushCircle_Smooth : public FLandscapeBrushCircle_Linear
{
protected:
	FLandscapeBrushCircle_Smooth(FEdModeLandscape* InEdMode, UMaterialInterface* InBrushMaterial)
		: FLandscapeBrushCircle_Linear(InEdMode, InBrushMaterial)
	{
	}

	virtual float CalculateFalloff(float Distance, float Radius, float Falloff) override
	{
		float y = FLandscapeBrushCircle_Linear::CalculateFalloff(Distance, Radius, Falloff);
		// Smooth-step it
		return y*y*(3 - 2 * y);
	}

public:
	static FLandscapeBrushCircle_Smooth* Create(FEdModeLandscape* InEdMode)
	{
		UMaterialInstanceConstant* CircleBrushMaterial_Smooth = LoadObject<UMaterialInstanceConstant>(nullptr, TEXT("/Engine/EditorLandscapeResources/CircleBrushMaterial_Smooth.CircleBrushMaterial_Smooth"), nullptr, LOAD_None, nullptr);
		return new FLandscapeBrushCircle_Smooth(InEdMode, CircleBrushMaterial_Smooth);
	}
	virtual const TCHAR* GetBrushName() override { return TEXT("Circle_Smooth"); }
	virtual FText GetDisplayName() override { return NSLOCTEXT("UnrealEd", "LandscapeMode_Brush_Falloff_Smooth", "Smooth falloff"); };

};

class FLandscapeBrushCircle_Spherical : public FLandscapeBrushCircle
{
protected:
	FLandscapeBrushCircle_Spherical(FEdModeLandscape* InEdMode, UMaterialInterface* InBrushMaterial)
		: FLandscapeBrushCircle(InEdMode, InBrushMaterial)
	{
	}

	virtual float CalculateFalloff(float Distance, float Radius, float Falloff) override
	{
		if (Distance <= Radius)
		{
			return 1.0f;
		}

		if (Distance > Radius + Falloff)
		{
			return 0.0f;
		}

		// Elliptical falloff
		return FMath::Sqrt(1.0f - FMath::Square((Distance - Radius) / Falloff));
	}

public:
	static FLandscapeBrushCircle_Spherical* Create(FEdModeLandscape* InEdMode)
	{
		UMaterialInstanceConstant* CircleBrushMaterial_Spherical = LoadObject<UMaterialInstanceConstant>(nullptr, TEXT("/Engine/EditorLandscapeResources/CircleBrushMaterial_Spherical.CircleBrushMaterial_Spherical"), nullptr, LOAD_None, nullptr);
		return new FLandscapeBrushCircle_Spherical(InEdMode, CircleBrushMaterial_Spherical);
	}
	virtual const TCHAR* GetBrushName() override { return TEXT("Circle_Spherical"); }
	virtual FText GetDisplayName() override { return NSLOCTEXT("UnrealEd", "LandscapeMode_Brush_Falloff_Spherical", "Spherical falloff"); };
};

class FLandscapeBrushCircle_Tip : public FLandscapeBrushCircle
{
protected:
	FLandscapeBrushCircle_Tip(FEdModeLandscape* InEdMode, UMaterialInterface* InBrushMaterial)
		: FLandscapeBrushCircle(InEdMode, InBrushMaterial)
	{
	}

	virtual float CalculateFalloff(float Distance, float Radius, float Falloff) override
	{
		if (Distance <= Radius)
		{
			return 1.0f;
		}

		if (Distance > Radius + Falloff)
		{
			return 0.0f;
		}

		// inverse elliptical falloff
		return 1.0f - FMath::Sqrt(1.0f - FMath::Square((Falloff + Radius - Distance) / Falloff));
	}

public:
	static FLandscapeBrushCircle_Tip* Create(FEdModeLandscape* InEdMode)
	{
		UMaterialInstanceConstant* CircleBrushMaterial_Tip = LoadObject<UMaterialInstanceConstant>(nullptr, TEXT("/Engine/EditorLandscapeResources/CircleBrushMaterial_Tip.CircleBrushMaterial_Tip"), nullptr, LOAD_None, nullptr);
		return new FLandscapeBrushCircle_Tip(InEdMode, CircleBrushMaterial_Tip);
	}
	virtual const TCHAR* GetBrushName() override { return TEXT("Circle_Tip"); }
	virtual FText GetDisplayName() override { return NSLOCTEXT("UnrealEd", "LandscapeMode_Brush_Falloff_Tip", "Tip falloff"); };
};


// FLandscapeBrushAlphaBase
class FLandscapeBrushAlphaBase : public FLandscapeBrushCircle_Smooth
{
public:
	FLandscapeBrushAlphaBase(FEdModeLandscape* InEdMode, UMaterialInterface* InBrushMaterial)
		: FLandscapeBrushCircle_Smooth(InEdMode, InBrushMaterial)
	{
	}

	float GetAlphaSample(float SampleX, float SampleY)
	{
		int32 SizeX = EdMode->UISettings->AlphaTextureSizeX;
		int32 SizeY = EdMode->UISettings->AlphaTextureSizeY;

		// Bilinear interpolate the values from the alpha texture
		int32 SampleX0 = FMath::FloorToInt(SampleX);
		int32 SampleX1 = (SampleX0 + 1) % SizeX;
		int32 SampleY0 = FMath::FloorToInt(SampleY);
		int32 SampleY1 = (SampleY0 + 1) % SizeY;

		const uint8* AlphaData = EdMode->UISettings->AlphaTextureData.GetData();

		float Alpha00 = (float)AlphaData[SampleX0 + SampleY0 * SizeX] / 255.0f;
		float Alpha01 = (float)AlphaData[SampleX0 + SampleY1 * SizeX] / 255.0f;
		float Alpha10 = (float)AlphaData[SampleX1 + SampleY0 * SizeX] / 255.0f;
		float Alpha11 = (float)AlphaData[SampleX1 + SampleY1 * SizeX] / 255.0f;

		return FMath::Lerp(
			FMath::Lerp(Alpha00, Alpha01, FMath::Fractional(SampleX)),
			FMath::Lerp(Alpha10, Alpha11, FMath::Fractional(SampleX)),
			FMath::Fractional(SampleY)
			);
	}

};

//
// FLandscapeBrushAlphaPattern
//
class FLandscapeBrushAlphaPattern : public FLandscapeBrushAlphaBase
{
protected:
	FLandscapeBrushAlphaPattern(FEdModeLandscape* InEdMode, UMaterialInterface* InBrushMaterial)
		: FLandscapeBrushAlphaBase(InEdMode, InBrushMaterial)
	{
	}

public:
	static FLandscapeBrushAlphaPattern* Create(FEdModeLandscape* InEdMode)
	{
		UMaterialInstanceConstant* PatternBrushMaterial = LoadObject<UMaterialInstanceConstant>(nullptr, TEXT("/Engine/EditorLandscapeResources/PatternBrushMaterial_Smooth.PatternBrushMaterial_Smooth"), nullptr, LOAD_None, nullptr);
		return new FLandscapeBrushAlphaPattern(InEdMode, PatternBrushMaterial);
	}

	virtual ELandscapeBrushType GetBrushType() override { return ELandscapeBrushType::Alpha; }

	virtual FLandscapeBrushData ApplyBrush(const TArray<FLandscapeToolInteractorPosition>& InteractorPositions) override
	{
		ULandscapeInfo* LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo.Get();
		const float ScaleXY = FMath::Abs(LandscapeInfo->DrawScale.X);
		const float TotalRadius = EdMode->UISettings->BrushRadius / ScaleXY;
		const float Radius = (1.0f - EdMode->UISettings->BrushFalloff) * TotalRadius;
		const float Falloff = EdMode->UISettings->BrushFalloff * TotalRadius;

		int32 SizeX = EdMode->UISettings->AlphaTextureSizeX;
		int32 SizeY = EdMode->UISettings->AlphaTextureSizeY;

		FIntRect Bounds;
		Bounds.Min.X = FMath::FloorToInt(LastMousePosition.X - TotalRadius);
		Bounds.Min.Y = FMath::FloorToInt(LastMousePosition.Y - TotalRadius);
		Bounds.Max.X = FMath::CeilToInt( LastMousePosition.X + TotalRadius);
		Bounds.Max.Y = FMath::CeilToInt( LastMousePosition.Y + TotalRadius);


		// Clamp to landscape bounds
		int32 MinX, MaxX, MinY, MaxY;
		if (!ensure(LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY)))
		{
			// Landscape has no components somehow
			return FLandscapeBrushData();
		}
		Bounds.Clip(FIntRect(MinX, MinY, MaxX + 1, MaxY + 1));

		FLandscapeBrushData BrushData(Bounds);

		for (int32 Y = Bounds.Min.Y; Y < Bounds.Max.Y; Y++)
		{
			float* Scanline = BrushData.GetDataPtr(FIntPoint(0, Y));

			for (int32 X = Bounds.Min.X; X < Bounds.Max.X; X++)
			{
				float Angle;
				FVector2D Scale;
				FVector2D Bias;
				if (EdMode->UISettings->bUseWorldSpacePatternBrush)
				{
					FVector2D LocalOrigin = -FVector2D(LandscapeInfo->GetLandscapeProxy()->LandscapeActorToWorld().InverseTransformPosition(FVector(EdMode->UISettings->WorldSpacePatternBrushSettings.Origin, 0.0f)));
					const FVector2D LocalScale = FVector2D(
						ScaleXY / (EdMode->UISettings->WorldSpacePatternBrushSettings.RepeatSize * ((float)SizeX / SizeY)),
						ScaleXY / (EdMode->UISettings->WorldSpacePatternBrushSettings.RepeatSize));
					LocalOrigin *= LocalScale;
					Angle = -EdMode->UISettings->WorldSpacePatternBrushSettings.Rotation;
					if (EdMode->UISettings->WorldSpacePatternBrushSettings.bCenterTextureOnOrigin)
					{
						LocalOrigin += FVector2D(0.5f, 0.5f).GetRotated(-Angle);
					}
					Scale = FVector2D(SizeX, SizeY) * LocalScale;
					Bias = FVector2D(SizeX, SizeY) * LocalOrigin;
				}
				else
				{
					Scale.X = 1.0f / EdMode->UISettings->AlphaBrushScale;
					Scale.Y = 1.0f / EdMode->UISettings->AlphaBrushScale;
					Bias.X = SizeX * EdMode->UISettings->AlphaBrushPanU;
					Bias.Y = SizeY * EdMode->UISettings->AlphaBrushPanV;
					Angle = EdMode->UISettings->AlphaBrushRotation;
				}

				// Find alphamap sample location
				FVector2D SamplePos = FVector2D(X, Y) * Scale + Bias;
				SamplePos = SamplePos.GetRotated(Angle);

				float ModSampleX = FMath::Fmod(SamplePos.X, (float)SizeX);
				float ModSampleY = FMath::Fmod(SamplePos.Y, (float)SizeY);

				if (ModSampleX < 0.0f)
				{
					ModSampleX += (float)SizeX;
				}
				if (ModSampleY < 0.0f)
				{
					ModSampleY += (float)SizeY;
				}

				// Sample the alpha texture
				float Alpha = GetAlphaSample(ModSampleX, ModSampleY);

				// Distance from mouse
				float MouseDist = FMath::Sqrt(FMath::Square(LastMousePosition.X - (float)X) + FMath::Square(LastMousePosition.Y - (float)Y));

				float PaintAmount = CalculateFalloff(MouseDist, Radius, Falloff) * Alpha;

				if (PaintAmount > 0.0f)
				{
					if (EdMode->CurrentTool && EdMode->CurrentTool->GetToolType() != ELandscapeToolType::Mask
						&& EdMode->UISettings->bUseSelectedRegion && LandscapeInfo->SelectedRegion.Num() > 0)
					{
						float MaskValue = LandscapeInfo->SelectedRegion.FindRef(FIntPoint(X, Y));
						if (EdMode->UISettings->bUseNegativeMask)
						{
							MaskValue = 1.0f - MaskValue;
						}
						PaintAmount *= MaskValue;
					}
					// Set the brush value for this vertex
					Scanline[X] = PaintAmount;
				}
			}
		}
		return BrushData;
	}

	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override
	{
		FLandscapeBrushCircle::Tick(ViewportClient, DeltaTime);

		ALandscapeProxy* Proxy = EdMode->CurrentToolTarget.LandscapeInfo.IsValid() ? EdMode->CurrentToolTarget.LandscapeInfo->GetLandscapeProxy() : nullptr;
		if (Proxy)
		{
			const float ScaleXY = FMath::Abs(EdMode->CurrentToolTarget.LandscapeInfo->DrawScale.X);
			int32 SizeX = EdMode->UISettings->AlphaTextureSizeX;
			int32 SizeY = EdMode->UISettings->AlphaTextureSizeY;

			FLinearColor AlphaScaleBias;
			float Angle;
			if (EdMode->UISettings->bUseWorldSpacePatternBrush)
			{
				FVector2D LocalOrigin = -FVector2D(Proxy->LandscapeActorToWorld().InverseTransformPosition(FVector(EdMode->UISettings->WorldSpacePatternBrushSettings.Origin, 0.0f)));
				const FVector2D Scale = FVector2D(
					ScaleXY / (EdMode->UISettings->WorldSpacePatternBrushSettings.RepeatSize * ((float)SizeX / SizeY)),
					ScaleXY / (EdMode->UISettings->WorldSpacePatternBrushSettings.RepeatSize));
				LocalOrigin *= Scale;
				Angle = -EdMode->UISettings->WorldSpacePatternBrushSettings.Rotation;
				if (EdMode->UISettings->WorldSpacePatternBrushSettings.bCenterTextureOnOrigin)
				{
					LocalOrigin += FVector2D(0.5f, 0.5f).GetRotated(-Angle);
				}
				AlphaScaleBias = FLinearColor(
					Scale.X,
					Scale.Y,
					LocalOrigin.X,
					LocalOrigin.Y);
			}
			else
			{
				AlphaScaleBias = FLinearColor(
					1.0f / (EdMode->UISettings->AlphaBrushScale * SizeX),
					1.0f / (EdMode->UISettings->AlphaBrushScale * SizeY),
					EdMode->UISettings->AlphaBrushPanU,
					EdMode->UISettings->AlphaBrushPanV);
				Angle = EdMode->UISettings->AlphaBrushRotation;
			}
			Angle = FMath::DegreesToRadians(Angle);

			FVector LandscapeLocation = Proxy->LandscapeActorToWorld().GetTranslation();
			FLinearColor LandscapeLocationParam(LandscapeLocation.X, LandscapeLocation.Y, LandscapeLocation.Z, Angle);

			int32 Channel = EdMode->UISettings->AlphaTextureChannel;
			FLinearColor AlphaTextureMask(Channel == 0 ? 1 : 0, Channel == 1 ? 1 : 0, Channel == 2 ? 1 : 0, Channel == 3 ? 1 : 0);

			for (const auto& BrushMaterialInstancePair : BrushMaterialInstanceMap)
			{
				UMaterialInstanceDynamic* const MaterialInstance = BrushMaterialInstancePair.Value;
				MaterialInstance->SetVectorParameterValue(FName(TEXT("AlphaScaleBias")),    AlphaScaleBias);
				MaterialInstance->SetVectorParameterValue(FName(TEXT("LandscapeLocation")), LandscapeLocationParam);
				MaterialInstance->SetVectorParameterValue(FName(TEXT("AlphaTextureMask")),  AlphaTextureMask);
				MaterialInstance->SetTextureParameterValue(FName(TEXT("AlphaTexture")),     EdMode->UISettings->AlphaTexture);
			}
		}
	}

	virtual const TCHAR* GetBrushName() override { return TEXT("Pattern"); }
	virtual FText GetDisplayName() override { return NSLOCTEXT("UnrealEd", "LandscapeMode_Brush_PatternAlpha", "Pattern Alpha"); };
};


//
// FLandscapeBrushAlpha
//
class FLandscapeBrushAlpha : public FLandscapeBrushAlphaBase
{
	float LastMouseAngle;
	FVector2D OldMousePosition;	// a previous mouse position, kept until we move a certain distance away, for smoothing deltas
	double LastMouseSampleTime;

protected:
	FLandscapeBrushAlpha(FEdModeLandscape* InEdMode, UMaterialInterface* InBrushMaterial)
		: FLandscapeBrushAlphaBase(InEdMode, InBrushMaterial)
		, LastMouseAngle(0.0f)
		, OldMousePosition(0.0f, 0.0f)
		, LastMouseSampleTime(FPlatformTime::Seconds())
	{
	}

public:
	static FLandscapeBrushAlpha* Create(FEdModeLandscape* InEdMode)
	{
		UMaterialInstanceConstant* AlphaBrushMaterial = LoadObject<UMaterialInstanceConstant>(nullptr, TEXT("/Engine/EditorLandscapeResources/AlphaBrushMaterial_Smooth.AlphaBrushMaterial_Smooth"), nullptr, LOAD_None, nullptr);
		return new FLandscapeBrushAlpha(InEdMode, AlphaBrushMaterial);
	}

	virtual FLandscapeBrushData ApplyBrush(const TArray<FLandscapeToolInteractorPosition>& InteractorPositions) override
	{
		ULandscapeInfo* LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo.Get();
		if (EdMode->UISettings->bAlphaBrushAutoRotate && OldMousePosition.IsZero())
		{
			OldMousePosition = LastMousePosition;
			LastMouseAngle = 0.0f;
			LastMouseSampleTime = FPlatformTime::Seconds();
			return FLandscapeBrushData();
		}
		else
		{
			float ScaleXY = FMath::Abs(LandscapeInfo->DrawScale.X);
			float Radius = EdMode->UISettings->BrushRadius / ScaleXY;
			int32 SizeX = EdMode->UISettings->AlphaTextureSizeX;
			int32 SizeY = EdMode->UISettings->AlphaTextureSizeY;
			float MaxSize = 2.0f * FMath::Sqrt(FMath::Square(Radius) / 2.0f);
			float AlphaBrushScale = MaxSize / (float)FMath::Max<int32>(SizeX, SizeY);
			const float BrushAngle = EdMode->UISettings->bAlphaBrushAutoRotate ? LastMouseAngle : FMath::DegreesToRadians(EdMode->UISettings->AlphaBrushRotation);

			FIntRect Bounds;
			Bounds.Min.X = FMath::FloorToInt(LastMousePosition.X - Radius);
			Bounds.Min.Y = FMath::FloorToInt(LastMousePosition.Y - Radius);
			Bounds.Max.X = FMath::CeilToInt( LastMousePosition.X + Radius);
			Bounds.Max.Y = FMath::CeilToInt( LastMousePosition.Y + Radius);


			// Clamp to landscape bounds
			int32 MinX, MaxX, MinY, MaxY;
			if (!ensure(LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY)))
			{
				// Landscape has no components somehow
				return FLandscapeBrushData();
			}
			Bounds.Clip(FIntRect(MinX, MinY, MaxX + 1, MaxY + 1));

			FLandscapeBrushData BrushData(Bounds);

			for (int32 Y = Bounds.Min.Y; Y < Bounds.Max.Y; Y++)
			{
				float* Scanline = BrushData.GetDataPtr(FIntPoint(0, Y));

				for (int32 X = Bounds.Min.X; X < Bounds.Max.X; X++)
				{
					// Find alphamap sample location
					float ScaleSampleX = ((float)X - LastMousePosition.X) / AlphaBrushScale;
					float ScaleSampleY = ((float)Y - LastMousePosition.Y) / AlphaBrushScale;

					// Rotate around center to match angle
					float SampleX = ScaleSampleX * FMath::Cos(BrushAngle) - ScaleSampleY * FMath::Sin(BrushAngle);
					float SampleY = ScaleSampleY * FMath::Cos(BrushAngle) + ScaleSampleX * FMath::Sin(BrushAngle);

					SampleX += (float)SizeX * 0.5f;
					SampleY += (float)SizeY * 0.5f;

					if (SampleX >= 0 && SampleX <= (SizeX - 1) &&
						SampleY >= 0 && SampleY <= (SizeY - 1))
					{
						// Sample the alpha texture
						float Alpha = GetAlphaSample(SampleX, SampleY);

						if (Alpha > 0.0f)
						{
							// Set the brush value for this vertex
							FIntPoint VertexKey = FIntPoint(X, Y);

							if (EdMode->CurrentTool && EdMode->CurrentTool->GetToolType() != ELandscapeToolType::Mask
								&& EdMode->UISettings->bUseSelectedRegion && LandscapeInfo->SelectedRegion.Num() > 0)
							{
								float MaskValue = LandscapeInfo->SelectedRegion.FindRef(FIntPoint(X, Y));
								if (EdMode->UISettings->bUseNegativeMask)
								{
									MaskValue = 1.0f - MaskValue;
								}
								Alpha *= MaskValue;
							}

							Scanline[X] = Alpha;
						}
					}
				}
			}

			return BrushData;
		}
	}

	virtual void MouseMove(float LandscapeX, float LandscapeY) override
	{
		FLandscapeBrushAlphaBase::MouseMove(LandscapeX, LandscapeY);

		if (EdMode->UISettings->bAlphaBrushAutoRotate)
		{
			// don't do anything with the angle unless we move at least 0.1 units.
			FVector2D MouseDelta = LastMousePosition - OldMousePosition;
			if (MouseDelta.SizeSquared() >= FMath::Square(0.5f))
			{
				double SampleTime = FPlatformTime::Seconds();
				float DeltaTime = (float)(SampleTime - LastMouseSampleTime);
				FVector2D MouseDirection = MouseDelta.GetSafeNormal();
				float MouseAngle = FMath::Lerp(LastMouseAngle, FMath::Atan2(-MouseDirection.Y, MouseDirection.X), FMath::Min<float>(10.0f * DeltaTime, 1.0f));		// lerp over 100ms
				LastMouseAngle = MouseAngle;
				LastMouseSampleTime = SampleTime;
				OldMousePosition = LastMousePosition;
				// UE_LOG(LogLandscape, Log, TEXT("(%f,%f) delta (%f,%f) angle %f"), LandscapeX, LandscapeY, MouseDirection.X, MouseDirection.Y, MouseAngle);
			}
		}
	}

	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override
	{
		FLandscapeBrushCircle::Tick(ViewportClient, DeltaTime);

		ULandscapeInfo* LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo.Get();
		if (LandscapeInfo)
		{
			float ScaleXY = FMath::Abs(LandscapeInfo->DrawScale.X);
			int32 SizeX = EdMode->UISettings->AlphaTextureSizeX;
			int32 SizeY = EdMode->UISettings->AlphaTextureSizeY;
			float Radius = EdMode->UISettings->BrushRadius / ScaleXY;
			float MaxSize = 2.0f * FMath::Sqrt(FMath::Square(Radius) / 2.0f);
			float AlphaBrushScale = MaxSize / (float)FMath::Max<int32>(SizeX, SizeY);

			FLinearColor BrushScaleRot(
				1.0f / (AlphaBrushScale * SizeX),
				1.0f / (AlphaBrushScale * SizeY),
				0.0f,
				EdMode->UISettings->bAlphaBrushAutoRotate ? LastMouseAngle : FMath::DegreesToRadians(EdMode->UISettings->AlphaBrushRotation)
				);

			int32 Channel = EdMode->UISettings->AlphaTextureChannel;
			FLinearColor AlphaTextureMask(Channel == 0 ? 1 : 0, Channel == 1 ? 1 : 0, Channel == 2 ? 1 : 0, Channel == 3 ? 1 : 0);

			for (const auto& BrushMaterialInstancePair : BrushMaterialInstanceMap)
			{
				UMaterialInstanceDynamic* const MaterialInstance = BrushMaterialInstancePair.Value;
				MaterialInstance->SetVectorParameterValue(FName(TEXT("BrushScaleRot")), BrushScaleRot);
				MaterialInstance->SetVectorParameterValue(FName(TEXT("AlphaTextureMask")), AlphaTextureMask);
				MaterialInstance->SetTextureParameterValue(FName(TEXT("AlphaTexture")), EdMode->UISettings->AlphaTexture);
			}
		}
	}

	virtual const TCHAR* GetBrushName() override { return TEXT("Alpha"); }
	virtual FText GetDisplayName() override { return NSLOCTEXT("UnrealEd", "LandscapeMode_Brush_Alpha", "Alpha"); };

};


void FEdModeLandscape::InitializeBrushes()
{
	FLandscapeBrushSet* BrushSet;
	BrushSet = new(LandscapeBrushSets)FLandscapeBrushSet(TEXT("BrushSet_Circle"));
	BrushSet->Brushes.Add(FLandscapeBrushCircle_Smooth::Create(this));
	BrushSet->Brushes.Add(FLandscapeBrushCircle_Linear::Create(this));
	BrushSet->Brushes.Add(FLandscapeBrushCircle_Spherical::Create(this));
	BrushSet->Brushes.Add(FLandscapeBrushCircle_Tip::Create(this));

	BrushSet = new(LandscapeBrushSets)FLandscapeBrushSet(TEXT("BrushSet_Alpha"));
	BrushSet->Brushes.Add(FLandscapeBrushAlpha::Create(this));

	BrushSet = new(LandscapeBrushSets)FLandscapeBrushSet(TEXT("BrushSet_Pattern"));
	BrushSet->Brushes.Add(FLandscapeBrushAlphaPattern::Create(this));

	BrushSet = new(LandscapeBrushSets)FLandscapeBrushSet(TEXT("BrushSet_Component"));
	BrushSet->Brushes.Add(new FLandscapeBrushComponent(this));

	BrushSet = new(LandscapeBrushSets)FLandscapeBrushSet(TEXT("BrushSet_Gizmo"));
	GizmoBrush = new FLandscapeBrushGizmo(this);
	BrushSet->Brushes.Add(GizmoBrush);

	BrushSet = new(LandscapeBrushSets)FLandscapeBrushSet(TEXT("BrushSet_Splines"));
	BrushSet->Brushes.Add(new FLandscapeBrushSplines(this));

	BrushSet = new(LandscapeBrushSets)FLandscapeBrushSet(TEXT("BrushSet_Dummy"));
	BrushSet->Brushes.Add(new FLandscapeBrushDummy(this));
}
