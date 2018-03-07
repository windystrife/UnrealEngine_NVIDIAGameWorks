// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/GCObject.h"
#include "UnrealWidget.h"
#include "EdMode.h"

class FEditorViewportClient;
class FPrimitiveDrawInterface;
class FSceneView;
class FViewport;
class ULandscapeInfo;
class ULandscapeLayerInfoObject;
class UMaterialInstance;
class UMaterialInterface;
class UViewportInteractor;
struct FViewportClick;

// FLandscapeToolMousePosition - Struct to store mouse positions since the last time we applied the brush
struct FLandscapeToolInteractorPosition
{
	// Stored in heightmap space.
	FVector2D Position;
	bool bModifierPressed;

	FLandscapeToolInteractorPosition(FVector2D InPosition, const bool bInModifierPressed)
		: Position(InPosition)
		, bModifierPressed(bInModifierPressed)
	{
	}
};

enum class ELandscapeBrushType
{
	Normal = 0,
	Alpha,
	Component,
	Gizmo,
	Splines
};

class FLandscapeBrushData
{
protected:
	FIntRect Bounds;
	TArray<float> BrushAlpha;

public:
	FLandscapeBrushData()
		: Bounds()
	{
	}

	FLandscapeBrushData(FIntRect InBounds)
		: Bounds(InBounds)
	{
		BrushAlpha.SetNumZeroed(Bounds.Area());
	}

	FIntRect GetBounds() const
	{
		return Bounds;
	}

	// For compatibility with older landscape code that uses inclusive bounds in 4 int32s
	void GetInclusiveBounds(int32& X1, int32& Y1, int32& X2, int32& Y2) const
	{
		X1 = Bounds.Min.X;
		Y1 = Bounds.Min.Y;
		X2 = Bounds.Max.X - 1;
		Y2 = Bounds.Max.Y - 1;
	}

	float* GetDataPtr(FIntPoint Position)
	{
		return BrushAlpha.GetData() + (Position.Y - Bounds.Min.Y) * Bounds.Width() + (Position.X - Bounds.Min.X);
	}
	const float* GetDataPtr(FIntPoint Position) const
	{
		return BrushAlpha.GetData() + (Position.Y - Bounds.Min.Y) * Bounds.Width() + (Position.X - Bounds.Min.X);
	}

	FORCEINLINE explicit operator bool() const
	{
		return BrushAlpha.Num() != 0;
	}

	FORCEINLINE bool operator!() const
	{
		return !(bool)*this;
	}
};

class FLandscapeBrush : public FGCObject
{
public:
	virtual void MouseMove(float LandscapeX, float LandscapeY) = 0;
	virtual FLandscapeBrushData ApplyBrush(const TArray<FLandscapeToolInteractorPosition>& InteractorPositions) = 0;
	virtual TOptional<bool> InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent) { return TOptional<bool>(); }
	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) {};
	virtual void BeginStroke(float LandscapeX, float LandscapeY, class FLandscapeTool* CurrentTool);
	virtual void EndStroke();
	virtual void EnterBrush() {}
	virtual void LeaveBrush() {}
	virtual ~FLandscapeBrush() {}
	virtual UMaterialInterface* GetBrushMaterial() { return NULL; }
	virtual const TCHAR* GetBrushName() = 0;
	virtual FText GetDisplayName() = 0;
	virtual ELandscapeBrushType GetBrushType() { return ELandscapeBrushType::Normal; }

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override {}
};

struct FLandscapeBrushSet
{
	FLandscapeBrushSet(const TCHAR* InBrushSetName)
		: BrushSetName(InBrushSetName)
		, PreviousBrushIndex(0)
	{
	}

	const FName BrushSetName;
	TArray<FLandscapeBrush*> Brushes;
	int32 PreviousBrushIndex;

	virtual ~FLandscapeBrushSet()
	{
		for (int32 BrushIdx = 0; BrushIdx < Brushes.Num(); BrushIdx++)
		{
			delete Brushes[BrushIdx];
		}
	}
};

namespace ELandscapeToolTargetType
{
	enum Type : int8
	{
		Heightmap  = 0,
		Weightmap  = 1,
		Visibility = 2,

		Invalid    = -1, // only valid for LandscapeEdMode->CurrentToolTarget.TargetType
	};
}

namespace ELandscapeToolTargetTypeMask
{
	enum Type : uint8
	{
		Heightmap  = 1 << ELandscapeToolTargetType::Heightmap,
		Weightmap  = 1 << ELandscapeToolTargetType::Weightmap,
		Visibility = 1 << ELandscapeToolTargetType::Visibility,

		NA = 0,
		All = 0xFF,
	};

	inline ELandscapeToolTargetTypeMask::Type FromType(ELandscapeToolTargetType::Type TargetType)
	{
		if (TargetType == ELandscapeToolTargetType::Invalid)
		{
			return ELandscapeToolTargetTypeMask::NA;
		}
		return (ELandscapeToolTargetTypeMask::Type)(1 << TargetType);
	}
}

struct FLandscapeToolTarget
{
	TWeakObjectPtr<ULandscapeInfo> LandscapeInfo;
	ELandscapeToolTargetType::Type TargetType;
	TWeakObjectPtr<ULandscapeLayerInfoObject> LayerInfo;
	FName LayerName;

	FLandscapeToolTarget()
		: LandscapeInfo()
		, TargetType(ELandscapeToolTargetType::Heightmap)
		, LayerInfo()
		, LayerName(NAME_None)
	{
	}
};

enum class ELandscapeToolType
{
	Normal = 0,
	Mask,
};

/**
 * FLandscapeTool
 */
class FLandscapeTool : public FGCObject
{
public:
	virtual void EnterTool() {}
	virtual bool IsToolActive() const { return false;  }
	virtual void ExitTool() {}
	virtual bool BeginTool(FEditorViewportClient* ViewportClient, const FLandscapeToolTarget& Target, const FVector& InHitLocation) = 0;
	virtual void EndTool(FEditorViewportClient* ViewportClient) = 0;
	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) {};
	virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y) = 0;
	virtual bool HandleClick(HHitProxy* HitProxy, const FViewportClick& Click) { return false; }
	virtual bool InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent) { return false; }
	virtual bool InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale) { return false; }
	virtual bool GetCursor(EMouseCursor::Type& OutCursor) const { return false;  }

	FLandscapeTool() : PreviousBrushIndex(-1) {}
	virtual ~FLandscapeTool() {}
	virtual const TCHAR* GetToolName() = 0;
	virtual FText GetDisplayName() = 0;
	virtual void SetEditRenderType();
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) {}
	virtual bool SupportsMask() { return true; }
	virtual bool SupportsComponentSelection() { return false; }
	virtual bool OverrideSelection() const { return false; }
	virtual bool IsSelectionAllowed(AActor* InActor, bool bInSelection) const { return false; }
	virtual bool UsesTransformWidget() const { return false; }
	virtual EAxisList::Type GetWidgetAxisToDraw(FWidget::EWidgetMode InWidgetMode) const { return EAxisList::All; }
	virtual FVector GetWidgetLocation() const { return FVector::ZeroVector; }
	virtual FMatrix GetWidgetRotation() const { return FMatrix::Identity; }
	virtual bool DisallowMouseDeltaTracking() const { return false; }

	virtual void SetCanToolBeActivated(bool Value) { }
	virtual bool CanToolBeActivated() const { return true;  }
	virtual void SetExternalModifierPressed(const bool bPressed) {};

	virtual EEditAction::Type GetActionEditDuplicate() { return EEditAction::Skip; }
	virtual EEditAction::Type GetActionEditDelete() { return EEditAction::Skip; }
	virtual EEditAction::Type GetActionEditCut() { return EEditAction::Skip; }
	virtual EEditAction::Type GetActionEditCopy() { return EEditAction::Skip; }
	virtual EEditAction::Type GetActionEditPaste() { return EEditAction::Skip; }
	virtual bool ProcessEditDuplicate() { return false; }
	virtual bool ProcessEditDelete() { return false; }
	virtual bool ProcessEditCut() { return false; }
	virtual bool ProcessEditCopy() { return false; }
	virtual bool ProcessEditPaste() { return false; }

	// Functions which doesn't need Viewport data...
	virtual void Process(int32 Index, int32 Arg) {}
	virtual ELandscapeToolType GetToolType() { return ELandscapeToolType::Normal; }
	virtual ELandscapeToolTargetTypeMask::Type GetSupportedTargetTypes() { return ELandscapeToolTargetTypeMask::NA; };

	// FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override {}

public:
	int32					PreviousBrushIndex;
	TArray<FName>			ValidBrushes;
};

namespace LandscapeTool
{
	UMaterialInstance* CreateMaterialInstance(UMaterialInterface* BaseMaterial);
}
