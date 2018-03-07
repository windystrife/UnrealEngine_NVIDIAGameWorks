#include "FlowRenderMaterial.h"
#include "NvFlowCommon.h"

#include "Curves/CurveLinearColor.h"

// NvFlow begin

namespace
{
	inline void AddColorMapPoint(UCurveLinearColor* ColorMap, float Time, FLinearColor Color)
	{
		ColorMap->FloatCurves[0].AddKey(Time, Color.R);
		ColorMap->FloatCurves[1].AddKey(Time, Color.G);
		ColorMap->FloatCurves[2].AddKey(Time, Color.B);
		ColorMap->FloatCurves[3].AddKey(Time, Color.A);
	}

	inline void CopyRenderCompMask(const NvFlowFloat4& In, FFlowRenderCompMask& Out)
	{
		Out.Temperature = In.x;
		Out.Fuel = In.y;
		Out.Burn = In.z;
		Out.Smoke = In.w;
	}
	inline void SetRenderCompMask(float T, float F, float B, float S, FFlowRenderCompMask& Out)
	{
		Out.Temperature = T;
		Out.Fuel = F;
		Out.Burn = B;
		Out.Smoke = S;
	}
}

UFlowRenderMaterial::UFlowRenderMaterial(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NvFlowRenderMaterialParams FlowRenderMaterialParams;
	NvFlowRenderMaterialParamsDefaultsInline(&FlowRenderMaterialParams);

	AlphaScale = FlowRenderMaterialParams.alphaScale;
	AdditiveFactor = FlowRenderMaterialParams.additiveFactor;

	ColorMap = CreateDefaultSubobject<UCurveLinearColor>(TEXT("DefaultColorMap0"));
	AddColorMapPoint(ColorMap, 0.f, FLinearColor(0.0f, 0.f, 0.f, 0.f));
	AddColorMapPoint(ColorMap, 0.1f, FLinearColor(0.f, 0.f, 0.f, 0.25f));
	AddColorMapPoint(ColorMap, 0.6f, FLinearColor(1.f * 213.f / 255.f, 1.f * 100.f / 255.f, 1.f * 30.f / 255.f, 0.8f));
	AddColorMapPoint(ColorMap, 0.75f, FLinearColor(2.f * 1.27f, 2.f * 1.20f, 1.f * 0.39f, 0.8f));
	AddColorMapPoint(ColorMap, 0.85f, FLinearColor(4.f * 1.27f, 4.f * 1.20f, 1.f * 0.39f, 0.8f));
	AddColorMapPoint(ColorMap, 1.f, FLinearColor(8.0f, 8.0f, 8.0f, 0.7f));

#if WITH_EDITORONLY_DATA
	// invalidate
	ColorMap->AssetImportData = nullptr;
#endif

	ColorMapMinX = 0.f;
	ColorMapMaxX = 1.f;

	bUseRenderPreset = true;
	RenderPreset = EFRP_Default;

	SyncRenderPresetProperties();
}

#if WITH_EDITOR
void UFlowRenderMaterial::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static bool IsReentrant = false;

	if (!IsReentrant)
	{
		IsReentrant = true;

		FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UFlowRenderMaterial, bUseRenderPreset))
		{
			if (bUseRenderPreset)
			{
				SyncRenderPresetProperties();
			}
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(UFlowRenderMaterial, RenderPreset))
		{
			SyncRenderPresetProperties();
		}

		IsReentrant = false;
	}
}
#endif

void UFlowRenderMaterial::SyncRenderPresetProperties()
{
	switch (RenderPreset)
	{
		case EFRP_Default:
		{
			NvFlowRenderMaterialParams FlowRenderMaterialParams;
			NvFlowRenderMaterialParamsDefaultsInline(&FlowRenderMaterialParams);

			CopyRenderCompMask(FlowRenderMaterialParams.colorMapCompMask, ColorMapCompMask);
			CopyRenderCompMask(FlowRenderMaterialParams.alphaCompMask, AlphaCompMask);
			CopyRenderCompMask(FlowRenderMaterialParams.intensityCompMask, IntensityCompMask);

			AlphaBias = FlowRenderMaterialParams.alphaBias;
			IntensityBias = FlowRenderMaterialParams.intensityBias;

			break;
		}
		case EFRP_Temperature:
		{
			SetRenderCompMask(1.0f, 0.0f, 0.0f, 0.0f, ColorMapCompMask);
			SetRenderCompMask(0.0f, 0.0f, 0.0f, 0.0f, AlphaCompMask);
			SetRenderCompMask(0.0f, 0.0f, 0.0f, 0.0f, IntensityCompMask);

			AlphaBias = 1.0f;
			IntensityBias = 1.0f;

			break;
		}
		case EFRP_Fuel:
		{
			SetRenderCompMask(0.0f, 1.0f, 0.0f, 0.0f, ColorMapCompMask);
			SetRenderCompMask(0.0f, 0.0f, 0.0f, 0.0f, AlphaCompMask);
			SetRenderCompMask(0.0f, 0.0f, 0.0f, 0.0f, IntensityCompMask);

			AlphaBias = 1.0f;
			IntensityBias = 1.0f;

			break;
		}
		case EFRP_Smoke:
		{
			SetRenderCompMask(0.0f, 0.0f, 0.0f, 1.0f, ColorMapCompMask);
			SetRenderCompMask(0.0f, 0.0f, 0.0f, 0.0f, AlphaCompMask);
			SetRenderCompMask(0.0f, 0.0f, 0.0f, 0.0f, IntensityCompMask);

			AlphaBias = 1.0f;
			IntensityBias = 1.0f;

			break;
		}
		case EFRP_SmokeWithShadow:
		{
			SetRenderCompMask(0.0f, 0.0f, 0.0f, 1.0f, ColorMapCompMask);
			SetRenderCompMask(0.0f, 0.0f, 0.0f, 0.0f, AlphaCompMask);
			SetRenderCompMask(0.0f, 0.0f, 1.0f, 0.0f, IntensityCompMask);

			AlphaBias = 1.0f;
			IntensityBias = 0.0f;

			break;
		}
	}
}

// NvFlow end
