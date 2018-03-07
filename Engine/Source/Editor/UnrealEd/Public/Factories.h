// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Factories.h: Unreal Engine factory types.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "Animation/MorphTarget.h"

UNREALED_API DECLARE_LOG_CATEGORY_EXTERN(LogEditorFactories, Log, All);

class Error;
class USkeletalMesh;
class UStaticMesh;

/**  
 * This class is a simple customizable object factory driven from a text buffer.  
 * Subclasses need to implement CanCreateClass and ProcessConstructedObject.
 */
class UNREALED_API FCustomizableTextObjectFactory
{
protected:
	FFeedbackContext* WarningContext;
	FObjectInstancingGraph InstanceGraph;
public:
	/** Constructor for the factory; takes a context for emitting warnings such as GWarn */
	FCustomizableTextObjectFactory(FFeedbackContext* InWarningContext);
	virtual ~FCustomizableTextObjectFactory() {}

	/**
	 *	Parse a text buffer and factories objects from it, subject to the restrictions imposed by CanCreateClass()
	 *
	 *	@param	InParent	Usually the parent sequence, but might be a package for example. Used as outer for created SequenceObjects.
	 *	@param	Flags		Flags used when creating AnimObjects
	 *	@param	TextBuffer	Text buffer with descriptions of nodes
	 */
	void ProcessBuffer(UObject* InParent, EObjectFlags Flags, const FString& TextBuffer);

	void ProcessBuffer(UObject* InParent, EObjectFlags Flags, const TCHAR* TextBuffer);

	/**
	 *	Determine if it is possible to create objects from the specified TextBuffer
	 */
	bool CanCreateObjectsFromText(const FString& TextBuffer) const;

protected:
	/** Return true if the an object of type ObjectClass is allowed to be created; If false is returned, the object and subobjects will be ignored. */
	virtual bool CanCreateClass(UClass* ObjectClass, bool& bOmitSubObjs) const;

	/** This is called on each created object after the property text is imported */
	virtual void ProcessConstructedObject(UObject* CreatedObject);

	/** Post handling of constructed objects by the factory */
	virtual void PostProcessConstructedObjects() {};

	/** Util to ensure that InName is a valid name for a new object within InParent. Will rename any existing object within InParent if it is called InName. */
	static void ClearObjectNameUsage(UObject* InParent, FName InName);

	/** If we cant do anything with the line ourselves hand off to child class */
	virtual void ProcessUnidentifiedLine(const FString& StrLine) {}

	/** Allow child class to override new object parent (only called when parent supplied to ProcessBuffer is NULL */
	virtual UObject* GetParentForNewObject(const UClass* ObjClass) {return NULL;}
};


/** morph target import error codes */
enum EMorphImportError
{
	// success
	MorphImport_OK=0,
	// target mesh exists
	MorphImport_AlreadyExists,
	// source file was not loaded
	MorphImport_CantLoadFile,
	// source file format is invalid
	MorphImport_InvalidMeshFormat,
	// source mesh vertex data doesn't match base
	MorphImport_MismatchBaseMesh,
	// source mesh is missing its metadata
	// needs to be reimported
	MorphImport_ReimportBaseMesh,
	// LOD index was out of range by more than 1
	MorphImport_InvalidLODIndex,
	// Missing morph target
	MorphImport_MissingMorphTarget,
	// max
	MorphImport_MAX
};

/**
 * Utility class for importing a new morph target
 */
class FMorphTargetBinaryImport
{
public:
	/** for outputing warnings */
	FFeedbackContext* Warn;
	/** raw mesh data used for calculating differences */
	FMorphMeshRawSource BaseMeshRawData;
	/** base mesh lod entry to use */
	int32 BaseLODIndex;
	/** the base mesh */
	UObject* BaseMesh;

	FMorphTargetBinaryImport( USkeletalMesh* InSrcMesh, int32 LODIndex=0, FFeedbackContext* InWarn=GWarn );
	FMorphTargetBinaryImport( UStaticMesh* InSrcMesh, int32 LODIndex=0, FFeedbackContext* InWarn=GWarn );
	virtual ~FMorphTargetBinaryImport() {}

    virtual USkeletalMesh* CreateSkeletalMesh(const TCHAR* SrcFilename, EMorphImportError* Error ) = 0;
};

#pragma pack(push,1)

struct FTGAFileHeader
{
	uint8 IdFieldLength;
	uint8 ColorMapType;
	uint8 ImageTypeCode;		// 2 for uncompressed RGB format
	uint16 ColorMapOrigin;
	uint16 ColorMapLength;
	uint8 ColorMapEntrySize;
	uint16 XOrigin;
	uint16 YOrigin;
	uint16 Width;
	uint16 Height;
	uint8 BitsPerPixel;
	uint8 ImageDescriptor;
	friend FArchive& operator<<( FArchive& Ar, FTGAFileHeader& H )
	{
		Ar << H.IdFieldLength << H.ColorMapType << H.ImageTypeCode;
		Ar << H.ColorMapOrigin << H.ColorMapLength << H.ColorMapEntrySize;
		Ar << H.XOrigin << H.YOrigin << H.Width << H.Height << H.BitsPerPixel;
		Ar << H.ImageDescriptor;
		return Ar;
	}
};

#pragma pack(pop)

/**
 * This helper allows to decompress TGA data in a pre-allocated memory block.
 * The pixel format is necessarily PF_A8R8G8B8.
 */
UNREALED_API bool DecompressTGA_helper(
	const FTGAFileHeader* TGA,
	uint32*& TextureData,
	const int32 TextureDataSize,
	FFeedbackContext* Warn );
