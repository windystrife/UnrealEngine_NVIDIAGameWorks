// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraComponent.h"
#include "VectorVM.h"
#include "NiagaraRenderer.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitterInstance.h"
#include "MeshBatch.h"
#include "SceneUtils.h"
#include "ComponentReregisterContext.h"
#include "NiagaraConstants.h"
#include "NiagaraStats.h"
#include "NiagaraCommon.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceStaticMesh.h"
#include "NameTypes.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraWorldManager.h"

DECLARE_CYCLE_STAT(TEXT("Gen Verts"),STAT_NiagaraGenerateVertices,STATGROUP_Niagara);

DEFINE_LOG_CATEGORY(LogNiagara);

FNiagaraSceneProxy::FNiagaraSceneProxy(const UNiagaraComponent* InComponent)
		: FPrimitiveSceneProxy(InComponent)
		, bRenderingEnabled(true)
{
	// In this case only, update the System renderers on the game thread.
	check(IsInGameThread());
	FNiagaraSystemInstance* SystemInst = InComponent->GetSystemInstance();
	if (SystemInst)
	{
		TArray<TSharedRef<FNiagaraEmitterInstance> > Sims = SystemInst->GetEmitters();
		TArray<NiagaraRenderer*> RenderersFromSims;
		for (TSharedRef<FNiagaraEmitterInstance> Sim : Sims)
		{
			for (int32 i = 0; i < Sim->GetEmitterRendererNum(); i++)
			{
				RenderersFromSims.Add(Sim->GetEmitterRenderer(i));
			}
		}
		UpdateEmitterRenderers(RenderersFromSims);
		//UE_LOG(LogNiagara, Warning, TEXT("FNiagaraSceneProxy %p"), this);

		bAlwaysHasVelocity = true;
	}
}

void FNiagaraSceneProxy::AddEmitterRenderer(NiagaraRenderer* Renderer)
{
	EmitterRenderers.Add(Renderer); 
	//UE_LOG(LogNiagara, Warning, TEXT("FNiagaraSceneProxy::AddEmitterRenderer %p"), Renderer);
}

void FNiagaraSceneProxy::UpdateEmitterRenderers(TArray<NiagaraRenderer*>& InRenderers)
{
	EmitterRenderers.Empty();
	for (NiagaraRenderer* EmitterRenderer : InRenderers)
	{
		AddEmitterRenderer(EmitterRenderer);
	}
}

FNiagaraSceneProxy::~FNiagaraSceneProxy()
{
	//UE_LOG(LogNiagara, Warning, TEXT("~FNiagaraSceneProxy %p"), this);
	ReleaseRenderThreadResources();
}

/** Called on render thread to assign new dynamic data */
void FNiagaraSceneProxy::SetDynamicData_RenderThread(FNiagaraDynamicDataBase* NewDynamicData)
{
	for (NiagaraRenderer* Renderer : EmitterRenderers)
	{
		if (Renderer)
		{
			Renderer->SetDynamicData_RenderThread(NewDynamicData);
		}
	}
	return;
}


void FNiagaraSceneProxy::ReleaseRenderThreadResources()
{
	for (NiagaraRenderer* Renderer : EmitterRenderers)
	{
		if (Renderer)
		{
			Renderer->ReleaseRenderThreadResources();
		}
	}
	return;
}

// FPrimitiveSceneProxy interface.
void FNiagaraSceneProxy::CreateRenderThreadResources()
{
	for (NiagaraRenderer* Renderer : EmitterRenderers)
	{
		if (Renderer)
		{
			Renderer->CreateRenderThreadResources();
		}
	}
	return;
}

void FNiagaraSceneProxy::OnTransformChanged()
{
	//WorldSpacePrimitiveUniformBuffer.ReleaseResource();
}

FPrimitiveViewRelevance FNiagaraSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Relevance;

	if (bRenderingEnabled == false)
	{
		return Relevance;
	}
	Relevance.bDynamicRelevance = true;

	for (NiagaraRenderer* Renderer : EmitterRenderers)
	{
		if (Renderer)
		{
			Relevance |= Renderer->GetViewRelevance(View, this);
		}
	}
	return Relevance;
}


uint32 FNiagaraSceneProxy::GetMemoryFootprint() const
{ 
	return (sizeof(*this) + GetAllocatedSize()); 
}

uint32 FNiagaraSceneProxy::GetAllocatedSize() const
{ 
	uint32 DynamicDataSize = 0;
	for (NiagaraRenderer* Renderer : EmitterRenderers)
	{
		if (Renderer)
		{
			DynamicDataSize += Renderer->GetDynamicDataSize();
		}
	}
	return FPrimitiveSceneProxy::GetAllocatedSize() + DynamicDataSize;
}

bool FNiagaraSceneProxy::GetRenderingEnabled() const
{
	return bRenderingEnabled;
}

void FNiagaraSceneProxy::SetRenderingEnabled(bool bInRenderingEnabled)
{
	bRenderingEnabled = bInRenderingEnabled;
}

void FNiagaraSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	for (NiagaraRenderer* Renderer : EmitterRenderers)
	{
		if (Renderer)
		{
			Renderer->GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector, this);
		}
	}
}



void FNiagaraSceneProxy::GatherSimpleLights(const FSceneViewFamily& ViewFamily, FSimpleLightArray& OutParticleLights) const
{
	NiagaraRendererLights *LightRenderer = nullptr;
	FNiagaraDynamicDataLights *DynamicData = nullptr;
	for (int32 Idx = 0; Idx < EmitterRenderers.Num(); Idx++)
	{
		NiagaraRenderer *Renderer = EmitterRenderers[Idx];
		if (Renderer && Renderer->GetPropertiesClass() == UNiagaraLightRendererProperties::StaticClass())
		{
			LightRenderer = static_cast<NiagaraRendererLights*>(Renderer);
			DynamicData = static_cast<FNiagaraDynamicDataLights*>(Renderer->GetDynamicData());
			break;
		}
	}


	if (DynamicData)
	{
		int32 LightCount = DynamicData->LightArray.Num();
		
		OutParticleLights.InstanceData.Reserve(LightCount);
		OutParticleLights.PerViewData.Reserve(LightCount);

		for (NiagaraRendererLights::SimpleLightData &LightData : DynamicData->LightArray)
		{
			// When not using camera-offset, output one position for all views to share. 
			OutParticleLights.PerViewData.Add(LightData.PerViewEntry);

			// Add an entry for the light instance.
			OutParticleLights.InstanceData.Add(LightData.LightEntry);
		}
	}

}


//////////////////////////////////////////////////////////////////////////

UNiagaraComponent::UNiagaraComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bForceSolo(false)
	, AgeUpdateMode(EAgeUpdateMode::TickDeltaTime)
	, DesiredAge(0.0f)
	, SeekDelta(1 / 30.0f)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.SetTickFunctionEnable(false);
	bTickInEditor = true;
	bAutoActivate = true;
	bRenderingEnabled = true;
}


void UNiagaraComponent::TickComponent(float DeltaSeconds, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	check(SystemInstance->IsSolo());
	if (bIsActive && SystemInstance.Get())
	{
		if (SceneProxy != nullptr)
		{
			FNiagaraSceneProxy* NiagaraProxy = static_cast<FNiagaraSceneProxy*>(SceneProxy);
			NiagaraProxy->SetRenderingEnabled(bRenderingEnabled);
		}

		if (AgeUpdateMode == EAgeUpdateMode::TickDeltaTime)
		{
			SystemInstance->ComponentTick(DeltaSeconds);
		}
		else
		{
			float AgeDiff = DesiredAge - SystemInstance->GetAge();
			if (FMath::Abs(AgeDiff) < KINDA_SMALL_NUMBER)
			{
				AgeDiff = 0.0f;
			}

			if (AgeDiff < 0.0f)
			{
				SystemInstance->Reset(FNiagaraSystemInstance::EResetMode::ImmediateReset);
				AgeDiff = DesiredAge - SystemInstance->GetAge();
			}

			if (AgeDiff > 0.0f)
			{
				// TODO: This maximum should be configurable, and we should not draw the System until we are caught up.
				static const float MaxSimTime = 1;
				//Take SeekDelta to be the max delta time we'll accept.
				FNiagaraSystemSimulation* SystemSim = GetSystemSimulation();

				float SimTime = FMath::Min(AgeDiff, MaxSimTime);
				int32 NumSteps = FMath::CeilToInt(SimTime / SeekDelta);
				//Num steps must be odd. This is because when doing > 1 we have to flip the solo data set for in the system sim. This has to end up in the same flip state it was in before our ticks.
				NumSteps = NumSteps % 2 == 0 ? NumSteps + 1 : NumSteps;
				float RealSeekDelta = SimTime / (float)NumSteps;

				for (int32 i = NumSteps - 1; i >= 0; --i)
				{
					SystemInstance->ComponentTick(RealSeekDelta);
					if (i > 0)
					{
						SystemSim->TickSoloDataSet();
					}
				}
			}
		}
	}

	UpdateComponentToWorld();
	MarkRenderDynamicDataDirty();
}

const UObject* UNiagaraComponent::AdditionalStatObject() const
{
	return Asset;
}

void UNiagaraComponent::ResetSystem()
{
	Activate(true);
}

void UNiagaraComponent::ReinitializeSystem()
{
	DestroyInstance();
	Activate();

// 	if (IsRegistered() && SystemInstance.IsValid())
// 	{
// 		SystemInstance->Init(Asset, true, bForceSolo);
// 	}
}

bool UNiagaraComponent::GetRenderingEnabled() const
{
	return bRenderingEnabled;
}

void UNiagaraComponent::SetRenderingEnabled(bool bInRenderingEnabled)
{
	bRenderingEnabled = bInRenderingEnabled;
}

void UNiagaraComponent::Activate(bool bReset /* = false */)
{
	if (Asset == nullptr || Asset->IsValid() == false)
	{
		DestroyInstance();

		UE_LOG(LogNiagara, Warning, TEXT("Failed to activate Niagara Component!"));
		SetComponentTickEnabled(false);
		return;
	}

	if(!IsRegistered())
	{
		return;
	}
	
	Super::Activate(bReset);

	if (SystemInstance.IsValid() == false)
	{
		if (FNiagaraSystemSimulation* SystemSim = GetSystemSimulation())
		{
			SystemInstance = MakeUnique<FNiagaraSystemInstance>(this);
#if WITH_EDITORONLY_DATA
			OnSystemInstanceChangedDelegate.Broadcast();
#endif
			SystemInstance->Init(SystemSim, true, bForceSolo);
			bReset = false;
		}
		else
		{
			return;
		}
	}

	SystemInstance->Activate(bReset);

	/** We only need to tick the component if we require solo mode. */
	SetComponentTickEnabled(SystemInstance->IsSolo());
}

void UNiagaraComponent::Deactivate()
{
	Super::Deactivate();

	SetComponentTickEnabled(false);
	bIsActive = false;

	if (SystemInstance)
	{
		SystemInstance->Deactivate();
	}
}

void UNiagaraComponent::DestroyInstance()
{
	SystemInstance = nullptr;
#if WITH_EDITORONLY_DATA
	OnSystemInstanceChangedDelegate.Broadcast();
#endif
}

void UNiagaraComponent::OnRegister()
{
	Super::OnRegister();
}

void UNiagaraComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	//UE_LOG(LogNiagara, Log, TEXT("OnComponentDestroyed %p %p"), this, SystemInstance.Get());
	DestroyInstance();

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UNiagaraComponent::OnUnregister()
{
	DestroyInstance();
	Super::OnUnregister();
}

void UNiagaraComponent::BeginDestroy()
{
	DestroyInstance();

	Super::BeginDestroy();
}

class FNiagaraSystemSimulation* UNiagaraComponent::GetSystemSimulation()
{
	UWorld* World = GetWorld();
	if (World)
	{
		return FNiagaraWorldManager::Get(World)->GetSystemSimulation(Asset);
	}
	return nullptr;
}

void UNiagaraComponent::SendRenderDynamicData_Concurrent()
{
	if (SystemInstance.IsValid() && SceneProxy)
	{
		SystemInstance->GetSystemBounds().Init();
		
		FNiagaraSceneProxy* NiagaraProxy = static_cast<FNiagaraSceneProxy*>(SceneProxy);

		for (int32 i = 0; i < SystemInstance->GetEmitters().Num(); i++)
		{
			TSharedPtr<FNiagaraEmitterInstance> Emitter = SystemInstance->GetEmitters()[i];
			for (int32 EmitterIdx = 0; EmitterIdx < Emitter->GetEmitterRendererNum(); EmitterIdx++)
			{
				NiagaraRenderer* Renderer = Emitter->GetEmitterRenderer(EmitterIdx);
				if (Renderer)
				{
					if (Emitter->IsEnabled())
					{
						FNiagaraDynamicDataBase* DynamicData = Renderer->GenerateVertexData(NiagaraProxy, Emitter->GetData(), Emitter->GetEmitterHandle().GetInstance()->SimTarget);

						ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
							FSendNiagaraDynamicData,
							NiagaraRenderer*, EmitterRenderer, Emitter->GetEmitterRenderer(EmitterIdx),
							FNiagaraDynamicDataBase*, DynamicData, DynamicData,
							{
								EmitterRenderer->SetDynamicData_RenderThread(DynamicData);
							});
					}
					else
					{
						ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
							FSendNiagaraDynamicData,
							NiagaraRenderer*, EmitterRenderer, Emitter->GetEmitterRenderer(EmitterIdx),
							{
								EmitterRenderer->SetDynamicData_RenderThread(nullptr);
							});
					}
				}
			}
		}
	}

}

int32 UNiagaraComponent::GetNumMaterials() const
{
	return 0;
}


FBoxSphereBounds UNiagaraComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox SimBounds(ForceInit);
	if (SystemInstance.IsValid())
	{
		SystemInstance->GetSystemBounds().Init();
		for (int32 i = 0; i < SystemInstance->GetEmitters().Num(); i++)
		{
			FNiagaraEmitterInstance &Sim = *(SystemInstance->GetEmitters()[i]);
			SystemInstance->GetSystemBounds() += Sim.GetCachedBounds();
		}
		FBoxSphereBounds BSBounds(SystemInstance->GetSystemBounds());

		return BSBounds;
	}
	return FBoxSphereBounds(SimBounds);
}

FPrimitiveSceneProxy* UNiagaraComponent::CreateSceneProxy()
{
	// The constructor will set up the System renderers from the component.
	FNiagaraSceneProxy* Proxy = new FNiagaraSceneProxy(this);
	return Proxy;
}

void UNiagaraComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (!SystemInstance.IsValid())
	{
		return;
	}

	for (TSharedRef<FNiagaraEmitterInstance> Sim : SystemInstance->GetEmitters())	
	{
		if (UNiagaraEmitter* Props = Sim->GetEmitterHandle().GetInstance())
		{	
			for (int32 i = 0; i < Props->RendererProperties.Num(); i++)
			{
				if (UNiagaraRendererProperties* Renderer = Props->RendererProperties[i])
				{
					Renderer->GetUsedMaterials(OutMaterials);
				}
			}
		}
	}
}

FNiagaraSystemInstance* UNiagaraComponent::GetSystemInstance() const
{
	return SystemInstance.Get();
}

void UNiagaraComponent::OnSystemDisabled()
{
	//TODO: Add KillOnComplete option to asset to kill the component.

}

void UNiagaraComponent::SetNiagaraVariableVec4(const FString& InVariableName, const FVector4& InValue)
{
	FName VarName = FName(*InVariableName);

	SystemInstance->GetParameters().SetParameterValue(InValue, FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), VarName));
}

void UNiagaraComponent::SetNiagaraVariableVec3(const FString& InVariableName, FVector InValue)
{
	FName VarName = FName(*InVariableName);
	
	SystemInstance->GetParameters().SetParameterValue(InValue, FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), VarName));
}

void UNiagaraComponent::SetNiagaraVariableVec2(const FString& InVariableName, FVector2D InValue)
{
	FName VarName = FName(*InVariableName);

	SystemInstance->GetParameters().SetParameterValue(InValue, FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(),VarName));
}

void UNiagaraComponent::SetNiagaraVariableFloat(const FString& InVariableName, float InValue)
{
	FName VarName = FName(*InVariableName);

	SystemInstance->GetParameters().SetParameterValue(InValue, FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), VarName));
}

void UNiagaraComponent::SetNiagaraVariableBool(const FString& InVariableName, bool InValue)
{
	FName VarName = FName(*InVariableName);

	SystemInstance->GetParameters().SetParameterValue(InValue ? FNiagaraBool::True : FNiagaraBool::False, FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), VarName));
}

TArray<FVector> UNiagaraComponent::GetNiagaraParticlePositions_DebugOnly(const FString& InEmitterName)
{
	return GetNiagaraParticleValueVec3_DebugOnly(InEmitterName, TEXT("Position"));
}

TArray<FVector> UNiagaraComponent::GetNiagaraParticleValueVec3_DebugOnly(const FString& InEmitterName, const FString& InValueName)
{
	TArray<FVector> Positions;
	FName EmitterName = FName(*InEmitterName);
	if (SystemInstance.IsValid())
	{
		for (TSharedRef<FNiagaraEmitterInstance>& Sim : SystemInstance->GetEmitters())
		{
			if (Sim->GetEmitterHandle().GetName() == EmitterName)
			{
				int32 NumParticles = Sim->GetData().GetNumInstances();
				Positions.SetNum(NumParticles);
				FNiagaraDataSetIterator<FVector> PosItr(Sim->GetData(), FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), *InValueName));
				int32 i = 0;
				while (PosItr.IsValid())
				{
					FVector Position;
					PosItr.Get(Position);
					Positions[i] = Position;
					i++;
					PosItr.Advance();
				}
			}
		}
	}
	return Positions;

}

TArray<float> UNiagaraComponent::GetNiagaraParticleValues_DebugOnly(const FString& InEmitterName, const FString& InValueName)
{

	TArray<float> Values;
	FName EmitterName = FName(*InEmitterName);
	if (SystemInstance.IsValid())
	{
		for (TSharedRef<FNiagaraEmitterInstance>& Sim : SystemInstance->GetEmitters())
		{
			if (Sim->GetEmitterHandle().GetName() == EmitterName)
			{
				int32 NumParticles = Sim->GetData().GetNumInstances();
				Values.SetNum(NumParticles);
				FNiagaraDataSetIterator<float> ValueItr(Sim->GetData(), FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), *InValueName));
				int32 i = 0;
				while (ValueItr.IsValid())
				{
					float Value;
					ValueItr.Get(Value);
					Values[i] = Value;
					i++;
					ValueItr.Advance();
				}
			}
		}
	}
	return Values;
}

void UNiagaraComponent::PostLoad()
{
	Super::PostLoad();
	if (Asset)
	{
		Asset->ConditionalPostLoad();
#if WITH_EDITOR
		SynchronizeWithSourceSystem();
#endif
	}
}

#if WITH_EDITOR

void UNiagaraComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName;
	if (PropertyChangedEvent.Property)
	{
		PropertyName = PropertyChangedEvent.Property->GetFName();
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UNiagaraComponent, Asset) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UNiagaraComponent, InitialParameters))
	{
		SynchronizeWithSourceSystem();
	}

	ReinitializeSystem();
}


bool UNiagaraComponent::SynchronizeWithSourceSystem()
{
	//TODO: Look through params in system in "Owner" namespace and add to our parameters.
	if (Asset == nullptr)
	{
		return false;
	}

	TArray<FNiagaraVariable> SourceVars;
	Asset->GetExposedParameters().GetParameters(SourceVars);

	bool bEditsMade = false;
	for (FNiagaraVariable& Param : SourceVars)
	{
		bEditsMade |= InitialParameters.AddParameter(Param, true);
	}

	TArray<FNiagaraVariable> ExistingVars;
	InitialParameters.GetParameters(ExistingVars);

	for (FNiagaraVariable ExistingVar : ExistingVars)
	{
		if (!SourceVars.Contains(ExistingVar))
		{
			InitialParameters.RemoveParameter(ExistingVar);
			EditorOverridesValue.Remove(ExistingVar.GetName());
		}
	}

	for (FNiagaraVariable ExistingVar : ExistingVars)
	{
		bool* FoundVar = EditorOverridesValue.Find(ExistingVar.GetName());

		if (!IsParameterValueOverriddenLocally(ExistingVar.GetName()))
		{
			Asset->GetExposedParameters().CopyParameterData(InitialParameters, ExistingVar);
		}
	}

	InitialParameters.Rebind();

	//////////////////////////////////////////////////////////////////////////
	//OLD IMPL
// 	if (Asset == nullptr)
// 	{
// 		return false; 
// 	}
// 
// 	UNiagaraScript* Script = Asset->GetSystemSpawnScript();
// 	if (Script == nullptr)
// 	{
// 		return false;
// 	}
// 
// 	bool bEditsMade = false;
// 	{
// 		TArray<int32> VarIndicesToRemove;
// 
// 		// Check over all of our overrides to see if they still match up with the source System.
// 		for (int32 i = 0; i < SystemParameterLocalOverrides.Num(); i++)
// 		{
// 			FNiagaraVariable* Variable = Script->Parameters.FindParameter(SystemParameterLocalOverrides[i].GetId());
// 			// If the variable still exists, keep digging... otherwise remove it.
// 			if (Variable)
// 			{
// 				if (Variable->GetType() != SystemParameterLocalOverrides[i].GetType())
// 				{
// 					UE_LOG(LogNiagara, Warning, TEXT("Variable '%s' types changed.. possibly losing override value."), *Variable->GetName().ToString());
// 					VarIndicesToRemove.Add(i);
// 				}
// 				else if (Variable->GetName() != SystemParameterLocalOverrides[i].GetName())
// 				{
// 					if (bEditsMade == false)
// 					{
// 						bEditsMade = true;
// 						Modify();
// 					}
// 					SystemParameterLocalOverrides[i].SetName(Variable->GetName());
// 				}
// 			}
// 			else
// 			{
// 				VarIndicesToRemove.Add(i);
// 			}
// 		}
// 
// 		// Handle the variables that we want to remove.
// 		if (VarIndicesToRemove.Num() != 0)
// 		{
// 			if (bEditsMade == false)
// 			{
// 				bEditsMade = true;
// 				Modify();
// 			}
// 
// 			for (int32 removeIdx = VarIndicesToRemove.Num() - 1; removeIdx >= 0; removeIdx--)
// 			{
// 				UE_LOG(LogNiagara, Warning, TEXT("Removing local override '%s'."), *SystemParameterLocalOverrides[VarIndicesToRemove[removeIdx]].GetName().ToString());
// 
// 				SystemParameterLocalOverrides.RemoveAt(VarIndicesToRemove[removeIdx]);
// 			}
// 		}
// 	}
// 
// 	{
// 		TArray<int32> VarIndicesToRemove;
// 
// 		// Check over all of our overrides to see if they still match up with the source System.
// 		for (int32 i = 0; i < SystemDataInterfaceLocalOverrides.Num(); i++)
// 		{
// 			FNiagaraScriptDataInterfaceInfo* Variable = Script->DataInterfaceInfo.FindByPredicate([&](const FNiagaraScriptDataInterfaceInfo& Var)
// 			{
// 				return Var.Id == SystemDataInterfaceLocalOverrides[i].Id;
// 			}); 
// 			// If the variable still exists, keep digging... otherwise remove it.
// 			if (Variable)
// 			{
// 				if (Variable->DataInterface->GetClass() != SystemDataInterfaceLocalOverrides[i].DataInterface->GetClass())
// 				{
// 					UE_LOG(LogNiagara, Warning, TEXT("Variable '%s' types changed.. losing override value."), *Variable->Name.ToString());
// 					VarIndicesToRemove.Add(i);
// 				}
// 				else if (Variable->Name != SystemDataInterfaceLocalOverrides[i].Name)
// 				{
// 					if (bEditsMade == false)
// 					{
// 						bEditsMade = true;
// 						Modify();
// 					}
// 					SystemDataInterfaceLocalOverrides[i].Name = Variable->Name;
// 				}
// 			}
// 			else
// 			{
// 				VarIndicesToRemove.Add(i);
// 			}
// 		}
// 
// 		// Handle the variables that we want to remove.
// 		if (VarIndicesToRemove.Num() != 0)
// 		{
// 			if (bEditsMade == false)
// 			{
// 				bEditsMade = true;
// 				Modify();
// 			}
// 
// 			for (int32 removeIdx = VarIndicesToRemove.Num() - 1; removeIdx >= 0; removeIdx--)
// 			{
// 				UE_LOG(LogNiagara, Warning, TEXT("Removing local override '%s'."), *SystemDataInterfaceLocalOverrides[VarIndicesToRemove[removeIdx]].Name.ToString());
// 
// 				SystemDataInterfaceLocalOverrides.RemoveAt(VarIndicesToRemove[removeIdx]);
// 			}
// 		}
// 	}
// 
// 	return bEditsMade;
	return false;
}

UNiagaraComponent::EAgeUpdateMode UNiagaraComponent::GetAgeUpdateMode() const
{
	return AgeUpdateMode;
}

void UNiagaraComponent::SetAgeUpdateMode(EAgeUpdateMode InAgeUpdateMode)
{
	AgeUpdateMode = InAgeUpdateMode;
}

float UNiagaraComponent::GetDesiredAge() const
{
	return DesiredAge;
}

void UNiagaraComponent::SetDesiredAge(float InDesiredAge)
{
	DesiredAge = InDesiredAge;
}

float UNiagaraComponent::GetSeekDelta() const
{
	return SeekDelta;
}

void UNiagaraComponent::SetSeekDelta(float InSeekDelta)
{
	SeekDelta = InSeekDelta;
}


bool UNiagaraComponent::IsParameterValueOverriddenLocally(const FName& InParamName)
{
	bool* FoundVar = EditorOverridesValue.Find(InParamName);

	if (FoundVar != nullptr && *(FoundVar))
	{
		return true;
	}
	return false;
}

void UNiagaraComponent::SetParameterValueOverriddenLocally(const FName& InParamName, bool bInOverriden)
{
	bool* FoundVar = EditorOverridesValue.Find(InParamName);

	if (FoundVar != nullptr && bInOverriden) 
	{
		*(FoundVar) = bInOverriden;
	}
	else if (FoundVar == nullptr && bInOverriden)			
	{
		EditorOverridesValue.Add(InParamName, true);
	}
	else
	{
		EditorOverridesValue.Remove(InParamName);

		UNiagaraScript* ScriptSpawn = Asset->GetSystemSpawnScript(true);
		UNiagaraScript* ScriptUpdate = Asset->GetSystemUpdateScript(true);
		if (ScriptSpawn != nullptr)
		{
			for (FNiagaraVariable Var : ScriptSpawn->Parameters.Parameters)
			{
				if (Var.GetName() == InParamName)
				{
					InitialParameters.SetParameter(Var);
					return;
				}
			}			
		}
		if (ScriptUpdate != nullptr)
		{
			for (FNiagaraVariable Var : ScriptUpdate->Parameters.Parameters)
			{
				if (Var.GetName() == InParamName)
				{
					InitialParameters.SetParameter(Var);
					return;
				}
			}
		}
	}
}


#endif // WITH_EDITOR



void UNiagaraComponent::SetAsset(UNiagaraSystem* InAsset)
{
	Asset = InAsset;

#if WITH_EDITOR
	SynchronizeWithSourceSystem();
#endif

	ReinitializeSystem();
}



