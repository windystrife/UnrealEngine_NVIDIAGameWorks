// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraRenderer.h: Base class for Niagara render modules
==============================================================================*/
#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"
#include "Materials/MaterialInterface.h"
#include "UniformBuffer.h"
#include "Materials/Material.h"
#include "PrimitiveViewRelevance.h"
#include "ParticleHelper.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraLightRendererProperties.h"
#include "RenderingThread.h"
#include "SceneView.h"
#include "NiagaraComponent.h"

class FNiagaraDataSet;

/** Struct used to pass dynamic data from game thread to render thread */
struct FNiagaraDynamicDataBase
{
};



class SimpleTimer
{
public:
	SimpleTimer()
	{
		StartTime = FPlatformTime::Seconds() * 1000.0;
	}

	double GetElapsedMilliseconds()
	{
		return (FPlatformTime::Seconds()*1000.0) - StartTime;
	}

	~SimpleTimer()
	{
	}

private:
	double StartTime;
};


/**
* Base class for Niagara System renderers. System renderers handle generating vertex data for and
* drawing of simulation data coming out of FNiagaraEmitterInstance instances.
*/
class NiagaraRenderer 
{
public:
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const = 0;

	virtual void SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData) = 0;
	virtual void CreateRenderThreadResources() = 0;
	virtual void ReleaseRenderThreadResources() = 0;
	virtual FNiagaraDynamicDataBase *GenerateVertexData(const FNiagaraSceneProxy* Proxy, FNiagaraDataSet &Data, const ENiagaraSimTarget Target) = 0;
	virtual int GetDynamicDataSize() = 0;

	virtual bool HasDynamicData() = 0;

	virtual bool SetMaterialUsage() = 0;

#if WITH_EDITORONLY_DATA
	virtual const TArray<FNiagaraVariable>& GetRequiredAttributes() = 0;
	virtual const TArray<FNiagaraVariable>& GetOptionalAttributes() = 0;
#endif

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View, const FNiagaraSceneProxy *SceneProxy)
	{
		FPrimitiveViewRelevance Result;
		bool bHasDynamicData = HasDynamicData();

		Result.bDrawRelevance = bHasDynamicData && SceneProxy->IsShown(View) && View->Family->EngineShowFlags.Particles;
		Result.bShadowRelevance = bHasDynamicData && SceneProxy->IsShadowCast(View);
		Result.bDynamicRelevance = bHasDynamicData;
		if (bHasDynamicData && View->Family->EngineShowFlags.Bounds)
		{
			Result.bOpaqueRelevance = true;
		}
		MaterialRelevance.SetPrimitiveViewRelevance(Result);

		return Result;
	}


	UMaterialInterface *GetMaterial()	{ return Material; }
	void SetMaterial(UMaterialInterface *InMaterial, ERHIFeatureLevel::Type FeatureLevel)
	{
		Material = InMaterial;
		if (Material->GetMaterialResource(FeatureLevel))
		{
			if (!Material || !SetMaterialUsage())
			{
				Material = UMaterial::GetDefaultMaterial(MD_Surface);
			}
			check(Material);
			MaterialRelevance = Material->GetRelevance(FeatureLevel);
		}
	}
	
	virtual UClass *GetPropertiesClass() = 0;
	virtual void SetRendererProperties(UNiagaraRendererProperties *Props) = 0;

	float GetCPUTimeMS() { return CPUTimeMS; }

	void SetLocalSpace(bool bInLocalSpace) { bLocalSpace = bInLocalSpace; }

	/** Release enqueues the System renderer to be killed on the render thread safely.*/
	void Release();

	FNiagaraDynamicDataBase *GetDynamicData() { return DynamicDataRender; }

	bool IsEnabled() const {return bEnabled;}

	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

protected:
	NiagaraRenderer()
		: CPUTimeMS(0.0f)
		, bLocalSpace(false)
		, bEnabled(true)
		, DynamicDataRender(nullptr)
	{
		Material = UMaterial::GetDefaultMaterial(MD_Surface);
	}

	virtual ~NiagaraRenderer();

	mutable float CPUTimeMS;
	UMaterialInterface* Material;
	bool bLocalSpace;
	bool bEnabled;

	FMaterialRelevance MaterialRelevance;

	struct FNiagaraDynamicDataBase *DynamicDataRender;
};




/**
* NiagaraRendererSprites renders an FNiagaraEmitterInstance as sprite particles
*/
class NIAGARA_API NiagaraRendererSprites : public NiagaraRenderer
{
public:

	explicit NiagaraRendererSprites(ERHIFeatureLevel::Type FeatureLevel, UNiagaraRendererProperties *Props);
	~NiagaraRendererSprites()
	{
		ReleaseRenderThreadResources();
	}


	virtual void ReleaseRenderThreadResources() override;

	// FPrimitiveSceneProxy interface.
	virtual void CreateRenderThreadResources() override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const override;
	virtual bool SetMaterialUsage() override;
	/** Update render data buffer from attributes */
	FNiagaraDynamicDataBase *GenerateVertexData(const FNiagaraSceneProxy* Proxy, FNiagaraDataSet &Data, const ENiagaraSimTarget Target) override;

	virtual void SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData) override;
	int GetDynamicDataSize() override;
	bool HasDynamicData() override;

	UClass *GetPropertiesClass() override { return UNiagaraSpriteRendererProperties::StaticClass(); }
	void SetRendererProperties(UNiagaraRendererProperties *Props) override { Properties = Cast<UNiagaraSpriteRendererProperties>(Props); }

#if WITH_EDITORONLY_DATA
	virtual const TArray<FNiagaraVariable>& GetRequiredAttributes() override;
	virtual const TArray<FNiagaraVariable>& GetOptionalAttributes() override;
#endif

private:
	UNiagaraSpriteRendererProperties *Properties;
	mutable TUniformBuffer<FPrimitiveUniformShaderParameters> WorldSpacePrimitiveUniformBuffer;
	class FNiagaraSpriteVertexFactory* VertexFactory;
};


/**
* NiagaraRendererLights renders an FNiagaraEmitterInstance as simple lights
*/
class NIAGARA_API NiagaraRendererLights : public NiagaraRenderer
{
public:
	struct SimpleLightData
	{
		FSimpleLightEntry LightEntry;
		FSimpleLightPerViewEntry PerViewEntry;
	};
	explicit NiagaraRendererLights(ERHIFeatureLevel::Type FeatureLevel, UNiagaraRendererProperties *Props);

	~NiagaraRendererLights()
	{
		ReleaseRenderThreadResources();
	}


	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View, const FNiagaraSceneProxy *SceneProxy) override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = false;
		Result.bShadowRelevance = false;
		Result.bDynamicRelevance = false;
		Result.bOpaqueRelevance = false;
		Result.bHasSimpleLights = true;
		//MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}


	virtual void ReleaseRenderThreadResources() override;

	// FPrimitiveSceneProxy interface.
	virtual void CreateRenderThreadResources() override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector, const FNiagaraSceneProxy *SceneProxy) const override;
	virtual bool SetMaterialUsage() override;
	/** Update render data buffer from attributes */
	FNiagaraDynamicDataBase *GenerateVertexData(const FNiagaraSceneProxy* Proxy, FNiagaraDataSet &Data, const ENiagaraSimTarget Target) override;

	virtual void SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData) override;
	int GetDynamicDataSize() override;
	bool HasDynamicData() override;

	UClass *GetPropertiesClass() override { return UNiagaraLightRendererProperties::StaticClass(); }
	void SetRendererProperties(UNiagaraRendererProperties *Props) override { Properties = Cast<UNiagaraLightRendererProperties>(Props); }
#if WITH_EDITORONLY_DATA
	virtual const TArray<FNiagaraVariable>& GetRequiredAttributes() override;
	virtual const TArray<FNiagaraVariable>& GetOptionalAttributes() override;
#endif

	TArray<SimpleLightData> &GetLights() { return LightArray; }
private:
	UNiagaraLightRendererProperties *Properties;
	TArray<SimpleLightData>LightArray;
};


struct FNiagaraDynamicDataLights : public FNiagaraDynamicDataBase
{
	TArray<NiagaraRendererLights::SimpleLightData> LightArray;
};






