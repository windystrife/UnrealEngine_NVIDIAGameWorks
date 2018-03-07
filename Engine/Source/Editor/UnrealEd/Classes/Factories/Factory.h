// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "Misc/SecureHash.h"
#include "Factory.generated.h"

struct FUntypedBulkData;

/**
 * Base class for all factories
 * An object responsible for creating and importing new objects.
 * 
 */
UCLASS(abstract)
class UNREALED_API UFactory : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * @return true if the factory can currently create a new object from scratch.
	 */
	virtual bool CanCreateNew() const
	{
		return bCreateNew;
	}

	/**
	 * Whether the specified file can be imported by this factory.
	 *
	 * @return true if the file is supported, false otherwise.
	 */
	virtual bool FactoryCanImport(const FString& Filename);

	/**
	 * Create a new object by importing it from a file name.
	 *
	 * The default implementation of this method will load the contents of the entire
	 * file into a byte buffer and call FactoryCreateBinary. User defined factories
	 * may override this behavior to process the provided file name on their own.
	 *
	 * @param InClass
	 * @param InParent
	 * @param InName
	 * @param Flags
	 * @param Filename
	 * @param Parms
	 * @param Warn
	 * @param bOutOperationCanceled Will indicate whether the user canceled the import.
	 * @return The new object.
	 */
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled);

	/**
	 * Create a new object by class.
	 *
	 * @param InClass
	 * @param InParent
	 * @param InName
	 * @param Flags
	 * @param Context
	 * @param Warn
	 * @param CallingContext
	 * @return The new object.
	 */
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn, FName CallingContext)
	{
		return FactoryCreateNew(InClass, InParent, InName, Flags, Context, Warn);
	}

	/**
	 * Create a new object by class.
	 *
	 * @param InClass
	 * @param InParent
	 * @param InName
	 * @param Flags
	 * @param Context
	 * @param Warn
	 * @return The new object.
	 */
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
	{
		return nullptr;
	}

	virtual UObject* ImportObject(UClass* InClass, UObject* InOuter, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, bool& OutCanceled);

	/** Returns true if this factory should be shown in the New Asset menu (by default calls CanCreateNew). */
	virtual bool ShouldShowInNewMenu() const;

	/** Returns an optional override brush name for the new asset menu. If this is not specified, the thumbnail for the supported class will be used. */
	virtual FName GetNewAssetThumbnailOverride() const;

	/** Returns the name of the factory for menus */
	virtual FText GetDisplayName() const;

	/** When shown in menus, this is the category containing this factory. Return type is a BitFlag mask using EAssetTypeCategories. */
	virtual uint32 GetMenuCategories() const;

	/** Returns the tooltip text description of this factory */
	virtual FText GetToolTip() const;

	/** Returns the documentation page that should be use for the rich tool tip for this factory */
	virtual FString GetToolTipDocumentationPage() const;

	/** Returns the documentation excerpt that should be use for the rich tool tip for this factory */
	virtual FString GetToolTipDocumentationExcerpt() const;

	/**
	 * @return		The object class supported by this factory.
	 */
	UClass* GetSupportedClass() const;

	/**
	 * @return true if it supports this class 
	 */
	virtual bool DoesSupportClass(UClass* Class);

	/**
	 * Resolves SupportedClass for factories which support multiple classes.
	 * Such factories will have a nullptr SupportedClass member.
	 */
	virtual UClass* ResolveSupportedClass();

	/** Opens a dialog to configure the factory properties. Return false if user opted out of configuring properties */
	virtual bool ConfigureProperties()
	{
		return true;
	}

	// @todo document
	virtual bool ImportUntypedBulkDataFromText(const TCHAR*& Buffer, FUntypedBulkData& BulkData);

	/** Creates a list of file extensions supported by this factory */
	void GetSupportedFileExtensions(TArray<FString>& OutExtensions) const;

	/** Do clean up after importing is done. Will be called once for multi batch import. */
	virtual void CleanUp() {}
	/**
	 * Creates an asset if it doesn't exist. If it does exist then it overwrites it if possible. If it can not overwrite then it will delete and replace. If it can not delete, it will return nullptr.
	 * 
	 * @param InClass The class of the asset to create
	 * @param InPackage The package to create this object within.
	 * @param Name The name to give the new asset. If no value (NAME_None) is specified, the asset will be given a unique name in the form of ClassName_#.
	 * @param InFlags The ObjectFlags to assign to the new asset.
	 * @param Template If specified, the property values from this object will be copied to the new object, and the new object's ObjectArchetype value will be set to this object.
	 *	               If nullptr, the class default object is used instead.
	 * @return A pointer to a new asset of the specified type or null if the creation failed.
	 */
	UObject* CreateOrOverwriteAsset(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InTemplate = nullptr) const;

	/** Returns a new starting point name for newly created assets in the content browser */
	virtual FString GetDefaultNewAssetName() const;

	/** @return the parser that is capable of parsing a json string of import settings for this factory */
	virtual class IImportSettingsParser* GetImportSettingsParser() {return nullptr;}

	/**
	 * Sets the automated import data being used with this factory
	 *
	 * @param Data	The automated import data or nullptr if it doesnt exist
	 */
	void SetAutomatedAssetImportData(const class UAutomatedAssetImportData* Data);

	/**
	 * @return true if this factory is being used for automated import.  Dialogs and user input should be disabled if this method returns true
	 */
	bool IsAutomatedImport() const { return  GIsAutomationTesting || AutomatedImportData; }
public:

	/**
	 * Pop up message to the user asking whether they wish to overwrite existing state or not.
	 *
	 * @param Message The message text.
	 **/
	void DisplayOverwriteOptionsDialog(const FText& Message);

	/** Get the name of the file currently being imported. */
	static FString GetCurrentFilename()
	{
		return CurrentFilename;
	}

	/** Get the default import priority for factories. */
	static int32 GetDefaultImportPriority()
	{
		return DefaultImportPriority;
	}

	//@third party code BEGIN SIMPLYGON
	/** Get the Hash for the file being imported. Provides enormous speed impovements for large CAD file imports  */
	static FMD5Hash GetFileHash()
	{
		return FileHash;
	}
	//@third party code END SIMPLYGON

	/**
	 * Resets the saved state of this factory.
	 *
	 * The states are used to suppress messages during multiple object import. 
	 * It needs to be reset each time a new import is started
	 */
	void ResetState();

	/** Helper function to sort an array of factories by their import priority - use as a predicate for Sort */
	static bool SortFactoriesByPriority(const UFactory& A, const UFactory& B);

	// @todo document
	static UObject* StaticImportObject(UClass* Class, UObject* InOuter, FName Name, EObjectFlags Flags, const TCHAR* Filename = TEXT(""), UObject* Context = nullptr, UFactory* Factory = nullptr, const TCHAR* Parms = nullptr, FFeedbackContext* Warn = GWarn, int32 MaxImportFileSize = 0xC100000);
	static UObject* StaticImportObject(UClass* Class, UObject* InOuter, FName Name, EObjectFlags Flags, bool& bOutOperationCanceled, const TCHAR* Filename = TEXT(""), UObject* Context = nullptr, UFactory* Factory = nullptr, const TCHAR* Parms = nullptr, FFeedbackContext* Warn = GWarn, int32 MaxImportFileSize = 0xC100000);

public:

	//~ UObject interface

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

protected:

	/** The default value to return from CanCreateNew() */
	UPROPERTY()
	uint32 bCreateNew : 1;

protected:

	/**
	 * Create a new object by importing it from a text buffer.
	 *
	 * @param InClass
	 * @param InParent
	 * @param InName
	 * @param Flags
	 * @param Context
	 * @param Type (must not be nullptr, i.e. TEXT("TGA"))
	 * @param Buffer
	 * @param BufferEnd
	 * @param Warn
	 * @return The new object.
	 */
	virtual UObject* FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
	{
		return nullptr;
	}

	/**
	 * Create a new object by importing it from a binary buffer.
	 *
	 * @param InClass
	 * @param InParent
	 * @param InName
	 * @param Flags
	 * @param Context
	 * @param Type (must not be nullptr, i.e. TEXT("TGA"))
	 * @param Buffer
	 * @param BufferEnd
	 * @param Warn
	 * @return The new object.
	 */
	virtual UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn)
	{
		return nullptr;
	}
	
	/**
	 * Create a new object by importing it from a binary buffer (cancelable).
	 *
	 * @param InClass
	 * @param InParent
	 * @param InName
	 * @param Flags
	 * @param Context
	 * @param Type (must not be nullptr, i.e. TEXT("TGA"))
	 * @param Buffer
	 * @param BufferEnd
	 * @param Warn
	 * @param bOutOperationCanceled Will indicate whether the user canceled the import.
	 * @return The new object.
	 */
	virtual UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn, bool& bOutOperationCanceled)
	{
		return FactoryCreateBinary(InClass, InParent, InName, Flags, Context, Type, Buffer, BufferEnd, Warn);
	}
public:

	/** The class manufactured by this factory. */
	UPROPERTY()
	TSubclassOf<UObject>  SupportedClass;

	/** Class of the context object used to help create the object. */
	UPROPERTY()
	TSubclassOf<UObject>  ContextClass;

	/** List of formats supported by the factory. Each entry is of the form "ext;Description" where ext is the file extension. */
	UPROPERTY()
	TArray<FString> Formats;

	/** true if the associated editor should be opened after creating a new object. */
	UPROPERTY()
	uint32 bEditAfterNew:1;

	/** true if the factory imports objects from files. */
	UPROPERTY()
	uint32 bEditorImport:1;

	/** true if the factory imports objects from text. */
	UPROPERTY()
	uint32 bText:1;

	/** Determines the order in which factories are tried when importing or reimporting an object.
	Factories with higher priority values will go first. Factories with negative priorities will be excluded. */
	UPROPERTY()
	int32 ImportPriority;

	/** Data for how to import files via the automated command line importing interface */
	UPROPERTY()
	const class UAutomatedAssetImportData* AutomatedImportData;


protected:

	/** Name of the file currently being imported. */
	static FString CurrentFilename;

	/** This is the import priority that all factories are given in the default constructor. */
	static const int32 DefaultImportPriority;

	//@third party code BEGIN SIMPLYGON
	/** This is the HASH for the file being imported */
	static FMD5Hash FileHash;
	//@third party code END SIMPLYGON

	/**
	 * For interactive object imports, this value indicates whether the user wants
	 * objects to be automatically overwritten (See EAppReturnType), or -1 if the
	  * user should be prompted.
	*/
	UPROPERTY()
	int32 OverwriteYesOrNoToAllState;

};
