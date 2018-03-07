// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshMergeUtilities.h"

#include "Engine/MapBuildDataRegistry.h"
#include "Engine/MeshMerging.h"

#include "MaterialOptions.h"
#include "IMaterialBakingModule.h"
#include "RawMesh.h"

#include "Misc/PackageName.h"
#include "MaterialUtilities.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/ShapeComponent.h"

#include "SkeletalMeshTypes.h"
#include "SkeletalRenderPublic.h"

#include "UObject/UObjectBaseUtility.h"
#include "UObject/Package.h"
#include "Materials/Material.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "HierarchicalLODUtilitiesModule.h"
#include "MeshMergeData.h"
#include "IHierarchicalLODUtilities.h"
#include "Engine/MeshMergeCullingVolume.h"

#include "Landscape.h"
#include "LandscapeProxy.h"

#include "Editor.h"
#include "ProxyGenerationProcessor.h"
#include "Editor/EditorPerProjectUserSettings.h"

#include "ProxyMaterialUtilities.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/BodySetup.h"
#include "MeshUtilities.h"
#include "ImageUtils.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "IMeshReductionManagerModule.h"

#include "ProxyGenerationProcessor.h"
#include "IMaterialBakingAdapter.h"
#include "StaticMeshComponentAdapter.h"
#include "SkeletalMeshAdapter.h"
#include "StaticMeshAdapter.h"
#include "MeshMergeEditorExtensions.h"

#include "MeshMergeDataTracker.h"

#include "Misc/FileHelper.h"
#include "MeshMergeHelpers.h"
#include "Settings/EditorExperimentalSettings.h"
#include "MaterialBakingStructures.h"
#include "Async/ParallelFor.h"

#define LOCTEXT_NAMESPACE "MeshMergeUtils"

DEFINE_LOG_CATEGORY(LogMeshMerging);

FMeshMergeUtilities::FMeshMergeUtilities()
{
	Processor = new FProxyGenerationProcessor();

	// If case the experimental material baking is turned on add callback for registering editor extensions with Skeletal/Static mesh editor
	if (GetDefault<UEditorExperimentalSettings>()->bAssetMaterialBaking)
	{
		ModuleLoadedDelegateHandle = FModuleManager::Get().OnModulesChanged().AddStatic(&FMeshMergeEditorExtensions::OnModulesChanged);
	}
}

FMeshMergeUtilities::~FMeshMergeUtilities()
{
	FModuleManager::Get().OnModulesChanged().Remove(ModuleLoadedDelegateHandle);
	FMeshMergeEditorExtensions::RemoveExtenders();
}

void FMeshMergeUtilities::BakeMaterialsForComponent(TArray<TWeakObjectPtr<UObject>>& OptionObjects, IMaterialBakingAdapter* Adapter) const
{
	// Try and find material (merge) options from provided set of objects
	TWeakObjectPtr<UObject>* MaterialOptionsObject = OptionObjects.FindByPredicate([](TWeakObjectPtr<UObject> Object)
	{
		return Cast<UMaterialOptions>(Object.Get()) != nullptr;
	});

	TWeakObjectPtr<UObject>* MaterialMergeOptionsObject = OptionObjects.FindByPredicate([](TWeakObjectPtr<UObject> Object)
	{
		return Cast<UMaterialMergeOptions>(Object.Get()) != nullptr;
	});

	UMaterialOptions* MaterialOptions = MaterialOptionsObject ? Cast<UMaterialOptions>(MaterialOptionsObject->Get()) : nullptr;
	checkf(MaterialOptions, TEXT("No valid material options found"));


	UMaterialMergeOptions* MaterialMergeOptions  = MaterialMergeOptionsObject ? Cast<UMaterialMergeOptions>(MaterialMergeOptionsObject->Get()) : nullptr;

	// Mesh / LOD index	
	TMap<uint32, FRawMesh> RawMeshLODs;

	// LOD index, <original section index, unique section index>
	TMultiMap<uint32, TPair<uint32, uint32>> UniqueSectionIndexPerLOD;

	// Unique set of sections in mesh
	TArray<FSectionInfo> UniqueSections;

	TArray<FSectionInfo> Sections;

	int32 NumLODs = Adapter->GetNumberOfLODs();

	// Retrieve raw mesh data and unique sections
	for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
	{
		// Reset section for reuse
		Sections.SetNum(0, false);

		// Extract raw mesh data 
		const bool bProcessedLOD = MaterialOptions->LODIndices.Contains(LODIndex);
		if (bProcessedLOD)
		{
			FRawMesh& RawMesh = RawMeshLODs.Add(LODIndex);
			Adapter->RetrieveRawMeshData(LODIndex, RawMesh, MaterialOptions->bUseMeshData);
		}

		// Extract sections for given LOD index from the mesh 
		Adapter->RetrieveMeshSections(LODIndex, Sections);

		for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
		{
			FSectionInfo& Section = Sections[SectionIndex];
			Section.bProcessed = bProcessedLOD;
			const int32 UniqueIndex = UniqueSections.AddUnique(Section);
			UniqueSectionIndexPerLOD.Add(LODIndex, TPair<uint32, uint32>(SectionIndex, UniqueIndex));
		}
	}

	TArray<UMaterialInterface*> UniqueMaterials;
	TMap<UMaterialInterface*, int32> MaterialIndices;
	TMultiMap<uint32, uint32> SectionToMaterialMap;
	// Populate list of unique materials and store section mappings
	for (int32 SectionIndex = 0; SectionIndex < UniqueSections.Num(); ++SectionIndex)
	{
		FSectionInfo& Section = UniqueSections[SectionIndex];
		const int32 UniqueIndex = UniqueMaterials.AddUnique(Section.Material);
		SectionToMaterialMap.Add(UniqueIndex, SectionIndex);
	}

	TArray<bool> bMaterialUsesVertexData;
	DetermineMaterialVertexDataUsage(bMaterialUsesVertexData, UniqueMaterials, MaterialOptions);

	TArray<FMeshData> GlobalMeshSettings;
	TArray<FMaterialData> GlobalMaterialSettings;
	TMultiMap< uint32, TPair<uint32, uint32>> OutputMaterialsMap;
	for (int32 MaterialIndex = 0; MaterialIndex < UniqueMaterials.Num(); ++MaterialIndex)
	{
		UMaterialInterface* Material = UniqueMaterials[MaterialIndex];
		const bool bDoesMaterialUseVertexData = bMaterialUsesVertexData[MaterialIndex];
		// Retrieve all sections using this material 
		TArray<uint32> SectionIndices;
		SectionToMaterialMap.MultiFind(MaterialIndex, SectionIndices);

		if (MaterialOptions->bUseMeshData)
		{
			for (const int32 LODIndex : MaterialOptions->LODIndices)
			{
				TArray<TPair<uint32, uint32>> IndexPairs;
				UniqueSectionIndexPerLOD.MultiFind(LODIndex, IndexPairs);

				FMeshData MeshSettings;

				// Add material indices used for rendering out material
				for (const TPair<uint32, uint32>& Pair : IndexPairs)
				{
					if (SectionIndices.Contains(Pair.Value))
					{
						MeshSettings.MaterialIndices.Add(Pair.Key);
					}
				}

				if (MeshSettings.MaterialIndices.Num())
				{
					// Retrieve raw mesh
					MeshSettings.RawMesh = RawMeshLODs.Find(LODIndex);

					MeshSettings.TextureCoordinateBox = FBox2D(FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f));
					const bool bUseVertexColor = (MeshSettings.RawMesh->WedgeColors.Num() > 0);
					if (MaterialOptions->bUseSpecificUVIndex)
					{
						MeshSettings.TextureCoordinateIndex = MaterialOptions->TextureCoordinateIndex;
					}
					// if you use vertex color, we can't rely on overlapping UV channel, so use light map UV to unwrap UVs
					else if (bUseVertexColor)
					{
						MeshSettings.TextureCoordinateIndex = Adapter->LightmapUVIndex();
					}
					else
					{
						MeshSettings.TextureCoordinateIndex = 0;
					}
					
					Adapter->ApplySettings(LODIndex, MeshSettings);
					
					// In case part of the UVs is not within the 0-1 range try to use the lightmap UVs
					const bool bNeedsUniqueUVs = FMeshMergeHelpers::CheckWrappingUVs(MeshSettings.RawMesh->WedgeTexCoords[MeshSettings.TextureCoordinateIndex]);
					const int32 LightMapUVIndex = Adapter->LightmapUVIndex();
					if (bNeedsUniqueUVs && MeshSettings.TextureCoordinateIndex != LightMapUVIndex && MeshSettings.RawMesh->WedgeTexCoords[LightMapUVIndex].Num())
					{
						MeshSettings.TextureCoordinateIndex = LightMapUVIndex;
					}

					FMaterialData MaterialSettings;
					MaterialSettings.Material = Material;					

					// Add all user defined properties for baking out
					for (const FPropertyEntry& Entry : MaterialOptions->Properties)
					{
						int32 NumTextureCoordinates;
						bool bUsesVertexData;
						Material->AnalyzeMaterialProperty(Entry.Property, NumTextureCoordinates, bUsesVertexData);

						if (!Entry.bUseConstantValue && Entry.Property != MP_MAX)
						{
							MaterialSettings.PropertySizes.Add(Entry.Property, Entry.bUseCustomSize ? Entry.CustomSize : MaterialOptions->TextureSize);
						}
					}

					// For each original material index add an entry to the corresponding LOD and bake output index 
					for (int32 Index : MeshSettings.MaterialIndices)
					{
						OutputMaterialsMap.Add(LODIndex, TPair<uint32, uint32>(Index, GlobalMeshSettings.Num()));
					}

					GlobalMeshSettings.Add(MeshSettings);
					GlobalMaterialSettings.Add(MaterialSettings);
				}
			}
		}
		else
		{
			// If we are not using the mesh data we aren't doing anything special, just bake out uv range
			FMeshData MeshSettings;
			for (int32 LODIndex : MaterialOptions->LODIndices)
			{
				TArray<TPair<uint32, uint32>> IndexPairs;
				UniqueSectionIndexPerLOD.MultiFind(LODIndex, IndexPairs);
				for (const TPair<uint32, uint32>& Pair : IndexPairs)
				{
					if (SectionIndices.Contains(Pair.Value))
					{
						MeshSettings.MaterialIndices.Add(Pair.Key);
					}
				}
			}

			if (MeshSettings.MaterialIndices.Num())
			{
				MeshSettings.RawMesh = nullptr;
				MeshSettings.TextureCoordinateBox = FBox2D(FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f));
				MeshSettings.TextureCoordinateIndex = 0;

				FMaterialData MaterialSettings;
				MaterialSettings.Material = Material;

				// Add all user defined properties for baking out
				for (const FPropertyEntry& Entry : MaterialOptions->Properties)
				{
					if (!Entry.bUseConstantValue && Material->IsPropertyActive(Entry.Property) && Entry.Property != MP_MAX)
					{
						MaterialSettings.PropertySizes.Add(Entry.Property, Entry.bUseCustomSize ? Entry.CustomSize : MaterialOptions->TextureSize);
					}
				}

				for (int32 LODIndex : MaterialOptions->LODIndices)
				{
					TArray<TPair<uint32, uint32>> IndexPairs;
					UniqueSectionIndexPerLOD.MultiFind(LODIndex, IndexPairs);
					for (const TPair<uint32, uint32>& Pair : IndexPairs)
					{
						if (SectionIndices.Contains(Pair.Value))
						{
							/// For each original material index add an entry to the corresponding LOD and bake output index 
							OutputMaterialsMap.Add(LODIndex, TPair<uint32, uint32>(Pair.Key, GlobalMeshSettings.Num()));
						}
					}
				}

				GlobalMeshSettings.Add(MeshSettings);
				GlobalMaterialSettings.Add(MaterialSettings);
			}
		}
	}

	TArray<FMeshData*> MeshSettingPtrs;
	for (int32 SettingsIndex = 0; SettingsIndex < GlobalMeshSettings.Num(); ++SettingsIndex)
	{
		MeshSettingPtrs.Add(&GlobalMeshSettings[SettingsIndex]);
	}

	TArray<FMaterialData*> MaterialSettingPtrs;
	for (int32 SettingsIndex = 0; SettingsIndex < GlobalMaterialSettings.Num(); ++SettingsIndex)
	{
		MaterialSettingPtrs.Add(&GlobalMaterialSettings[SettingsIndex]);
	}

	TArray<FBakeOutput> BakeOutputs;
	IMaterialBakingModule& Module = FModuleManager::Get().LoadModuleChecked<IMaterialBakingModule>("MaterialBaking");
	Module.BakeMaterials(MaterialSettingPtrs, MeshSettingPtrs, BakeOutputs);

	// Append constant properties which did not require baking out
	TArray<FColor> ConstantData;
	FIntPoint ConstantSize(1, 1);
	for (const FPropertyEntry& Entry : MaterialOptions->Properties)
	{
		if (Entry.bUseConstantValue && Entry.Property != MP_MAX)
		{
			ConstantData.SetNum(1, false);
			ConstantData[0] = FColor(Entry.ConstantValue * 255.0f, Entry.ConstantValue * 255.0f, Entry.ConstantValue * 255.0f);
			for (FBakeOutput& Ouput : BakeOutputs)
			{
				Ouput.PropertyData.Add(Entry.Property, ConstantData);
				Ouput.PropertySizes.Add(Entry.Property, ConstantSize);
			}
		}
	}

	TArray<UMaterialInterface*> NewMaterials;

	FString PackageName = Adapter->GetBaseName();

	const FGuid NameGuid = FGuid::NewGuid();
	for (int32 OutputIndex = 0; OutputIndex < BakeOutputs.Num(); ++OutputIndex)
	{
		// Create merged material asset
		FString MaterialAssetName = TEXT("M_") + FPackageName::GetShortName(PackageName) + TEXT("_") + MaterialSettingPtrs[OutputIndex]->Material->GetName() + TEXT("_") + NameGuid.ToString();
		FString MaterialPackageName = FPackageName::GetLongPackagePath(PackageName) + TEXT("/") + MaterialAssetName;

		FBakeOutput& Output = BakeOutputs[OutputIndex];
		// Optimize output 
		for (auto DataPair : Output.PropertyData)
		{
			FMaterialUtilities::OptimizeSampleArray(DataPair.Value, Output.PropertySizes[DataPair.Key]);
		}

		UMaterialInterface* Material = nullptr;

		if (Adapter->GetOuter())
		{
			Material = FMaterialUtilities::CreateProxyMaterialAndTextures(Adapter->GetOuter(), MaterialAssetName, Output, *MeshSettingPtrs[OutputIndex], *MaterialSettingPtrs[OutputIndex], MaterialOptions);
		}
		else
		{
			Material = FMaterialUtilities::CreateProxyMaterialAndTextures(MaterialPackageName, MaterialAssetName, Output, *MeshSettingPtrs[OutputIndex], *MaterialSettingPtrs[OutputIndex], MaterialOptions);
		}

		
		NewMaterials.Add(Material);
	}

	// Retrieve material indices which were not baked out and should still be part of the final asset
	TArray<int32> NonReplaceMaterialIndices;
	for (int32 MaterialIndex = 0; MaterialIndex < NewMaterials.Num(); ++MaterialIndex)
	{
		TArray<uint32> SectionIndices;
		SectionToMaterialMap.MultiFind(MaterialIndex, SectionIndices);

		for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
		{
			const bool bProcessedLOD = MaterialOptions->LODIndices.Contains(LODIndex);
			if (!bProcessedLOD)
			{
				TArray<TPair<uint32, uint32>> IndexPairs;
				UniqueSectionIndexPerLOD.MultiFind(LODIndex, IndexPairs);

				for (TPair<uint32, uint32>& Pair : IndexPairs)
				{
					NonReplaceMaterialIndices.AddUnique(Adapter->GetMaterialIndex(LODIndex, Pair.Key));
				}
			}
		}
	}

	// Remap all baked out materials to their new material indices
	TMap<uint32, uint32> NewMaterialRemap;
	for (int32 LODIndex : MaterialOptions->LODIndices)
	{
		TArray<TPair<uint32, uint32>> IndexPairs;
		OutputMaterialsMap.MultiFind(LODIndex, IndexPairs);

		// Key == original section index, Value == unique material index
		for (auto Pair : IndexPairs)
		{
			int32 SetIndex = Adapter->GetMaterialIndex(LODIndex, Pair.Key);
			if (!NonReplaceMaterialIndices.Contains(SetIndex))
			{
				Adapter->SetMaterial(SetIndex, NewMaterials[Pair.Value]);
			}
			else
			{
				const FSectionInfo& SectionInfo = UniqueSections[Pair.Key];
				// Check if this material was  processed and a new entry already exists
				if (uint32* ExistingIndex = NewMaterialRemap.Find(Pair.Value))
				{
					Adapter->RemapMaterialIndex(LODIndex, Pair.Key, *ExistingIndex);
				}
				else
				{
					// Add new material
					const int32 NewMaterialIndex = Adapter->AddMaterial(NewMaterials[Pair.Value]);
					NewMaterialRemap.Add(Pair.Value, NewMaterialIndex);
					Adapter->RemapMaterialIndex(LODIndex, Pair.Key, NewMaterialIndex);
				}
			}
		}
	}

	Adapter->UpdateUVChannelData();
}

void FMeshMergeUtilities::BakeMaterialsForComponent(USkeletalMeshComponent* SkeletalMeshComponent) const
{
	// Retrieve settings object
	UMaterialOptions* MaterialOptions = DuplicateObject(GetMutableDefault<UMaterialOptions>(), GetTransientPackage());
	UAssetBakeOptions* AssetOptions = GetMutableDefault<UAssetBakeOptions>();
	UMaterialMergeOptions* MergeOptions = GetMutableDefault<UMaterialMergeOptions>();
	TArray<TWeakObjectPtr<UObject>> Objects{ MergeOptions, AssetOptions, MaterialOptions };

	const int32 NumLODs = SkeletalMeshComponent->SkeletalMesh->LODInfo.Num();
	IMaterialBakingModule& Module = FModuleManager::Get().LoadModuleChecked<IMaterialBakingModule>("MaterialBaking");
	if (!Module.SetupMaterialBakeSettings(Objects, NumLODs))
	{
		return;
	}

	// Bake out materials for skeletal mesh
	FSkeletalMeshComponentAdapter Adapter(SkeletalMeshComponent);
	BakeMaterialsForComponent(Objects, &Adapter);
	SkeletalMeshComponent->MarkRenderStateDirty();
}

void FMeshMergeUtilities::BakeMaterialsForComponent(UStaticMeshComponent* StaticMeshComponent) const
{
	// Retrieve settings object
	UMaterialOptions* MaterialOptions = DuplicateObject(GetMutableDefault<UMaterialOptions>(), GetTransientPackage());
	UAssetBakeOptions* AssetOptions = GetMutableDefault<UAssetBakeOptions>();
	UMaterialMergeOptions* MergeOptions = GetMutableDefault<UMaterialMergeOptions>();
	TArray<TWeakObjectPtr<UObject>> Objects{ MergeOptions, AssetOptions, MaterialOptions };

	const int32 NumLODs = StaticMeshComponent->GetStaticMesh()->GetNumLODs();
	IMaterialBakingModule& Module = FModuleManager::Get().LoadModuleChecked<IMaterialBakingModule>("MaterialBaking");
	if (!Module.SetupMaterialBakeSettings(Objects, NumLODs))
	{
		return;
	}

	// Bake out materials for static mesh component
	FStaticMeshComponentAdapter Adapter(StaticMeshComponent);
	BakeMaterialsForComponent(Objects, &Adapter);
	StaticMeshComponent->MarkRenderStateDirty();
}

void FMeshMergeUtilities::BakeMaterialsForMesh(UStaticMesh* StaticMesh) const
{
	// Retrieve settings object
	UMaterialOptions* MaterialOptions = DuplicateObject(GetMutableDefault<UMaterialOptions>(), GetTransientPackage());
	UAssetBakeOptions* AssetOptions = GetMutableDefault<UAssetBakeOptions>();
	UMaterialMergeOptions* MergeOptions = GetMutableDefault<UMaterialMergeOptions>();
	TArray<TWeakObjectPtr<UObject>> Objects{ MergeOptions, AssetOptions, MaterialOptions };

	const int32 NumLODs = StaticMesh->GetNumLODs();
	IMaterialBakingModule& Module = FModuleManager::Get().LoadModuleChecked<IMaterialBakingModule>("MaterialBaking");
	if (!Module.SetupMaterialBakeSettings(Objects, NumLODs))
	{
		return;
	}

	// Bake out materials for static mesh asset
	FStaticMeshAdapter Adapter(StaticMesh);
	BakeMaterialsForComponent(Objects, &Adapter);
}

void FMeshMergeUtilities::DetermineMaterialVertexDataUsage(TArray<bool>& InOutMaterialUsesVertexData, const TArray<UMaterialInterface*>& UniqueMaterials, const UMaterialOptions* MaterialOptions) const
{
	InOutMaterialUsesVertexData.SetNum(UniqueMaterials.Num());
	for (int32 MaterialIndex = 0; MaterialIndex < UniqueMaterials.Num(); ++MaterialIndex)
	{
		UMaterialInterface* Material = UniqueMaterials[MaterialIndex];
		for (const FPropertyEntry& Entry : MaterialOptions->Properties)
		{
			// Don't have to check a property if the result is going to be constant anyway
			if (!Entry.bUseConstantValue && Entry.Property != MP_MAX)
			{
				int32 NumTextureCoordinates;
				bool bUsesVertexData;
				Material->AnalyzeMaterialProperty(Entry.Property, NumTextureCoordinates, bUsesVertexData);

				if (bUsesVertexData || NumTextureCoordinates > 1)
				{
					InOutMaterialUsesVertexData[MaterialIndex] = true;
					break;
				}
			}
		}
	}
}

void FMeshMergeUtilities::ConvertOutputToFlatMaterials(const TArray<FBakeOutput>& BakeOutputs, const TArray<FMaterialData>& MaterialData, TArray<FFlattenMaterial> &FlattenedMaterials) const
{
	for (int32 OutputIndex = 0; OutputIndex < BakeOutputs.Num(); ++OutputIndex)
	{
		const FBakeOutput& Output = BakeOutputs[OutputIndex];
		const FMaterialData& MaterialInfo = MaterialData[OutputIndex];

		FFlattenMaterial Material;		

		for (TPair<EMaterialProperty, FIntPoint> SizePair : Output.PropertySizes)
		{
			EFlattenMaterialProperties OldProperty = NewToOldProperty(SizePair.Key);
			Material.SetPropertySize(OldProperty, SizePair.Value);
			Material.GetPropertySamples(OldProperty).Append(Output.PropertyData[SizePair.Key]);
		}

		Material.bDitheredLODTransition = MaterialInfo.Material->IsDitheredLODTransition();
		Material.BlendMode = BLEND_Opaque;
		Material.bTwoSided = MaterialInfo.Material->IsTwoSided();
		Material.EmissiveScale = Output.EmissiveScale;

		FlattenedMaterials.Add(Material);
	}
}

EFlattenMaterialProperties FMeshMergeUtilities::NewToOldProperty(int32 NewProperty) const
{
	const EFlattenMaterialProperties Remap[MP_Refraction] =
	{
		EFlattenMaterialProperties::Emissive,
		EFlattenMaterialProperties::Opacity,
		EFlattenMaterialProperties::OpacityMask,
		EFlattenMaterialProperties::NumFlattenMaterialProperties,
		EFlattenMaterialProperties::NumFlattenMaterialProperties,
		EFlattenMaterialProperties::Diffuse,
		EFlattenMaterialProperties::Metallic,
		EFlattenMaterialProperties::Specular,
		EFlattenMaterialProperties::Roughness,
		EFlattenMaterialProperties::Normal,
		EFlattenMaterialProperties::NumFlattenMaterialProperties,
		EFlattenMaterialProperties::NumFlattenMaterialProperties,
		EFlattenMaterialProperties::NumFlattenMaterialProperties,
		EFlattenMaterialProperties::NumFlattenMaterialProperties,
		EFlattenMaterialProperties::NumFlattenMaterialProperties,
		EFlattenMaterialProperties::NumFlattenMaterialProperties,
		EFlattenMaterialProperties::AmbientOcclusion
	};
	
	return Remap[NewProperty];
}

UMaterialOptions* FMeshMergeUtilities::PopulateMaterialOptions(const FMaterialProxySettings& MaterialSettings) const
{
	UMaterialOptions* MaterialOptions = DuplicateObject(GetMutableDefault<UMaterialOptions>(), GetTransientPackage());
	MaterialOptions->Properties.Empty();	
	MaterialOptions->TextureSize = MaterialSettings.TextureSize;
	
	const bool bCustomSizes = MaterialSettings.TextureSizingType == TextureSizingType_UseManualOverrideTextureSize;

	FPropertyEntry Property;
	PopulatePropertyEntry(MaterialSettings, MP_BaseColor, Property);
	MaterialOptions->Properties.Add(Property);

	PopulatePropertyEntry(MaterialSettings, MP_Specular, Property);
	if (MaterialSettings.bSpecularMap)
		MaterialOptions->Properties.Add(Property);

	PopulatePropertyEntry(MaterialSettings, MP_Roughness, Property);
	if (MaterialSettings.bRoughnessMap)
		MaterialOptions->Properties.Add(Property);

	PopulatePropertyEntry(MaterialSettings, MP_Metallic, Property);
	if (MaterialSettings.bMetallicMap)
		MaterialOptions->Properties.Add(Property);

	PopulatePropertyEntry(MaterialSettings, MP_Normal, Property);
	if (MaterialSettings.bNormalMap)
		MaterialOptions->Properties.Add(Property);

	PopulatePropertyEntry(MaterialSettings, MP_Opacity, Property);
	if (MaterialSettings.bOpacityMap)
		MaterialOptions->Properties.Add(Property);

	PopulatePropertyEntry(MaterialSettings, MP_OpacityMask, Property);
	if (MaterialSettings.bOpacityMaskMap)
		MaterialOptions->Properties.Add(Property);

	PopulatePropertyEntry(MaterialSettings, MP_EmissiveColor, Property);
	if (MaterialSettings.bEmissiveMap)
		MaterialOptions->Properties.Add(Property);

	PopulatePropertyEntry(MaterialSettings, MP_AmbientOcclusion, Property);
	if (MaterialSettings.bAmbientOcclusionMap)
		MaterialOptions->Properties.Add(Property);

	return MaterialOptions;
}

void FMeshMergeUtilities::PopulatePropertyEntry(const FMaterialProxySettings& MaterialSettings, EMaterialProperty MaterialProperty, FPropertyEntry& InOutPropertyEntry) const
{
	InOutPropertyEntry.Property = MaterialProperty;
	switch (MaterialSettings.TextureSizingType)
	{	
		/** Set property output size to unique per-property user set sizes */
		case TextureSizingType_UseManualOverrideTextureSize:
		{
			InOutPropertyEntry.bUseCustomSize = true;
			InOutPropertyEntry.CustomSize = [MaterialSettings, MaterialProperty]() -> FIntPoint
			{
				switch (MaterialProperty)
				{
					case MP_BaseColor: return MaterialSettings.DiffuseTextureSize;
					case MP_Specular: return MaterialSettings.SpecularTextureSize;
					case MP_Roughness: return MaterialSettings.RoughnessTextureSize;
					case MP_Metallic: return MaterialSettings.MetallicTextureSize;
					case MP_Normal: return MaterialSettings.NormalTextureSize;
					case MP_Opacity: return MaterialSettings.OpacityTextureSize;
					case MP_OpacityMask: return MaterialSettings.OpacityMaskTextureSize;
					case MP_EmissiveColor: return MaterialSettings.EmissiveTextureSize;
					case MP_AmbientOcclusion: return MaterialSettings.AmbientOcclusionTextureSize;
					default:
					{
						checkf(false, TEXT("Invalid Material Property"));
						return FIntPoint();
					}	
				}
			}();

			break;
		}
		/** Set property output size to biased values off the TextureSize value (Normal at fullres, Diffuse at halfres, and anything else at quarter res */
		case TextureSizingType_UseAutomaticBiasedSizes:
		{
			const FIntPoint FullRes = MaterialSettings.TextureSize;
			const FIntPoint HalfRes = FIntPoint(FMath::Max(8, FullRes.X >> 1), FMath::Max(8, FullRes.Y >> 1));
			const FIntPoint QuarterRes = FIntPoint(FMath::Max(4, FullRes.X >> 2), FMath::Max(4, FullRes.Y >> 2));

			InOutPropertyEntry.bUseCustomSize = true;
			InOutPropertyEntry.CustomSize = [FullRes, HalfRes, QuarterRes, MaterialSettings, MaterialProperty]() -> FIntPoint
			{
				switch (MaterialProperty)
				{
				case MP_Normal: return FullRes;
				case MP_BaseColor: return HalfRes;
				case MP_Specular: return QuarterRes;
				case MP_Roughness: return QuarterRes;
				case MP_Metallic: return QuarterRes;				
				case MP_Opacity: return QuarterRes;
				case MP_OpacityMask: return QuarterRes;
				case MP_EmissiveColor: return QuarterRes;
				case MP_AmbientOcclusion: return QuarterRes;
				default:
				{
					checkf(false, TEXT("Invalid Material Property"));
					return FIntPoint();
				}
				}
			}();

			break;
		}
 		/** Set all sizes to TextureSize */
		case TextureSizingType_UseSingleTextureSize:
		case TextureSizingType_UseSimplygonAutomaticSizing:
		{
			InOutPropertyEntry.bUseCustomSize = false;
			InOutPropertyEntry.CustomSize = MaterialSettings.TextureSize;
			break;
		}
	}
	/** Check whether or not a constant value should be used for this property */
	InOutPropertyEntry.bUseConstantValue = [MaterialSettings, MaterialProperty]() -> bool
	{
		switch (MaterialProperty)
		{
			case MP_BaseColor: return false;
			case MP_Normal: return !MaterialSettings.bNormalMap;
			case MP_Specular: return !MaterialSettings.bSpecularMap;
			case MP_Roughness: return !MaterialSettings.bRoughnessMap;
			case MP_Metallic: return !MaterialSettings.bMetallicMap;
			case MP_Opacity: return !MaterialSettings.bOpacityMap;
			case MP_OpacityMask: return !MaterialSettings.bOpacityMaskMap;
			case MP_EmissiveColor: return !MaterialSettings.bEmissiveMap;
			case MP_AmbientOcclusion: return !MaterialSettings.bAmbientOcclusionMap;
			default:
			{
				checkf(false, TEXT("Invalid Material Property"));
				return false;
			}
		}
	}();
	/** Set the value if a constant value should be used for this property */
	InOutPropertyEntry.ConstantValue = [MaterialSettings, MaterialProperty]() -> float
	{
		switch (MaterialProperty)
		{
			case MP_BaseColor: return 1.0f;
			case MP_Normal: return 1.0f;
			case MP_Specular: return MaterialSettings.SpecularConstant;
			case MP_Roughness: return MaterialSettings.RoughnessConstant;
			case MP_Metallic: return MaterialSettings.MetallicConstant;
			case MP_Opacity: return MaterialSettings.OpacityConstant;
			case MP_OpacityMask: return MaterialSettings.OpacityMaskConstant;
			case MP_EmissiveColor: return 0.0f;
			case MP_AmbientOcclusion: return MaterialSettings.AmbientOcclusionConstant;
			default:
			{
				checkf(false, TEXT("Invalid Material Property"));
				return 1.0f;
			}
		}
	}();
}

void FMeshMergeUtilities::CopyTextureRect(const FColor* Src, const FIntPoint& SrcSize, FColor* Dst, const FIntPoint& DstSize, const FIntPoint& DstPos) const
{
	const int32 RowLength = SrcSize.X * sizeof(FColor);
	FColor* RowDst = Dst + DstSize.X*DstPos.Y;
	const FColor* RowSrc = Src;

	for (int32 RowIdx = 0; RowIdx < SrcSize.Y; ++RowIdx)
	{
		FMemory::Memcpy(RowDst + DstPos.X, RowSrc, RowLength);
		RowDst += DstSize.X;
		RowSrc += SrcSize.X;
	}
}

void FMeshMergeUtilities::SetTextureRect(const FColor& ColorValue, const FIntPoint& SrcSize, FColor* Dst, const FIntPoint& DstSize, const FIntPoint& DstPos) const
{
	FColor* RowDst = Dst + DstSize.X*DstPos.Y;

	for (int32 RowIdx = 0; RowIdx < SrcSize.Y; ++RowIdx)
	{
		for (int32 ColIdx = 0; ColIdx < SrcSize.X; ++ColIdx)
		{
			RowDst[DstPos.X + ColIdx] = ColorValue;
		}

		RowDst += DstSize.X;
	}
}

FIntPoint FMeshMergeUtilities::ConditionalImageResize(const FIntPoint& SrcSize, const FIntPoint& DesiredSize, TArray<FColor>& InOutImage, bool bLinearSpace) const
{
	const int32 NumDesiredSamples = DesiredSize.X*DesiredSize.Y;
	if (InOutImage.Num() && InOutImage.Num() != NumDesiredSamples)
	{
		check(InOutImage.Num() == SrcSize.X*SrcSize.Y);
		TArray<FColor> OutImage;
		if (NumDesiredSamples > 0)
		{
			FImageUtils::ImageResize(SrcSize.X, SrcSize.Y, InOutImage, DesiredSize.X, DesiredSize.Y, OutImage, bLinearSpace);
		}
		Exchange(InOutImage, OutImage);
		return DesiredSize;
	}

	return SrcSize;
}

void FMeshMergeUtilities::MergeFlattenedMaterials(TArray<struct FFlattenMaterial>& InMaterialList, FFlattenMaterial& OutMergedMaterial, TArray<FUVOffsetScalePair>& OutUVTransforms) const
{
	OutUVTransforms.Reserve(InMaterialList.Num());

	// Fill output UV transforms with invalid values
	for (auto Material : InMaterialList)
	{

		// Invalid UV transform
		FUVOffsetScalePair UVTransform;
		UVTransform.Key = FVector2D::ZeroVector;
		UVTransform.Value = FVector2D::ZeroVector;
		OutUVTransforms.Add(UVTransform);
	}

	const int32 AtlasGridSize = FMath::CeilToInt(FMath::Sqrt(InMaterialList.Num()));
	OutMergedMaterial.EmissiveScale = FlattenEmissivescale(InMaterialList);

	for (int32 PropertyIndex = 0; PropertyIndex < (int32)EFlattenMaterialProperties::NumFlattenMaterialProperties; ++PropertyIndex)
	{
		const EFlattenMaterialProperties Property = (EFlattenMaterialProperties)PropertyIndex;
		if (OutMergedMaterial.ShouldGenerateDataForProperty(Property))
		{
			const FIntPoint AtlasTextureSize = OutMergedMaterial.GetPropertySize(Property);
			const FIntPoint ExportTextureSize = AtlasTextureSize / AtlasGridSize;
			const int32 AtlasNumSamples = AtlasTextureSize.X*AtlasTextureSize.Y;
			check(OutMergedMaterial.GetPropertySize(Property) == AtlasTextureSize);
			TArray<FColor>& Samples = OutMergedMaterial.GetPropertySamples(Property);
			Samples.SetNumZeroed(AtlasNumSamples);
		}
	}

	int32 AtlasRowIdx = 0;
	int32 AtlasColIdx = 0;
	FIntPoint GlobalAtlasTargetPos = FIntPoint(0, 0);

	bool bSamplesWritten[(uint32)EFlattenMaterialProperties::NumFlattenMaterialProperties];
	FMemory::Memset(bSamplesWritten, 0);

	// Used to calculate UV transforms
	const FIntPoint GlobalAtlasTextureSize = OutMergedMaterial.GetPropertySize(EFlattenMaterialProperties::Diffuse);
	const FIntPoint GlobalExportTextureSize = GlobalAtlasTextureSize / AtlasGridSize;

	// Flatten all materials and merge them into one material using texture atlases
	for (int32 MatIdx = 0; MatIdx < InMaterialList.Num(); ++MatIdx)
	{
		FFlattenMaterial& FlatMaterial = InMaterialList[MatIdx];
		OutMergedMaterial.bTwoSided |= FlatMaterial.bTwoSided;
		OutMergedMaterial.bDitheredLODTransition = FlatMaterial.bDitheredLODTransition;
		for (int32 PropertyIndex = 0; PropertyIndex < (int32)EFlattenMaterialProperties::NumFlattenMaterialProperties; ++PropertyIndex)
		{
			const EFlattenMaterialProperties Property = (EFlattenMaterialProperties)PropertyIndex;
			const FIntPoint PropertyTextureSize = OutMergedMaterial.GetPropertySize(Property);			
			const int32 NumPropertySamples = PropertyTextureSize.X*PropertyTextureSize.Y;

			const FIntPoint PropertyAtlasTextureSize = PropertyTextureSize / AtlasGridSize;
			const FIntPoint AtlasTargetPos(AtlasColIdx*PropertyAtlasTextureSize.X, AtlasRowIdx*PropertyAtlasTextureSize.Y);
			
			if (OutMergedMaterial.ShouldGenerateDataForProperty(Property) && FlatMaterial.DoesPropertyContainData(Property))
			{
				TArray<FColor>& SourceSamples = FlatMaterial.GetPropertySamples(Property);
				TArray<FColor>& TargetSamples = OutMergedMaterial.GetPropertySamples(Property);
				if (FlatMaterial.IsPropertyConstant(Property))
				{
					SetTextureRect(SourceSamples[0], PropertyAtlasTextureSize, TargetSamples.GetData(), PropertyTextureSize, AtlasTargetPos);
				}
				else
				{
					FIntPoint PropertySize = FlatMaterial.GetPropertySize(Property);
					PropertySize = ConditionalImageResize(PropertySize, PropertyAtlasTextureSize, SourceSamples, false);
					CopyTextureRect(SourceSamples.GetData(), PropertyAtlasTextureSize, TargetSamples.GetData(), PropertyTextureSize, AtlasTargetPos);
					FlatMaterial.SetPropertySize(Property, PropertySize);
				}

				bSamplesWritten[PropertyIndex] |= true;
			}
		}

		check(OutUVTransforms.IsValidIndex(MatIdx));

		OutUVTransforms[MatIdx].Key = FVector2D(
			(float)GlobalAtlasTargetPos.X / GlobalAtlasTextureSize.X,
			(float)GlobalAtlasTargetPos.Y / GlobalAtlasTextureSize.Y);

		OutUVTransforms[MatIdx].Value = FVector2D(
			(float)GlobalExportTextureSize.X / GlobalAtlasTextureSize.X,
			(float)GlobalExportTextureSize.Y / GlobalAtlasTextureSize.Y);

		AtlasColIdx++;
		if (AtlasColIdx >= AtlasGridSize)
		{
			AtlasColIdx = 0;
			AtlasRowIdx++;
		}

		GlobalAtlasTargetPos = FIntPoint(AtlasColIdx*GlobalExportTextureSize.X, AtlasRowIdx*GlobalExportTextureSize.Y);
	}

	// Check if some properties weren't populated with data (which means we can empty them out)
	for (int32 PropertyIndex = 0; PropertyIndex < (int32)EFlattenMaterialProperties::NumFlattenMaterialProperties; ++PropertyIndex)
	{
		if (!bSamplesWritten[PropertyIndex])
		{
			EFlattenMaterialProperties Property = (EFlattenMaterialProperties)PropertyIndex;
			OutMergedMaterial.GetPropertySamples(Property).Empty();
			OutMergedMaterial.SetPropertySize(Property, FIntPoint(0, 0));
		}
	}

}

void FMeshMergeUtilities::FlattenBinnedMaterials(TArray<struct FFlattenMaterial>& InMaterialList, const TArray<FBox2D>& InMaterialBoxes, FFlattenMaterial& OutMergedMaterial, TArray<FUVOffsetScalePair>& OutUVTransforms) const
{
	OutUVTransforms.AddZeroed(InMaterialList.Num());
	// Flatten emissive scale across all incoming materials
	OutMergedMaterial.EmissiveScale = FlattenEmissivescale(InMaterialList);

	// Merge all material properties
	for (int32 Index = 0; Index < (int32)EFlattenMaterialProperties::NumFlattenMaterialProperties; ++Index)
	{
		const EFlattenMaterialProperties Property = (EFlattenMaterialProperties)Index;
		const FIntPoint& OutTextureSize = OutMergedMaterial.GetPropertySize(Property);
		if (OutTextureSize != FIntPoint::ZeroValue)
		{
			TArray<FColor>& OutSamples = OutMergedMaterial.GetPropertySamples(Property);
			OutSamples.Reserve(OutTextureSize.X * OutTextureSize.Y);
			OutSamples.SetNumZeroed(OutTextureSize.X * OutTextureSize.Y);

			bool bMaterialsWritten = false;
			for (int32 MaterialIndex = 0; MaterialIndex < InMaterialList.Num(); ++MaterialIndex)
			{
				// Determine output size and offset
				FFlattenMaterial& FlatMaterial = InMaterialList[MaterialIndex];
				OutMergedMaterial.bDitheredLODTransition |= FlatMaterial.bDitheredLODTransition;
				OutMergedMaterial.bTwoSided |= FlatMaterial.bTwoSided;

				if (FlatMaterial.DoesPropertyContainData(Property))
				{
					const FBox2D MaterialBox = InMaterialBoxes[MaterialIndex];
					const FIntPoint& InputSize = FlatMaterial.GetPropertySize(Property);
					TArray<FColor>& InputSamples = FlatMaterial.GetPropertySamples(Property);

					// Resize material to match output (area) size
					FIntPoint OutputSize = FIntPoint(OutTextureSize.X * MaterialBox.GetSize().X, OutTextureSize.Y * MaterialBox.GetSize().Y);
					ConditionalImageResize(InputSize, OutputSize, InputSamples, false);

					// Copy material data to the merged 'atlas' texture
					FIntPoint OutputPosition = FIntPoint(OutTextureSize.X * MaterialBox.Min.X, OutTextureSize.Y * MaterialBox.Min.Y);
					CopyTextureRect(InputSamples.GetData(), OutputSize, OutSamples.GetData(), OutTextureSize, OutputPosition);

					// Set the UV tranforms only once
					if (Index == 0)
					{
						FUVOffsetScalePair& UVTransform = OutUVTransforms[MaterialIndex];
						UVTransform.Key = MaterialBox.Min;
						UVTransform.Value = MaterialBox.GetSize();
					}

					bMaterialsWritten = true;
				}
			}

			if (!bMaterialsWritten)
			{
				OutSamples.Empty();
				OutMergedMaterial.SetPropertySize(Property, FIntPoint(0, 0));
			}
		}
	}
}


float FMeshMergeUtilities::FlattenEmissivescale(TArray<struct FFlattenMaterial>& InMaterialList) const
{
	// Find maximum emissive scaling value across materials
	float MaxScale = 0.0f;
	for (const FFlattenMaterial& Material : InMaterialList)
	{
		MaxScale = FMath::Max(MaxScale, Material.EmissiveScale);
	}
	
	// Renormalize samples 
	const float Multiplier = 1.0f / MaxScale;
	const int32 NumThreads = [&]()
	{
		return FPlatformProcess::SupportsMultithreading() ? FPlatformMisc::NumberOfCores() : 1;
	}();

	const int32 MaterialsPerThread = FMath::CeilToInt((float)InMaterialList.Num() / (float)NumThreads);
	ParallelFor(NumThreads, [&InMaterialList, MaterialsPerThread, Multiplier, MaxScale]
	(int32 Index)
	{
		int32 StartIndex = FMath::CeilToInt((Index)* MaterialsPerThread);
		const int32 EndIndex = FMath::Min(FMath::CeilToInt((Index + 1) * MaterialsPerThread), InMaterialList.Num());

		for (; StartIndex < EndIndex; ++StartIndex)
		{
			FFlattenMaterial& Material = InMaterialList[StartIndex];
			if (Material.EmissiveScale != MaxScale)
			{
				for (FColor& Sample : Material.GetPropertySamples(EFlattenMaterialProperties::Emissive))
				{
					Sample.R = Sample.R * Multiplier;
					Sample.G = Sample.G * Multiplier;
					Sample.B = Sample.B * Multiplier;
					Sample.A = Sample.A * Multiplier;
				}
			}
		}
	}, NumThreads == 1);

	return MaxScale;
}

void FMeshMergeUtilities::CreateProxyMesh(const TArray<AActor*>& InActors, const struct FMeshProxySettings& InMeshProxySettings, UPackage* InOuter, const FString& InProxyBasePackageName, const FGuid InGuid, const FCreateProxyDelegate& InProxyCreatedDelegate, const bool bAllowAsync, const float ScreenSize) const
{
	IMeshReductionModule& ReductionModule = FModuleManager::Get().LoadModuleChecked<IMeshReductionModule>("MeshReductionInterface");
	// Error/warning checking for input
	if (ReductionModule.GetMeshMergingInterface() == nullptr)
	{
		UE_LOG(LogMeshMerging, Log, TEXT("No automatic mesh merging module available"));
		return;
	}

	// Check that the delegate has a func-ptr bound to it
	if (!InProxyCreatedDelegate.IsBound())
	{
		UE_LOG(LogMeshMerging, Log, TEXT("Invalid (unbound) delegate for returning generated proxy mesh"));
		return;
	}

	// No actors given as input
	if (InActors.Num() == 0)
	{
		UE_LOG(LogMeshMerging, Log, TEXT("No actors specified to generate a proxy mesh for"));
		return;
	}

	// Base asset name for a new assets
	// In case outer is null ProxyBasePackageName has to be long package name
	if (InOuter == nullptr && FPackageName::IsShortPackageName(InProxyBasePackageName))
	{
		UE_LOG(LogMeshMerging, Warning, TEXT("Invalid long package name: '%s'."), *InProxyBasePackageName);
		return;
	}

	FScopedSlowTask SlowTask(100.f, (LOCTEXT("CreateProxyMesh_CreateMesh", "Creating Mesh Proxy")));
	SlowTask.MakeDialog();

	// Retrieve static mesh components valid for merging from the given set of actors	
	TArray<UStaticMeshComponent*> ComponentsToMerge;
	{
		// Collect components to merge
		for (AActor* Actor : InActors)
		{
			TInlineComponentArray<UStaticMeshComponent*> Components;
			Actor->GetComponents<UStaticMeshComponent>(Components);

			// Remove anything non-regular or non-spline static mesh components
			Components.RemoveAll([](UStaticMeshComponent* Val) { return !(Val->GetClass() == UStaticMeshComponent::StaticClass() || Val->IsA(USplineMeshComponent::StaticClass())); });

			ComponentsToMerge.Append(Components);
		}
	}

	// Check if there are actually any static mesh components to merge
	if (ComponentsToMerge.Num() == 0)
	{
		UE_LOG(LogMeshMerging, Log, TEXT("No valid static mesh components found in given set of Actors"));
		return;
	}

	TArray<FRawMeshExt> SourceMeshes;
	//TArray<FSectionInfo> UniqueSections;
	TMap<FMeshIdAndLOD, TArray<int32>> GlobalMaterialMap;
	static const int32 ProxyMeshTargetLODLevel = 0;

	FBoxSphereBounds EstimatedBounds(ForceInitToZero);
	for (const UStaticMeshComponent* StaticMeshComponent : ComponentsToMerge)
	{
		EstimatedBounds = EstimatedBounds + StaticMeshComponent->Bounds;
	}

	static const float FOVRad = 90.0f * (float)PI / 360.0f;
	static const FMatrix ProjectionMatrix = FPerspectiveMatrix(FOVRad, 1920, 1080, 0.01f);
	FHierarchicalLODUtilitiesModule& HLODModule = FModuleManager::LoadModuleChecked<FHierarchicalLODUtilitiesModule>("HierarchicalLODUtilities");
	IHierarchicalLODUtilities* Utilities = HLODModule.GetUtilities();
	float EstimatedDistance = Utilities->CalculateDrawDistanceFromScreenSize(EstimatedBounds.SphereRadius, ScreenSize, ProjectionMatrix);

	SlowTask.EnterProgressFrame(5.0f, LOCTEXT("CreateProxyMesh_CollectingMeshes", "Collecting Input Static Meshes"));

	// Mesh / LOD index	
	TMap<uint32, FRawMesh*> RawMeshLODs;

	TArray<FRawMesh*> RawMeshData;

	// LOD index, <original section index, unique section index>
	TMultiMap<uint32, TPair<uint32, uint32>> UniqueSectionIndexPerLOD;

	// Unique set of sections in mesh
	TArray<FSectionInfo> UniqueSections;
	TArray<FSectionInfo> Sections;
	TMultiMap<uint32, uint32> SectionToMesh;

	int32 SummedLightmapPixels = 0;

	for (const UStaticMeshComponent* StaticMeshComponent : ComponentsToMerge)
	{
		const int32 ScreenSizeBasedLODLevel = Utilities->GetLODLevelForScreenSize(StaticMeshComponent, Utilities->CalculateScreenSizeFromDrawDistance(StaticMeshComponent->Bounds.SphereRadius, ProjectionMatrix, EstimatedDistance));
		const int32 LODIndex = InMeshProxySettings.bCalculateCorrectLODModel ? ScreenSizeBasedLODLevel : 0;
		static const bool bPropagateVertexColours = true;

		// Retrieve mesh data in FRawMesh form
		FRawMesh* RawMesh = new FRawMesh();
		FMeshMergeHelpers::RetrieveMesh(StaticMeshComponent, LODIndex, *RawMesh, bPropagateVertexColours);
		const int32 MeshIndex = RawMeshData.Add(RawMesh);

		// Reset section array for reuse
		Sections.SetNum(0, false);
		// Extract sections for given LOD index from the mesh 
		FMeshMergeHelpers::ExtractSections(StaticMeshComponent, LODIndex, Sections);

		for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
		{
			FSectionInfo& Section = Sections[SectionIndex];
			const int32 UniqueIndex = UniqueSections.AddUnique(Section);
			UniqueSectionIndexPerLOD.Add(MeshIndex, TPair<uint32, uint32>(UniqueIndex, Section.MaterialIndex));

			SectionToMesh.Add(UniqueIndex, MeshIndex);
		}

		int32 LightMapWidth, LightMapHeight;
		StaticMeshComponent->GetLightMapResolution(LightMapWidth, LightMapHeight);
		// Make sure we at least have some lightmap space allocated in case the static mesh is set up with invalid input
		SummedLightmapPixels += FMath::Max(16, LightMapHeight * LightMapWidth);
	}

	TArray<UMaterialInterface*> UniqueMaterials;
	TMultiMap<uint32, uint32> SectionToMaterialMap;
	for (int32 SectionIndex = 0; SectionIndex < UniqueSections.Num(); ++SectionIndex)
	{
		FSectionInfo& Section = UniqueSections[SectionIndex];
		const int32 UniqueIndex = UniqueMaterials.AddUnique(Section.Material);
		SectionToMaterialMap.Add(UniqueIndex, SectionIndex);
	}

	TArray<FMeshData> GlobalMeshSettings;
	TArray<FMaterialData> GlobalMaterialSettings;

	UMaterialOptions* Options = PopulateMaterialOptions(InMeshProxySettings.MaterialSettings);
	TArray<EMaterialProperty> MaterialProperties;
	for (const FPropertyEntry& Entry : Options->Properties )
	{
		if (Entry.Property != MP_MAX)
		{
			MaterialProperties.Add(Entry.Property);
		}
	}

	// Mesh index / ( Mesh relative section index / output index )	
	TMultiMap<uint32, TPair<uint32, uint32>> OutputMaterialsMap;
	for (int32 MaterialIndex = 0; MaterialIndex < UniqueMaterials.Num(); ++MaterialIndex)
	{
		UMaterialInterface* Material = UniqueMaterials[MaterialIndex];

		TArray<uint32> SectionIndices;
		SectionToMaterialMap.MultiFind(MaterialIndex, SectionIndices);

		// Check whether or not this material requires mesh data
		int32 NumTexCoords = 0;
		bool bUseVertexData = false;
		FMaterialUtilities::AnalyzeMaterial(Material, MaterialProperties, NumTexCoords, bUseVertexData);

		FMaterialData MaterialSettings;
		MaterialSettings.Material = Material;

		for (const FPropertyEntry& Entry : Options->Properties)
		{
			if (!Entry.bUseConstantValue && Material->IsPropertyActive(Entry.Property) && Entry.Property != MP_MAX)
			{
				MaterialSettings.PropertySizes.Add(Entry.Property, Entry.bUseCustomSize ? Entry.CustomSize : Options->TextureSize);
			}
		}

		if (bUseVertexData || NumTexCoords != 0)
		{
			for (uint32 SectionIndex : SectionIndices)
			{
				TArray<uint32> MeshIndices;
				SectionToMesh.MultiFind(SectionIndex, MeshIndices);

				for (uint32 MeshIndex : MeshIndices)
				{
					FMeshData MeshSettings;
					// Add entries for each used mesh
					MeshSettings.RawMesh = RawMeshData[MeshIndex];

					// If we already have lightmap uvs generated or the lightmap coordinate index != 0 and available we can reuse those instead of having to generate new ones
					if (ComponentsToMerge[MeshIndex]->GetStaticMesh()->SourceModels[0].BuildSettings.bGenerateLightmapUVs || (ComponentsToMerge[MeshIndex]->GetStaticMesh()->LightMapCoordinateIndex != 0 && MeshSettings.RawMesh->WedgeTexCoords[ComponentsToMerge[MeshIndex]->GetStaticMesh()->LightMapCoordinateIndex].Num() != 0))
					{
						MeshSettings.CustomTextureCoordinates = MeshSettings.RawMesh->WedgeTexCoords[ComponentsToMerge[MeshIndex]->GetStaticMesh()->LightMapCoordinateIndex];
						ScaleTextureCoordinatesToBox(FBox2D(FVector2D::ZeroVector, FVector2D(1, 1)), MeshSettings.CustomTextureCoordinates);
					}
					else
					{
						IMeshUtilities& MeshUtilities = FModuleManager::LoadModuleChecked<IMeshUtilities>("MeshUtilities");
						// Generate unique UVs for mesh (should only be done if needed)
						MeshUtilities.GenerateUniqueUVsForStaticMesh(*MeshSettings.RawMesh, Options->TextureSize.GetMax(), MeshSettings.CustomTextureCoordinates);
						ScaleTextureCoordinatesToBox(FBox2D(FVector2D::ZeroVector, FVector2D(1, 1)), MeshSettings.CustomTextureCoordinates);
					}

					MeshSettings.TextureCoordinateBox = FBox2D(MeshSettings.CustomTextureCoordinates);

					// Section index is a unique one so we need to map it to the mesh's equivalent(s)
					TArray<TPair<uint32, uint32>> UniqueToMeshSectionIndices;
					UniqueSectionIndexPerLOD.MultiFind(MeshIndex, UniqueToMeshSectionIndices);
					for (const TPair<uint32, uint32> IndexPair : UniqueToMeshSectionIndices)
					{
						if (IndexPair.Key == SectionIndex)
						{
							MeshSettings.MaterialIndices.Add(IndexPair.Value);
						}
					}

					// Retrieve lightmap for usage of lightmap data 
					const UStaticMeshComponent* StaticMeshComponent = ComponentsToMerge[MeshIndex];
					if (StaticMeshComponent->LODData.IsValidIndex(0))
					{
						const FStaticMeshComponentLODInfo& ComponentLODInfo = StaticMeshComponent->LODData[0];
						const FMeshMapBuildData* MeshMapBuildData = StaticMeshComponent->GetMeshMapBuildData(ComponentLODInfo);
						if (MeshMapBuildData)
						{
							MeshSettings.LightMap = MeshMapBuildData->LightMap;
							MeshSettings.LightMapIndex = StaticMeshComponent->GetStaticMesh()->LightMapCoordinateIndex;
						}
					}

					// For each original material index add an entry to the corresponding LOD and bake output index
					for (int32 Index : MeshSettings.MaterialIndices)
					{
						OutputMaterialsMap.Add(MeshIndex, TPair<uint32, uint32>(Index, GlobalMeshSettings.Num()));
					}

					GlobalMeshSettings.Add(MeshSettings);
					GlobalMaterialSettings.Add(MaterialSettings);
				}
			}
		}
		else
		{
			// Add simple bake entry 
			FMeshData MeshSettings;
			MeshSettings.RawMesh = nullptr;
			MeshSettings.TextureCoordinateBox = FBox2D(FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f));
			MeshSettings.TextureCoordinateIndex = 0;

			// For each original material index add an entry to the corresponding LOD and bake output index 
			for (uint32 SectionIndex : SectionIndices)
			{
				TArray<uint32> MeshIndices;
				SectionToMesh.MultiFind(SectionIndex, MeshIndices);

				for (uint32 MeshIndex : MeshIndices)
				{
					TArray<TPair<uint32, uint32>> UniqueToMeshSectionIndices;
					UniqueSectionIndexPerLOD.MultiFind(MeshIndex, UniqueToMeshSectionIndices);
					for (const TPair<uint32, uint32> IndexPair : UniqueToMeshSectionIndices)
					{
						if (IndexPair.Key == SectionIndex)
						{
							OutputMaterialsMap.Add(MeshIndex, TPair<uint32, uint32>(IndexPair.Value, GlobalMeshSettings.Num()));
						}
					}
				}
			}

			GlobalMeshSettings.Add(MeshSettings);
			GlobalMaterialSettings.Add(MaterialSettings);
		}
	}

	TArray<FMeshData*> MeshSettingPtrs;
	for (int32 SettingsIndex = 0; SettingsIndex < GlobalMeshSettings.Num(); ++SettingsIndex)
	{
		MeshSettingPtrs.Add(&GlobalMeshSettings[SettingsIndex]);
	}

	TArray<FMaterialData*> MaterialSettingPtrs;
	for (int32 SettingsIndex = 0; SettingsIndex < GlobalMaterialSettings.Num(); ++SettingsIndex)
	{
		MaterialSettingPtrs.Add(&GlobalMaterialSettings[SettingsIndex]);
	}

	TArray<FBakeOutput> BakeOutputs;
	IMaterialBakingModule& Module = FModuleManager::Get().LoadModuleChecked<IMaterialBakingModule>("MaterialBaking");
	Module.BakeMaterials(MaterialSettingPtrs, MeshSettingPtrs, BakeOutputs);

	// Append constant properties ?
	TArray<FColor> ConstantData;
	FIntPoint ConstantSize(1, 1);
	for (const FPropertyEntry& Entry : Options->Properties)
	{
		if (Entry.bUseConstantValue && Entry.Property != MP_MAX)
		{
			ConstantData.SetNum(1, false);
			ConstantData[0] = FColor(Entry.ConstantValue * 255.0f, Entry.ConstantValue * 255.0f, Entry.ConstantValue * 255.0f);
			for (FBakeOutput& Ouput : BakeOutputs)
			{
				Ouput.PropertyData.Add(Entry.Property, ConstantData);
				Ouput.PropertySizes.Add(Entry.Property, ConstantSize);
			}
		}
	}

	// Now have the baked out material data, need to have a map or actually remap the raw mesh data to baked material indices
	for (int32 MeshIndex = 0; MeshIndex < RawMeshData.Num(); ++MeshIndex)
	{
		FRawMesh& RawMesh = *RawMeshData[MeshIndex];

		TArray<TPair<uint32, uint32>> SectionAndOutputIndices;
		OutputMaterialsMap.MultiFind(MeshIndex, SectionAndOutputIndices);

		TArray<int32> Remap;
		// Reorder loops 
		for (const TPair<uint32, uint32>& IndexPair : SectionAndOutputIndices)
		{
			const int32 SectionIndex = IndexPair.Key;
			const int32 NewIndex = IndexPair.Value;

			if (Remap.Num() < (SectionIndex + 1))
			{
				Remap.SetNum(SectionIndex + 1);
			}

			Remap[SectionIndex] = NewIndex;
		}

		for (int32& FaceMaterialIndex : RawMesh.FaceMaterialIndices)
		{
			checkf(Remap.IsValidIndex(FaceMaterialIndex), TEXT("Missing material bake output index entry for mesh(section)"));
			FaceMaterialIndex = Remap[FaceMaterialIndex];
		}
	}

	// Landscape culling
	TArray<FRawMesh*> CullingRawMeshes;
	if (InMeshProxySettings.bUseLandscapeCulling)
	{
		SlowTask.EnterProgressFrame(5.0f, LOCTEXT("CreateProxyMesh_LandscapeCulling", "Applying Landscape Culling"));
		UWorld* InWorld = InActors[0]->GetWorld();
		FMeshMergeHelpers::RetrieveCullingLandscapeAndVolumes(InWorld, EstimatedBounds, InMeshProxySettings.LandscapeCullingPrecision, CullingRawMeshes);
	}

	// Allocate merge complete data
	FMergeCompleteData* Data = new FMergeCompleteData();
	Data->InOuter = InOuter;
	Data->InProxySettings = InMeshProxySettings;
	Data->ProxyBasePackageName = InProxyBasePackageName;
	Data->CallbackDelegate = InProxyCreatedDelegate;

	// Lightmap resolution
	if (InMeshProxySettings.bComputeLightMapResolution)
	{
		Data->InProxySettings.LightMapResolution = FMath::CeilToInt(FMath::Sqrt(SummedLightmapPixels));
	}

	// Add this proxy job to map	
	Processor->AddProxyJob(InGuid, Data);

	// We are only using LOD level 0 (ProxyMeshTargetLODLevel)
	TArray<FMeshMergeData> MergeDataEntries;
	for (int32 Index = 0; Index < RawMeshData.Num(); ++Index)
	{
		FMeshMergeData MergeData;
		MergeData.SourceStaticMesh = ComponentsToMerge[Index]->GetStaticMesh();
		MergeData.RawMesh = RawMeshData[Index];
		MergeData.bIsClippingMesh = false;		

		FMeshMergeHelpers::CalculateTextureCoordinateBoundsForRawMesh(*MergeData.RawMesh, MergeData.TexCoordBounds);
		
		FMeshData* MeshData = GlobalMeshSettings.FindByPredicate([&](const FMeshData& Entry)
		{
			return Entry.RawMesh == MergeData.RawMesh && (Entry.CustomTextureCoordinates.Num() || Entry.TextureCoordinateIndex != 0);
		});

		if (MeshData)
		{
			if (MeshData->CustomTextureCoordinates.Num())
			{
				MergeData.NewUVs = MeshData->CustomTextureCoordinates;
			}
			else
			{
				MergeData.NewUVs = MeshData->RawMesh->WedgeTexCoords[MeshData->TextureCoordinateIndex];
			}
			MergeData.TexCoordBounds[0] = FBox2D(FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f));
		}
		MergeDataEntries.Add(MergeData);
	}

	// Populate landscape clipping geometry
	for (FRawMesh* RawMesh : CullingRawMeshes)
	{
		FMeshMergeData ClipData;
		ClipData.bIsClippingMesh = true;
		ClipData.RawMesh = RawMesh;
		MergeDataEntries.Add(ClipData);
	}

	TArray<FFlattenMaterial> FlattenedMaterials;
	ConvertOutputToFlatMaterials(BakeOutputs, GlobalMaterialSettings, FlattenedMaterials);

	SlowTask.EnterProgressFrame(50.0f, LOCTEXT("CreateProxyMesh_GenerateProxy", "Generating Proxy Mesh"));
	// Choose Simplygon Swarm (if available) or local proxy lod method
	if (ReductionModule.GetDistributedMeshMergingInterface() != nullptr && GetDefault<UEditorPerProjectUserSettings>()->bUseSimplygonSwarm && bAllowAsync)
	{
		ReductionModule.GetDistributedMeshMergingInterface()->ProxyLOD(MergeDataEntries, Data->InProxySettings, FlattenedMaterials, InGuid);
	}
	else
	{
		ReductionModule.GetMeshMergingInterface()->ProxyLOD(MergeDataEntries, Data->InProxySettings, FlattenedMaterials, InGuid);
		Processor->Tick(0); // make sure caller gets merging results
	}

	for (FMeshMergeData& DataToRelease : MergeDataEntries)
	{
		DataToRelease.ReleaseData();
	}
}


void FMeshMergeUtilities::MergeComponentsToStaticMesh(const TArray<UPrimitiveComponent*>& ComponentsToMerge, UWorld* World, const FMeshMergingSettings& InSettings, UPackage* InOuter, const FString& InBasePackageName, TArray<UObject*>& OutAssetsToSync, FVector& OutMergedActorLocation, const float ScreenSize, bool bSilent /*= false*/) const
{
	// Use first mesh for naming and pivot
	bool bFirstMesh = true;
	FString MergedAssetPackageName;
	FVector MergedAssetPivot;

	TArray<UStaticMeshComponent*> StaticMeshComponentsToMerge;

	for (int32 MeshId = 0; MeshId < ComponentsToMerge.Num(); ++MeshId)
	{
		UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(ComponentsToMerge[MeshId]);
		if (MeshComponent)
		{
			StaticMeshComponentsToMerge.Add(MeshComponent);

		// Save the pivot and asset package name of the first mesh, will later be used for creating merged mesh asset 
		if (bFirstMesh)
		{
			// Mesh component pivot point
			MergedAssetPivot = InSettings.bPivotPointAtZero ? FVector::ZeroVector : MeshComponent->GetComponentTransform().GetLocation();
			// Source mesh asset package name
			MergedAssetPackageName = MeshComponent->GetStaticMesh()->GetOutermost()->GetName();

				bFirstMesh = false;
			}
		}
	}

	// Nothing to do if no StaticMeshComponents
	if (StaticMeshComponentsToMerge.Num() == 0)
	{
		return;
	}

	FMeshMergeDataTracker DataTracker;

	const bool bMergeAllLODs = InSettings.LODSelectionType == EMeshLODSelectionType::AllLODs;
	const bool bMergeMaterialData = InSettings.bMergeMaterials && InSettings.LODSelectionType != EMeshLODSelectionType::AllLODs;
	const bool bPropagateMeshData = InSettings.bBakeVertexDataToMesh || (bMergeMaterialData && InSettings.bUseVertexDataForBakingMaterial);

	TArray<FStaticMeshComponentAdapter> Adapters;

	TArray<FSectionInfo> Sections;
	if (bMergeAllLODs)
	{
		for (int32 ComponentIndex = 0; ComponentIndex < StaticMeshComponentsToMerge.Num(); ++ComponentIndex)
		{
			UStaticMeshComponent* Component = StaticMeshComponentsToMerge[ComponentIndex];
			Adapters.Add(FStaticMeshComponentAdapter(Component));
			FStaticMeshComponentAdapter& Adapter = Adapters.Last();
			
			if (InSettings.bComputedLightMapResolution)
			{
				int32 LightMapHeight, LightMapWidth;
				if (Component->GetLightMapResolution(LightMapWidth, LightMapHeight))
				{
					DataTracker.AddLightMapPixels(LightMapWidth * LightMapHeight);
				}
			}			

			const int32 NumLODs = Adapter.GetNumberOfLODs();
			for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
			{
				FRawMesh& RawMesh = DataTracker.AddAndRetrieveRawMesh(ComponentIndex, LODIndex);
				Adapter.RetrieveRawMeshData(LODIndex, RawMesh, bPropagateMeshData);

				// Reset section for reuse
				Sections.SetNum(0, false);

				// Extract sections for given LOD index from the mesh 
				Adapter.RetrieveMeshSections(LODIndex, Sections);

				for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
				{
					const FSectionInfo& Section = Sections[SectionIndex];
					const int32 UniqueIndex = DataTracker.AddSection(Section);
					DataTracker.AddSectionRemapping(ComponentIndex, LODIndex, Section.MaterialIndex, UniqueIndex);
					DataTracker.AddMaterialSlotName(Section.Material, Section.MaterialSlotName);
					for (int32 StartIndex = Section.StartIndex; StartIndex < Section.EndIndex; ++StartIndex)
					{
						RawMesh.FaceMaterialIndices[StartIndex] = UniqueIndex;
					}
				}

				if (InSettings.bUseLandscapeCulling)
				{
					FMeshMergeHelpers::CullTrianglesFromVolumesAndUnderLandscapes(Component->GetWorld(), Adapter.GetBounds(), RawMesh);
				}

				bool bValidMesh = RawMesh.IsValid();

				if (!bValidMesh)
				{
					DataTracker.RemoveRawMesh(ComponentIndex, LODIndex);
					break;
				}
				else if(Component->GetStaticMesh() != nullptr)
				{
					// If the mesh is valid at this point, record the lightmap UV so we have a record for use later
					DataTracker.AddLightmapChannelRecord(ComponentIndex, LODIndex, Component->GetStaticMesh()->LightMapCoordinateIndex);
				}

				DataTracker.AddLODIndex(LODIndex);
			}
		}
	}
	else
	{
		// Retrieve HLOD module for calculating LOD index from screen size
		FHierarchicalLODUtilitiesModule& Module = FModuleManager::LoadModuleChecked<FHierarchicalLODUtilitiesModule>("HierarchicalLODUtilities");
		IHierarchicalLODUtilities* Utilities = Module.GetUtilities();

		// Adding LOD 0 for merged mesh output
		DataTracker.AddLODIndex(0);

		// Retrieve mesh and section data for each component
		for (int32 ComponentIndex = 0; ComponentIndex < ComponentsToMerge.Num(); ++ComponentIndex)
		{
			// Create material merge adapter for this component
			UStaticMeshComponent* Component = StaticMeshComponentsToMerge[ComponentIndex];
			Adapters.Add(FStaticMeshComponentAdapter(Component));
			FStaticMeshComponentAdapter& Adapter = Adapters.Last();

			// Determine LOD to use for merging, either user specified or calculated index and ensure we clamp to the maximum LOD index for this adapter 
			const int32 LODIndex = [&]()
			{
				if (InSettings.LODSelectionType == EMeshLODSelectionType::SpecificLOD)
				{
					return FMath::Min(Adapter.GetNumberOfLODs() - 1, InSettings.SpecificLOD);
				}
				else
				{
					return FMath::Min(Adapter.GetNumberOfLODs() - 1, Utilities->GetLODLevelForScreenSize(Component, FMath::Clamp(ScreenSize, 0.0f, 1.0f)));
				}
			}();

			// Retrieve raw mesh data
			FRawMesh& RawMesh = DataTracker.AddAndRetrieveRawMesh(ComponentIndex, LODIndex);
			Adapter.RetrieveRawMeshData(LODIndex, RawMesh, bPropagateMeshData);

			// Reset section for reuse
			Sections.SetNum(0, false);

			// Extract sections for given LOD index from the mesh 
			Adapter.RetrieveMeshSections(LODIndex, Sections);

			for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
			{
				const FSectionInfo& Section = Sections[SectionIndex];
				// Unique section index for remapping
				const int32 UniqueIndex = DataTracker.AddSection(Section);

				// Store of original to unique section index entry for this component + LOD index
				DataTracker.AddSectionRemapping(ComponentIndex, LODIndex, Section.MaterialIndex, UniqueIndex);
				DataTracker.AddMaterialSlotName(Section.Material, Section.MaterialSlotName);

				if (!bMergeMaterialData)
				{
					for (int32 StartIndex = Section.StartIndex; StartIndex < Section.EndIndex; ++StartIndex)
					{
						RawMesh.FaceMaterialIndices[StartIndex] = UniqueIndex;
					}
				}
			}

			if (InSettings.bUseLandscapeCulling)
			{
				FMeshMergeHelpers::CullTrianglesFromVolumesAndUnderLandscapes(Component->GetWorld(), Adapter.GetBounds(), RawMesh);
			}

			// If the valid became invalid during retrieval remove it again
			const bool bValidMesh = RawMesh.IsValid();
			if (!bValidMesh)
			{
				DataTracker.RemoveRawMesh(ComponentIndex, LODIndex);
			}
			else if(Component->GetStaticMesh() != nullptr)
			{
				// If the mesh is valid at this point, record the lightmap UV so we have a record for use later
				DataTracker.AddLightmapChannelRecord(ComponentIndex, LODIndex, Component->GetStaticMesh()->LightMapCoordinateIndex);
			}
		}
	}

	DataTracker.ProcessRawMeshes();

	// Retrieve physics data
	UBodySetup* BodySetupSource = nullptr;
	TArray<FKAggregateGeom> PhysicsGeometry;
	if (InSettings.bMergePhysicsData)
	{
		ExtractPhysicsDataFromComponents(ComponentsToMerge, PhysicsGeometry, BodySetupSource);
	}

	// Find all unique materials and remap section to unique materials
	TArray<UMaterialInterface*> UniqueMaterials;
	TMap<UMaterialInterface*, int32> MaterialIndices;
	TMultiMap<uint32, uint32> SectionToMaterialMap;

	for (int32 SectionIndex = 0; SectionIndex < DataTracker.NumberOfUniqueSections(); ++SectionIndex)
	{
		// Unique index for material
		const int32 UniqueIndex = UniqueMaterials.AddUnique(DataTracker.GetMaterialForSectionIndex(SectionIndex));

		// Store off usage of unique material by unique sections
		SectionToMaterialMap.Add(UniqueIndex, SectionIndex);
	}

	// For each unique material calculate how 'important' they are
	TArray<float> MaterialImportanceValues;
	FMaterialUtilities::DetermineMaterialImportance(UniqueMaterials, MaterialImportanceValues);

	// If the user wants to merge materials into a single one
	if (bMergeMaterialData)
	{
		UMaterialOptions* MaterialOptions = PopulateMaterialOptions(InSettings.MaterialSettings);
		// Check each material to see if the shader actually uses vertex data and collect flags
		TArray<bool> bMaterialUsesVertexData;
		DetermineMaterialVertexDataUsage(bMaterialUsesVertexData, UniqueMaterials, MaterialOptions);

		TArray<FMeshData> GlobalMeshSettings;
		TArray<FMaterialData> GlobalMaterialSettings;
		TArray<float> SectionMaterialImportanceValues;

		TMultiMap< FMeshLODKey, MaterialRemapPair > OutputMaterialsMap;

		TMap<EMaterialProperty, FIntPoint> PropertySizes;
		for (const FPropertyEntry& Entry : MaterialOptions->Properties)
		{
			if (!Entry.bUseConstantValue && Entry.Property != MP_MAX)
			{
				PropertySizes.Add(Entry.Property, Entry.bUseCustomSize ? Entry.CustomSize : MaterialOptions->TextureSize);
			}
		}

		TMap<UMaterialInterface*, int32> MaterialToDefaultMeshData;

		for (TConstRawMeshIterator RawMeshIterator = DataTracker.GetConstRawMeshIterator(); RawMeshIterator; ++RawMeshIterator)
		{
			const FMeshLODKey& Key = RawMeshIterator.Key();
			const FRawMesh& RawMesh = RawMeshIterator.Value();
			const bool bRequiresUniqueUVs = DataTracker.DoesMeshLODRequireUniqueUVs(Key);

			// Retrieve all sections and materials for key
			TArray<SectionRemapPair> SectionRemapPairs;
			DataTracker.GetMappingsForMeshLOD(Key, SectionRemapPairs);

			// Contains unique materials used for this key, and the accompanying section index which point to the material
			TMap<UMaterialInterface*, TArray<int32>> MaterialAndSectionIndices;

			for (const SectionRemapPair& RemapPair : SectionRemapPairs)
			{
				const int32 UniqueIndex = RemapPair.Value;
				const int32 SectionIndex = RemapPair.Key;
				TArray<int32>& SectionIndices = MaterialAndSectionIndices.FindOrAdd(DataTracker.GetMaterialForSectionIndex(UniqueIndex));
				SectionIndices.Add(SectionIndex);
			}

			// Cache unique texture coordinates
			TArray<FVector2D> UniqueTextureCoordinates;

			for (TPair<UMaterialInterface*, TArray<int32>>& MaterialSectionIndexPair : MaterialAndSectionIndices)
			{
				UMaterialInterface* Material = MaterialSectionIndexPair.Key;
				const int32 MaterialIndex = UniqueMaterials.IndexOfByKey(Material);
				const TArray<int32>& SectionIndices = MaterialSectionIndexPair.Value;
				const bool bDoesMaterialUseVertexData = bMaterialUsesVertexData[MaterialIndex];

				FMaterialData MaterialData;
				MaterialData.Material = Material;
				MaterialData.PropertySizes = PropertySizes;

				FMeshData MeshData;
				int32 MeshDataIndex = 0;

				if (InSettings.bUseVertexDataForBakingMaterial && (bDoesMaterialUseVertexData || bRequiresUniqueUVs))
				{
					MeshData.RawMesh = DataTracker.GetRawMeshPtr(Key);
					// if it has vertex color/*WedgetColors.Num()*/, it should also use light map UV index
					// we can't do this for all meshes, but only for the mesh that has vertex color.
					if (bRequiresUniqueUVs || MeshData.RawMesh->WedgeColors.Num() > 0)
					{
						// Check if there are lightmap uvs available?
						const int32 LightMapUVIndex = StaticMeshComponentsToMerge[Key.GetMeshIndex()]->GetStaticMesh()->LightMapCoordinateIndex;

						if (MeshData.RawMesh->WedgeTexCoords[LightMapUVIndex].Num())
						{
							MeshData.TextureCoordinateIndex = LightMapUVIndex;
						}
						else
						{
							if (!UniqueTextureCoordinates.Num())
							{
								IMeshUtilities& MeshUtilities = FModuleManager::LoadModuleChecked<IMeshUtilities>("MeshUtilities");
								MeshUtilities.GenerateUniqueUVsForStaticMesh(*MeshData.RawMesh, MaterialOptions->TextureSize.GetMax(), UniqueTextureCoordinates);
								ScaleTextureCoordinatesToBox(FBox2D(FVector2D::ZeroVector, FVector2D(1, 1)), UniqueTextureCoordinates);
							}
							MeshData.CustomTextureCoordinates = UniqueTextureCoordinates;
						}
					}

					MeshData.TextureCoordinateBox = FBox2D(FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f));
					MeshData.MaterialIndices = SectionIndices;
					MeshDataIndex = GlobalMeshSettings.Num();

					Adapters[Key.GetMeshIndex()].ApplySettings(Key.GetLODIndex(), MeshData);

					GlobalMeshSettings.Add(MeshData);
					GlobalMaterialSettings.Add(MaterialData);
					SectionMaterialImportanceValues.Add(MaterialImportanceValues[MaterialIndex]);
				}
				else
				{
					MeshData.RawMesh = nullptr;
					MeshData.TextureCoordinateBox = FBox2D(FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f));

					// This prevents baking out the same material multiple times, which would be wasteful when it does not use vertex data anyway
					const bool bPreviouslyAdded = MaterialToDefaultMeshData.Contains(Material);
					int32& DefaultMeshDataIndex = MaterialToDefaultMeshData.FindOrAdd(Material);

					if (!bPreviouslyAdded)
					{
						DefaultMeshDataIndex = GlobalMeshSettings.Num();
						GlobalMeshSettings.Add(MeshData);
						GlobalMaterialSettings.Add(MaterialData);
						SectionMaterialImportanceValues.Add(MaterialImportanceValues[MaterialIndex]);
					}

					MeshDataIndex = DefaultMeshDataIndex;
				}

				for (const uint32& OriginalSectionIndex : SectionIndices)
				{
					OutputMaterialsMap.Add(Key, MaterialRemapPair(OriginalSectionIndex, MeshDataIndex));
				}
			}
		}

		TArray<FMeshData*> MeshSettingPtrs;
		for (int32 SettingsIndex = 0; SettingsIndex < GlobalMeshSettings.Num(); ++SettingsIndex)
		{
			MeshSettingPtrs.Add(&GlobalMeshSettings[SettingsIndex]);
		}

		TArray<FMaterialData*> MaterialSettingPtrs;
		for (int32 SettingsIndex = 0; SettingsIndex < GlobalMaterialSettings.Num(); ++SettingsIndex)
		{
			MaterialSettingPtrs.Add(&GlobalMaterialSettings[SettingsIndex]);
		}

		TArray<FBakeOutput> BakeOutputs;
		IMaterialBakingModule& Module = FModuleManager::Get().LoadModuleChecked<IMaterialBakingModule>("MaterialBaking");
		Module.BakeMaterials(MaterialSettingPtrs, MeshSettingPtrs, BakeOutputs);

		// Append constant properties ?
		TArray<FColor> ConstantData;
		FIntPoint ConstantSize(1, 1);
		for (const FPropertyEntry& Entry : MaterialOptions->Properties)
		{
			if (Entry.bUseConstantValue && Entry.Property != MP_MAX)
			{
				ConstantData.SetNum(1, false);
				ConstantData[0] = FLinearColor(Entry.ConstantValue, Entry.ConstantValue, Entry.ConstantValue).ToFColor(true);
				for (FBakeOutput& Ouput : BakeOutputs)
				{
					Ouput.PropertyData.Add(Entry.Property, ConstantData);
					Ouput.PropertySizes.Add(Entry.Property, ConstantSize);
				}
			}
		}

		TArray<FFlattenMaterial> FlattenedMaterials;
		ConvertOutputToFlatMaterials(BakeOutputs, GlobalMaterialSettings, FlattenedMaterials);

		// Try to optimize materials where possible	
		for (FFlattenMaterial& InMaterial : FlattenedMaterials)
		{
			FMaterialUtilities::OptimizeFlattenMaterial(InMaterial);
		}

		FFlattenMaterial OutMaterial;
		for (const FPropertyEntry& Entry : MaterialOptions->Properties)
		{
			if (Entry.Property != MP_MAX)
			{
				EFlattenMaterialProperties OldProperty = NewToOldProperty(Entry.Property);
				OutMaterial.SetPropertySize(OldProperty, Entry.bUseCustomSize ? Entry.CustomSize : MaterialOptions->TextureSize);
			}
		}

		/** Reweighting */
		float TotalValue = 0.0f;
		for (const float& Value : SectionMaterialImportanceValues)
		{
			TotalValue += Value;
		}

		float Multiplier = 1.0f / TotalValue;

		for (float& Value : SectionMaterialImportanceValues)
		{
			Value *= Multiplier;
		}
		/** End reweighting */

		TArray<FUVOffsetScalePair> UVTransforms;
		if (InSettings.bUseTextureBinning)
		{
			TArray<FBox2D> MaterialBoxes;
			FMaterialUtilities::GeneratedBinnedTextureSquares(FVector2D(1.0f, 1.0f), SectionMaterialImportanceValues, MaterialBoxes);
			FlattenBinnedMaterials(FlattenedMaterials, MaterialBoxes, OutMaterial, UVTransforms);
		}
		else
		{
			MergeFlattenedMaterials(FlattenedMaterials, OutMaterial, UVTransforms);
		}
		
		// If materials were baked out using either a different UV channel than 0 or with fully custom uvs we should replace them
		for (FMeshData& MeshData : GlobalMeshSettings)
		{
			FRawMesh* RawMesh = MeshData.RawMesh;
			if (MeshData.CustomTextureCoordinates.Num())
			{
				RawMesh->WedgeTexCoords[0] = MeshData.CustomTextureCoordinates;
			}
			else if (MeshData.TextureCoordinateIndex != 0)
			{
				RawMesh->WedgeTexCoords[0] = RawMesh->WedgeTexCoords[MeshData.TextureCoordinateIndex];
			}
		}

		// Adjust UVs
		for (int32 ComponentIndex = 0; ComponentIndex < ComponentsToMerge.Num(); ++ComponentIndex)
		{
			TArray<uint32> ProcessedMaterials;
			for (TPair<FMeshLODKey, MaterialRemapPair>& MappingPair : OutputMaterialsMap)
			{
				if (MappingPair.Key.GetMeshIndex() == ComponentIndex && !ProcessedMaterials.Contains(MappingPair.Value.Key))
				{
					const int32 LODIndex = MappingPair.Key.GetLODIndex();
					// Found component entry

					// Retrieve raw mesh data for this component and lod pair
					FRawMesh* RawMesh = DataTracker.GetRawMeshPtr(MappingPair.Key);

					const FMeshData& MeshData = GlobalMeshSettings[MappingPair.Value.Value];
					const FUVOffsetScalePair& UVTransform = UVTransforms[MappingPair.Value.Value];

					const uint32 MaterialIndex = MappingPair.Value.Key;
					ProcessedMaterials.Add(MaterialIndex);
					if (RawMesh->VertexPositions.Num())
					{
						for (int32 UVChannelIdx = 0; UVChannelIdx < MAX_MESH_TEXTURE_COORDS; ++UVChannelIdx)
						{
							TArray<FVector2D>& UVs = RawMesh->WedgeTexCoords[UVChannelIdx];
							if (RawMesh->WedgeTexCoords[UVChannelIdx].Num() > 0)
							{
								int32 UVIdx = 0;
								for (const int32 FaceMaterialIndex : RawMesh->FaceMaterialIndices)
								{
									if (FaceMaterialIndex == MaterialIndex)
									{
										if (UVTransform.Value != FVector2D::ZeroVector)
										{
											RawMesh->WedgeTexCoords[UVChannelIdx][UVIdx + 0] = UVs[UVIdx + 0] * UVTransform.Value + UVTransform.Key;
											RawMesh->WedgeTexCoords[UVChannelIdx][UVIdx + 1] = UVs[UVIdx + 1] * UVTransform.Value + UVTransform.Key;
											RawMesh->WedgeTexCoords[UVChannelIdx][UVIdx + 2] = UVs[UVIdx + 2] * UVTransform.Value + UVTransform.Key;
										}
									}

									UVIdx += 3;
								}
							}
						}
					}
				}
			}
		}

		for (TRawMeshIterator Iterator = DataTracker.GetRawMeshIterator(); Iterator; ++Iterator)
		{
			FRawMesh& RawMesh = Iterator.Value();
			// Reset material indexes
			for (int32& FaceMaterialIndex : RawMesh.FaceMaterialIndices)
			{
				FaceMaterialIndex = 0;
			}
		}

		UMaterialInterface* MergedMaterial = CreateProxyMaterial(InBasePackageName, MergedAssetPackageName, InOuter, InSettings, OutMaterial, OutAssetsToSync);
		UniqueMaterials.Empty(1);
		UniqueMaterials.Add(MergedMaterial);
		
		FSectionInfo NewSection;
		NewSection.Material = MergedMaterial;
		NewSection.EnabledProperties.Add(GET_MEMBER_NAME_CHECKED(FStaticMeshSection, bCastShadow));
		DataTracker.AddBakedMaterialSection(NewSection);
	}

	TArray<FRawMesh> MergedRawMeshes;
	if (bMergeAllLODs)
	{
		MergedRawMeshes.AddDefaulted(DataTracker.GetNumLODsForMergedMesh());
		for (TConstLODIndexIterator Iterator = DataTracker.GetLODIndexIterator(); Iterator; ++Iterator)
		{
			// Find meshes for each lod
			const int32 LODIndex = *Iterator;
			FRawMesh& MergedMesh = MergedRawMeshes[LODIndex];
			for (int32 ComponentIndex = 0; ComponentIndex < ComponentsToMerge.Num(); ++ComponentIndex)
			{
				int32 RetrievedLODIndex = LODIndex;
				FRawMesh* RawMeshPtr = DataTracker.TryFindRawMeshForLOD(ComponentIndex, RetrievedLODIndex);

				if (RawMeshPtr != nullptr)
				{
					MergedMesh.FaceSmoothingMasks.Append(RawMeshPtr->FaceSmoothingMasks);

					if (bMergeMaterialData)
					{
						MergedMesh.FaceMaterialIndices.AddZeroed(RawMeshPtr->FaceMaterialIndices.Num());
					}
					else
					{
						for (const int32 FaceIndex : RawMeshPtr->FaceMaterialIndices)
						{
							int32 NewIndex = UniqueMaterials.IndexOfByKey(DataTracker.GetMaterialForSectionIndex(FaceIndex));
							MergedMesh.FaceMaterialIndices.Add(FaceIndex);
						}
					}

					const int32 IndicesOffset = MergedMesh.VertexPositions.Num();
					for (int32 WedgeIndex : RawMeshPtr->WedgeIndices)
					{
						MergedMesh.WedgeIndices.Add(WedgeIndex + IndicesOffset);
					}

					for (FVector VertexPos : RawMeshPtr->VertexPositions)
					{
						MergedMesh.VertexPositions.Add(VertexPos - MergedAssetPivot);
					}

					MergedMesh.WedgeTangentX.Append(RawMeshPtr->WedgeTangentX);
					MergedMesh.WedgeTangentY.Append(RawMeshPtr->WedgeTangentY);
					MergedMesh.WedgeTangentZ.Append(RawMeshPtr->WedgeTangentZ);

					// Deal with vertex colors
					// Some meshes may have it, in this case merged mesh will be forced to have vertex colors as well
					if (InSettings.bBakeVertexDataToMesh)
					{
						if (DataTracker.DoesLODContainVertexColors(0) && RawMeshPtr->WedgeColors.Num())
						{
							MergedMesh.WedgeColors.Append(RawMeshPtr->WedgeColors);
						}
						else
						{
							// In case this source mesh does not have vertex colors, fill target with 0xFF
							int32 ColorsOffset = MergedMesh.WedgeColors.Num();
							int32 ColorsNum = RawMeshPtr->WedgeIndices.Num();
							MergedMesh.WedgeColors.AddUninitialized(ColorsNum);
							FMemory::Memset(&MergedMesh.WedgeColors[ColorsOffset], 0xFF, ColorsNum*MergedMesh.WedgeColors.GetTypeSize());
						}
					}

					// Merge all other UV channels 
					for (int32 ChannelIdx = 0; ChannelIdx < MAX_MESH_TEXTURE_COORDS; ++ChannelIdx)
					{
						// Whether this channel has data
						if (DataTracker.DoesUVChannelContainData(ChannelIdx, RetrievedLODIndex))
						{
							const TArray<FVector2D>& SourceChannel = RawMeshPtr->WedgeTexCoords[ChannelIdx];
							TArray<FVector2D>& TargetChannel = MergedMesh.WedgeTexCoords[ChannelIdx];

							// Whether source mesh has data in this channel
							if (SourceChannel.Num())
							{
								TargetChannel.Append(SourceChannel);
							}
							else
							{
								// Fill with zero coordinates if source mesh has no data for this channel
								const int32 TexCoordNum = RawMeshPtr->WedgeIndices.Num();
								for (int32 CoordIdx = 0; CoordIdx < TexCoordNum; ++CoordIdx)
								{
									TargetChannel.Add(FVector2D::ZeroVector);
								}
							}
						}
					}
				}
			}
		}
	}
	else
	{
		MergedRawMeshes.AddZeroed(1);
		FRawMesh& MergedMesh = MergedRawMeshes.Last();
		for (int32 ComponentIndex = 0; ComponentIndex < ComponentsToMerge.Num(); ++ComponentIndex)
		{
			int32 LODIndex = 0;

			FRawMesh* RawMeshPtr = DataTracker.FindRawMeshAndLODIndex(ComponentIndex, LODIndex);

			if (RawMeshPtr != nullptr)
			{
				MergedMesh.FaceSmoothingMasks.Append(RawMeshPtr->FaceSmoothingMasks);

				if (bMergeMaterialData)
				{
					MergedMesh.FaceMaterialIndices.AddZeroed(RawMeshPtr->FaceMaterialIndices.Num());
				}
				else
				{
					for (const int32 Index : RawMeshPtr->FaceMaterialIndices)
					{
						int32 NewIndex = UniqueMaterials.IndexOfByKey(DataTracker.GetMaterialForSectionIndex(Index));
						MergedMesh.FaceMaterialIndices.Add(Index);
					}
				}

				int32 IndicesOffset = MergedMesh.VertexPositions.Num();

				for (int32 Index : RawMeshPtr->WedgeIndices)
				{
					MergedMesh.WedgeIndices.Add(Index + IndicesOffset);
				}

				for (FVector VertexPos : RawMeshPtr->VertexPositions)
				{
					MergedMesh.VertexPositions.Add(VertexPos - MergedAssetPivot);
				}

				MergedMesh.WedgeTangentX.Append(RawMeshPtr->WedgeTangentX);
				MergedMesh.WedgeTangentY.Append(RawMeshPtr->WedgeTangentY);
				MergedMesh.WedgeTangentZ.Append(RawMeshPtr->WedgeTangentZ);

				// Deal with vertex colors
				// Some meshes may have it, in this case merged mesh will be forced to have vertex colors as well
				if (InSettings.bBakeVertexDataToMesh)
				{
					if (DataTracker.DoesLODContainVertexColors(0) && RawMeshPtr->WedgeColors.Num())
					{
						MergedMesh.WedgeColors.Append(RawMeshPtr->WedgeColors);
					}
					else
					{
						// In case this source mesh does not have vertex colors, fill target with 0xFF
						int32 ColorsOffset = MergedMesh.WedgeColors.Num();
						int32 ColorsNum = RawMeshPtr->WedgeIndices.Num();
						MergedMesh.WedgeColors.AddUninitialized(ColorsNum);
						FMemory::Memset(&MergedMesh.WedgeColors[ColorsOffset], 0xFF, ColorsNum*MergedMesh.WedgeColors.GetTypeSize());
					}
				}

				// Merge all other UV channels 
				for (int32 ChannelIdx = 0; ChannelIdx < MAX_MESH_TEXTURE_COORDS; ++ChannelIdx)
				{
					// Whether this channel has data
					if (DataTracker.DoesUVChannelContainData(ChannelIdx, LODIndex))
					{
						const TArray<FVector2D>& SourceChannel = RawMeshPtr->WedgeTexCoords[ChannelIdx];
						TArray<FVector2D>& TargetChannel = MergedMesh.WedgeTexCoords[ChannelIdx];

						// Whether source mesh has data in this channel
						if (SourceChannel.Num())
						{
							TargetChannel.Append(SourceChannel);
						}
						else
						{
							// Fill with zero coordinates if source mesh has no data for this channel
							const int32 TexCoordNum = RawMeshPtr->WedgeIndices.Num();
							for (int32 CoordIdx = 0; CoordIdx < TexCoordNum; ++CoordIdx)
							{
								TargetChannel.Add(FVector2D::ZeroVector);
							}
						}
					}
				}
			}
		}
	}

	// Populate mesh section map
	FMeshSectionInfoMap SectionInfoMap;	
	for (TConstLODIndexIterator Iterator = DataTracker.GetLODIndexIterator(); Iterator; ++Iterator)
	{
		const int32 LODIndex = *Iterator;
		TArray<uint32> UniqueMaterialIndices;
		const FRawMesh& TargetRawMesh = MergedRawMeshes[LODIndex];
		for (uint32 MaterialIndex : TargetRawMesh.FaceMaterialIndices)
		{
			UniqueMaterialIndices.AddUnique(MaterialIndex);
		}

		for (int32 Index = 0; Index < UniqueMaterialIndices.Num(); ++Index)
		{
			const int32 SectionIndex = UniqueMaterialIndices[Index];
			const FSectionInfo& StoredSectionInfo = DataTracker.GetSection(SectionIndex);
			FMeshSectionInfo SectionInfo;
			SectionInfo.bCastShadow = StoredSectionInfo.EnabledProperties.Contains(GET_MEMBER_NAME_CHECKED(FMeshSectionInfo, bCastShadow));
			SectionInfo.bEnableCollision = StoredSectionInfo.EnabledProperties.Contains(GET_MEMBER_NAME_CHECKED(FMeshSectionInfo, bEnableCollision));
			SectionInfo.MaterialIndex = UniqueMaterials.IndexOfByKey(StoredSectionInfo.Material);
			SectionInfoMap.Set(LODIndex, Index, SectionInfo);
		}
	}

	// Transform physics primitives to merged mesh pivot
	if (InSettings.bMergePhysicsData && !MergedAssetPivot.IsZero())
	{
		FTransform PivotTM(-MergedAssetPivot);
		for (FKAggregateGeom& Geometry : PhysicsGeometry)
		{
			FMeshMergeHelpers::TransformPhysicsGeometry(PivotTM, Geometry);
		}
	}

	// Compute target lightmap channel for each LOD, by looking at the first empty UV channel
	const int32 LightMapUVChannel = [&]()
	{
		if (InSettings.bGenerateLightMapUV)
		{
			const int32 TempChannel = DataTracker.GetAvailableLightMapUVChannel();
			if (TempChannel != INDEX_NONE)
			{
				return TempChannel;
			}
			else
			{
				// Output warning message
				UE_LOG(LogMeshMerging, Log, TEXT("Failed to find available lightmap uv channel"));
				
			}
		}

		return 0;
	}();		

	//
	//Create merged mesh asset
	//
	{
		FString AssetName;
		FString PackageName;
		if (InBasePackageName.IsEmpty())
		{
			AssetName = TEXT("SM_MERGED_") + FPackageName::GetShortName(MergedAssetPackageName);
			PackageName = FPackageName::GetLongPackagePath(MergedAssetPackageName) + TEXT("/") + AssetName;
		}
		else
		{
			AssetName = FPackageName::GetShortName(InBasePackageName);
			PackageName = InBasePackageName;
		}

		UPackage* Package = InOuter;
		if (Package == nullptr)
		{
			Package = CreatePackage(NULL, *PackageName);
			check(Package);
			Package->FullyLoad();
			Package->Modify();
		}

		UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, *AssetName, RF_Public | RF_Standalone);
		StaticMesh->InitResources();

		FString OutputPath = StaticMesh->GetPathName();

		// make sure it has a new lighting guid
		StaticMesh->LightingGuid = FGuid::NewGuid();
		if (InSettings.bGenerateLightMapUV)
		{
			StaticMesh->LightMapResolution = InSettings.TargetLightMapResolution;
			StaticMesh->LightMapCoordinateIndex = LightMapUVChannel;
		}


		for (int32 LODIndex = 0; LODIndex < MergedRawMeshes.Num(); ++LODIndex)
		{	
			FRawMesh& MergedMeshLOD = MergedRawMeshes[LODIndex];
			if (MergedMeshLOD.VertexPositions.Num() > 0)
			{
				FStaticMeshSourceModel* SrcModel = new (StaticMesh->SourceModels) FStaticMeshSourceModel();
				// Don't allow the engine to recalculate normals
				SrcModel->BuildSettings.bRecomputeNormals = false;
				SrcModel->BuildSettings.bRecomputeTangents = false;
				SrcModel->BuildSettings.bRemoveDegenerates = false;
				SrcModel->BuildSettings.bUseHighPrecisionTangentBasis = false;
				SrcModel->BuildSettings.bUseFullPrecisionUVs = false;
				SrcModel->BuildSettings.bGenerateLightmapUVs = InSettings.bGenerateLightMapUV;
				SrcModel->BuildSettings.MinLightmapResolution = InSettings.bComputedLightMapResolution ? DataTracker.GetLightMapDimension() : InSettings.TargetLightMapResolution;
				SrcModel->BuildSettings.SrcLightmapIndex = 0;
				SrcModel->BuildSettings.DstLightmapIndex = LightMapUVChannel;

				SrcModel->RawMeshBulkData->SaveRawMesh(MergedMeshLOD);
			}
		}
		
		for (UMaterialInterface* Material : UniqueMaterials)
		{
			if (Material && (!Material->IsAsset() && InOuter != GetTransientPackage()))
			{
				Material = nullptr; // do not save non-asset materials
			}
			StaticMesh->StaticMaterials.Add(FStaticMaterial(Material, DataTracker.GetMaterialSlotName(Material)));
		}

		if (InSettings.bMergePhysicsData)
		{
			StaticMesh->CreateBodySetup();
			if (BodySetupSource)
			{
				StaticMesh->BodySetup->CopyBodyPropertiesFrom(BodySetupSource);
			}

			StaticMesh->BodySetup->AggGeom = FKAggregateGeom();
			// Copy collision from the source meshes
			for (const FKAggregateGeom& Geom : PhysicsGeometry)
			{
				StaticMesh->BodySetup->AddCollisionFrom(Geom);
			}

			// Bake rotation into verts of convex hulls, so they scale correctly after rotation
			for (FKConvexElem& ConvexElem : StaticMesh->BodySetup->AggGeom.ConvexElems)
			{
				ConvexElem.BakeTransformToVerts();
			}
		}

		StaticMesh->SectionInfoMap.CopyFrom(SectionInfoMap);
		StaticMesh->OriginalSectionInfoMap.CopyFrom(SectionInfoMap);

		//Set the Imported version before calling the build
		StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
		StaticMesh->LightMapResolution = InSettings.bComputedLightMapResolution ? DataTracker.GetLightMapDimension() : InSettings.TargetLightMapResolution;

		StaticMesh->Build(bSilent);
		StaticMesh->PostEditChange();

		OutAssetsToSync.Add(StaticMesh);
		OutMergedActorLocation = MergedAssetPivot;
	}

	return;
}


UMaterialInterface* FMeshMergeUtilities::CreateProxyMaterial(const FString &InBasePackageName, FString MergedAssetPackageName, UPackage* InOuter, const FMeshMergingSettings &InSettings, FFlattenMaterial OutMaterial, TArray<UObject *>& OutAssetsToSync) const
{
	// Create merged material asset
	FString MaterialAssetName;
	FString MaterialPackageName;
	if (InBasePackageName.IsEmpty())
	{
		MaterialAssetName = TEXT("M_MERGED_") + FPackageName::GetShortName(MergedAssetPackageName);
		MaterialPackageName = FPackageName::GetLongPackagePath(MergedAssetPackageName) + TEXT("/") + MaterialAssetName;
	}
	else
	{
		MaterialAssetName = TEXT("M_") + FPackageName::GetShortName(InBasePackageName);
		MaterialPackageName = FPackageName::GetLongPackagePath(InBasePackageName) + TEXT("/") + MaterialAssetName;
	}

	UPackage* MaterialPackage = InOuter;
	if (MaterialPackage == nullptr)
	{
		MaterialPackage = CreatePackage(nullptr, *MaterialPackageName);
		check(MaterialPackage);
		MaterialPackage->FullyLoad();
		MaterialPackage->Modify();
	}

	UMaterialInstanceConstant* MergedMaterial = ProxyMaterialUtilities::CreateProxyMaterialInstance(MaterialPackage, InSettings.MaterialSettings, OutMaterial, MaterialAssetName, MaterialPackageName, OutAssetsToSync);
	// Set material static lighting usage flag if project has static lighting enabled
	static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
	const bool bAllowStaticLighting = (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnGameThread() != 0);
	if (bAllowStaticLighting)
	{
		MergedMaterial->CheckMaterialUsage(MATUSAGE_StaticLighting);
	}

	return MergedMaterial;
}

void FMeshMergeUtilities::ExtractPhysicsDataFromComponents(const TArray<UPrimitiveComponent*>& ComponentsToMerge, TArray<FKAggregateGeom>& InOutPhysicsGeometry, UBodySetup*& OutBodySetupSource) const
{
	InOutPhysicsGeometry.AddDefaulted(ComponentsToMerge.Num());
	for (int32 ComponentIndex = 0; ComponentIndex < ComponentsToMerge.Num(); ++ComponentIndex)
	{
		UPrimitiveComponent* PrimComp = ComponentsToMerge[ComponentIndex];
		UBodySetup* BodySetup = nullptr;
		FTransform ComponentToWorld = FTransform::Identity;

		if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(PrimComp))
		{
			UStaticMesh* SrcMesh = StaticMeshComp->GetStaticMesh();
			if (SrcMesh)
			{
				BodySetup = SrcMesh->BodySetup;
			}
			ComponentToWorld = StaticMeshComp->GetComponentToWorld();
		}
		else if (UShapeComponent* ShapeComp = Cast<UShapeComponent>(PrimComp))
		{
			BodySetup = ShapeComp->GetBodySetup();
			ComponentToWorld = ShapeComp->GetComponentToWorld();
		}
		
		FMeshMergeHelpers::ExtractPhysicsGeometry(BodySetup, ComponentToWorld, InOutPhysicsGeometry[ComponentIndex]);
		if (USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>(PrimComp))
		{
			FMeshMergeHelpers::PropagateSplineDeformationToPhysicsGeometry(SplineMeshComponent, InOutPhysicsGeometry[ComponentIndex]);
		}

		// We will use first valid BodySetup as a source of physics settings
		if (OutBodySetupSource == nullptr)
		{
			OutBodySetupSource = BodySetup;
		}
	}
}

void FMeshMergeUtilities::ScaleTextureCoordinatesToBox(const FBox2D& Box, TArray<FVector2D>& InOutTextureCoordinates) const
{
	const FBox2D CoordinateBox(InOutTextureCoordinates);
	const FVector2D CoordinateRange = CoordinateBox.GetSize();
	const FVector2D Offset = CoordinateBox.Min + Box.Min;
	const FVector2D Scale = Box.GetSize() / CoordinateRange;
	for (FVector2D& Coordinate : InOutTextureCoordinates)
	{
		Coordinate = (Coordinate - Offset) * Scale;
	}

}

#undef LOCTEXT_NAMESPACE // "MeshMergeUtils"