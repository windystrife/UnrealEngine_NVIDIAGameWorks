// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraRenderer.h"
#include "ParticleResources.h"
#include "ParticleBeamTrailVertexFactory.h"
#include "NiagaraDataSet.h"
#include "NiagaraStats.h"

DECLARE_CYCLE_STAT(TEXT("Generate Particle Lights"), STAT_NiagaraGenLights, STATGROUP_Niagara);


/** Enable/disable parallelized System renderers */
int32 GbNiagaraParallelEmitterRenderers = 1;
static FAutoConsoleVariableRef CVarParallelEmitterRenderers(
	TEXT("niagara.ParallelEmitterRenderers"),
	GbNiagaraParallelEmitterRenderers,
	TEXT("Whether to run Niagara System renderers in parallel"),
	ECVF_Default
	);




NiagaraRenderer::~NiagaraRenderer() 
{
}

void NiagaraRenderer::Release()
{
	check(IsInGameThread());
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		NiagaraRendererDeletion,
		NiagaraRenderer*, Renderer, this,
		{
			delete Renderer;
		}
	);
}

	


NiagaraRendererLights::NiagaraRendererLights(ERHIFeatureLevel::Type FeatureLevel, UNiagaraRendererProperties *InProps) :
	NiagaraRenderer()
{
	Properties = Cast<UNiagaraLightRendererProperties>(InProps);
}


void NiagaraRendererLights::ReleaseRenderThreadResources()
{
}

void NiagaraRendererLights::CreateRenderThreadResources()
{
}



/** Update render data buffer from attributes */
FNiagaraDynamicDataBase *NiagaraRendererLights::GenerateVertexData(const FNiagaraSceneProxy* Proxy, FNiagaraDataSet &Data, const ENiagaraSimTarget Target)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraGenLights);

	SimpleTimer VertexDataTimer;

	//I'm not a great fan of pulling scalar components out to a structured vert buffer like this.
	//TODO: Experiment with a new VF that reads the data directly from the scalar layout.
	FNiagaraDataSetIterator<FVector> PosItr(Data, FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
	FNiagaraDataSetIterator<FLinearColor> ColItr(Data, FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), TEXT("Color")));
	FNiagaraDataSetIterator<FVector2D> SizeItr(Data, FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), TEXT("Size")));

	//Bail if we don't have the required attributes to render this emitter.
	if (!PosItr.IsValid() || !ColItr.IsValid() || !SizeItr.IsValid() || !bEnabled)
	{
		return nullptr;
	}

	FNiagaraDynamicDataLights *DynamicData = new FNiagaraDynamicDataLights;

	DynamicData->LightArray.Empty();

	for (uint32 ParticleIndex = 0; ParticleIndex < Data.GetNumInstances(); ParticleIndex++)
	{
		SimpleLightData LightData;
		LightData.LightEntry.Radius = (*SizeItr).X;	//LightPayload->RadiusScale * (Size.X + Size.Y) / 2.0f;
		LightData.LightEntry.Color = FVector((*ColItr));				//FVector(Particle.Color) * Particle.Color.A * LightPayload->ColorScale;
		LightData.LightEntry.Exponent = 1.0;
		LightData.LightEntry.bAffectTranslucency = true;
		LightData.PerViewEntry.Position = (*PosItr);

		DynamicData->LightArray.Add(LightData);

		PosItr.Advance();
		ColItr.Advance();
		SizeItr.Advance();
	}

	CPUTimeMS = VertexDataTimer.GetElapsedMilliseconds();

	return DynamicData;
}


void NiagaraRendererLights::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const
{

}

void NiagaraRendererLights::SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData)
{
	if (DynamicDataRender)
	{
		delete static_cast<FNiagaraDynamicDataLights*>(DynamicDataRender);
		DynamicDataRender = NULL;
	}
	DynamicDataRender = static_cast<FNiagaraDynamicDataLights*>(NewDynamicData);
}

int NiagaraRendererLights::GetDynamicDataSize()
{
	return 0;
}
bool NiagaraRendererLights::HasDynamicData()
{
	return false;
}

bool NiagaraRendererLights::SetMaterialUsage()
{
	return false;
}

#if WITH_EDITORONLY_DATA

const TArray<FNiagaraVariable>& NiagaraRendererLights::GetRequiredAttributes()
{
	return Properties->GetRequiredAttributes();
}

const TArray<FNiagaraVariable>& NiagaraRendererLights::GetOptionalAttributes()
{
	return Properties->GetOptionalAttributes();
}

#endif












