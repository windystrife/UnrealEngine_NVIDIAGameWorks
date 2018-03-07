#pragma once

// NvFlow begin

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "FlowRenderMaterial.generated.h"

UENUM()
enum EFlowRenderPreset
{
	EFRP_Default = 0 UMETA(DisplayName = "Default"),
	EFRP_Temperature = 1 UMETA(DisplayName = "Temperature"),
	EFRP_Fuel = 2 UMETA(DisplayName = "Fuel"),
	EFRP_Smoke = 3 UMETA(DisplayName = "Smoke"),
	EFRP_SmokeWithShadow = 4 UMETA(DisplayName = "Smoke with Shadow"),

	EFRP_MAX,
};

USTRUCT(BlueprintType)
struct FFlowRenderCompMask
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	float Temperature;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	float Fuel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	float Burn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "")
	float Smoke;
};

UCLASS(BlueprintType, hidecategories=object, MinimalAPI)
class UFlowRenderMaterial : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Global alpha scale for adjust net opacity without color map changes, applied after saturate(alpha) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (ClampMin = 0.0f, ClampMax = 1.0f))
	float AlphaScale;

	/** 1.0 makes material blend fully additive */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (ClampMin = 0.0f, ClampMax = 1.0f))
	float AdditiveFactor;

	/** Color curve */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Export, Category = "Rendering")
	class UCurveLinearColor* ColorMap;

	/** Color curve minimum X value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (UIMin = -1.0f, UIMax = 1.0f))
	float ColorMapMinX;

	/** Color curve maximum X value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (UIMin = -1.0f, UIMax = 1.0f))
	float ColorMapMaxX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	bool bUseRenderPreset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (EditCondition = "bUseRenderPreset"))
	TEnumAsByte<EFlowRenderPreset> RenderPreset;

	/** Component mask for colormap, control what channel drives color map X axis */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Export, Category = "Rendering", meta = (EditCondition = "!bUseRenderPreset"))
	FFlowRenderCompMask ColorMapCompMask;

	/** Component mask to control which channel(s) modulation the alpha */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Export, Category = "Rendering", meta = (EditCondition = "!bUseRenderPreset"))
	FFlowRenderCompMask AlphaCompMask;

	/** Component mask to control which channel(s) modulates the intensity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Export, Category = "Rendering", meta = (EditCondition = "!bUseRenderPreset"))
	FFlowRenderCompMask IntensityCompMask;

	/** Offsets alpha before saturate(alpha) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Export, Category = "Rendering", meta = (EditCondition = "!bUseRenderPreset"))
	float AlphaBias;

	/** Offsets intensity before modulating color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Export, Category = "Rendering", meta = (EditCondition = "!bUseRenderPreset"))
	float IntensityBias;

protected:

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	void SyncRenderPresetProperties();
};

// NvFlow end
