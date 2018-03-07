// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "FbxFactory.generated.h"

class IImportSettingsParser;

UCLASS(hidecategories=Object)
class UNREALED_API UFbxFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	class UFbxImportUI* ImportUI;


	/**  Set import batch **/
	void EnableShowOption() { bShowOption = true; }


	//~ Begin UObject Interface
	virtual void CleanUp() override;
	virtual bool ConfigureProperties() override;
	virtual void PostInitProperties() override;
	//~ End UObject Interface

	//~ Begin UFactory Interface
	virtual bool DoesSupportClass(UClass * Class) override;
	virtual UClass* ResolveSupportedClass() override;
	virtual UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual IImportSettingsParser* GetImportSettingsParser() override;

	//~ End UFactory Interface
	
	/**
	 * Detect mesh type to import: Static Mesh or Skeletal Mesh.
	 * Only the first mesh will be detected.
	 *
	 * @param InFilename	FBX file name
	 * @return bool	return true if parse the file successfully
	 */
	bool DetectImportType(const FString& InFilename);

	void SetDetectImportTypeOnImport(bool bDetectState) { bDetectImportTypeOnImport = bDetectState; }

protected:
	// @todo document
	UObject* RecursiveImportNode(void* FFbxImporter, void* Node, UObject* InParent, FName InName, EObjectFlags Flags, int32& Index, int32 Total, TArray<UObject*>& OutNewAssets);

	// @todo document
	UObject* ImportANode(void* VoidFbxImporter, TArray<void*> VoidNodes, UObject* InParent, FName InName, EObjectFlags Flags, int32& NodeIndex, int32 Total = 0, UObject* InMesh = NULL, int LODIndex = 0);

	bool bShowOption;
	bool bDetectImportTypeOnImport;

	/** true if the import operation was canceled. */
	bool bOperationCanceled;
};



