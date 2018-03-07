// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectThreadContext.cpp: Unreal object globals
=============================================================================*/

#include "UObject/UObjectThreadContext.h"

DEFINE_LOG_CATEGORY(LogUObjectThreadContext);

FUObjectThreadContext::FUObjectThreadContext()
: ImportCount(0)
, ForcedExportCount(0)
, ObjBeginLoadCount(0)
, IsRoutingPostLoad(false)
, IsDeletingLinkers(false)
, IsInConstructor(0)
, ConstructedObject(nullptr)
, SerializedObject(nullptr)
, SerializedPackageLinker(nullptr)
, SerializedImportIndex(INDEX_NONE)
, SerializedImportLinker(nullptr)
, SerializedExportIndex(INDEX_NONE)
, SerializedExportLinker(nullptr)
, AsyncPackage(nullptr)
{}
