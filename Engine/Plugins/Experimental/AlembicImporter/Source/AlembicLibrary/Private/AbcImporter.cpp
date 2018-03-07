// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AbcImporter.h"

#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
#endif

THIRD_PARTY_INCLUDES_START
#include <Alembic/AbcCoreHDF5/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcCoreFactory/All.h>
#include <Alembic/AbcCoreAbstract/TimeSampling.h>
#include <Alembic/AbcCoreHDF5/All.h>
THIRD_PARTY_INCLUDES_END

#include "AbcImportData.h"
#include "Misc/Paths.h"
#include "Misc/FeedbackContext.h"
#include "Stats/StatsMisc.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectHash.h"
#include "RawIndexBuffer.h"
#include "Misc/ScopedSlowTask.h"

#include "PackageTools.h"
#include "RawMesh.h"
#include "ObjectTools.h"

#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "SkelImport.h"
#include "Animation/AnimSequence.h"

#include "AbcImportUtilities.h"
#include "Runnables/AbcMeshDataImportRunnable.h"
#include "../Utils.h"

#include "MeshUtilities.h"
#include "MaterialUtilities.h"
#include "Materials/MaterialInstance.h"

#include "Runtime/Engine/Classes/Materials/MaterialInterface.h"
#include "Runtime/Engine/Public/MaterialCompiler.h"

#include "ParallelFor.h"

#include "EigenHelper.h"

#include "AbcAssetImportData.h"

#include "AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "AbcImporter"

DEFINE_LOG_CATEGORY_STATIC(LogAbcImporter, Verbose, All);

#define OBJECT_TYPE_SWITCH(a, b, c) if (AbcImporterUtilities::IsType<a>(ObjectMetaData)) { \
	a TypedObject = a(b, Alembic::Abc::kWrapExisting); \
	ParseAbcObject<a>(TypedObject, c); bHandled = true; }

#define PRINT_UNIQUE_VERTICES 0

// Static variable to define the first sample index (sample zero for now)
const int32 FAbcImporter::FirstSampleIndex = 0;

FAbcImporter::FAbcImporter()
	: ImportData(nullptr)
{

}

FAbcImporter::~FAbcImporter()
{
	if (ImportData)
	{
		delete ImportData;
		ImportData = nullptr;
	}
}

void FAbcImporter::UpdateAssetImportData(UAbcAssetImportData* AssetImportData)
{
	AssetImportData->TrackNames.Empty();
	for (const TSharedPtr<FAbcPolyMeshObject>& MeshObject : ImportData->PolyMeshObjects)
	{
		if (MeshObject->bShouldImport)
		{
			AssetImportData->TrackNames.Add(MeshObject->Name);
		}
	}
}

void FAbcImporter::RetrieveAssetImportData(UAbcAssetImportData* AssetImportData)
{
	bool bAnySetForImport = false;

	for (TSharedPtr<FAbcPolyMeshObject>& MeshObject : ImportData->PolyMeshObjects)
	{
		if (AssetImportData->TrackNames.Contains(MeshObject->Name))
		{
			MeshObject->bShouldImport = true;
			bAnySetForImport = true;
		}		
	}

	// If none were set to import, set all of them to import (probably different scene/setup)
	if (!bAnySetForImport)
	{
		for (TSharedPtr<FAbcPolyMeshObject>& MeshObject : ImportData->PolyMeshObjects)
		{
			MeshObject->bShouldImport = true;
		}
	}
}

const EAbcImportError FAbcImporter::OpenAbcFileForImport(const FString InFilePath)
{
	// Init factory
	Alembic::AbcCoreFactory::IFactory Factory;
	Factory.setPolicy(Alembic::Abc::ErrorHandler::kThrowPolicy);
	Factory.setOgawaNumStreams(12);
	
	// Extract Archive and compression type from file
	Alembic::AbcCoreFactory::IFactory::CoreType CompressionType;
	Alembic::Abc::IArchive Archive = Factory.getArchive(TCHAR_TO_ANSI(*FPaths::ConvertRelativePathToFull(InFilePath)), CompressionType);
	if (!Archive.valid())
	{
		return AbcImportError_InvalidArchive;
	}

	// Get Top/root object
	Alembic::Abc::IObject TopObject(Archive, Alembic::Abc::kTop);
	if (!TopObject.valid())
	{
		return AbcImportError_NoValidTopObject;
	}

	if (ImportData)
	{
		delete ImportData;
		ImportData = nullptr;
	}

	ImportData = new FAbcImportData();

	TArray<TSharedPtr<FAbcTransformObject>> AbcHierarchy;
	FGuid ZeroGuid;
	TraverseAbcHierarchy(TopObject, AbcHierarchy, ZeroGuid);

	// Determine top level archive bounding box
	Alembic::AbcCoreAbstract::ObjectHeader Header = TopObject.getHeader();
	const Alembic::Abc::MetaData ObjectMetaData = TopObject.getMetaData();
	Alembic::Abc::ICompoundProperty Properties = TopObject.getProperties();
	
	Alembic::Abc::IBox3dProperty ArchiveBoundsProperty = Alembic::AbcGeom::GetIArchiveBounds(Archive, Alembic::Abc::ErrorHandler::kQuietNoopPolicy);
	if (ArchiveBoundsProperty.valid())
	{
		ImportData->ArchiveBounds = AbcImporterUtilities::ExtractBounds(ArchiveBoundsProperty);
	}	
	
	if (ImportData->PolyMeshObjects.Num() == 0)
	{
		return AbcImportError_NoMeshes;
	}

	ImportData->FilePath = InFilePath;
	ImportData->NumTotalMaterials = 0;
	ImportData->bBackedSupportsMultithreading = (CompressionType == Alembic::AbcCoreFactory::IFactory::kOgawa);

	const int32 NumTimeSamples = Archive.getNumTimeSamplings();
	if (NumTimeSamples >= 2)
	{
		ImportData->ArchiveTimePerCycle = Archive.getTimeSampling(1)->getTimeSamplingType().getTimePerCycle();
	}


	return AbcImportError_NoError;
}

const EAbcImportError FAbcImporter::ImportTrackData(const int32 InNumThreads, UAbcImportSettings* ImportSettings)
{
	SCOPE_LOG_TIME(TEXT("Alembic_ReadTrackData"), nullptr);
	TArray<FAbcMeshDataImportRunnable*> MeshImportRunnables;

	ImportData->ImportSettings = ImportSettings;

	const int32 NumMeshTracks = ImportData->PolyMeshObjects.Num();	
	FAbcSamplingSettings& SamplingSettings = ImportSettings->SamplingSettings;	
	
	// This will remove all poly meshes that are set not to be imported in the settings UI
	ImportData->PolyMeshObjects.RemoveAll([=](const TSharedPtr<FAbcPolyMeshObject>& Object) {return !Object->bShouldImport; });

	if (ImportSettings->MaterialSettings.bFindMaterials)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> AssetData;
		const UClass* Class = UMaterialInterface::StaticClass();
		AssetRegistryModule.Get().GetAssetsByClass(Class->GetFName(), AssetData, true);
		for (TSharedPtr<FAbcPolyMeshObject>& MeshObject : ImportData->PolyMeshObjects)
		{
			for (const FString& FaceSetName : MeshObject->FaceSetNames)
			{
				UMaterialInterface** ExistingMaterial = ImportData->MaterialMap.Find(*FaceSetName);
				if (!ExistingMaterial)
				{
					FAssetData* MaterialAsset = AssetData.FindByPredicate([=](const FAssetData& Asset)
					{
						return Asset.AssetName.ToString() == FaceSetName;
					});

					if (MaterialAsset)
					{
						UMaterialInterface* FoundMaterialInterface = Cast<UMaterialInterface>(MaterialAsset->GetAsset());
						if (FoundMaterialInterface)
						{							
							ImportData->MaterialMap.Add(FaceSetName, FoundMaterialInterface);
							UMaterial* BaseMaterial = Cast<UMaterial>(FoundMaterialInterface);
							if ( !BaseMaterial )
							{
								if (UMaterialInstance* FoundInstance = Cast<UMaterialInstance>(FoundMaterialInterface))
								{
									BaseMaterial = FoundInstance->GetMaterial();
								}
							}

							if (BaseMaterial)
							{
								BaseMaterial->bUsedWithSkeletalMesh |= ImportSettings->ImportType == EAlembicImportType::Skeletal;
								BaseMaterial->bUsedWithMorphTargets |= ImportSettings->ImportType == EAlembicImportType::Skeletal;
							}							
						}
					}
					else
					{
						TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("NoMaterialForFaceSet", "Unable to find matching Material for Face Set {0}, using default material instead."), FText::FromString(FaceSetName)));
						FAbcImportLogger::AddImportMessage(Message);
					}
				}
			}
		}
	}
	else if (ImportSettings->MaterialSettings.bCreateMaterials)
	{
		// Creates materials according to the face set names that were found in the Alembic file
		for (TSharedPtr<FAbcPolyMeshObject>& MeshObject : ImportData->PolyMeshObjects)
		{
			for (const FString& FaceSetName : MeshObject->FaceSetNames)
			{
				// Preventing duplicate material creation
				UMaterialInterface** ExistingMaterial = ImportData->MaterialMap.Find(*FaceSetName);
				if (!ExistingMaterial)
				{ 
					UMaterial* Material = NewObject<UMaterial>((UObject*)GetTransientPackage(), *FaceSetName);
					Material->bUsedWithMorphTargets = true;
					ImportData->MaterialMap.Add(FaceSetName, Material);
				}
			}
		}
	}	

        // If there were not bounds available at the archive level just sum all child bounds
	if (FMath::IsNearlyZero(ImportData->ArchiveBounds.SphereRadius))
	{
		for (const TSharedPtr<FAbcPolyMeshObject>& PolyMeshObject : ImportData->PolyMeshObjects)
		{
			ImportData->ArchiveBounds = ImportData->ArchiveBounds + PolyMeshObject->SelfBounds + PolyMeshObject->ChildBounds;
		}

		for (const TSharedPtr<FAbcTransformObject>& TransformObject : ImportData->TransformObjects)
		{
			ImportData->ArchiveBounds = ImportData->ArchiveBounds + TransformObject->SelfBounds + TransformObject->ChildBounds;
		}	
	}

	// Apply conversion to bounds as well
	AbcImporterUtilities::ApplyConversion(ImportData->ArchiveBounds, ImportSettings->ConversionSettings);

	// Determining sampling time/types and when to start and stop sampling
	uint32 StartFrameIndex = SamplingSettings.bSkipEmpty ? (SamplingSettings.FrameStart > ImportData->MinFrameIndex ? SamplingSettings.FrameStart : ImportData->MinFrameIndex ): SamplingSettings.FrameStart;
	uint32 EndFrameIndex = SamplingSettings.FrameEnd;

	// When importing static meshes optimize frame import span
	if (ImportSettings->ImportType == EAlembicImportType::StaticMesh)
	{
		SamplingSettings.FrameEnd = SamplingSettings.FrameStart + 1;
		EndFrameIndex = StartFrameIndex + 1;
	}
		
	int32 FrameSpan = EndFrameIndex - StartFrameIndex;
	float CacheLength = ImportData->MaxTime - ImportData->MinTime;

	// If Start==End or Start > End output error message due to invalid frame span
	if (FrameSpan <= 0)
	{
		TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("NoFramesForMeshObject", "Invalid frame range specified {0} - {1}."), FText::FromString(FString::FromInt(StartFrameIndex)),FText::FromString(FString::FromInt(EndFrameIndex))));
		FAbcImportLogger::AddImportMessage(Message);
		return AbcImportError_FailedToImportData;
	}

	float TimeStep = 0.0f;
	EAlembicSamplingType SamplingType = SamplingSettings.SamplingType;
	switch (SamplingType)
	{
	case EAlembicSamplingType::PerFrame:
		{
			// Calculates the time step required to get the number of frames
			TimeStep = !FMath::IsNearlyZero(ImportData->ArchiveTimePerCycle) ? ImportData->ArchiveTimePerCycle : CacheLength / (float)(ImportData->MaxFrameIndex - ImportData->MinFrameIndex);
			break;
		}
	
	case EAlembicSamplingType::PerTimeStep:
		{
			// Calculates the original time step and the ratio between it and the user specified time step
			const float OriginalTimeStep = CacheLength / (float)(ImportData->MaxFrameIndex - ImportData->MinFrameIndex);
			const float FrameStepRatio = OriginalTimeStep / SamplingSettings.TimeSteps;
			TimeStep = SamplingSettings.TimeSteps;

			AbcImporterUtilities::CalculateNewStartAndEndFrameIndices(FrameStepRatio, StartFrameIndex, EndFrameIndex);
			FrameSpan = EndFrameIndex - StartFrameIndex;		
			break;
		}
	case EAlembicSamplingType::PerXFrames:
		{
			// Calculates the original time step and the ratio between it and the user specified time step
			const float OriginalTimeStep = CacheLength / (float)(ImportData->MaxFrameIndex - ImportData->MinFrameIndex);
			const float FrameStepRatio = OriginalTimeStep / ((float)SamplingSettings.FrameSteps * OriginalTimeStep);
			TimeStep = ((float)SamplingSettings.FrameSteps * OriginalTimeStep);			

			AbcImporterUtilities::CalculateNewStartAndEndFrameIndices(FrameStepRatio, StartFrameIndex, EndFrameIndex);
			FrameSpan = EndFrameIndex - StartFrameIndex;
			break;
		}

	default:
		checkf(false, TEXT("Incorrect sampling type found in import settings (%i)"), (uint8)SamplingType);
	}

	ImportData->SecondsPerFrame = TimeStep;
	ImportData->ImportLength = (FrameSpan - 1) * TimeStep;

	// Override the frame start to not crash when indexing the sample array using it as a frame offset TODO CHANGE THIS TO BE MORE CLEAN
	ImportSettings->SamplingSettings.FrameStart = StartFrameIndex;

	// Reading the required transform tracks
	ParallelFor(ImportData->TransformObjects.Num(), [&](int32 ObjectIndex)
	{
		TSharedPtr<FAbcTransformObject>& TransformObject = ImportData->TransformObjects[ObjectIndex];

		Alembic::AbcGeom::IXform Transform = TransformObject->Transform;
		TransformObject->MatrixSamples.SetNumZeroed(FrameSpan);
		TransformObject->TimeSamples.SetNumZeroed(FrameSpan);

		// Get schema from parent object
		Alembic::AbcGeom::IXformSchema Schema = Transform.getSchema();
		Alembic::AbcGeom::XformSample MatrixSample;
		for (int32 FrameIndex = 0; FrameIndex < FrameSpan; ++FrameIndex)
		{
			const float SampleTime = TimeStep * (StartFrameIndex + FrameIndex);
			Alembic::Abc::ISampleSelector SampleSelector = AbcImporterUtilities::GenerateAlembicSampleSelector<double>((double)SampleTime);
			Schema.get(MatrixSample, SampleSelector);

			// Get matrix and concatenate
			Alembic::Abc::M44d Matrix = MatrixSample.getMatrix();
			TransformObject->MatrixSamples[FrameIndex] = AbcImporterUtilities::ConvertAlembicMatrix(Matrix);

			// Get TimeSampler for this sample's time
			Alembic::Abc::TimeSamplingPtr TimeSampler = Schema.getTimeSampling();
			TransformObject->TimeSamples[FrameIndex] = SampleTime;
		}
	}, !ImportData->bBackedSupportsMultithreading);

	// Now we have loaded all the transformations, cache the accumulated transforms for each used hierarchy path
	CacheHierarchyTransforms(StartFrameIndex * TimeStep, EndFrameIndex * TimeStep);

	// Allocating the number of meshsamples we will import for each object
	for (TSharedPtr<FAbcPolyMeshObject>& MeshObject : ImportData->PolyMeshObjects)
	{
		MeshObject->MeshSamples.AddZeroed(FrameSpan);
	}

	// Initializing and running the importing threads 
	const uint32 NumThreads = ImportData->bBackedSupportsMultithreading ? InNumThreads : 1;

	// At least 4 frames are required in order for use to spin off multiple threads to import the data
	const uint32 MinimumNumberOfSamplesForSpinningOfThreads = 4;
	const uint32 Steps = (FrameSpan <= MinimumNumberOfSamplesForSpinningOfThreads) ? FrameSpan : FMath::CeilToInt((float)FrameSpan / (float)NumThreads);

	uint32 StartingFrameIndex = StartFrameIndex;
	while (StartingFrameIndex < EndFrameIndex)
	{
		FAbcMeshDataImportRunnable* TestRunable = new FAbcMeshDataImportRunnable(ImportData, StartingFrameIndex, FMath::Min(StartingFrameIndex + Steps, EndFrameIndex), TimeStep);
		StartingFrameIndex += Steps;
		MeshImportRunnables.Push(TestRunable);
	}
	
	bool bImportSuccesful = true;

	// All Mesh data is imported from the Alembic format after the runnables have finished
	for (FAbcMeshDataImportRunnable* Runnable : MeshImportRunnables)
	{
		Runnable->Wait();
		bImportSuccesful &= Runnable->WasSuccesful();
	}

	if (!bImportSuccesful)
	{
		return AbcImportError_FailedToImportData;
	}
		
	// Processing the mesh objects in order to calculate their normals/tangents
	for (TSharedPtr<FAbcPolyMeshObject>& MeshObject : ImportData->PolyMeshObjects)
	{
		// Remove invalid or empty samples
		MeshObject->MeshSamples.RemoveAll([&](FAbcMeshSample* In) { return In == nullptr; });
		MeshObject->NumSamples = MeshObject->MeshSamples.Num();

		const bool bFramesAvailable = MeshObject->MeshSamples.Num() > 0;
		if (!bFramesAvailable)
		{
			TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("NoFramesForMeshObject", "Unable to import valid frames for {0}, skipping object."), FText::FromString(MeshObject->Name)));
			FAbcImportLogger::AddImportMessage(Message);
			continue;
		}

		// Make sure we have smoothing groups for the first frame
		if (MeshObject->MeshSamples[FirstSampleIndex]->SmoothingGroupIndices.Num() == 0)
		{
			if (ImportData->ImportSettings->NormalGenerationSettings.bForceOneSmoothingGroupPerObject)
			{
				if (MeshObject->MeshSamples[FirstSampleIndex]->Normals.Num() == 0)
				{
					AbcImporterUtilities::CalculateSmoothNormals(MeshObject->MeshSamples[FirstSampleIndex]);
				}

				MeshObject->MeshSamples[FirstSampleIndex]->SmoothingGroupIndices.AddZeroed(MeshObject->MeshSamples[FirstSampleIndex]->Indices.Num() / 3);
				MeshObject->MeshSamples[FirstSampleIndex]->NumSmoothingGroups = 1;
			}
			else
			{
				if (MeshObject->MeshSamples[FirstSampleIndex]->Normals.Num() == 0)
				{
					AbcImporterUtilities::CalculateSmoothNormals(MeshObject->MeshSamples[FirstSampleIndex]);
				}

				AbcImporterUtilities::GenerateSmoothingGroupsIndices(MeshObject->MeshSamples[FirstSampleIndex], ImportData->ImportSettings);
			}
			
		}

		// We determine whether or not the mesh contains constant topology to know if it can be PCA compressed
		int32 VertexCount = bFramesAvailable ? MeshObject->MeshSamples[FirstSampleIndex]->Vertices.Num() : 0;
		int32 IndexCount = bFramesAvailable ? MeshObject->MeshSamples[FirstSampleIndex]->Indices.Num() : 0;
		MeshObject->bConstantTopology = true;
		for (FAbcMeshSample* Sample : MeshObject->MeshSamples)
		{
			if (Sample && (VertexCount != Sample->Vertices.Num() || IndexCount != Sample->Indices.Num()))
			{
				MeshObject->bConstantTopology = false;
				break;
			}
		}
		
		// Normal availability determination and calculating what's needed/missing
		const bool bNormalsAvailable = MeshObject->MeshSamples[FirstSampleIndex]->Normals.Num() > 0 && !ImportData->ImportSettings->NormalGenerationSettings.bRecomputeNormals;
		const bool bFullFrameNormalsAvailable = (!MeshObject->bConstant && MeshObject->MeshSamples.Num() > (FirstSampleIndex + 1)) && (MeshObject->MeshSamples[FirstSampleIndex + 1]->Normals.Num() > 0);
		const bool bCalculateSmoothingGroups = !ImportData->ImportSettings->NormalGenerationSettings.bForceOneSmoothingGroupPerObject;		
		if (!bNormalsAvailable || !bFullFrameNormalsAvailable)
		{
			// Require calculating Normals, no normals available whatsoever or we have varying topology for which we cannot reuse smoothing groups
			if (!bNormalsAvailable || !MeshObject->bConstantTopology)
			{
				// Function bodies for regular and smooth normals to prevent branch within loop
				const TFunction<void(int32)> RegularNormalsFunction
					= [this,MeshObject](int32 Index)
				{
					FAbcMeshSample* MeshSample = MeshObject->MeshSamples[Index];
					if (MeshSample)
					{
						AbcImporterUtilities::CalculateNormals(MeshSample);
						AbcImporterUtilities::GenerateSmoothingGroupsIndices(MeshSample, ImportData->ImportSettings);
						AbcImporterUtilities::CalculateNormalsWithSmoothingGroups(MeshSample, MeshSample->SmoothingGroupIndices, MeshSample->NumSmoothingGroups);
					}
				};

				const TFunction<void(int32)> SmoothNormalsFunction
					= [MeshObject](int32 Index)
				{
					FAbcMeshSample* MeshSample = MeshObject->MeshSamples[Index];
					if (MeshSample)
					{
						AbcImporterUtilities::CalculateSmoothNormals(MeshSample);

						// Setup smoothing masks to 0
						MeshSample->SmoothingGroupIndices.Empty(MeshSample->Indices.Num() / 3);
						MeshSample->SmoothingGroupIndices.AddZeroed(MeshSample->Indices.Num() / 3);
						MeshSample->NumSmoothingGroups = 1;
					}
				};

				ParallelFor(MeshObject->MeshSamples.Num(), (ImportData->ImportSettings->NormalGenerationSettings.bRecomputeNormals || !MeshObject->bConstantTopology) && bCalculateSmoothingGroups ? RegularNormalsFunction : SmoothNormalsFunction);
			}
			else
			{
				// Just normals for first frame, and we have the extracted smoothing groups
				ParallelFor(MeshObject->MeshSamples.Num() - 1,
					[&](int32 Index)
				{
					FAbcMeshSample* MeshSample = MeshObject->MeshSamples[Index + 1];
					if (MeshSample)
					{
						AbcImporterUtilities::CalculateNormalsWithSmoothingGroups(MeshSample, MeshObject->MeshSamples[FirstSampleIndex]->SmoothingGroupIndices, MeshObject->MeshSamples[FirstSampleIndex]->NumSmoothingGroups);

						

						// Copy smoothing masks from frame 0
						MeshSample->SmoothingGroupIndices = MeshObject->MeshSamples[FirstSampleIndex]->SmoothingGroupIndices;
					}
				});
			}
		}	

		// Module manager is not thread safe, so need to prefetch before parallelfor
		IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");

		// Since we have normals and UVs now calculate tangents
		ParallelFor(MeshObject->MeshSamples.Num(),
			[&](int32 Index)
		{
			FAbcMeshSample* MeshSample = MeshObject->MeshSamples[Index];
			
			if (MeshSample)
			{
				AbcImporterUtilities::ComputeTangents(MeshSample, ImportSettings, MeshUtilities);
			}
		});

		if (bFramesAvailable)
		{
			ImportData->NumTotalMaterials += MeshObject->MeshSamples[FirstSampleIndex]->NumMaterials;
		}
	}

	// Simple duplicate frame removal (only needs to be done if we're importing the data as a geometry cache asset
	if (ImportData->ImportSettings->ImportType == EAlembicImportType::GeometryCache)
	{
		ParallelFor(ImportData->PolyMeshObjects.Num(), [&](int32 MeshObjectIndex)
		{
			TSharedPtr<FAbcPolyMeshObject>& MeshObject = ImportData->PolyMeshObjects[MeshObjectIndex];
			if (!MeshObject->bConstant)
			{
				TMap<uint32, uint32> IdenticalPositions;

				for (int32 SampleIndex = FAbcImporter::FirstSampleIndex; SampleIndex < MeshObject->MeshSamples.Num() - 1; ++SampleIndex)
				{
					FAbcMeshSample* Sample = MeshObject->MeshSamples[SampleIndex];
					FAbcMeshSample* NextSample = MeshObject->MeshSamples[SampleIndex + 1];
					int32 Result = FMemory::Memcmp(&Sample->Vertices[0], &NextSample->Vertices[0], Sample->Vertices.GetTypeSize() * Sample->Vertices.Num());
					if (Result == 0)
					{
						IdenticalPositions.Add(SampleIndex, SampleIndex + 1);
					}
				}

				for (TPair<uint32, uint32> Pair : IdenticalPositions)
				{
					uint32 FrameIndex = Pair.Value;
					delete MeshObject->MeshSamples[FrameIndex];
					MeshObject->MeshSamples[FrameIndex] = nullptr;
				}

				MeshObject->MeshSamples.RemoveAll([&](FAbcMeshSample* In) { return In == nullptr; });
				MeshObject->NumSamples = MeshObject->MeshSamples.Num();
			}
		});
	}

	const bool bApplyTransformation = (ImportData->ImportSettings->ImportType == EAlembicImportType::StaticMesh && ImportData->ImportSettings->StaticMeshSettings.bMergeMeshes && ImportData->ImportSettings->StaticMeshSettings.bPropagateMatrixTransformations)
		|| (ImportData->ImportSettings->ImportType == EAlembicImportType::Skeletal && ImportData->ImportSettings->CompressionSettings.bBakeMatrixAnimation);

	const bool bInverseIndices = bApplyTransformation || (ImportData->ImportSettings->ImportType == EAlembicImportType::GeometryCache);
	ParallelFor(ImportData->PolyMeshObjects.Num(), [&](int32 MeshObjectIndex)
	{
		TSharedPtr<FAbcPolyMeshObject>& MeshObject = ImportData->PolyMeshObjects[MeshObjectIndex];
		const bool bFramesAvailable = MeshObject->MeshSamples.Num() > 0;
		if (bApplyTransformation && bFramesAvailable)
		{
			TSharedPtr<FCachedHierarchyTransforms>& CachedHierarchyTransforms = ImportData->CachedHierarchyTransforms.FindChecked(MeshObject->HierarchyGuid);

			const bool bStaticMesh = MeshObject->bConstant &&  MeshObject->bConstantTransformation;

			// Loop through entire imported framespan
			for (uint32 FrameIndex = StartFrameIndex; FrameIndex < EndFrameIndex; ++FrameIndex)
			{	
				// If we are dealing with a static mesh only apply matrix to 1 sample after that break out
				if (bStaticMesh && FrameIndex > ImportData->ImportSettings->SamplingSettings.FrameStart)
				{
					break;
				}
				// If we are dealing with a mesh for which samples start after T0, wait until we reach their starting frame
				if (MeshObject->StartFrameIndex > FrameIndex)
				{
					continue;
				}

				// Determine the sample offset into the MeshObject's samples array (optimized to store only necessary samples)
				const int32 SampleOffset = ( ImportData->ImportSettings->SamplingSettings.bSkipEmpty && MeshObject->StartFrameIndex > StartFrameIndex) || (MeshObject->StartFrameIndex > StartFrameIndex ) ? MeshObject->StartFrameIndex : StartFrameIndex;
				// If completely constant there is only one sample, otherwise calculate correct index using the sample offset
				const int32 SampleIndex = bStaticMesh ? 0 : FrameIndex - SampleOffset;
				FAbcMeshSample* Sample = MeshObject->MeshSamples[SampleIndex];

				const int32 MatrixIndex = FrameIndex - ImportData->ImportSettings->SamplingSettings.FrameStart;
				checkf(MeshObject->bConstantTransformation || CachedHierarchyTransforms->MatrixSamples.IsValidIndex(MatrixIndex), TEXT("Trying to sample an invalid matrix sample"));
				const FMatrix& Transform = MeshObject->bConstantTransformation ? CachedHierarchyTransforms->MatrixSamples[0] : CachedHierarchyTransforms->MatrixSamples[MatrixIndex];

				AbcImporterUtilities::PropogateMatrixTransformationToSample(Sample, Transform);
			}
		}
			
		// Apply conversion according to user set scale/rotation and uv flipping
		for (FAbcMeshSample* Sample : MeshObject->MeshSamples)
		{
			AbcImporterUtilities::ApplyConversion(Sample, ImportData->ImportSettings->ConversionSettings, bInverseIndices);
		}
	});
	
	return AbcImportError_NoError;
}

void FAbcImporter::TraverseAbcHierarchy(const Alembic::Abc::IObject& InObject, TArray<TSharedPtr<FAbcTransformObject>>& InObjectHierarchy, FGuid InGuid)
{
	// Get Header and MetaData info from current Alembic Object
	Alembic::AbcCoreAbstract::ObjectHeader Header = InObject.getHeader();	
	const Alembic::Abc::MetaData ObjectMetaData = InObject.getMetaData();
	const uint32 NumChildren = InObject.getNumChildren();

	if (InObjectHierarchy.Num())
	{
		ImportData->Hierarchies.Add(InGuid, InObjectHierarchy);
	}

	bool bHandled = false;

	OBJECT_TYPE_SWITCH(Alembic::AbcGeom::IPolyMesh, InObject, InGuid);
	OBJECT_TYPE_SWITCH(Alembic::AbcGeom::IXform, InObject, InGuid);

	// Recursive traversal of child objects
	if (NumChildren > 0)
	{
		// Push back this object for the Hierarchy
		TArray<TSharedPtr<FAbcTransformObject>> NewObjectHierarchy = InObjectHierarchy;

		// Only add handled objects to ensure we have valid objects in the hierarchies
		if (bHandled && AbcImporterUtilities::IsType<Alembic::AbcGeom::IXform>(ObjectMetaData))
		{
			NewObjectHierarchy.Add(ImportData->TransformObjects.Last());
		}

		FGuid ChildGuid = (NewObjectHierarchy.Num() != InObjectHierarchy.Num()) ? FGuid::NewGuid() : InGuid;

		for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
		{
			const Alembic::Abc::IObject& AbcChildObject = InObject.getChild(ChildIndex);
			TraverseAbcHierarchy(AbcChildObject, NewObjectHierarchy, ChildGuid);
		}
	}
}

template<>
void FAbcImporter::ParseAbcObject<Alembic::AbcGeom::IXform>(Alembic::AbcGeom::IXform& InXform, FGuid InHierarchyGuid)
{
	TSharedPtr<FAbcTransformObject> TransformObject = TSharedPtr<FAbcTransformObject>(new FAbcTransformObject());
	TransformObject->HierarchyGuid = InHierarchyGuid;
	TransformObject->Transform = InXform;
	TransformObject->Name = FString(InXform.getName().c_str());

	// Retrieve schema and frame information
	Alembic::AbcGeom::IXformSchema Schema = InXform.getSchema();
	TransformObject->NumSamples = Schema.getNumSamples();
	TransformObject->bConstant = Schema.isConstant();

	float MinTime, MaxTime;
	AbcImporterUtilities::GetMinAndMaxTime(InXform.getSchema(), MinTime, MaxTime);
	ImportData->MinTime = FMath::Min(ImportData->MinTime, MinTime);
	ImportData->MaxTime = FMath::Max(ImportData->MaxTime, MaxTime);
	ImportData->NumFrames = FMath::Max(ImportData->NumFrames, (uint32)InXform.getSchema().getNumSamples());	

	AbcImporterUtilities::GetStartTimeAndFrame(InXform.getSchema(), TransformObject->StartFrameTime, TransformObject->StartFrameIndex);
	ImportData->MinFrameIndex = FMath::Min(ImportData->MinFrameIndex, TransformObject->StartFrameIndex);
	ImportData->MaxFrameIndex = FMath::Max(ImportData->MaxFrameIndex, TransformObject->StartFrameIndex + TransformObject->NumSamples);

	ImportData->TransformObjects.Push(TransformObject);
}

template<>
void FAbcImporter::ParseAbcObject<Alembic::AbcGeom::IPolyMesh>(Alembic::AbcGeom::IPolyMesh& InPolyMesh, FGuid InHierarchyGuid)
{
	TSharedPtr<FAbcPolyMeshObject> PolyMeshObject = TSharedPtr<FAbcPolyMeshObject>( new FAbcPolyMeshObject());
	PolyMeshObject->Mesh = InPolyMesh;
	PolyMeshObject->Name = FString(InPolyMesh.getName().c_str());
	PolyMeshObject->bShouldImport = true;

	// Retrieve schema and frame information
	Alembic::AbcGeom::IPolyMeshSchema Schema = InPolyMesh.getSchema();
	PolyMeshObject->NumSamples = Schema.getNumSamples();
	PolyMeshObject->bConstant = Schema.isConstant();
	PolyMeshObject->SelfBounds = AbcImporterUtilities::ExtractBounds(Schema.getSelfBoundsProperty());
	PolyMeshObject->ChildBounds = AbcImporterUtilities::ExtractBounds(Schema.getChildBoundsProperty());
	
	PolyMeshObject->HierarchyGuid = InHierarchyGuid;

	AbcImporterUtilities::RetrieveFaceSetNames(Schema, PolyMeshObject->FaceSetNames);

	float MinTime, MaxTime;
	AbcImporterUtilities::GetMinAndMaxTime(InPolyMesh.getSchema(), MinTime, MaxTime);
	ImportData->MinTime = FMath::Min(ImportData->MinTime, MinTime);
	ImportData->MaxTime = FMath::Max(ImportData->MaxTime, MaxTime);
	ImportData->NumFrames = FMath::Max(ImportData->NumFrames, PolyMeshObject->NumSamples);
	
	AbcImporterUtilities::GetStartTimeAndFrame(InPolyMesh.getSchema(), PolyMeshObject->StartFrameTime, PolyMeshObject->StartFrameIndex);
	ImportData->MinFrameIndex = FMath::Min(ImportData->MinFrameIndex, PolyMeshObject->StartFrameIndex);
	ImportData->MaxFrameIndex = FMath::Max(ImportData->MaxFrameIndex, PolyMeshObject->StartFrameIndex + PolyMeshObject->NumSamples);
	
	ImportData->PolyMeshObjects.Push(PolyMeshObject);
}

template<typename T>
T* FAbcImporter::CreateObjectInstance(UObject*& InParent, const FString& ObjectName, const EObjectFlags Flags)
{
	// Parent package to place new mesh
	UPackage* Package = nullptr;
	FString NewPackageName;

	// Setup package name and create one accordingly
	NewPackageName = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetName() + TEXT("/") + ObjectName);
	NewPackageName = PackageTools::SanitizePackageName(NewPackageName);
	Package = CreatePackage(nullptr, *NewPackageName);

	const FString SanitizedObjectName = ObjectTools::SanitizeObjectName(ObjectName);

	T* ExistingTypedObject = FindObject<T>(Package, *SanitizedObjectName);
	UObject* ExistingObject = FindObject<UObject>(Package, *SanitizedObjectName);

	if (ExistingTypedObject != nullptr)
	{
		ExistingTypedObject->PreEditChange(nullptr);
	}
	else if (ExistingObject != nullptr)
	{
		// Replacing an object.  Here we go!
		// Delete the existing object
		const bool bDeleteSucceeded = ObjectTools::DeleteSingleObject(ExistingObject);

		if (bDeleteSucceeded)
		{
			// Force GC so we can cleanly create a new asset (and not do an 'in place' replacement)
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

			// Create a package for each mesh
			Package = CreatePackage(nullptr, *NewPackageName);
			InParent = Package;
		}
		else
		{
			// failed to delete
			return nullptr;
		}
	}

	return NewObject<T>(Package, FName(*SanitizedObjectName), Flags | RF_Public);
}

UStaticMesh* FAbcImporter::ImportSingleAsStaticMesh(const int32 MeshTrackIndex, UObject* InParent, EObjectFlags Flags)
{
	// Get Mesh object from array
	check(MeshTrackIndex >= 0 && MeshTrackIndex < ImportData->PolyMeshObjects.Num() && "Incorrect Mesh index");

	// Populate raw mesh from sample
	const uint32 FrameIndex = 0;
	FRawMesh RawMesh;
	GenerateRawMeshFromSample(MeshTrackIndex, FrameIndex, RawMesh);
	
	// Setup static mesh instance
	UStaticMesh* StaticMesh = CreateStaticMeshFromRawMesh(InParent, ImportData->PolyMeshObjects[MeshTrackIndex]->Name, Flags, ImportData->PolyMeshObjects[MeshTrackIndex]->MeshSamples[FirstSampleIndex]->NumMaterials, ImportData->PolyMeshObjects[MeshTrackIndex]->FaceSetNames, RawMesh);
	
	// Return the freshly created static mesh
	return StaticMesh;
}

UStaticMesh* FAbcImporter::CreateStaticMeshFromRawMesh(UObject* InParent, const FString& Name, EObjectFlags Flags, const uint32 NumMaterials, const TArray<FString>& FaceSetNames, FRawMesh& RawMesh)
{
	UStaticMesh* StaticMesh = CreateObjectInstance<UStaticMesh>(InParent, Name, Flags);

	// Only import data if a valid object was created
	if (StaticMesh)
	{
		// Add the first LOD, we only support one
		new(StaticMesh->SourceModels) FStaticMeshSourceModel();

		// Generate a new lighting GUID (so its unique)
		StaticMesh->LightingGuid = FGuid::NewGuid();

		// Set it to use textured lightmaps. Note that Build Lighting will do the error-checking (texcoord index exists for all LODs, etc).
		StaticMesh->LightMapResolution = 64;
		StaticMesh->LightMapCoordinateIndex = 1;

		// Material setup, since there isn't much material information in the Alembic file, 
		UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		check(DefaultMaterial);

		// Material list
		StaticMesh->StaticMaterials.Empty();
		// If there were FaceSets available in the Alembic file use the number of unique face sets as num material entries, otherwise default to one material for the whole mesh
		const uint32 FrameIndex = 0;
		uint32 NumFaceSets = FaceSetNames.Num();

		const bool bCreateMaterial = ImportData->ImportSettings->MaterialSettings.bCreateMaterials;
		for (uint32 MaterialIndex = 0; MaterialIndex < ((NumMaterials != 0) ? NumMaterials : 1); ++MaterialIndex)
		{
			UMaterialInterface* Material = nullptr;
			if (FaceSetNames.IsValidIndex(MaterialIndex))
			{
				Material = RetrieveMaterial(FaceSetNames[MaterialIndex], InParent, Flags);
			}

			StaticMesh->StaticMaterials.Add(( Material != nullptr) ? Material : DefaultMaterial);
		}

		// Get the first LOD for filling it up with geometry, only support one LOD
		FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[0];
		// Set build settings for the static mesh
		SrcModel.BuildSettings.bRecomputeNormals = false;
		SrcModel.BuildSettings.bRecomputeTangents = false;
		SrcModel.BuildSettings.bUseMikkTSpace = false;
		// Generate Lightmaps uvs (no support for importing right now)
		SrcModel.BuildSettings.bGenerateLightmapUVs = true;
		// Set lightmap UV index to 1 since we currently only import one set of UVs from the Alembic Data file
		SrcModel.BuildSettings.DstLightmapIndex = 1;

		// Store the raw mesh within the RawMeshBulkData
		SrcModel.RawMeshBulkData->SaveRawMesh(RawMesh);
		
		//Set the Imported version before calling the build
		StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

		// Build the static mesh (using the build setting etc.) this generates correct tangents using the extracting smoothing group along with the imported Normals data
		StaticMesh->Build(false);

		// No collision generation for now
		StaticMesh->CreateBodySetup();
	}

	return StaticMesh;
}

const TArray<UStaticMesh*> FAbcImporter::ImportAsStaticMesh(UObject* InParent, EObjectFlags Flags)
{
	checkf(ImportData->PolyMeshObjects.Num() > 0, TEXT("No poly meshes found"));

	TArray<UStaticMesh*> StaticMeshes;
	
	const FAbcStaticMeshSettings& Settings = ImportData->ImportSettings->StaticMeshSettings;

	// Check if the user want the meshes separate or merged
	if (Settings.bMergeMeshes)
	{
		// If merging we merge all the raw mesh structures together and generate a static mesh asset from this
		TArray<FString> MergedFaceSetNames;
		TArray<FAbcMeshSample*> Samples;
		uint32 TotalNumMaterials = 0;
		for (TSharedPtr<FAbcPolyMeshObject> Mesh : ImportData->PolyMeshObjects)
		{
			if (Mesh->bShouldImport && Mesh->MeshSamples.Num())
			{
				FAbcMeshSample* Sample = Mesh->MeshSamples[FirstSampleIndex];
				TotalNumMaterials += (Sample->NumMaterials != 0) ? Sample->NumMaterials : 1;

				Samples.Add(Sample);
				if (Mesh->FaceSetNames.Num() > 0)
				{
					MergedFaceSetNames.Append(Mesh->FaceSetNames);
				}
				else
				{
					// Default name
					static const FString DefaultName("NoFaceSetName");
					MergedFaceSetNames.Add(DefaultName);
				}				
			}
		}
		
		// Only merged samples if there are any
		if (Samples.Num())
		{
			FAbcMeshSample* MergedSample = AbcImporterUtilities::MergeMeshSamples(Samples);
			FRawMesh RawMesh;
			GenerateRawMeshFromSample(MergedSample, RawMesh);

			UStaticMesh* StaticMesh = CreateStaticMeshFromRawMesh(InParent, FPaths::GetBaseFilename(ImportData->FilePath), Flags, TotalNumMaterials, MergedFaceSetNames, RawMesh);
			if (StaticMesh)
			{
				StaticMeshes.Add(StaticMesh);
			}
		}
	}
	else
	{
		uint32 MeshIndex = 0;
		for (TSharedPtr<FAbcPolyMeshObject> Mesh : ImportData->PolyMeshObjects)
		{
			if (Mesh->bShouldImport && Mesh->MeshSamples.Num())
			{
				UStaticMesh* StaticMesh = ImportSingleAsStaticMesh(MeshIndex, InParent, Flags);
				if (StaticMesh)
				{
					StaticMeshes.Add(StaticMesh);
				}
			}
			++MeshIndex;
		}
	}


	return StaticMeshes;
}

UGeometryCache* FAbcImporter::ImportAsGeometryCache(UObject* InParent, EObjectFlags Flags)
{
	// Create a GeometryCache instance 
	UGeometryCache* GeometryCache = CreateObjectInstance<UGeometryCache>(InParent, FPaths::GetBaseFilename(InParent->GetName()), Flags);
	
	// Only import data if a valid object was created
	if (GeometryCache)
	{
		// In case this is a reimport operation
		GeometryCache->ClearForReimporting();

		// Load the default material for later usage
		UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		check(DefaultMaterial);

		uint32 MaterialOffset = 0;

		for (TSharedPtr<FAbcPolyMeshObject>& MeshObject : ImportData->PolyMeshObjects)
		{
			UGeometryCacheTrack* Track = nullptr;
			// Determine what kind of GeometryCacheTrack we must create
			if (MeshObject->MeshSamples.Num() > 0)
			{
				if (MeshObject->bConstant)
				{
					// TransformAnimation
					Track = CreateTransformAnimationTrack(MeshObject->Name, MeshObject, GeometryCache, MaterialOffset);
				}
				else
				{
					// FlibookAnimation
					Track = CreateFlipbookAnimationTrack(MeshObject->Name, MeshObject, GeometryCache, MaterialOffset);
				}

				if (Track == nullptr)
				{
					// Import was cancelled
					delete GeometryCache;
					return nullptr;
				}

				// Add materials for this Mesh Object
				const uint32 NumMaterials = (MeshObject->FaceSetNames.Num() > 0) ? MeshObject->FaceSetNames.Num() : 1;
				for (uint32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
				{
					UMaterialInterface* Material = nullptr;
					if (MeshObject->FaceSetNames.IsValidIndex(MaterialIndex))
					{
						Material = RetrieveMaterial(MeshObject->FaceSetNames[MaterialIndex], InParent, Flags);
					}

					GeometryCache->Materials.Add((Material != nullptr) ? Material : DefaultMaterial);
				}
				MaterialOffset += NumMaterials;

				// Get Matrix samples 
				TArray<FMatrix> Matrices;
				TArray<float> SampleTimes;

				// Retrieved cached matrix transformation for this object's hierarchy GUID
				TSharedPtr<FCachedHierarchyTransforms> CachedHierarchyTransforms = ImportData->CachedHierarchyTransforms.FindChecked(MeshObject->HierarchyGuid);
				// Store samples inside the track
				Track->SetMatrixSamples(CachedHierarchyTransforms->MatrixSamples, CachedHierarchyTransforms->TimeSamples);

				// Update Total material count
				ImportData->NumTotalMaterials += Track->GetNumMaterials();

				check(Track != nullptr && "Invalid track data");
				GeometryCache->AddTrack(Track);
			}
		}

		// Update all geometry cache components, TODO move render-data from component to GeometryCache and allow for DDC population
		for (TObjectIterator<UGeometryCacheComponent> CacheIt; CacheIt; ++CacheIt)
		{
			CacheIt->OnObjectReimported(GeometryCache);
		}
	}

	return GeometryCache;
}

TArray<UObject*> FAbcImporter::ImportAsSkeletalMesh(UObject* InParent, EObjectFlags Flags)
{
	// First compress the animation data
	const bool bCompressionResult = CompressAnimationDataUsingPCA(ImportData->ImportSettings->CompressionSettings, true);

	TArray<UObject*> GeneratedObjects;

	if (!bCompressionResult)
	{
		return GeneratedObjects;
	}

	// Enforce to compute normals and tangents for the average sample which forms the base of the skeletal mesh
	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	for (const FCompressedAbcData& CompressedData : ImportData->CompressedMeshData)
	{
		FAbcMeshSample* AverageSample = CompressedData.AverageSample;
		if (ImportData->ImportSettings->NormalGenerationSettings.bForceOneSmoothingGroupPerObject)
		{
			// Set smoothing group indices and calculate smooth normals
			AverageSample->SmoothingGroupIndices.Empty(AverageSample->Indices.Num() / 3);
			AverageSample->SmoothingGroupIndices.AddZeroed(AverageSample->Indices.Num() / 3);
			AverageSample->NumSmoothingGroups = 1;
			AbcImporterUtilities::CalculateSmoothNormals(AverageSample);
		}
		else
		{
			AbcImporterUtilities::CalculateNormals(AverageSample);
			AbcImporterUtilities::GenerateSmoothingGroupsIndices(AverageSample, ImportData->ImportSettings);
			AbcImporterUtilities::CalculateNormalsWithSmoothingGroups(AverageSample, AverageSample->SmoothingGroupIndices, AverageSample->NumSmoothingGroups);
		}		
	}

	// Create a Skeletal mesh instance 
	USkeletalMesh* SkeletalMesh = CreateObjectInstance<USkeletalMesh>(InParent, FPaths::GetBaseFilename( InParent ? InParent->GetName() : ImportData->FilePath), Flags);

	// Only import data if a valid object was created
	if (SkeletalMesh)
	{
		// Touch pre edit change
		SkeletalMesh->PreEditChange(NULL);

		// Retrieve the imported resource structure and allocate a new LOD model
		FSkeletalMeshResource* ImportedResource = SkeletalMesh->GetImportedResource();
		check(ImportedResource->LODModels.Num() == 0);
		ImportedResource->LODModels.Empty();
		new(ImportedResource->LODModels)FStaticLODModel();
		SkeletalMesh->LODInfo.Empty();
		SkeletalMesh->LODInfo.AddZeroed();
		FStaticLODModel& LODModel = ImportedResource->LODModels[0];
		SkeletalMesh->LODInfo[0].TriangleSortSettings.AddZeroed(LODModel.Sections.Num());

		const FMeshBoneInfo BoneInfo(FName(TEXT("RootBone"), FNAME_Add), TEXT("RootBone_Export"), INDEX_NONE);
		const FTransform BoneTransform;
		{
			FReferenceSkeletonModifier RefSkelModifier(SkeletalMesh->RefSkeleton, SkeletalMesh->Skeleton);
			RefSkelModifier.Add(BoneInfo, BoneTransform);
		}


		FAbcMeshSample* MergedMeshSample = new FAbcMeshSample();
		for (const FCompressedAbcData& Data : ImportData->CompressedMeshData)
		{
			AbcImporterUtilities::AppendMeshSample(MergedMeshSample, Data.AverageSample);
		}
		
		// Forced to 1
		ImportedResource->LODModels[0].NumTexCoords = MergedMeshSample->NumUVSets;
		SkeletalMesh->bHasVertexColors = true;

		/* Bounding box according to animation */
		SkeletalMesh->SetImportedBounds(ImportData->ArchiveBounds.GetBox());

		bool bBuildSuccess = false;
		TArray<int32> MorphTargetVertexRemapping;
		TArray<int32> UsedVertexIndicesForMorphs;
		MergedMeshSample->TangentX.Empty();
		MergedMeshSample->TangentY.Empty();
		bBuildSuccess = BuildSkeletalMesh(LODModel, SkeletalMesh->RefSkeleton, MergedMeshSample, MorphTargetVertexRemapping, UsedVertexIndicesForMorphs);

		if (!bBuildSuccess)
		{
			SkeletalMesh->MarkPendingKill();
			return GeneratedObjects;
		}

		// Create the skeleton object
		FString SkeletonName = FString::Printf(TEXT("%s_Skeleton"), *SkeletalMesh->GetName());
		USkeleton* Skeleton = CreateObjectInstance<USkeleton>(InParent, SkeletonName, Flags);

		// Merge bones to the selected skeleton
		check(Skeleton->MergeAllBonesToBoneTree(SkeletalMesh));
		Skeleton->MarkPackageDirty();
		if (SkeletalMesh->Skeleton != Skeleton)
		{
			SkeletalMesh->Skeleton = Skeleton;
			SkeletalMesh->MarkPackageDirty();
		}

		// Create animation sequence for the skeleton
		UAnimSequence* Sequence = CreateObjectInstance<UAnimSequence>(InParent, FString::Printf(TEXT("%s_Animation"), *SkeletalMesh->GetName()), Flags);
		Sequence->SetSkeleton(Skeleton);
		Sequence->SequenceLength = ImportData->ImportLength;
		int32 ObjectIndex = 0;
		uint32 TriangleOffset = 0;
		uint32 WedgeOffset = 0;
		uint32 VertexOffset = 0;

		for (FCompressedAbcData& CompressedData : ImportData->CompressedMeshData)
		{
			FAbcMeshSample* AverageSample = CompressedData.AverageSample;

			if (CompressedData.BaseSamples.Num() > 0)
			{
				const int32 NumBases = CompressedData.BaseSamples.Num();
				int32 NumUsedBases = 0;

				const int32 NumIndices = CompressedData.AverageSample->Indices.Num();

				for (int32 BaseIndex = 0; BaseIndex < NumBases; ++BaseIndex)
				{
					FAbcMeshSample* BaseSample = CompressedData.BaseSamples[BaseIndex];

					AbcImporterUtilities::CalculateNormalsWithSmoothingGroups(BaseSample, AverageSample->SmoothingGroupIndices, AverageSample->NumSmoothingGroups);

					// Create new morph target with name based on object and base index
					UMorphTarget* MorphTarget = NewObject<UMorphTarget>(SkeletalMesh, FName(*FString::Printf(TEXT("Base_%i_%i"), BaseIndex, ObjectIndex)));

					// Setup morph target vertices directly
					TArray<FMorphTargetDelta> MorphDeltas;
					GenerateMorphTargetVertices(BaseSample, MorphDeltas, AverageSample, WedgeOffset, MorphTargetVertexRemapping, UsedVertexIndicesForMorphs, VertexOffset, WedgeOffset);
					MorphTarget->PopulateDeltas(MorphDeltas, 0);

					const float PercentageOfVerticesInfluences = ((float)MorphTarget->MorphLODModels[0].Vertices.Num() / (float)NumIndices) * 100.0f;
					if (PercentageOfVerticesInfluences > ImportData->ImportSettings->CompressionSettings.MinimumNumberOfVertexInfluencePercentage)
					{
						SkeletalMesh->RegisterMorphTarget(MorphTarget);
						MorphTarget->MarkPackageDirty();

						// Set up curves
						const TArray<float>& CurveValues = CompressedData.CurveValues[BaseIndex];
						const TArray<float>& TimeValues = CompressedData.TimeValues[BaseIndex];
						// Morph target stuffies
						FString CurveName = FString::Printf(TEXT("Base_%i_%i"), BaseIndex, ObjectIndex);
						FName ConstCurveName = *CurveName;

						// Sets up the morph target curves with the sample values and time keys
						SetupMorphTargetCurves(Skeleton, ConstCurveName, Sequence, CurveValues, TimeValues);
					}
					else
					{
						MorphTarget->MarkPendingKill();
					}
				}
			}

			Sequence->RawCurveData.RemoveRedundantKeys();

			WedgeOffset += CompressedData.AverageSample->Indices.Num();
			VertexOffset += CompressedData.AverageSample->Vertices.Num();

			const uint32 NumMaterials = CompressedData.MaterialNames.Num();
			for (uint32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
			{
				const FString& MaterialName = CompressedData.MaterialNames[MaterialIndex];
				UMaterialInterface* Material = RetrieveMaterial(MaterialName, InParent, Flags);
				SkeletalMesh->Materials.Add(FSkeletalMaterial(Material, true));	
			}

			++ObjectIndex;
		}
		
		// Set recompute tangent flag on skeletal mesh sections
		for (FSkelMeshSection& Section : SkeletalMesh->GetSourceModel().Sections)
		{
			Section.bRecomputeTangent = true;
		}

		SkeletalMesh->CalculateInvRefMatrices();
		SkeletalMesh->PostEditChange();
		SkeletalMesh->MarkPackageDirty();

		// Retrieve the name mapping container
		const FSmartNameMapping* NameMapping = Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
		Sequence->RawCurveData.RefreshName(NameMapping);
		Sequence->MarkRawDataAsModified();
		Sequence->PostEditChange();
		Sequence->SetPreviewMesh(SkeletalMesh);
		Sequence->MarkPackageDirty();

		Skeleton->SetPreviewMesh(SkeletalMesh);
		Skeleton->PostEditChange();

		GeneratedObjects.Add(SkeletalMesh);
		GeneratedObjects.Add(Skeleton);
		GeneratedObjects.Add(Sequence);

	}
	
	return GeneratedObjects;
}

void FAbcImporter::SetupMorphTargetCurves(USkeleton* Skeleton, FName ConstCurveName, UAnimSequence* Sequence, const TArray<float> &CurveValues, const TArray<float>& TimeValues)
{
	FSmartName NewName;
	Skeleton->AddSmartNameAndModify(USkeleton::AnimCurveMappingName, ConstCurveName, NewName);

	check(Sequence->RawCurveData.AddCurveData(NewName));
	FFloatCurve * NewCurve = static_cast<FFloatCurve *> (Sequence->RawCurveData.GetCurveData(NewName.UID, ERawCurveTrackTypes::RCT_Float));

	for (int32 KeyIndex = 0; KeyIndex < CurveValues.Num(); ++KeyIndex)
	{
		const float& CurveValue = CurveValues[KeyIndex];
		const float& TimeValue = TimeValues[KeyIndex];

		FKeyHandle NewKeyHandle = NewCurve->FloatCurve.AddKey(TimeValue, CurveValue, false);

		ERichCurveInterpMode NewInterpMode = RCIM_Linear;
		ERichCurveTangentMode NewTangentMode = RCTM_Auto;
		ERichCurveTangentWeightMode NewTangentWeightMode = RCTWM_WeightedNone;

		float LeaveTangent = 0.f;
		float ArriveTangent = 0.f;
		float LeaveTangentWeight = 0.f;
		float ArriveTangentWeight = 0.f;

		NewCurve->FloatCurve.SetKeyInterpMode(NewKeyHandle, NewInterpMode);
		NewCurve->FloatCurve.SetKeyTangentMode(NewKeyHandle, NewTangentMode);
		NewCurve->FloatCurve.SetKeyTangentWeightMode(NewKeyHandle, NewTangentWeightMode);
	}
}

const bool FAbcImporter::CompressAnimationDataUsingPCA(const FAbcCompressionSettings& InCompressionSettings, const bool bRunComparison /*= false*/)
{
	// Split up poly mesh objects into constant and animated objects to process
	TArray<TSharedPtr<FAbcPolyMeshObject>> PolyMeshObjectsToCompress;
	TArray<TSharedPtr<FAbcPolyMeshObject>> ConstantPolyMeshObjects;
	for (TSharedPtr<FAbcPolyMeshObject> PolyMeshObject : ImportData->PolyMeshObjects)
	{
		if (PolyMeshObject->bConstantTopology)
		{
			if (PolyMeshObject->bConstant && PolyMeshObject->bConstantTransformation)
			{
				ConstantPolyMeshObjects.Add(PolyMeshObject);
			}
			else if (!PolyMeshObject->bConstant || (InCompressionSettings.bBakeMatrixAnimation && !PolyMeshObject->bConstantTransformation))
			{
				PolyMeshObjectsToCompress.Add(PolyMeshObject);
			}
		}
	}

	bool bResult = true;
	if (PolyMeshObjectsToCompress.Num() > 0)
	{
		// Non merged path
		const uint32 FrameZeroIndex = 0;

		if (InCompressionSettings.bMergeMeshes)
		{
			TArray<FVector> AverageVertexData;
			TArray<FVector> AverageNormalData;
			float MinTime = FLT_MAX;
			float MaxTime = -FLT_MAX;

			FAbcMeshSample MergedZeroFrameSample;
			// Allocate compressed mesh data object
			ImportData->CompressedMeshData.AddDefaulted();
			FCompressedAbcData& CompressedData = ImportData->CompressedMeshData.Last();

			TArray<uint32> ObjectVertexOffsets;
			int32 NumSamples = 0;
			// Populate average frame data, frame zero sample and material names from all objects
			for (const TSharedPtr<FAbcPolyMeshObject>& MeshObject : PolyMeshObjectsToCompress)
			{
				ObjectVertexOffsets.Add(AverageVertexData.Num());

				NumSamples = FMath::Max(NumSamples, MeshObject->MeshSamples.Num());
				AbcImporterUtilities::CalculateAverageFrameData(MeshObject, AverageVertexData, AverageNormalData, MinTime, MaxTime);
				AbcImporterUtilities::AppendMeshSample(&MergedZeroFrameSample, MeshObject->MeshSamples[FrameZeroIndex]);
				AbcImporterUtilities::AppendMaterialNames(MeshObject, CompressedData);
			}

			const uint32 NumVertices = AverageVertexData.Num();
			const uint32 NumMatrixRows = NumVertices * 3;
			const uint32 NumIndices = AverageNormalData.Num();

			TArray<float> OriginalMatrix;
			OriginalMatrix.AddZeroed(NumMatrixRows * NumSamples);

			uint32 ObjectIndex = 0;
			// For each object generate the delta frame data for the PCA compression
			for (const TSharedPtr<FAbcPolyMeshObject>& MeshObject : PolyMeshObjectsToCompress)
			{
				TArray<float> ObjectMatrix;
				uint32 SampleIndex = 0;
				for (const FAbcMeshSample* MeshSample : MeshObject->MeshSamples)
				{
					AbcImporterUtilities::GenerateDeltaFrameDataMatrix(MeshSample->Vertices, AverageVertexData, SampleIndex * NumMatrixRows, ObjectVertexOffsets[ObjectIndex], OriginalMatrix);
					++SampleIndex;
				}
				ObjectIndex++;
			}

			// Perform compression
			TArray<float> OutU, OutV, OutMatrix;
			const uint32 NumUsedSingularValues = PerformSVDCompression(OriginalMatrix, NumMatrixRows, NumSamples, OutU, OutV, InCompressionSettings.BaseCalculationType == EBaseCalculationType::PercentageBased ? InCompressionSettings.PercentageOfTotalBases / 100.0f : 100.0f, InCompressionSettings.BaseCalculationType == EBaseCalculationType::FixedNumber ? InCompressionSettings.MaxNumberOfBases : 0);

			// Set up average frame 
			CompressedData.AverageSample = new FAbcMeshSample(MergedZeroFrameSample);
			FMemory::Memcpy(CompressedData.AverageSample->Vertices.GetData(), AverageVertexData.GetData(), sizeof(FVector) * NumVertices);

			const float FrameStep = (MaxTime - MinTime) / (float)(NumSamples - 1);
			AbcImporterUtilities::GenerateCompressedMeshData(CompressedData, NumUsedSingularValues, NumSamples, OutU, OutV, FrameStep, MinTime);

			if (bRunComparison)
			{
				CompareCompressionResult(OriginalMatrix, NumSamples, NumMatrixRows, NumUsedSingularValues, NumVertices, OutU, OutV, AverageVertexData);
			}
		}
		else
		{
			// Each individual object creates a compressed data object
			for (const TSharedPtr<FAbcPolyMeshObject>& MeshObject : PolyMeshObjectsToCompress)
			{
				const uint32 NumSamples = MeshObject->MeshSamples.Num();
				const uint32 NumVertices = MeshObject->MeshSamples[FrameZeroIndex]->Vertices.Num();
				const uint32 NumMatrixRows = NumVertices * 3;
				const uint32 NumIndices = MeshObject->MeshSamples[FrameZeroIndex]->Indices.Num();

				TArray<FVector> AverageVertexData;
				TArray<FVector> AverageNormalData;
				float MinTime = FLT_MAX;
				float MaxTime = -FLT_MAX;
				AbcImporterUtilities::CalculateAverageFrameData(MeshObject, AverageVertexData, AverageNormalData, MinTime, MaxTime);

				// Setup original matrix from data
				TArray<float> OriginalMatrix;
				AbcImporterUtilities::GenerateDeltaFrameDataMatrix(MeshObject, AverageVertexData, OriginalMatrix);

				// Perform compression
				TArray<float> OutU, OutV, OutMatrix;
				const uint32 NumUsedSingularValues = PerformSVDCompression(OriginalMatrix, NumMatrixRows, NumSamples, OutU, OutV, InCompressionSettings.BaseCalculationType == EBaseCalculationType::PercentageBased ? InCompressionSettings.PercentageOfTotalBases / 100.0f : 100.0f, InCompressionSettings.BaseCalculationType == EBaseCalculationType::FixedNumber ? InCompressionSettings.MaxNumberOfBases : 0);

				// Allocate compressed mesh data object
				ImportData->CompressedMeshData.AddDefaulted();
				FCompressedAbcData& CompressedData = ImportData->CompressedMeshData.Last();
				CompressedData.Guid = MeshObject->HierarchyGuid;
				CompressedData.AverageSample = new FAbcMeshSample(*MeshObject->MeshSamples[FirstSampleIndex]);
				FMemory::Memcpy(CompressedData.AverageSample->Vertices.GetData(), AverageVertexData.GetData(), sizeof(FVector) * NumVertices);

				const float FrameStep = (MaxTime - MinTime) / (float)(NumSamples);
				AbcImporterUtilities::GenerateCompressedMeshData(CompressedData, NumUsedSingularValues, NumSamples, OutU, OutV, FrameStep, MinTime);
				AbcImporterUtilities::AppendMaterialNames(MeshObject, CompressedData);

				if (bRunComparison)
				{
					CompareCompressionResult(OriginalMatrix, NumSamples, NumMatrixRows, NumUsedSingularValues, NumVertices, OutU, OutV, AverageVertexData);
				}
			}
		}
	}
	else
	{
		TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("NoMeshesToProcess", "Unable to compress animation data, no meshes found with Vertex Animation and baked Matrix Animation is turned off."));
		FAbcImportLogger::AddImportMessage(Message);
		bResult = false;
	}

	// Process the constant meshes by only adding them as average samples (without any bases/morphtargets to add as well)
	for (const TSharedPtr<FAbcPolyMeshObject>& MeshObject : ConstantPolyMeshObjects)
	{
		// Allocate compressed mesh data object
		ImportData->CompressedMeshData.AddDefaulted();
		FCompressedAbcData& CompressedData = ImportData->CompressedMeshData.Last();
		CompressedData.Guid = MeshObject->HierarchyGuid;
		CompressedData.AverageSample = new FAbcMeshSample(*MeshObject->MeshSamples[FirstSampleIndex]);
		AbcImporterUtilities::AppendMaterialNames(MeshObject, CompressedData);
	}

	return bResult;
}

void FAbcImporter::CompareCompressionResult(const TArray<float>& OriginalMatrix, const uint32 NumSamples, const uint32 NumRows, const uint32 NumUsedSingularValues, const uint32 NumVertices, const TArray<float>& OutU, const TArray<float>& OutV, const TArray<FVector>& AverageFrame)
{
	// TODO NEED FEEDBACK FOR USER ON COMPRESSION RESULTS
#if 0
	TArray<float> ComparisonMatrix;
	ComparisonMatrix.AddZeroed(OriginalMatrix.Num());
	for (uint32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
	{
		const int32 SampleOffset = (SampleIndex * NumRows);
		const int32 CurveOffset = (SampleIndex * NumUsedSingularValues);
		for (uint32 BaseIndex = 0; BaseIndex < NumUsedSingularValues; ++BaseIndex)
		{
			const int32 BaseOffset = (BaseIndex * NumVertices * 3);
			for (uint32 VertexIndex = 0; VertexIndex < NumVertices; VertexIndex++)
			{
				const int32 VertexOffset = (VertexIndex * 3);
				ComparisonMatrix[VertexOffset + SampleOffset + 0] += OutU[BaseOffset + VertexOffset + 0] * OutV[BaseIndex + CurveOffset];
				ComparisonMatrix[VertexOffset + SampleOffset + 1] += OutU[BaseOffset + VertexOffset + 1] * OutV[BaseIndex + CurveOffset];
				ComparisonMatrix[VertexOffset + SampleOffset + 2] += OutU[BaseOffset + VertexOffset + 2] * OutV[BaseIndex + CurveOffset];
			}
		}
	}

	Eigen::MatrixXf EigenOriginalMatrix;
	EigenHelpers::ConvertArrayToEigenMatrix(OriginalMatrix, NumRows, NumSamples, EigenOriginalMatrix);

	Eigen::MatrixXf EigenComparisonMatrix;
	EigenHelpers::ConvertArrayToEigenMatrix(ComparisonMatrix, NumRows, NumSamples, EigenComparisonMatrix);

	TArray<float> AverageMatrix;
	AverageMatrix.AddZeroed(NumRows * NumSamples);
	
	
	for (int32 Index = 0; Index < AverageFrame.Num(); Index++ )
	{
		const FVector& Vector = AverageFrame[Index];
		for (uint32 i = 0; i < NumSamples; ++i)
		{
			const uint32 IndexOffset = (Index * 3) + (i * NumRows);
			AverageMatrix[IndexOffset + 0] = Vector.X;
			AverageMatrix[IndexOffset + 1] = Vector.Y;
			AverageMatrix[IndexOffset + 2] = Vector.Z;
		}		
	}

	Eigen::MatrixXf EigenAverageMatrix;
	EigenHelpers::ConvertArrayToEigenMatrix(AverageMatrix, NumRows, NumSamples, EigenAverageMatrix);

	Eigen::MatrixXf One = (EigenOriginalMatrix - EigenComparisonMatrix);
	Eigen::MatrixXf Two = (EigenOriginalMatrix - EigenAverageMatrix);

	const float LengthOne = One.squaredNorm();
	const float LengthTwo = Two.squaredNorm();
	const float Distortion = (LengthOne / LengthTwo) * 100.0f;

	// Compare arrays
	for (int32 i = 0; i < ComparisonMatrix.Num(); ++i)
	{
		ensureMsgf(FMath::IsNearlyEqual(OriginalMatrix[i], ComparisonMatrix[i]), TEXT("Difference of %2.10f found"), FMath::Abs(OriginalMatrix[i] - ComparisonMatrix[i]));
	}
#endif 
}

const int32 FAbcImporter::PerformSVDCompression(TArray<float>& OriginalMatrix, const uint32 NumRows, const uint32 NumSamples, TArray<float>& OutU, TArray<float>& OutV, const float InPercentage, const int32 InFixedNumValue)
{
	TArray<float> OutS;
	EigenHelpers::PerformSVD(OriginalMatrix, NumRows, NumSamples, OutU, OutV, OutS);

	// Now we have the new basis data we have to construct the correct morph target data and curves
	const float PercentageBasesUsed = InPercentage;
	const int32 NumNonZeroSingularValues = OutS.Num();
	const int32 NumUsedSingularValues = (InFixedNumValue != 0) ? FMath::Min(InFixedNumValue, (int32)OutS.Num()) : (int32)((float)NumNonZeroSingularValues * PercentageBasesUsed);

	// Pre-multiply the bases with it's singular values
	ParallelFor(NumUsedSingularValues, [&](int32 ValueIndex)
	{
		const float Multiplier = OutS[ValueIndex];
		const int32 ValueOffset = ValueIndex * NumRows;

		for (uint32 RowIndex = 0; RowIndex < NumRows; ++RowIndex)
		{
			OutU[ValueOffset + RowIndex] *= Multiplier;
		}
	});

	UE_LOG(LogAbcImporter, Log, TEXT("Decomposed animation and reconstructed with %i number of bases (full %i, percentage %f, calculated %i)"), NumUsedSingularValues, OutS.Num(), PercentageBasesUsed * 100.0f, NumUsedSingularValues);	
	
	return NumUsedSingularValues;
}

const TArray<UStaticMesh*> FAbcImporter::ReimportAsStaticMesh(UStaticMesh* Mesh)
{
	const FString StaticMeshName = Mesh->GetName();
	const TArray<UStaticMesh*> StaticMeshes = ImportAsStaticMesh(Mesh->GetOuter(), RF_Public | RF_Standalone);

	return StaticMeshes;
}

UGeometryCache* FAbcImporter::ReimportAsGeometryCache(UGeometryCache* GeometryCache)
{
	UGeometryCache* ReimportedCache = ImportAsGeometryCache(GeometryCache->GetOuter(), RF_Public | RF_Standalone);
	return ReimportedCache;
}

TArray<UObject*> FAbcImporter::ReimportAsSkeletalMesh(USkeletalMesh* SkeletalMesh)
{
	TArray<UObject*> ReimportedObjects = ImportAsSkeletalMesh(SkeletalMesh->GetOuter(), RF_Public | RF_Standalone);
	return ReimportedObjects;
}

const TArray<TSharedPtr<FAbcPolyMeshObject>>& FAbcImporter::GetPolyMeshes() const
{
	return ImportData->PolyMeshObjects;
}

const uint32 FAbcImporter::GetNumFrames() const
{
	return (ImportData != nullptr) ? ImportData->NumFrames : 0;
}

const uint32 FAbcImporter::GetStartFrameIndex() const
{
	return (ImportData != nullptr) ? ImportData->MinFrameIndex : 0;
}

const uint32 FAbcImporter::GetEndFrameIndex() const
{
	return (ImportData != nullptr) ? ImportData->MaxFrameIndex : 1;
}

const uint32 FAbcImporter::GetNumMeshTracks() const
{
	return (ImportData != nullptr) ? ImportData->PolyMeshObjects.Num() : 0;
}

void FAbcImporter::GenerateRawMeshFromSample(const uint32 MeshTrackIndex, const uint32 SampleIndex, FRawMesh& RawMesh)
{
	FAbcMeshSample* Sample = ImportData->PolyMeshObjects[MeshTrackIndex]->MeshSamples[SampleIndex];
	GenerateRawMeshFromSample(Sample, RawMesh);
}

void FAbcImporter::GenerateRawMeshFromSample(FAbcMeshSample* Sample, FRawMesh& RawMesh)
{
	// Set vertex data for mesh
	RawMesh.VertexPositions = Sample->Vertices;

	// Copy over per-index based data
	RawMesh.WedgeIndices = Sample->Indices;
	RawMesh.WedgeTangentX = Sample->TangentX;
	RawMesh.WedgeTangentY = Sample->TangentY;
	RawMesh.WedgeTangentZ = Sample->Normals;

	for (uint32 UVIndex = 0; UVIndex < Sample->NumUVSets; ++UVIndex)
	{
		RawMesh.WedgeTexCoords[UVIndex] = Sample->UVs[UVIndex];
	}

	if ( Sample->Colors.Num() )
	{
		for (const FLinearColor& LinearColor : Sample->Colors)
		{
			RawMesh.WedgeColors.Add(LinearColor.ToFColor(false));
		}
	}
	else
	{
		RawMesh.WedgeColors.AddDefaulted(RawMesh.WedgeIndices.Num());
	}

	// Copy over per-face data
	RawMesh.FaceMaterialIndices = Sample->MaterialIndices;
	RawMesh.FaceSmoothingMasks = Sample->SmoothingGroupIndices;
}

UGeometryCacheTrack_FlipbookAnimation* FAbcImporter::CreateFlipbookAnimationTrack(const FString& TrackName, TSharedPtr<FAbcPolyMeshObject>& InMeshObject, UGeometryCache* GeometryCacheParent, const uint32 MaterialOffset)
{
	UGeometryCacheTrack_FlipbookAnimation* Track = NewObject<UGeometryCacheTrack_FlipbookAnimation>(GeometryCacheParent, FName(*TrackName), RF_Public);

	Alembic::Abc::TimeSamplingPtr TimeSampler = InMeshObject->Mesh.getSchema().getTimeSampling();

	FGeometryCacheMeshData PreviousMeshData;
	bool first = true;

	FScopedSlowTask SlowTask(150, FText::FromString(FString(TEXT("Loading Tracks"))));
	SlowTask.MakeDialog(true);

	// We need all mesh data per sample for vertex animation
	const uint32 NumSamples = InMeshObject->NumSamples;
	for (uint32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("TrackName"), FText::FromString(TrackName));
		Arguments.Add(TEXT("SampleIndex"), FText::AsNumber(SampleIndex + 1));
		Arguments.Add(TEXT("NumSamples"), FText::AsNumber(NumSamples));

		SlowTask.EnterProgressFrame(100.0f / (float)NumSamples, FText::Format(LOCTEXT("AbcImporter_CreateFlipbookAnimationTrack", "Loading Track: {TrackName} [Sample {SampleIndex} of {NumSamples}]"), Arguments));

		// Generate the mesh data for this sample
		FGeometryCacheMeshData MeshData;
		GenerateGeometryCacheMeshDataForSample(MeshData, InMeshObject, SampleIndex, MaterialOffset);
				
		// Get the SampleTime		
		const float SampleTime = InMeshObject->MeshSamples[SampleIndex]->SampleTime; 

		// Store sample in track
		Track->AddMeshSample(MeshData, SampleTime);

		PreviousMeshData = MeshData;

		if (GWarn->ReceivedUserCancel())
		{
			delete Track;
			return nullptr;
		}
	}

	return Track;
}

UGeometryCacheTrack_TransformAnimation* FAbcImporter::CreateTransformAnimationTrack(const FString& TrackName, TSharedPtr<FAbcPolyMeshObject>& InMeshObject, UGeometryCache* GeometryCacheParent, const uint32 MaterialOffset)
{
	// Create the TransformAnimationTrack 
	UGeometryCacheTrack_TransformAnimation* Track = NewObject<UGeometryCacheTrack_TransformAnimation>(GeometryCacheParent, FName(*TrackName), RF_Public);

	// Only need to generate GeometryCacheMeshData for the from the first sample
	const int32 MeshTrackIndex = 0;
	FGeometryCacheMeshData MeshData;
	GenerateGeometryCacheMeshDataForSample(MeshData, InMeshObject, MeshTrackIndex, MaterialOffset);

	Track->SetMesh(MeshData);
	return Track;
}

void FAbcImporter::GenerateGeometryCacheMeshDataForSample(FGeometryCacheMeshData& OutMeshData, TSharedPtr<FAbcPolyMeshObject>& InMeshObject, const uint32 SampleIndex, const uint32 MaterialOffset)
{
	check(SampleIndex < (uint32)InMeshObject->NumSamples);
	check(SampleIndex < (uint32)InMeshObject->MeshSamples.Num());
	
	FAbcMeshSample* MeshSample = InMeshObject->MeshSamples[SampleIndex];
	check(MeshSample != nullptr);
	// Bounding box
	OutMeshData.BoundingBox = FBox(MeshSample->Vertices);

	uint32 NumMaterials = MaterialOffset;

	const int32 NumTriangles = MeshSample->Indices.Num() / 3;	
	const uint32 NumSections = MeshSample->NumMaterials ? MeshSample->NumMaterials : 1;

	TArray<TArray<uint32>> SectionIndices;
	SectionIndices.AddDefaulted(NumSections);

	OutMeshData.Vertices.AddZeroed(MeshSample->Normals.Num());

	for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; ++TriangleIndex)
	{
		const int32 SectionIndex = MeshSample->MaterialIndices[TriangleIndex];
		TArray<uint32>& Section = SectionIndices[SectionIndex];

		for (int32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
		{
			const int32 CornerIndex = (TriangleIndex * 3) + VertexIndex;
			const int32 Index = MeshSample->Indices[CornerIndex];
			FDynamicMeshVertex& Vertex = OutMeshData.Vertices[CornerIndex];

			Vertex.Position = MeshSample->Vertices[Index];
			Vertex.SetTangents(MeshSample->TangentX[CornerIndex], MeshSample->TangentY[CornerIndex], MeshSample->Normals[CornerIndex]);
			Vertex.TextureCoordinate = MeshSample->UVs[0][CornerIndex];
			Vertex.Color = MeshSample->Colors[CornerIndex].ToFColor(false);

			Section.Add(CornerIndex);
		}
	}

	OutMeshData.BatchesInfo.AddDefaulted(SectionIndices.Num());

	TArray<uint32>& Indices = OutMeshData.Indices;
	for (uint32 BatchIndex = 0; BatchIndex < NumSections; ++BatchIndex)
	{
		FGeometryCacheMeshBatchInfo& BatchInfo = OutMeshData.BatchesInfo[BatchIndex];
		BatchInfo.StartIndex = Indices.Num();
		BatchInfo.MaterialIndex = NumMaterials;
		NumMaterials++;

		BatchInfo.NumTriangles = SectionIndices[BatchIndex].Num() / 3;
		Indices.Append(SectionIndices[BatchIndex]);
	}
}

bool FAbcImporter::BuildSkeletalMesh(
	FStaticLODModel& LODModel,
	const FReferenceSkeleton& RefSkeleton,
	FAbcMeshSample* Sample,
	/*const TArray<FVector>& Vertices,
	const TArray<uint32>& Indices,
	const TArray<FVector2D>& UVs,
	TArray<FVector>& TangentX,
	TArray<FVector>& TangentY,
	TArray<FVector>& TangentZ,
	const TArray<FColor>& Colours,
	const TArray<int32>& MaterialIndices,
	const TArray<uint32>& SmoothingGroupIndices,
	const uint32 NumMaterials,*/
	TArray<int32>& OutMorphTargetVertexRemapping,
	TArray<int32>& OutUsedVertexIndicesForMorphs
	)
{
	// Module manager is not thread safe, so need to prefetch before parallelfor
	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");

	const bool bComputeNormals = (Sample->Normals.Num() == 0);
	const bool bComputeTangents = (Sample->TangentX.Num() == 0) || (Sample->TangentY.Num() == 0);

	// Compute normals/tangents if needed
	if (bComputeNormals || bComputeTangents)
	{
		uint32 TangentOptions = 0;
		MeshUtilities.CalculateTangents(Sample->Vertices, Sample->Indices, Sample->UVs[0], Sample->SmoothingGroupIndices, TangentOptions, Sample->TangentX, Sample->TangentY, Sample->Normals);
	}

	// Populate faces
	const uint32 NumFaces = Sample->Indices.Num() / 3;
	TArray<FMeshFace> Faces;
	Faces.AddZeroed(NumFaces);

	TArray<FMeshSection> MeshSections;
	MeshSections.AddDefaulted(Sample->NumMaterials);

	// Process all the faces and add to their respective mesh section
	for (uint32 FaceIndex = 0; FaceIndex < NumFaces; ++FaceIndex)
	{
		const uint32 FaceOffset = FaceIndex * 3;
		const int32 MaterialIndex = Sample->MaterialIndices[FaceIndex];

		check(MeshSections.IsValidIndex(MaterialIndex));

		FMeshSection& Section = MeshSections[MaterialIndex];
		Section.MaterialIndex = MaterialIndex;
		Section.NumUVSets = Sample->NumUVSets;
	
		for (uint32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
		{
			LODModel.MaxImportVertex = FMath::Max<int32>(LODModel.MaxImportVertex, Sample->Indices[FaceOffset + VertexIndex]);

			Section.OriginalIndices.Add(FaceOffset + VertexIndex);
			Section.Indices.Add(Sample->Indices[FaceOffset + VertexIndex]);
			Section.TangentX.Add(Sample->TangentX[FaceOffset + VertexIndex]);
			Section.TangentY.Add(Sample->TangentY[FaceOffset + VertexIndex]);
			Section.TangentZ.Add(Sample->Normals[FaceOffset + VertexIndex]);

			for (uint32 UVIndex = 0; UVIndex < Sample->NumUVSets; ++UVIndex)
			{
				Section.UVs[UVIndex].Add(Sample->UVs[UVIndex][FaceOffset + VertexIndex]);
			}		
			
			Section.Colors.Add(Sample->Colors[FaceOffset + VertexIndex].ToFColor(false));
		}

		++Section.NumFaces;
	}

	// Sort the vertices by z value
	MeshSections.Sort([](const FMeshSection& A, const FMeshSection& B) { return A.MaterialIndex < B.MaterialIndex; });

	// Create Skeletal mesh LOD sections
	LODModel.Sections.Empty(MeshSections.Num());
	LODModel.NumVertices = 0;
	if (LODModel.MultiSizeIndexContainer.IsIndexBufferValid())
	{
		LODModel.MultiSizeIndexContainer.GetIndexBuffer()->Empty();
	}

	TArray<uint32> RawPointIndices;
	TArray< TArray<uint32> > VertexIndexRemap;
	VertexIndexRemap.Empty(MeshSections.Num());

	// Create actual skeletal mesh sections
	for (int32 SectionIndex = 0; SectionIndex < MeshSections.Num(); ++SectionIndex)
	{
		const FMeshSection& SourceSection = MeshSections[SectionIndex];
		FSkelMeshSection& TargetSection = *new(LODModel.Sections) FSkelMeshSection();
		TargetSection.MaterialIndex = (uint16)SourceSection.MaterialIndex;
		TargetSection.NumTriangles = SourceSection.NumFaces;
		TargetSection.BaseVertexIndex = LODModel.NumVertices;

		// Separate the section's vertices into rigid and soft vertices.
		TArray<uint32>& ChunkVertexIndexRemap = *new(VertexIndexRemap)TArray<uint32>();
		ChunkVertexIndexRemap.AddUninitialized(SourceSection.NumFaces * 3);

		TMultiMap<uint32, uint32> FinalVertices;
		TMap<FSoftSkinVertex*, uint32> VertexMapping;
		
		// Reused soft vertex
		FSoftSkinVertex NewVertex;

		uint32 VertexOffset = 0;
		// Generate Soft Skin vertices (used by the skeletal mesh)
		for (uint32 FaceIndex = 0; FaceIndex < SourceSection.NumFaces; ++FaceIndex)
		{
			const uint32 FaceOffset = FaceIndex * 3;
			const int32 MaterialIndex = Sample->MaterialIndices[FaceIndex];

			for (uint32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
			{
				const uint32 Index = SourceSection.Indices[FaceOffset + VertexIndex];

				TArray<uint32> DuplicateVertexIndices;
				FinalVertices.MultiFind(Index, DuplicateVertexIndices);
				
				// Populate vertex data
				NewVertex.Position = Sample->Vertices[Index];
				NewVertex.TangentX = SourceSection.TangentX[FaceOffset + VertexIndex];
				NewVertex.TangentY = SourceSection.TangentY[FaceOffset + VertexIndex];
				NewVertex.TangentZ = SourceSection.TangentZ[FaceOffset + VertexIndex];
				for (uint32 UVIndex = 0; UVIndex < SourceSection.NumUVSets; ++UVIndex)
				{
					NewVertex.UVs[UVIndex] = SourceSection.UVs[UVIndex][FaceOffset + VertexIndex];
				}
				
				NewVertex.Color = SourceSection.Colors[FaceOffset + VertexIndex];

				// Set up bone influence (only using one bone so maxed out weight)
				FMemory::Memzero(NewVertex.InfluenceBones);
				FMemory::Memzero(NewVertex.InfluenceWeights);
				NewVertex.InfluenceWeights[0] = 255;
				
				int32 FinalVertexIndex = INDEX_NONE;
				if (DuplicateVertexIndices.Num())
				{
					for (const uint32 DuplicateVertexIndex : DuplicateVertexIndices)
					{
						if (AbcImporterUtilities::AreVerticesEqual(TargetSection.SoftVertices[DuplicateVertexIndex], NewVertex))
						{
							// Use the existing vertex
							FinalVertexIndex = DuplicateVertexIndex;
							break;
						}						
					}
				}

				if (FinalVertexIndex == INDEX_NONE)
				{
					FinalVertexIndex = TargetSection.SoftVertices.Add(NewVertex);
#if PRINT_UNIQUE_VERTICES
					FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Vert - P(%.2f, %.2f,%.2f) N(%.2f, %.2f,%.2f) TX(%.2f, %.2f,%.2f) TY(%.2f, %.2f,%.2f) UV(%.2f, %.2f)\n"), NewVertex.Position.X, NewVertex.Position.Y, NewVertex.Position.Z, SourceSection.TangentX[FaceOffset + VertexIndex].X, 
						SourceSection.TangentZ[FaceOffset + VertexIndex].X, SourceSection.TangentZ[FaceOffset + VertexIndex].Y, SourceSection.TangentZ[FaceOffset + VertexIndex].Z, SourceSection.TangentX[FaceOffset + VertexIndex].Y, SourceSection.TangentX[FaceOffset + VertexIndex].Z, SourceSection.TangentY[FaceOffset + VertexIndex].X, SourceSection.TangentY[FaceOffset + VertexIndex].Y, SourceSection.TangentY[FaceOffset + VertexIndex].Z, NewVertex.UVs[0].X, NewVertex.UVs[0].Y);
#endif

					FinalVertices.Add(Index, FinalVertexIndex);
					OutUsedVertexIndicesForMorphs.Add(Index);
					OutMorphTargetVertexRemapping.Add(SourceSection.OriginalIndices[FaceOffset + VertexIndex]);
				}

				RawPointIndices.Add(FinalVertexIndex);
				ChunkVertexIndexRemap[VertexOffset] = TargetSection.BaseVertexIndex + FinalVertexIndex;
				++VertexOffset;
			}
		}

		LODModel.NumVertices += TargetSection.SoftVertices.Num();
		TargetSection.NumVertices = TargetSection.SoftVertices.Num();

		// Only need first bone from active bone indices
		TargetSection.BoneMap.Add(0);

		TargetSection.CalcMaxBoneInfluences();
	}

	// Only using bone zero
	LODModel.ActiveBoneIndices.Add(0);

	// Copy raw point indices to LOD model.
	LODModel.RawPointIndices.RemoveBulkData();
	if (RawPointIndices.Num())
	{
		LODModel.RawPointIndices.Lock(LOCK_READ_WRITE);
		void* Dest = LODModel.RawPointIndices.Realloc(RawPointIndices.Num());
		FMemory::Memcpy(Dest, RawPointIndices.GetData(), LODModel.RawPointIndices.GetBulkDataSize());
		LODModel.RawPointIndices.Unlock();
	}
	LODModel.MultiSizeIndexContainer.CreateIndexBuffer((LODModel.NumVertices < MAX_uint16) ? sizeof(uint16) : sizeof(uint32));

	// Finish building the sections.
	for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
	{
		FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

		const TArray<uint32>& SectionIndices = MeshSections[SectionIndex].Indices;
		FRawStaticIndexBuffer16or32Interface* IndexBuffer = LODModel.MultiSizeIndexContainer.GetIndexBuffer();
		Section.BaseIndex = IndexBuffer->Num();
		const int32 NumIndices = SectionIndices.Num();
		const TArray<uint32>& SectionVertexIndexRemap = VertexIndexRemap[SectionIndex];
		for (int32 Index = 0; Index < NumIndices; Index++)
		{
			uint32 VertexIndex = SectionVertexIndexRemap[Index];
			IndexBuffer->AddItem(VertexIndex);
		}
	}

	// Build the adjacency index buffer used for tessellation.
	TArray<FSoftSkinVertex> SoftSkinVertices;
	LODModel.GetVertices(SoftSkinVertices);

	FMultiSizeIndexContainerData IndexData;
	LODModel.MultiSizeIndexContainer.GetIndexBufferData(IndexData);

	FMultiSizeIndexContainerData AdjacencyIndexData;
	AdjacencyIndexData.DataTypeSize = IndexData.DataTypeSize;

	MeshUtilities.BuildSkeletalAdjacencyIndexBuffer(SoftSkinVertices, LODModel.NumTexCoords, IndexData.Indices, AdjacencyIndexData.Indices);
	LODModel.AdjacencyMultiSizeIndexContainer.RebuildIndexBuffer(AdjacencyIndexData);

	// Compute the required bones for this model.
	USkeletalMesh::CalculateRequiredBones(LODModel, RefSkeleton, NULL);

	return true;
}

void FAbcImporter::GenerateMorphTargetVertices(FAbcMeshSample* BaseSample, TArray<FMorphTargetDelta> &MorphDeltas, FAbcMeshSample* AverageSample, uint32 WedgeOffset, const TArray<int32>& RemapIndices, const TArray<int32>& UsedVertexIndicesForMorphs, const uint32 VertexOffset, const uint32 IndexOffset)
{
	FMorphTargetDelta MorphVertex;
	const uint32 NumberOfUsedVertices = UsedVertexIndicesForMorphs.Num();	
	for (uint32 VertIndex = 0; VertIndex < NumberOfUsedVertices; ++VertIndex)
	{
		const int32 UsedVertexIndex = UsedVertexIndicesForMorphs[VertIndex] - VertexOffset;
		const uint32 UsedNormalIndex = RemapIndices[VertIndex] - IndexOffset;

		if (UsedVertexIndex >= 0 && UsedVertexIndex < BaseSample->Vertices.Num())
		{			
			// Position delta
			MorphVertex.PositionDelta = BaseSample->Vertices[UsedVertexIndex] - AverageSample->Vertices[UsedVertexIndex];
			// Tangent delta
			MorphVertex.TangentZDelta = BaseSample->Normals[UsedNormalIndex] - AverageSample->Normals[UsedNormalIndex];
			// Index of base mesh vert this entry is to modify
			MorphVertex.SourceIdx = VertIndex;
			MorphDeltas.Add(MorphVertex);
		}
	}
}

UMaterialInterface* FAbcImporter::RetrieveMaterial(const FString& MaterialName, UObject* InParent, EObjectFlags Flags)
{
	UMaterialInterface* Material = nullptr;
	UMaterialInterface** CachedMaterial = ImportData->MaterialMap.Find(MaterialName);
	if (CachedMaterial)
	{
		Material = *CachedMaterial;
		// Material could have been deleted if we're overriding/reimporting an asset
		if (Material->IsValidLowLevel())
		{
			if (Material->GetOuter() == GetTransientPackage())
			{
				UMaterial* ExistingTypedObject = FindObject<UMaterial>(InParent, *MaterialName);
				if (!ExistingTypedObject)
				{
					// This is in for safety, as we do not expect this to happen
					UObject* ExistingObject = FindObject<UObject>(InParent, *MaterialName);
					if (ExistingObject)
					{
						return nullptr;
					}

					Material->Rename(*MaterialName, InParent);				
					Material->SetFlags(Flags);
					FAssetRegistryModule::AssetCreated(Material);
				}
				else
				{
					ExistingTypedObject->PreEditChange(nullptr);
					Material = ExistingTypedObject;
				}
			}
		}
		else
		{
			// In this case recreate the material
			Material = NewObject<UMaterial>(InParent, *MaterialName);
			Material->SetFlags(Flags);
			FAssetRegistryModule::AssetCreated(Material);
		}
	}
	else
	{
		Material = UMaterial::GetDefaultMaterial(MD_Surface);
		check(Material);
	}

	return Material;
}

void FAbcImporter::GetMatrixSamplesForGUID(const FGuid& InGUID, const float StartSampleTime, const float EndSampleTime, TArray<FMatrix>& MatrixSamples, TArray<float>& SampleTimes, bool& OutConstantTransform)
{
	bool bConstantTransforms = true;
	const TArray<TSharedPtr<FAbcTransformObject>>* TransformHierarchyPtr = ImportData->Hierarchies.Find(InGUID);
	if (TransformHierarchyPtr)
	{
		const TArray<TSharedPtr<FAbcTransformObject>>& TransformHierarchy = *TransformHierarchyPtr;
		const uint32 HierarchyDepth = TransformHierarchy.Num();
		if (HierarchyDepth > 1)
		{
			const uint32 NumSamples = TransformHierarchy[0]->MatrixSamples.Num();
			MatrixSamples.Empty(NumSamples);
			MatrixSamples.AddZeroed(NumSamples);
			SampleTimes.Append(TransformHierarchy[0]->TimeSamples);
			for (int32 HierarchyIndex = HierarchyDepth - 1; HierarchyIndex >= 0; --HierarchyIndex)
			{
				TSharedPtr<FAbcTransformObject> Object = TransformHierarchy[HierarchyIndex];
				bConstantTransforms &= Object->bConstant;
				check(Object->MatrixSamples.Num() == NumSamples);

				if (Object->bConstant)
				{
					if (HierarchyIndex == (HierarchyDepth - 1))
					{
						for (uint32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
						{
							MatrixSamples[SampleIndex] = Object->MatrixSamples[0];
						}
					}
				}
				else
				{
					ParallelFor(NumSamples, [&](const int32 SampleIndex)
					{
						if (HierarchyIndex != (HierarchyDepth - 1))
						{
							MatrixSamples[SampleIndex] *= Object->MatrixSamples[SampleIndex];
						}
						else
						{
							MatrixSamples[SampleIndex] = Object->MatrixSamples[SampleIndex];
						}
					});
				}
			}

		}
		else
		{
			const TSharedPtr<FAbcTransformObject> Object = TransformHierarchy[0];
			bConstantTransforms &= Object->bConstant;
			MatrixSamples.Append(Object->MatrixSamples);
			SampleTimes.Append(Object->TimeSamples);
		}

		if (bConstantTransforms)
		{
			MatrixSamples.SetNum(1);
			SampleTimes.SetNum(1);
		}
	}
	else
	{
		// No entries in the hierarchy append constant identity matrix and sample time
		MatrixSamples.Add(FMatrix::Identity);
		SampleTimes.Add(0.0f);
	}

	if (!bConstantTransforms)
	{
		// Remove matrix samples that fall outside of the import range and remap the remaining samples
		uint32 ImportStart = 0, ImportEnd = 0;
		for (int32 SampleIndex = 0; SampleIndex < MatrixSamples.Num(); ++SampleIndex)
		{
			if (SampleTimes[SampleIndex] >= StartSampleTime)
			{	
			    // Substract start sample time in order to remap the samples correctly   
				SampleTimes[SampleIndex] -= StartSampleTime;
			}
			else
			{
				ImportStart = SampleIndex;
			}

			if (SampleTimes[SampleIndex] <= EndSampleTime)
			{
				ImportEnd = SampleIndex;
			}
		}

		// Remove trailing samples
		if (ImportEnd != MatrixSamples.Num() - 1)
		{
			MatrixSamples.RemoveAt(ImportEnd, (MatrixSamples.Num() - 1) - ImportEnd);
			SampleTimes.RemoveAt(ImportEnd, (SampleTimes.Num() - 1) - ImportEnd);
		}
		
		// Remove front samples
		if (ImportStart != 0)
		{
			MatrixSamples.RemoveAt(0, ImportStart);
			SampleTimes.RemoveAt(0, ImportStart);
		}

		if (MatrixSamples.Num() == 1)
		{
			bConstantTransforms = true;
		}
	}

	AbcImporterUtilities::ApplyConversion(MatrixSamples, ImportData->ImportSettings->ConversionSettings);

	OutConstantTransform = bConstantTransforms;
}

void FAbcImporter::CacheHierarchyTransforms(const float StartSampleTime, const float EndSampleTime)
{
	for (TSharedPtr<FAbcPolyMeshObject>& PolyMeshObject : ImportData->PolyMeshObjects)
	{
		TSharedPtr<FCachedHierarchyTransforms> CachedTransforms = TSharedPtr<FCachedHierarchyTransforms>( new FCachedHierarchyTransforms());
		GetMatrixSamplesForGUID(PolyMeshObject->HierarchyGuid, StartSampleTime, EndSampleTime, CachedTransforms->MatrixSamples, CachedTransforms->TimeSamples, PolyMeshObject->bConstantTransformation);
		ImportData->CachedHierarchyTransforms.Add(PolyMeshObject->HierarchyGuid, CachedTransforms);
	}
}

#undef LOCTEXT_NAMESPACE
