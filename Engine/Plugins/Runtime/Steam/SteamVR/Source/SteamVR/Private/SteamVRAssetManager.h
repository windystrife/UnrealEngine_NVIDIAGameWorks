// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IXRDeviceAssets.h"
#include "UObject/SoftObjectPtr.h"

class  FSteamVRAsyncMeshLoader;
struct FSteamVRMeshData;
class  UProceduralMeshComponent;
class  UMaterial;
class  UTexture2D;

/**
 *
 */
class FSteamVRAssetManager : public IXRDeviceAssets
{
public:
	FSteamVRAssetManager();
	virtual ~FSteamVRAssetManager();

public:
	//~ IXRDeviceAssets interface 

	virtual bool EnumerateRenderableDevices(TArray<int32>& DeviceListOut) override;
	virtual UPrimitiveComponent* CreateRenderComponent(const int32 DeviceId, AActor* Owner, EObjectFlags Flags) override;

protected:
	struct FAsyncLoadData
	{
		TWeakPtr<FSteamVRAsyncMeshLoader>        AsyncLoader;
		TWeakObjectPtr<UProceduralMeshComponent> ComponentPtr;
	};
	void OnMeshLoaded(int32 SubMeshIndex, const FSteamVRMeshData& MeshData, UTexture2D* DiffuseTex, FAsyncLoadData LoadData);

private:
	TArray< TSharedPtr<FSteamVRAsyncMeshLoader> > AsyncMeshLoaders;
	TSoftObjectPtr<UMaterial> DefaultDeviceMat;
};
