// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ThumbnailRendering/ThumbnailManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/Material.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "Engine/StaticMesh.h"
#include "UnrealClient.h"
#include "Engine/TextureCube.h"

#include "ImageUtils.h"


DEFINE_LOG_CATEGORY_STATIC(LogThumbnailManager, Log, All);

//////////////////////////////////////////////////////////////////////////

UThumbnailManager* UThumbnailManager::ThumbnailManagerSingleton = nullptr;

UThumbnailManager::UThumbnailManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (!IsRunningCommandlet())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinder<UStaticMesh> EditorCubeMesh;
			ConstructorHelpers::FObjectFinder<UStaticMesh> EditorSphereMesh;
			ConstructorHelpers::FObjectFinder<UStaticMesh> EditorCylinderMesh;
			ConstructorHelpers::FObjectFinder<UStaticMesh> EditorPlaneMesh;
			ConstructorHelpers::FObjectFinder<UStaticMesh> EditorSkySphereMesh;
			ConstructorHelpers::FObjectFinder<UMaterial> FloorPlaneMaterial;
			ConstructorHelpers::FObjectFinder<UTextureCube> DaylightAmbientCubemap;
			FConstructorStatics()
				: EditorCubeMesh(TEXT("/Engine/EditorMeshes/EditorCube"))
				, EditorSphereMesh(TEXT("/Engine/EditorMeshes/EditorSphere"))
				, EditorCylinderMesh(TEXT("/Engine/EditorMeshes/EditorCylinder"))
				, EditorPlaneMesh(TEXT("/Engine/EditorMeshes/EditorPlane"))
				, EditorSkySphereMesh(TEXT("/Engine/EditorMeshes/EditorSkySphere"))
				, FloorPlaneMaterial(TEXT("/Engine/EditorMaterials/Thumbnails/FloorPlaneMaterial"))
				, DaylightAmbientCubemap(TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		EditorCube = ConstructorStatics.EditorCubeMesh.Object;
		EditorSphere = ConstructorStatics.EditorSphereMesh.Object;
		EditorCylinder = ConstructorStatics.EditorCylinderMesh.Object;
		EditorPlane = ConstructorStatics.EditorPlaneMesh.Object;
		EditorSkySphere = ConstructorStatics.EditorSkySphereMesh.Object;
		FloorPlaneMaterial = ConstructorStatics.FloorPlaneMaterial.Object;
		AmbientCubemap = ConstructorStatics.DaylightAmbientCubemap.Object;

		SetupCheckerboardTexture();
	}
}

void UThumbnailManager::Initialize(void)
{
	if (bIsInitialized == false)
	{
		InitializeRenderTypeArray(RenderableThumbnailTypes);

		bIsInitialized = true;
	}
}

void UThumbnailManager::InitializeRenderTypeArray(TArray<FThumbnailRenderingInfo>& ThumbnailRendererTypes)
{
	// Loop through setting up each thumbnail entry
	for (int32 Index = 0; Index < ThumbnailRendererTypes.Num(); Index++)
	{
		FThumbnailRenderingInfo& RenderInfo = ThumbnailRendererTypes[Index];

		// Load the class that this is for
		if (RenderInfo.ClassNeedingThumbnailName.Len() > 0)
		{
			// Try to load the specified class
			RenderInfo.ClassNeedingThumbnail = LoadObject<UClass>(nullptr, *RenderInfo.ClassNeedingThumbnailName, nullptr, LOAD_None, nullptr);
		}

		if (RenderInfo.RendererClassName.Len() > 0)
		{
			// Try to create the renderer object by loading its class and
			// constructing one
			UClass* RenderClass = LoadObject<UClass>(nullptr, *RenderInfo.RendererClassName, nullptr, LOAD_None, nullptr);
			if (RenderClass != nullptr)
			{
				RenderInfo.Renderer = NewObject<UThumbnailRenderer>(GetTransientPackage(), RenderClass);
			}
		}

		// Add this to the map if it created the renderer component
		if (RenderInfo.Renderer != nullptr)
		{
			RenderInfoMap.Add(RenderInfo.ClassNeedingThumbnail, &RenderInfo);
		}
	}
}

FThumbnailRenderingInfo* UThumbnailManager::GetRenderingInfo(UObject* Object)
{
	// If something may have been GCed, empty the map so we don't crash
	if (bMapNeedsUpdate == true)
	{
		RenderInfoMap.Empty();
		bMapNeedsUpdate = false;
	}

	check(Object);

	TArray<FThumbnailRenderingInfo>& ThumbnailTypes = RenderableThumbnailTypes;

	// Get the class to check against.
	UClass *ClassToCheck = ClassToCheck = Object->GetClass();
	
	// Search for the cached entry and do the slower if not found
	FThumbnailRenderingInfo* RenderInfo = RenderInfoMap.FindRef(ClassToCheck);
	if (RenderInfo == nullptr)
	{
		// Loop through searching for the right thumbnail entry
		for (int32 Index = ThumbnailTypes.Num() - 1; (Index >= 0) && (RenderInfo == nullptr); Index--)
		{
			RenderInfo = &ThumbnailTypes[Index];

			// See if this thumbnail renderer will work for the specified class or
			// if there is some data reason not to render the thumbnail
			if ((ClassToCheck->IsChildOf(RenderInfo->ClassNeedingThumbnail) == false) || (RenderInfo->Renderer == nullptr))
			{
				RenderInfo = nullptr;
			}
		}

		// Make sure to add it to the cache if it is missing
		RenderInfoMap.Add(ClassToCheck, (RenderInfo != nullptr) ? RenderInfo : &NotSupported);
	}

	if ( RenderInfo && RenderInfo->Renderer )
	{
		if ( !RenderInfo->Renderer->CanVisualizeAsset(Object) )
		{
			// This is an asset with a thumbnail renderer, but it can't visualized (i.e it is something like a blueprint that doesn't contain any visible primitive components)
			RenderInfo = nullptr;
		}
	}

	// Check to see if this object is the "not supported" type or not
	if (RenderInfo == &NotSupported)
	{
		RenderInfo = nullptr;
	}
	
	return RenderInfo;
}

void UThumbnailManager::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	// Just mark us as dirty so that the cache is rebuilt
	bMapNeedsUpdate = true;
}

void UThumbnailManager::RegisterCustomRenderer(UClass* Class, TSubclassOf<UThumbnailRenderer> RendererClass)
{
	check(Class != nullptr);
	check(*RendererClass != nullptr);

	const FString NewClassPathName = Class->GetPathName();

	// Verify that this class isn't already registered
	for (int32 Index = 0; Index < RenderableThumbnailTypes.Num(); ++Index)
	{
		if (ensure(RenderableThumbnailTypes[Index].ClassNeedingThumbnailName != NewClassPathName))
		{
		}
		else
		{
			return;
		}
	}

	// Register the new class
	FThumbnailRenderingInfo& Info = *(new (RenderableThumbnailTypes) FThumbnailRenderingInfo());
	Info.ClassNeedingThumbnailName = NewClassPathName;
	Info.ClassNeedingThumbnail = Class;
	Info.Renderer = NewObject<UThumbnailRenderer>(GetTransientPackage(), RendererClass);
	Info.RendererClassName = RendererClass->GetPathName();

	bMapNeedsUpdate = true;
}

void UThumbnailManager::UnregisterCustomRenderer(UClass* Class)
{
	check(Class != nullptr);

	const FString OldClassPathName = Class->GetPathName();

	for (int32 Index = 0; Index < RenderableThumbnailTypes.Num(); )
	{
		if (RenderableThumbnailTypes[Index].ClassNeedingThumbnailName == OldClassPathName)
		{
			RenderableThumbnailTypes.RemoveAtSwap(Index);
		}
		else
		{
			++Index;
		}
	}

	bMapNeedsUpdate = true;
}

UThumbnailManager& UThumbnailManager::Get()
{
	// Create it if we need to
	if (ThumbnailManagerSingleton == nullptr)
	{
		FString ClassName = GetDefault<UThumbnailManager>()->ThumbnailManagerClassName;
		if (!ClassName.IsEmpty())
		{
			// Try to load the specified class
			UClass* Class = LoadObject<UClass>(nullptr, *ClassName, nullptr, LOAD_None, nullptr);
			if (Class != nullptr)
			{
				// Create an instance of this class
				ThumbnailManagerSingleton = NewObject<UThumbnailManager>(GetTransientPackage(), Class);
			}
		}

		// If the class couldn't be loaded or is the wrong type, fallback to the default
		if (ThumbnailManagerSingleton == nullptr)
		{
			ThumbnailManagerSingleton = NewObject<UThumbnailManager>();
		}

		// Keep the singleton alive
		ThumbnailManagerSingleton->AddToRoot();

		// Tell it to load all of its classes
		ThumbnailManagerSingleton->Initialize();
	}

	return *ThumbnailManagerSingleton;
}

void UThumbnailManager::SetupCheckerboardTexture()
{
	if (CheckerboardTexture)
	{
		return;
	}

	CheckerboardTexture = FImageUtils::CreateCheckerboardTexture(FColor(128, 128, 128), FColor(64, 64, 64), 32);
}

bool UThumbnailManager::CaptureProjectThumbnail(FViewport* Viewport, const FString& OutputFilename, bool bUseSCCIfPossible)
{
	const uint32 AutoScreenshotSize = 192;

	//capture the thumbnail
	uint32 SrcWidth = Viewport->GetSizeXY().X;
	uint32 SrcHeight = Viewport->GetSizeXY().Y;
	// Read the contents of the viewport into an array.
	TArray<FColor> OrigBitmap;
	if (Viewport->ReadPixels(OrigBitmap))
	{
		check(OrigBitmap.Num() == SrcWidth * SrcHeight);

		//pin to smallest value
		int32 CropSize = FMath::Min<uint32>(SrcWidth, SrcHeight);
		//pin to max size
		int32 ScaledSize  = FMath::Min<uint32>(AutoScreenshotSize, CropSize);

		//calculations for cropping
		TArray<FColor> CroppedBitmap;
		CroppedBitmap.AddUninitialized(CropSize*CropSize);

		//Crop the image
		int32 CroppedSrcTop  = (SrcHeight - CropSize) / 2;
		int32 CroppedSrcLeft = (SrcWidth - CropSize) / 2;

		for (int32 Row = 0; Row < CropSize; ++Row)
		{
			//Row*Side of a row*byte per color
			int32 SrcPixelIndex = (CroppedSrcTop+Row) * SrcWidth + CroppedSrcLeft;
			const void* SrcPtr = &(OrigBitmap[SrcPixelIndex]);
			void* DstPtr = &(CroppedBitmap[Row * CropSize]);
			FMemory::Memcpy(DstPtr, SrcPtr, CropSize * 4);
		}

		//Scale image down if needed
		TArray<FColor> ScaledBitmap;
		if (ScaledSize < CropSize)
		{
			FImageUtils::ImageResize( CropSize, CropSize, CroppedBitmap, ScaledSize, ScaledSize, ScaledBitmap, true );
		}
		else
		{
			//just copy the data over. sizes are the same
			ScaledBitmap = CroppedBitmap;
		}

		// Compress the scaled image
		TArray<uint8> ScaledPng;
		FImageUtils::CompressImageArray(ScaledSize, ScaledSize, ScaledBitmap, ScaledPng);

		// Save to file
		const FString ScreenShotPath = FPaths::GetPath(OutputFilename);
		if ( IFileManager::Get().MakeDirectory(*ScreenShotPath, true) )
		{
			// If source control is available, try to check out the file if necessary.
			// If not, silently continue. This is just a courtesy.
			bool bMarkFileForAdd = false;
			FString AbsoluteFilename = FPaths::ConvertRelativePathToFull(OutputFilename);
			TArray<FString> FilesToBeCheckedOut;
			FilesToBeCheckedOut.Add(AbsoluteFilename);

			ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
			if ( bUseSCCIfPossible && ISourceControlModule::Get().IsEnabled() && SourceControlProvider.IsAvailable() )
			{
				FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(AbsoluteFilename, EStateCacheUsage::ForceUpdate);
				if(SourceControlState.IsValid())
				{
					if ( SourceControlState->CanCheckout() )
					{
						SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), FilesToBeCheckedOut);
					}
					else if ( !SourceControlState->IsSourceControlled() )
					{
						bMarkFileForAdd = true;
					}
				}
			}

			if ( FFileHelper::SaveArrayToFile( ScaledPng, *OutputFilename ) )
			{
				if ( bMarkFileForAdd )
				{
					SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), FilesToBeCheckedOut);
				}

				return true;
			}
		}
	}

	return false;
}
