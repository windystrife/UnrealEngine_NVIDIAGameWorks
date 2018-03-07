// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
=============================================================================*/
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Misc/EngineVersion.h"
#include "GameFramework/PlayerController.h"
#include "EngineGlobals.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/LevelStreaming.h"
#include "Engine/Console.h"

#include "ProfilingDebugging/DiagnosticTable.h"
#include "Interfaces/ITargetPlatform.h"

DEFINE_LOG_CATEGORY_STATIC(LogEngineUtils, Log, All);

IMPLEMENT_HIT_PROXY(HActor,HHitProxy)
IMPLEMENT_HIT_PROXY(HBSPBrushVert,HHitProxy);
IMPLEMENT_HIT_PROXY(HStaticMeshVert,HHitProxy);
IMPLEMENT_HIT_PROXY(HTranslucentActor,HActor)


#if !UE_BUILD_SHIPPING
FContentComparisonHelper::FContentComparisonHelper()
{
	FConfigSection* RefTypes = GConfig->GetSectionPrivate(TEXT("ContentComparisonReferenceTypes"), false, true, GEngineIni);
	if (RefTypes != NULL)
	{
		for( FConfigSectionMap::TIterator It(*RefTypes); It; ++It )
		{
			const FString& RefType = It.Value().GetValue();
			ReferenceClassesOfInterest.Add(RefType, true);
			UE_LOG(LogEngineUtils, Log, TEXT("Adding class of interest: %s"), *RefType);
		}
	}
}

FContentComparisonHelper::~FContentComparisonHelper()
{
}

bool FContentComparisonHelper::CompareClasses(const FString& InBaseClassName, int32 InRecursionDepth)
{
	TArray<FString> EmptyIgnoreList;
	return CompareClasses(InBaseClassName, EmptyIgnoreList, InRecursionDepth);
}

bool FContentComparisonHelper::CompareClasses(const FString& InBaseClassName, const TArray<FString>& InBaseClassesToIgnore, int32 InRecursionDepth)
{
	TMap<FString,TArray<FContentComparisonAssetInfo> > ClassToAssetsMap;

	UClass* TheClass = (UClass*)StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *InBaseClassName, true);
	if (TheClass != NULL)
	{
		TArray<UClass*> IgnoreBaseClasses;
		for (int32 IgnoreIdx = 0; IgnoreIdx < InBaseClassesToIgnore.Num(); IgnoreIdx++)
		{
			UClass* IgnoreClass = (UClass*)StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *(InBaseClassesToIgnore[IgnoreIdx]), true);
			if (IgnoreClass != NULL)
			{
				IgnoreBaseClasses.Add(IgnoreClass);
			}
		}

		for( TObjectIterator<UClass> It; It; ++It )
		{
			UClass* TheAssetClass = *It;
			if ((TheAssetClass->IsChildOf(TheClass) == true) && 
				(TheAssetClass->HasAnyClassFlags(CLASS_Abstract) == false))
			{
				bool bSkipIt = false;
				for (int32 CheckIdx = 0; CheckIdx < IgnoreBaseClasses.Num(); CheckIdx++)
				{
					UClass* CheckClass = IgnoreBaseClasses[CheckIdx];
					if (TheAssetClass->IsChildOf(CheckClass) == true)
					{
// 						UE_LOG(LogEngineUtils, Warning, TEXT("Skipping class derived from other content comparison class..."));
// 						UE_LOG(LogEngineUtils, Warning, TEXT("\t%s derived from %s"), *TheAssetClass->GetFullName(), *CheckClass->GetFullName());
						bSkipIt = true;
					}
				}
				if (bSkipIt == false)
				{
					TArray<FContentComparisonAssetInfo>* AssetList = ClassToAssetsMap.Find(TheAssetClass->GetFullName());
					if (AssetList == NULL)
					{
						TArray<FContentComparisonAssetInfo> TempAssetList;
						ClassToAssetsMap.Add(TheAssetClass->GetFullName(), TempAssetList);
						AssetList = ClassToAssetsMap.Find(TheAssetClass->GetFullName());
					}
					check(AssetList);

					// Serialize object with reference collector.
					const int32 MaxRecursionDepth = 6;
					InRecursionDepth = FMath::Clamp<int32>(InRecursionDepth, 1, MaxRecursionDepth);
					TMap<UObject*,bool> RecursivelyGatheredReferences;
					RecursiveObjectCollection(TheAssetClass, 0, InRecursionDepth, RecursivelyGatheredReferences);

					// Add them to the asset list
					for (TMap<UObject*,bool>::TIterator GatheredIt(RecursivelyGatheredReferences); GatheredIt; ++GatheredIt)
					{
						UObject* Object = GatheredIt.Key();
						if (Object)
						{
							bool bAddIt = true;
							if (ReferenceClassesOfInterest.Num() > 0)
							{
								FString CheckClassName = Object->GetClass()->GetName();
								if (ReferenceClassesOfInterest.Find(CheckClassName) == NULL)
								{
									bAddIt = false;
								}
							}
							if (bAddIt == true)
							{
								int32 NewIndex = AssetList->AddZeroed();
								FContentComparisonAssetInfo& Info = (*AssetList)[NewIndex];
								Info.AssetName = Object->GetFullName();
								Info.ResourceSize = Object->GetResourceSizeBytes(EResourceSizeMode::Inclusive);
							}
						}
					}
				}
			}
		}
	}
	else
	{
		UE_LOG(LogEngineUtils, Warning, TEXT("Failed to find class: %s"), *InBaseClassName);
		return false;
	}

#if 0
	// Log them all out
	UE_LOG(LogEngineUtils, Log, TEXT("CompareClasses on %s"), *InBaseClassName);
	for (TMap<FString,TArray<FContentComparisonAssetInfo>>::TIterator It(ClassToAssetsMap); It; ++It)
	{
		FString ClassName = It.Key();
		TArray<FContentComparisonAssetInfo>& AssetList = It.Value();

		UE_LOG(LogEngineUtils, Log, TEXT("\t%s"), *ClassName);
		for (int32 AssetIdx = 0; AssetIdx < AssetList.Num(); AssetIdx++)
		{
			FContentComparisonAssetInfo& Info = AssetList(AssetIdx);

			UE_LOG(LogEngineUtils, Log, TEXT("\t\t%s,%f"), *(Info.AssetName), Info.ResourceSize/1024.0f);
		}
	}
#endif

#if ALLOW_DEBUG_FILES
	// Write out a CSV file
	FString CurrentTime = FDateTime::Now().ToString();
	FString Platform(FPlatformProperties::PlatformName());

	FString BaseCSVName = (
		FString(TEXT("ContentComparison/")) + 
		FString::Printf(TEXT("ContentCompare-%s/"), *FEngineVersion::Current().ToString()) +
		FString::Printf(TEXT("%s"), *InBaseClassName)
		);

	// Handle file name length on consoles... 
	FString EditedBaseClassName = InBaseClassName;
	FString TimeString = *FDateTime::Now().ToString();
	FString CheckLenName = FString::Printf(TEXT("%s-%s.csv"),*InBaseClassName,*TimeString);
	if (CheckLenName.Len() > PLATFORM_MAX_FILEPATH_LENGTH)
	{
		while (CheckLenName.Len() > PLATFORM_MAX_FILEPATH_LENGTH)
		{
			EditedBaseClassName = EditedBaseClassName.Right(EditedBaseClassName.Len() - 1);
			CheckLenName = FString::Printf(TEXT("%s-%s.csv"),*EditedBaseClassName,*TimeString);
		}
		BaseCSVName = (
			FString(TEXT("ContentComparison/")) + 
			FString::Printf(TEXT("ContentCompare-%s/"), *FEngineVersion::Current().ToString()) +
			FString::Printf(TEXT("%s"), *EditedBaseClassName)
			);
	}

	FDiagnosticTableViewer* AssetTable = new FDiagnosticTableViewer(
			*FDiagnosticTableViewer::GetUniqueTemporaryFilePath(*BaseCSVName), true);
	if ((AssetTable != NULL) && (AssetTable->OutputStreamIsValid() == true))
	{
		// Fill in the header row
		AssetTable->AddColumn(TEXT("Class"));
		AssetTable->AddColumn(TEXT("Asset"));
		AssetTable->AddColumn(TEXT("ResourceSize(kB)"));
		AssetTable->CycleRow();

		// Fill it in
		for (TMap<FString,TArray<FContentComparisonAssetInfo> >::TIterator It(ClassToAssetsMap); It; ++It)
		{
			FString ClassName = It.Key();
			TArray<FContentComparisonAssetInfo>& AssetList = It.Value();

			AssetTable->AddColumn(*ClassName);
			AssetTable->CycleRow();
			for (int32 AssetIdx = 0; AssetIdx < AssetList.Num(); AssetIdx++)
			{
				FContentComparisonAssetInfo& Info = AssetList[AssetIdx];

				AssetTable->AddColumn(TEXT(""));
				AssetTable->AddColumn(*(Info.AssetName));
				AssetTable->AddColumn(TEXT("%f"), Info.ResourceSize/1024.0f);
				AssetTable->CycleRow();
			}
		}
	}
	else if (AssetTable != NULL)
	{
		// Created the class, but it failed to open the output stream.
		UE_LOG(LogEngineUtils, Warning, TEXT("Failed to open output stream in asset table!"));
	}

	if (AssetTable != NULL)
	{
		// Close it and kill it
		AssetTable->Close();
		delete AssetTable;
	}
#endif

	return true;
}

void FContentComparisonHelper::RecursiveObjectCollection(UObject* InStartObject, int32 InCurrDepth, int32 InMaxDepth, TMap<UObject*,bool>& OutCollectedReferences)
{
	// Serialize object with reference collector.
	TArray<UObject*> LocalCollectedReferences;
	FReferenceFinder ObjectReferenceCollector( LocalCollectedReferences, NULL, false, true, true, true );
	ObjectReferenceCollector.FindReferences( InStartObject );

	if (InCurrDepth < InMaxDepth)
	{
		InCurrDepth++;
		for (int32 ObjRefIdx = 0; ObjRefIdx < LocalCollectedReferences.Num(); ObjRefIdx++)
		{
			UObject* InnerObject = LocalCollectedReferences[ObjRefIdx];
			if ((InnerObject != NULL) &&
				(InnerObject->IsA(UFunction::StaticClass()) == false) &&
				(InnerObject->IsA(UPackage::StaticClass()) == false)
				)
			{
				OutCollectedReferences.Add(InnerObject, true);
				RecursiveObjectCollection(InnerObject, InCurrDepth, InMaxDepth, OutCollectedReferences);
			}
		}
		InCurrDepth--;
	}
}
#endif

bool EngineUtils::FindOrLoadAssetsByPath(const FString& Path, TArray<UObject*>& OutAssets, EAssetToLoad Type)
{
	if ( !FPackageName::IsValidLongPackageName(Path, true) )
	{
		return false;
	}

	// Convert the package path to a filename with no extension (directory)
	const FString FilePath = FPackageName::LongPackageNameToFilename(Path);

	// Gather the package files in that directory and subdirectories
	TArray<FString> Filenames;
	FPackageName::FindPackagesInDirectory(Filenames, FilePath);

	// Cull out map files
	for (int32 FilenameIdx = Filenames.Num() - 1; FilenameIdx >= 0; --FilenameIdx)
	{
		const FString Extension = FPaths::GetExtension(Filenames[FilenameIdx], true);
		if ( Extension == FPackageName::GetMapPackageExtension() )
		{
			Filenames.RemoveAt(FilenameIdx);
		}
	}

	// Load packages or find existing ones and fully load them
	for (int32 FileIdx = 0; FileIdx < Filenames.Num(); ++FileIdx)
	{
		const FString& Filename = Filenames[FileIdx];

		UPackage* Package = FindPackage(NULL, *FPackageName::FilenameToLongPackageName(Filename));

		if (Package)
		{
			Package->FullyLoad();
		}
		else
		{
			Package = LoadPackage(NULL, *Filename, LOAD_None);
		}

		if (Package)
		{
			ForEachObjectWithOuter(Package, [Type, &OutAssets](UObject* Object)
			{
				const bool bWantedType = 
					((EAssetToLoad::ATL_Regular == Type) && Object->IsAsset()) ||
					((EAssetToLoad::ATL_Class == Type) && Object->IsA<UClass>());
				if (bWantedType)
				{
					OutAssets.Add(Object);
				}
			});
		}
	}
	return true;
}

TArray<FSubLevelStatus> GetSubLevelsStatus( UWorld* World )
{
	TArray<FSubLevelStatus> Result;
	FWorldContext &Context = GEngine->GetWorldContextFromWorldChecked(World);

	Result.Reserve(World->StreamingLevels.Num() + 1);
	
	// Add persistent level
	{
		FSubLevelStatus LevelStatus = {};
		LevelStatus.PackageName = World->GetOutermost()->GetFName();
		LevelStatus.StreamingStatus = LEVEL_Visible;
		LevelStatus.LODIndex	= INDEX_NONE;
		Result.Add(LevelStatus);
	}
	
	// Iterate over the world info's level streaming objects to find and see whether levels are loaded, visible or neither.
	for( int32 LevelIndex=0; LevelIndex < World->StreamingLevels.Num(); LevelIndex++ )
	{
		ULevelStreaming* LevelStreaming = World->StreamingLevels[LevelIndex];

		if( LevelStreaming 
			&&  !LevelStreaming->GetWorldAsset().IsNull()
			&&	LevelStreaming->GetWorldAsset() != World )
		{
			ULevel* Level = LevelStreaming->GetLoadedLevel();
			FSubLevelStatus LevelStatus = {};
			LevelStatus.PackageName = LevelStreaming->GetWorldAssetPackageFName();
			LevelStatus.LODIndex	= LevelStreaming->LevelLODIndex;

			if( Level != NULL )
			{
				if( World->ContainsLevel( Level ) == true )
				{
					if( World->CurrentLevelPendingVisibility == Level )
					{
						LevelStatus.StreamingStatus = LEVEL_MakingVisible;
					}
					else
					{
						LevelStatus.StreamingStatus = LEVEL_Visible;
					}
				}
				else
				{
					LevelStatus.StreamingStatus = LEVEL_Loaded;
				}
			}
			else
			{
				// See whether the level's world object is still around.
				UPackage* LevelPackage	= FindObjectFast<UPackage>(NULL, LevelStatus.PackageName);
				UWorld*	  LevelWorld	= NULL;
				if( LevelPackage )
				{
					LevelWorld = UWorld::FindWorldInPackage(LevelPackage);
				}

				if( LevelWorld )
				{
					LevelStatus.StreamingStatus = LEVEL_UnloadedButStillAround;
				}
				else if( LevelStreaming->bHasLoadRequestPending )
				{
					LevelStatus.StreamingStatus = LEVEL_Loading;
				}
				else
				{
					LevelStatus.StreamingStatus = LEVEL_Unloaded;
				}
			}

			Result.Add(LevelStatus);
		}
	}

	
	// toss in the levels being loaded by PrepareMapChange
	for( int32 LevelIndex=0; LevelIndex < Context.LevelsToLoadForPendingMapChange.Num(); LevelIndex++ )
	{
		const FName LevelName = Context.LevelsToLoadForPendingMapChange[LevelIndex];
		
		FSubLevelStatus LevelStatus = {};
		LevelStatus.PackageName = LevelName;
		LevelStatus.StreamingStatus = LEVEL_Preloading;
		LevelStatus.LODIndex = INDEX_NONE;
		Result.Add(LevelStatus);
	}


	for( FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		APlayerController* PlayerController = Iterator->Get();

		if( PlayerController->GetPawn() != NULL )
		{
			// need to do a trace down here
			//TraceActor = Trace( out_HitLocation, out_HitNormal, TraceDest, TraceStart, false, TraceExtent, HitInfo, true );
			FHitResult Hit(1.f);

			// this will not work for flying around :-(
			PlayerController->GetWorld()->LineTraceSingleByObjectType(Hit,PlayerController->GetPawn()->GetActorLocation(), (PlayerController->GetPawn()->GetActorLocation()-FVector(0.f, 0.f, 256.f)), FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionQueryParams(SCENE_QUERY_STAT(FindLevel), true, PlayerController->GetPawn()));

			ULevel* LevelPlayerIsIn = NULL;

			if( Hit.GetActor() != NULL )
			{
				LevelPlayerIsIn = Hit.GetActor()->GetLevel();
			}
			else if( Hit.Component != NULL )
			{
				LevelPlayerIsIn = Hit.Component->GetComponentLevel();
			}

			if (LevelPlayerIsIn)
			{
				FName LevelName = LevelPlayerIsIn->GetOutermost()->GetFName();
				FSubLevelStatus* LevelStatusPlayerIn = Result.FindByPredicate([LevelName](const FSubLevelStatus& InLevelStatus)
				{
					return InLevelStatus.PackageName == LevelName;
				});

				if (LevelStatusPlayerIn)
				{
					LevelStatusPlayerIn->bPlayerInside = true;
				}
			}
		}
	}

	return Result;
}

//////////////////////////////////////////////////////////////////////////
// FConsoleOutputDevice

void FConsoleOutputDevice::Serialize(const TCHAR* Text, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	FStringOutputDevice::Serialize(Text, Verbosity, Category);
	FStringOutputDevice::Serialize(TEXT("\n"), Verbosity, Category);
	GLog->Serialize(Text, Verbosity, Category);

	if( Console != NULL )
	{
		bool bLogToConsole = true;

		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("con.MinLogVerbosity"));

		if(CVar)
		{
			int MinVerbosity = CVar->GetValueOnAnyThread(true);

			if((int)Verbosity <= MinVerbosity)
			{
				// in case the cvar is used we don't need this printout (avoid double print)
				bLogToConsole = false;
			}
		}

		if(bLogToConsole)
		{
			Console->OutputText(Text);
		}
	}
}

/*-----------------------------------------------------------------------------
	Serialized data stripping.
-----------------------------------------------------------------------------*/
FStripDataFlags::FStripDataFlags( class FArchive& Ar, uint8 InClassFlags /*= 0*/, int32 InVersion /*= VER_UE4_OLDEST_LOADABLE_PACKAGE */ )
	: GlobalStripFlags( 0 )
	, ClassStripFlags( 0 )
{
	check(InVersion >= VER_UE4_OLDEST_LOADABLE_PACKAGE);
	if (Ar.UE4Ver() >= InVersion)
	{
		if (Ar.IsCooking())
		{
			// When cooking GlobalStripFlags are automatically generated based on the current target
			// platform's properties.
			GlobalStripFlags |= Ar.CookingTarget()->HasEditorOnlyData() ? FStripDataFlags::None : FStripDataFlags::Editor;
			GlobalStripFlags |= Ar.CookingTarget()->IsServerOnly() ? FStripDataFlags::Server : FStripDataFlags::None;
			ClassStripFlags = InClassFlags;
		}
		Ar << GlobalStripFlags;
		Ar << ClassStripFlags;
	}
}

FStripDataFlags::FStripDataFlags( class FArchive& Ar, uint8 InGlobalFlags, uint8 InClassFlags, int32 InVersion /*= VER_UE4_OLDEST_LOADABLE_PACKAGE */ )
	: GlobalStripFlags( 0 )
	, ClassStripFlags( 0 )
{
	check(InVersion >= VER_UE4_OLDEST_LOADABLE_PACKAGE);
	if (Ar.UE4Ver() >= InVersion)
	{
		if (Ar.IsCooking())
		{
			// Don't generate global strip flags and use the ones passed in by the caller.
			GlobalStripFlags = InGlobalFlags;
			ClassStripFlags = InClassFlags;
		}
		Ar << GlobalStripFlags;
		Ar << ClassStripFlags;
	}
}
