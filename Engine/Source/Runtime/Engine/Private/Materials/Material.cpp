// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnMaterial.cpp: Shader implementation.
=============================================================================*/

#include "Materials/Material.h"
#include "Stats/StatsMisc.h"
#include "Misc/FeedbackContext.h"
#include "UObject/RenderingObjectVersion.h"
#include "Misc/App.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "UObject/UObjectAnnotation.h"
#include "UObject/LinkerLoad.h"
#include "EngineGlobals.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UnrealEngine.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "Materials/MaterialExpressionDynamicParameter.h"
#include "Materials/MaterialExpressionFontSampleParameter.h"
#include "Materials/MaterialExpressionQualitySwitch.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticComponentMaskParameter.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexInterpolator.h"
#include "Materials/MaterialExpressionSceneColor.h"
#include "SceneManagement.h"
#include "Materials/MaterialUniformExpressions.h"
#include "Engine/SubsurfaceProfile.h"
#include "EditorSupportDelegates.h"
#include "ComponentRecreateRenderStateContext.h"
#include "ShaderCompiler.h"
#include "Materials/MaterialParameterCollection.h"
#include "ShaderPlatformQualitySettings.h"
#include "MaterialShaderQualitySettings.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "MaterialCompiler.h"
#include "Materials/MaterialInstanceSupport.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "Materials/MaterialExpressionComment.h"

// NVCHANGE_BEGIN: Add VXGI
#include "Materials/MaterialExpressionVxgiVoxelization.h"
// NVCHANGE_END: Add VXGI

#if WITH_EDITOR
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "MaterialGraph/MaterialGraph.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#endif

#define LOCTEXT_NAMESPACE "Material"

#if WITH_EDITOR
const FMaterialsWithDirtyUsageFlags FMaterialsWithDirtyUsageFlags::DefaultAnnotation;

void FMaterialsWithDirtyUsageFlags::MarkUsageFlagDirty(EMaterialUsage UsageFlag)
{
	MaterialFlagsThatHaveChanged |= (1 << UsageFlag);
}

bool FMaterialsWithDirtyUsageFlags::IsUsageFlagDirty(EMaterialUsage UsageFlag)
{
	return (MaterialFlagsThatHaveChanged & (1 << UsageFlag)) != 0;
}

FUObjectAnnotationSparseBool GMaterialsThatNeedSamplerFixup;
FUObjectAnnotationSparse<FMaterialsWithDirtyUsageFlags,true> GMaterialsWithDirtyUsageFlags;
FUObjectAnnotationSparseBool GMaterialsThatNeedExpressionsFlipped;
FUObjectAnnotationSparseBool GMaterialsThatNeedCoordinateCheck;
FUObjectAnnotationSparseBool GMaterialsThatNeedCommentFix;

#endif // #if WITH_EDITOR

FMaterialResource::FMaterialResource()
	: FMaterial()
	, Material(NULL)
	, MaterialInstance(NULL)
{
}

int32 FMaterialResource::CompilePropertyAndSetMaterialProperty(EMaterialProperty Property, FMaterialCompiler* Compiler, EShaderFrequency OverrideShaderFrequency, bool bUsePreviousFrameTime) const
{
#if WITH_EDITOR
	// needs to be called in this function!!
	// sets CurrentShaderFrequency
	Compiler->SetMaterialProperty(Property, OverrideShaderFrequency, bUsePreviousFrameTime);

	EShaderFrequency ShaderFrequency = Compiler->GetCurrentShaderFrequency();

	int32 SelectionColorIndex = INDEX_NONE;

	if (ShaderFrequency == SF_Pixel && GetMaterialDomain() != MD_Volume)
	{
		SelectionColorIndex = Compiler->ComponentMask(Compiler->VectorParameter(NAME_SelectionColor,FLinearColor::Black),1,1,1,0);
	}
	
	//Compile the material instance if we have one.
	UMaterialInterface* MaterialInterface = MaterialInstance ? static_cast<UMaterialInterface*>(MaterialInstance) : Material;

	int32 Ret = INDEX_NONE;

	switch(Property)
	{
		case MP_EmissiveColor:
			if (SelectionColorIndex != INDEX_NONE)
			{
				Ret = Compiler->Add(MaterialInterface->CompileProperty(Compiler, MP_EmissiveColor, MFCF_ForceCast), SelectionColorIndex);
			}
			else
			{
				Ret = MaterialInterface->CompileProperty(Compiler, MP_EmissiveColor);
			}
			break;

		case MP_DiffuseColor: 
			Ret = MaterialInterface->CompileProperty(Compiler, MP_DiffuseColor, MFCF_ForceCast);
			break;
		case MP_BaseColor: 
			Ret = MaterialInterface->CompileProperty(Compiler, MP_BaseColor, MFCF_ForceCast);
			break;
		case MP_MaterialAttributes:
			Ret = INDEX_NONE;
			break;
		default:
			Ret = MaterialInterface->CompileProperty(Compiler, Property);
	};
	
	EMaterialValueType AttributeType = FMaterialAttributeDefinitionMap::GetValueType(Property);

	if (Ret != INDEX_NONE)
	{
		FMaterialUniformExpression* Expression = Compiler->GetParameterUniformExpression(Ret);

		if (Expression && Expression->IsConstant())
		{
			// Where possible we want to preserve constant expressions allowing default value checks
			EMaterialValueType ResultType = Compiler->GetParameterType(Ret);
			EMaterialValueType ExactAttributeType = (AttributeType == MCT_Float) ? MCT_Float1 : AttributeType;
			EMaterialValueType ExactResultType = (ResultType == MCT_Float) ? MCT_Float1 : ResultType;

			if (ExactAttributeType == ExactResultType)
			{
				return Ret;
			}
			else if (ResultType == MCT_Float || (ExactAttributeType == MCT_Float1 && ResultType & MCT_Float))
			{
				return Compiler->ComponentMask(Ret, true, ExactAttributeType >= MCT_Float2, ExactAttributeType >= MCT_Float3, ExactAttributeType >= MCT_Float4);
			}
		}
	}

	// Output should always be the right type for this property
	return Compiler->ForceCast(Ret, AttributeType);
#else // WITH_EDITOR
	check(0); // This is editor-only function
	return INDEX_NONE;
#endif // WITH_EDITOR
}

#if HANDLE_CUSTOM_OUTPUTS_AS_MATERIAL_ATTRIBUTES
int32 FMaterialResource::CompileCustomAttribute(const FGuid& AttributeID, FMaterialCompiler* Compiler) const
{
	return Material->CompilePropertyEx(Compiler, AttributeID);
}
#endif

void FMaterialResource::GatherCustomOutputExpressions(TArray<UMaterialExpressionCustomOutput*>& OutCustomOutputs) const
{
	Material->GetAllCustomOutputExpressions(OutCustomOutputs);
}

void FMaterialResource::GatherExpressionsForCustomInterpolators(TArray<UMaterialExpression*>& OutExpressions) const
{
	Material->GetAllExpressionsForCustomInterpolators(OutExpressions);
}

void FMaterialResource::GetShaderMapId(EShaderPlatform Platform, FMaterialShaderMapId& OutId) const
{
	FMaterial::GetShaderMapId(Platform, OutId);
	Material->AppendReferencedFunctionIdsTo(OutId.ReferencedFunctions);
	Material->AppendReferencedParameterCollectionIdsTo(OutId.ReferencedParameterCollections);

	Material->GetForceRecompileTextureIdsHash(OutId.TextureReferencesHash);

	if(MaterialInstance)
	{
		MaterialInstance->GetBasePropertyOverridesHash(OutId.BasePropertyOverridesHash);

		FStaticParameterSet CompositedStaticParameters;
		MaterialInstance->GetStaticParameterValues(CompositedStaticParameters);
		OutId.ParameterSet = CompositedStaticParameters;
	}
}

/**
 * A resource which represents the default instance of a UMaterial to the renderer.
 * Note that default parameter values are stored in the FMaterialUniformExpressionXxxParameter objects now.
 * This resource is only responsible for the selection color.
 */
class FDefaultMaterialInstance : public FMaterialRenderProxy
{
public:

	/**
	 * Called from the game thread to destroy the material instance on the rendering thread.
	 */
	void GameThread_Destroy()
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FDestroyDefaultMaterialInstanceCommand,
			FDefaultMaterialInstance*,Resource,this,
		{
			delete Resource;
		});
	}

	// FMaterialRenderProxy interface.
	virtual const class FMaterial* GetMaterial(ERHIFeatureLevel::Type InFeatureLevel) const
	{
		const FMaterialResource* MaterialResource = Material->GetMaterialResource(InFeatureLevel);
		if (MaterialResource && MaterialResource->GetRenderingThreadShaderMap())
		{
			// Verify that compilation has been finalized, the rendering thread shouldn't be touching it otherwise
			checkSlow(MaterialResource->GetRenderingThreadShaderMap()->IsCompilationFinalized());
			// The shader map reference should have been NULL'ed if it did not compile successfully
			checkSlow(MaterialResource->GetRenderingThreadShaderMap()->CompiledSuccessfully());
			return MaterialResource;
		}

		// If we are the default material, must not try to fall back to the default material in an error state as that will be infinite recursion
		check(!Material->IsDefaultMaterial());

		return GetFallbackRenderProxy().GetMaterial(InFeatureLevel);
	}

	virtual FMaterial* GetMaterialNoFallback(ERHIFeatureLevel::Type InFeatureLevel) const
	{
		checkSlow(IsInRenderingThread());
		return Material->GetMaterialResource(InFeatureLevel);
	}

	virtual UMaterialInterface* GetMaterialInterface() const override
	{
		return Material;
	}

	virtual bool GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
	{
		const FMaterialResource* MaterialResource = Material->GetMaterialResource(Context.Material.GetFeatureLevel());
		if(MaterialResource && MaterialResource->GetRenderingThreadShaderMap())
		{
			if(ParameterName == NAME_SelectionColor)
			{
				*OutValue = FLinearColor::Black;
				if( GIsEditor && Context.bShowSelection )
				{
					if( IsSelected() )
					{
						// Note this code is only used for mesh selections in mesh editors now
						// Selected objects in the level editor now goes through the selection outline post process effect where it applies the highlight intensity there
						*OutValue = GEngine->GetSelectedMaterialColor() * GEngine->SelectionMeshSectionHighlightIntensity;
					}
					else if( IsHovered() )
					{
						*OutValue = GEngine->GetHoveredMaterialColor() * GEngine->HoverHighlightIntensity;
					}
				}
				return true;
			}
			return false;
		}
		else
		{
			return GetFallbackRenderProxy().GetVectorValue(ParameterName, OutValue, Context);
		}
	}
	virtual bool GetScalarValue(const FName ParameterName, float* OutValue, const FMaterialRenderContext& Context) const
	{
		const FMaterialResource* MaterialResource = Material->GetMaterialResource(Context.Material.GetFeatureLevel());
		if(MaterialResource && MaterialResource->GetRenderingThreadShaderMap())
		{
			static FName NameSubsurfaceProfile(TEXT("__SubsurfaceProfile"));
			if (ParameterName == NameSubsurfaceProfile)
			{
				const USubsurfaceProfile* MySubsurfaceProfileRT = GetSubsurfaceProfileRT();

				int32 AllocationId = 0;
				if(MySubsurfaceProfileRT)
				{
					// can be optimized (cached)
					AllocationId = GSubsurfaceProfileTextureObject.FindAllocationId(MySubsurfaceProfileRT);
				}
				else
				{
					// no profile specified means we use the default one stored at [0] which is human skin
					AllocationId = 0;
				}

				*OutValue = AllocationId / 255.0f;

				return true;
			}

			return false;
		}
		else
		{
			return GetFallbackRenderProxy().GetScalarValue(ParameterName, OutValue, Context);
		}
	}
	virtual bool GetTextureValue(const FName ParameterName,const UTexture** OutValue, const FMaterialRenderContext& Context) const
	{
		const FMaterialResource* MaterialResource = Material->GetMaterialResource(Context.Material.GetFeatureLevel());
		if(MaterialResource && MaterialResource->GetRenderingThreadShaderMap())
		{
			return false;
		}
		else
		{
			return GetFallbackRenderProxy().GetTextureValue(ParameterName,OutValue,Context);
		}
	}

	// FRenderResource interface.
	virtual FString GetFriendlyName() const { return Material->GetName(); }

	// Constructor.
	FDefaultMaterialInstance(UMaterial* InMaterial,bool bInSelected,bool bInHovered):
		FMaterialRenderProxy(bInSelected, bInHovered),
		Material(InMaterial)
	{}

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	virtual FVxgiMaterialProperties GetVxgiMaterialProperties() const override { return Material->GetVxgiMaterialProperties(); }
	virtual bool IsTwoSided() const override { return Material->IsTwoSided(); }
#endif
	// NVCHANGE_END: Add VXGI

private:

	/** Get the fallback material. */
	FMaterialRenderProxy& GetFallbackRenderProxy() const
	{
		return *(UMaterial::GetDefaultMaterial(Material->MaterialDomain)->GetRenderProxy(IsSelected(),IsHovered()));
	}

	UMaterial* Material;
};

#if WITH_EDITOR
static bool GAllowCompilationInPostLoad=true;
#else
#define GAllowCompilationInPostLoad true
#endif

void UMaterial::ForceNoCompilationInPostLoad(bool bForceNoCompilation)
{
#if WITH_EDITOR
	GAllowCompilationInPostLoad = !bForceNoCompilation;
#endif
}

static UMaterialFunction* GPowerToRoughnessMaterialFunction = NULL;
static UMaterialFunction* GConvertFromDiffSpecMaterialFunction = NULL;

static UMaterial* GDefaultMaterials[MD_MAX] = {0};

static const TCHAR* GDefaultMaterialNames[MD_MAX] =
{
	// Surface
	TEXT("engine-ini:/Script/Engine.Engine.DefaultMaterialName"),
	// Deferred Decal
	TEXT("engine-ini:/Script/Engine.Engine.DefaultDeferredDecalMaterialName"),
	// Light Function
	TEXT("engine-ini:/Script/Engine.Engine.DefaultLightFunctionMaterialName"),
	// Volume
	//@todo - get a real MD_Volume default material
	TEXT("engine-ini:/Script/Engine.Engine.DefaultMaterialName"),
	// Post Process
	TEXT("engine-ini:/Script/Engine.Engine.DefaultPostProcessMaterialName"),
	// User Interface 
	TEXT("engine-ini:/Script/Engine.Engine.DefaultMaterialName"),
};

void UMaterialInterface::InitDefaultMaterials()
{
	// Note that this function will (in fact must!) be called recursively. This
	// guarantees that the default materials will have been loaded and pointers
	// set before any other material interface has been instantiated -- even
	// one of the default materials! It is actually possible to assert that
	// these materials exist in the UMaterial or UMaterialInstance constructor.
	// 
	// The check for initialization is purely an optimization as initializing
	// the default materials is only done very early in the boot process.
	static bool bInitialized = false;
	if (!bInitialized)
	{		
		check(IsInGameThread());
		if (!IsInGameThread())
		{
			return;
		}
		static int32 RecursionLevel = 0;
		RecursionLevel++;

		
#if WITH_EDITOR
		GPowerToRoughnessMaterialFunction = LoadObject< UMaterialFunction >(nullptr, TEXT("/Engine/Functions/Engine_MaterialFunctions01/Shading/PowerToRoughness.PowerToRoughness"), nullptr, LOAD_None, nullptr);
		checkf( GPowerToRoughnessMaterialFunction, TEXT("Cannot load PowerToRoughness") );
		GPowerToRoughnessMaterialFunction->AddToRoot();

		GConvertFromDiffSpecMaterialFunction = LoadObject< UMaterialFunction >(nullptr, TEXT("/Engine/Functions/Engine_MaterialFunctions01/Shading/ConvertFromDiffSpec.ConvertFromDiffSpec"), nullptr, LOAD_None, nullptr);
		checkf( GConvertFromDiffSpecMaterialFunction, TEXT("Cannot load ConvertFromDiffSpec") );
		GConvertFromDiffSpecMaterialFunction->AddToRoot();
#endif

		for (int32 Domain = 0; Domain < MD_MAX; ++Domain)
		{
			if (GDefaultMaterials[Domain] == nullptr)
			{
				FString ResolvedPath = ResolveIniObjectsReference(GDefaultMaterialNames[Domain]);

				GDefaultMaterials[Domain] = FindObject<UMaterial>(nullptr, *ResolvedPath);
				if (GDefaultMaterials[Domain] == nullptr
#if USE_EVENT_DRIVEN_ASYNC_LOAD_AT_BOOT_TIME
					&& (RecursionLevel == 1 || !GEventDrivenLoaderEnabled)
#endif
					)
				{
					GDefaultMaterials[Domain] = LoadObject<UMaterial>(nullptr, *ResolvedPath, nullptr, LOAD_DisableDependencyPreloading, nullptr);
					checkf(GDefaultMaterials[Domain] != nullptr, TEXT("Cannot load default material '%s'"), GDefaultMaterialNames[Domain]);
				}
				if (GDefaultMaterials[Domain])
				{
					GDefaultMaterials[Domain]->AddToRoot();
				}
			}
		}
		RecursionLevel--;
#if USE_EVENT_DRIVEN_ASYNC_LOAD_AT_BOOT_TIME
		bInitialized = !GEventDrivenLoaderEnabled || RecursionLevel == 0;
#else
		bInitialized = true;
#endif
	}
}

void UMaterialInterface::PostCDOContruct()
{
	if (GEventDrivenLoaderEnabled && EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME)
	{
		UMaterial::StaticClass()->GetDefaultObject();
		UMaterialInterface::InitDefaultMaterials();
	}
}


void UMaterialInterface::PostLoadDefaultMaterials()
{
	LLM_SCOPE(ELLMTag::Materials);

	// Here we prevent this function from being called recursively. Mostly this
	// is an optimization and guarantees that default materials are post loaded
	// in the order material domains are defined. Surface -> deferred decal -> etc.
	static bool bPostLoaded = false;
	if (!bPostLoaded)
	{
		check(IsInGameThread());
		bPostLoaded = true;

#if WITH_EDITOR
		GPowerToRoughnessMaterialFunction->ConditionalPostLoad();
		GConvertFromDiffSpecMaterialFunction->ConditionalPostLoad();
#endif

		for (int32 Domain = 0; Domain < MD_MAX; ++Domain)
		{
			UMaterial* Material = GDefaultMaterials[Domain];
#if USE_EVENT_DRIVEN_ASYNC_LOAD_AT_BOOT_TIME
			check(Material || (GIsInitialLoad && GEventDrivenLoaderEnabled));
			check((GIsInitialLoad && GEventDrivenLoaderEnabled) || !Material->HasAnyFlags(RF_NeedLoad)); //-V595
			if (Material && !Material->HasAnyFlags(RF_NeedLoad))
#else
			check(Material);
			if (Material)
#endif
			{
				Material->ConditionalPostLoad();
				// Sometimes the above will get called before the material has been fully serialized
				// in this case its NeedPostLoad flag will not be cleared.
				if (Material->HasAnyFlags(RF_NeedPostLoad))
				{
					bPostLoaded = false;
				}
			}
			else
			{
				bPostLoaded = false;
			}
		}
	}
}

void UMaterialInterface::AssertDefaultMaterialsExist()
{
#if (USE_EVENT_DRIVEN_ASYNC_LOAD_AT_BOOT_TIME)
	if (!GIsInitialLoad || !GEventDrivenLoaderEnabled)
#endif
	{
		for (int32 Domain = 0; Domain < MD_MAX; ++Domain)
		{
			check(GDefaultMaterials[Domain] != NULL);
		}
	}
}

void UMaterialInterface::AssertDefaultMaterialsPostLoaded()
{
#if (USE_EVENT_DRIVEN_ASYNC_LOAD_AT_BOOT_TIME)
	if (!GIsInitialLoad || !GEventDrivenLoaderEnabled)
#endif
	{
		for (int32 Domain = 0; Domain < MD_MAX; ++Domain)
		{
			check(GDefaultMaterials[Domain] != NULL);
			check(!GDefaultMaterials[Domain]->HasAnyFlags(RF_NeedPostLoad));
		}
	}
}

static TAutoConsoleVariable<int32> CVarDiscardUnusedQualityLevels(
	TEXT("r.DiscardUnusedQuality"),
	0,
	TEXT("Wether to keep or discard unused quality level shadermaps in memory.\n")
	TEXT("0: keep all quality levels in memory. (default)\n")
	TEXT("1: Discard unused quality levels on load."),
	ECVF_ReadOnly);

void SerializeInlineShaderMaps(const TMap<const ITargetPlatform*, TArray<FMaterialResource*>>* PlatformMaterialResourcesToSavePtr, FArchive& Ar, TArray<FMaterialResource>& OutLoadedResources)
{
	LLM_SCOPE(ELLMTag::Materials);
	SCOPED_LOADTIMER(SerializeInlineShaderMaps);

	if (Ar.IsSaving())
	{
		int32 NumResourcesToSave = 0;
		const TArray<FMaterialResource*> *MaterialResourcesToSavePtr = NULL;
		if (Ar.IsCooking())
		{
			check( PlatformMaterialResourcesToSavePtr );
			auto& PlatformMaterialResourcesToSave = *PlatformMaterialResourcesToSavePtr;

			MaterialResourcesToSavePtr = PlatformMaterialResourcesToSave.Find( Ar.CookingTarget() );
			check( MaterialResourcesToSavePtr != NULL || (Ar.GetLinker()==NULL) );
			if (MaterialResourcesToSavePtr!= NULL )
			{
				NumResourcesToSave = MaterialResourcesToSavePtr->Num();
			}
		}

		Ar << NumResourcesToSave;

		if ( MaterialResourcesToSavePtr )
		{
			const TArray<FMaterialResource*> &MaterialResourcesToSave = *MaterialResourcesToSavePtr;
			for (int32 ResourceIndex = 0; ResourceIndex < NumResourcesToSave; ResourceIndex++)
			{
				MaterialResourcesToSave[ResourceIndex]->SerializeInlineShaderMap(Ar);
			}
		}
		
	}
	else if (Ar.IsLoading())
	{
		int32 NumLoadedResources = 0;
		Ar << NumLoadedResources;

		OutLoadedResources.Empty(NumLoadedResources);

		for (int32 ResourceIndex = 0; ResourceIndex < NumLoadedResources; ResourceIndex++)
		{
			FMaterialResource LoadedResource;
			LoadedResource.SerializeInlineShaderMap(Ar);
			OutLoadedResources.Add(LoadedResource);
		}
	}
}

void ProcessSerializedInlineShaderMaps(UMaterialInterface* Owner, TArray<FMaterialResource>& LoadedResources, FMaterialResource* (&OutMaterialResourcesLoaded)[EMaterialQualityLevel::Num][ERHIFeatureLevel::Num])
{
	LLM_SCOPE(ELLMTag::Materials);
	check(IsInGameThread());

	UMaterial* OwnerMaterial = Cast<UMaterial>(Owner);
	UMaterialInstance* OwnerMaterialInstance = Cast<UMaterialInstance>(Owner);

	for (FMaterialResource& Resource : LoadedResources)
	{
		Resource.RegisterInlineShaderMap();
	}

	if (CVarDiscardUnusedQualityLevels.GetValueOnAnyThread())
	{
		// Map EMaterialQualityLevel to a score.
		// Higher quality levels are of increasing weight such that lower quality is preferred when neighbouring hi/lo QLs exist.
		static const int32 QualityScores[EMaterialQualityLevel::Num + 1] = { 0, 3, 1, 10 };

		int32 DesiredQL = (int32)GetCachedScalabilityCVars().MaterialQualityLevel;
		check(DesiredQL < EMaterialQualityLevel::Num);
		const int32 DesiredScore = QualityScores[DesiredQL];

		FMaterialShaderMap* BestShaderMap[ERHIFeatureLevel::Num];
		for (int32 FeatureIdx = 0; FeatureIdx < ERHIFeatureLevel::Num; ++FeatureIdx)
		{
			BestShaderMap[FeatureIdx] = nullptr;
		}

		for (int32 ResourceIndex = 0; ResourceIndex < LoadedResources.Num(); ResourceIndex++)
		{
			FMaterialResource& LoadedResource = LoadedResources[ResourceIndex];
			FMaterialShaderMap* LoadedShaderMap = LoadedResource.GetGameThreadShaderMap();

			if (LoadedShaderMap && LoadedShaderMap->GetShaderPlatform() == GMaxRHIShaderPlatform)
			{
				EMaterialQualityLevel::Type LoadedQualityLevel = LoadedShaderMap->GetShaderMapId().QualityLevel;
				LoadedQualityLevel = LoadedQualityLevel == EMaterialQualityLevel::Num ? EMaterialQualityLevel::High : LoadedQualityLevel;
				ERHIFeatureLevel::Type LoadedFeatureLevel = LoadedShaderMap->GetShaderMapId().FeatureLevel;

				// find current QL:
				int32 CurrentQL = (int32)EMaterialQualityLevel::Num;
				for (int32 i = 0; i < EMaterialQualityLevel::Num; i++)
				{
					FMaterialResource* MaterialResource = OutMaterialResourcesLoaded[i][LoadedFeatureLevel];
					if (MaterialResource != nullptr && MaterialResource->GetGameThreadShaderMap() != nullptr)
					{
						int32 FoundQL = MaterialResource->GetGameThreadShaderMap()->GetShaderMapId().QualityLevel;
						CurrentQL = FoundQL == EMaterialQualityLevel::Num ? EMaterialQualityLevel::High : FoundQL;
					}
				}

				// Determine if this is a better match than our current shader map.
				const int32 CurrentScore = FMath::Abs(QualityScores[CurrentQL] - DesiredScore);
				const int32 PotentialScore = FMath::Abs(QualityScores[LoadedQualityLevel] - DesiredScore);
				if (PotentialScore < CurrentScore)
				{
					BestShaderMap[LoadedFeatureLevel] = LoadedShaderMap;
				}
			}
		}

		for (int32 FeatureIdx = 0; FeatureIdx < ERHIFeatureLevel::Num; ++FeatureIdx)
		{
			if (BestShaderMap[FeatureIdx])
			{
				// replace existing shadermap with loadedshadermap.
				for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
				{
					if (!OutMaterialResourcesLoaded[QualityLevelIndex][FeatureIdx])
					{
						OutMaterialResourcesLoaded[QualityLevelIndex][FeatureIdx] =
							OwnerMaterialInstance ? OwnerMaterialInstance->AllocatePermutationResource() : OwnerMaterial->AllocateResource();
					}
					OutMaterialResourcesLoaded[QualityLevelIndex][FeatureIdx]->ReleaseShaderMap();
					OutMaterialResourcesLoaded[QualityLevelIndex][FeatureIdx]->SetInlineShaderMap(BestShaderMap[FeatureIdx]);
				}
			}
		}
	}
	else
	{
		// Apply in 2 passes - first pass is for shader maps without a specified quality level
		// Second pass is where shader maps with a specified quality level override
		for (int32 PassIndex = 0; PassIndex < 2; PassIndex++)
		{
			for (int32 ResourceIndex = 0; ResourceIndex < LoadedResources.Num(); ResourceIndex++)
			{
				FMaterialResource& LoadedResource = LoadedResources[ResourceIndex];
				FMaterialShaderMap* LoadedShaderMap = LoadedResource.GetGameThreadShaderMap();

				if (LoadedShaderMap && LoadedShaderMap->GetShaderPlatform() == GMaxRHIShaderPlatform)
				{
					EMaterialQualityLevel::Type LoadedQualityLevel = LoadedShaderMap->GetShaderMapId().QualityLevel;
					ERHIFeatureLevel::Type LoadedFeatureLevel = LoadedShaderMap->GetShaderMapId().FeatureLevel;
					for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
					{
						// Apply to all resources in the first pass if the shader map does not have a quality level specified
						if ((PassIndex == 0 && LoadedQualityLevel == EMaterialQualityLevel::Num)
							// Apply to just the corresponding resource in the second pass if the shader map has a quality level specified
							|| (PassIndex == 1 && QualityLevelIndex == LoadedQualityLevel))
						{
							if (!OutMaterialResourcesLoaded[QualityLevelIndex][LoadedFeatureLevel])
							{
								OutMaterialResourcesLoaded[QualityLevelIndex][LoadedFeatureLevel] =
									OwnerMaterialInstance ? OwnerMaterialInstance->AllocatePermutationResource() : OwnerMaterial->AllocateResource();
							}

							OutMaterialResourcesLoaded[QualityLevelIndex][LoadedFeatureLevel]->SetInlineShaderMap(LoadedShaderMap);
						}
					}
				}
			}
		}
	}
}

UMaterial* UMaterial::GetDefaultMaterial(EMaterialDomain Domain)
{
	InitDefaultMaterials();
	check(Domain >= MD_Surface && Domain < MD_MAX);
	check(GDefaultMaterials[Domain] != NULL);
	return GDefaultMaterials[Domain];
}

bool UMaterial::IsDefaultMaterial() const
{
	bool bDefault = false;
	for (int32 Domain = MD_Surface; !bDefault && Domain < MD_MAX; ++Domain)
	{
		bDefault = (this == GDefaultMaterials[Domain]);
	}
	return bDefault;
}

UMaterial::UMaterial(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BlendMode = BLEND_Opaque;
	ShadingModel = MSM_DefaultLit;
	TranslucencyLightingMode = TLM_VolumetricNonDirectional;
	TranslucencyDirectionalLightingIntensity = 1.0f;
	TranslucentShadowDensityScale = 0.5f;
	TranslucentSelfShadowDensityScale = 2.0f;
	TranslucentSelfShadowSecondDensityScale = 10.0f;
	TranslucentSelfShadowSecondOpacity = 0.0f;
	TranslucentBackscatteringExponent = 30.0f;
	TranslucentMultipleScatteringExtinction = FLinearColor(1.0f, 0.833f, 0.588f, 1.0f);
	TranslucentShadowStartOffset = 100.0f;

	DiffuseColor_DEPRECATED.Constant = FColor(128,128,128);
	SpecularColor_DEPRECATED.Constant = FColor(128,128,128);
	BaseColor.Constant = FColor(128,128,128);	
	Metallic.Constant = 0.0f;
	Specular.Constant = 0.5f;
	Roughness.Constant = 0.5f;
	
	Opacity.Constant = 1.0f;
	OpacityMask.Constant = 1.0f;
	OpacityMaskClipValue = 0.3333f;
	bCastDynamicShadowAsMasked = false;
	bUsedWithStaticLighting = false;
	D3D11TessellationMode = MTM_NoTessellation;
	bEnableCrackFreeDisplacement = false;
	bEnableAdaptiveTessellation = true;
	MaxDisplacement = 0.0f;
	bOutputVelocityOnBasePass = true;
	bEnableSeparateTranslucency = true;
	bEnableMobileSeparateTranslucency = false;
	bEnableResponsiveAA = false;
	bTangentSpaceNormal = true;
	bUseLightmapDirectionality = true;
	bAutomaticallySetUsageInEditor = true;

	bUseMaterialAttributes = false;
	bUseTranslucencyVertexFog = true;
	BlendableLocation = BL_AfterTonemapping;
	BlendablePriority = 0;
	BlendableOutputAlpha = false;

	bUseEmissiveForDynamicAreaLighting = false;
	bBlockGI = false;
	RefractionDepthBias = 0.0f;
	MaterialDecalResponse = MDR_ColorNormalRoughness;

	bAllowDevelopmentShaderCompile = true;
	bIsMaterialEditorStatsMaterial = false;

	// NVCHANGE_BEGIN: Add VXGI
	bVxgiConeTracingEnable = false;
	bUsedWithVxgiVoxelization = true;
	bVxgiAllowTesselationDuringVoxelization = false;
	bVxgiOmniDirectional = false;
	bVxgiProportionalEmittance = false;
	VxgiMaterialSamplingRate = VXGIMSR_FixedDefault;
	VxgiOpacityNoiseScaleBias = FVector2D(0.f, 0.f);
	bVxgiCoverageSupersampling = false;
	VxgiVoxelizationThickness = 1.f;

	if (UObject* Outer = GetOuter())
	{
		// Guess the special materials from the file path, disable voxelization on them

		FString OuterName = Outer->GetName();
		if (OuterName.StartsWith("/Engine/"))
			bUsedWithVxgiVoxelization = false;
	}

	// NVCHANGE_END: Add VXGI

#if WITH_EDITORONLY_DATA
	MaterialGraph = NULL;
#endif //WITH_EDITORONLY_DATA
}

void UMaterial::PreSave(const class ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);
#if WITH_EDITOR
	GMaterialsWithDirtyUsageFlags.RemoveAnnotation(this);
#endif
}

void UMaterial::PostInitProperties()
{
	LLM_SCOPE(ELLMTag::Materials);

	Super::PostInitProperties();
	if(!HasAnyFlags(RF_ClassDefaultObject))
	{
		DefaultMaterialInstances[0] = new FDefaultMaterialInstance(this,false,false);
		if(GIsEditor)
		{
			DefaultMaterialInstances[1] = new FDefaultMaterialInstance(this,true,false);
			DefaultMaterialInstances[2] = new FDefaultMaterialInstance(this,false,true);
		}
	}

	// Initialize StateId to something unique, in case this is a new material
	FPlatformMisc::CreateGuid(StateId);

	UpdateResourceAllocations();
}

FMaterialResource* UMaterial::AllocateResource()
{
	LLM_SCOPE(ELLMTag::Materials);

	return new FMaterialResource();
}

void UMaterial::GetUsedTextures(TArray<UTexture*>& OutTextures, EMaterialQualityLevel::Type QualityLevel, bool bAllQualityLevels, ERHIFeatureLevel::Type FeatureLevel, bool bAllFeatureLevels) const
{
	OutTextures.Empty();

	if (QualityLevel == EMaterialQualityLevel::Num)
	{
		QualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
	}

	if (!FPlatformProperties::IsServerOnly())
	{
		for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
		{
			if (QualityLevelIndex != QualityLevel && !bAllQualityLevels)
				continue;

			for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
			{
				const FMaterialResource* CurrentResource = MaterialResources[QualityLevelIndex][FeatureLevelIndex];
				if (CurrentResource == nullptr || (FeatureLevelIndex != FeatureLevel && !bAllFeatureLevels))
					continue;

				const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >* ExpressionsByType[2] =
				{
					&CurrentResource->GetUniform2DTextureExpressions(),
					&CurrentResource->GetUniformCubeTextureExpressions()
				};
				for (int32 TypeIndex = 0; TypeIndex < ARRAY_COUNT(ExpressionsByType); TypeIndex++)
				{
					// Iterate over each of the material's texture expressions.
					for (FMaterialUniformExpressionTexture* Expression : *ExpressionsByType[TypeIndex])
					{
						const bool bAllowOverride = false;
						UTexture* Texture = NULL;
						Expression->GetGameThreadTextureValue(this, *CurrentResource, Texture, bAllowOverride);

						if (Texture)
						{
							OutTextures.AddUnique(Texture);
						}
					}
				}
			}
		}
	}
}

void UMaterial::GetUsedTexturesAndIndices(TArray<UTexture*>& OutTextures, TArray< TArray<int32> >& OutIndices, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel) const
{
	OutTextures.Empty();
	OutIndices.Empty();

	check(QualityLevel != EMaterialQualityLevel::Num && FeatureLevel != ERHIFeatureLevel::Num);

	if (!FPlatformProperties::IsServerOnly())
	{
		const FMaterialResource* CurrentResource = MaterialResources[QualityLevel][FeatureLevel];

		if (CurrentResource)
		{
			const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >* ExpressionsByType[2] =
			{
				&CurrentResource->GetUniform2DTextureExpressions(),
				&CurrentResource->GetUniformCubeTextureExpressions()
			};

			// Try to prevent resizing since this would be expensive.
			OutIndices.Empty(ExpressionsByType[0]->Num() + ExpressionsByType[1]->Num());

			for (int32 TypeIndex = 0; TypeIndex < ARRAY_COUNT(ExpressionsByType); TypeIndex++)
			{
				// Iterate over each of the material's texture expressions.
				for (FMaterialUniformExpressionTexture* Expression : *ExpressionsByType[TypeIndex])
				{
					const bool bAllowOverride = false;
					UTexture* Texture = NULL;
					Expression->GetGameThreadTextureValue(this, *CurrentResource, Texture, bAllowOverride);

					if (Texture)
					{
						int32 InsertIndex = OutTextures.AddUnique(Texture);
						if (InsertIndex >= OutIndices.Num())
						{
							OutIndices.AddDefaulted(InsertIndex - OutIndices.Num() + 1);
						}
						OutIndices[InsertIndex].Add(Expression->GetTextureIndex());
					}
				}
			}
		}
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void UMaterial::LogMaterialsAndTextures(FOutputDevice& Ar, int32 Indent) const
{
	auto World = GetWorld();
	const EMaterialQualityLevel::Type QualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
	const ERHIFeatureLevel::Type FeatureLevel = World ? World->FeatureLevel : GMaxRHIFeatureLevel;

	Ar.Logf(TEXT("%sMaterial: %s"), FCString::Tab(Indent), *GetName());

	if (FPlatformProperties::IsServerOnly())
	{
		Ar.Logf(TEXT("%sNo Textures: IsServerOnly"), FCString::Tab(Indent + 1));
	}
	else
	{
		const FMaterialResource* MaterialResource = MaterialResources[QualityLevel][FeatureLevel];
		if (MaterialResource)
		{
			if (MaterialResource->HasValidGameThreadShaderMap())
			{
				TArray<UTexture*> Textures;
				// GetTextureExpressionValues(MaterialResource, Textures);
				{
					const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >* ExpressionsByType[2] = { &MaterialResource->GetUniform2DTextureExpressions(), &MaterialResource->GetUniformCubeTextureExpressions() };
					for (int32 TypeIndex = 0; TypeIndex < ARRAY_COUNT(ExpressionsByType); TypeIndex++)
					{
						for (FMaterialUniformExpressionTexture* Expression : *ExpressionsByType[TypeIndex])
						{
							UTexture* Texture = NULL;
							Expression->GetGameThreadTextureValue(this, *MaterialResource, Texture, false);
							Textures.AddUnique(Texture);
						}
					}
				}

				for (UTexture* Texture : Textures)
				{
					if (Texture)
					{
						Ar.Logf(TEXT("%s%s"), FCString::Tab(Indent + 1), *Texture->GetName());
					}
				}
			}
			else
			{
				Ar.Logf(TEXT("%sNo Textures : Invalid GameThread ShaderMap"), FCString::Tab(Indent + 1));
			}
		}
		else
		{
			Ar.Logf(TEXT("%sNo Textures : Invalid MaterialResource"), FCString::Tab(Indent + 1));
		}
	}
}
#endif

void UMaterial::OverrideTexture(const UTexture* InTextureToOverride, UTexture* OverrideTexture, ERHIFeatureLevel::Type InFeatureLevel)
{
#if WITH_EDITOR
	bool bShouldRecacheMaterialExpressions = false;
	const bool bES2Preview = false;
	//ERHIFeatureLevel::Type FeatureLevelsToUpdate[2] = { InFeatureLevel, ERHIFeatureLevel::ES2 };
	//int32 NumFeatureLevelsToUpdate = bES2Preview ? 2 : 1;
	ERHIFeatureLevel::Type FeatureLevelsToUpdate[1] = { InFeatureLevel };
	int32 NumFeatureLevelsToUpdate = 1;
	
	for (int32 i = 0; i < NumFeatureLevelsToUpdate; ++i)
	{
		FMaterialResource* Resource = GetMaterialResource(FeatureLevelsToUpdate[i]);
		// Iterate over both the 2D textures and cube texture expressions.
		const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >* ExpressionsByType[2] =
		{
			&Resource->GetUniform2DTextureExpressions(),
			&Resource->GetUniformCubeTextureExpressions()
		};
		for(int32 TypeIndex = 0;TypeIndex < ARRAY_COUNT(ExpressionsByType);TypeIndex++)
		{
			// Iterate over each of the material's texture expressions.
			for (FMaterialUniformExpressionTexture* Expression : *ExpressionsByType[TypeIndex])
			{
				// Evaluate the expression in terms of this material instance.
				const bool bAllowOverride = false;
				UTexture* Texture = NULL;
				Expression->GetGameThreadTextureValue(this,*Resource,Texture,bAllowOverride);

				if( Texture != NULL && Texture == InTextureToOverride )
				{
					// Override this texture!
					Expression->SetTransientOverrideTextureValue( OverrideTexture );
					bShouldRecacheMaterialExpressions = true;
				}
			}
		}
	}

	if (bShouldRecacheMaterialExpressions)
	{
		RecacheUniformExpressions();
		RecacheMaterialInstanceUniformExpressions(this);
	}
#endif // #if WITH_EDITOR
}

void UMaterial::OverrideVectorParameterDefault(FName ParameterName, const FLinearColor& Value, bool bOverride, ERHIFeatureLevel::Type InFeatureLevel)
{
#if WITH_EDITOR
	bool bShouldRecacheMaterialExpressions = false;

	FMaterialResource* Resource = GetMaterialResource(InFeatureLevel);
	// Iterate over both the 2D textures and cube texture expressions.
	const TArray<TRefCountPtr<FMaterialUniformExpression> >& UniformExpressions = Resource->GetUniformVectorParameterExpressions();

	// Iterate over each of the material's texture expressions.
	for (FMaterialUniformExpression* UniformExpression : UniformExpressions)
	{
		if (UniformExpression->GetType() == &FMaterialUniformExpressionVectorParameter::StaticType)
		{
			FMaterialUniformExpressionVectorParameter* VectorExpression = static_cast<FMaterialUniformExpressionVectorParameter*>(UniformExpression);

			if (VectorExpression->GetParameterName() == ParameterName)
			{
				VectorExpression->SetTransientOverrideDefaultValue(Value, bOverride);
				bShouldRecacheMaterialExpressions = true;
			}
		}
	}

	if (bShouldRecacheMaterialExpressions)
	{
		RecacheUniformExpressions();
		RecacheMaterialInstanceUniformExpressions(this);
	}
#endif // #if WITH_EDITOR
}

void UMaterial::OverrideScalarParameterDefault(FName ParameterName, float Value, bool bOverride, ERHIFeatureLevel::Type InFeatureLevel)
{
#if WITH_EDITOR
	bool bShouldRecacheMaterialExpressions = false;

	FMaterialResource* Resource = GetMaterialResource(InFeatureLevel);
	// Iterate over both the 2D textures and cube texture expressions.
	const TArray<TRefCountPtr<FMaterialUniformExpression> >& UniformExpressions = Resource->GetUniformScalarParameterExpressions();

	// Iterate over each of the material's texture expressions.
	for (FMaterialUniformExpression* UniformExpression : UniformExpressions)
	{
		if (UniformExpression->GetType() == &FMaterialUniformExpressionScalarParameter::StaticType)
		{
			FMaterialUniformExpressionScalarParameter* ScalarExpression = static_cast<FMaterialUniformExpressionScalarParameter*>(UniformExpression);

			if (ScalarExpression->GetParameterName() == ParameterName)
			{
				ScalarExpression->SetTransientOverrideDefaultValue(Value, bOverride);
				bShouldRecacheMaterialExpressions = true;
			}
		}
	}

	if (bShouldRecacheMaterialExpressions)
	{
		RecacheUniformExpressions();
		RecacheMaterialInstanceUniformExpressions(this);
	}
#endif // #if WITH_EDITOR
}

float UMaterial::GetScalarParameterDefault(FName ParameterName, ERHIFeatureLevel::Type InFeatureLevel)
{
	if (FApp::CanEverRender())
	{
		FMaterialResource* Resource = GetMaterialResource(InFeatureLevel);
		
		if (ensureAlways(Resource))
		{
			// Iterate over both the 2D textures and cube texture expressions.
			const TArray<TRefCountPtr<FMaterialUniformExpression> >& UniformExpressions = Resource->GetUniformScalarParameterExpressions();

			// Iterate over each of the material's texture expressions.
			for (FMaterialUniformExpression* UniformExpression : UniformExpressions)
			{
				if (UniformExpression->GetType() == &FMaterialUniformExpressionScalarParameter::StaticType)
				{
					FMaterialUniformExpressionScalarParameter* ScalarExpression = static_cast<FMaterialUniformExpressionScalarParameter*>(UniformExpression);

					if (ScalarExpression->GetParameterName() == ParameterName)
					{
						float Value = 0.f;
						ScalarExpression->GetDefaultValue(Value);
						return Value;
					}
				}
			}
		}
	}


	return 0.f;
}

void UMaterial::RecacheUniformExpressions() const
{
	bool bUsingNewLoader = EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME && GEventDrivenLoaderEnabled;

	// Ensure that default material is available before caching expressions.
	if (!bUsingNewLoader)
	{
		UMaterial::GetDefaultMaterial(MD_Surface);
	}

	// Only cache the unselected + unhovered material instance. Selection color
	// can change at runtime and would invalidate the parameter cache.
	if (DefaultMaterialInstances[0])
	{
		DefaultMaterialInstances[0]->CacheUniformExpressions_GameThread();
	}
}

bool UMaterial::GetUsageByFlag(EMaterialUsage Usage) const
{
	bool UsageValue = false;
	switch(Usage)
	{
		case MATUSAGE_SkeletalMesh: UsageValue = bUsedWithSkeletalMesh; break;
		case MATUSAGE_ParticleSprites: UsageValue = bUsedWithParticleSprites; break;
		case MATUSAGE_BeamTrails: UsageValue = bUsedWithBeamTrails; break;
		case MATUSAGE_MeshParticles: UsageValue = bUsedWithMeshParticles; break;
		case MATUSAGE_NiagaraSprites: UsageValue = bUsedWithNiagaraSprites; break;
		case MATUSAGE_NiagaraRibbons: UsageValue = bUsedWithNiagaraRibbons; break;
		case MATUSAGE_NiagaraMeshParticles: UsageValue = bUsedWithNiagaraMeshParticles; break;
		case MATUSAGE_StaticLighting: UsageValue = bUsedWithStaticLighting; break;
		case MATUSAGE_MorphTargets: UsageValue = bUsedWithMorphTargets; break;
		case MATUSAGE_SplineMesh: UsageValue = bUsedWithSplineMeshes; break;
		case MATUSAGE_InstancedStaticMeshes: UsageValue = bUsedWithInstancedStaticMeshes; break;
		case MATUSAGE_Clothing: UsageValue = bUsedWithClothing; break;
		case MATUSAGE_FlexFluidSurfaces: UsageValue = bUsedWithFlexFluidSurfaces; break;
		case MATUSAGE_FlexMeshes: UsageValue = bUsedWithFlexMeshes; break;
		// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
		case MATUSAGE_VxgiVoxelization: UsageValue = bUsedWithVxgiVoxelization; break;
#endif
		// NVCHANGE_END: Add VXGI		default: UE_LOG(LogMaterial, Fatal,TEXT("Unknown material usage: %u"), (int32)Usage);
	};
	return UsageValue;
}

bool UMaterial::IsUsageFlagDirty(EMaterialUsage Usage)
{
#if WITH_EDITOR
	return GMaterialsWithDirtyUsageFlags.GetAnnotation(this).IsUsageFlagDirty(Usage);
#endif
	return false;
}

bool UMaterial::IsCompilingOrHadCompileError(ERHIFeatureLevel::Type InFeatureLevel)
{
	FMaterialResource* Res = GetMaterialResource(InFeatureLevel);

	// should never be the case
	check(Res);

	return Res->GetGameThreadShaderMap() == NULL;
}

void UMaterial::MarkUsageFlagDirty(EMaterialUsage Usage, bool CurrentValue, bool NewValue)
{
#if WITH_EDITOR
	if(CurrentValue != NewValue)
	{
		FMaterialsWithDirtyUsageFlags Annotation = GMaterialsWithDirtyUsageFlags.GetAnnotation(this);
		Annotation.MarkUsageFlagDirty(Usage);
		GMaterialsWithDirtyUsageFlags.AddAnnotation(this,Annotation);
	}
#endif
}

void UMaterial::SetUsageByFlag(EMaterialUsage Usage, bool NewValue)
{
	bool bOldValue = GetUsageByFlag(Usage);
	MarkUsageFlagDirty(Usage, bOldValue, NewValue);

	switch(Usage)
	{
		case MATUSAGE_SkeletalMesh:
		{
			bUsedWithSkeletalMesh = NewValue; break;
		}
		case MATUSAGE_ParticleSprites:
		{
			bUsedWithParticleSprites = NewValue; break;
		}
		case MATUSAGE_BeamTrails:
		{
			bUsedWithBeamTrails = NewValue; break;
		}
		case MATUSAGE_MeshParticles:
		{
			bUsedWithMeshParticles = NewValue; break;
		}
		case MATUSAGE_NiagaraSprites:
		{
			bUsedWithNiagaraSprites = NewValue; break;
		}
		case MATUSAGE_NiagaraRibbons:
		{
			bUsedWithNiagaraRibbons = NewValue; break;
		}
		case MATUSAGE_NiagaraMeshParticles:
		{
			bUsedWithNiagaraMeshParticles = NewValue; break;
		}
		case MATUSAGE_StaticLighting:
		{
			bUsedWithStaticLighting = NewValue; break;
		}
		case MATUSAGE_MorphTargets:
		{
			bUsedWithMorphTargets = NewValue; break;
		}
		case MATUSAGE_SplineMesh:
		{
			bUsedWithSplineMeshes = NewValue; break;
		}
		case MATUSAGE_InstancedStaticMeshes:
		{
			bUsedWithInstancedStaticMeshes = NewValue; break;
		}
		case MATUSAGE_Clothing:
		{
			bUsedWithClothing = NewValue; break;
		}
		case MATUSAGE_FlexFluidSurfaces:
		{
			bUsedWithFlexFluidSurfaces = NewValue; break;
		}
		case MATUSAGE_FlexMeshes:
		{
			bUsedWithFlexMeshes = NewValue; break;
		}
		// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
		case MATUSAGE_VxgiVoxelization:
		{
			bUsedWithVxgiVoxelization = NewValue; break;
		}
#endif
		// NVCHANGE_END: Add VXGI		default: UE_LOG(LogMaterial, Fatal,TEXT("Unknown material usage: %u"), (int32)Usage);
	};
#if WITH_EDITOR
	FEditorSupportDelegates::MaterialUsageFlagsChanged.Broadcast(this, Usage);
#endif
}


FString UMaterial::GetUsageName(EMaterialUsage Usage) const
{
	FString UsageName = TEXT("");
	switch(Usage)
	{
		case MATUSAGE_SkeletalMesh: UsageName = TEXT("bUsedWithSkeletalMesh"); break;
		case MATUSAGE_ParticleSprites: UsageName = TEXT("bUsedWithParticleSprites"); break;
		case MATUSAGE_BeamTrails: UsageName = TEXT("bUsedWithBeamTrails"); break;
		case MATUSAGE_MeshParticles: UsageName = TEXT("bUsedWithMeshParticles"); break;
		case MATUSAGE_NiagaraSprites: UsageName = TEXT("bUsedWithNiagaraSprites"); break;
		case MATUSAGE_NiagaraRibbons: UsageName = TEXT("bUsedWithNiagaraRibbons"); break;
		case MATUSAGE_NiagaraMeshParticles: UsageName = TEXT("bUsedWithNiagaraMeshParticles"); break;
		case MATUSAGE_StaticLighting: UsageName = TEXT("bUsedWithStaticLighting"); break;
		case MATUSAGE_MorphTargets: UsageName = TEXT("bUsedWithMorphTargets"); break;
		case MATUSAGE_SplineMesh: UsageName = TEXT("bUsedWithSplineMeshes"); break;
		case MATUSAGE_InstancedStaticMeshes: UsageName = TEXT("bUsedWithInstancedStaticMeshes"); break;
		case MATUSAGE_Clothing: UsageName = TEXT("bUsedWithClothing"); break;
		case MATUSAGE_FlexFluidSurfaces: UsageName = TEXT("bUsedWithFlexFluidSurfaces"); break;
		case MATUSAGE_FlexMeshes: UsageName = TEXT("bUsedWithFlexMeshes"); break;
		// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
		case MATUSAGE_VxgiVoxelization: UsageName = TEXT("bUsedWithVxgiVoxelization"); break;
#endif
		// NVCHANGE_END: Add VXGI		default: UE_LOG(LogMaterial, Fatal,TEXT("Unknown material usage: %u"), (int32)Usage);
	};
	return UsageName;
}


bool UMaterial::CheckMaterialUsage(EMaterialUsage Usage)
{
	check(IsInGameThread());
	bool bNeedsRecompile = false;
	return SetMaterialUsage(bNeedsRecompile, Usage);
}

bool UMaterial::CheckMaterialUsage_Concurrent(EMaterialUsage Usage) const 
{
	bool bUsageSetSuccessfully = false;
	if (NeedsSetMaterialUsage_Concurrent(bUsageSetSuccessfully, Usage))
	{
		if (IsInGameThread())
		{
			bUsageSetSuccessfully = const_cast<UMaterial*>(this)->CheckMaterialUsage(Usage);
		}	
		else
		{
			struct FCallSMU
			{
				UMaterial* Material;
				EMaterialUsage Usage;

				FCallSMU(UMaterial* InMaterial, EMaterialUsage InUsage)
					: Material(InMaterial)
					, Usage(InUsage)
				{
				}

				void Task()
				{
					Material->CheckMaterialUsage(Usage);
				}
			};
			UE_LOG(LogMaterial, Log, TEXT("Had to pass SMU back to game thread. Please ensure correct material usage flags."));

			TSharedRef<FCallSMU, ESPMode::ThreadSafe> CallSMU = MakeShareable(new FCallSMU(const_cast<UMaterial*>(this), Usage));
			bUsageSetSuccessfully = false;

			DECLARE_CYCLE_STAT(TEXT("FSimpleDelegateGraphTask.CheckMaterialUsage"),
				STAT_FSimpleDelegateGraphTask_CheckMaterialUsage,
				STATGROUP_TaskGraphTasks);

			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(
				FSimpleDelegateGraphTask::FDelegate::CreateThreadSafeSP(CallSMU, &FCallSMU::Task),
				GET_STATID(STAT_FSimpleDelegateGraphTask_CheckMaterialUsage), NULL, ENamedThreads::GameThread_Local
			);
		}
	}
	return bUsageSetSuccessfully;
}

/** Returns true if the given usage flag controls support for a primitive type. */
static bool IsPrimitiveTypeUsageFlag(EMaterialUsage Usage)
{
	return Usage == MATUSAGE_SkeletalMesh
		|| Usage == MATUSAGE_ParticleSprites
		|| Usage == MATUSAGE_BeamTrails
		|| Usage == MATUSAGE_MeshParticles
		|| Usage == MATUSAGE_NiagaraSprites
		|| Usage == MATUSAGE_NiagaraRibbons
		|| Usage == MATUSAGE_NiagaraMeshParticles
		|| Usage == MATUSAGE_MorphTargets
		|| Usage == MATUSAGE_SplineMesh
		|| Usage == MATUSAGE_InstancedStaticMeshes
		|| Usage == MATUSAGE_Clothing
		|| Usage == MATUSAGE_FlexFluidSurfaces
		|| Usage == MATUSAGE_FlexMeshes;
}

bool UMaterial::NeedsSetMaterialUsage_Concurrent(bool &bOutHasUsage, EMaterialUsage Usage) const
{
	bOutHasUsage = true;
	// Material usage is only relevant for materials that can be applied onto a mesh / use with different vertex factories.
	if (MaterialDomain != MD_Surface && MaterialDomain != MD_DeferredDecal && MaterialDomain != MD_Volume)
	{
		bOutHasUsage = false;
		return false;
	}
	// Check that the material has been flagged for use with the given usage flag.
	if(!GetUsageByFlag(Usage) && !bUsedAsSpecialEngineMaterial)
	{
		uint32 UsageFlagBit = (1 << (uint32)Usage);
		if ((UsageFlagWarnings & UsageFlagBit) == 0)
		{
			// This will be overwritten later by SetMaterialUsage, since we are saying that it needs to be called with the return value
			bOutHasUsage = false;
			return true;
		}
		else
		{
			// We have already warned about this, so we aren't going to warn or compile or set anything this time
			bOutHasUsage = false;
			return false;
		}
	}
	return false;
}

bool UMaterial::SetMaterialUsage(bool &bNeedsRecompile, EMaterialUsage Usage)
{
	bNeedsRecompile = false;

	// Material usage is only relevant for materials that can be applied onto a mesh / use with different vertex factories.
	if (MaterialDomain != MD_Surface && MaterialDomain != MD_DeferredDecal && MaterialDomain != MD_Volume)
	{
		return false;
	}

	// Check that the material has been flagged for use with the given usage flag.
	if(!GetUsageByFlag(Usage) && !bUsedAsSpecialEngineMaterial)
	{
		// For materials which do not have their bUsedWith____ correctly set the DefaultMaterial<type> should be used in game
		// Leaving this GIsEditor ensures that in game on PC will not look different than on the Consoles as we will not be compiling shaders on the fly
		if( GIsEditor && !FApp::IsGame() && bAutomaticallySetUsageInEditor )
		{
			check(IsInGameThread());
			//Do not warn the user during automation testing
			if (!GIsAutomationTesting)
			{
				UE_LOG(LogMaterial, Warning, TEXT("Material %s needed to have new flag set %s !"), *GetPathName(), *GetUsageName(Usage));
			}

			// Open a material update context so this material can be modified safely.
			FMaterialUpdateContext UpdateContext(
				// We need to sync with the rendering thread but don't reregister components
				// because SetMaterialUsage may be called during registration!
				FMaterialUpdateContext::EOptions::SyncWithRenderingThread
				);
			UpdateContext.AddMaterial(this);

			// If the flag is missing in the editor, set it, and recompile shaders.
			SetUsageByFlag(Usage, true);
			bNeedsRecompile = true;

			// Compile and force the Id to be regenerated, since we changed the material in a way that changes compilation
			CacheResourceShadersForRendering(true);

			// Mark the package dirty so that hopefully it will be saved with the new usage flag.
			// This is important because the only way an artist can fix an infinite 'compile on load' scenario is by saving with the new usage flag
			if (!MarkPackageDirty())
			{
#if WITH_EDITOR
				// The package could not be marked as dirty as we're loading content in the editor. Add a Map Check error to notify the user.
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("Material"), FText::FromString(*GetPathName()));
				Arguments.Add(TEXT("Usage"), FText::FromString(*GetUsageName(Usage)));
				FMessageLog("MapCheck").Warning()
					->AddToken(FUObjectToken::Create(this))
					->AddToken(FTextToken::Create(FText::Format(LOCTEXT("MapCheck_SetMaterialUsage", "Material {Material} was missing the usage flag {Usage}. If the material asset is not re-saved, it may not render correctly when run outside the editor."), Arguments)))
					->AddToken(FActionToken::Create(LOCTEXT("MapCheck_FixMaterialUsage", "Fix"), LOCTEXT("MapCheck_FixMaterialUsage_Desc", "Click to set the usage flag correctly and mark the asset file as needing to be saved."), FOnActionTokenExecuted::CreateUObject(this, &UMaterial::FixupMaterialUsageAfterLoad), true));
				FMessageLog("MapCheck").Open(EMessageSeverity::Warning);
#endif
			}
		}
		else
		{
			uint32 UsageFlagBit = (1 << (uint32)Usage);
			if ((UsageFlagWarnings & UsageFlagBit) == 0)
			{
				UE_LOG(LogMaterial, Warning, TEXT("Material %s missing %s=True! Default Material will be used in game."), *GetPathName(), *GetUsageName(Usage));
				
				if (bAutomaticallySetUsageInEditor)
				{
					UE_LOG(LogMaterial, Warning, TEXT("     The material will recompile every editor launch until resaved."));
				}
				else if (GIsEditor && !FApp::IsGame())
				{
#if WITH_EDITOR
					FFormatNamedArguments Args;
					Args.Add(TEXT("UsageName"), FText::FromString(GetUsageName(Usage)));
					FNotificationInfo Info(FText::Format(LOCTEXT("CouldntSetMaterialUsage","Material didn't allow automatic setting of usage flag {UsageName} needed to render on this component, using Default Material instead."), Args));
					Info.ExpireDuration = 5.0f;
					Info.bUseSuccessFailIcons = true;

					// Give the user feedback as to why they are seeing the default material
					FSlateNotificationManager::Get().AddNotification(Info);
#endif
				}

				UsageFlagWarnings |= UsageFlagBit;
			}

			// Return failure if the flag is missing in game, since compiling shaders in game is not supported on some platforms.
			return false;
		}
	}
	return true;
}

#if WITH_EDITOR
void UMaterial::FixupMaterialUsageAfterLoad()
{
	// All we need to do here is mark the package dirty as the usage itself was set on load.
	MarkPackageDirty();
}
#endif

void UMaterial::GetAllVectorParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const
{
	OutParameterNames.Reset();
	OutParameterIds.Reset();
	GetAllParameterNames<UMaterialExpressionVectorParameter>(OutParameterNames, OutParameterIds);
}
void UMaterial::GetAllScalarParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const
{
	OutParameterNames.Reset();
	OutParameterIds.Reset();
	GetAllParameterNames<UMaterialExpressionScalarParameter>(OutParameterNames, OutParameterIds);
}
void UMaterial::GetAllTextureParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const
{
	OutParameterNames.Reset();
	OutParameterIds.Reset();
	GetAllParameterNames<UMaterialExpressionTextureSampleParameter>(OutParameterNames, OutParameterIds);
}

void UMaterial::GetAllFontParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const
{
	OutParameterNames.Reset();
	OutParameterIds.Reset();
	GetAllParameterNames<UMaterialExpressionFontSampleParameter>(OutParameterNames, OutParameterIds);
}

void UMaterial::GetAllStaticSwitchParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const
{
	OutParameterNames.Reset();
	OutParameterIds.Reset();
	GetAllParameterNames<UMaterialExpressionStaticBoolParameter>(OutParameterNames, OutParameterIds);
}

void UMaterial::GetAllStaticComponentMaskParameterNames(TArray<FName> &OutParameterNames, TArray<FGuid> &OutParameterIds) const
{
	OutParameterNames.Reset();
	OutParameterIds.Reset();
	GetAllParameterNames<UMaterialExpressionStaticComponentMaskParameter>(OutParameterNames, OutParameterIds);
}

extern FPostProcessMaterialNode* IteratePostProcessMaterialNodes(const FFinalPostProcessSettings& Dest, const UMaterial* Material, FBlendableEntry*& Iterator);

void UMaterialInterface::OverrideBlendableSettings(class FSceneView& View, float Weight) const
{
	check(Weight > 0.0f && Weight <= 1.0f);

	FFinalPostProcessSettings& Dest = View.FinalPostProcessSettings;

	const UMaterial* Base = GetMaterial();

	//	should we use UMaterial::GetDefaultMaterial(Domain) instead of skipping the material

	if(!Base || Base->MaterialDomain != MD_PostProcess || !View.State)
	{
		return;
	}

	FBlendableEntry* Iterator = 0;

	FPostProcessMaterialNode* DestNode = IteratePostProcessMaterialNodes(Dest, Base, Iterator);

	// is this the first one of this material?
	if(!DestNode)
	{
		UMaterialInstanceDynamic* InitialMID = View.State->GetReusableMID((UMaterialInterface*)this);

		if(InitialMID)
		{
			// If the initial node is faded in partly we add the base material (it's assumed to be the neutral state, see docs)
			// and then blend in the material instance (it it's the base there is no need for that)
			const UMaterialInterface* SourceData = (Weight < 1.0f) ? Base : this;

			InitialMID->CopyScalarAndVectorParameters(*SourceData, View.FeatureLevel);

			FPostProcessMaterialNode InitialNode(InitialMID, Base->BlendableLocation, Base->BlendablePriority);

			// no blending needed on this one
			FPostProcessMaterialNode* InitialDestNode = Dest.BlendableManager.PushBlendableData(1.0f, InitialNode);

			if(Weight < 1.0f && this != Base)
			{
				// We are not done, we still need to fade with SrcMID
				DestNode = InitialDestNode;
			}
		}
	}

	if(DestNode)
	{
		// we apply this material on top of an existing one
		UMaterialInstanceDynamic* DestMID = DestNode->GetMID();
		check(DestMID);

		UMaterialInstance* SrcMID = (UMaterialInstance*)this;
		check(SrcMID);

		// Here we could check for Weight=1.0 and use copy instead of interpolate but that case quite likely not intended anyway.

		// a material already exists, blend (Scalar and Vector parameters) with existing ones
		DestMID->K2_InterpolateMaterialInstanceParams(DestMID, SrcMID, Weight);
	}
}

UMaterial* UMaterial::GetMaterial()
{
	return this;
}

const UMaterial* UMaterial::GetMaterial() const
{
	return this;
}

const UMaterial* UMaterial::GetMaterial_Concurrent(TMicRecursionGuard&) const
{
	return this;
}

bool UMaterial::GetGroupName(FName ParameterName, FName& OutDesc) const
{
	for (const UMaterialExpression* Expression : Expressions)
	{
		// Parameter is a basic Expression Parameter
		if (Expression->IsA<UMaterialExpressionParameter>())
		{
			const UMaterialExpressionParameter* Parameter = CastChecked<const UMaterialExpressionParameter>(Expression);
			if (Parameter->ParameterName == ParameterName)
			{
				OutDesc = Parameter->Group;
				return true;
			}
		}
		// Parameter is a Texture Sample Parameter
		else if (Expression->IsA<UMaterialExpressionTextureSampleParameter>())
		{
			const UMaterialExpressionTextureSampleParameter* Parameter = CastChecked<const UMaterialExpressionTextureSampleParameter>(Expression);
			if (Parameter->ParameterName == ParameterName)
			{
				OutDesc = Parameter->Group;
				return true;
			}
		}
		// Parameter is a Font Sample Parameter
		else if (Expression->IsA<UMaterialExpressionFontSampleParameter>())
		{
			const UMaterialExpressionFontSampleParameter* Parameter = CastChecked<const UMaterialExpressionFontSampleParameter>(Expression);
			if (Parameter->ParameterName == ParameterName)
			{
				OutDesc = Parameter->Group;
				return true;
			}
		}
		// Parameter is a function call
		else if (Expression->IsA<UMaterialExpressionMaterialFunctionCall>())
		{
			const UMaterialExpressionMaterialFunctionCall* FunctionCall = CastChecked<const UMaterialExpressionMaterialFunctionCall>(Expression);
			if (FunctionCall->MaterialFunction)
			{
				TArray<UMaterialFunction*> Functions;
				Functions.Add(FunctionCall->MaterialFunction);
				FunctionCall->MaterialFunction->GetDependentFunctions(Functions);

				for (UMaterialFunction* Function : Functions)
				{
					for (UMaterialExpression* FunctionExpression : Function->FunctionExpressions)
					{
						// Parameter is a basic Expression Parameter
						if (FunctionExpression->IsA<UMaterialExpressionParameter>())
						{
							const UMaterialExpressionParameter* Parameter = CastChecked<const UMaterialExpressionParameter>(FunctionExpression);
							if (Parameter->ParameterName == ParameterName)
							{
								OutDesc = Parameter->Group;
								return true;
							}
						}
						// Parameter is a Texture Sample Parameter
						else if (FunctionExpression->IsA<UMaterialExpressionTextureSampleParameter>())
						{
							const UMaterialExpressionTextureSampleParameter* Parameter = CastChecked<const UMaterialExpressionTextureSampleParameter>(FunctionExpression);
							if (Parameter->ParameterName == ParameterName)
							{
								OutDesc = Parameter->Group;
								return true;
							}
						}
						// Parameter is a Font Sample Parameter
						else if (FunctionExpression->IsA<UMaterialExpressionFontSampleParameter>())
						{
							const UMaterialExpressionFontSampleParameter* Parameter = CastChecked<const UMaterialExpressionFontSampleParameter>(FunctionExpression);
							if (Parameter->ParameterName == ParameterName)
							{
								OutDesc = Parameter->Group;
								return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}

bool UMaterial::GetParameterDesc(FName ParameterName, FString& OutDesc) const
{
	for (const UMaterialExpression* Expression : Expressions)
	{
		// Parameter is a basic Expression Parameter
		if (Expression->IsA<UMaterialExpressionParameter>())
		{
			const UMaterialExpressionParameter* Parameter = CastChecked<const UMaterialExpressionParameter>(Expression);
			if (Parameter->ParameterName == ParameterName)
			{
				OutDesc = Parameter->Desc;
				return true;
			}
		}
		// Parameter is a Texture Sample Parameter
		else if (Expression->IsA<UMaterialExpressionTextureSampleParameter>())
		{
			const UMaterialExpressionTextureSampleParameter* Parameter = CastChecked<const UMaterialExpressionTextureSampleParameter>(Expression);
			if (Parameter->ParameterName == ParameterName)
			{
				OutDesc = Parameter->Desc;
				return true;
			}
		}
		// Parameter is a Font Sample Parameter
		else if (Expression->IsA<UMaterialExpressionFontSampleParameter>())
		{
			const UMaterialExpressionFontSampleParameter* Parameter = CastChecked<const UMaterialExpressionFontSampleParameter>(Expression);
			if (Parameter->ParameterName == ParameterName)
			{
				OutDesc = Parameter->Desc;
				return true;
			}
		}
		// Parameter is a function call
		else if (Expression->IsA<UMaterialExpressionMaterialFunctionCall>())
		{
			const UMaterialExpressionMaterialFunctionCall* FunctionCall = CastChecked<const UMaterialExpressionMaterialFunctionCall>(Expression);
			if (FunctionCall->MaterialFunction)
			{
				TArray<UMaterialFunction*> Functions;
				Functions.Add(FunctionCall->MaterialFunction);
				FunctionCall->MaterialFunction->GetDependentFunctions(Functions);

				for (UMaterialFunction* Function : Functions)
				{
					for (UMaterialExpression* FunctionExpression : Function->FunctionExpressions)
					{
						// Parameter is a basic Expression Parameter
						if (FunctionExpression->IsA<UMaterialExpressionParameter>())
						{
							const UMaterialExpressionParameter* Parameter = CastChecked<const UMaterialExpressionParameter>(FunctionExpression);
							if (Parameter->ParameterName == ParameterName)
							{
								OutDesc = Parameter->Desc;
								return true;
							}
						}
						// Parameter is a Texture Sample Parameter
						else if (FunctionExpression->IsA<UMaterialExpressionTextureSampleParameter>())
						{
							const UMaterialExpressionTextureSampleParameter* Parameter = CastChecked<const UMaterialExpressionTextureSampleParameter>(FunctionExpression);
							if (Parameter->ParameterName == ParameterName)
							{
								OutDesc = Parameter->Desc;
								return true;
							}
						}
						// Parameter is a Font Sample Parameter
						else if (FunctionExpression->IsA<UMaterialExpressionFontSampleParameter>())
						{
							const UMaterialExpressionFontSampleParameter* Parameter = CastChecked<const UMaterialExpressionFontSampleParameter>(FunctionExpression);
							if (Parameter->ParameterName == ParameterName)
							{
								OutDesc = Parameter->Desc;
								return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}

bool UMaterial::GetVectorParameterValue(FName ParameterName, FLinearColor& OutValue) const
{
	for (const UMaterialExpression* Expression : Expressions)
	{
		if (const UMaterialExpressionVectorParameter* Parameter = Cast<const UMaterialExpressionVectorParameter>(Expression))
		{
			if (Parameter->IsNamedParameter(ParameterName, OutValue))
			{
				// Warning: in the case of duplicate parameters with different default values, this will find the first in the expression array, not necessarily the one that's used for rendering
				return true;
			}
		}
		else if (const UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<const UMaterialExpressionMaterialFunctionCall>(Expression))
		{
			if (FunctionCall->MaterialFunction)
			{
				TArray<UMaterialFunction*> Functions;
				Functions.Add(FunctionCall->MaterialFunction);
				FunctionCall->MaterialFunction->GetDependentFunctions(Functions);

				for (UMaterialFunction* Function : Functions)
				{
					for (UMaterialExpression* FunctionExpression : Function->FunctionExpressions)
					{
						if (const UMaterialExpressionVectorParameter* FunctionExpressionParameter = Cast<const UMaterialExpressionVectorParameter>(FunctionExpression))
						{
							if (FunctionExpressionParameter->IsNamedParameter(ParameterName, OutValue))
							{
								return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}

bool UMaterial::GetScalarParameterValue(FName ParameterName, float& OutValue) const
{
	for (const UMaterialExpression* Expression : Expressions)
	{
		if (const UMaterialExpressionScalarParameter* Parameter = Cast<const UMaterialExpressionScalarParameter>(Expression))
		{
			if (Parameter->IsNamedParameter(ParameterName, OutValue))
			{
				// Warning: in the case of duplicate parameters with different default values, this will find the first in the expression array, not necessarily the one that's used for rendering
				return true;
			}
		}
		else if (const UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<const UMaterialExpressionMaterialFunctionCall>(Expression))
		{
			if (FunctionCall->MaterialFunction)
			{
				TArray<UMaterialFunction*> Functions;
				Functions.Add(FunctionCall->MaterialFunction);
				FunctionCall->MaterialFunction->GetDependentFunctions(Functions);

				for (UMaterialFunction* Function : Functions)
				{
					for (UMaterialExpression* FunctionExpression : Function->FunctionExpressions)
					{
						if (const UMaterialExpressionScalarParameter* FunctionExpressionParameter = Cast<const UMaterialExpressionScalarParameter>(FunctionExpression))
						{
							if (FunctionExpressionParameter->IsNamedParameter(ParameterName, OutValue))
							{
								return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}

bool UMaterial::GetScalarParameterSliderMinMax(FName ParameterName, float& OutSliderMin, float& OutSliderMax) const
{
	float Value = 0;

	for (const UMaterialExpression* Expression : Expressions)
	{
		if (Expression->IsA<UMaterialExpressionScalarParameter>())
		{
			const UMaterialExpressionScalarParameter* Parameter = CastChecked<const UMaterialExpressionScalarParameter>(Expression);
			if (Parameter->IsNamedParameter(ParameterName, Value))
			{
				OutSliderMin = Parameter->SliderMin;
				OutSliderMax = Parameter->SliderMax;
				// Warning: in the case of duplicate parameters with different default values, this will find the first in the expression array, not necessarily the one that's used for rendering
				return true;
			}
		}
		else if (Expression->IsA<UMaterialExpressionMaterialFunctionCall>())
		{
			const UMaterialExpressionMaterialFunctionCall* FunctionCall = CastChecked<const UMaterialExpressionMaterialFunctionCall>(Expression);
			if (FunctionCall->MaterialFunction)
			{
				TArray<UMaterialFunction*> Functions;
				Functions.Add(FunctionCall->MaterialFunction);
				FunctionCall->MaterialFunction->GetDependentFunctions(Functions);

				for (UMaterialFunction* Function : Functions)
				{
					for (UMaterialExpression* FunctionExpression : Function->FunctionExpressions)
					{
						if (FunctionExpression->IsA<UMaterialExpressionScalarParameter>())
						{
							const UMaterialExpressionScalarParameter* Parameter = CastChecked<const UMaterialExpressionScalarParameter>(FunctionExpression);
							if (Parameter->IsNamedParameter(ParameterName, Value))
							{
								OutSliderMin = Parameter->SliderMin;
								OutSliderMax = Parameter->SliderMax;
								return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}

bool UMaterial::GetTextureParameterValue(FName ParameterName, UTexture*& OutValue) const
{
	for (const UMaterialExpression* Expression : Expressions)
	{
		if (const UMaterialExpressionTextureSampleParameter* Parameter = Cast<const UMaterialExpressionTextureSampleParameter>(Expression))
		{
			if (Parameter->IsNamedParameter(ParameterName, OutValue))
			{
				// Warning: in the case of duplicate parameters with different default values, this will find the first in the expression array, not necessarily the one that's used for rendering
				return true;
			}
		}
		else if (const UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<const UMaterialExpressionMaterialFunctionCall>(Expression))
		{
			if (FunctionCall->MaterialFunction)
			{
				TArray<UMaterialFunction*> Functions;
				Functions.Add(FunctionCall->MaterialFunction);
				FunctionCall->MaterialFunction->GetDependentFunctions(Functions);

				for (UMaterialFunction* Function : Functions)
				{
					for (UMaterialExpression* FunctionExpression : Function->FunctionExpressions)
					{
						if (const UMaterialExpressionTextureSampleParameter* FunctionExpressionParameter = Cast<const UMaterialExpressionTextureSampleParameter>(FunctionExpression))
						{
							if (FunctionExpressionParameter->IsNamedParameter(ParameterName, OutValue))
							{
								return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}

bool UMaterial::GetFontParameterValue(FName ParameterName, UFont*& OutFontValue, int32& OutFontPage) const
{
	for (const UMaterialExpression* Expression : Expressions)
	{
		if (const UMaterialExpressionFontSampleParameter* Parameter = Cast<const UMaterialExpressionFontSampleParameter>(Expression))
		{
			if (Parameter->IsNamedParameter(ParameterName, OutFontValue, OutFontPage))
			{
				// Warning: in the case of duplicate parameters with different default values, this will find the first in the expression array, not necessarily the one that's used for rendering
				return true;
			}
		}
		else if (const UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<const UMaterialExpressionMaterialFunctionCall>(Expression))
		{
			if (FunctionCall->MaterialFunction)
			{
				TArray<UMaterialFunction*> Functions;
				Functions.Add(FunctionCall->MaterialFunction);
				FunctionCall->MaterialFunction->GetDependentFunctions(Functions);

				for (UMaterialFunction* Function : Functions)
				{
					for (UMaterialExpression* FunctionExpression : Function->FunctionExpressions)
					{
						if (const UMaterialExpressionFontSampleParameter* FunctionExpressionParameter = Cast<const UMaterialExpressionFontSampleParameter>(FunctionExpression))
						{
							if (FunctionExpressionParameter->IsNamedParameter(ParameterName, OutFontValue, OutFontPage))
							{
								return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}


bool UMaterial::GetStaticSwitchParameterValue(FName ParameterName, bool& OutValue, FGuid& OutExpressionGuid) const
{
	for (const UMaterialExpression* Expression : Expressions)
	{
		if (const UMaterialExpressionStaticBoolParameter* Parameter = Cast<const UMaterialExpressionStaticBoolParameter>(Expression))
		{
			if (Parameter->IsNamedParameter(ParameterName, OutValue, OutExpressionGuid))
			{
				// Warning: in the case of duplicate parameters with different default values, this will find the first in the expression array, not necessarily the one that's used for rendering
				return true;
			}
		}
		else if (const UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<const UMaterialExpressionMaterialFunctionCall>(Expression))
		{
			if (FunctionCall->MaterialFunction)
			{
				TArray<UMaterialFunction*> Functions;
				Functions.Add(FunctionCall->MaterialFunction);
				FunctionCall->MaterialFunction->GetDependentFunctions(Functions);

				for (UMaterialFunction* Function : Functions)
				{
					for (UMaterialExpression* FunctionExpression : Function->FunctionExpressions)
					{
						if (const UMaterialExpressionStaticBoolParameter* FunctionExpressionParameter = Cast<const UMaterialExpressionStaticBoolParameter>(FunctionExpression))
						{
							if (FunctionExpressionParameter->IsNamedParameter(ParameterName, OutValue, OutExpressionGuid))
							{
								return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}


bool UMaterial::GetStaticComponentMaskParameterValue(FName ParameterName, bool& OutR, bool& OutG, bool& OutB, bool& OutA, FGuid& OutExpressionGuid) const
{
	for (const UMaterialExpression* Expression : Expressions)
	{
		if (const UMaterialExpressionStaticComponentMaskParameter* Parameter = Cast<const UMaterialExpressionStaticComponentMaskParameter>(Expression))
		{
			if (Parameter->IsNamedParameter(ParameterName, OutR, OutG, OutB, OutA, OutExpressionGuid))
			{
				// Warning: in the case of duplicate parameters with different default values, this will find the first in the expression array, not necessarily the one that's used for rendering
				return true;
			}
		}
		else if (const UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<const UMaterialExpressionMaterialFunctionCall>(Expression))
		{
			if (FunctionCall->MaterialFunction)
			{
				TArray<UMaterialFunction*> Functions;
				Functions.Add(FunctionCall->MaterialFunction);
				FunctionCall->MaterialFunction->GetDependentFunctions(Functions);

				for (UMaterialFunction* Function : Functions)
				{
					for (UMaterialExpression* FunctionExpression : Function->FunctionExpressions)
					{
						if (const UMaterialExpressionStaticComponentMaskParameter* FunctionExpressionParameter = Cast<const UMaterialExpressionStaticComponentMaskParameter>(FunctionExpression))
						{
							if (FunctionExpressionParameter->IsNamedParameter(ParameterName, OutR, OutG, OutB, OutA, OutExpressionGuid))
							{
								return true;
							}
						}
					}
				}
			}
		}
	}

	return false;
}


bool UMaterial::GetTerrainLayerWeightParameterValue(FName ParameterName, int32& OutWeightmapIndex, FGuid &OutExpressionGuid) const
{
	bool bSuccess = false;
	OutWeightmapIndex = INDEX_NONE;
	bSuccess = true;
	return bSuccess;
}



bool UMaterial::GetRefractionSettings(float& OutBiasValue) const
{
	OutBiasValue = RefractionDepthBias;
	return true;
}

FMaterialRenderProxy* UMaterial::GetRenderProxy(bool Selected,bool Hovered) const
{
	check(!( Selected || Hovered ) || GIsEditor);
	return DefaultMaterialInstances[Selected ? 1 : ( Hovered ? 2 : 0 )];
}

UPhysicalMaterial* UMaterial::GetPhysicalMaterial() const
{
	return (PhysMaterial != NULL) ? PhysMaterial : GEngine->DefaultPhysMaterial;
}

/** Helper functions for text output of properties... */
#ifndef CASE_ENUM_TO_TEXT
#define CASE_ENUM_TO_TEXT(txt) case txt: return TEXT(#txt);
#endif

#ifndef TEXT_TO_ENUM
#define TEXT_TO_ENUM(eVal, txt)		if (FCString::Stricmp(TEXT(#eVal), txt) == 0)	return eVal;
#endif

const TCHAR* UMaterial::GetMaterialShadingModelString(EMaterialShadingModel InMaterialShadingModel)
{
	switch (InMaterialShadingModel)
	{
		FOREACH_ENUM_EMATERIALSHADINGMODEL(CASE_ENUM_TO_TEXT)
	}
	return TEXT("MSM_DefaultLit");
}

EMaterialShadingModel UMaterial::GetMaterialShadingModelFromString(const TCHAR* InMaterialShadingModelStr)
{
	#define TEXT_TO_SHADINGMODEL(m) TEXT_TO_ENUM(m, InMaterialShadingModelStr);
	FOREACH_ENUM_EMATERIALSHADINGMODEL(TEXT_TO_SHADINGMODEL)
	#undef TEXT_TO_SHADINGMODEL
	return MSM_DefaultLit;
}

const TCHAR* UMaterial::GetBlendModeString(EBlendMode InBlendMode)
{
	switch (InBlendMode)
	{
		FOREACH_ENUM_EBLENDMODE(CASE_ENUM_TO_TEXT)
	}
	return TEXT("BLEND_Opaque");
}

EBlendMode UMaterial::GetBlendModeFromString(const TCHAR* InBlendModeStr)
{
	#define TEXT_TO_BLENDMODE(b) TEXT_TO_ENUM(b, InBlendModeStr);
	FOREACH_ENUM_EBLENDMODE(TEXT_TO_BLENDMODE)
	#undef TEXT_TO_BLENDMODE
	return BLEND_Opaque;
}

static FAutoConsoleVariable GCompileMaterialsForShaderFormatCVar(
	TEXT("r.CompileMaterialsForShaderFormat"),
	TEXT(""),
	TEXT("When enabled, compile materials for this shader format in addition to those for the running platform.\n")
	TEXT("Note that these shaders are compiled and immediately tossed. This is only useful when directly inspecting output via r.DebugDumpShaderInfo.")
	);

void UMaterial::GetForceRecompileTextureIdsHash(FSHAHash &TextureReferencesHash)
{
	TArray<UTexture*> ForceRecompileTextures;
	for (const UMaterialExpression *MaterialExpression : Expressions)
	{
		if (MaterialExpression == nullptr)
		{
			continue;
		}
		TArray<UTexture*> ExpressionForceRecompileTextures;
		MaterialExpression->GetTexturesForceMaterialRecompile(ExpressionForceRecompileTextures);
		for (UTexture *ForceRecompileTexture : ExpressionForceRecompileTextures)
		{
			ForceRecompileTextures.AddUnique(ForceRecompileTexture);
		}
	}
	if (ForceRecompileTextures.Num() <= 0)
	{
		//There is no Texture that trig a recompile of the material, nothing to add to the hash
		return;
	}

	FSHA1 TextureCompileDependencies;
	FString OriginalHash = TextureReferencesHash.ToString();
	TextureCompileDependencies.UpdateWithString(*OriginalHash, OriginalHash.Len());

	for (UTexture *ForceRecompileTexture : ForceRecompileTextures)
	{
		FString TextureGuidString = ForceRecompileTexture->GetLightingGuid().ToString();
		TextureCompileDependencies.UpdateWithString(*TextureGuidString, TextureGuidString.Len());
	}

	TextureCompileDependencies.Final();
	TextureCompileDependencies.GetHash(&TextureReferencesHash.Hash[0]);
}

bool UMaterial::IsTextureForceRecompileCacheRessource(UTexture *Texture)
{
	for (const UMaterialExpression *MaterialExpression : Expressions)
	{
		if (MaterialExpression == nullptr)
		{
			continue;
		}
		TArray<UTexture*> ExpressionForceRecompileTextures;
		MaterialExpression->GetTexturesForceMaterialRecompile(ExpressionForceRecompileTextures);
		for (UTexture *ForceRecompileTexture : ExpressionForceRecompileTextures)
		{
			if (Texture == ForceRecompileTexture)
			{
				return true;
			}
		}
	}
	return false;
}

#if WITH_EDITOR

void UMaterial::UpdateMaterialShaderCacheAndTextureReferences()
{
	// If the material changes, then the debug view material must reset to prevent parameters mismatch
	void ClearAllDebugViewMaterials();
	ClearAllDebugViewMaterials();

	//Cancel any current compilation jobs that are in flight for this material.
	CancelOutstandingCompilation();

	//Force a recompute of the DDC key
	CacheResourceShadersForRendering(true);
	RecacheMaterialInstanceUniformExpressions(this);
	
	// Ensure that the ReferencedTextureGuids array is up to date.
	if (GIsEditor)
	{
		UpdateLightmassTextureTracking();
	}

	// Ensure that any components with static elements using this material have their render state recreated
	// so changes are propagated to them. The preview material is only applied to the preview mesh component,
	// and that reregister is handled by the material editor.
	if (!bIsPreviewMaterial && !bIsMaterialEditorStatsMaterial)
	{
		FGlobalComponentRecreateRenderStateContext RecreateComponentsRenderState;
	}
	// needed for UMaterial as it doesn't have the InitResources() override where this is called
	PropagateDataToMaterialProxy();
}

#endif //WITH_EDITOR

void UMaterial::CacheResourceShadersForRendering(bool bRegenerateId)
{
	if (bRegenerateId)
	{
		// Regenerate this material's Id if requested
		FlushResourceShaderMaps();
	}

	UpdateResourceAllocations();

	if (FApp::CanEverRender())
	{
		uint32 FeatureLevelsToCompile = GetFeatureLevelsToCompileForRendering();
		EMaterialQualityLevel::Type ActiveQualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
		TArray<FMaterialResource*> ResourcesToCache;

		while (FeatureLevelsToCompile != 0)
		{
			ERHIFeatureLevel::Type FeatureLevel = (ERHIFeatureLevel::Type)FBitSet::GetAndClearNextBit(FeatureLevelsToCompile); 
			EShaderPlatform ShaderPlatform = GShaderPlatformForFeatureLevel[FeatureLevel];

			// Only cache shaders for the quality level that will actually be used to render
			ResourcesToCache.Reset();

			FMaterialResource* MaterialResource = MaterialResources[ActiveQualityLevel][FeatureLevel];

			check(MaterialResource);

			ResourcesToCache.Add(MaterialResource);
			CacheShadersForResources(ShaderPlatform, ResourcesToCache, true);
		}

		FString AdditionalFormatToCache = GCompileMaterialsForShaderFormatCVar->GetString();
		if (!AdditionalFormatToCache.IsEmpty())
		{
			EShaderPlatform AdditionalPlatform = ShaderFormatToLegacyShaderPlatform(FName(*AdditionalFormatToCache));
			if (AdditionalPlatform != SP_NumPlatforms)
			{
				ResourcesToCache.Reset();
				CacheResourceShadersForCooking(AdditionalPlatform,ResourcesToCache);
				for (int32 i = 0; i < ResourcesToCache.Num(); ++i)
				{
					FMaterialResource* Resource = ResourcesToCache[i];
					delete Resource;
				}
				ResourcesToCache.Reset();
			}
		}

		RecacheUniformExpressions();
	}
}

void UMaterial::CacheResourceShadersForCooking(EShaderPlatform ShaderPlatform, TArray<FMaterialResource*>& OutCachedMaterialResources)
{
	TArray<FMaterialResource*> ResourcesToCache;
	ERHIFeatureLevel::Type TargetFeatureLevel = GetMaxSupportedFeatureLevel(ShaderPlatform);

	TArray<bool, TInlineAllocator<EMaterialQualityLevel::Num> > QualityLevelsUsed;
	GetQualityLevelUsage(QualityLevelsUsed, ShaderPlatform);

	bool bAnyQualityLevelUsed = false;

	for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
	{
		bAnyQualityLevelUsed = bAnyQualityLevelUsed || QualityLevelsUsed[QualityLevelIndex];
	}

	for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
	{
		// Add all quality levels if multiple are needed (due to different node graphs), otherwise just add the high quality entry
		if (bAnyQualityLevelUsed || QualityLevelIndex == EMaterialQualityLevel::High)
		{
			FMaterialResource* NewResource = AllocateResource();
			NewResource->SetMaterial(this, (EMaterialQualityLevel::Type)QualityLevelIndex, QualityLevelsUsed[QualityLevelIndex], (ERHIFeatureLevel::Type)TargetFeatureLevel);
			ResourcesToCache.Add(NewResource);
		}
	}

	check(ResourcesToCache.Num() > 0);

	CacheShadersForResources(ShaderPlatform, ResourcesToCache, false);

	for (int32 ResourceIndex = 0; ResourceIndex < ResourcesToCache.Num(); ResourceIndex++)
	{
		OutCachedMaterialResources.Add(ResourcesToCache[ResourceIndex]);
	}
}

void UMaterial::CacheShadersForResources(EShaderPlatform ShaderPlatform, const TArray<FMaterialResource*>& ResourcesToCache, bool bApplyCompletedShaderMapForRendering)
{
	RebuildExpressionTextureReferences();

	for (int32 ResourceIndex = 0; ResourceIndex < ResourcesToCache.Num(); ResourceIndex++)
	{
		FMaterialResource* CurrentResource = ResourcesToCache[ResourceIndex];
		const bool bSuccess = CurrentResource->CacheShaders(ShaderPlatform, bApplyCompletedShaderMapForRendering);

		if (!bSuccess)
		{
			if (IsDefaultMaterial())
			{
				UE_ASSET_LOG(LogMaterial, Fatal, this,
					TEXT("Failed to compile Default Material for platform %s!"),
					*LegacyShaderPlatformToShaderFormat(ShaderPlatform).ToString());
			}

			UE_ASSET_LOG(LogMaterial, Warning, this, TEXT("Failed to compile Material for platform %s, Default Material will be used in game."), 
				*LegacyShaderPlatformToShaderFormat(ShaderPlatform).ToString());

			const TArray<FString>& CompileErrors = CurrentResource->GetCompileErrors();
			for (int32 ErrorIndex = 0; ErrorIndex < CompileErrors.Num(); ErrorIndex++)
			{
				// Always log material errors in an unsuppressed category
				UE_LOG(LogMaterial, Log, TEXT("	%s"), *CompileErrors[ErrorIndex]);
			}
		}
	}
}

void UMaterial::FlushResourceShaderMaps()
{
	FPlatformMisc::CreateGuid(StateId);

	if(FApp::CanEverRender())
	{
		UMaterialInterface::IterateOverActiveFeatureLevels([&](ERHIFeatureLevel::Type InFeatureLevel)
		{
			for(int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
			{
				FMaterialResource* CurrentResource = MaterialResources[QualityLevelIndex][InFeatureLevel];
				CurrentResource->ReleaseShaderMap();
			}
		});
	}
}

void UMaterial::RebuildMaterialFunctionInfo()
{	
	MaterialFunctionInfos.Empty();

	for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ExpressionIndex++)
	{
		UMaterialExpression* Expression = Expressions[ExpressionIndex];
		UMaterialExpressionMaterialFunctionCall* MaterialFunctionNode = Cast<UMaterialExpressionMaterialFunctionCall>(Expression);

		if (MaterialFunctionNode)
		{
			if (MaterialFunctionNode->MaterialFunction)
			{
				{
					FMaterialFunctionInfo NewFunctionInfo;
					NewFunctionInfo.Function = MaterialFunctionNode->MaterialFunction;
					// Store the Id separate from the function, so we can detect changes to the function
					NewFunctionInfo.StateId = MaterialFunctionNode->MaterialFunction->StateId;
					MaterialFunctionInfos.Add(NewFunctionInfo);
				}

				TArray<UMaterialFunction*> DependentFunctions;
				MaterialFunctionNode->MaterialFunction->GetDependentFunctions(DependentFunctions);

				// Handle nested functions
				for (int32 FunctionIndex = 0; FunctionIndex < DependentFunctions.Num(); FunctionIndex++)
				{
					FMaterialFunctionInfo NewFunctionInfo;
					NewFunctionInfo.Function = DependentFunctions[FunctionIndex];
					NewFunctionInfo.StateId = DependentFunctions[FunctionIndex]->StateId;
					MaterialFunctionInfos.Add(NewFunctionInfo);
				}
			}

			// Update the function call node, so it can relink inputs and outputs as needed
			// Update even if MaterialFunctionNode->MaterialFunction is NULL, because we need to remove the invalid inputs in that case
			MaterialFunctionNode->UpdateFromFunctionResource();
		}
	}
}

void UMaterial::RebuildMaterialParameterCollectionInfo()
{
	MaterialParameterCollectionInfos.Empty();

	for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ExpressionIndex++)
	{
		UMaterialExpression* Expression = Expressions[ExpressionIndex];
		UMaterialExpressionCollectionParameter* CollectionParameter = Cast<UMaterialExpressionCollectionParameter>(Expression);
		UMaterialExpressionMaterialFunctionCall* MaterialFunctionNode = Cast<UMaterialExpressionMaterialFunctionCall>(Expression);

		if (CollectionParameter && CollectionParameter->Collection)
		{
			FMaterialParameterCollectionInfo NewInfo;
			NewInfo.ParameterCollection = CollectionParameter->Collection;
			NewInfo.StateId = CollectionParameter->Collection->StateId;
			MaterialParameterCollectionInfos.AddUnique(NewInfo);
		}
		else if (MaterialFunctionNode && MaterialFunctionNode->MaterialFunction)
		{
			TArray<UMaterialFunction*> Functions;
			Functions.Add(MaterialFunctionNode->MaterialFunction);

			MaterialFunctionNode->MaterialFunction->GetDependentFunctions(Functions);

			// Handle nested functions
			for (int32 FunctionIndex = 0; FunctionIndex < Functions.Num(); FunctionIndex++)
			{
				UMaterialFunction* CurrentFunction = Functions[FunctionIndex];

				for (int32 FunctionExpressionIndex = 0; FunctionExpressionIndex < CurrentFunction->FunctionExpressions.Num(); FunctionExpressionIndex++)
				{
					UMaterialExpressionCollectionParameter* FunctionCollectionParameter = Cast<UMaterialExpressionCollectionParameter>(CurrentFunction->FunctionExpressions[FunctionExpressionIndex]);

					if (FunctionCollectionParameter && FunctionCollectionParameter->Collection)
					{
						FMaterialParameterCollectionInfo NewInfo;
						NewInfo.ParameterCollection = FunctionCollectionParameter->Collection;
						NewInfo.StateId = FunctionCollectionParameter->Collection->StateId;
						MaterialParameterCollectionInfos.AddUnique(NewInfo);
					}
				}
			}
		}
	}
}

void UMaterial::CacheExpressionTextureReferences()
{
	if ( ExpressionTextureReferences.Num() <= 0 )
	{
		RebuildExpressionTextureReferences();
	}
}

bool UMaterial::AttemptInsertNewGroupName(const FString & InNewName)
{
#if WITH_EDITOR
	FParameterGroupData* ParameterGroupDataElement = ParameterGroupData.FindByPredicate([&InNewName](const FParameterGroupData& DataElement)
	{
		return InNewName == DataElement.GroupName;
	});

	if (ParameterGroupDataElement == nullptr)
	{
		FParameterGroupData NewGroupData;
		NewGroupData.GroupName = InNewName;
		NewGroupData.GroupSortPriority = 0;
		ParameterGroupData.Add(NewGroupData);
		return true;
	}
#endif
	return false;
}

void UMaterial::RebuildExpressionTextureReferences()
{
	// Note: builds without editor only data will have an incorrect shader map id due to skipping this
	// That's ok, FMaterial::CacheShaders handles this 
	if (FPlatformProperties::HasEditorOnlyData())
	{
		// Rebuild all transient cached material properties which are based off of the editor-only data (expressions) and need to be up to date for compiling
		// Update the cached material function information, which will store off information about the functions this material uses
		RebuildMaterialFunctionInfo();
		RebuildMaterialParameterCollectionInfo();
	}

	ExpressionTextureReferences.Empty();
	AppendReferencedTextures(ExpressionTextureReferences);
}

FMaterialResource* UMaterial::GetMaterialResource(ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type QualityLevel)
{
	check(InFeatureLevel != ERHIFeatureLevel::Num);

	if (QualityLevel == EMaterialQualityLevel::Num)
	{
		QualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
	}

	return MaterialResources[QualityLevel][InFeatureLevel];
}

const FMaterialResource* UMaterial::GetMaterialResource(ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type QualityLevel) const
{
	if (QualityLevel == EMaterialQualityLevel::Num)
	{
		QualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
	}

	return MaterialResources[QualityLevel][InFeatureLevel];
}

void UMaterial::Serialize(FArchive& Ar)
{
	LLM_SCOPE(ELLMTag::Materials);

	SCOPED_LOADTIMER(MaterialSerializeTime);

	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);

	Super::Serialize(Ar);

	if(Ar.IsLoading())
	{
#if WITH_EDITOR
		bool bCooked = FPlatformProperties::RequiresCookedData() || GetOutermost()->bIsCookedForEditor;
#else
		bool bCooked = FPlatformProperties::RequiresCookedData();
#endif
		if (bCooked)
		{
			Expressions.Remove(nullptr);
		}
	}

	if (Ar.UE4Ver() >= VER_UE4_PURGED_FMATERIAL_COMPILE_OUTPUTS)
	{
#if WITH_EDITOR
		SerializeInlineShaderMaps(&CachedMaterialResourcesForCooking, Ar, LoadedMaterialResources);
#else
		SerializeInlineShaderMaps(NULL, Ar, LoadedMaterialResources);
#endif
	}
	else
	{
		FMaterialResource* LegacyResource = AllocateResource();
		LegacyResource->LegacySerialize(Ar);
		StateId = LegacyResource->GetLegacyId();
		delete LegacyResource;
	}

#if WITH_EDITOR
	if (Ar.UE4Ver() < VER_UE4_FLIP_MATERIAL_COORDS)
	{
		GMaterialsThatNeedExpressionsFlipped.Set(this);
	}
	else if (Ar.UE4Ver() < VER_UE4_FIX_MATERIAL_COORDS)
	{
		GMaterialsThatNeedCoordinateCheck.Set(this);
	}
	else if (Ar.UE4Ver() < VER_UE4_FIX_MATERIAL_COMMENTS)
	{
		GMaterialsThatNeedCommentFix.Set(this);
	}

	if (Ar.UE4Ver() < VER_UE4_ADD_LINEAR_COLOR_SAMPLER)
	{
		GMaterialsThatNeedSamplerFixup.Set(this);
	}
#endif // #if WITH_EDITOR

	static_assert(MP_MAX == 29, "New material properties must have DoMaterialAttributesReorder called on them to ensure that any future reordering of property pins is correctly applied.");

	if (Ar.UE4Ver() < VER_UE4_MATERIAL_MASKED_BLENDMODE_TIDY)
	{
		//Set based on old value. Real check may not be possible here in cooked builds?
		//Cached using acutal check in PostEditChangProperty().
		if (BlendMode == BLEND_Masked && !bIsMasked_DEPRECATED)
		{
			bCanMaskedBeAssumedOpaque = true;
		}
		else
		{
			bCanMaskedBeAssumedOpaque = false;
		}
	}

	if(Ar.IsLoading() && Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::IntroducedMeshDecals)
	{
		if(MaterialDomain == MD_DeferredDecal)
		{
			BlendMode = BLEND_Translucent;
		}
	}
}

void UMaterial::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	// Reset the StateId on duplication since it needs to be unique for each material.
	FPlatformMisc::CreateGuid(StateId);
}

void UMaterial::BackwardsCompatibilityInputConversion()
{
#if WITH_EDITOR
	if( ShadingModel != MSM_Unlit )
	{
		bool bIsDS = DiffuseColor_DEPRECATED.IsConnected() || SpecularColor_DEPRECATED.IsConnected();
		bool bIsBMS = BaseColor.IsConnected() || Metallic.IsConnected() || Specular.IsConnected();

		if( bIsDS && !bIsBMS )
		{
			// ConvertFromDiffSpec

			check( GConvertFromDiffSpecMaterialFunction );

			UMaterialExpressionMaterialFunctionCall* FunctionExpression = NewObject<UMaterialExpressionMaterialFunctionCall>(this);
			Expressions.Add( FunctionExpression );

			FunctionExpression->MaterialExpressionEditorX += 200;

			FunctionExpression->MaterialFunction = GConvertFromDiffSpecMaterialFunction;
			FunctionExpression->UpdateFromFunctionResource();

			if( DiffuseColor_DEPRECATED.IsConnected() )
			{
				FunctionExpression->GetInput(0)->Connect( DiffuseColor_DEPRECATED.OutputIndex, DiffuseColor_DEPRECATED.Expression );
			}

			if( SpecularColor_DEPRECATED.IsConnected() )
			{
				FunctionExpression->GetInput(1)->Connect( SpecularColor_DEPRECATED.OutputIndex, SpecularColor_DEPRECATED.Expression );
			}

			BaseColor.Connect( 0, FunctionExpression );
			Metallic.Connect( 1, FunctionExpression );
			Specular.Connect( 2, FunctionExpression );
		}
	}
#endif // WITH_EDITOR
}

void UMaterial::GetQualityLevelUsage(TArray<bool, TInlineAllocator<EMaterialQualityLevel::Num> >& OutQualityLevelsUsed, EShaderPlatform ShaderPlatform)
{
	GetQualityLevelNodeUsage(OutQualityLevelsUsed);

	// OR in the quality overrides if we're a valid shader platform
	if (ShaderPlatform != SP_NumPlatforms)
	{
		const UShaderPlatformQualitySettings* MaterialQualitySettings = UMaterialShaderQualitySettings::Get()->GetShaderPlatformQualitySettings(ShaderPlatform);
		OutQualityLevelsUsed[EMaterialQualityLevel::Low] |= MaterialQualitySettings->GetQualityOverrides(EMaterialQualityLevel::Low).bEnableOverride;
		OutQualityLevelsUsed[EMaterialQualityLevel::Medium] |= MaterialQualitySettings->GetQualityOverrides(EMaterialQualityLevel::Medium).bEnableOverride;
		// No need for EMaterialQualityLevel::High as this is always available
	}
}

void UMaterial::GetQualityLevelNodeUsage(TArray<bool, TInlineAllocator<EMaterialQualityLevel::Num> >& OutQualityLevelsUsed)
{
	OutQualityLevelsUsed.AddZeroed(EMaterialQualityLevel::Num);

	for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ExpressionIndex++)
	{
		UMaterialExpression* Expression = Expressions[ExpressionIndex];
		UMaterialExpressionQualitySwitch* QualitySwitchNode = Cast<UMaterialExpressionQualitySwitch>(Expression);
		UMaterialExpressionMaterialFunctionCall* MaterialFunctionNode = Cast<UMaterialExpressionMaterialFunctionCall>(Expression);

		if (QualitySwitchNode)
		{
			for (int32 InputIndex = 0; InputIndex < EMaterialQualityLevel::Num; InputIndex++)
			{
				if (QualitySwitchNode->Inputs[InputIndex].IsConnected())
				{
					OutQualityLevelsUsed[InputIndex] = true;
				}
			}
		}
		else if (MaterialFunctionNode && MaterialFunctionNode->MaterialFunction)
		{
			TArray<UMaterialFunction*> Functions;
			Functions.Add(MaterialFunctionNode->MaterialFunction);

			MaterialFunctionNode->MaterialFunction->GetDependentFunctions(Functions);

			// Handle nested functions
			for (int32 FunctionIndex = 0; FunctionIndex < Functions.Num(); FunctionIndex++)
			{
				UMaterialFunction* CurrentFunction = Functions[FunctionIndex];

				for (int32 FunctionExpressionIndex = 0; FunctionExpressionIndex < CurrentFunction->FunctionExpressions.Num(); FunctionExpressionIndex++)
				{
					UMaterialExpressionQualitySwitch* SwitchNode = Cast<UMaterialExpressionQualitySwitch>(CurrentFunction->FunctionExpressions[FunctionExpressionIndex]);

					if (SwitchNode)
					{
						for (int32 InputIndex = 0; InputIndex < EMaterialQualityLevel::Num; InputIndex++)
						{
							if (SwitchNode->Inputs[InputIndex].IsConnected())
							{
								OutQualityLevelsUsed[InputIndex] = true;
							}
						}
					}
				}
			}
		}
	}
}

void UMaterial::UpdateResourceAllocations()
{
	if (FApp::CanEverRender())
	{
		for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
		{
			TArray<bool, TInlineAllocator<EMaterialQualityLevel::Num> > QualityLevelsUsed;
			EShaderPlatform ShaderPlatform = GShaderPlatformForFeatureLevel[FeatureLevelIndex];
			GetQualityLevelUsage(QualityLevelsUsed, ShaderPlatform);
			for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
			{
				FMaterialResource*& CurrentResource = MaterialResources[QualityLevelIndex][FeatureLevelIndex];

				if (!CurrentResource)
				{
					CurrentResource = AllocateResource();
				}

				const bool bHasQualityLevelUsage = QualityLevelsUsed[QualityLevelIndex];
				// Setup transient FMaterialResource properties that are needed to use this resource for rendering or compilation
				CurrentResource->SetMaterial(this, (EMaterialQualityLevel::Type)QualityLevelIndex, bHasQualityLevelUsage, (ERHIFeatureLevel::Type)FeatureLevelIndex);
			}
		}
	}
}

TMap<FGuid, UMaterialInterface*> LightingGuidFixupMap;

void UMaterial::PostLoad()
{
	LLM_SCOPE(ELLMTag::Materials);

	SCOPED_LOADTIMER(MaterialPostLoad);

	Super::PostLoad();

	if (FApp::CanEverRender())
	{
		// Resources can be processed / registered now that we're back on the main thread
		ProcessSerializedInlineShaderMaps(this, LoadedMaterialResources, MaterialResources);
	}
	else
	{
		// Discard all loaded material resources
		for (FMaterialResource& Resource : LoadedMaterialResources)
		{
			Resource.DiscardShaderMap();
		}		
	}
	// Empty the lsit of loaded resources, we don't need it anymore
	LoadedMaterialResources.Empty();

#if WITH_EDITORONLY_DATA
	const int32 UE4Ver = GetLinkerUE4Version();
	DoMaterialAttributeReorder(&DiffuseColor_DEPRECATED, UE4Ver);
	DoMaterialAttributeReorder(&SpecularColor_DEPRECATED, UE4Ver);
	DoMaterialAttributeReorder(&BaseColor, UE4Ver);
	DoMaterialAttributeReorder(&Metallic, UE4Ver);
	DoMaterialAttributeReorder(&Specular, UE4Ver);
	DoMaterialAttributeReorder(&Roughness, UE4Ver);
	DoMaterialAttributeReorder(&Normal, UE4Ver);
	DoMaterialAttributeReorder(&EmissiveColor, UE4Ver);
	DoMaterialAttributeReorder(&Opacity, UE4Ver);
	DoMaterialAttributeReorder(&OpacityMask, UE4Ver);
	DoMaterialAttributeReorder(&WorldPositionOffset, UE4Ver);
	DoMaterialAttributeReorder(&WorldDisplacement, UE4Ver);
	DoMaterialAttributeReorder(&TessellationMultiplier, UE4Ver);
	DoMaterialAttributeReorder(&SubsurfaceColor, UE4Ver);
	DoMaterialAttributeReorder(&ClearCoat, UE4Ver);
	DoMaterialAttributeReorder(&ClearCoatRoughness, UE4Ver);
	DoMaterialAttributeReorder(&AmbientOcclusion, UE4Ver);
	DoMaterialAttributeReorder(&Refraction, UE4Ver);
	DoMaterialAttributeReorder(&CustomizedUVs[0], UE4Ver);
	DoMaterialAttributeReorder(&CustomizedUVs[1], UE4Ver);
	DoMaterialAttributeReorder(&CustomizedUVs[2], UE4Ver);
	DoMaterialAttributeReorder(&CustomizedUVs[3], UE4Ver);
	DoMaterialAttributeReorder(&CustomizedUVs[4], UE4Ver);
	DoMaterialAttributeReorder(&CustomizedUVs[5], UE4Ver);
	DoMaterialAttributeReorder(&CustomizedUVs[6], UE4Ver);
	DoMaterialAttributeReorder(&CustomizedUVs[7], UE4Ver);
	DoMaterialAttributeReorder(&PixelDepthOffset, UE4Ver);
#endif // WITH_EDITORONLY_DATA

	if (!IsDefaultMaterial())
	{
		AssertDefaultMaterialsPostLoaded();
	}	

	if ( GIsEditor && GetOuter() == GetTransientPackage() && FCString::Strstr(*GetName(), TEXT("MEStatsMaterial_")))
	{
		bIsMaterialEditorStatsMaterial = true;
	}


	if( GetLinkerUE4Version() < VER_UE4_REMOVED_MATERIAL_USED_WITH_UI_FLAG && bUsedWithUI_DEPRECATED == true )
	{
		MaterialDomain = MD_UI;
	}

	// Ensure expressions have been postloaded before we use them for compiling
	// Any UObjects used by material compilation must be postloaded here
	for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ExpressionIndex++)
	{
		if (Expressions[ExpressionIndex])
		{
			Expressions[ExpressionIndex]->ConditionalPostLoad();
		}
	}

	for (int32 CollectionIndex = 0; CollectionIndex < MaterialParameterCollectionInfos.Num(); CollectionIndex++)
	{
		if (MaterialParameterCollectionInfos[CollectionIndex].ParameterCollection)
		{
			MaterialParameterCollectionInfos[CollectionIndex].ParameterCollection->ConditionalPostLoad();
		}
	}

	// Fixup for legacy materials which didn't recreate the lighting guid properly on duplication
	if (GetLinker() && GetLinker()->UE4Ver() < VER_UE4_BUMPED_MATERIAL_EXPORT_GUIDS)
	{
		UMaterialInterface** ExistingMaterial = LightingGuidFixupMap.Find(GetLightingGuid());

		if (ExistingMaterial)
		{
			SetLightingGuid();
		}

		LightingGuidFixupMap.Add(GetLightingGuid(), this);
	}

	// Fix the shading model to be valid.  Loading a material saved with a shading model that has been removed will yield a MSM_MAX.
	if(ShadingModel == MSM_MAX)
	{
		ShadingModel = MSM_DefaultLit;
	}

	if(DecalBlendMode == DBM_MAX)
	{
		DecalBlendMode = DBM_Translucent;
	}

#if WITH_EDITOR
	if (GIsEditor)
	{
		// Clean up any removed material expression classes	
		if (Expressions.Remove(NULL) != 0)
		{
			// Force this material to recompile because its expressions have changed
			// Warning: any content taking this path will recompile every load until saved!
			// Which means removing an expression class will cause the need for a resave of all materials
			FlushResourceShaderMaps();
		}
	}
#endif

	if (!StateId.IsValid())
	{
		// Fixup for some legacy content
		// This path means recompiling every time the material is loaded until it is saved
		FPlatformMisc::CreateGuid(StateId);
	}

	BackwardsCompatibilityInputConversion();

#if WITH_EDITOR
	if ( GMaterialsThatNeedSamplerFixup.Get( this ) )
	{
		GMaterialsThatNeedSamplerFixup.Clear( this );
		const int32 ExpressionCount = Expressions.Num();
		for ( int32 ExpressionIndex = 0; ExpressionIndex < ExpressionCount; ++ExpressionIndex )
		{
			UMaterialExpressionTextureBase* TextureExpression = Cast<UMaterialExpressionTextureBase>(Expressions[ExpressionIndex]);
			if ( TextureExpression && TextureExpression->Texture )
			{
				switch( TextureExpression->Texture->CompressionSettings )
				{
				case TC_Normalmap:
					TextureExpression->SamplerType = SAMPLERTYPE_Normal;
					break;
					
				case TC_Grayscale:
					TextureExpression->SamplerType = TextureExpression->Texture->SRGB ? SAMPLERTYPE_Grayscale : SAMPLERTYPE_LinearGrayscale;
					break;

				case TC_Masks:
					TextureExpression->SamplerType = SAMPLERTYPE_Masks;
					break;

				case TC_Alpha:
					TextureExpression->SamplerType = SAMPLERTYPE_Alpha;
					break;
				default:
					TextureExpression->SamplerType = TextureExpression->Texture->SRGB ? SAMPLERTYPE_Color : SAMPLERTYPE_LinearColor;
					break;
				}
			}
		}
	}
#endif // #if WITH_EDITOR

	// needed for UMaterial as it doesn't have the InitResources() override where this is called
	PropagateDataToMaterialProxy();

	STAT(double MaterialLoadTime = 0);
	{
		SCOPE_SECONDS_COUNTER(MaterialLoadTime);
// Daniel: Disable compiling shaders for cooked platforms as the cooker will manually call the BeginCacheForCookedPlatformData function and load balence
/*#if WITH_EDITOR
		// enable caching in postload for derived data cache commandlet and cook by the book
		ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
		if (TPM && (TPM->RestrictFormatsToRuntimeOnly() == false))
		{
			TArray<ITargetPlatform*> Platforms = TPM->GetActiveTargetPlatforms();
			// Cache for all the shader formats that the cooking target requires
			for (int32 FormatIndex = 0; FormatIndex < Platforms.Num(); FormatIndex++)
			{
				BeginCacheForCookedPlatformData(Platforms[FormatIndex]);
			}
		}
#endif*/
		//Don't compile shaders in post load for dev overhead materials.
		if (FApp::CanEverRender() && !bIsMaterialEditorStatsMaterial)
		{
			// Before caching shader resources we have to make sure all referenced textures have been post loaded
			// as we depend on their resources being valid.
			RebuildExpressionTextureReferences();
			for (int32 TextureIndex=0, NumTextures=ExpressionTextureReferences.Num(); TextureIndex < NumTextures; ++TextureIndex)
			{
				UTexture* Texture = ExpressionTextureReferences[TextureIndex];
				if (Texture)
				{
					Texture->ConditionalPostLoad();
				}
			}

			CacheResourceShadersForRendering(false);
		}
	}
	INC_FLOAT_STAT_BY(STAT_ShaderCompiling_MaterialLoading,(float)MaterialLoadTime);

	if( GIsEditor && !IsTemplate() )
	{
		// Ensure that the ReferencedTextureGuids array is up to date.
		UpdateLightmassTextureTracking();
	}


#if WITH_EDITOR
	if (GMaterialsThatNeedExpressionsFlipped.Get(this))
	{
		GMaterialsThatNeedExpressionsFlipped.Clear(this);
		FlipExpressionPositions(Expressions, EditorComments, true, this);
	}
	else if (GMaterialsThatNeedCoordinateCheck.Get(this))
	{
		GMaterialsThatNeedCoordinateCheck.Clear(this);
		if (HasFlippedCoordinates())
		{
			FlipExpressionPositions(Expressions, EditorComments, false, this);
		}
		FixCommentPositions(EditorComments);
	}
	else if (GMaterialsThatNeedCommentFix.Get(this))
	{
		GMaterialsThatNeedCommentFix.Clear(this);
		FixCommentPositions(EditorComments);
	}
#endif // #if WITH_EDITOR
}

void UMaterial::PropagateDataToMaterialProxy()
{
	for (int32 i = 0; i < ARRAY_COUNT(DefaultMaterialInstances); i++)
	{
		if (DefaultMaterialInstances[i])
		{
			UpdateMaterialRenderProxy(*DefaultMaterialInstances[i]);
		}
	}
}
#if WITH_EDITOR
void UMaterial::BeginCacheForCookedPlatformData( const ITargetPlatform *TargetPlatform )
{
	TArray<FName> DesiredShaderFormats;
	TargetPlatform->GetAllTargetedShaderFormats(DesiredShaderFormats);

	TArray<FMaterialResource*> *CachedMaterialResourcesForPlatform = CachedMaterialResourcesForCooking.Find( TargetPlatform );

	if ( CachedMaterialResourcesForPlatform == NULL )
	{
		CachedMaterialResourcesForCooking.Add( TargetPlatform );
		CachedMaterialResourcesForPlatform = CachedMaterialResourcesForCooking.Find( TargetPlatform );

		check( CachedMaterialResourcesForPlatform != NULL );

		if (DesiredShaderFormats.Num())
		{
			// Cache for all the shader formats that the cooking target requires
			for (int32 FormatIndex = 0; FormatIndex < DesiredShaderFormats.Num(); FormatIndex++)
			{
				const EShaderPlatform LegacyShaderPlatform = ShaderFormatToLegacyShaderPlatform(DesiredShaderFormats[FormatIndex]);

				// Begin caching shaders for the target platform and store the material resource being compiled into CachedMaterialResourcesForCooking
				CacheResourceShadersForCooking(LegacyShaderPlatform, *CachedMaterialResourcesForPlatform);
			}
		}
	}
}

bool UMaterial::IsCachedCookedPlatformDataLoaded( const ITargetPlatform* TargetPlatform ) 
{
	TArray<FName> DesiredShaderFormats;
	TargetPlatform->GetAllTargetedShaderFormats(DesiredShaderFormats);

	const TArray<FMaterialResource*>* CachedMaterialResourcesForPlatform = CachedMaterialResourcesForCooking.Find( TargetPlatform );

	if ( CachedMaterialResourcesForPlatform != NULL ) // this should always succeed if begincacheforcookedcplatformdata is called first
	{
		for ( const auto& MaterialResource : *CachedMaterialResourcesForPlatform )
		{
			if ( MaterialResource->IsCompilationFinished() == false )
			{
				return false;
			}
		}
		return true;
	}
	return false;
}

void UMaterial::ClearCachedCookedPlatformData( const ITargetPlatform *TargetPlatform )
{
	TArray<FMaterialResource*> *CachedMaterialResourcesForPlatform = CachedMaterialResourcesForCooking.Find( TargetPlatform );
	if ( CachedMaterialResourcesForPlatform != NULL )
	{
		for (int32 CachedResourceIndex = 0; CachedResourceIndex < CachedMaterialResourcesForPlatform->Num(); CachedResourceIndex++)
		{
			delete (*CachedMaterialResourcesForPlatform)[CachedResourceIndex];
		}
	}
	CachedMaterialResourcesForCooking.Remove( TargetPlatform );
}

void UMaterial::ClearAllCachedCookedPlatformData()
{
	for ( auto It : CachedMaterialResourcesForCooking )
	{
		TArray<FMaterialResource*> &CachedMaterialResourcesForPlatform = It.Value;
		for (int32 CachedResourceIndex = 0; CachedResourceIndex < CachedMaterialResourcesForPlatform.Num(); CachedResourceIndex++)
		{
			delete (CachedMaterialResourcesForPlatform)[CachedResourceIndex];
		}
	}
	CachedMaterialResourcesForCooking.Empty();
}
#endif
#if WITH_EDITOR
bool UMaterial::CanEditChange(const UProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, PhysMaterial))
		{
			return MaterialDomain == MD_Surface;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, OpacityMaskClipValue) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, DitherOpacityMask)
			)
		{
			return BlendMode == BLEND_Masked ||
			bCastDynamicShadowAsMasked ||
			IsTranslucencyWritingCustomDepth();
		}

		if ( PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, bCastDynamicShadowAsMasked) )
		{
			return BlendMode == BLEND_Translucent;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, DecalBlendMode))
		{
			return MaterialDomain == MD_DeferredDecal;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, MaterialDecalResponse))
		{
			static auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DBuffer"));

			return MaterialDomain == MD_Surface && CVar->GetValueOnGameThread() > 0;
		}		

		if(MaterialDomain == MD_PostProcess)
		{
			// some settings don't make sense for postprocess materials

			if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, bTangentSpaceNormal) ||
				PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, bDisableDepthTest) ||
				PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, bUseMaterialAttributes)
				)
			{
				return false;
			}
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, bFullyRough) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, bNormalCurvatureToRoughness) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, TwoSided) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, bUseLightmapDirectionality) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, bUseHQForwardReflections) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, bUsePlanarForwardReflections)
			)
		{
			return MaterialDomain == MD_Surface;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, D3D11TessellationMode))
		{
			return MaterialDomain == MD_DeferredDecal || MaterialDomain == MD_Surface;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, bEnableCrackFreeDisplacement) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, MaxDisplacement) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, bEnableAdaptiveTessellation)
			)
		{
			return (MaterialDomain == MD_DeferredDecal || MaterialDomain == MD_Surface) && D3D11TessellationMode != MTM_NoTessellation;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, BlendableLocation) ||
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, BlendablePriority) || 
			PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, BlendableOutputAlpha)	)
		{
			return MaterialDomain == MD_PostProcess;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, BlendMode))
		{
			return MaterialDomain == MD_DeferredDecal || MaterialDomain == MD_Surface || MaterialDomain == MD_Volume || MaterialDomain == MD_UI;
		}
	
		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, ShadingModel))
		{
			return MaterialDomain == MD_Surface || (MaterialDomain == MD_DeferredDecal && DecalBlendMode == DBM_Volumetric_DistanceFunction);
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, DecalBlendMode))
		{
			return MaterialDomain == MD_DeferredDecal;
		}
		else if (
			FCString::Strncmp(*PropertyName, TEXT("bUsedWith"), 9) == 0 ||
			FCString::Strcmp(*PropertyName, TEXT("bUsesDistortion")) == 0
			)
		{
			return MaterialDomain == MD_DeferredDecal || MaterialDomain == MD_Surface;
		}
		else if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, RefractionDepthBias))
		{
			return Refraction.IsConnected();
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, bEnableSeparateTranslucency)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, bEnableResponsiveAA)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, bScreenSpaceReflections)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, bDisableDepthTest)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, bUseTranslucencyVertexFog)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, bComputeFogPerPixel))
		{
			return MaterialDomain != MD_DeferredDecal && IsTranslucentBlendMode(BlendMode);
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, TranslucencyLightingMode)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, TranslucencyDirectionalLightingIntensity)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, TranslucentShadowDensityScale)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, TranslucentSelfShadowDensityScale)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, TranslucentSelfShadowSecondDensityScale)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, TranslucentSelfShadowSecondOpacity)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, TranslucentBackscatteringExponent)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, TranslucentMultipleScatteringExtinction)
			|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, TranslucentShadowStartOffset))
		{
			return MaterialDomain != MD_DeferredDecal && IsTranslucentBlendMode(BlendMode) && ShadingModel != MSM_Unlit;
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UMaterial, SubsurfaceProfile))
		{
			return MaterialDomain == MD_Surface && ShadingModel == MSM_SubsurfaceProfile && (BlendMode == BLEND_Opaque || BlendMode == BLEND_Masked);
		}

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassMaterialInterfaceSettings, bCastShadowAsMasked))
		{
			return BlendMode != BLEND_Opaque && BlendMode != BLEND_Modulate;
		}
	}

	return true;
}

void UMaterial::PreEditChange(UProperty* PropertyThatChanged)
{
	Super::PreEditChange(PropertyThatChanged);

	// Flush all pending rendering commands.
	FlushRenderingCommands();
}

void UMaterial::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// If the material changes, then the debug view material must reset to prevent parameters mismatch
	void ClearAllDebugViewMaterials();
	ClearAllDebugViewMaterials();

	Super::PostEditChangeProperty(PropertyChangedEvent);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	
	//Cancel any current compilation jobs that are in flight for this material.
	CancelOutstandingCompilation();

	// check for distortion in material 
	{
		bUsesDistortion = false;
		// check for a distortion value
		if (Refraction.Expression
			|| (Refraction.UseConstant && FMath::Abs(Refraction.Constant - 1.0f) >= KINDA_SMALL_NUMBER))
		{
			bUsesDistortion = true;
		}
	}

	//If we can be sure this material would be the same opaque as it is masked then allow it to be assumed opaque.
	bCanMaskedBeAssumedOpaque = !OpacityMask.Expression && !(OpacityMask.UseConstant && OpacityMask.Constant < 0.999f) && !bUseMaterialAttributes;

	//Flex fluid surfaces can never be considered fully opaque.
	bCanMaskedBeAssumedOpaque &= !bUsedWithFlexFluidSurfaces;

	bool bRequiresCompilation = true;
	if( PropertyThatChanged ) 
	{
		// Don't recompile the material if we only changed the PhysMaterial property.
		if (PropertyThatChanged->GetName() == TEXT("PhysMaterial"))
		{
			bRequiresCompilation = false;
		}
		// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
		else if (PropertyThatChanged->GetNameCPP() == TEXT("bVxgiOmniDirectional"))
		{
			bRequiresCompilation = false;
		}
		else if (PropertyThatChanged->GetNameCPP() == TEXT("bVxgiProportionalEmittance"))
		{
			bRequiresCompilation = false;
		}
		else if (PropertyThatChanged->GetNameCPP() == TEXT("VxgiVoxelizationThickness"))
		{
			bRequiresCompilation = false;
		}
		else if (PropertyThatChanged->GetNameCPP() == TEXT("VxgiOpacityNoiseScaleBias"))
		{
			bRequiresCompilation = false;
		}
		else if (PropertyThatChanged->GetNameCPP() == TEXT("VxgiCoverageSupersamplingMode"))
		{
			bRequiresCompilation = false;
		}
		else if (PropertyThatChanged->GetNameCPP() == TEXT("VxgiMaterialSamplingRate"))
		{
			bRequiresCompilation = false;
		}
#endif
		// NVCHANGE_END: Add VXGI
	}

	TranslucencyDirectionalLightingIntensity = FMath::Clamp(TranslucencyDirectionalLightingIntensity, .1f, 10.0f);

	// Don't want to recompile after a duplicate because it's just been done by PostLoad, nor during interactive changes to prevent constant recompliation while spinning properties.
	if( PropertyChangedEvent.ChangeType == EPropertyChangeType::Duplicate || PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive )
	{
		bRequiresCompilation = false;
	}
	
	if (bRequiresCompilation)
	{
		// When redirecting an object pointer, we trust that the DDC hash will detect the change and that we don't need to force a recompile.
		const bool bRegenerateId = PropertyChangedEvent.ChangeType != EPropertyChangeType::Redirected;
		CacheResourceShadersForRendering(bRegenerateId);
		RecacheMaterialInstanceUniformExpressions(this);

		// Ensure that the ReferencedTextureGuids array is up to date.
		if (GIsEditor)
		{
			UpdateLightmassTextureTracking();
		}

		// Ensure that any components with static elements using this material have their render state recreated
		// so changes are propagated to them. The preview material is only applied to the preview mesh component,
		// and that reregister is handled by the material editor.
		if (!bIsPreviewMaterial && !bIsMaterialEditorStatsMaterial)
		{
			FGlobalComponentRecreateRenderStateContext RecreateComponentsRenderState;
		}
	}

	// needed for UMaterial as it doesn't have the InitResources() override where this is called
	PropagateDataToMaterialProxy();

	// many property changes can require rebuild of graph so always mark as changed
	// not interested in PostEditChange calls though as the graph may have instigated it
	if (PropertyThatChanged && MaterialGraph)
	{
		MaterialGraph->NotifyGraphChanged();
	}
} 
#endif // WITH_EDITOR

bool UMaterial::AddExpressionParameter(UMaterialExpression* Expression, TMap<FName, TArray<UMaterialExpression*> >& ParameterTypeMap)
{
	if(!Expression)
	{
		return false;
	}

	bool bRet = false;

	if(Expression->IsA(UMaterialExpressionParameter::StaticClass()))
	{
		UMaterialExpressionParameter *Param = (UMaterialExpressionParameter*)Expression;

		TArray<UMaterialExpression*> *ExpressionList = ParameterTypeMap.Find(Param->ParameterName);

		if(!ExpressionList)
		{
			ExpressionList = &ParameterTypeMap.Add(Param->ParameterName, TArray<UMaterialExpression*>());
		}

		ExpressionList->Add(Param);
		bRet = true;
	}
	else if(Expression->IsA(UMaterialExpressionTextureSampleParameter::StaticClass()))
	{
		UMaterialExpressionTextureSampleParameter *Param = (UMaterialExpressionTextureSampleParameter*)Expression;

		TArray<UMaterialExpression*> *ExpressionList = ParameterTypeMap.Find(Param->ParameterName);

		if(!ExpressionList)
		{
			ExpressionList = &ParameterTypeMap.Add(Param->ParameterName, TArray<UMaterialExpression*>());
		}

		ExpressionList->Add(Param);
		bRet = true;
	}
	else if(Expression->IsA(UMaterialExpressionFontSampleParameter::StaticClass()))
	{
		UMaterialExpressionFontSampleParameter *Param = (UMaterialExpressionFontSampleParameter*)Expression;

		TArray<UMaterialExpression*> *ExpressionList = ParameterTypeMap.Find(Param->ParameterName);

		if(!ExpressionList)
		{
			ExpressionList = &ParameterTypeMap.Add(Param->ParameterName, TArray<UMaterialExpression*>());
		}

		ExpressionList->Add(Param);
		bRet = true;
	}

	return bRet;
}


bool UMaterial::RemoveExpressionParameter(UMaterialExpression* Expression)
{
	FName ParmName;

	if(GetExpressionParameterName(Expression, ParmName))
	{
		TArray<UMaterialExpression*>* ExpressionList = EditorParameters.Find(ParmName);

		if(ExpressionList)
		{
			return ExpressionList->Remove(Expression) > 0;
		}
	}

	return false;
}


bool UMaterial::IsParameter(const UMaterialExpression* Expression)
{
	bool bRet = false;

	if(Expression->IsA(UMaterialExpressionParameter::StaticClass()))
	{
		bRet = true;
	}
	else if(Expression->IsA(UMaterialExpressionTextureSampleParameter::StaticClass()))
	{
		bRet = true;
	}
	else if(Expression->IsA(UMaterialExpressionFontSampleParameter::StaticClass()))
	{
		bRet = true;
	}

	return bRet;
}


bool UMaterial::IsDynamicParameter(const UMaterialExpression* Expression)
{
	if (Expression->IsA(UMaterialExpressionDynamicParameter::StaticClass()))
	{
		return true;
	}

	return false;
}



void UMaterial::BuildEditorParameterList()
{
	EditorParameters.Empty();

	for(int32 MaterialExpressionIndex = 0 ; MaterialExpressionIndex < Expressions.Num() ; ++MaterialExpressionIndex)
	{
		AddExpressionParameter(Expressions[MaterialExpressionIndex], EditorParameters);
	}
}


bool UMaterial::HasDuplicateParameters(const UMaterialExpression* Expression)
{
	FName ExpressionName;

	if(GetExpressionParameterName(Expression, ExpressionName))
	{
		TArray<UMaterialExpression*>* ExpressionList = EditorParameters.Find(ExpressionName);

		if(ExpressionList)
		{
			for(int32 ParmIndex = 0; ParmIndex < ExpressionList->Num(); ++ParmIndex)
			{
				UMaterialExpression* CurNode = (*ExpressionList)[ParmIndex];
				if(CurNode != Expression && CurNode->GetClass() == Expression->GetClass())
				{
					return true;
				}
			}
		}
	}

	return false;
}


bool UMaterial::HasDuplicateDynamicParameters(const UMaterialExpression* Expression)
{
	const UMaterialExpressionDynamicParameter* DynParam = Cast<UMaterialExpressionDynamicParameter>(Expression);
	if (DynParam)
	{
		for (int32 ExpIndex = 0; ExpIndex < Expressions.Num(); ExpIndex++)
		{
			UMaterialExpressionDynamicParameter* CheckDynParam = Cast<UMaterialExpressionDynamicParameter>(Expressions[ExpIndex]);
			if (CheckDynParam != Expression)
			{
				return true;
			}
		}
	}
	return false;
}


void UMaterial::UpdateExpressionDynamicParameters(const UMaterialExpression* Expression)
{
	const UMaterialExpressionDynamicParameter* DynParam = Cast<UMaterialExpressionDynamicParameter>(Expression);
	if (DynParam)
	{
		for (int32 ExpIndex = 0; ExpIndex < Expressions.Num(); ExpIndex++)
		{
			UMaterialExpressionDynamicParameter* CheckParam = Cast<UMaterialExpressionDynamicParameter>(Expressions[ExpIndex]);
			if (CheckParam && CheckParam->CopyDynamicParameterProperties(DynParam))
			{
#if WITH_EDITOR
				CheckParam->GraphNode->ReconstructNode();
#endif // WITH_EDITOR
			}
		}
	}
}


void UMaterial::PropagateExpressionParameterChanges(UMaterialExpression* Parameter)
{
	FName ParmName;
	bool bRet = GetExpressionParameterName(Parameter, ParmName);

	if(bRet)
	{
		TArray<UMaterialExpression*>* ExpressionList = EditorParameters.Find(ParmName);

		if(ExpressionList && ExpressionList->Num() > 1)
		{
			for(int32 Index = 0; Index < ExpressionList->Num(); ++Index)
			{
				CopyExpressionParameters(Parameter, (*ExpressionList)[Index]);
			}
		}
		else if(!ExpressionList)
		{
			bRet = false;
		}
	}
}


void UMaterial::UpdateExpressionParameterName(UMaterialExpression* Expression)
{
	FName ExpressionName;

	for(TMap<FName, TArray<UMaterialExpression*> >::TIterator Iter(EditorParameters); Iter; ++Iter)
	{
		if(Iter.Value().Remove(Expression) > 0)
		{
			if(Iter.Value().Num() == 0)
			{
				EditorParameters.Remove(Iter.Key());
			}

			AddExpressionParameter(Expression, EditorParameters);
			break;
		}
	}
}


bool UMaterial::GetExpressionParameterName(const UMaterialExpression* Expression, FName& OutName)
{
	bool bRet = false;

	if(Expression->IsA(UMaterialExpressionParameter::StaticClass()))
	{
		OutName = ((UMaterialExpressionParameter*)Expression)->ParameterName;
		bRet = true;
	}
	else if(Expression->IsA(UMaterialExpressionTextureSampleParameter::StaticClass()))
	{
		OutName = ((UMaterialExpressionTextureSampleParameter*)Expression)->ParameterName;
		bRet = true;
	}
	else if(Expression->IsA(UMaterialExpressionFontSampleParameter::StaticClass()))
	{
		OutName = ((UMaterialExpressionFontSampleParameter*)Expression)->ParameterName;
		bRet = true;
	}

	return bRet;
}


bool UMaterial::CopyExpressionParameters(UMaterialExpression* Source, UMaterialExpression* Destination)
{
	if(!Source || !Destination || Source == Destination || Source->GetClass() != Destination->GetClass())
	{
		return false;
	}

	bool bRet = true;

	if(Source->IsA(UMaterialExpressionTextureSampleParameter::StaticClass()))
	{
		UMaterialExpressionTextureSampleParameter *SourceTex = (UMaterialExpressionTextureSampleParameter*)Source;
		UMaterialExpressionTextureSampleParameter *DestTex = (UMaterialExpressionTextureSampleParameter*)Destination;

		DestTex->Modify();
		DestTex->Texture = SourceTex->Texture;
	}
	else if(Source->IsA(UMaterialExpressionVectorParameter::StaticClass()))
	{
		UMaterialExpressionVectorParameter *SourceVec = (UMaterialExpressionVectorParameter*)Source;
		UMaterialExpressionVectorParameter *DestVec = (UMaterialExpressionVectorParameter*)Destination;

		DestVec->Modify();
		DestVec->DefaultValue = SourceVec->DefaultValue;
	}
	else if(Source->IsA(UMaterialExpressionStaticBoolParameter::StaticClass()))
	{
		UMaterialExpressionStaticBoolParameter *SourceVec = (UMaterialExpressionStaticBoolParameter*)Source;
		UMaterialExpressionStaticBoolParameter *DestVec = (UMaterialExpressionStaticBoolParameter*)Destination;

		DestVec->Modify();
		DestVec->DefaultValue = SourceVec->DefaultValue;
	}
	else if(Source->IsA(UMaterialExpressionStaticComponentMaskParameter::StaticClass()))
	{
		UMaterialExpressionStaticComponentMaskParameter *SourceVec = (UMaterialExpressionStaticComponentMaskParameter*)Source;
		UMaterialExpressionStaticComponentMaskParameter *DestVec = (UMaterialExpressionStaticComponentMaskParameter*)Destination;

		DestVec->Modify();
		DestVec->DefaultR = SourceVec->DefaultR;
		DestVec->DefaultG = SourceVec->DefaultG;
		DestVec->DefaultB = SourceVec->DefaultB;
		DestVec->DefaultA = SourceVec->DefaultA;
	}
	else if(Source->IsA(UMaterialExpressionScalarParameter::StaticClass()))
	{
		UMaterialExpressionScalarParameter *SourceVec = (UMaterialExpressionScalarParameter*)Source;
		UMaterialExpressionScalarParameter *DestVec = (UMaterialExpressionScalarParameter*)Destination;

		DestVec->Modify();
		DestVec->DefaultValue = SourceVec->DefaultValue;
	}
	else if(Source->IsA(UMaterialExpressionFontSampleParameter::StaticClass()))
	{
		UMaterialExpressionFontSampleParameter *SourceFont = (UMaterialExpressionFontSampleParameter*)Source;
		UMaterialExpressionFontSampleParameter *DestFont = (UMaterialExpressionFontSampleParameter*)Destination;

		DestFont->Modify();
		DestFont->Font = SourceFont->Font;
		DestFont->FontTexturePage = SourceFont->FontTexturePage;
	}
	else
	{
		bRet = false;
	}

	return bRet;
}

void UMaterial::BeginDestroy()
{
	Super::BeginDestroy();

	for (int32 InstanceIndex = 0; InstanceIndex < 3; ++InstanceIndex)
	{
		if (DefaultMaterialInstances[InstanceIndex])
		{
			BeginReleaseResource(DefaultMaterialInstances[InstanceIndex]);
		}
	}

	ReleaseFence.BeginFence();
}

bool UMaterial::IsReadyForFinishDestroy()
{
	bool bReady = Super::IsReadyForFinishDestroy();

	return bReady && ReleaseFence.IsFenceComplete();
}

void UMaterial::ReleaseResources()
{
	for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
	{
		for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
		{
			FMaterialResource*& CurrentResource = MaterialResources[QualityLevelIndex][FeatureLevelIndex];
			delete CurrentResource;
			CurrentResource = NULL;
		}
	}
#if WITH_EDITOR
	ClearAllCachedCookedPlatformData();
#endif
	for (int32 InstanceIndex = 0; InstanceIndex < 3; ++InstanceIndex)
	{
		if (DefaultMaterialInstances[InstanceIndex])
		{
			DefaultMaterialInstances[InstanceIndex]->GameThread_Destroy();
			DefaultMaterialInstances[InstanceIndex] = NULL;
		}
	}
}

void UMaterial::FinishDestroy()
{
	ReleaseResources();

	Super::FinishDestroy();
}

void UMaterial::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	for (int32 InstanceIndex = 0; InstanceIndex < 3; ++InstanceIndex)
	{
		if (DefaultMaterialInstances[InstanceIndex])
		{
			CumulativeResourceSize.AddDedicatedSystemMemoryBytes(sizeof(FDefaultMaterialInstance));
		}
	}

	if (CumulativeResourceSize.GetResourceSizeMode() == EResourceSizeMode::Inclusive)
	{
		for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
		{
			for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
			{
				FMaterialResource* CurrentResource = MaterialResources[QualityLevelIndex][FeatureLevelIndex];
				CurrentResource->GetResourceSizeEx(CumulativeResourceSize);
			}
		}

		TArray<UTexture*> TheReferencedTextures;
		for ( int32 ExpressionIndex= 0 ; ExpressionIndex < Expressions.Num() ; ++ExpressionIndex )
		{
			UMaterialExpressionTextureSample* TextureSample = Cast<UMaterialExpressionTextureSample>( Expressions[ExpressionIndex] );
			if ( TextureSample && TextureSample->Texture )
			{
				UTexture* Texture						= TextureSample->Texture;
				const bool bTextureAlreadyConsidered	= TheReferencedTextures.Contains( Texture );
				if ( !bTextureAlreadyConsidered )
				{
					TheReferencedTextures.Add( Texture );
					Texture->GetResourceSizeEx(CumulativeResourceSize);
				}
			}
		}
	}
}

void UMaterial::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UMaterial* This = CastChecked<UMaterial>(InThis);

	for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
	{
		for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
		{
			FMaterialResource* CurrentResource = This->MaterialResources[QualityLevelIndex][FeatureLevelIndex];
			if (CurrentResource)
			{
				CurrentResource->AddReferencedObjects(Collector);
			}
		}
	}
#if WITH_EDITORONLY_DATA
	Collector.AddReferencedObject(This->MaterialGraph, This);
#endif

	Super::AddReferencedObjects(This, Collector);
}

bool UMaterial::CanBeClusterRoot() const 
{
	return true;
}

void UMaterial::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	// Search for any scene color nodes
	bool bHasSceneColor = HasAnyExpressionsInMaterialAndFunctionsOfType<UMaterialExpressionSceneColor>();
	OutTags.Add(FAssetRegistryTag("HasSceneColor", bHasSceneColor ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical));

	Super::GetAssetRegistryTags(OutTags);
}

#if WITH_EDITOR
void UMaterial::CancelOutstandingCompilation()
{
	for (int32 FeatureLevel = 0; FeatureLevel < ERHIFeatureLevel::Num; ++FeatureLevel)
	{
		if (FMaterialResource* Res = GetMaterialResource((ERHIFeatureLevel::Type)FeatureLevel))
		{
			Res->CancelCompilation();
		}
	}
}
#endif

void UMaterial::UpdateMaterialShaders(TArray<FShaderType*>& ShaderTypesToFlush, TArray<const FShaderPipelineType*>& ShaderPipelineTypesToFlush, TArray<const FVertexFactoryType*>& VFTypesToFlush, EShaderPlatform ShaderPlatform)
{
	// Create a material update context so we can safely update materials.
	{
		FMaterialUpdateContext UpdateContext(FMaterialUpdateContext::EOptions::Default, ShaderPlatform);

		int32 NumMaterials = 0;

		for( TObjectIterator<UMaterial> It; It; ++It )
		{
			NumMaterials++;
		}

		GWarn->StatusUpdate(0, NumMaterials, NSLOCTEXT("Material", "BeginAsyncMaterialShaderCompilesTask", "Kicking off async material shader compiles..."));

		int32 UpdateStatusDivisor = FMath::Max<int32>(NumMaterials / 20, 1);
		int32 MaterialIndex = 0;

		// Reinitialize the material shader maps
		for( TObjectIterator<UMaterial> It; It; ++It )
		{
			UMaterial* BaseMaterial = *It;
			UpdateContext.AddMaterial(BaseMaterial);
			BaseMaterial->CacheResourceShadersForRendering(false);

			// Limit the frequency of progress updates
			if (MaterialIndex % UpdateStatusDivisor == 0)
			{
				GWarn->UpdateProgress(MaterialIndex, NumMaterials);
			}
			MaterialIndex++;
		}

		// The material update context will safely update all dependent material instances when
		// it leaves scope.
	}

	// Update any FMaterials not belonging to a UMaterialInterface, for example FExpressionPreviews
	// If we did not do this, the editor would crash the next time it tried to render one of those previews
	// And didn't find a shader that had been flushed for the preview's shader map.
	FMaterial::UpdateEditorLoadedMaterialResources(ShaderPlatform);
}

void UMaterial::BackupMaterialShadersToMemory(TMap<FMaterialShaderMap*, TUniquePtr<TArray<uint8> > >& ShaderMapToSerializedShaderData)
{
	// Process FMaterialShaderMap's referenced by UObjects (UMaterial, UMaterialInstance)
	for (TObjectIterator<UMaterialInterface> It; It; ++It)
	{
		UMaterialInterface* Material = *It;
		UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Material);
		UMaterial* BaseMaterial = Cast<UMaterial>(Material);

		if (MaterialInstance)
		{
			if (MaterialInstance->bHasStaticPermutationResource)
			{
				TArray<FMaterialShaderMap*> MIShaderMaps;
				MaterialInstance->GetAllShaderMaps(MIShaderMaps);

				for (int32 ShaderMapIndex = 0; ShaderMapIndex < MIShaderMaps.Num(); ShaderMapIndex++)
				{
					FMaterialShaderMap* ShaderMap = MIShaderMaps[ShaderMapIndex];

					if (ShaderMap && !ShaderMapToSerializedShaderData.Contains(ShaderMap))
					{
						TArray<uint8>* ShaderData = ShaderMap->BackupShadersToMemory();
						ShaderMapToSerializedShaderData.Emplace(ShaderMap, ShaderData);
					}
				}
			}
		}
		else if (BaseMaterial)
		{
			for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
			{
				for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
				{
					FMaterialResource* CurrentResource = BaseMaterial->MaterialResources[QualityLevelIndex][FeatureLevelIndex];
					FMaterialShaderMap* ShaderMap = CurrentResource->GetGameThreadShaderMap();

					if (ShaderMap && !ShaderMapToSerializedShaderData.Contains(ShaderMap))
					{
						TArray<uint8>* ShaderData = ShaderMap->BackupShadersToMemory();
						ShaderMapToSerializedShaderData.Emplace(ShaderMap, ShaderData);
					}
				}
			}
		}
	}

	// Process FMaterialShaderMap's referenced by the editor
	FMaterial::BackupEditorLoadedMaterialShadersToMemory(ShaderMapToSerializedShaderData);
}

void UMaterial::RestoreMaterialShadersFromMemory(const TMap<FMaterialShaderMap*, TUniquePtr<TArray<uint8> > >& ShaderMapToSerializedShaderData)
{
	// Process FMaterialShaderMap's referenced by UObjects (UMaterial, UMaterialInstance)
	for (TObjectIterator<UMaterialInterface> It; It; ++It)
	{
		UMaterialInterface* Material = *It;
		UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Material);
		UMaterial* BaseMaterial = Cast<UMaterial>(Material);

		if (MaterialInstance)
		{
			if (MaterialInstance->bHasStaticPermutationResource)
			{
				TArray<FMaterialShaderMap*> MIShaderMaps;
				MaterialInstance->GetAllShaderMaps(MIShaderMaps);

				for (int32 ShaderMapIndex = 0; ShaderMapIndex < MIShaderMaps.Num(); ShaderMapIndex++)
				{
					FMaterialShaderMap* ShaderMap = MIShaderMaps[ShaderMapIndex];

					if (ShaderMap)
					{
						const TUniquePtr<TArray<uint8> >* ShaderData = ShaderMapToSerializedShaderData.Find(ShaderMap);

						if (ShaderData)
						{
							ShaderMap->RestoreShadersFromMemory(**ShaderData);
						}
					}
				}
			}
		}
		else if (BaseMaterial)
		{
			for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
			{
				for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
				{
					FMaterialResource* CurrentResource = BaseMaterial->MaterialResources[QualityLevelIndex][FeatureLevelIndex];
					FMaterialShaderMap* ShaderMap = CurrentResource->GetGameThreadShaderMap();

					if (ShaderMap)
					{
						const TUniquePtr<TArray<uint8> >* ShaderData = ShaderMapToSerializedShaderData.Find(ShaderMap);

						if (ShaderData)
						{
							ShaderMap->RestoreShadersFromMemory(**ShaderData);
						}
					}
				}
			}
		}
	}

	// Process FMaterialShaderMap's referenced by the editor
	FMaterial::RestoreEditorLoadedMaterialShadersFromMemory(ShaderMapToSerializedShaderData);
}

void UMaterial::CompileMaterialsForRemoteRecompile(
	const TArray<UMaterialInterface*>& MaterialsToCompile,
	EShaderPlatform ShaderPlatform, 
	TMap<FString, TArray<TRefCountPtr<FMaterialShaderMap> > >& OutShaderMaps)
{
	// Build a map from UMaterial / UMaterialInstance to the resources which are being compiled
	TMap<FString, TArray<FMaterialResource*> > CompilingResources;

	// compile the requested materials
	for (int32 Index = 0; Index < MaterialsToCompile.Num(); Index++)
	{
		// get the material resource from the UMaterialInterface
		UMaterialInterface* Material = MaterialsToCompile[Index];
		UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Material);
		UMaterial* BaseMaterial = Cast<UMaterial>(Material);

		if (MaterialInstance && MaterialInstance->bHasStaticPermutationResource)
		{
			TArray<FMaterialResource*>& ResourceArray = CompilingResources.Add(Material->GetPathName(), TArray<FMaterialResource*>());
			MaterialInstance->CacheResourceShadersForCooking(ShaderPlatform, ResourceArray);
		}
		else if (BaseMaterial)
		{
			TArray<FMaterialResource*>& ResourceArray = CompilingResources.Add(Material->GetPathName(), TArray<FMaterialResource*>());
			BaseMaterial->CacheResourceShadersForCooking(ShaderPlatform, ResourceArray);
		}
	}

	// Wait until all compilation is finished and all of the gathered FMaterialResources have their GameThreadShaderMap up to date
	GShaderCompilingManager->FinishAllCompilation();

	for(TMap<FString, TArray<FMaterialResource*> >::TIterator It(CompilingResources); It; ++It)
	{
		TArray<FMaterialResource*>& ResourceArray = It.Value();
		TArray<TRefCountPtr<FMaterialShaderMap> >& OutShaderMapArray = OutShaderMaps.Add(It.Key(), TArray<TRefCountPtr<FMaterialShaderMap> >());

		for (int32 Index = 0; Index < ResourceArray.Num(); Index++)
		{
			FMaterialResource* CurrentResource = ResourceArray[Index];
			OutShaderMapArray.Add(CurrentResource->GetGameThreadShaderMap());
			delete CurrentResource;
		}
	}
}

bool UMaterial::UpdateLightmassTextureTracking()
{
	bool bTexturesHaveChanged = false;
#if WITH_EDITORONLY_DATA
	TArray<UTexture*> UsedTextures;
	
	GetUsedTextures(UsedTextures, EMaterialQualityLevel::Num, true, GMaxRHIFeatureLevel, true);
	if (UsedTextures.Num() != ReferencedTextureGuids.Num())
	{
		bTexturesHaveChanged = true;
		// Just clear out all the guids and the code below will
		// fill them back in...
		ReferencedTextureGuids.Empty(UsedTextures.Num());
		ReferencedTextureGuids.AddZeroed(UsedTextures.Num());
	}
	
	for (int32 CheckIdx = 0; CheckIdx < UsedTextures.Num(); CheckIdx++)
	{
		UTexture* Texture = UsedTextures[CheckIdx];
		if (Texture)
		{
			if (ReferencedTextureGuids[CheckIdx] != Texture->GetLightingGuid())
			{
				ReferencedTextureGuids[CheckIdx] = Texture->GetLightingGuid();
				bTexturesHaveChanged = true;
			}
		}
		else
		{
			if (ReferencedTextureGuids[CheckIdx] != FGuid(0,0,0,0))
			{
				ReferencedTextureGuids[CheckIdx] = FGuid(0,0,0,0);
				bTexturesHaveChanged = true;
			}
		}
	}
#endif // WITH_EDITORONLY_DATA

	return bTexturesHaveChanged;
}


FExpressionInput* UMaterial::GetExpressionInputForProperty(EMaterialProperty InProperty)
{
	switch (InProperty)
	{
		case MP_EmissiveColor:			return &EmissiveColor;
		case MP_Opacity:				return &Opacity;
		case MP_OpacityMask:			return &OpacityMask;
		case MP_BaseColor:				return &BaseColor;
		case MP_Metallic:				return &Metallic;
		case MP_Specular:				return &Specular;
		case MP_Roughness:				return &Roughness;
		case MP_Normal:					return &Normal;
		case MP_WorldPositionOffset:	return &WorldPositionOffset;
		case MP_WorldDisplacement:		return &WorldDisplacement;
		case MP_TessellationMultiplier:	return &TessellationMultiplier;
		case MP_SubsurfaceColor:		return &SubsurfaceColor;
		case MP_CustomData0:			return &ClearCoat;
		case MP_CustomData1:			return &ClearCoatRoughness;
		case MP_AmbientOcclusion:		return &AmbientOcclusion;
		case MP_Refraction:				return &Refraction;
		case MP_MaterialAttributes:		return &MaterialAttributes;
		case MP_PixelDepthOffset:		return &PixelDepthOffset;
	}

	if (InProperty >= MP_CustomizedUVs0 && InProperty <= MP_CustomizedUVs7)
	{
		return &CustomizedUVs[InProperty - MP_CustomizedUVs0];
	}

	return nullptr;
}

void UMaterial::GetAllCustomOutputExpressions(TArray<class UMaterialExpressionCustomOutput*>& OutCustomOutputs) const
{
	for (UMaterialExpression* Expression : Expressions)
	{
		UMaterialExpressionCustomOutput* CustomOutput = Cast<UMaterialExpressionCustomOutput>(Expression);
		if (CustomOutput)
		{
			OutCustomOutputs.Add(CustomOutput);
		}
	}
}

void UMaterial::GetAllExpressionsForCustomInterpolators(TArray<class UMaterialExpression*>& OutExpressions) const
{
	for (UMaterialExpression* Expression : Expressions)
	{
		if (Expression &&
			(Expression->IsA(UMaterialExpressionVertexInterpolator::StaticClass()) ||
			Expression->IsA(UMaterialExpressionMaterialFunctionCall::StaticClass())) )
		{
				OutExpressions.Add(Expression);
		}
	}
}

#if WITH_EDITOR
bool UMaterial::GetAllReferencedExpressions(TArray<UMaterialExpression*>& OutExpressions, class FStaticParameterSet* InStaticParameterSet)
{
	OutExpressions.Empty();

	for (int32 MPIdx = 0; MPIdx < MP_MAX; MPIdx++)
	{
		EMaterialProperty MaterialProp = EMaterialProperty(MPIdx);
		TArray<UMaterialExpression*> MPRefdExpressions;
		if (GetExpressionsInPropertyChain(MaterialProp, MPRefdExpressions, InStaticParameterSet) == true)
		{
			for (int32 AddIdx = 0; AddIdx < MPRefdExpressions.Num(); AddIdx++)
			{
				OutExpressions.AddUnique(MPRefdExpressions[AddIdx]);
			}
		}
	}

	TArray<class UMaterialExpressionCustomOutput*> CustomOutputExpressions;
	GetAllCustomOutputExpressions(CustomOutputExpressions);
	for (UMaterialExpressionCustomOutput* Expression : CustomOutputExpressions)
	{
		TArray<FExpressionInput*> ProcessedInputs;
		RecursiveGetExpressionChain(Expression, ProcessedInputs, OutExpressions, InStaticParameterSet);
	}

	return true;
}


bool UMaterial::GetExpressionsInPropertyChain(EMaterialProperty InProperty, 
	TArray<UMaterialExpression*>& OutExpressions, class FStaticParameterSet* InStaticParameterSet)
{
	OutExpressions.Empty();
	FExpressionInput* StartingExpression = GetExpressionInputForProperty(InProperty);

	if (StartingExpression == NULL)
	{
		// Failed to find the starting expression
		return false;
	}

	TArray<FExpressionInput*> ProcessedInputs;
	if (StartingExpression->Expression)
	{
		ProcessedInputs.AddUnique(StartingExpression);
		RecursiveGetExpressionChain(StartingExpression->Expression, ProcessedInputs, OutExpressions, InStaticParameterSet);
	}
	return true;
}

bool UMaterial::GetParameterSortPriority(FName ParameterName, int32& OutSortPriority) const
{
#if WITH_EDITOR
	for (UMaterialExpression* Expression : Expressions)
	{
		UMaterialExpressionParameter* Parameter = Cast<UMaterialExpressionParameter>(Expression);
		if (Parameter && Expression->GetParameterName() == ParameterName)
		{
			OutSortPriority = Parameter->SortPriority;
			return true;
		}
		UMaterialExpressionTextureSampleParameter* TextureParameter = Cast<UMaterialExpressionTextureSampleParameter>(Expression);
		if (TextureParameter && Expression->GetParameterName() == ParameterName)
		{
			OutSortPriority = TextureParameter->SortPriority;
			return true;
		}
	}
#endif
	return false;
}

bool UMaterial::GetGroupSortPriority(const FString& InGroupName, int32& OutSortPriority) const
{
#if WITH_EDITOR
	const FParameterGroupData* ParameterGroupDataElement = ParameterGroupData.FindByPredicate([&InGroupName](const FParameterGroupData& DataElement)
	{
		return InGroupName == DataElement.GroupName;
	});
	if (ParameterGroupDataElement != nullptr)
	{
		OutSortPriority = ParameterGroupDataElement->GroupSortPriority;
		return true;
	}
#endif
	return false;
}

bool UMaterial::GetTexturesInPropertyChain(EMaterialProperty InProperty, TArray<UTexture*>& OutTextures,
	TArray<FName>* OutTextureParamNames, class FStaticParameterSet* InStaticParameterSet)
{
	TArray<UMaterialExpression*> ChainExpressions;
	if (GetExpressionsInPropertyChain(InProperty, ChainExpressions, InStaticParameterSet) == true)
	{
		// Extract the texture and texture parameter expressions...
		for (int32 ExpressionIdx = 0; ExpressionIdx < ChainExpressions.Num(); ExpressionIdx++)
		{
			UMaterialExpression* MatExp = ChainExpressions[ExpressionIdx];
			if (MatExp != NULL)
			{
				// Is it a texture sample or texture parameter sample?
				UMaterialExpressionTextureSample* TextureSampleExp = Cast<UMaterialExpressionTextureSample>(MatExp);
				if (TextureSampleExp != NULL)
				{
					// Check the default texture...
					if (TextureSampleExp->Texture != NULL)
					{
						OutTextures.Add(TextureSampleExp->Texture);
					}

					if (OutTextureParamNames != NULL)
					{
						// If the expression is a parameter, add it's name to the texture names array
						UMaterialExpressionTextureSampleParameter* TextureSampleParamExp = Cast<UMaterialExpressionTextureSampleParameter>(MatExp);
						if (TextureSampleParamExp != NULL)
						{
							OutTextureParamNames->AddUnique(TextureSampleParamExp->ParameterName);
						}
					}
				}
			}
		}
	
		return true;
	}

	return false;
}

bool UMaterial::RecursiveGetExpressionChain(UMaterialExpression* InExpression, TArray<FExpressionInput*>& InOutProcessedInputs, 
	TArray<UMaterialExpression*>& OutExpressions, class FStaticParameterSet* InStaticParameterSet)
{
	OutExpressions.AddUnique(InExpression);
	TArray<FExpressionInput*> Inputs = InExpression->GetInputs();
	for (int32 InputIdx = 0; InputIdx < Inputs.Num(); InputIdx++)
	{
		FExpressionInput* InnerInput = Inputs[InputIdx];
		if (InnerInput != NULL)
		{
			int32 DummyIdx;
			if (InOutProcessedInputs.Find(InnerInput,DummyIdx) == false)
			{
				if (InnerInput->Expression)
				{
					bool bProcessInput = true;
					if (InStaticParameterSet != NULL)
					{
						// By default, static switches use B...
						// Is this a static switch parameter?
						//@todo. Handle Terrain weight map layer expression here as well!
						UMaterialExpressionStaticSwitchParameter* StaticSwitchExp = Cast<UMaterialExpressionStaticSwitchParameter>(InExpression);
						if (StaticSwitchExp != NULL)
						{
							bool bUseInputA = StaticSwitchExp->DefaultValue;
							FName StaticSwitchExpName = StaticSwitchExp->ParameterName;
							for (int32 CheckIdx = 0; CheckIdx < InStaticParameterSet->StaticSwitchParameters.Num(); CheckIdx++)
							{
								FStaticSwitchParameter& SwitchParam = InStaticParameterSet->StaticSwitchParameters[CheckIdx];
								if (SwitchParam.ParameterName == StaticSwitchExpName)
								{
									// Found it...
									if (SwitchParam.bOverride == true)
									{
										bUseInputA = SwitchParam.Value;
										break;
									}
								}
							}

							if (bUseInputA == true)
							{
								if (InnerInput->Expression != StaticSwitchExp->A.Expression)
								{
									bProcessInput = false;
								}
							}
							else
							{
								if (InnerInput->Expression != StaticSwitchExp->B.Expression)
								{
									bProcessInput = false;
								}
							}
						}
					}

					if (bProcessInput == true)
					{
						InOutProcessedInputs.Add(InnerInput);
						RecursiveGetExpressionChain(InnerInput->Expression, InOutProcessedInputs, OutExpressions, InStaticParameterSet);
					}
				}
			}
		}
	}

	return true;
}
#endif // WITH_EDITOR

void UMaterial::AppendReferencedTextures(TArray<UTexture*>& InOutTextures) const
{
	for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ExpressionIndex++)
	{
		UMaterialExpression* Expression = Expressions[ExpressionIndex];
		UMaterialExpressionMaterialFunctionCall* MaterialFunctionNode = Cast<UMaterialExpressionMaterialFunctionCall>(Expression);

		if (MaterialFunctionNode && MaterialFunctionNode->MaterialFunction)
		{
			TArray<UMaterialFunction*> Functions;
			Functions.Add(MaterialFunctionNode->MaterialFunction);

			MaterialFunctionNode->MaterialFunction->GetDependentFunctions(Functions);

			// Handle nested functions
			for (int32 FunctionIndex = 0; FunctionIndex < Functions.Num(); FunctionIndex++)
			{
				UMaterialFunction* CurrentFunction = Functions[FunctionIndex];
				CurrentFunction->AppendReferencedTextures(InOutTextures);
			}
		}
		// Not all expressions are cooked
		else if (Expression)
		{
			UTexture* ReferencedTexture = Expression->GetReferencedTexture();

			if (ReferencedTexture)
			{
				InOutTextures.AddUnique(ReferencedTexture);
			}
		}
	}
}

#if WITH_EDITOR
void UMaterial::RecursiveUpdateRealtimePreview( UMaterialExpression* InExpression, TArray<UMaterialExpression*>& InOutExpressionsToProcess )
{
	// remove ourselves from the list to process
	InOutExpressionsToProcess.Remove(InExpression);

	bool bOldRealtimePreview = InExpression->bRealtimePreview;

	// See if we know ourselves if we need realtime preview or not.
	InExpression->bRealtimePreview = InExpression->NeedsRealtimePreview();

	if( InExpression->bRealtimePreview )
	{
		if( InExpression->bRealtimePreview != bOldRealtimePreview )
		{
			InExpression->bNeedToUpdatePreview = true;
		}

		return;		
	}

	// We need to examine our inputs. If any of them need realtime preview, so do we.
	TArray<FExpressionInput*> Inputs = InExpression->GetInputs();
	for (int32 InputIdx = 0; InputIdx < Inputs.Num(); InputIdx++)
	{
		FExpressionInput* InnerInput = Inputs[InputIdx];
		if (InnerInput != NULL && InnerInput->Expression != NULL)
		{
			// See if we still need to process this expression, and if so do that first.
			if (InOutExpressionsToProcess.Find(InnerInput->Expression) != INDEX_NONE)
			{
				RecursiveUpdateRealtimePreview(InnerInput->Expression, InOutExpressionsToProcess);
			}

			// If our input expression needed realtime preview, we do too.
			if( InnerInput->Expression->bRealtimePreview )
			{

				InExpression->bRealtimePreview = true;
				if( InExpression->bRealtimePreview != bOldRealtimePreview )
				{
					InExpression->bNeedToUpdatePreview = true;
				}
				return;		
			}
		}
	}

	if( InExpression->bRealtimePreview != bOldRealtimePreview )
	{
		InExpression->bNeedToUpdatePreview = true;
	}
}
#endif // WITH_EDITOR

void UMaterial::AppendReferencedFunctionIdsTo(TArray<FGuid>& Ids) const
{
	for (int32 FunctionIndex = 0; FunctionIndex < MaterialFunctionInfos.Num(); FunctionIndex++)
	{
		Ids.AddUnique(MaterialFunctionInfos[FunctionIndex].StateId);
	}
}

void UMaterial::AppendReferencedParameterCollectionIdsTo(TArray<FGuid>& Ids) const
{
	for (int32 CollectionIndex = 0; CollectionIndex < MaterialParameterCollectionInfos.Num(); CollectionIndex++)
	{
		Ids.AddUnique(MaterialParameterCollectionInfos[CollectionIndex].StateId);
	}
}

#if WITH_EDITOR
int32 UMaterial::CompilePropertyEx( FMaterialCompiler* Compiler, const FGuid& AttributeID )
{
	const EMaterialProperty Property = FMaterialAttributeDefinitionMap::GetProperty(AttributeID);

	if( bUseMaterialAttributes && MP_DiffuseColor != Property && MP_SpecularColor != Property )
	{
		return MaterialAttributes.CompileWithDefault(Compiler, AttributeID);
	}

	switch (Property)
	{
		case MP_Opacity:				return Opacity.CompileWithDefault(Compiler, Property);
		case MP_OpacityMask:			return OpacityMask.CompileWithDefault(Compiler, Property);
		case MP_Metallic:				return Metallic.CompileWithDefault(Compiler, Property);
		case MP_Specular:				return Specular.CompileWithDefault(Compiler, Property);
		case MP_Roughness:				return Roughness.CompileWithDefault(Compiler, Property);
		case MP_TessellationMultiplier:	return TessellationMultiplier.CompileWithDefault(Compiler, Property);
		case MP_CustomData0:			return ClearCoat.CompileWithDefault(Compiler, Property);
		case MP_CustomData1:			return ClearCoatRoughness.CompileWithDefault(Compiler, Property);
		case MP_AmbientOcclusion:		return AmbientOcclusion.CompileWithDefault(Compiler, Property);
		case MP_Refraction:				return Refraction.CompileWithDefault(Compiler, Property);
		case MP_EmissiveColor:			return EmissiveColor.CompileWithDefault(Compiler, Property);
		case MP_BaseColor:				return BaseColor.CompileWithDefault(Compiler, Property);
		case MP_SubsurfaceColor:		return SubsurfaceColor.CompileWithDefault(Compiler, Property);
		case MP_Normal:					return Normal.CompileWithDefault(Compiler, Property);
		case MP_WorldPositionOffset:	return WorldPositionOffset.CompileWithDefault(Compiler, Property);
		case MP_WorldDisplacement:		return WorldDisplacement.CompileWithDefault(Compiler, Property);
		case MP_PixelDepthOffset:		return PixelDepthOffset.CompileWithDefault(Compiler, Property);
		default:
			if (Property >= MP_CustomizedUVs0 && Property <= MP_CustomizedUVs7)
			{
				const int32 TextureCoordinateIndex = Property - MP_CustomizedUVs0;

				if (CustomizedUVs[TextureCoordinateIndex].Expression && TextureCoordinateIndex < NumCustomizedUVs)
				{
					return CustomizedUVs[TextureCoordinateIndex].CompileWithDefault(Compiler, Property);
				}
				else
				{
					// The user did not customize this UV, pass through the vertex texture coordinates
					return Compiler->TextureCoordinate(TextureCoordinateIndex, false, false);
				}
			}
		
	}

	check(0);
	return INDEX_NONE;
}
#endif // WITH_EDITOR

void UMaterial::NotifyCompilationFinished(UMaterialInterface* Material)
{
	UMaterial::OnMaterialCompilationFinished().Broadcast(Material);
}

void UMaterial::ForceRecompileForRendering()
{
	CacheResourceShadersForRendering( false );
}

UMaterial::FMaterialCompilationFinished UMaterial::MaterialCompilationFinishedEvent;
UMaterial::FMaterialCompilationFinished& UMaterial::OnMaterialCompilationFinished()
{
	return MaterialCompilationFinishedEvent;
}

void UMaterial::AllMaterialsCacheResourceShadersForRendering()
{
	for (TObjectIterator<UMaterial> It; It; ++It)
	{
		UMaterial* Material = *It;

		Material->CacheResourceShadersForRendering(false);
	}
}

/**
 * Lists all materials that read from scene color.
 */
static void ListSceneColorMaterials()
{
	int32 NumSceneColorMaterials = 0;

	UMaterialInterface::IterateOverActiveFeatureLevels([&](ERHIFeatureLevel::Type FeatureLevel) 
	{
		FString FeatureLevelName;
		GetFeatureLevelName(FeatureLevel, FeatureLevelName);

		for (TObjectIterator<UMaterialInterface> It; It; ++It)
		{
			UMaterialInterface* Mat = *It;
			const FMaterial* MatRes = Mat->GetRenderProxy(false)->GetMaterial(FeatureLevel);
			if (MatRes && MatRes->RequiresSceneColorCopy_GameThread())
			{
				UMaterial* BaseMat = Mat->GetMaterial();
				UE_LOG(LogConsoleResponse, Display, TEXT("[SepTrans=%d][FeatureLevel=%s] %s"),
					BaseMat ? BaseMat->bEnableSeparateTranslucency : 3,
					*FeatureLevelName,
					*Mat->GetPathName()
					);
				NumSceneColorMaterials++;
			}
		}
	});
	UE_LOG(LogConsoleResponse,Display,TEXT("%d loaded materials read from scene color."),NumSceneColorMaterials);
}

static FAutoConsoleCommand CmdListSceneColorMaterials(
	TEXT("r.ListSceneColorMaterials"),
	TEXT("Lists all materials that read from scene color."),
	FConsoleCommandDelegate::CreateStatic(ListSceneColorMaterials)
	);

float UMaterial::GetOpacityMaskClipValue() const
{
	return OpacityMaskClipValue;
}

bool UMaterial::GetCastDynamicShadowAsMasked() const
{
	return bCastDynamicShadowAsMasked;
}

EBlendMode UMaterial::GetBlendMode() const
{
	if (EBlendMode(BlendMode) == BLEND_Masked)
	{
		if (bCanMaskedBeAssumedOpaque)
		{
			return BLEND_Opaque;
		}
		else
		{
			return BLEND_Masked;
		}
	}
	else
	{
		return BlendMode;
	}
}

EMaterialShadingModel UMaterial::GetShadingModel() const
{
	switch (MaterialDomain)
	{
		case MD_Surface:
		case MD_Volume:
			return ShadingModel;
		case MD_DeferredDecal:
			return MSM_DefaultLit;

		// Post process and light function materials must be rendered with the unlit model.
		case MD_PostProcess:
		case MD_LightFunction:
		case MD_UI:
			return MSM_Unlit;

		default:
			checkNoEntry();
			return MSM_Unlit;
	}
}

bool UMaterial::IsTwoSided() const
{
	return TwoSided != 0;
}

bool UMaterial::IsDitheredLODTransition() const
{
	return DitheredLODTransition != 0;
}

bool UMaterial::IsTranslucencyWritingCustomDepth() const
{
	return AllowTranslucentCustomDepthWrites != 0 && IsTranslucentBlendMode(GetBlendMode());
}

bool UMaterial::IsMasked() const
{
	return GetBlendMode() == BLEND_Masked || (GetBlendMode() == BLEND_Translucent && GetCastDynamicShadowAsMasked());
}

USubsurfaceProfile* UMaterial::GetSubsurfaceProfile_Internal() const
{
	checkSlow(IsInGameThread());
	return SubsurfaceProfile; 
}

bool UMaterial::IsPropertyActive(EMaterialProperty InProperty) const 
{
	if(MaterialDomain == MD_PostProcess)
	{
		return InProperty == MP_EmissiveColor || ( BlendableOutputAlpha && InProperty == MP_Opacity );
	}
	else if(MaterialDomain == MD_LightFunction)
	{
		// light functions should already use MSM_Unlit but we also we don't want WorldPosOffset
		return InProperty == MP_EmissiveColor;
	}
	else if(MaterialDomain == MD_DeferredDecal)
	{
		if (InProperty >= MP_CustomizedUVs0 && InProperty <= MP_CustomizedUVs7)

		{
			return true;
		}
		else if (InProperty == MP_MaterialAttributes)
		{
			// todo: MaterialAttruibutes would not return true, should it? Why we don't check for the checkbox in the material
			return true;
		}
		else if( InProperty == MP_WorldPositionOffset )
		{
			// Note: DeferredDecals don't support this but MeshDecals do
			return true;
		}

		switch(DecalBlendMode)
		{
			case DBM_Translucent:
				return InProperty == MP_EmissiveColor
					|| InProperty == MP_Normal
					|| InProperty == MP_Metallic
					|| InProperty == MP_Specular
					|| InProperty == MP_BaseColor
					|| InProperty == MP_Roughness
					|| InProperty == MP_Opacity;

			case DBM_Stain:
				return InProperty == MP_EmissiveColor
					|| InProperty == MP_Normal
					|| InProperty == MP_Metallic
					|| InProperty == MP_Specular
					|| InProperty == MP_BaseColor
					|| InProperty == MP_Roughness
					|| InProperty == MP_Opacity;

			case DBM_Normal:
				return InProperty == MP_Normal
					|| InProperty == MP_Opacity;

			case DBM_Emissive:
				// even emissive supports opacity
				return InProperty == MP_EmissiveColor
					|| InProperty == MP_Opacity;

			case DBM_DBuffer_ColorNormalRoughness:
				return InProperty == MP_Normal
					|| InProperty == MP_BaseColor
					|| InProperty == MP_Roughness
					|| InProperty == MP_Opacity;

			case DBM_DBuffer_Color:
				return InProperty == MP_BaseColor
					|| InProperty == MP_Opacity;

			case DBM_DBuffer_ColorNormal:
				return InProperty == MP_BaseColor
					|| InProperty == MP_Normal
					|| InProperty == MP_Opacity;

			case DBM_DBuffer_ColorRoughness:
				return InProperty == MP_BaseColor
					|| InProperty == MP_Roughness
					|| InProperty == MP_Opacity;

			case DBM_DBuffer_NormalRoughness:
				return InProperty == MP_Normal
					|| InProperty == MP_Roughness
					|| InProperty == MP_Opacity;

			case DBM_DBuffer_Normal:
				return InProperty == MP_Normal
					|| InProperty == MP_Opacity;

			case DBM_DBuffer_Roughness:
				return InProperty == MP_Roughness
					|| InProperty == MP_Opacity;

			case DBM_Volumetric_DistanceFunction:
				return InProperty == MP_EmissiveColor
					|| InProperty == MP_Normal
					|| InProperty == MP_Metallic
					|| InProperty == MP_Specular
					|| InProperty == MP_BaseColor
					|| InProperty == MP_Roughness
					|| InProperty == MP_OpacityMask;

			default:
				// if you create a new mode it needs to expose the right pins
				return false;
		}
	}
	else if (MaterialDomain == MD_Volume)
	{
		return InProperty == MP_EmissiveColor
			|| InProperty == MP_Opacity
			|| InProperty == MP_BaseColor;
	}
	else if ( MaterialDomain == MD_UI )
	{
		return InProperty == MP_EmissiveColor
			|| ( InProperty == MP_WorldPositionOffset )
			|| ( InProperty == MP_OpacityMask && BlendMode == BLEND_Masked ) 
			|| ( InProperty == MP_Opacity && IsTranslucentBlendMode((EBlendMode)BlendMode) && BlendMode != BLEND_Modulate )
			|| ( InProperty >= MP_CustomizedUVs0 && InProperty <= MP_CustomizedUVs7);
		{
			return true;
		}
	}

	const bool bIsTranslucentBlendMode = IsTranslucentBlendMode((EBlendMode)BlendMode);
	const bool bIsNonDirectionalTranslucencyLightingMode = TranslucencyLightingMode == TLM_VolumetricNonDirectional || TranslucencyLightingMode == TLM_VolumetricPerVertexNonDirectional;
	const bool bIsVolumetricTranslucencyLightingMode = TranslucencyLightingMode == TLM_VolumetricNonDirectional 
		|| TranslucencyLightingMode == TLM_VolumetricDirectional 
		|| TranslucencyLightingMode == TLM_VolumetricPerVertexNonDirectional 
		|| TranslucencyLightingMode == TLM_VolumetricPerVertexDirectional;

	bool Active = true;

	switch (InProperty)
	{
	case MP_DiffuseColor:
	case MP_SpecularColor:
		Active = false;
		break;
	case MP_Refraction:
		Active =bIsTranslucentBlendMode && BlendMode != BLEND_Modulate;
		break;
	case MP_Opacity:
		Active = bIsTranslucentBlendMode && BlendMode != BLEND_Modulate;
		if (IsSubsurfaceShadingModel(ShadingModel))
		{
			Active = true;
		}
		break;
	case MP_OpacityMask:
		Active = BlendMode == BLEND_Masked;
		break;
	case MP_BaseColor:
	case MP_AmbientOcclusion:
		Active = ShadingModel != MSM_Unlit;
		break;
	case MP_Specular:
	case MP_Roughness:
		Active = ShadingModel != MSM_Unlit && (!bIsTranslucentBlendMode || !bIsVolumetricTranslucencyLightingMode);
		break;
	case MP_Metallic:
		// Subsurface models store opacity in place of Metallic in the GBuffer
		Active = ShadingModel != MSM_Unlit && (!bIsTranslucentBlendMode || !bIsVolumetricTranslucencyLightingMode);
		break;
	case MP_Normal:
		Active = (ShadingModel != MSM_Unlit && (!bIsTranslucentBlendMode || !bIsNonDirectionalTranslucencyLightingMode)) || Refraction.IsConnected();
		break;
	case MP_SubsurfaceColor:
		Active = ShadingModel == MSM_Subsurface || ShadingModel == MSM_PreintegratedSkin || ShadingModel == MSM_TwoSidedFoliage || ShadingModel == MSM_Cloth;
		break;
	case MP_CustomData0:
		Active = ShadingModel == MSM_ClearCoat || ShadingModel == MSM_Hair || ShadingModel == MSM_Cloth || ShadingModel == MSM_Eye;
		break;
	case MP_CustomData1:
		Active = ShadingModel == MSM_ClearCoat || ShadingModel == MSM_Eye;
		break;
	case MP_TessellationMultiplier:
	case MP_WorldDisplacement:
		Active = D3D11TessellationMode != MTM_NoTessellation;
		break;
	case MP_EmissiveColor:
		// Emissive is always active, even for light functions and post process materials
		Active = true;
		break;
	case MP_WorldPositionOffset:
		Active = true;
		break;
	case MP_PixelDepthOffset:
		Active = !bIsTranslucentBlendMode;
		break;
	case MP_MaterialAttributes:
	default:
		Active = true;
		break;
	}
	return Active;
}

// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
FVxgiMaterialProperties UMaterial::GetVxgiMaterialProperties() const
{
	FVxgiMaterialProperties Properties;
	Properties.bVxgiConeTracingEnabled = bVxgiConeTracingEnable;
	Properties.bUsedWithVxgiVoxelization = bUsedWithVxgiVoxelization;
	Properties.bVxgiAllowTesselationDuringVoxelization = bVxgiAllowTesselationDuringVoxelization;
	Properties.bVxgiOmniDirectional = bVxgiOmniDirectional;
	Properties.bVxgiProportionalEmittance = bVxgiProportionalEmittance;
	Properties.bVxgiCoverageSupersampling = bVxgiCoverageSupersampling;
	Properties.VxgiMaterialSamplingRate = VxgiMaterialSamplingRate;
	Properties.VxgiOpacityNoiseScaleBias = VxgiOpacityNoiseScaleBias;
	Properties.VxgiVoxelizationThickness = VxgiVoxelizationThickness;
	return Properties;
}
#endif
// NVCHANGE_END: Add VXGI

#if WITH_EDITORONLY_DATA
void UMaterial::FlipExpressionPositions(const TArray<UMaterialExpression*>& Expressions, const TArray<UMaterialExpressionComment*>& Comments, bool bScaleCoords, UMaterial* InMaterial)
{
	// Rough estimate of average increase in node size for the new editor
	const float PosScaling = bScaleCoords ? 1.25f : 1.0f;

	if (InMaterial)
	{
		InMaterial->EditorX = -InMaterial->EditorX;
	}
	for (int32 ExpressionIndex = 0; ExpressionIndex < Expressions.Num(); ExpressionIndex++)
	{
		UMaterialExpression* Expression = Expressions[ExpressionIndex];
		if (Expression)
		{
			Expression->MaterialExpressionEditorX = -Expression->MaterialExpressionEditorX * PosScaling;
			Expression->MaterialExpressionEditorY *= PosScaling;
		}
	}
	for (int32 ExpressionIndex = 0; ExpressionIndex < Comments.Num(); ExpressionIndex++)
	{
		UMaterialExpressionComment* Comment = Comments[ExpressionIndex];
		if (Comment)
		{
			Comment->MaterialExpressionEditorX = (-Comment->MaterialExpressionEditorX - Comment->SizeX) * PosScaling;
			Comment->MaterialExpressionEditorY *= PosScaling;
			Comment->SizeX *= PosScaling;
			Comment->SizeY *= PosScaling;
		}
	}
}

void UMaterial::FixCommentPositions(const TArray<UMaterialExpressionComment*>& Comments)
{
	// equivalent to 1/1.25 * 0.25 to get the amount that should have been used when first flipping
	const float SizeScaling = 0.2f;

	for (int32 Index = 0; Index < Comments.Num(); Index++)
	{
		UMaterialExpressionComment* Comment = Comments[Index];
		Comment->MaterialExpressionEditorX -= Comment->SizeX * SizeScaling;
	}
}

bool UMaterial::HasFlippedCoordinates()
{
	uint32 ReversedInputCount = 0;
	uint32 StandardInputCount = 0;

	// Check inputs to see if they are right of the root node
	for (int32 InputIndex = 0; InputIndex < MP_MAX; InputIndex++)
	{
		FExpressionInput* Input = GetExpressionInputForProperty((EMaterialProperty)InputIndex);
		if (Input && Input->Expression)
		{
			if (Input->Expression->MaterialExpressionEditorX > EditorX)
			{
				++ReversedInputCount;
			}
			else
			{
				++StandardInputCount;
			}
		}
	}

	// Can't be sure coords are flipped if most are set out correctly
	return ReversedInputCount > StandardInputCount;
}
#endif //WITH_EDITORONLY_DATA

void UMaterial::GetLightingGuidChain(bool bIncludeTextures, TArray<FGuid>& OutGuids) const
{
#if WITH_EDITORONLY_DATA
	if (bIncludeTextures)
	{
		OutGuids.Append(ReferencedTextureGuids);
	}
	AppendReferencedFunctionIdsTo(OutGuids);
	AppendReferencedParameterCollectionIdsTo(OutGuids);
	OutGuids.Add(StateId);
	Super::GetLightingGuidChain(bIncludeTextures, OutGuids);
#endif
}

#undef LOCTEXT_NAMESPACE
