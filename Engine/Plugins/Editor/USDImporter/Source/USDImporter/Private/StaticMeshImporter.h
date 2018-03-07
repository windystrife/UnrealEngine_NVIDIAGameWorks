// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

class UStaticMesh;
struct FUsdImportContext;
struct FUsdPrimToImport;
struct FUsdGeomData;

class FUSDStaticMeshImporter
{
public:
	static UStaticMesh* ImportStaticMesh(FUsdImportContext& ImportContext, const FUsdPrimToImport& PrimToImport);
private:
	static bool IsTriangleMesh(const FUsdGeomData* GeomData);
};