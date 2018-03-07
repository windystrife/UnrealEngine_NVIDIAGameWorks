// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "USDImportOptions.h"
#include "TokenizedMessage.h"
#include "USDPrimResolver.h"

THIRD_PARTY_INCLUDES_START
#include "UnrealUSDWrapper.h"
THIRD_PARTY_INCLUDES_END

#include "USDImporter.generated.h"


class UUSDPrimResolver;
class IUsdPrim;
class IUsdStage;
struct FUsdGeomData;

DECLARE_LOG_CATEGORY_EXTERN(LogUSDImport, Log, All);

namespace USDKindTypes
{
	// Note: std::string for compatiblity with USD

	const std::string Component("component"); 
	const std::string Group("group");
	const std::string SubComponent("subcomponent");
}

USTRUCT()
struct FUsdImportContext
{
	GENERATED_BODY()
	
	/** Mapping of path to imported assets  */
	TMap<FString, UObject*> PathToImportAssetMap;

	/** Parent package to import a single mesh to */
	UPROPERTY()
	UObject* Parent;

	/** Name to use when importing a single mesh */
	UPROPERTY()
	FString ObjectName;

	UPROPERTY()
	FString ImportPathName;

	UPROPERTY()
	UUSDImportOptions* ImportOptions;

	UPROPERTY()
	UUSDPrimResolver* PrimResolver;

	IUsdStage* Stage;

	/** Root Prim of the USD file */
	IUsdPrim* RootPrim;

	/** Converts from a source coordinate system to Unreal */
	FTransform ConversionTransform;

	/** Object flags to apply to newly imported objects */
	EObjectFlags ImportObjectFlags;

	/** Whether or not to apply world transformations to the actual geometry */
	bool bApplyWorldTransformToGeometry;

	/** If true stop at any USD prim that has an unreal asset reference.  Geometry that is a child such prims will be ignored */
	bool bFindUnrealAssetReferences;

	virtual ~FUsdImportContext() { }

	virtual void Init(UObject* InParent, const FString& InName, IUsdStage* InStage);

	void AddErrorMessage(EMessageSeverity::Type MessageSeverity, FText ErrorMessage);
	void DisplayErrorMessages(bool bAutomated);
	void ClearErrorMessages();
private:
	/** Error messages **/
	TArray<TSharedRef<FTokenizedMessage>> TokenizedErrorMessages;
};


UCLASS(transient)
class UUSDImporter : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	bool ShowImportOptions(UObject& ImportOptions);

	IUsdStage* ReadUSDFile(FUsdImportContext& ImportContext, const FString& Filename);

	UObject* ImportMeshes(FUsdImportContext& ImportContext, const TArray<FUsdPrimToImport>& PrimsToImport);

	UObject* ImportSingleMesh(FUsdImportContext& ImportContext, EUsdMeshImportType ImportType, const FUsdPrimToImport& PrimToImport);
};