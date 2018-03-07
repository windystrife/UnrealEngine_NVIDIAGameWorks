// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SteamVRAssetManager.h"
#include "ISteamVRPlugin.h" // STEAMVR_SUPPORTED_PLATFORMS
#include "ProceduralMeshComponent.h"
#include "Tickable.h" // for FTickableGameObject
#include "Engine/Engine.h" // for GEngine
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "UObject/Package.h"
#include "UObject/GCObject.h"

#if STEAMVR_SUPPORTED_PLATFORMS
	#include "SteamVRHMD.h"
	#include <openvr.h>
#endif 

class FSteamVRHMD;
namespace vr { struct RenderModel_t; }
namespace vr { class IVRRenderModels; }
namespace vr { struct RenderModel_TextureMap_t; }

/* SteamVRDevice_Impl 
 *****************************************************************************/

namespace SteamVRDevice_Impl
{
	static FSteamVRHMD* GetSteamHMD();
	static int32 GetDeviceStringProperty(int32 DeviceIndex, int32 PropertyId, FString& StringPropertyOut);
	static vr::IVRRenderModels* GetSteamVRModelManager();
}

static FSteamVRHMD* SteamVRDevice_Impl::GetSteamHMD()
{
#if STEAMVR_SUPPORTED_PLATFORMS
	static FName SystemName(TEXT("SteamVR"));
	if (GEngine->XRSystem.IsValid() && (GEngine->XRSystem->GetSystemName() == SystemName))
	{
		return static_cast<FSteamVRHMD*>(GEngine->XRSystem.Get());
	}
#endif
	return nullptr;
}

// @TODO: move this to a shared util library
static int32 SteamVRDevice_Impl::GetDeviceStringProperty(int32 DeviceIndex, int32 PropertyId, FString& StringPropertyOut)
{
	int32 ErrorResult = -1;
#if STEAMVR_SUPPORTED_PLATFORMS
	if (FSteamVRHMD* SteamHMD = GetSteamHMD())
	{
		vr::IVRSystem* SteamVRSystem = SteamHMD->GetVRSystem();
		vr::ETrackedDeviceProperty SteamPropId = (vr::ETrackedDeviceProperty)PropertyId;

		vr::TrackedPropertyError APIError;
		TArray<char> Buffer;
		Buffer.AddUninitialized(vr::k_unMaxPropertyStringSize);

		int Size = SteamVRSystem->GetStringTrackedDeviceProperty(DeviceIndex, SteamPropId, Buffer.GetData(), Buffer.Num(), &APIError);
		if (APIError == vr::TrackedProp_BufferTooSmall)
		{
			Buffer.AddUninitialized(Size - Buffer.Num());
			Size = SteamVRSystem->GetStringTrackedDeviceProperty(DeviceIndex, SteamPropId, Buffer.GetData(), Buffer.Num(), &APIError);
		}

		if (APIError == vr::TrackedProp_Success)
		{
			StringPropertyOut = UTF8_TO_TCHAR(Buffer.GetData());
		}
		else
		{
			StringPropertyOut = UTF8_TO_TCHAR(SteamVRSystem->GetPropErrorNameFromEnum(APIError));
		}
		ErrorResult = (int32)APIError;
	}
#endif 
	return ErrorResult;
}

static vr::IVRRenderModels* SteamVRDevice_Impl::GetSteamVRModelManager()
{
	vr::IVRRenderModels* VRModelManager = nullptr;
#if STEAMVR_SUPPORTED_PLATFORMS
	if (FSteamVRHMD* SteamHMD = SteamVRDevice_Impl::GetSteamHMD())
	{
		VRModelManager = SteamHMD->GetRenderModelManager();
	}
#endif
	return VRModelManager;
}

/* TSteamVRResource 
 *****************************************************************************/

template<typename ResType>
struct TSharedSteamVRResource
{
	TSharedSteamVRResource() : RefCount(0), RawResource(nullptr) {}
	int32    RefCount;
	ResType* RawResource;
};

template< typename ResType, typename IDType >
struct TSteamVRResource
{
private:
	typedef TSharedSteamVRResource<ResType>  FSharedSteamVRResource;
	typedef TMap<IDType, FSharedSteamVRResource> TSharedResourceMap;
	static  TSharedResourceMap SharedResources;

public:
	TSteamVRResource(const IDType& ResID, bool bKickOffLoad = true)
		: ResourceId(ResID)
		, RawResource(nullptr)
		, bLoadFailed(false)
	{
		SharedResources.FindOrAdd(ResourceId).RefCount += 1;
		if (bKickOffLoad)
		{
			TickAsyncLoad();
		}
	}

	bool IsPending()
	{
		return (RawResource == nullptr) && !bLoadFailed;
	}

	bool IsValid()
	{
		return (RawResource != nullptr);
	}

	ResType* TickAsyncLoad()
	{
		if (IsPending())
		{
			FSharedSteamVRResource& SharedResource = SharedResources.FindOrAdd(ResourceId);
			if (SharedResource.RawResource != nullptr)
			{
				RawResource = SharedResource.RawResource;
			}
#if STEAMVR_SUPPORTED_PLATFORMS
			else if (vr::IVRRenderModels* VRModelManager = SteamVRDevice_Impl::GetSteamVRModelManager())
			{
				vr::EVRRenderModelError LoadResult = (vr::EVRRenderModelError)TickAsyncLoad_Internal(VRModelManager, &RawResource);

				const bool bLoadComplete = (LoadResult != vr::VRRenderModelError_Loading);
				if (bLoadComplete)
				{
					bLoadFailed = !RawResource || (LoadResult != vr::VRRenderModelError_None);
					if (!bLoadFailed)
					{
						SharedResource.RawResource = RawResource;
					}
					else
					{
						RawResource = nullptr;
					}
				}
			}
#endif
			else
 
			{
				bLoadFailed = true;
			}
		}
		return RawResource;
	}

	operator ResType*()   { return RawResource; }
	ResType* operator->() { return RawResource; }

protected:
	int32 TickAsyncLoad_Internal(vr::IVRRenderModels* VRModelManager, ResType** ResourceOut);

	void Reset()
	{
		if (FSharedSteamVRResource* SharedResource = SharedResources.Find(ResourceId))
		{
			int32 NewRefCount = --SharedResource->RefCount;
			if (NewRefCount <= 0)
			{
				if ( vr::IVRRenderModels* VRModelManager = SteamVRDevice_Impl::GetSteamVRModelManager() )
				{
					if (!RawResource)
					{
						RawResource = SharedResource->RawResource;
					}
					FreeResource(VRModelManager);
				}
				SharedResources.Remove(ResourceId);
			}
		}
		RawResource = nullptr;
	}

	void FreeResource(vr::IVRRenderModels* VRModelManager);

protected:
	IDType   ResourceId;
	ResType* RawResource;
	bool     bLoadFailed;
};

template<typename ResType, typename IDType>
TMap< IDType, TSharedSteamVRResource<ResType> > TSteamVRResource<ResType, IDType>::SharedResources;

typedef TSteamVRResource<vr::RenderModel_t, FString> TSteamVRModel;
typedef TSteamVRResource<vr::RenderModel_TextureMap_t, int32> TSteamVRTexture;

/// @cond DOXYGEN_WARNINGS
template<>
int32 TSteamVRModel::TickAsyncLoad_Internal(vr::IVRRenderModels* VRModelManager, vr::RenderModel_t** ResourceOut)
{
#if STEAMVR_SUPPORTED_PLATFORMS
	return (int32)VRModelManager->LoadRenderModel_Async(TCHAR_TO_UTF8(*ResourceId), ResourceOut);
#else
	return INDEX_NONE;
#endif
}
/// @endcond

template<>
void TSteamVRModel::FreeResource(vr::IVRRenderModels* VRModelManager)
{
#if STEAMVR_SUPPORTED_PLATFORMS
	VRModelManager->FreeRenderModel(RawResource);
#endif
}

/// @cond DOXYGEN_WARNINGS
template<>
int32 TSteamVRTexture::TickAsyncLoad_Internal(vr::IVRRenderModels* VRModelManager, vr::RenderModel_TextureMap_t** ResourceOut)
{
#if STEAMVR_SUPPORTED_PLATFORMS
	return (int32)VRModelManager->LoadTexture_Async(ResourceId, ResourceOut);
#else
	return INDEX_NONE;
#endif
}
/// @endcond

template<>
void TSteamVRTexture::FreeResource(vr::IVRRenderModels* VRModelManager)
{
#if STEAMVR_SUPPORTED_PLATFORMS
	VRModelManager->FreeTexture(RawResource);
#endif
}

/* FSteamVRModel 
 *****************************************************************************/

struct FSteamVRModel : public TSteamVRModel
{
public:
	FSteamVRModel(const FString& ResID, bool bKickOffLoad = true)
		: TSteamVRModel(ResID, bKickOffLoad)
	{}

public:
	bool GetRawMeshData(float UEMeterScale, FSteamVRMeshData& MeshDataOut);
};

struct FSteamVRMeshData
{
	TArray<FVector> VertPositions;
	TArray<int32> Indices;
	TArray<FVector2D> UVs;
	TArray<FVector> Normals;
	TArray<FColor> VertColors;
	TArray<FProcMeshTangent> Tangents;
};

bool FSteamVRModel::GetRawMeshData(float UEMeterScale, FSteamVRMeshData& MeshDataOut)
{
	bool bIsValidData = (RawResource != nullptr);
#if STEAMVR_SUPPORTED_PLATFORMS
	if (RawResource)
	{
		const uint32 VertCount = RawResource->unVertexCount;
		MeshDataOut.VertPositions.Empty(VertCount);
		MeshDataOut.UVs.Empty(VertCount);
		MeshDataOut.Normals.Empty(VertCount);

		const uint32 TriCount = RawResource->unTriangleCount;
		const uint32 IndxCount = TriCount * 3;
		MeshDataOut.Indices.Empty(IndxCount);

		// @TODO: move this into a shared utility class
		auto SteamVecToFVec = [](const vr::HmdVector3_t SteamVec)
		{
			return FVector(-SteamVec.v[2], SteamVec.v[0], SteamVec.v[1]);
		};

		for (uint32 VertIndex = 0; VertIndex < VertCount; ++VertIndex)
		{
			const vr::RenderModel_Vertex_t& VertData = RawResource->rVertexData[VertIndex];

			const vr::HmdVector3_t& VertPos = VertData.vPosition;
			MeshDataOut.VertPositions.Add(SteamVecToFVec(VertPos) * UEMeterScale);

			const FVector2D VertUv = FVector2D(VertData.rfTextureCoord[0], VertData.rfTextureCoord[1]);
			MeshDataOut.UVs.Add(VertUv);

			MeshDataOut.Normals.Add(SteamVecToFVec(VertData.vNormal));
		}

		for (uint32 Indice = 0; Indice < IndxCount; ++Indice)
		{
			MeshDataOut.Indices.Add((int32)RawResource->rIndexData[Indice]);
		}
	}
#endif
	return bIsValidData;
}

/* FSteamVRTexture 
 *****************************************************************************/

struct FSteamVRTexture : public TSteamVRTexture
{
public:
	FSteamVRTexture(int32 ResID, bool bKickOffLoad = true)
		: TSteamVRTexture(ResID, bKickOffLoad)
	{}

	int32 GetResourceID() const { return ResourceId; }

public:
	UTexture2D* ConstructUETexture(UObject* ObjOuter, const FName ObjName, EObjectFlags ObjFlags = RF_NoFlags)
	{
		UTexture2D* NewTexture = nullptr;

#if STEAMVR_SUPPORTED_PLATFORMS
		if (RawResource != nullptr)
		{
#if WITH_EDITORONLY_DATA // @TODO: UTexture::Source is only available in editor builds, we need to find some other way to construct textures
			NewTexture = NewObject<UTexture2D>(ObjOuter, ObjName, ObjFlags);
			NewTexture->Source.Init(RawResource->unWidth, RawResource->unHeight, /*NewNumSlices =*/1, /*NewNumMips =*/1, TSF_BGRA8, RawResource->rubTextureMapData);

			NewTexture->MipGenSettings = TMGS_NoMipmaps;
			// disable compression
			NewTexture->CompressionNone = true;
			NewTexture->DeferCompression = false;

			NewTexture->PostEditChange();
#endif 
		}
#endif

		return NewTexture;
	}
};

/* FSteamVRAsyncMeshLoader 
 *****************************************************************************/

DECLARE_DELEGATE_ThreeParams(FOnSteamVRMeshLoadComplete, int32, const FSteamVRMeshData&, UTexture2D*);

class FSteamVRAsyncMeshLoader : public FTickableGameObject, public FGCObject
{
public:
	FSteamVRAsyncMeshLoader(const float WorldMetersScaleIn);

	/** */
	void SetLoadCallback(const FOnSteamVRMeshLoadComplete& OnLoadComplete);
	/** */
	int32 EnqueMeshLoad(const FString& ModelName);

public:
	//~ FTickableObjectBase interface
	
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

public:
	//~ FTickableGameObject interface

	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override   { return true; }

public:
	//~ FGCObject interface

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

protected:
	/** */
	bool EnqueueTextureLoad(int32 SubMeshIndex, vr::RenderModel_t* RenderModel);
	/** */
	void OnLoadComplete(int32 SubMeshIndex);

private:
	int32 PendingLoadCount;
	float WorldMetersScale;
	FOnSteamVRMeshLoadComplete LoadCompleteCallback;

	TArray<FSteamVRModel>    EnqueuedModels;
	TArray<FSteamVRTexture>  EnqueuedTextures;
	TMap<int32, int32>       PendingTextureLoads;
	TMap<int32, UTexture2D*> ConstructedTextures;
};

FSteamVRAsyncMeshLoader::FSteamVRAsyncMeshLoader(const float WorldMetersScaleIn)
	: PendingLoadCount(0)
	, WorldMetersScale(WorldMetersScaleIn)
{}

void FSteamVRAsyncMeshLoader::SetLoadCallback(const FOnSteamVRMeshLoadComplete& LoadCompleteDelegate)
{
	LoadCompleteCallback = LoadCompleteDelegate;
}

int32 FSteamVRAsyncMeshLoader::EnqueMeshLoad(const FString& ModelName)
{
	int32 MeshIndex = INDEX_NONE;
	if (!ModelName.IsEmpty())
	{
		++PendingLoadCount;
		MeshIndex = EnqueuedModels.Add( FSteamVRModel(ModelName) );
	}
	return MeshIndex;
}

void FSteamVRAsyncMeshLoader::Tick(float /*DeltaTime*/)
{
	for (int32 SubMeshIndex = 0; SubMeshIndex < EnqueuedModels.Num(); ++SubMeshIndex)
	{
		FSteamVRModel& ModelResource = EnqueuedModels[SubMeshIndex];
		if (ModelResource.IsPending())
		{
			vr::RenderModel_t* RenderModel = ModelResource.TickAsyncLoad();
			if (!ModelResource.IsPending())
			{
				--PendingLoadCount;

				if (!RenderModel)
				{
					// valid index + missing RenderModel => signifies failure
					OnLoadComplete(SubMeshIndex);
				}
#if STEAMVR_SUPPORTED_PLATFORMS
				// if we've already loaded and converted the texture
				else if (ConstructedTextures.Contains(RenderModel->diffuseTextureId))
				{
					OnLoadComplete(SubMeshIndex);
				}
#endif // STEAMVR_SUPPORTED_PLATFORMS
				else if (!EnqueueTextureLoad(SubMeshIndex, RenderModel))
				{
					// if we fail to load the texture, we'll have to do without it
					OnLoadComplete(SubMeshIndex);
				}			
			}
		}
	}

	for (int32 TexIndex = 0; TexIndex < EnqueuedTextures.Num(); ++TexIndex)
	{
		FSteamVRTexture& TextureResource = EnqueuedTextures[TexIndex];
		if (TextureResource.IsPending())
		{
			const bool bLoadSuccess = (TextureResource.TickAsyncLoad() != nullptr);
			if (!TextureResource.IsPending())
			{
				--PendingLoadCount;

				if (bLoadSuccess)
				{
					UObject* TextureOuter = GetTransientPackage();
					FName    TextureName  = *FString::Printf(TEXT("T_SteamVR_%d"), TextureResource.GetResourceID());

					UTexture2D* UETexture = FindObjectFast<UTexture2D>(TextureOuter, TextureName, /*ExactClass =*/true);
					if (UETexture == nullptr)
					{
						UETexture = TextureResource.ConstructUETexture(TextureOuter, TextureName);
					}
					ConstructedTextures.Add(TextureResource.GetResourceID(), UETexture);
				}

				int32* ModelIndexPtr = PendingTextureLoads.Find(TexIndex);
				if ( ensure(ModelIndexPtr != nullptr && EnqueuedModels.IsValidIndex(*ModelIndexPtr)) )
				{
					FSteamVRModel& AssociatedModel = EnqueuedModels[*ModelIndexPtr];
					OnLoadComplete(*ModelIndexPtr);
				}	
			}
		}
	}

	if (PendingLoadCount <= 0)
	{
		// INDEX_NONE => signifies everything has been loaded
		LoadCompleteCallback.Execute(INDEX_NONE, FSteamVRMeshData(), /*UTexture2D =*/nullptr);
	}
}

bool FSteamVRAsyncMeshLoader::IsTickable() const
{
	return (PendingLoadCount > 0);
}

TStatId FSteamVRAsyncMeshLoader::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FSteamVRAsyncMeshLoader, STATGROUP_Tickables);
}

void FSteamVRAsyncMeshLoader::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(ConstructedTextures);
}

bool FSteamVRAsyncMeshLoader::EnqueueTextureLoad(int32 SubMeshIndex, vr::RenderModel_t* RenderModel)
{
	bool bLoadEnqueued = false;
#if STEAMVR_SUPPORTED_PLATFORMS
	if (RenderModel && RenderModel->diffuseTextureId != vr::INVALID_TEXTURE_ID)
	{
		++PendingLoadCount;
		bLoadEnqueued = true;

		// load will be kicked off later on in Tick() loop (no need to do it twice in the same tick)
		int32 TextureIndex = EnqueuedTextures.Add( FSteamVRTexture(RenderModel->diffuseTextureId, /*bKickOffLoad =*/false) );
		PendingTextureLoads.Add(TextureIndex, SubMeshIndex);
	}
#endif
	return bLoadEnqueued;
}

void FSteamVRAsyncMeshLoader::OnLoadComplete(int32 SubMeshIndex)
{
	FSteamVRMeshData RawMeshData;
	UTexture2D* Texture = nullptr;

	if (EnqueuedModels.IsValidIndex(SubMeshIndex))
	{
		FSteamVRModel& LoadedModel = EnqueuedModels[SubMeshIndex];
		LoadedModel.GetRawMeshData(WorldMetersScale, RawMeshData);

		
		if (LoadedModel.IsValid())
		{
#if STEAMVR_SUPPORTED_PLATFORMS
			UTexture2D** CachedTexturePtr = ConstructedTextures.Find(LoadedModel->diffuseTextureId);
			if (CachedTexturePtr)
			{
				Texture = *CachedTexturePtr;
			}
#endif // STEAMVR_SUPPORTED_PLATFORMS
		}
	}
	LoadCompleteCallback.ExecuteIfBound(SubMeshIndex, RawMeshData, Texture);
}


/* FSteamVRAssetManager 
 *****************************************************************************/

FSteamVRAssetManager::FSteamVRAssetManager()
	: DefaultDeviceMat(FString(TEXT("/SteamVR/Materials/M_DefaultDevice.M_DefaultDevice")))
{
	IModularFeatures::Get().RegisterModularFeature(IXRDeviceAssets::GetModularFeatureName(), this);
}

FSteamVRAssetManager::~FSteamVRAssetManager()
{
	IModularFeatures::Get().UnregisterModularFeature(IXRDeviceAssets::GetModularFeatureName(), this);
}

bool FSteamVRAssetManager::EnumerateRenderableDevices(TArray<int32>& DeviceListOut)
{
	bool bHasActiveVRSystem = false;

#if STEAMVR_SUPPORTED_PLATFORMS
	FSteamVRHMD* SteamHMD = SteamVRDevice_Impl::GetSteamHMD();
	bHasActiveVRSystem = SteamHMD && SteamHMD->GetVRSystem();
	
	if (bHasActiveVRSystem)
	{
		DeviceListOut.Empty();

		for (uint32 DeviceIndex = 0; DeviceIndex < vr::k_unMaxTrackedDeviceCount; ++DeviceIndex)
		{
			// Add only devices with a currently valid tracked pose
			if (SteamHMD->IsTracking(DeviceIndex))
			{
				DeviceListOut.Add(DeviceIndex);
			}
		}
	}
#endif
	return bHasActiveVRSystem;
}

UPrimitiveComponent* FSteamVRAssetManager::CreateRenderComponent(const int32 DeviceId, AActor* Owner, EObjectFlags Flags)
{
	UPrimitiveComponent* NewRenderComponent = nullptr;
#if STEAMVR_SUPPORTED_PLATFORMS

	FString ModelName;
	if (SteamVRDevice_Impl::GetDeviceStringProperty(DeviceId, vr::Prop_RenderModelName_String, ModelName) == vr::TrackedProp_Success)
	{
		FSteamVRHMD* SteamHMD = SteamVRDevice_Impl::GetSteamHMD();
		vr::IVRRenderModels* VRModelManager = (SteamHMD) ? SteamHMD->GetRenderModelManager() : nullptr;

		if (VRModelManager != nullptr)
		{
			const char* RawModelName = TCHAR_TO_UTF8(*ModelName);
			const uint32 SubMeshCount = VRModelManager->GetComponentCount(RawModelName);

			const FName DeviceName = *FString::Printf(TEXT("%s_Device%d"), TEXT("SteamVR"), DeviceId);
			UProceduralMeshComponent* ProceduralMesh = NewObject<UProceduralMeshComponent>(Owner, DeviceName, Flags);

			float MeterScale = 1.f;
			if (UWorld* World = Owner->GetWorld())
			{
				AWorldSettings* WorldSettings = World->GetWorldSettings();
				if (WorldSettings)
				{
					MeterScale = WorldSettings->WorldToMeters;
				}
			}
			TSharedPtr<FSteamVRAsyncMeshLoader> NewMeshLoader = MakeShareable(new FSteamVRAsyncMeshLoader(MeterScale));
			
			FAsyncLoadData CallbackPayload;
			CallbackPayload.AsyncLoader  = NewMeshLoader;
			CallbackPayload.ComponentPtr = ProceduralMesh;

			FOnSteamVRMeshLoadComplete LoadHandler;
			LoadHandler.BindRaw(this, &FSteamVRAssetManager::OnMeshLoaded, CallbackPayload);
			NewMeshLoader->SetLoadCallback(LoadHandler);

			if (SubMeshCount > 0)
			{
				TArray<char> NameBuffer;
				NameBuffer.AddUninitialized(vr::k_unMaxPropertyStringSize);

				for (uint32 SubMeshIndex = 0; SubMeshIndex < SubMeshCount; ++SubMeshIndex)
				{
					uint32 NeededSize = VRModelManager->GetComponentName(RawModelName, SubMeshIndex, NameBuffer.GetData(), NameBuffer.Num());
					if (NeededSize == 0)
					{
						continue;
					}
					else if (NeededSize > (uint32)NameBuffer.Num())
					{
						NameBuffer.AddUninitialized(NeededSize - NameBuffer.Num());
						VRModelManager->GetComponentName(RawModelName, SubMeshIndex, NameBuffer.GetData(), NameBuffer.Num());
					}
					
					FString ComponentName = UTF8_TO_TCHAR(NameBuffer.GetData());
					// arbitrary pieces that are not present on the physical device
					// @TODO: probably useful for something, should figure out their purpose 
					if (ComponentName == TEXT("status") ||
						ComponentName == TEXT("scroll_wheel") ||
						ComponentName == TEXT("trackpad_scroll_cut") ||
						ComponentName == TEXT("trackpad_touch"))
					{
						continue;
					}

					NeededSize = VRModelManager->GetComponentRenderModelName(RawModelName, TCHAR_TO_UTF8(*ComponentName), NameBuffer.GetData(), NameBuffer.Num());
					if (NeededSize == 0)
					{
						continue;
					}
					else if (NeededSize > (uint32)NameBuffer.Num())
					{
						NameBuffer.AddUninitialized(NeededSize - NameBuffer.Num());
						NeededSize = VRModelManager->GetComponentRenderModelName(RawModelName, TCHAR_TO_UTF8(*ComponentName), NameBuffer.GetData(), NameBuffer.Num());
					}

					FString ComponentModelName = UTF8_TO_TCHAR(NameBuffer.GetData());
					NewMeshLoader->EnqueMeshLoad(ComponentModelName);
				}
			}
			else
			{
				NewMeshLoader->EnqueMeshLoad(ModelName);
			}

			AsyncMeshLoaders.Add(NewMeshLoader);
			NewRenderComponent = ProceduralMesh;
		}
	}
#endif
	return NewRenderComponent;
}

void FSteamVRAssetManager::OnMeshLoaded(int32 SubMeshIndex, const FSteamVRMeshData& MeshData, UTexture2D* DiffuseTex, FAsyncLoadData LoadData)
{
	if (SubMeshIndex == INDEX_NONE && LoadData.AsyncLoader.IsValid())
	{
		AsyncMeshLoaders.Remove(LoadData.AsyncLoader.Pin());
	}
	else if (MeshData.VertPositions.Num() > 0 && LoadData.ComponentPtr.IsValid())
	{
		LoadData.ComponentPtr->CreateMeshSection(SubMeshIndex
			, MeshData.VertPositions
			, MeshData.Indices
			, MeshData.Normals
			, MeshData.UVs
			, MeshData.VertColors
			, MeshData.Tangents
			, /*bCreateCollision =*/false);

		if (DiffuseTex != nullptr)
		{
			UMaterial* DefaultMaterial = DefaultDeviceMat.LoadSynchronous();
			if (DefaultMaterial)
			{
				FName MatName = *FString::Printf(TEXT("M_%s_SubMesh%d"), *LoadData.ComponentPtr->GetName(), SubMeshIndex);
				UMaterialInstanceDynamic* MeshMaterial = UMaterialInstanceDynamic::Create(DefaultMaterial, LoadData.ComponentPtr.Get(), MatName);

				MeshMaterial->SetTextureParameterValue(TEXT("DiffuseTex"), DiffuseTex);
				LoadData.ComponentPtr->SetMaterial(SubMeshIndex, MeshMaterial);
			}
		}
	}
}
