// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UObject/ObjectResource.h"
#include "UObject/Class.h"

/*-----------------------------------------------------------------------------
	Helper functions.
-----------------------------------------------------------------------------*/
namespace
{
	FORCEINLINE bool IsCorePackage(const FName& PackageName)
	{
		return PackageName == NAME_Core || PackageName == GLongCorePackageName;
	}
}

/*-----------------------------------------------------------------------------
	FObjectResource
-----------------------------------------------------------------------------*/

FObjectResource::FObjectResource()
{}

FObjectResource::FObjectResource( UObject* InObject )
:	ObjectName		( InObject ? InObject->GetFName() : FName(NAME_None)		)
{
}

/*-----------------------------------------------------------------------------
	FObjectExport.
-----------------------------------------------------------------------------*/

FObjectExport::FObjectExport()
: FObjectResource()
, ObjectFlags(RF_NoFlags)
, SerialSize(0)
, SerialOffset(0)
, ScriptSerializationStartOffset(0)
, ScriptSerializationEndOffset(0)
, Object(NULL)
, HashNext(INDEX_NONE)
, bForcedExport(false)
, bNotForClient(false)
, bNotForServer(false)
, bNotAlwaysLoadedForEditorGame(true)
, bIsAsset(false)
, bExportLoadFailed(false)
, DynamicType(EDynamicType::NotDynamicExport)
, bWasFiltered(false)
, PackageGuid(FGuid(0, 0, 0, 0))
, PackageFlags(0)
, FirstExportDependency(-1)
, SerializationBeforeSerializationDependencies(0)
, CreateBeforeSerializationDependencies(0)
, SerializationBeforeCreateDependencies(0)
, CreateBeforeCreateDependencies(0)

{}

FObjectExport::FObjectExport( UObject* InObject )
: FObjectResource(InObject)
, ObjectFlags(InObject ? InObject->GetMaskedFlags() : RF_NoFlags)
, SerialSize(0)
, SerialOffset(0)
, ScriptSerializationStartOffset(0)
, ScriptSerializationEndOffset(0)
, Object(InObject)
, HashNext(INDEX_NONE)
, bForcedExport(false)
, bNotForClient(false)
, bNotForServer(false)
, bNotAlwaysLoadedForEditorGame(true)
, bIsAsset(false)
, bExportLoadFailed(false)
, DynamicType(EDynamicType::NotDynamicExport)
, bWasFiltered(false)
, PackageGuid(FGuid(0, 0, 0, 0))
, PackageFlags(0)
, FirstExportDependency(-1)
, SerializationBeforeSerializationDependencies(0)
, CreateBeforeSerializationDependencies(0)
, SerializationBeforeCreateDependencies(0)
, CreateBeforeCreateDependencies(0)
{
	if(Object)		
	{
		bNotForClient = Object->HasAnyMarks(OBJECTMARK_NotForClient);
		bNotForServer = Object->HasAnyMarks(OBJECTMARK_NotForServer);
		bNotAlwaysLoadedForEditorGame = Object->HasAnyMarks(OBJECTMARK_NotAlwaysLoadedForEditorGame);
		bIsAsset = Object->IsAsset();
	}
}

FArchive& operator<<( FArchive& Ar, FObjectExport& E )
{
	Ar << E.ClassIndex;
	Ar << E.SuperIndex;
	if (Ar.UE4Ver() >= VER_UE4_TemplateIndex_IN_COOKED_EXPORTS)
	{
		Ar << E.TemplateIndex;
	}

	Ar << E.OuterIndex;
	Ar << E.ObjectName;

	uint32 Save = E.ObjectFlags & RF_Load;
	Ar << Save;
	if (Ar.IsLoading())
	{
		E.ObjectFlags = EObjectFlags(Save & RF_Load);
	}

	if (Ar.UE4Ver() < VER_UE4_64BIT_EXPORTMAP_SERIALSIZES)
	{
		int32 SerialSize = E.SerialSize;
		Ar << SerialSize;
		E.SerialSize = (int64)SerialSize;

		int32 SerialOffset = E.SerialOffset;
		Ar << SerialOffset;
		E.SerialOffset = SerialOffset;
	}
	else
	{
		Ar << E.SerialSize;
		Ar << E.SerialOffset;
	}

	Ar << E.bForcedExport;
	Ar << E.bNotForClient;
	Ar << E.bNotForServer;

	Ar << E.PackageGuid;
	Ar << E.PackageFlags;

	if (Ar.UE4Ver() >= VER_UE4_LOAD_FOR_EDITOR_GAME)
	{
		Ar << E.bNotAlwaysLoadedForEditorGame;
	}

	if (Ar.UE4Ver() >= VER_UE4_COOKED_ASSETS_IN_EDITOR_SUPPORT)
	{
		Ar << E.bIsAsset;
	}

	if (Ar.UE4Ver() >= VER_UE4_PRELOAD_DEPENDENCIES_IN_COOKED_EXPORTS)
	{
		Ar << E.FirstExportDependency;
		Ar << E.SerializationBeforeSerializationDependencies;
		Ar << E.CreateBeforeSerializationDependencies;
		Ar << E.SerializationBeforeCreateDependencies;
		Ar << E.CreateBeforeCreateDependencies;
	}	
	return Ar;
}

/*-----------------------------------------------------------------------------
	FObjectImport.
-----------------------------------------------------------------------------*/

FObjectImport::FObjectImport()
	: FObjectResource()
	, bImportPackageHandled(false)
	, bImportSearchedFor(false)
	, bImportFailed(false)
{
}

FObjectImport::FObjectImport(UObject* InObject)
	: FObjectResource(InObject)
	, ClassPackage(InObject ? InObject->GetClass()->GetOuter()->GetFName() : NAME_None)
	, ClassName(InObject ? InObject->GetClass()->GetFName() : NAME_None)
	, XObject(InObject)
	, SourceLinker(NULL)
	, SourceIndex(INDEX_NONE)
	, bImportPackageHandled(false)
	, bImportSearchedFor(false)
	, bImportFailed(false)
{
}

FObjectImport::FObjectImport(UObject* InObject, UClass* InClass)
	: FObjectResource(InObject)
	, ClassPackage((InObject && InClass) ? InClass->GetOuter()->GetFName() : NAME_None)
	, ClassName((InObject && InClass) ? InClass->GetFName() : NAME_None)
	, XObject(InObject)
	, SourceLinker(NULL)
	, SourceIndex(INDEX_NONE)
	, bImportPackageHandled(false)
	, bImportSearchedFor(false)
	, bImportFailed(false)
{
}

FArchive& operator<<( FArchive& Ar, FObjectImport& I )
{
	Ar << I.ClassPackage << I.ClassName;
	Ar << I.OuterIndex;
	Ar << I.ObjectName;
	if( Ar.IsLoading() )
	{
		I.SourceLinker	= NULL;
		I.SourceIndex	= INDEX_NONE;
		I.XObject		= NULL;
	}
	return Ar;
}
