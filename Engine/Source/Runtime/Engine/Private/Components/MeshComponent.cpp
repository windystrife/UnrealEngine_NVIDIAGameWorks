// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/MeshComponent.h"
#include "Materials/Material.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ContentStreaming.h"
#include "Streaming/TextureStreamingHelpers.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogMaterialParameter, Warning, All);

UMeshComponent::UMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CastShadow = true;
	bUseAsOccluder = true;
	bCanEverAffectNavigation = true;
	bCachedMaterialParameterIndicesAreDirty = true;
}

UMaterialInterface* UMeshComponent::GetMaterial(int32 ElementIndex) const
{
	if (OverrideMaterials.IsValidIndex(ElementIndex))
	{
		return OverrideMaterials[ElementIndex];
	}
	else
	{
		return nullptr;
	}
}

void UMeshComponent::SetMaterial(int32 ElementIndex, UMaterialInterface* Material)
{
	if (ElementIndex >= 0)
	{
		if (OverrideMaterials.IsValidIndex(ElementIndex) && (OverrideMaterials[ElementIndex] == Material))
		{
			// Do nothing, the material is already set
		}
		else
		{
			// Grow the array if the new index is too large
			if (OverrideMaterials.Num() <= ElementIndex)
			{
				OverrideMaterials.AddZeroed(ElementIndex + 1 - OverrideMaterials.Num());
			}
			
			// Check if we are setting a dynamic instance of the original material, or replacing a nullptr material  (if not we should dirty the material parameter name cache)
			if (Material != nullptr)
			{
				UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(Material);
				if ( (DynamicMaterial != nullptr && DynamicMaterial->Parent != OverrideMaterials[ElementIndex]) || OverrideMaterials[ElementIndex] == nullptr)
				{
					// Mark cached material parameter names dirty
					MarkCachedMaterialParameterNameIndicesDirty();
				}
			}	

			// Set the material and invalidate things
			OverrideMaterials[ElementIndex] = Material;
			MarkRenderStateDirty();			
			if (Material)
			{
				Material->AddToCluster(this, true);
			}

			FBodyInstance* BodyInst = GetBodyInstance();
			if (BodyInst && BodyInst->IsValidBodyInstance())
			{
				BodyInst->UpdatePhysicalMaterials();
			}
		}
	}
}

void UMeshComponent::SetMaterialByName(FName MaterialSlotName, UMaterialInterface* Material)
{
	int32 MaterialIndex = GetMaterialIndex(MaterialSlotName);
	if (MaterialIndex < 0)
		return;

	SetMaterial(MaterialIndex, Material);
}

FMaterialRelevance UMeshComponent::GetMaterialRelevance(ERHIFeatureLevel::Type InFeatureLevel) const
{
	// Combine the material relevance for all materials.
	FMaterialRelevance Result;
	for(int32 ElementIndex = 0;ElementIndex < GetNumMaterials();ElementIndex++)
	{
		UMaterialInterface const* MaterialInterface = GetMaterial(ElementIndex);
		if(!MaterialInterface)
		{
			MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
		}
		Result |= MaterialInterface->GetRelevance_Concurrent(InFeatureLevel);
	}
	return Result;
}

int32 UMeshComponent::GetNumOverrideMaterials() const
{
	return OverrideMaterials.Num();
}

#if WITH_EDITOR
void UMeshComponent::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property != nullptr)
	{
		if (PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_STRING_CHECKED(UMeshComponent, OverrideMaterials))
		{
			CleanUpOverrideMaterials();
		}
	}
}

void UMeshComponent::CleanUpOverrideMaterials()
{
	//We have to remove material override Ids that are bigger then the material list
	if (GetNumOverrideMaterials() > GetNumMaterials())
	{
		//Remove the override material id that are superior to the static mesh materials number
		int32 RemoveCount = GetNumOverrideMaterials() - GetNumMaterials();
		OverrideMaterials.RemoveAt(GetNumMaterials(), RemoveCount);
	}
}
void UMeshComponent::EmptyOverrideMaterials()
{
	OverrideMaterials.Reset();
}
#endif

int32 UMeshComponent::GetNumMaterials() const
{
	return 0;
}

void UMeshComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	for (int32 ElementIndex = 0; ElementIndex < GetNumMaterials(); ElementIndex++)
	{
		if (UMaterialInterface* MaterialInterface = GetMaterial(ElementIndex))
		{
			OutMaterials.Add(MaterialInterface);
		}
	}
}

void UMeshComponent::PrestreamTextures( float Seconds, bool bPrioritizeCharacterTextures, int32 CinematicTextureGroups )
{
	// If requested, tell the streaming system to only process character textures for 30 frames.
	if (bPrioritizeCharacterTextures)
	{
		IStreamingManager::Get().SetDisregardWorldResourcesForFrames(30);
	}

	TArray<UTexture*> Textures;
	GetUsedTextures(/*out*/ Textures, EMaterialQualityLevel::Num);

	for (UTexture* Texture : Textures)
	{
		if (UTexture2D* Texture2D = Cast<UTexture2D>(Texture))
		{
			Texture2D->SetForceMipLevelsToBeResident(Seconds, CinematicTextureGroups);
		}
	}
}

void UMeshComponent::SetTextureForceResidentFlag( bool bForceMiplevelsToBeResident )
{
	const int32 CinematicTextureGroups = 0;
	const float Seconds = -1.0f;

	TArray<UTexture*> Textures;
	GetUsedTextures(/*out*/ Textures, EMaterialQualityLevel::Num);

	for (UTexture* Texture : Textures)
	{
		if (UTexture2D* Texture2D = Cast<UTexture2D>(Texture))
		{
			Texture2D->SetForceMipLevelsToBeResident(Seconds, CinematicTextureGroups);
			Texture2D->bForceMiplevelsToBeResident = bForceMiplevelsToBeResident;
		}
	}
}

TArray<class UMaterialInterface*> UMeshComponent::GetMaterials() const
{
	TArray<class UMaterialInterface*> OutMaterials;
	int32 TotalNumMaterials = GetNumMaterials();
	if(TotalNumMaterials > 0)
	{
		// make sure to extend it
		OutMaterials.AddZeroed(TotalNumMaterials);

		for(int32 MaterialIndex=0; MaterialIndex < TotalNumMaterials; ++MaterialIndex)
		{
			OutMaterials[MaterialIndex] = GetMaterial(MaterialIndex);
		}
	}

	return OutMaterials;
}

int32 UMeshComponent::GetMaterialIndex(FName MaterialSlotName) const
{
	//This function should be override
	return -1;
}

TArray<FName> UMeshComponent::GetMaterialSlotNames() const
{
	//This function should be override
	return TArray<FName>();
}

bool UMeshComponent::IsMaterialSlotNameValid(FName MaterialSlotName) const
{
	//This function should be override
	return false;
}

void UMeshComponent::SetScalarParameterValueOnMaterials(const FName ParameterName, const float ParameterValue)
{
	if (bCachedMaterialParameterIndicesAreDirty)
	{
		CacheMaterialParameterNameIndices();
	}

	// Look up material index array according to ParameterName
	if (FMaterialParameterCache* ParameterCache = MaterialParameterCache.Find(ParameterName))
	{
		const TArray<int32>& MaterialIndices = ParameterCache->ScalarParameterMaterialIndices;
		// Loop over all the material indices and update set the parameter value on the corresponding materials		
		for ( int32 MaterialIndex : MaterialIndices)
		{
			UMaterialInterface* MaterialInterface = GetMaterial(MaterialIndex);
			if (MaterialInterface)
			{
				UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(MaterialInterface);
				if (!DynamicMaterial) 
				{
					DynamicMaterial = CreateAndSetMaterialInstanceDynamic(MaterialIndex);
				}
				DynamicMaterial->SetScalarParameterValue(ParameterName, ParameterValue);
			}
		}
	}
	else
	{
		UE_LOG(LogMaterialParameter, Log, TEXT("%s material parameter hasn't found on the component %s"), *ParameterName.ToString(), *GetPathName());
	}
}

void UMeshComponent::SetVectorParameterValueOnMaterials(const FName ParameterName, const FVector ParameterValue)
{
	if (bCachedMaterialParameterIndicesAreDirty)
	{
		CacheMaterialParameterNameIndices();
	}

	// Look up material index array according to ParameterName
	if (FMaterialParameterCache* ParameterCache = MaterialParameterCache.Find(ParameterName))
	{
		const TArray<int32>& MaterialIndices = ParameterCache->VectorParameterMaterialIndices;
		// Loop over all the material indices and update set the parameter value on the corresponding materials		
		for ( int32 MaterialIndex : MaterialIndices )
		{
			UMaterialInterface* MaterialInterface = GetMaterial(MaterialIndex);
			if (MaterialInterface)
			{
				UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(MaterialInterface);
				if (!DynamicMaterial)
				{
					DynamicMaterial = CreateAndSetMaterialInstanceDynamic(MaterialIndex);
				}
				DynamicMaterial->SetVectorParameterValue(ParameterName, ParameterValue);
			}
		}
	}
}

void UMeshComponent::MarkCachedMaterialParameterNameIndicesDirty()
{
	// Flag the cached material parameter indices as dirty
	bCachedMaterialParameterIndicesAreDirty = true;
}

void UMeshComponent::CacheMaterialParameterNameIndices()
{
	// Clean up possible previous data
	MaterialParameterCache.Reset();

	// not sure if this is the best way to do this
	const UWorld* World = GetWorld();
	// to set the default value for scalar params, we use a FMaterialResource, which means the world has to be rendering
	const bool bHasMaterialResource = (World && World->WorldType != EWorldType::Inactive);
	const ERHIFeatureLevel::Type FeatureLevel = bHasMaterialResource ? World->FeatureLevel : ERHIFeatureLevel::Num;
	
	// Retrieve all used materials
	TArray<UMaterialInterface*> MaterialInterfaces = GetMaterials();
	int32 MaterialIndex = 0;
	for (UMaterialInterface* MaterialInterface : MaterialInterfaces)
	{
		// If available retrieve material instance
		UMaterial* Material = (MaterialInterface != nullptr) ? MaterialInterface->GetMaterial() : nullptr;
		if (Material)
		{
			TArray<FName> OutParameterNames;
			TArray<FGuid> OutParameterIds;

			// Retrieve all scalar parameter names from the material
			Material->GetAllScalarParameterNames(OutParameterNames, OutParameterIds);
			for (FName& ParameterName : OutParameterNames)
			{
				// Add or retrieve entry for this parameter name
				FMaterialParameterCache& ParameterCache = MaterialParameterCache.FindOrAdd(ParameterName);
				// Add the corresponding material index
				ParameterCache.ScalarParameterMaterialIndices.Add(MaterialIndex);
				
				// GetScalarParameterDefault() expects to use a FMaterialResource, which means the world has to be rendering
				if (bHasMaterialResource)
				{
					// store the default value
					ParameterCache.ScalarParameterDefaultValue = Material->GetScalarParameterDefault(ParameterName, FeatureLevel);
				}
			}

			// Empty parameter names and ids
			OutParameterNames.Reset();
			OutParameterIds.Reset();

			// Retrieve all vector parameter names from the material
			Material->GetAllVectorParameterNames(OutParameterNames, OutParameterIds);
			for (FName& ParameterName : OutParameterNames)
			{
				// Add or retrieve entry for this parameter name
				FMaterialParameterCache& ParameterCache = MaterialParameterCache.FindOrAdd(ParameterName);
				// Add the corresponding material index
				ParameterCache.VectorParameterMaterialIndices.Add(MaterialIndex);
			}
		}
		++MaterialIndex;
	}

	bCachedMaterialParameterIndicesAreDirty = false;
}

void UMeshComponent::GetStreamingTextureInfoInner(FStreamingTextureLevelContext& LevelContext, const TArray<FStreamingTextureBuildInfo>* PreBuiltData, float ComponentScaling, TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const
{
	LevelContext.BindBuildData(PreBuiltData);

	const int32 NumMaterials = GetNumMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
	{
		FPrimitiveMaterialInfo MaterialData;
		if (GetMaterialStreamingData(MaterialIndex, MaterialData))
		{
			LevelContext.ProcessMaterial(Bounds, MaterialData, ComponentScaling, OutStreamingTextures);
		}
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

void UMeshComponent::LogMaterialsAndTextures(FOutputDevice& Ar, int32 Indent) const
{
	Ar.Logf(TEXT("%s%s:"), FCString::Tab(Indent), *GetClass()->GetName());

	for (int32 MaterialIndex = 0; MaterialIndex < OverrideMaterials.Num(); ++MaterialIndex)
	{
		Ar.Logf(TEXT("%s[Material Override: %d]"), FCString::Tab(Indent + 1), MaterialIndex);
		const UMaterialInterface* MaterialInterface = OverrideMaterials[MaterialIndex];
		if (MaterialInterface)
		{
			MaterialInterface->LogMaterialsAndTextures(Ar, Indent + 2);
		}
		else
		{
			Ar.Logf(TEXT("%snullptr"), FCString::Tab(Indent + 2), MaterialIndex);
		}
	}

	// Backup the material overrides so we can access the mesh original materials.
	TArray<class UMaterialInterface*> OverrideMaterialsBackup;
	FMemory::Memswap(&OverrideMaterialsBackup, &const_cast<UMeshComponent*>(this)->OverrideMaterials, sizeof(OverrideMaterialsBackup));

	TArray<UMaterialInterface*> MaterialInterfaces = GetMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialInterfaces.Num(); ++MaterialIndex)
	{
		Ar.Logf(TEXT("%s[Mesh Material: %d]"), FCString::Tab(Indent + 1), MaterialIndex);
		const UMaterialInterface* MaterialInterface = MaterialInterfaces[MaterialIndex];
		if (MaterialInterface)
		{
			MaterialInterface->LogMaterialsAndTextures(Ar, Indent + 2);
		}
		else
		{
			Ar.Logf(TEXT("%snullptr"), FCString::Tab(Indent + 2), MaterialIndex);
		}
	}

	// Restore the overrides.
	FMemory::Memswap(&OverrideMaterialsBackup, &const_cast<UMeshComponent*>(this)->OverrideMaterials, sizeof(OverrideMaterialsBackup));
}

#endif
