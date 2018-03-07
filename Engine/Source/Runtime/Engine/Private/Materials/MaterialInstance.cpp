// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Materials/MaterialInstance.h"
#include "Stats/StatsMisc.h"
#include "EngineGlobals.h"
#include "BatchedElements.h"
#include "Engine/Font.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/LinkerLoad.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UnrealEngine.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionFontSampleParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionStaticComponentMaskParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialUniformExpressions.h"
#include "Materials/MaterialInstanceSupport.h"
#include "Engine/SubsurfaceProfile.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Components.h"
#include "HAL/LowLevelMemTracker.h"

/**
 * Cache uniform expressions for the given material.
 * @param MaterialInstance - The material instance for which to cache uniform expressions.
 */
void CacheMaterialInstanceUniformExpressions(const UMaterialInstance* MaterialInstance)
{
	// Only cache the unselected + unhovered material instance. Selection color
	// can change at runtime and would invalidate the parameter cache.
	if (MaterialInstance->Resources[0])
	{
		MaterialInstance->Resources[0]->CacheUniformExpressions_GameThread();
	}
}

/**
 * Recaches uniform expressions for all material instances with a given parent.
 * WARNING: This function is a noop outside of the Editor!
 * @param ParentMaterial - The parent material to look for.
 */
void RecacheMaterialInstanceUniformExpressions(const UMaterialInterface* ParentMaterial)
{
	if (GIsEditor)
	{
		UE_LOG(LogMaterial,Verbose,TEXT("Recaching MI Uniform Expressions for parent %s"), *ParentMaterial->GetFullName());
		TArray<FMICReentranceGuard> ReentranceGuards;
		for (TObjectIterator<UMaterialInstance> It; It; ++It)
		{
			UMaterialInstance* MaterialInstance = *It;
			do 
			{
				if (MaterialInstance->Parent == ParentMaterial)
				{
					UE_LOG(LogMaterial,Verbose,TEXT("--> %s"), *MaterialInstance->GetFullName());
					CacheMaterialInstanceUniformExpressions(*It);
					break;
				}
				new (ReentranceGuards) FMICReentranceGuard(MaterialInstance);
				MaterialInstance = Cast<UMaterialInstance>(MaterialInstance->Parent);
			} while (MaterialInstance && !MaterialInstance->ReentrantFlag);
			ReentranceGuards.Reset();
		}
	}
}

FFontParameterValue::ValueType FFontParameterValue::GetValue(const FFontParameterValue& Parameter)
{
	ValueType Value = NULL;
	if (Parameter.FontValue && Parameter.FontValue->Textures.IsValidIndex(Parameter.FontPage))
	{
		// get the texture for the font page
		Value = Parameter.FontValue->Textures[Parameter.FontPage];
	}
	return Value;
}

FMaterialInstanceResource::FMaterialInstanceResource(UMaterialInstance* InOwner,bool bInSelected,bool bInHovered)
	: FMaterialRenderProxy(bInSelected, bInHovered)
	, Parent(NULL)
	, Owner(InOwner)
	, GameThreadParent(NULL)
{
}

const FMaterial* FMaterialInstanceResource::GetMaterial(ERHIFeatureLevel::Type InFeatureLevel) const
{
	checkSlow(IsInParallelRenderingThread());

	if (Parent)
	{
		if (Owner->bHasStaticPermutationResource)
		{
			EMaterialQualityLevel::Type ActiveQualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
			FMaterialResource* StaticPermutationResource = Owner->StaticPermutationMaterialResources[ActiveQualityLevel][InFeatureLevel];

			if (StaticPermutationResource->GetRenderingThreadShaderMap())
			{
				// Verify that compilation has been finalized, the rendering thread shouldn't be touching it otherwise
				checkSlow(StaticPermutationResource->GetRenderingThreadShaderMap()->IsCompilationFinalized());
				// The shader map reference should have been NULL'ed if it did not compile successfully
				checkSlow(StaticPermutationResource->GetRenderingThreadShaderMap()->CompiledSuccessfully());
				return StaticPermutationResource;
			}
			else
			{
				EMaterialDomain Domain = (EMaterialDomain)StaticPermutationResource->GetMaterialDomain();
				UMaterial* FallbackMaterial = UMaterial::GetDefaultMaterial(Domain);
				//there was an error, use the default material's resource
				return FallbackMaterial->GetRenderProxy(IsSelected(), IsHovered())->GetMaterial(InFeatureLevel);
			}
		}
		else
		{
			//use the parent's material resource
			return Parent->GetRenderProxy(IsSelected(), IsHovered())->GetMaterial(InFeatureLevel);
		}
	}
	else 
	{
		UMaterial* FallbackMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		return FallbackMaterial->GetRenderProxy(IsSelected(), IsHovered())->GetMaterial(InFeatureLevel);
	}
}

FMaterial* FMaterialInstanceResource::GetMaterialNoFallback(ERHIFeatureLevel::Type InFeatureLevel) const
{
	checkSlow(IsInParallelRenderingThread());

	if (Parent)
	{
		if (Owner->bHasStaticPermutationResource)
		{
			EMaterialQualityLevel::Type ActiveQualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
			FMaterialResource* StaticPermutationResource = Owner->StaticPermutationMaterialResources[ActiveQualityLevel][InFeatureLevel];
			return StaticPermutationResource;
		}
		else
		{
			FMaterialRenderProxy* ParentProxy = Parent->GetRenderProxy(IsSelected(), IsHovered());

			if (ParentProxy)
			{
				return ParentProxy->GetMaterialNoFallback(InFeatureLevel);
			}
		}
	}
	return NULL;
}

UMaterialInterface* FMaterialInstanceResource::GetMaterialInterface() const
{
	return Owner;
}

bool FMaterialInstanceResource::GetScalarValue(
	const FName ParameterName, 
	float* OutValue,
	const FMaterialRenderContext& Context
	) const
{
	checkSlow(IsInParallelRenderingThread());

	static FName NameSubsurfaceProfile(TEXT("__SubsurfaceProfile"));
	if (ParameterName == NameSubsurfaceProfile)
	{
		const USubsurfaceProfile* MySubsurfaceProfileRT = GetSubsurfaceProfileRT();

		int32 AllocationId = 0;
		if (MySubsurfaceProfileRT)
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

	const float* Value = RenderThread_FindParameterByName<float>(ParameterName);
	if(Value)
	{
		*OutValue = *Value;
		return true;
	}
	else if (Parent)
	{
		return Parent->GetRenderProxy(IsSelected(), IsHovered())->GetScalarValue(ParameterName, OutValue, Context);
	}
	else
	{
		return false;
	}
}

bool FMaterialInstanceResource::GetVectorValue(
	const FName ParameterName, 
	FLinearColor* OutValue,
	const FMaterialRenderContext& Context
	) const
{
	checkSlow(IsInParallelRenderingThread());
	const FLinearColor* Value = RenderThread_FindParameterByName<FLinearColor>(ParameterName);
	if(Value)
	{
		*OutValue = *Value;
		return true;
	}
	else if(Parent)
	{
		return Parent->GetRenderProxy(IsSelected(), IsHovered())->GetVectorValue(ParameterName, OutValue, Context);
	}
	else
	{
		return false;
	}
}

bool FMaterialInstanceResource::GetTextureValue(
	const FName ParameterName,
	const UTexture** OutValue,
	const FMaterialRenderContext& Context
	) const
{
	checkSlow(IsInParallelRenderingThread());
	const UTexture* const * Value = RenderThread_FindParameterByName<const UTexture*>(ParameterName);
	if(Value && *Value)
	{
		*OutValue = *Value;
		return true;
	}
	else if(Parent)
	{
		return Parent->GetRenderProxy(IsSelected(), IsHovered())->GetTextureValue(ParameterName, OutValue, Context);
	}
	else
	{
		return false;
	}
}

void UMaterialInstance::PropagateDataToMaterialProxy()
{
	for (int32 i = 0; i < ARRAY_COUNT(Resources); i++)
	{
		if (Resources[i])
		{
			UpdateMaterialRenderProxy(*Resources[i]);
		}
	}
}

void FMaterialInstanceResource::GameThread_SetParent(UMaterialInterface* ParentMaterialInterface)
{
	check(IsInGameThread() || IsAsyncLoading());

	if (GameThreadParent != ParentMaterialInterface)
	{
		// Set the game thread accessible parent.
		UMaterialInterface* OldParent = GameThreadParent;
		GameThreadParent = ParentMaterialInterface;

		// Set the rendering thread's parent and instance pointers.
		check(ParentMaterialInterface != NULL);
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			InitMaterialInstanceResource,
			FMaterialInstanceResource*, Resource, this,
			UMaterialInterface*, Parent, ParentMaterialInterface,
			{
			Resource->Parent = Parent;
			Resource->InvalidateUniformExpressionCache();
		});

		if (OldParent)
		{
			// make sure that the old parent sticks around until we've set the new parent on FMaterialInstanceResource
			OldParent->ParentRefFence.BeginFence();
		}
	}
}

ENQUEUE_UNIQUE_RENDER_COMMAND_FIVEPARAMETER_DECLARE_TEMPLATE(
	SetMIParameterValue, ParameterType,
	FMaterialInstanceResource*, Resource0, Resource0,
	FMaterialInstanceResource*, Resource1, Resource1,
	FMaterialInstanceResource*, Resource2, Resource2,
	FName, ParameterName, Parameter.ParameterName,
	typename ParameterType::ValueType, Value, ParameterType::GetValue(Parameter),
	{
		Resource0->RenderThread_UpdateParameter(ParameterName, Value);
		if (Resource1)
		{
			Resource1->RenderThread_UpdateParameter(ParameterName, Value);
		}
		if (Resource2)
		{
			Resource2->RenderThread_UpdateParameter(ParameterName, Value);
		}
	});

/**
* Updates a parameter on the material instance from the game thread.
*/
template <typename ParameterType>
void GameThread_UpdateMIParameter(const UMaterialInstance* Instance, const ParameterType& Parameter)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_FIVEPARAMETER_CREATE_TEMPLATE(
		SetMIParameterValue, ParameterType,
		FMaterialInstanceResource*, Instance->Resources[0],
		FMaterialInstanceResource*, Instance->Resources[1],
		FMaterialInstanceResource*, Instance->Resources[2],
		FName, Parameter.ParameterName,
		typename ParameterType::ValueType, ParameterType::GetValue(Parameter)
		);
}

bool UMaterialInstance::UpdateParameters()
{
	bool bDirty = false;
	if(IsTemplate(RF_ClassDefaultObject)==false)
	{
		// Get a pointer to the parent material.
		UMaterial* ParentMaterial = NULL;
		UMaterialInstance* ParentInst = this;
		while(ParentInst && ParentInst->Parent)
		{
			if(ParentInst->Parent->IsA(UMaterial::StaticClass()))
			{
				ParentMaterial = Cast<UMaterial>(ParentInst->Parent);
				break;
			}
			else
			{
				ParentInst = Cast<UMaterialInstance>(ParentInst->Parent);
			}
		}

		if(ParentMaterial)
		{
			// Scalar parameters
			bDirty = UpdateParameterSet<FScalarParameterValue, UMaterialExpressionScalarParameter>(ScalarParameterValues, ParentMaterial) || bDirty;

			// Vector parameters	
			bDirty = UpdateParameterSet<FVectorParameterValue, UMaterialExpressionVectorParameter>(VectorParameterValues, ParentMaterial) || bDirty;

			// Texture parameters
			bDirty = UpdateParameterSet<FTextureParameterValue, UMaterialExpressionTextureSampleParameter>(TextureParameterValues, ParentMaterial) || bDirty;

			// Font parameters
			bDirty = UpdateParameterSet<FFontParameterValue, UMaterialExpressionFontSampleParameter>(FontParameterValues, ParentMaterial) || bDirty;

			// Static switch parameters
			bDirty = UpdateParameterSet<FStaticSwitchParameter, UMaterialExpressionStaticBoolParameter>(StaticParameters.StaticSwitchParameters, ParentMaterial) || bDirty;

			// Static component mask parameters
			bDirty = UpdateParameterSet<FStaticComponentMaskParameter, UMaterialExpressionStaticComponentMaskParameter>(StaticParameters.StaticComponentMaskParameters, ParentMaterial) || bDirty;

			// Custom parameters
			for (const auto& CustomParameterSetUpdater : CustomParameterSetUpdaters)
			{
				bDirty |= CustomParameterSetUpdater.Execute(StaticParameters, ParentMaterial);
			}
		}
	}
	return bDirty;
}

UMaterialInstance::UMaterialInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHasStaticPermutationResource = false;
}

void UMaterialInstance::PostInitProperties()	
{
	Super::PostInitProperties();

	if(!HasAnyFlags(RF_ClassDefaultObject))
	{
		Resources[0] = new FMaterialInstanceResource(this,false,false);
		if(GIsEditor)
		{
			Resources[1] = new FMaterialInstanceResource(this,true,false);
			Resources[2] = new FMaterialInstanceResource(this,false,true);
		}
	}
}

/**
 * Initializes MI parameters from the game thread.
 */
template <typename ParameterType>
void GameThread_InitMIParameters(const UMaterialInstance* Instance, const TArray<ParameterType>& Parameters)
{
	if (!Instance->HasAnyFlags(RF_ClassDefaultObject))
	{
		for (int32 ParameterIndex = 0; ParameterIndex < Parameters.Num(); ParameterIndex++)
		{
			GameThread_UpdateMIParameter(Instance, Parameters[ParameterIndex]);
		}
	}
}

void UMaterialInstance::InitResources()
{	
	// Find the instance's parent.
	UMaterialInterface* SafeParent = NULL;
	if (Parent)
	{
		SafeParent = Parent;
	}

	// Don't use the instance's parent if it has a circular dependency on the instance.
	if (SafeParent && SafeParent->IsDependent(this))
	{
		SafeParent = NULL;
	}

	// Don't allow MIDs as parents for material instances.
	if (SafeParent && SafeParent->IsA(UMaterialInstanceDynamic::StaticClass()))
	{
		SafeParent = NULL;
	}

	// If the instance doesn't have a valid parent, use the default material as the parent.
	if (!SafeParent)
	{
		SafeParent = UMaterial::GetDefaultMaterial(MD_Surface);
	}

	checkf(SafeParent, TEXT("Invalid parent on %s"), *GetFullName());

	// Set the material instance's parent on its resources.
	for (int32 CurResourceIndex = 0; CurResourceIndex < ARRAY_COUNT(Resources); ++CurResourceIndex)
	{
		if (Resources[CurResourceIndex] != NULL)
		{
			Resources[CurResourceIndex]->GameThread_SetParent(SafeParent);
		}
	}

	GameThread_InitMIParameters(this, ScalarParameterValues);
	GameThread_InitMIParameters(this, VectorParameterValues);
	GameThread_InitMIParameters(this, TextureParameterValues);
	GameThread_InitMIParameters(this, FontParameterValues);
	PropagateDataToMaterialProxy();

	CacheMaterialInstanceUniformExpressions(this);
}

const UMaterial* UMaterialInstance::GetMaterial() const
{
	check(IsInGameThread() || IsAsyncLoading());
	if(ReentrantFlag)
	{
		return UMaterial::GetDefaultMaterial(MD_Surface);
	}

	FMICReentranceGuard	Guard(this);
	if(Parent)
	{
		return Parent->GetMaterial();
	}
	else
	{
		return UMaterial::GetDefaultMaterial(MD_Surface);
	}
}

const UMaterial* UMaterialInstance::GetMaterial_Concurrent(TMicRecursionGuard& RecursionGuard) const
{
	if(!Parent || RecursionGuard.Contains(this))
	{
		return UMaterial::GetDefaultMaterial(MD_Surface);
	}

	RecursionGuard.Add(this);
	return Parent->GetMaterial_Concurrent(RecursionGuard);
}

UMaterial* UMaterialInstance::GetMaterial()
{
	if(ReentrantFlag)
	{
		return UMaterial::GetDefaultMaterial(MD_Surface);
	}

	FMICReentranceGuard	Guard(this);
	if(Parent)
	{
		return Parent->GetMaterial();
	}
	else
	{
		return UMaterial::GetDefaultMaterial(MD_Surface);
	}
}

bool UMaterialInstance::GetVectorParameterValue(FName ParameterName, FLinearColor& OutValue) const
{
	bool bFoundAValue = false;

	if(ReentrantFlag)
	{
		return false;
	}

	const FVectorParameterValue* ParameterValue = GameThread_FindParameterByName(VectorParameterValues, ParameterName);
	if(ParameterValue)
	{
		OutValue = ParameterValue->ParameterValue;
		return true;
	}
	else if(Parent)
	{
		FMICReentranceGuard	Guard(this);
		return Parent->GetVectorParameterValue(ParameterName,OutValue);
	}
	else
	{
		return false;
	}
}

bool UMaterialInstance::GetScalarParameterValue(FName ParameterName, float& OutValue) const
{
	bool bFoundAValue = false;

	if(ReentrantFlag)
	{
		return false;
	}

	const FScalarParameterValue* ParameterValue = GameThread_FindParameterByName(ScalarParameterValues,ParameterName);
	if(ParameterValue)
	{
		OutValue = ParameterValue->ParameterValue;
		return true;
	}
	else if(Parent)
	{
		FMICReentranceGuard	Guard(this);
		return Parent->GetScalarParameterValue(ParameterName,OutValue);
	}
	else
	{
		return false;
	}
}

bool UMaterialInstance::GetTextureParameterValue(FName ParameterName, UTexture*& OutValue) const
{
	if(ReentrantFlag)
	{
		return false;
	}

	const FTextureParameterValue* ParameterValue = GameThread_FindParameterByName(TextureParameterValues,ParameterName);
	if(ParameterValue && ParameterValue->ParameterValue)
	{
		OutValue = ParameterValue->ParameterValue;
		return true;
	}
	else if(Parent)
	{
		FMICReentranceGuard	Guard(this);
		return Parent->GetTextureParameterValue(ParameterName,OutValue);
	}
	else
	{
		return false;
	}
}

bool UMaterialInstance::GetTextureParameterOverrideValue(FName ParameterName, UTexture*& OutValue) const
{
	if(ReentrantFlag)
	{
		return false;
	}

	const FTextureParameterValue* ParameterValue = GameThread_FindParameterByName(TextureParameterValues,ParameterName);
	if(ParameterValue && ParameterValue->ParameterValue)
	{
		OutValue = ParameterValue->ParameterValue;
		return true;
	}
	else if(Parent)
	{
		FMICReentranceGuard	Guard(this);
		return Parent->GetTextureParameterOverrideValue(ParameterName,OutValue);
	}
	else
	{
		return false;
	}
}

bool UMaterialInstance::GetFontParameterValue(FName ParameterName,class UFont*& OutFontValue, int32& OutFontPage) const
{
	if( ReentrantFlag )
	{
		return false;
	}

	const FFontParameterValue* ParameterValue = GameThread_FindParameterByName(FontParameterValues,ParameterName);
	if(ParameterValue && ParameterValue->FontValue)
	{
		OutFontValue = ParameterValue->FontValue;
		OutFontPage = ParameterValue->FontPage;
		return true;
	}
	else if( Parent )
	{
		FMICReentranceGuard	Guard(this);
		return Parent->GetFontParameterValue(ParameterName,OutFontValue,OutFontPage);
	}
	else
	{
		return false;
	}
}

bool UMaterialInstance::GetRefractionSettings(float& OutBiasValue) const
{
	bool bFoundAValue = false;

	FName ParamName;
	if( GetLinkerUE4Version() >= VER_UE4_REFRACTION_BIAS_TO_REFRACTION_DEPTH_BIAS )
	{
		static FName NAME_RefractionDepthBias(TEXT("RefractionDepthBias"));
		ParamName = NAME_RefractionDepthBias;
	}
	else
	{
		static FName NAME_RefractionBias(TEXT("RefractionBias"));
		ParamName = NAME_RefractionBias;
	}

	const FScalarParameterValue* BiasParameterValue = GameThread_FindParameterByName(ScalarParameterValues, ParamName);
	if(BiasParameterValue )
	{
		OutBiasValue = BiasParameterValue->ParameterValue;
		return true;
	}
	else if(Parent)
	{
		return Parent->GetRefractionSettings(OutBiasValue);
	}
	else
	{
		return false;
	}
}

void UMaterialInstance::GetTextureExpressionValues(const FMaterialResource* MaterialResource, TArray<UTexture*>& OutTextures, TArray< TArray<int32> >* OutIndices) const
{
	const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >* ExpressionsByType[2];

	check(MaterialResource);

	ExpressionsByType[0] = &MaterialResource->GetUniform2DTextureExpressions();
	ExpressionsByType[1] = &MaterialResource->GetUniformCubeTextureExpressions();

	if (OutIndices) // Try to prevent resizing since this would be expensive.
	{
		OutIndices->Empty(ExpressionsByType[0]->Num() + ExpressionsByType[1]->Num());
	}

	for(int32 TypeIndex = 0;TypeIndex < ARRAY_COUNT(ExpressionsByType);TypeIndex++)
	{
		const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >& Expressions = *ExpressionsByType[TypeIndex];

		// Iterate over each of the material's texture expressions.
		for(int32 ExpressionIndex = 0;ExpressionIndex < Expressions.Num();ExpressionIndex++)
		{
			FMaterialUniformExpressionTexture* Expression = Expressions[ExpressionIndex];

			// Evaluate the expression in terms of this material instance.
			UTexture* Texture = NULL;
			Expression->GetGameThreadTextureValue(this,*MaterialResource,Texture, true);
			
			if (Texture)
			{
				int32 InsertIndex = OutTextures.AddUnique(Texture);

				if (OutIndices)
				{
					if (InsertIndex >= OutIndices->Num())
					{
						OutIndices->AddDefaulted(InsertIndex - OutIndices->Num() + 1);
					}
					(*OutIndices)[InsertIndex].Add(Expression->GetTextureIndex());
				}
			}
		}
	}
}

void UMaterialInstance::GetUsedTextures(TArray<UTexture*>& OutTextures, EMaterialQualityLevel::Type QualityLevel, bool bAllQualityLevels, ERHIFeatureLevel::Type FeatureLevel, bool bAllFeatureLevels) const
{
	OutTextures.Empty();

	// Do not care if we're running dedicated server
	if (!FPlatformProperties::IsServerOnly())
	{
		FInt32Range QualityLevelRange(0, EMaterialQualityLevel::Num - 1);
		if (!bAllQualityLevels)
		{
			if (QualityLevel == EMaterialQualityLevel::Num)
			{
				QualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
			}
			QualityLevelRange = FInt32Range(QualityLevel, QualityLevel);
		}

		FInt32Range FeatureLevelRange(0, ERHIFeatureLevel::Num - 1);
		if (!bAllFeatureLevels)
		{
			if (FeatureLevel == ERHIFeatureLevel::Num)
			{
				FeatureLevel = GMaxRHIFeatureLevel;
			}
			FeatureLevelRange = FInt32Range(FeatureLevel, FeatureLevel);
		}

		const UMaterial* BaseMaterial = GetMaterial();
		const UMaterialInstance* MaterialInstanceToUse = this;

		if (BaseMaterial && !BaseMaterial->IsDefaultMaterial())
		{
			// Walk up the material instance chain to the first parent that has static parameters
			while (MaterialInstanceToUse && !MaterialInstanceToUse->bHasStaticPermutationResource)
			{
				MaterialInstanceToUse = Cast<const UMaterialInstance>(MaterialInstanceToUse->Parent);
			}

			// Use the uniform expressions from the lowest material instance with static parameters in the chain, if one exists
			const UMaterialInterface* MaterialToUse = (MaterialInstanceToUse && MaterialInstanceToUse->bHasStaticPermutationResource) ? (const UMaterialInterface*)MaterialInstanceToUse : (const UMaterialInterface*)BaseMaterial;

			// Parse all relevant quality and feature levels.
			for (int32 QualityLevelIndex = QualityLevelRange.GetLowerBoundValue(); QualityLevelIndex <= QualityLevelRange.GetUpperBoundValue(); ++QualityLevelIndex)
			{
				for (int32 FeatureLevelIndex = FeatureLevelRange.GetLowerBoundValue(); FeatureLevelIndex <= FeatureLevelRange.GetUpperBoundValue(); ++FeatureLevelIndex)
				{
					const FMaterialResource* MaterialResource = MaterialToUse->GetMaterialResource((ERHIFeatureLevel::Type)FeatureLevelIndex, (EMaterialQualityLevel::Type)QualityLevelIndex);
					if (MaterialResource)
					{
						GetTextureExpressionValues(MaterialResource, OutTextures);
					}
				}
			}
		}
		else
		{
			// If the material instance has no material, use the default material.
			UMaterial::GetDefaultMaterial(MD_Surface)->GetUsedTextures(OutTextures, QualityLevel, bAllQualityLevels, FeatureLevel, bAllFeatureLevels);
		}
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
void UMaterialInstance::LogMaterialsAndTextures(FOutputDevice& Ar, int32 Indent) const
{
	auto World = GetWorld();
	const EMaterialQualityLevel::Type QualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
	const ERHIFeatureLevel::Type FeatureLevel = World ? World->FeatureLevel : GMaxRHIFeatureLevel;

	Ar.Logf(TEXT("%sMaterialInstance: %s"), FCString::Tab(Indent), *GetName());

	if (FPlatformProperties::IsServerOnly())
	{
		Ar.Logf(TEXT("%sNo Textures: IsServerOnly"), FCString::Tab(Indent + 1));
	}
	else
	{
		const UMaterialInstance* MaterialInstanceToUse = nullptr;
		const UMaterial* MaterialToUse = nullptr;

		const UMaterialInterface* CurrentMaterialInterface = this;
		{
			TSet<const UMaterialInterface*> MaterialParents;

			// Walk up the parent chain to the materials to use.
			while (CurrentMaterialInterface && !MaterialParents.Contains(CurrentMaterialInterface))
			{
				MaterialParents.Add(CurrentMaterialInterface);

				const UMaterialInstance* CurrentMaterialInstance = Cast<const UMaterialInstance>(CurrentMaterialInterface);
				const UMaterial* CurrentMaterial = Cast<const UMaterial>(CurrentMaterialInterface);

				// The parent material is the first parent of this class.
				if (!MaterialToUse && CurrentMaterial)
				{
					MaterialToUse = CurrentMaterial;
				}

				if (!MaterialInstanceToUse && CurrentMaterialInstance && CurrentMaterialInstance->bHasStaticPermutationResource)
				{
					MaterialInstanceToUse = CurrentMaterialInstance;
				}

				CurrentMaterialInterface = CurrentMaterialInstance ? CurrentMaterialInstance->Parent : nullptr;
			}
		}

		if (CurrentMaterialInterface)
		{
			Ar.Logf(TEXT("%sNo Textures : Cycling Parent Loop"), FCString::Tab(Indent + 1));
		}
		else if (MaterialInstanceToUse)
		{
			const FMaterialResource* MaterialResource = MaterialInstanceToUse->StaticPermutationMaterialResources[QualityLevel][FeatureLevel];
			if (MaterialResource)
			{
				if (MaterialResource->HasValidGameThreadShaderMap())
				{
					TArray<UTexture*> Textures;
					GetTextureExpressionValues(MaterialResource, Textures);
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
		else if (MaterialToUse)
		{
			MaterialToUse->LogMaterialsAndTextures(Ar, Indent + 1);
		}
		else
		{
			Ar.Logf(TEXT("%sNo Textures : No Material Found"), FCString::Tab(Indent + 1));
		}
	}
}
#endif

void UMaterialInstance::GetUsedTexturesAndIndices(TArray<UTexture*>& OutTextures, TArray< TArray<int32> >& OutIndices, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel) const
{
	OutTextures.Empty();
	OutIndices.Empty();

	if (!FPlatformProperties::IsServerOnly())
	{
		const UMaterialInstance* MaterialInstanceToUse = this;
		// Walk up the material instance chain to the first parent that has static parameters
		while (MaterialInstanceToUse && !MaterialInstanceToUse->bHasStaticPermutationResource)
		{
			MaterialInstanceToUse = Cast<const UMaterialInstance>(MaterialInstanceToUse->Parent);
		}

		if (MaterialInstanceToUse && MaterialInstanceToUse->bHasStaticPermutationResource)
		{
			const FMaterialResource* CurrentResource = MaterialInstanceToUse->StaticPermutationMaterialResources[QualityLevel][FeatureLevel];
			if (CurrentResource)
			{
				GetTextureExpressionValues(CurrentResource, OutTextures, &OutIndices);
			}
		}
		else // Use the uniform expressions from the base material
		{ 
			const UMaterial* Material = GetMaterial();
			if (Material)
			{
				const FMaterialResource* MaterialResource = Material->GetMaterialResource(FeatureLevel, QualityLevel);
				if( MaterialResource )
				{
					GetTextureExpressionValues(MaterialResource, OutTextures, &OutIndices);
				}
			}
			else // If the material instance has no material, use the default material.
			{
				UMaterial::GetDefaultMaterial(MD_Surface)->GetUsedTexturesAndIndices(OutTextures, OutIndices, QualityLevel, FeatureLevel);
			}
		}
	}
}

void UMaterialInstance::OverrideTexture(const UTexture* InTextureToOverride, UTexture* OverrideTexture, ERHIFeatureLevel::Type InFeatureLevel)
{
#if WITH_EDITOR
	bool bShouldRecacheMaterialExpressions = false;
	
	const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >* ExpressionsByType[2];

	const FMaterialResource* SourceMaterialResource = NULL;
	if (bHasStaticPermutationResource)
	{
		SourceMaterialResource = GetMaterialResource(InFeatureLevel);
		// Iterate over both the 2D textures and cube texture expressions.
		ExpressionsByType[0] = &SourceMaterialResource->GetUniform2DTextureExpressions();
		ExpressionsByType[1] = &SourceMaterialResource->GetUniformCubeTextureExpressions();
	}
	else
	{
		//@todo - this isn't handling chained MIC's correctly, where a parent in the chain has static parameters
		UMaterial* Material = GetMaterial();
		SourceMaterialResource = Material->GetMaterialResource(InFeatureLevel);
			
		// Iterate over both the 2D textures and cube texture expressions.
		ExpressionsByType[0] = &SourceMaterialResource->GetUniform2DTextureExpressions();
		ExpressionsByType[1] = &SourceMaterialResource->GetUniformCubeTextureExpressions();
	}
		
	for(int32 TypeIndex = 0;TypeIndex < ARRAY_COUNT(ExpressionsByType);TypeIndex++)
	{
		const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >& Expressions = *ExpressionsByType[TypeIndex];

		// Iterate over each of the material's texture expressions.
		for(int32 ExpressionIndex = 0;ExpressionIndex < Expressions.Num();ExpressionIndex++)
		{
			FMaterialUniformExpressionTexture* Expression = Expressions[ExpressionIndex];

			// Evaluate the expression in terms of this material instance.
			const bool bAllowOverride = false;
			UTexture* Texture = NULL;
			Expression->GetGameThreadTextureValue(this,*SourceMaterialResource,Texture,bAllowOverride);

			if( Texture != NULL && Texture == InTextureToOverride )
			{
				// Override this texture!
				Expression->SetTransientOverrideTextureValue( OverrideTexture );
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

void UMaterialInstance::OverrideVectorParameterDefault(FName ParameterName, const FLinearColor& Value, bool bOverride, ERHIFeatureLevel::Type InFeatureLevel)
{
#if WITH_EDITOR
	bool bShouldRecacheMaterialExpressions = false;

	if (bHasStaticPermutationResource)
	{
		const FMaterialResource* SourceMaterialResource = GetMaterialResource(InFeatureLevel);
		const TArray<TRefCountPtr<FMaterialUniformExpression> >& UniformExpressions = SourceMaterialResource->GetUniformVectorParameterExpressions();

		// Iterate over each of the material's texture expressions.
		for (int32 ExpressionIndex = 0; ExpressionIndex < UniformExpressions.Num(); ExpressionIndex++)
		{
			FMaterialUniformExpression* UniformExpression = UniformExpressions[ExpressionIndex];
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
	}

	if (bShouldRecacheMaterialExpressions)
	{
		RecacheUniformExpressions();
		RecacheMaterialInstanceUniformExpressions(this);
	}
#endif // #if WITH_EDITOR
}

void UMaterialInstance::OverrideScalarParameterDefault(FName ParameterName, float Value, bool bOverride, ERHIFeatureLevel::Type InFeatureLevel)
{
#if WITH_EDITOR
	bool bShouldRecacheMaterialExpressions = false;

	if (bHasStaticPermutationResource)
	{
		const FMaterialResource* SourceMaterialResource = GetMaterialResource(InFeatureLevel);
		const TArray<TRefCountPtr<FMaterialUniformExpression> >& UniformExpressions = SourceMaterialResource->GetUniformScalarParameterExpressions();

		// Iterate over each of the material's texture expressions.
		for (int32 ExpressionIndex = 0; ExpressionIndex < UniformExpressions.Num(); ExpressionIndex++)
		{
			FMaterialUniformExpression* UniformExpression = UniformExpressions[ExpressionIndex];
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
	}

	if (bShouldRecacheMaterialExpressions)
	{
		RecacheUniformExpressions();
		RecacheMaterialInstanceUniformExpressions(this);
	}
#endif // #if WITH_EDITOR
}

float UMaterialInstance::GetScalarParameterDefault(FName ParameterName, ERHIFeatureLevel::Type InFeatureLevel)
{
	if (bHasStaticPermutationResource )
	{
		if (FApp::CanEverRender())
		{
			const FMaterialResource* SourceMaterialResource = GetMaterialResource(InFeatureLevel);
			if (ensureAlways(SourceMaterialResource))
			{
				const TArray<TRefCountPtr<FMaterialUniformExpression> >& UniformExpressions = SourceMaterialResource->GetUniformScalarParameterExpressions();

				// Iterate over each of the material's texture expressions.
				for (int32 ExpressionIndex = 0; ExpressionIndex < UniformExpressions.Num(); ExpressionIndex++)
				{
					FMaterialUniformExpression* UniformExpression = UniformExpressions[ExpressionIndex];
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
	}

	return 0.f;
}

bool UMaterialInstance::CheckMaterialUsage(const EMaterialUsage Usage)
{
	check(IsInGameThread());
	UMaterial* Material = GetMaterial();
	if(Material)
	{
		bool bNeedsRecompile = false;
		bool bUsageSetSuccessfully = Material->SetMaterialUsage(bNeedsRecompile, Usage);
		if (bNeedsRecompile)
		{
			CacheResourceShadersForRendering();
			MarkPackageDirty();
		}
		return bUsageSetSuccessfully;
	}
	else
	{
		return false;
	}
}

bool UMaterialInstance::CheckMaterialUsage_Concurrent(const EMaterialUsage Usage) const
{
	TMicRecursionGuard RecursionGuard;
	UMaterial const* Material = GetMaterial_Concurrent(RecursionGuard);
	if(Material)
	{
		bool bUsageSetSuccessfully = false;
		if (Material->NeedsSetMaterialUsage_Concurrent(bUsageSetSuccessfully, Usage))
		{
			if (IsInGameThread())
			{
				bUsageSetSuccessfully = const_cast<UMaterialInstance*>(this)->CheckMaterialUsage(Usage);
			}
			else
			{
				struct FCallSMU
				{
					UMaterialInstance* Material;
					EMaterialUsage Usage;

					FCallSMU(UMaterialInstance* InMaterial, EMaterialUsage InUsage)
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

				TSharedRef<FCallSMU, ESPMode::ThreadSafe> CallSMU = MakeShareable(new FCallSMU(const_cast<UMaterialInstance*>(this), Usage));
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
	else
	{
		return false;
	}
}

bool UMaterialInstance::IsDependent(UMaterialInterface* TestDependency)
{
	if(TestDependency == this)
	{
		return true;
	}
	else if(Parent)
	{
		if(ReentrantFlag)
		{
			return true;
		}

		FMICReentranceGuard	Guard(this);
		return Parent->IsDependent(TestDependency);
	}
	else
	{
		return false;
	}
}

void UMaterialInstanceDynamic::CopyScalarAndVectorParameters(const UMaterialInterface& SourceMaterialToCopyFrom, ERHIFeatureLevel::Type FeatureLevel)
{
	check(IsInGameThread());

	// We get the parameter list form the input material, this might be different from the base material
	// because static (bool) parameters can cause some parameters to be hidden
	FMaterialResource* MaterialResource = GetMaterialResource(FeatureLevel);

	if(MaterialResource)
	{
		UMaterial* BaseMaterial = GetMaterial();

		// first, clear out all the parameter values
		ClearParameterValuesInternal(false);

		// scalar
		{
			const TArray<TRefCountPtr<FMaterialUniformExpression> >& Array = MaterialResource->GetUniformScalarParameterExpressions();

			for (int32 i = 0, Count = Array.Num(); i < Count; ++i)
			{
				const FMaterialUniformExpression* UniformExpression = Array[i];

				// the array can have non scalar parameters in it, those we don't want to interpolate
				if (UniformExpression->GetType() == &FMaterialUniformExpressionScalarParameter::StaticType)
				{
					const FMaterialUniformExpressionScalarParameter* ScalarExpression = static_cast<const FMaterialUniformExpressionScalarParameter*>(UniformExpression);

					float Value;
					ScalarExpression->GetGameThreadNumberValue(&SourceMaterialToCopyFrom, Value);

					FName ParameterName = ScalarExpression->GetParameterName();

					FScalarParameterValue* ParameterValue = GameThread_FindParameterByName(ScalarParameterValues, ParameterName);

					if (!ParameterValue)
					{
						ParameterValue = new(ScalarParameterValues)FScalarParameterValue;
						ParameterValue->ParameterName = ParameterName;
					}

					ParameterValue->ParameterValue = Value;
				}
			}
		}

		// vector
		{
			const TArray<TRefCountPtr<FMaterialUniformExpression> >& Array = MaterialResource->GetUniformVectorParameterExpressions();

			for (int32 i = 0, Count = Array.Num(); i < Count; ++i)
			{
				const FMaterialUniformExpression* UniformExpression = Array[i];

				// the array can have non vector parameters in it, those we don't want to inetrpolate
				if (UniformExpression->GetType() == &FMaterialUniformExpressionVectorParameter::StaticType)
				{
					const FMaterialUniformExpressionVectorParameter* VectorExpression = static_cast<const FMaterialUniformExpressionVectorParameter*>(UniformExpression);

					FLinearColor Value;
					VectorExpression->GetGameThreadNumberValue(&SourceMaterialToCopyFrom, Value);

					FName ParameterName = VectorExpression->GetParameterName();

					FVectorParameterValue* ParameterValue = GameThread_FindParameterByName(VectorParameterValues, ParameterName);

					if (!ParameterValue)
					{
						ParameterValue = new(VectorParameterValues)FVectorParameterValue;
						ParameterValue->ParameterName = ParameterName;
					}

					ParameterValue->ParameterValue = Value;
				}
			}
		}

		// now, init the resources
		InitResources();
	}
}

float UMaterialInstanceDynamic::GetOpacityMaskClipValue() const
{
	return Parent ? Parent->GetOpacityMaskClipValue() : 0.0f;
}

bool UMaterialInstanceDynamic::GetCastDynamicShadowAsMasked() const
{
	return Parent ? Parent->GetCastDynamicShadowAsMasked() : false;
}

EBlendMode UMaterialInstanceDynamic::GetBlendMode() const
{
	return Parent ? Parent->GetBlendMode() : BLEND_Opaque;
}

bool UMaterialInstanceDynamic::IsTwoSided() const
{
	return Parent ? Parent->IsTwoSided() : false;
}

bool UMaterialInstanceDynamic::IsDitheredLODTransition() const
{
	return Parent ? Parent->IsDitheredLODTransition() : false;
}

bool UMaterialInstanceDynamic::IsMasked() const
{
	return Parent ? Parent->IsMasked() : false;
}

EMaterialShadingModel UMaterialInstanceDynamic::GetShadingModel() const
{
	return Parent ? Parent->GetShadingModel() : MSM_DefaultLit;
}

void UMaterialInstance::CopyMaterialInstanceParameters(UMaterialInterface* Source)
{
	if(Source)
	{
		UMaterial* Material = GetMaterial();

		//first, clear out all the parameter values
		ClearParameterValuesInternal();

		//setup some arrays to use
		TArray<FName> Names;
		TArray<FGuid> Guids;

		//handle all the fonts
		Material->GetAllFontParameterNames(Names, Guids);
		for(int32 i = 0, Size = Names.Num(); i < Size; ++i)
		{
			FName ParameterName = Names[i];
			UFont* FontValue = NULL;
			int32 FontPage;
			if(Source->GetFontParameterValue(ParameterName, FontValue, FontPage))
			{
				FFontParameterValue* ParameterValue = new(FontParameterValues) FFontParameterValue;
				ParameterValue->ParameterName = ParameterName;
				ParameterValue->ExpressionGUID.Invalidate();
				ParameterValue->FontValue = FontValue;
				ParameterValue->FontPage = FontPage;
			}
		}


		//now do the scalar params
		Names.Reset();
		Guids.Reset();
		Material->GetAllScalarParameterNames(Names, Guids);
		for(int32 i = 0, Size = Names.Num(); i < Size; ++i)
		{
			FName ParameterName = Names[i];
			float ScalarValue = 1.0f;
			if(Source->GetScalarParameterValue(ParameterName, ScalarValue))
			{
				FScalarParameterValue* ParameterValue = new(ScalarParameterValues) FScalarParameterValue;
				ParameterValue->ParameterName = ParameterName;
				ParameterValue->ExpressionGUID.Invalidate();
				ParameterValue->ParameterValue = ScalarValue;
			}
		}

		//now do the vector params
		Names.Reset();
		Guids.Reset();
		Material->GetAllVectorParameterNames(Names, Guids);
		for(int32 i = 0, Size = Names.Num(); i < Size; ++i)
		{
			FName ParameterName = Names[i];
			FLinearColor VectorValue;
			if(Source->GetVectorParameterValue(ParameterName, VectorValue))
			{
				FVectorParameterValue* ParameterValue = new(VectorParameterValues) FVectorParameterValue;
				ParameterValue->ParameterName = ParameterName;
				ParameterValue->ExpressionGUID.Invalidate();
				ParameterValue->ParameterValue = VectorValue;
			}
		}

		//now do the texture params
		Names.Reset();
		Guids.Reset();
		Material->GetAllTextureParameterNames(Names, Guids);
		for(int32 i = 0, Size = Names.Num(); i < Size; ++i)
		{
			FName ParameterName = Names[i];
			UTexture* TextureValue = NULL;
			if(Source->GetTextureParameterValue(ParameterName, TextureValue))
			{
				FTextureParameterValue* ParameterValue = new(TextureParameterValues) FTextureParameterValue;
				ParameterValue->ParameterName = ParameterName;
				ParameterValue->ExpressionGUID.Invalidate();
				ParameterValue->ParameterValue = TextureValue;
			}
		}

		//now, init the resources
		InitResources();
	}
}

FMaterialResource* UMaterialInstance::GetMaterialResource(ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type QualityLevel)
{
	check(!IsInActualRenderingThread());

	if (QualityLevel == EMaterialQualityLevel::Num)
	{
		QualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
	}

	if (bHasStaticPermutationResource)
	{
		//if there is a static permutation resource, use that
		return StaticPermutationMaterialResources[QualityLevel][InFeatureLevel];
	}

	//there was no static permutation resource
	return Parent ? Parent->GetMaterialResource(InFeatureLevel,QualityLevel) : NULL;
}

const FMaterialResource* UMaterialInstance::GetMaterialResource(ERHIFeatureLevel::Type InFeatureLevel, EMaterialQualityLevel::Type QualityLevel) const
{
	//check(!IsInActualRenderingThread());

	if (QualityLevel == EMaterialQualityLevel::Num)
	{
		QualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
	}

	if (bHasStaticPermutationResource)
	{
		//if there is a static permutation resource, use that
		return StaticPermutationMaterialResources[QualityLevel][InFeatureLevel];
	}

	//there was no static permutation resource
	return Parent ? Parent->GetMaterialResource(InFeatureLevel,QualityLevel) : NULL;
}

FMaterialRenderProxy* UMaterialInstance::GetRenderProxy(bool Selected, bool bHovered) const
{
	check(!( Selected || bHovered ) || GIsEditor);
	return Resources[Selected ? 1 : ( bHovered ? 2 : 0 )];
}

UPhysicalMaterial* UMaterialInstance::GetPhysicalMaterial() const
{
	if(ReentrantFlag)
	{
		return UMaterial::GetDefaultMaterial(MD_Surface)->GetPhysicalMaterial();
	}

	FMICReentranceGuard	Guard(const_cast<UMaterialInstance*>(this));  // should not need this to determine loop
	if(PhysMaterial)
	{
		return PhysMaterial;
	}
	else if(Parent)
	{
		// If no physical material has been associated with this instance, simply use the parent's physical material.
		return Parent->GetPhysicalMaterial();
	}
	else
	{
		// no material specified and no parent, fall back to default physical material
		check( GEngine->DefaultPhysMaterial != NULL );
		return GEngine->DefaultPhysMaterial;
	}
}

void UMaterialInstance::GetStaticParameterValues(FStaticParameterSet& OutStaticParameters)
{
	check(IsInGameThread());

	if (Parent)
	{
		UMaterial* ParentMaterial = Parent->GetMaterial();
		TArray<FName> ParameterNames;
		TArray<FGuid> Guids;

		// Static Switch Parameters
		ParentMaterial->GetAllStaticSwitchParameterNames(ParameterNames, Guids);
		OutStaticParameters.StaticSwitchParameters.AddZeroed(ParameterNames.Num());

		for(int32 ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{
			FStaticSwitchParameter& ParentParameter = OutStaticParameters.StaticSwitchParameters[ParameterIdx];
			FName ParameterName = ParameterNames[ParameterIdx];
			bool Value = false;
			FGuid ExpressionId = Guids[ParameterIdx];

			ParentParameter.bOverride = false;
			ParentParameter.ParameterName = ParameterName;

			//get the settings from the parent in the MIC chain
			if(Parent->GetStaticSwitchParameterValue(ParameterName, Value, ExpressionId))
			{
				ParentParameter.Value = Value;
			}
			ParentParameter.ExpressionGUID = ExpressionId;

			//if the SourceInstance is overriding this parameter, use its settings
			for(int32 SwitchParamIdx = 0; SwitchParamIdx < StaticParameters.StaticSwitchParameters.Num(); SwitchParamIdx++)
			{
				const FStaticSwitchParameter& StaticSwitchParam = StaticParameters.StaticSwitchParameters[SwitchParamIdx];

				if(ParameterName == StaticSwitchParam.ParameterName)
				{
					ParentParameter.bOverride = StaticSwitchParam.bOverride;
					if (StaticSwitchParam.bOverride)
					{
						ParentParameter.Value = StaticSwitchParam.Value;
					}
				}
			}
		}

		// Static Component Mask Parameters
		ParentMaterial->GetAllStaticComponentMaskParameterNames(ParameterNames, Guids);
		OutStaticParameters.StaticComponentMaskParameters.AddZeroed(ParameterNames.Num());
		for(int32 ParameterIdx=0; ParameterIdx<ParameterNames.Num(); ParameterIdx++)
		{
			FStaticComponentMaskParameter& ParentParameter = OutStaticParameters.StaticComponentMaskParameters[ParameterIdx];
			FName ParameterName = ParameterNames[ParameterIdx];
			bool R = false;
			bool G = false;
			bool B = false;
			bool A = false;
			FGuid ExpressionId = Guids[ParameterIdx];

			ParentParameter.bOverride = false;
			ParentParameter.ParameterName = ParameterName;

			//get the settings from the parent in the MIC chain
			if(Parent->GetStaticComponentMaskParameterValue(ParameterName, R, G, B, A, ExpressionId))
			{
				ParentParameter.R = R;
				ParentParameter.G = G;
				ParentParameter.B = B;
				ParentParameter.A = A;
			}
			ParentParameter.ExpressionGUID = ExpressionId;

			//if the SourceInstance is overriding this parameter, use its settings
			for(int32 MaskParamIdx = 0; MaskParamIdx < StaticParameters.StaticComponentMaskParameters.Num(); MaskParamIdx++)
			{
				const FStaticComponentMaskParameter &StaticComponentMaskParam = StaticParameters.StaticComponentMaskParameters[MaskParamIdx];

				if(ParameterName == StaticComponentMaskParam.ParameterName)
				{
					ParentParameter.bOverride = StaticComponentMaskParam.bOverride;
					if (StaticComponentMaskParam.bOverride)
					{
						ParentParameter.R = StaticComponentMaskParam.R;
						ParentParameter.G = StaticComponentMaskParam.G;
						ParentParameter.B = StaticComponentMaskParam.B;
						ParentParameter.A = StaticComponentMaskParam.A;
					}
				}
			}
		}
	}

	// Custom parameters.
	CustomStaticParametersGetters.Broadcast(OutStaticParameters, this);
}

void UMaterialInstance::ForceRecompileForRendering()
{
	CacheResourceShadersForRendering();
}

void UMaterialInstance::InitStaticPermutation()
{
	UpdateOverridableBaseProperties();

	// Update bHasStaticPermutationResource in case the parent was not found
	bHasStaticPermutationResource = (!StaticParameters.IsEmpty() || HasOverridenBaseProperties()) && Parent;

	// Allocate material resources if needed even if we are cooking, so that StaticPermutationMaterialResources will always be valid
	UpdatePermutationAllocations();

	if ( FApp::CanEverRender() ) 
	{
		// Cache shaders for the current platform to be used for rendering
		CacheResourceShadersForRendering();
	}
}

void UMaterialInstance::UpdateOverridableBaseProperties()
{
	//Parents base property overrides have to be cashed by now.
	//This should be done on PostLoad()
	//Or via an FMaterialUpdateContext when editing.

	if (!Parent)
	{
		OpacityMaskClipValue = 0.0f;
		BlendMode = BLEND_Opaque;
		ShadingModel = MSM_DefaultLit;
		TwoSided = 0;
		DitheredLODTransition = 0;
		// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
		VxgiMaterialProperties = FVxgiMaterialProperties();
#endif
		// NVCHANGE_END: Add VXGI
		return;
	}

	if (BasePropertyOverrides.bOverride_OpacityMaskClipValue)
	{
		OpacityMaskClipValue = BasePropertyOverrides.OpacityMaskClipValue;
	}
	else
	{
		OpacityMaskClipValue = Parent->GetOpacityMaskClipValue();
	}

	if ( BasePropertyOverrides.bOverride_CastDynamicShadowAsMasked )
	{
		bCastDynamicShadowAsMasked = BasePropertyOverrides.bCastDynamicShadowAsMasked;
	}
	else
	{
		bCastDynamicShadowAsMasked = Parent->GetCastDynamicShadowAsMasked();
	}

	if (BasePropertyOverrides.bOverride_BlendMode)
	{
		BlendMode = BasePropertyOverrides.BlendMode;
	}
	else
	{
		BlendMode = Parent->GetBlendMode();
	}

	if (BasePropertyOverrides.bOverride_ShadingModel)
	{
		ShadingModel = BasePropertyOverrides.ShadingModel;
	}
	else
	{
		ShadingModel = Parent->GetShadingModel();
	}

	if (BasePropertyOverrides.bOverride_TwoSided)
	{
		TwoSided = BasePropertyOverrides.TwoSided != 0;
	}
	else
	{
		TwoSided = Parent->IsTwoSided();
	}

	if (BasePropertyOverrides.bOverride_DitheredLODTransition)
	{
		DitheredLODTransition = BasePropertyOverrides.DitheredLODTransition != 0;
	}
	else
	{
		DitheredLODTransition = Parent->IsDitheredLODTransition();
	}
	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	VxgiMaterialProperties = Parent->GetVxgiMaterialProperties();

	if (BasePropertyOverrides.bOverride_VxgiConeTracingEnabled)
	{
		VxgiMaterialProperties.bVxgiConeTracingEnabled = BasePropertyOverrides.bVxgiConeTracingEnabled;
	}

	if (BasePropertyOverrides.bOverride_UsedWithVxgiVoxelization)
	{
		VxgiMaterialProperties.bUsedWithVxgiVoxelization = BasePropertyOverrides.bUsedWithVxgiVoxelization;
	}

	if (BasePropertyOverrides.bOverride_VxgiAllowTesselationDuringVoxelization)
	{
		VxgiMaterialProperties.bVxgiAllowTesselationDuringVoxelization = BasePropertyOverrides.bVxgiAllowTesselationDuringVoxelization;
	}

	if (BasePropertyOverrides.bOverride_VxgiOmniDirectional)
	{
		VxgiMaterialProperties.bVxgiOmniDirectional = BasePropertyOverrides.bVxgiOmniDirectional;
	}

	if (BasePropertyOverrides.bOverride_VxgiProportionalEmittance)
	{
		VxgiMaterialProperties.bVxgiProportionalEmittance = BasePropertyOverrides.bVxgiProportionalEmittance;
	}

	if (BasePropertyOverrides.bOverride_VxgiCoverageSupersampling)
	{
		VxgiMaterialProperties.bVxgiCoverageSupersampling = BasePropertyOverrides.bVxgiCoverageSupersampling;
	}

	if (BasePropertyOverrides.bOverride_VxgiMaterialSamplingRate)
	{
		VxgiMaterialProperties.VxgiMaterialSamplingRate = BasePropertyOverrides.VxgiMaterialSamplingRate;
	}

	if (BasePropertyOverrides.bOverride_VxgiOpacityNoiseScaleBias)
	{
		VxgiMaterialProperties.VxgiOpacityNoiseScaleBias = BasePropertyOverrides.VxgiOpacityNoiseScaleBias;
	}

	if (BasePropertyOverrides.bOverride_VxgiVoxelizationThickness)
	{
		VxgiMaterialProperties.VxgiVoxelizationThickness = BasePropertyOverrides.VxgiVoxelizationThickness;
	}
#endif
	// NVCHANGE_END: Add VXGI
}

void UMaterialInstance::GetAllShaderMaps(TArray<FMaterialShaderMap*>& OutShaderMaps)
{
	for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
	{
		for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
		{
			FMaterialResource* CurrentResource = StaticPermutationMaterialResources[QualityLevelIndex][FeatureLevelIndex];
			FMaterialShaderMap* ShaderMap = CurrentResource->GetGameThreadShaderMap();
			OutShaderMaps.Add(ShaderMap);
		}
	}
}

FMaterialResource* UMaterialInstance::AllocatePermutationResource()
{
	return new FMaterialResource();
}

void UMaterialInstance::UpdatePermutationAllocations()
{
	if (bHasStaticPermutationResource)
	{
		UMaterial* BaseMaterial = GetMaterial();

		for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
		{
			EShaderPlatform ShaderPlatform = GShaderPlatformForFeatureLevel[FeatureLevelIndex];
			TArray<bool, TInlineAllocator<EMaterialQualityLevel::Num> > QualityLevelsUsed;
			BaseMaterial->GetQualityLevelUsage(QualityLevelsUsed, ShaderPlatform);

			for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
			{
				FMaterialResource*& CurrentResource = StaticPermutationMaterialResources[QualityLevelIndex][FeatureLevelIndex];

				if (!CurrentResource)
				{
					CurrentResource = AllocatePermutationResource();
				}

				const bool bQualityLevelHasDifferentNodes = QualityLevelsUsed[QualityLevelIndex];
				CurrentResource->SetMaterial(BaseMaterial, (EMaterialQualityLevel::Type)QualityLevelIndex, bQualityLevelHasDifferentNodes, (ERHIFeatureLevel::Type)FeatureLevelIndex, this);
			}
		}
	}
}

void UMaterialInstance::CacheResourceShadersForRendering()
{
	check(IsInGameThread() || IsAsyncLoading());

	UpdatePermutationAllocations();
	UpdateOverridableBaseProperties();

	if (bHasStaticPermutationResource && FApp::CanEverRender())
	{
		check(IsA(UMaterialInstanceConstant::StaticClass()));

		uint32 FeatureLevelsToCompile = GetFeatureLevelsToCompileForRendering();
		EMaterialQualityLevel::Type ActiveQualityLevel = GetCachedScalabilityCVars().MaterialQualityLevel;
		TArray<FMaterialResource*> ResourcesToCache;

		while (FeatureLevelsToCompile != 0)
		{
			ERHIFeatureLevel::Type FeatureLevel = (ERHIFeatureLevel::Type)FBitSet::GetAndClearNextBit(FeatureLevelsToCompile); 
			EShaderPlatform ShaderPlatform = GShaderPlatformForFeatureLevel[FeatureLevel];

			// Only cache shaders for the quality level that will actually be used to render
			ResourcesToCache.Reset();
			ResourcesToCache.Add(StaticPermutationMaterialResources[ActiveQualityLevel][FeatureLevel]);
			CacheShadersForResources(ShaderPlatform, ResourcesToCache, true);
		}
	}

	InitResources();
}

void UMaterialInstance::CacheResourceShadersForCooking(EShaderPlatform ShaderPlatform, TArray<FMaterialResource*>& OutCachedMaterialResources)
{
	if (bHasStaticPermutationResource)
	{
		UMaterial* BaseMaterial = GetMaterial();

		TArray<bool, TInlineAllocator<EMaterialQualityLevel::Num> > QualityLevelsUsed;
		BaseMaterial->GetQualityLevelUsage(QualityLevelsUsed, ShaderPlatform);

		TArray<FMaterialResource*> ResourcesToCache;
		ERHIFeatureLevel::Type TargetFeatureLevel = GetMaxSupportedFeatureLevel(ShaderPlatform);

		bool bAnyQualityLevelUsed = false;

		for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
		{
			bAnyQualityLevelUsed = bAnyQualityLevelUsed || QualityLevelsUsed[QualityLevelIndex];
		}

		for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
		{
			// Cache all quality levels, unless they are all the same (due to using the same nodes), then just cache the high quality
			if (bAnyQualityLevelUsed || QualityLevelIndex == EMaterialQualityLevel::High)
			{
				FMaterialResource* NewResource = AllocatePermutationResource();
				NewResource->SetMaterial(BaseMaterial, (EMaterialQualityLevel::Type)QualityLevelIndex, QualityLevelsUsed[QualityLevelIndex], (ERHIFeatureLevel::Type)TargetFeatureLevel, this);
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
}

void UMaterialInstance::CacheShadersForResources(EShaderPlatform ShaderPlatform, const TArray<FMaterialResource*>& ResourcesToCache, bool bApplyCompletedShaderMapForRendering)
{
	UMaterial* BaseMaterial = GetMaterial();

	BaseMaterial->CacheExpressionTextureReferences();

	for (int32 ResourceIndex = 0; ResourceIndex < ResourcesToCache.Num(); ResourceIndex++)
	{
		FMaterialResource* CurrentResource = ResourcesToCache[ResourceIndex];

		FMaterialShaderMapId ShaderMapId;
		CurrentResource->GetShaderMapId(ShaderPlatform, ShaderMapId);

		const bool bSuccess = CurrentResource->CacheShaders(ShaderMapId, ShaderPlatform, bApplyCompletedShaderMapForRendering);

		if (!bSuccess)
		{
			UE_ASSET_LOG(LogMaterial, Warning, this,
				TEXT("Failed to compile Material Instance with Base %s for platform %s, Default Material will be used in game."), 
				BaseMaterial ? *BaseMaterial->GetName() : TEXT("Null"), 
				*LegacyShaderPlatformToShaderFormat(ShaderPlatform).ToString()
				);

			const TArray<FString>& CompileErrors = CurrentResource->GetCompileErrors();
			for (int32 ErrorIndex = 0; ErrorIndex < CompileErrors.Num(); ErrorIndex++)
			{
				UE_LOG(LogMaterial, Log, TEXT("	%s"), *CompileErrors[ErrorIndex]);
			}
		}
	}
}

bool UMaterialInstance::GetStaticSwitchParameterValue(FName ParameterName, bool &OutValue,FGuid &OutExpressionGuid) const
{
	if(ReentrantFlag)
	{
		return false;
	}

	for (int32 ValueIndex = 0;ValueIndex < StaticParameters.StaticSwitchParameters.Num();ValueIndex++)
	{
		const FStaticSwitchParameter& Param = StaticParameters.StaticSwitchParameters[ValueIndex];
		if (Param.bOverride && Param.ParameterName == ParameterName)
		{
			OutValue = Param.Value;
			OutExpressionGuid = Param.ExpressionGUID;
			return true;
		}
	}

	if(Parent)
	{
		FMICReentranceGuard	Guard(this);
		return Parent->GetStaticSwitchParameterValue(ParameterName,OutValue,OutExpressionGuid);
	}
	else
	{
		return false;
	}
}

bool UMaterialInstance::GetStaticComponentMaskParameterValue(FName ParameterName, bool &OutR, bool &OutG, bool &OutB, bool &OutA, FGuid &OutExpressionGuid) const
{
	if(ReentrantFlag)
	{
		return false;
	}

	for (int32 ValueIndex = 0;ValueIndex < StaticParameters.StaticComponentMaskParameters.Num();ValueIndex++)
	{
		const FStaticComponentMaskParameter& Param = StaticParameters.StaticComponentMaskParameters[ValueIndex];
		if (Param.bOverride && Param.ParameterName == ParameterName)
		{
			OutR = Param.R;
			OutG = Param.G;
			OutB = Param.B;
			OutA = Param.A;
			OutExpressionGuid = Param.ExpressionGUID;
			return true;
		}
	}

	if(Parent)
	{
		FMICReentranceGuard	Guard(this);
		return Parent->GetStaticComponentMaskParameterValue(ParameterName, OutR, OutG, OutB, OutA, OutExpressionGuid);
	}
	else
	{
		return false;
	}
}

bool UMaterialInstance::GetTerrainLayerWeightParameterValue(FName ParameterName, int32& OutWeightmapIndex, FGuid &OutExpressionGuid) const
{
	if(ReentrantFlag)
	{
		return false;
	}

	for (int32 ValueIndex = 0;ValueIndex < StaticParameters.TerrainLayerWeightParameters.Num();ValueIndex++)
	{
		const FStaticTerrainLayerWeightParameter& Param = StaticParameters.TerrainLayerWeightParameters[ValueIndex];
		if (Param.bOverride && Param.ParameterName == ParameterName)
		{
			OutWeightmapIndex = Param.WeightmapIndex;
			OutExpressionGuid = Param.ExpressionGUID;
			return true;
		}
	}

	if(Parent)
	{
		FMICReentranceGuard	Guard(this);
		return Parent->GetTerrainLayerWeightParameterValue(ParameterName, OutWeightmapIndex, OutExpressionGuid);
	}
	else
	{
		return false;
	}
}

template <typename ParameterType>
void TrimToOverriddenOnly(TArray<ParameterType>& Parameters)
{
	for (int32 ParameterIndex = Parameters.Num() - 1; ParameterIndex >= 0; ParameterIndex--)
	{
		if (!Parameters[ParameterIndex].bOverride)
		{
			Parameters.RemoveAt(ParameterIndex);
		}
	}
}

#if WITH_EDITOR

void UMaterialInstance::BeginCacheForCookedPlatformData( const ITargetPlatform *TargetPlatform )
{
	TArray<FMaterialResource*> *CachedMaterialResourcesForPlatform = CachedMaterialResourcesForCooking.Find( TargetPlatform );

	if ( CachedMaterialResourcesForPlatform == NULL )
	{
		check( CachedMaterialResourcesForPlatform == NULL );

		CachedMaterialResourcesForCooking.Add( TargetPlatform );
		CachedMaterialResourcesForPlatform = CachedMaterialResourcesForCooking.Find( TargetPlatform );

		check( CachedMaterialResourcesForPlatform != NULL );

		TArray<FName> DesiredShaderFormats;
		TargetPlatform->GetAllTargetedShaderFormats(DesiredShaderFormats);

		// Cache shaders for each shader format, storing the results in CachedMaterialResourcesForCooking so they will be available during saving
		for (int32 FormatIndex = 0; FormatIndex < DesiredShaderFormats.Num(); FormatIndex++)
		{
			const EShaderPlatform TargetShaderPlatform = ShaderFormatToLegacyShaderPlatform(DesiredShaderFormats[FormatIndex]);

			CacheResourceShadersForCooking(TargetShaderPlatform, *CachedMaterialResourcesForPlatform );
		}
	}
}

bool UMaterialInstance::IsCachedCookedPlatformDataLoaded( const ITargetPlatform* TargetPlatform ) 
{
	const TArray<FMaterialResource*> *CachedMaterialResourcesForPlatform = CachedMaterialResourcesForCooking.Find( TargetPlatform );
	if ( CachedMaterialResourcesForPlatform != NULL )
	{
		for ( const auto& MaterialResource : *CachedMaterialResourcesForPlatform )
		{
			if ( MaterialResource->IsCompilationFinished() == false )
				return false;
		}

		return true;
	}
	return false; // this happens if we haven't started caching (begincache hasn't been called yet)
}


void UMaterialInstance::ClearCachedCookedPlatformData( const ITargetPlatform *TargetPlatform )
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


void UMaterialInstance::ClearAllCachedCookedPlatformData()
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

void UMaterialInstance::Serialize(FArchive& Ar)
{
	LLM_SCOPE(ELLMTag::Materials);
	SCOPED_LOADTIMER(MaterialInstanceSerializeTime);
	Super::Serialize(Ar);

	// Only serialize the static permutation resource if one exists
	if (bHasStaticPermutationResource)
	{
		if (Ar.UE4Ver() >= VER_UE4_PURGED_FMATERIAL_COMPILE_OUTPUTS)
		{
			StaticParameters.Serialize(Ar);
#if WITH_EDITOR
			SerializeInlineShaderMaps(&CachedMaterialResourcesForCooking, Ar, LoadedMaterialResources);
#else
			SerializeInlineShaderMaps(NULL, Ar, LoadedMaterialResources);
#endif

		}
		else
		{
			FMaterialResource LegacyResource;
			LegacyResource.LegacySerialize(Ar);

			FMaterialShaderMapId LegacyId;
			LegacyId.Serialize(Ar);

			StaticParameters.StaticSwitchParameters = LegacyId.ParameterSet.StaticSwitchParameters;
			StaticParameters.StaticComponentMaskParameters = LegacyId.ParameterSet.StaticComponentMaskParameters;
			StaticParameters.TerrainLayerWeightParameters = LegacyId.ParameterSet.TerrainLayerWeightParameters;

			TrimToOverriddenOnly(StaticParameters.StaticSwitchParameters);
			TrimToOverriddenOnly(StaticParameters.StaticComponentMaskParameters);
			TrimToOverriddenOnly(StaticParameters.TerrainLayerWeightParameters);
		}
	}

	if (Ar.UE4Ver() >= VER_UE4_MATERIAL_INSTANCE_BASE_PROPERTY_OVERRIDES )
	{
		if( Ar.UE4Ver() < VER_UE4_FIX_MATERIAL_PROPERTY_OVERRIDE_SERIALIZE )
		{
			// awful old native serialize of FMaterialInstanceBasePropertyOverrides UStruct
			Ar << bOverrideBaseProperties_DEPRECATED;
			bool bHasPropertyOverrides = false;
			Ar << bHasPropertyOverrides;
			if( bHasPropertyOverrides )
			{
				Ar << BasePropertyOverrides.bOverride_OpacityMaskClipValue << BasePropertyOverrides.OpacityMaskClipValue;

				if( Ar.UE4Ver() >= VER_UE4_MATERIAL_INSTANCE_BASE_PROPERTY_OVERRIDES_PHASE_2 )
				{
					Ar	<< BasePropertyOverrides.bOverride_BlendMode << BasePropertyOverrides.BlendMode
						<< BasePropertyOverrides.bOverride_ShadingModel << BasePropertyOverrides.ShadingModel
						<< BasePropertyOverrides.bOverride_TwoSided;

					bool bTwoSided;
					Ar << bTwoSided;
					BasePropertyOverrides.TwoSided = bTwoSided;

					if( Ar.UE4Ver() >= VER_UE4_MATERIAL_INSTANCE_BASE_PROPERTY_OVERRIDES_DITHERED_LOD_TRANSITION )
					{
						Ar	<< BasePropertyOverrides.bOverride_DitheredLODTransition;

						bool bDitheredLODTransition;
						Ar << bDitheredLODTransition;
						BasePropertyOverrides.DitheredLODTransition = bDitheredLODTransition;
					}
					// unrelated but closest change to bug
					if( Ar.UE4Ver() < VER_UE4_STATIC_SHADOW_DEPTH_MAPS )
					{
						// switched enum order
						switch( BasePropertyOverrides.ShadingModel )
						{
							case MSM_Unlit:			BasePropertyOverrides.ShadingModel = MSM_DefaultLit; break;
							case MSM_DefaultLit:	BasePropertyOverrides.ShadingModel = MSM_Unlit; break;
						}
					}
				}
			}
		}
	}
}

void UMaterialInstance::PostLoad()
{
	SCOPED_LOADTIMER(MaterialInstancePostLoad);
	Super::PostLoad();

	if (FApp::CanEverRender())
	{
	// Resources can be processed / registered now that we're back on the main thread
	ProcessSerializedInlineShaderMaps(this, LoadedMaterialResources, StaticPermutationMaterialResources);
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

	AssertDefaultMaterialsPostLoaded();

	// Ensure that the instance's parent is PostLoaded before the instance.
	if (Parent)
	{
		if (GEventDrivenLoaderEnabled && EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME)
		{
			check(!Parent->HasAnyFlags(RF_NeedLoad));
		}
		Parent->ConditionalPostLoad();
	}

	// Add references to the expression object if we do not have one already, and fix up any names that were changed.
	UpdateParameters();

	// We have to make sure the resources are created for all used textures.
	for( int32 ValueIndex=0; ValueIndex<TextureParameterValues.Num(); ValueIndex++ )
	{
		// Make sure the texture is postloaded so the resource isn't null.
		UTexture* Texture = TextureParameterValues[ValueIndex].ParameterValue;
		if( Texture )
		{
			Texture->ConditionalPostLoad();
		}
	}

	// do the same for font textures
	for( int32 ValueIndex=0; ValueIndex < FontParameterValues.Num(); ValueIndex++ )
	{
		// Make sure the font is postloaded so the resource isn't null.
		UFont* Font = FontParameterValues[ValueIndex].FontValue;
		if( Font )
		{
			Font->ConditionalPostLoad();
		}
	}

	// called before we cache the uniform expression as a call to SubsurfaceProfileRT affects the dta in there
	PropagateDataToMaterialProxy();

	STAT(double MaterialLoadTime = 0);
	{
		SCOPE_SECONDS_COUNTER(MaterialLoadTime);

		// Make sure static parameters are up to date and shaders are cached for the current platform
		InitStaticPermutation();
#if WITH_EDITOR
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
#endif
	}

	INC_FLOAT_STAT_BY(STAT_ShaderCompiling_MaterialLoading,(float)MaterialLoadTime);

	if (GIsEditor && GEngine != NULL && !IsTemplate() && Parent)
	{
		// Ensure that the ReferencedTextureGuids array is up to date.
		UpdateLightmassTextureTracking();
	}

	// Fixup for legacy instances which didn't recreate the lighting guid properly on duplication
	if (GetLinker() && GetLinker()->UE4Ver() < VER_UE4_BUMPED_MATERIAL_EXPORT_GUIDS)
	{
		extern TMap<FGuid, UMaterialInterface*> LightingGuidFixupMap;
		UMaterialInterface** ExistingMaterial = LightingGuidFixupMap.Find(GetLightingGuid());

		if (ExistingMaterial)
		{
			SetLightingGuid();
		}

		LightingGuidFixupMap.Add(GetLightingGuid(), this);
	}
}

void UMaterialInstance::BeginDestroy()
{
	Super::BeginDestroy();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		BeginReleaseResource(Resources[0]);

		if(GIsEditor)
		{
			BeginReleaseResource(Resources[1]);
			BeginReleaseResource(Resources[2]);
		}
	}

	ReleaseFence.BeginFence();
}

bool UMaterialInstance::IsReadyForFinishDestroy()
{
	bool bIsReady = Super::IsReadyForFinishDestroy();

	return bIsReady && ReleaseFence.IsFenceComplete();;
}

void UMaterialInstance::FinishDestroy()
{
	if(!HasAnyFlags(RF_ClassDefaultObject))
	{
		Resources[0]->GameThread_Destroy();
		Resources[0] = NULL;

		if(GIsEditor)
		{
			Resources[1]->GameThread_Destroy();
			Resources[1] = NULL;
			Resources[2]->GameThread_Destroy();
			Resources[2] = NULL;
		}
	}

	for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
	{
		for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
		{
			FMaterialResource*& CurrentResource = StaticPermutationMaterialResources[QualityLevelIndex][FeatureLevelIndex];
			delete CurrentResource;
			CurrentResource = NULL;
		}
	}
#if WITH_EDITOR
	ClearAllCachedCookedPlatformData();
#endif
	Super::FinishDestroy();
}

void UMaterialInstance::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UMaterialInstance* This = CastChecked<UMaterialInstance>(InThis);

	if (This->bHasStaticPermutationResource)
	{
		for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
		{
			for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
			{
				FMaterialResource* CurrentResource = This->StaticPermutationMaterialResources[QualityLevelIndex][FeatureLevelIndex];
				if (CurrentResource)
				{
					CurrentResource->AddReferencedObjects(Collector);
				}
			}
		}
	}

	Super::AddReferencedObjects(This, Collector);
}

void UMaterialInstance::SetParentInternal(UMaterialInterface* NewParent, bool RecacheShaders)
{
	if (!Parent || Parent != NewParent)
	{
		// Check if the new parent is already an existing child
		UMaterialInstance* ParentAsMaterialInstance = Cast<UMaterialInstance>(NewParent);
		bool bSetParent = false;

		if (ParentAsMaterialInstance != nullptr && ParentAsMaterialInstance->IsChildOf(this))
		{
			UE_LOG(LogMaterial, Warning, TEXT("%s is not a valid parent for %s as it is already a child of this material instance."),
				   *NewParent->GetFullName(),
				   *GetFullName());
		}
		else if (NewParent &&
				 !NewParent->IsA(UMaterial::StaticClass()) &&
				 !NewParent->IsA(UMaterialInstanceConstant::StaticClass()))
		{
			UE_LOG(LogMaterial, Warning, TEXT("%s is not a valid parent for %s. Only Materials and MaterialInstanceConstants are valid parents for a material instance."),
				*NewParent->GetFullName(),
				*GetFullName());
		}
		else
		{
			Parent = NewParent;
			bSetParent = true;

			if( Parent )
			{
				// It is possible to set a material's parent while post-loading. In
				// such a case it is also possible that the parent has not been
				// post-loaded, so call ConditionalPostLoad() just in case.
				Parent->ConditionalPostLoad();
			}
		}

		if (bSetParent && RecacheShaders)
		{
			InitStaticPermutation();
		}
		else
		{
			InitResources();
		}
	}
}

bool UMaterialInstance::SetVectorParameterByIndexInternal(int32 ParameterIndex, FLinearColor Value)
{
	FVectorParameterValue* ParameterValue = GameThread_FindParameterByIndex(VectorParameterValues, ParameterIndex);
	if (ParameterValue == nullptr)
	{
		return false;
	}

	ParameterValue->ParameterValue = Value;
	// Update the material instance data in the rendering thread.
	GameThread_UpdateMIParameter(this, *ParameterValue);
	CacheMaterialInstanceUniformExpressions(this);

	return true;
}

void UMaterialInstance::SetVectorParameterValueInternal(FName ParameterName, FLinearColor Value)
{
	FVectorParameterValue* ParameterValue = GameThread_FindParameterByName(
		VectorParameterValues,
		ParameterName
		);

	if(!ParameterValue)
	{
		// If there's no element for the named parameter in array yet, add one.
		ParameterValue = new(VectorParameterValues) FVectorParameterValue;
		ParameterValue->ParameterName = ParameterName;
		ParameterValue->ExpressionGUID.Invalidate();
		// Force an update on first use
		ParameterValue->ParameterValue.B = Value.B - 1.f;
	}

	// Don't enqueue an update if it isn't needed
	if (ParameterValue->ParameterValue != Value)
	{
		ParameterValue->ParameterValue = Value;
		// Update the material instance data in the rendering thread.
		GameThread_UpdateMIParameter(this, *ParameterValue);
		CacheMaterialInstanceUniformExpressions(this);
	}
}

bool UMaterialInstance::SetScalarParameterByIndexInternal(int32 ParameterIndex, float Value)
{
	FScalarParameterValue* ParameterValue = GameThread_FindParameterByIndex(ScalarParameterValues, ParameterIndex);
	if (ParameterValue == nullptr)
	{
		return false;
	}

	ParameterValue->ParameterValue = Value;
	// Update the material instance data in the rendering thread.
	GameThread_UpdateMIParameter(this, *ParameterValue);
	CacheMaterialInstanceUniformExpressions(this);

	return true;
}

void UMaterialInstance::SetScalarParameterValueInternal(FName ParameterName, float Value)
{
	FScalarParameterValue* ParameterValue = GameThread_FindParameterByName(
		ScalarParameterValues,
		ParameterName
		);

	if(!ParameterValue)
	{
		// If there's no element for the named parameter in array yet, add one.
		ParameterValue = new(ScalarParameterValues) FScalarParameterValue;
		ParameterValue->ParameterName = ParameterName;
		ParameterValue->ExpressionGUID.Invalidate();
		// Force an update on first use
		ParameterValue->ParameterValue = Value - 1.f;
	}

	// Don't enqueue an update if it isn't needed
	if (ParameterValue->ParameterValue != Value)
	{
		ParameterValue->ParameterValue = Value;
		// Update the material instance data in the rendering thread.
		GameThread_UpdateMIParameter(this, *ParameterValue);
		CacheMaterialInstanceUniformExpressions(this);
	}
}

void UMaterialInstance::SetTextureParameterValueInternal(FName ParameterName, UTexture* Value)
{
	FTextureParameterValue* ParameterValue = GameThread_FindParameterByName(
		TextureParameterValues,
		ParameterName
		);

	if(!ParameterValue)
	{
		// If there's no element for the named parameter in array yet, add one.
		ParameterValue = new(TextureParameterValues) FTextureParameterValue;
		ParameterValue->ParameterName = ParameterName;
		ParameterValue->ExpressionGUID.Invalidate();
		// Force an update on first use
		ParameterValue->ParameterValue = Value == GEngine->DefaultDiffuseTexture ? NULL : GEngine->DefaultDiffuseTexture;
	}

	// Don't enqueue an update if it isn't needed
	if (ParameterValue->ParameterValue != Value)
	{
		// set as an ensure, because it is somehow possible to accidentally pass non-textures into here via blueprints...
		if (Value && ensureMsgf(Value->IsA(UTexture::StaticClass()), TEXT("Expecting a UTexture! Value='%s' class='%s'"), *Value->GetName(), *Value->GetClass()->GetName()))
		{
		ParameterValue->ParameterValue = Value;
		// Update the material instance data in the rendering thread.
		GameThread_UpdateMIParameter(this, *ParameterValue);
		CacheMaterialInstanceUniformExpressions(this);
		}		
	}
}

void UMaterialInstance::SetFontParameterValueInternal(FName ParameterName,class UFont* FontValue,int32 FontPage)
{
	FFontParameterValue* ParameterValue = GameThread_FindParameterByName(
		FontParameterValues,
		ParameterName
		);

	if(!ParameterValue)
	{
			// If there's no element for the named parameter in array yet, add one.
			ParameterValue = new(FontParameterValues) FFontParameterValue;
			ParameterValue->ParameterName = ParameterName;
			ParameterValue->ExpressionGUID.Invalidate();
			// Force an update on first use
			ParameterValue->FontValue == GEngine->GetTinyFont() ? NULL : GEngine->GetTinyFont();
			ParameterValue->FontPage = FontPage - 1;
	}

	// Don't enqueue an update if it isn't needed
	if (ParameterValue->FontValue != FontValue ||
		ParameterValue->FontPage != FontPage)
	{
		ParameterValue->FontValue = FontValue;
		ParameterValue->FontPage = FontPage;
		// Update the material instance data in the rendering thread.
		GameThread_UpdateMIParameter(this, *ParameterValue);
		CacheMaterialInstanceUniformExpressions(this);
	}
}

void UMaterialInstance::ClearParameterValuesInternal(const bool bAllParameters)
{
	VectorParameterValues.Empty();
	ScalarParameterValues.Empty();

	if(bAllParameters)
	{
		TextureParameterValues.Empty();
		FontParameterValues.Empty();
	}

	for (int32 ResourceIndex = 0; ResourceIndex < ARRAY_COUNT(Resources); ++ResourceIndex)
	{
		if (Resources[ResourceIndex])
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				FClearMIParametersCommand,
				FMaterialInstanceResource*,Resource,Resources[ResourceIndex],
			{
				Resource->RenderThread_ClearParameters();
			});
		}
	}

	InitResources();
}

#if WITH_EDITOR
void UMaterialInstance::UpdateStaticPermutation(const FStaticParameterSet& NewParameters, FMaterialInstanceBasePropertyOverrides& NewBasePropertyOverrides)
{
	check(GIsEditor);

	FStaticParameterSet CompareParameters = NewParameters;

	TrimToOverriddenOnly(CompareParameters.StaticSwitchParameters);
	TrimToOverriddenOnly(CompareParameters.StaticComponentMaskParameters);
	TrimToOverriddenOnly(CompareParameters.TerrainLayerWeightParameters);

	const bool bParamsHaveChanged = StaticParameters != CompareParameters;
	const bool bBasePropertyOverridesHaveChanged = BasePropertyOverrides != NewBasePropertyOverrides;

	BasePropertyOverrides = NewBasePropertyOverrides;

	//Ensure our cached base property overrides are up to date.
	UpdateOverridableBaseProperties();

	const bool bHasBasePropertyOverrides = HasOverridenBaseProperties();

	const bool bWantsStaticPermutationResource = Parent && (!CompareParameters.IsEmpty() || bHasBasePropertyOverrides);

	if (bHasStaticPermutationResource != bWantsStaticPermutationResource || bParamsHaveChanged || (bBasePropertyOverridesHaveChanged && bWantsStaticPermutationResource))
	{
		// This will flush the rendering thread which is necessary before changing bHasStaticPermutationResource, since the RT is reading from that directly
		// The update context will also make sure any dependent MI's with static parameters get recompiled
		FMaterialUpdateContext MaterialUpdateContext;
		MaterialUpdateContext.AddMaterialInstance(this);
		bHasStaticPermutationResource = bWantsStaticPermutationResource;
		StaticParameters = CompareParameters;

		CacheResourceShadersForRendering();
	}
}

void UMaterialInstance::UpdateStaticPermutation(const FStaticParameterSet& NewParameters)
{
	UpdateStaticPermutation(NewParameters, BasePropertyOverrides);
}

void UMaterialInstance::UpdateStaticPermutation()
{
	UpdateStaticPermutation(StaticParameters, BasePropertyOverrides);
}

void UMaterialInstance::UpdateParameterNames()
{
	bool bDirty = UpdateParameters();

	// Atleast 1 parameter changed, initialize parameters
	if (bDirty)
	{
		InitResources();
	}
}
#endif

void UMaterialInstance::RecacheUniformExpressions() const
{	
	CacheMaterialInstanceUniformExpressions(this);
}

#if WITH_EDITOR
void UMaterialInstance::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Ensure that the ReferencedTextureGuids array is up to date.
	if (GIsEditor)
	{
		UpdateLightmassTextureTracking();
	}

	PropagateDataToMaterialProxy();

	InitResources();

	UpdateStaticPermutation();

	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet || PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayClear || PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayRemove || PropertyChangedEvent.ChangeType == EPropertyChangeType::Unspecified || PropertyChangedEvent.ChangeType == EPropertyChangeType::Duplicate)
	{
		RecacheMaterialInstanceUniformExpressions(this);
	}
}

#endif // WITH_EDITOR


bool UMaterialInstance::UpdateLightmassTextureTracking()
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


bool UMaterialInstance::GetCastShadowAsMasked() const
{
	if (LightmassSettings.bOverrideCastShadowAsMasked)
	{
		return LightmassSettings.bCastShadowAsMasked;
	}

	if (Parent)
	{
		return Parent->GetCastShadowAsMasked();
	}

	return false;
}

float UMaterialInstance::GetEmissiveBoost() const
{
	if (LightmassSettings.bOverrideEmissiveBoost)
	{
		return LightmassSettings.EmissiveBoost;
	}

	if (Parent)
	{
		return Parent->GetEmissiveBoost();
	}

	return 1.0f;
}

float UMaterialInstance::GetDiffuseBoost() const
{
	if (LightmassSettings.bOverrideDiffuseBoost)
	{
		return LightmassSettings.DiffuseBoost;
	}

	if (Parent)
	{
		return Parent->GetDiffuseBoost();
	}

	return 1.0f;
}

float UMaterialInstance::GetExportResolutionScale() const
{
	if (LightmassSettings.bOverrideExportResolutionScale)
	{
		return FMath::Clamp(LightmassSettings.ExportResolutionScale, .1f, 10.0f);
	}

	if (Parent)
	{
		return FMath::Clamp(Parent->GetExportResolutionScale(), .1f, 10.0f);
	}

	return 1.0f;
}

#if WITH_EDITOR
bool UMaterialInstance::GetTexturesInPropertyChain(EMaterialProperty InProperty, TArray<UTexture*>& OutTextures,  
	TArray<FName>* OutTextureParamNames, class FStaticParameterSet* InStaticParameterSet)
{
	if (Parent != NULL)
	{
		TArray<FName> LocalTextureParamNames;
		bool bResult = Parent->GetTexturesInPropertyChain(InProperty, OutTextures, &LocalTextureParamNames, InStaticParameterSet);
		if (LocalTextureParamNames.Num() > 0)
		{
			// Check textures set in parameters as well...
			for (int32 TPIdx = 0; TPIdx < LocalTextureParamNames.Num(); TPIdx++)
			{
				UTexture* ParamTexture = NULL;
				if (GetTextureParameterValue(LocalTextureParamNames[TPIdx], ParamTexture) == true)
				{
					if (ParamTexture != NULL)
					{
						OutTextures.AddUnique(ParamTexture);
					}
				}

				if (OutTextureParamNames != NULL)
				{
					OutTextureParamNames->AddUnique(LocalTextureParamNames[TPIdx]);
				}
			}
		}
		return bResult;
	}
	return false;
}
#endif // WITH_EDITOR

void UMaterialInstance::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	if (bHasStaticPermutationResource)
	{
		if (CumulativeResourceSize.GetResourceSizeMode() == EResourceSizeMode::Inclusive)
		{
			for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
			{
				for (int32 FeatureLevelIndex = 0; FeatureLevelIndex < ERHIFeatureLevel::Num; FeatureLevelIndex++)
				{
					FMaterialResource* CurrentResource = StaticPermutationMaterialResources[QualityLevelIndex][FeatureLevelIndex];
					CurrentResource->GetResourceSizeEx(CumulativeResourceSize);
				}
			}
		}
	}

	for (int32 ResourceIndex = 0; ResourceIndex < 3; ++ResourceIndex)
	{
		if (Resources[ResourceIndex])
		{
			CumulativeResourceSize.AddDedicatedSystemMemoryBytes(sizeof(FMaterialInstanceResource));
			CumulativeResourceSize.AddDedicatedSystemMemoryBytes(ScalarParameterValues.Num() * sizeof(FMaterialInstanceResource::TNamedParameter<float>));
			CumulativeResourceSize.AddDedicatedSystemMemoryBytes(VectorParameterValues.Num() * sizeof(FMaterialInstanceResource::TNamedParameter<FLinearColor>));
			CumulativeResourceSize.AddDedicatedSystemMemoryBytes(TextureParameterValues.Num() * sizeof(FMaterialInstanceResource::TNamedParameter<const UTexture*>));
			CumulativeResourceSize.AddDedicatedSystemMemoryBytes(FontParameterValues.Num() * sizeof(FMaterialInstanceResource::TNamedParameter<const UTexture*>));
		}
	}
}

FPostProcessMaterialNode* IteratePostProcessMaterialNodes(const FFinalPostProcessSettings& Dest, const UMaterial* Material, FBlendableEntry*& Iterator)
{
	EBlendableLocation Location = Material->BlendableLocation;
	int32 Priority = Material->BlendablePriority;

	for(;;)
	{
		FPostProcessMaterialNode* DataPtr = Dest.BlendableManager.IterateBlendables<FPostProcessMaterialNode>(Iterator);

		if(!DataPtr)
		{
			// end reached
			return 0;
		}

		if(DataPtr->GetLocation() == Location && DataPtr->GetPriority() == Priority && DataPtr->GetMaterialInterface()->GetMaterial() == Material)
		{
			return DataPtr;
		}
	}
}

void UMaterialInstance::AllMaterialsCacheResourceShadersForRendering()
{
	for (TObjectIterator<UMaterialInstance> It; It; ++It)
	{
		UMaterialInstance* MaterialInstance = *It;

		MaterialInstance->CacheResourceShadersForRendering();
	}
}


bool UMaterialInstance::IsChildOf(const UMaterialInterface* ParentMaterialInterface) const
{
	const UMaterialInterface* Material = this;

	while (Material != ParentMaterialInterface && Material != nullptr)
	{
		const UMaterialInstance* MaterialInstance = Cast<const UMaterialInstance>(Material);
		Material = (MaterialInstance != nullptr) ? MaterialInstance->Parent : nullptr;
	}

	return (Material != nullptr);
}


/**
	Properties of the base material. Can now be overridden by instances.
*/

void UMaterialInstance::GetBasePropertyOverridesHash(FSHAHash& OutHash)const
{
	check(IsInGameThread());

	const UMaterial* Mat = GetMaterial();
	check(Mat);

	FSHA1 Hash;
	bool bHasOverrides = false;

	float UsedOpacityMaskClipValue = GetOpacityMaskClipValue();
	if (FMath::Abs(UsedOpacityMaskClipValue - Mat->GetOpacityMaskClipValue()) > SMALL_NUMBER)
	{
		const FString HashString = TEXT("bOverride_OpacityMaskClipValue");
		Hash.UpdateWithString(*HashString, HashString.Len());
		Hash.Update((const uint8*)&UsedOpacityMaskClipValue, sizeof(UsedOpacityMaskClipValue));
		bHasOverrides = true;
	}

	bool bUsedCastDynamicShadowAsMasked = GetCastDynamicShadowAsMasked();
	if ( bUsedCastDynamicShadowAsMasked != Mat->GetCastDynamicShadowAsMasked() )
	{
		const FString HashString = TEXT("bOverride_CastDynamicShadowAsMasked");
		Hash.UpdateWithString(*HashString, HashString.Len());
		Hash.Update((const uint8*)&bUsedCastDynamicShadowAsMasked, sizeof(bUsedCastDynamicShadowAsMasked));
		bHasOverrides = true;
	}

	EBlendMode UsedBlendMode = GetBlendMode();
	if (UsedBlendMode != Mat->GetBlendMode())
	{
		const FString HashString = TEXT("bOverride_BlendMode");
		Hash.UpdateWithString(*HashString, HashString.Len());
		Hash.Update((const uint8*)&UsedBlendMode, sizeof(UsedBlendMode));
		bHasOverrides = true;
 	}
	
	EMaterialShadingModel UsedShadingModel = GetShadingModel();
 	if (UsedShadingModel != Mat->GetShadingModel())
 	{
		const FString HashString = TEXT("bOverride_ShadingModel");
		Hash.UpdateWithString(*HashString, HashString.Len());
		Hash.Update((const uint8*)&UsedShadingModel, sizeof(UsedShadingModel));
		bHasOverrides = true;
	}

 	bool bUsedIsTwoSided = IsTwoSided();
 	if (bUsedIsTwoSided != Mat->IsTwoSided())
 	{
		const FString HashString = TEXT("bOverride_TwoSided");
		Hash.UpdateWithString(*HashString, HashString.Len());
		Hash.Update((uint8*)&bUsedIsTwoSided, sizeof(bUsedIsTwoSided));
		bHasOverrides = true;
	}
	bool bUsedIsDitheredLODTransition = IsDitheredLODTransition();
	if (bUsedIsDitheredLODTransition != Mat->IsDitheredLODTransition())
	{
		const FString HashString = TEXT("bOverride_DitheredLODTransition");
		Hash.UpdateWithString(*HashString, HashString.Len());
		Hash.Update((uint8*)&bUsedIsDitheredLODTransition, sizeof(bUsedIsDitheredLODTransition));
		bHasOverrides = true;
	}

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	FVxgiMaterialProperties BaseVxgiMaterialProperties = Mat->GetVxgiMaterialProperties();

	auto UpdateHashWithBool = [&Hash](bool b) { Hash.Update((uint8*)&b, sizeof(uint8)); };

	if (VxgiMaterialProperties.bVxgiConeTracingEnabled != BaseVxgiMaterialProperties.bVxgiConeTracingEnabled)
	{
		const FString HashString = TEXT("bOverride_IsVxgiConeTracingEnabled");
		Hash.UpdateWithString(*HashString, HashString.Len());
		UpdateHashWithBool(VxgiMaterialProperties.bVxgiConeTracingEnabled);
		bHasOverrides = true;
	}

	if (VxgiMaterialProperties.bUsedWithVxgiVoxelization != BaseVxgiMaterialProperties.bUsedWithVxgiVoxelization)
	{
		const FString HashString = TEXT("bOverride_IsUsedWithVxgiVoxelization");
		Hash.UpdateWithString(*HashString, HashString.Len());
		UpdateHashWithBool(VxgiMaterialProperties.bUsedWithVxgiVoxelization);
		bHasOverrides = true;
	}

	if (VxgiMaterialProperties.bVxgiOmniDirectional != BaseVxgiMaterialProperties.bVxgiOmniDirectional)
	{
		const FString HashString = TEXT("bOverride_IsVxgiOmniDirectional");
		Hash.UpdateWithString(*HashString, HashString.Len());
		UpdateHashWithBool(VxgiMaterialProperties.bVxgiOmniDirectional);
		bHasOverrides = true;
	}

	if (VxgiMaterialProperties.bVxgiProportionalEmittance != BaseVxgiMaterialProperties.bVxgiProportionalEmittance)
	{
		const FString HashString = TEXT("bOverride_IsVxgiProportionalEmittance");
		Hash.UpdateWithString(*HashString, HashString.Len());
		UpdateHashWithBool(VxgiMaterialProperties.bVxgiProportionalEmittance);
		bHasOverrides = true;
	}

	if (VxgiMaterialProperties.bVxgiAllowTesselationDuringVoxelization != BaseVxgiMaterialProperties.bVxgiAllowTesselationDuringVoxelization)
	{
		const FString HashString = TEXT("bOverride_VxgiAllowTesselationDuringVoxelization");
		Hash.UpdateWithString(*HashString, HashString.Len());
		UpdateHashWithBool(VxgiMaterialProperties.bVxgiAllowTesselationDuringVoxelization);
		bHasOverrides = true;
	}

	if (FMath::Abs(VxgiMaterialProperties.VxgiVoxelizationThickness - BaseVxgiMaterialProperties.VxgiVoxelizationThickness) > SMALL_NUMBER)
	{
		const FString HashString = TEXT("bOverride_GetVxgiVoxelizationThickness");
		Hash.UpdateWithString(*HashString, HashString.Len());
		Hash.Update((uint8*)&VxgiMaterialProperties.VxgiVoxelizationThickness, sizeof(VxgiMaterialProperties.VxgiVoxelizationThickness));
		bHasOverrides = true;
	}

	if (!(VxgiMaterialProperties.VxgiOpacityNoiseScaleBias - BaseVxgiMaterialProperties.VxgiOpacityNoiseScaleBias).IsNearlyZero(SMALL_NUMBER))
	{
		const FString HashString = TEXT("bOverride_GetVxgiOpacityNoiseScaleBias");
		Hash.UpdateWithString(*HashString, HashString.Len());
		Hash.Update((uint8*)&VxgiMaterialProperties.VxgiOpacityNoiseScaleBias, sizeof(VxgiMaterialProperties.VxgiOpacityNoiseScaleBias));
		bHasOverrides = true;
	}

	if (VxgiMaterialProperties.bVxgiCoverageSupersampling != VxgiMaterialProperties.bVxgiCoverageSupersampling)
	{
		const FString HashString = TEXT("bOverride_VxgiCoverageSupersampling");
		Hash.UpdateWithString(*HashString, HashString.Len());
		UpdateHashWithBool(VxgiMaterialProperties.bVxgiCoverageSupersampling);
		bHasOverrides = true;
	}

	if (VxgiMaterialProperties.VxgiMaterialSamplingRate != BaseVxgiMaterialProperties.VxgiMaterialSamplingRate)
	{
		const FString HashString = TEXT("bOverride_GetVxgiMaterialSamplingRate");
		Hash.UpdateWithString(*HashString, HashString.Len());
		uint8 VxgiMaterialSamplingRate = VxgiMaterialProperties.VxgiMaterialSamplingRate;
		Hash.Update(&VxgiMaterialSamplingRate, sizeof(VxgiMaterialSamplingRate));
		bHasOverrides = true;
	}
#endif
	// NVCHANGE_END: Add VXGI

 	if (bHasOverrides)
 	{
		Hash.Final();
		Hash.GetHash(&OutHash.Hash[0]);
	}
}

bool UMaterialInstance::HasOverridenBaseProperties()const
{
	check(IsInGameThread());

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	if (Parent)
	{
		FVxgiMaterialProperties BaseVxgiMaterialProperties = Parent->GetVxgiMaterialProperties();

		if (VxgiMaterialProperties.bVxgiConeTracingEnabled != BaseVxgiMaterialProperties.bVxgiConeTracingEnabled ||
			VxgiMaterialProperties.bUsedWithVxgiVoxelization != BaseVxgiMaterialProperties.bUsedWithVxgiVoxelization ||
			VxgiMaterialProperties.bVxgiOmniDirectional != BaseVxgiMaterialProperties.bVxgiOmniDirectional ||
			VxgiMaterialProperties.bVxgiProportionalEmittance != BaseVxgiMaterialProperties.bVxgiProportionalEmittance ||
			VxgiMaterialProperties.bVxgiAllowTesselationDuringVoxelization != BaseVxgiMaterialProperties.bVxgiAllowTesselationDuringVoxelization ||
			FMath::Abs(VxgiMaterialProperties.VxgiVoxelizationThickness - BaseVxgiMaterialProperties.VxgiVoxelizationThickness) > SMALL_NUMBER ||
			!(VxgiMaterialProperties.VxgiOpacityNoiseScaleBias - BaseVxgiMaterialProperties.VxgiOpacityNoiseScaleBias).IsNearlyZero(SMALL_NUMBER) ||
			VxgiMaterialProperties.bVxgiCoverageSupersampling != VxgiMaterialProperties.bVxgiCoverageSupersampling ||
			VxgiMaterialProperties.VxgiMaterialSamplingRate != BaseVxgiMaterialProperties.VxgiMaterialSamplingRate)
		{
			return true;
		}
	}
#endif
	// NVCHANGE_END: Add VXGI

	const UMaterial* Material = GetMaterial();
	if (Parent && Material && Material->bUsedAsSpecialEngineMaterial == false &&
		((FMath::Abs(GetOpacityMaskClipValue() - Parent->GetOpacityMaskClipValue()) > SMALL_NUMBER) ||
		(GetBlendMode() != Parent->GetBlendMode()) ||
		(GetShadingModel() != Parent->GetShadingModel()) ||
		(IsTwoSided() != Parent->IsTwoSided()) ||
		(IsDitheredLODTransition() != Parent->IsDitheredLODTransition()) ||
		(GetCastDynamicShadowAsMasked() != Parent->GetCastDynamicShadowAsMasked())
		))
	{
		return true;
	}

	return false;
}

float UMaterialInstance::GetOpacityMaskClipValue() const
{
	return (!Parent || BasePropertyOverrides.bOverride_OpacityMaskClipValue) ? OpacityMaskClipValue : Parent->GetOpacityMaskClipValue();
}

EBlendMode UMaterialInstance::GetBlendMode() const
{
	return (!Parent || BasePropertyOverrides.bOverride_BlendMode) ? (EBlendMode)BlendMode : (EBlendMode)Parent->GetBlendMode();
}

EMaterialShadingModel UMaterialInstance::GetShadingModel() const
{
	return (!Parent || BasePropertyOverrides.bOverride_ShadingModel) ? (EMaterialShadingModel)ShadingModel : (EMaterialShadingModel)Parent->GetShadingModel();
}

bool UMaterialInstance::IsTwoSided() const
{
	return (!Parent || BasePropertyOverrides.bOverride_TwoSided) ? TwoSided : Parent->IsTwoSided();
}

bool UMaterialInstance::IsDitheredLODTransition() const
{
	return (!Parent || BasePropertyOverrides.bOverride_DitheredLODTransition) ? DitheredLODTransition : Parent->IsDitheredLODTransition();
}

bool UMaterialInstance::IsMasked() const
{
	return GetBlendMode() == EBlendMode::BLEND_Masked;
}

USubsurfaceProfile* UMaterialInstance::GetSubsurfaceProfile_Internal() const
{
	checkSlow(IsInGameThread());
	if (bOverrideSubsurfaceProfile)
	{
		return SubsurfaceProfile;
	}

	// go up the chain if possible
	return Parent ? Parent->GetSubsurfaceProfile_Internal() : 0;
}

/** Checks to see if an input property should be active, based on the state of the material */
bool UMaterialInstance::IsPropertyActive(EMaterialProperty InProperty) const
{
	if(InProperty == MP_DiffuseColor || InProperty == MP_SpecularColor)
	{
		// to suppress some CompilePropertyEx calls
		return false;
	}

	return true;
}

#if WITH_EDITOR
int32 UMaterialInstance::CompilePropertyEx( class FMaterialCompiler* Compiler, const FGuid& AttributeID )
{
	return Parent ? Parent->CompilePropertyEx(Compiler, AttributeID) : INDEX_NONE;
}
#endif // WITH_EDITOR

const FStaticParameterSet& UMaterialInstance::GetStaticParameters() const
{
	return StaticParameters;
}

void UMaterialInstance::GetLightingGuidChain(bool bIncludeTextures, TArray<FGuid>& OutGuids) const
{
#if WITH_EDITORONLY_DATA
	if (bIncludeTextures)
	{
		OutGuids.Append(ReferencedTextureGuids);
	}
	if (Parent)
	{
		Parent->GetLightingGuidChain(bIncludeTextures, OutGuids);
	}
	Super::GetLightingGuidChain(bIncludeTextures, OutGuids);
#endif
}

void UMaterialInstance::PreSave(const class ITargetPlatform* TargetPlatform)
{
	// @TODO : Remove any duplicate data from parent? Aims at improving change propagation (if controlled by parent)
	Super::PreSave(TargetPlatform);
}

float UMaterialInstance::GetTextureDensity(FName TextureName, const struct FMeshUVChannelInfo& UVChannelData) const
{
	ensure(UVChannelData.bInitialized);

	const float Density = Super::GetTextureDensity(TextureName, UVChannelData);
	
	// If it is not handled by this instance, try the parent
	if (!Density && Parent)
	{
		return Parent->GetTextureDensity(TextureName, UVChannelData);
	}
	return Density;
}

UMaterialInstance::FCustomStaticParametersGetterDelegate UMaterialInstance::CustomStaticParametersGetters;
TArray<UMaterialInstance::FCustomParameterSetUpdaterDelegate> UMaterialInstance::CustomParameterSetUpdaters;
