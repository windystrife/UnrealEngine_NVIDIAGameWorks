// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AbcMeshDataImportRunnable.h"
#include "AbcImportData.h"
#include "AbcImportUtilities.h"
#include "Stats/StatsMisc.h"

DEFINE_LOG_CATEGORY_STATIC(LogAlembicImport, Log, All);

#define LOCTEXT_NAMESPACE "AbcImportRunnable"

namespace AbcMeshImporter
{
	static const int32 ThreadStackSize = 4 * 1024 * 1024;
};

FAbcMeshDataImportRunnable::FAbcMeshDataImportRunnable(FAbcImportData* InImportData, const int32 InStartFrameIndex, const int32 InStopFrameIndex,  const float InTimeStep)
	: ImportData(InImportData)
	, StartFrameIndex(InStartFrameIndex)
	, StopFrameIndex(InStopFrameIndex)
	, TimeStep(InTimeStep)
	, bImportSuccesful(true)
{
	WorkerThread = FRunnableThread::Create(this, TEXT("FAbcMeshDataImportRunnable"), AbcMeshImporter::ThreadStackSize, TPri_AboveNormal);
}

FAbcMeshDataImportRunnable::~FAbcMeshDataImportRunnable()
{
	// Clean up data and thread
	ImportData = nullptr;
	delete WorkerThread;
	WorkerThread = nullptr;
}

bool FAbcMeshDataImportRunnable::Init()
{
	// Check if initialised data is valid
	checkf(ImportData != nullptr, TEXT("Invalid ImportData found"));
	checkf((StopFrameIndex - StartFrameIndex) > 0, TEXT("Invalid frame span to process"));
	checkf(StartFrameIndex >= 0 && StopFrameIndex > 0, TEXT("Invalid start or stop frame index found"));

	return true;
}

uint32 FAbcMeshDataImportRunnable::Run()
{
	SCOPE_LOG_TIME(TEXT("Alembic_FAbcMeshDataImportRunnable::Run"), nullptr);
		
	UE_LOG(LogAlembicImport, Display, TEXT("Running import for frame %i to frame %i"), StartFrameIndex, StopFrameIndex);
	
	const int32 NumMeshTracks = ImportData->PolyMeshObjects.Num();	
	const int32 FrameSpan = StopFrameIndex - StartFrameIndex;
	const int32 FrameOffset = ImportData->ImportSettings->SamplingSettings.FrameStart;
	for (TSharedPtr< FAbcPolyMeshObject>& PolyMeshObject : ImportData->PolyMeshObjects)
	{		
		Alembic::AbcGeom::IPolyMesh PolyMesh = PolyMeshObject->Mesh;
		check(PolyMesh.valid());

		Alembic::AbcGeom::IPolyMeshSchema& Schema = PolyMesh.getSchema();
		check(Schema.valid());

		for (int32 FrameIndex = StartFrameIndex; FrameIndex < StopFrameIndex; ++FrameIndex)
		{
			// Do not process meshes that have constant geometry beyond the first frame we need to sample
			if (PolyMeshObject->bConstant &&  PolyMeshObject->bConstantTransformation && FrameIndex > FrameOffset)
			{
				break;
			}
			
			// No data for this frame index available (empty frames at beginning of sequence)
			if (PolyMeshObject->StartFrameIndex > (uint32)FrameIndex)
			{
				continue;
			}

			// Determine sample time from frame index and time-step
			const float SampleTime = FrameIndex * TimeStep;
			
			// Generate mesh sample data from the Alembic Poly Mesh Schema
			Alembic::Abc::ISampleSelector Selector = AbcImporterUtilities::GenerateAlembicSampleSelector<double>((double)SampleTime);
			FAbcMeshSample* Sample = AbcImporterUtilities::GenerateAbcMeshSampleForFrame(Schema, Selector, FrameIndex == FrameOffset);

			// If we failed to create a sample skip it
			if (Sample == nullptr)
			{
				TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("InvalidFrameForMeshObject", "Invalid or empty frame number {0} in {1}, skipping frame."), FText::FromString( FString::FromInt(FrameIndex) ), FText::FromString(PolyMeshObject->Name)));
				FAbcImportLogger::AddImportMessage(Message);
				continue;
			}
			
			// Set the sample time according to the current frame index we are at
			Sample->SampleTime = (FrameIndex - FrameOffset) * TimeStep;
			
			// Only generate Smoothing groups when Normals are available
			if (Sample->Normals.Num() > 0)
			{
				// Set smoothing group data according to whether or not the user opted to force one smoothing group per object
				if (ImportData->ImportSettings->NormalGenerationSettings.bForceOneSmoothingGroupPerObject)
				{
					Sample->SmoothingGroupIndices.AddZeroed(Sample->Indices.Num() / 3);
					Sample->NumSmoothingGroups = 1;					
				}
				else
				{
					AbcImporterUtilities::GenerateSmoothingGroupsIndices(Sample, ImportData->ImportSettings);
				}
			}

			// Store the generated sample data
			PolyMeshObject->MeshSamples[FrameIndex - FrameOffset] = Sample;
		}
	}

	return 0;
}

void FAbcMeshDataImportRunnable::Wait()
{
	WorkerThread->WaitForCompletion();
}

const bool FAbcMeshDataImportRunnable::WasSuccesful() const
{
	return bImportSuccesful;
}

#undef LOCTEXT_NAMESPACE // "AbcImportRunnable"