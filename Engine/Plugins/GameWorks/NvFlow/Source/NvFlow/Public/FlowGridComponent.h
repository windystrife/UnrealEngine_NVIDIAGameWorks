
/**
 *	NvFlow - Grid Component
 */
#pragma once

// NvFlow begin

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/PrimitiveComponent.h"

#include "FlowTimeStepper.h"

#include "FlowGridSceneProxy.h"
#include "FlowGridComponent.generated.h"

#if STATS

DECLARE_STATS_GROUP(TEXT("Flow"), STATGROUP_Flow, STATCAT_Advanced);

enum EFlowStats
{
	// Container stats
	STAT_Flow_Tick,
	STAT_Flow_UpdateShapes,
	STAT_Flow_UpdateColorMap,
	STAT_Flow_SimulateGrids,
	STAT_Flow_RenderGrids,
	STAT_Flow_GridCount,
	STAT_Flow_EmitterCount,
	STAT_Flow_ColliderCount,
};

DECLARE_CYCLE_STAT_EXTERN(TEXT("RenderThread, Simulate Grids"), STAT_Flow_SimulateGrids, STATGROUP_Flow, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("RenderThread, Render Grids"), STAT_Flow_RenderGrids, STATGROUP_Flow, );

#endif

UCLASS(ClassGroup = Physics, config = Engine, editinlinenew, HideCategories = (Physics, Collision, Activation, PhysX), meta = (BlueprintSpawnableComponent), MinimalAPI)
class UFlowGridComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	/** The flow grid asset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Grid)
	class UFlowGridAsset* FlowGridAsset;

	UFUNCTION(BlueprintCallable, Category=Grid)
	class UFlowGridAsset* CreateOverrideAsset();

	UFUNCTION(BlueprintCallable, Category = Grid)
	void SetOverrideAsset(class UFlowGridAsset* asset);

	UPROPERTY(Transient)
	class UFlowGridAsset* FlowGridAssetOverride;

	class UFlowGridAsset** FlowGridAssetCurrent;
	class UFlowGridAsset* FlowGridAssetOld;

	/** Default flow material. */
	UPROPERTY(NoClear, EditAnywhere, BlueprintReadWrite, Category = Grid)
	class UFlowMaterial* DefaultFlowMaterial;

	UFUNCTION(BlueprintCallable, Category = Grid)
	class UFlowMaterial* CreateOverrideMaterial(class UFlowMaterial* materialToDuplicate);

	UFUNCTION(BlueprintCallable, Category = Grid)
	void SetOverrideMaterial(class UFlowMaterial* materialToOverride, class UFlowMaterial* overrideMaterial);

	/** If true, Flow Grid will collide with emitter/colliders. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Grid)
	uint32 bFlowGridCollisionEnabled : 1;

	// UObject interface
	virtual void OnCreatePhysicsState() override;
	virtual void OnDestroyPhysicsState() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

#if WITH_EDITOR	
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
#endif
	// End of UObject interface

	// Begin UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// End UActorComponent interface

	// USceneComponent interface
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None) override;

	// Begin UPrimitiveComponent interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	// End UPrimitiveComponent interface.

	virtual ~UFlowGridComponent();

	FFlowGridProperties* FlowGridProperties;
	TArray<FFlowGridProperties*> FlowGridPropertiesPool;
protected:

	static void InitializeGridProperties(FFlowGridProperties* FlowGridProperties);

	// Begin UActorComponent Interface
	virtual FBoxSphereBounds CalcBounds(const FTransform & LocalToWorld) const override;
	virtual void SendRenderTransform_Concurrent() override;
	virtual void SendRenderDynamicData_Concurrent() override;
	// End UActorComponent Interface

	void ResetShapes();
	void UpdateShapes(float DeltaTime, uint32 NumSimSubSteps);
	FlowMaterialKeyType AddMaterialParams(class UFlowMaterial* FlowMaterial);

	uint64 VersionCounter = 0ul;
	uint64 LastVersionPushed = 0ul;

	FFlowTimeStepper TimeStepper;

	int32 GridEmitParams_Num_Old = 0;
	int32 GridCollideParams_Num_Old = 0;

	struct MaterialData
	{
		bool bUpdated = false;
		class UFlowMaterial* OverrideMaterial = nullptr;
	};

	TMap<class UFlowMaterial*, MaterialData> MaterialsMap;

	TMap<const class UStaticMesh*, const class FDistanceFieldVolumeData*> DistanceFieldMap;

public:
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
};

// NvFlow end
