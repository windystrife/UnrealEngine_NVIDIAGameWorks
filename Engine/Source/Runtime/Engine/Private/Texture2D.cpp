// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Texture2D.cpp: Implementation of UTexture2D.
=============================================================================*/

#include "Engine/Texture2D.h"
#include "ProfilingDebugging/ScopedDebugInfo.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/App.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Containers/ResourceArray.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "UObject/LinkerLoad.h"
#include "RenderUtils.h"
#include "ContentStreaming.h"
#include "EngineUtils.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "DerivedDataCacheInterface.h"
#include "Engine/TextureStreamingTypes.h"
#include "Streaming/TextureStreamingHelpers.h"
#include "Streaming/Texture2DStreamOut_AsyncReallocate.h"
#include "Streaming/Texture2DStreamOut_Virtual.h"
#include "Streaming/Texture2DStreamIn_DDC_AsyncCreate.h"
#include "Streaming/Texture2DStreamIn_DDC_AsyncReallocate.h"
#include "Streaming/Texture2DStreamIn_IO_AsyncCreate.h"
#include "Streaming/Texture2DStreamIn_IO_AsyncReallocate.h"
#include "Streaming/Texture2DStreamIn_IO_Virtual.h"
#include "AsyncFileHandle.h"

UTexture2D::UTexture2D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PendingUpdate = nullptr;
	StreamingIndex = INDEX_NONE;
	LevelIndex = INDEX_NONE;
	SRGB = true;
}

/*-----------------------------------------------------------------------------
	Global helper functions
-----------------------------------------------------------------------------*/

/** CVars */
static TAutoConsoleVariable<float> CVarSetMipMapLODBias(
	TEXT("r.MipMapLODBias"),
	0.0f,
	TEXT("Apply additional mip map bias for all 2D textures, range of -15.0 to 15.0"),
	ECVF_RenderThreadSafe | ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarVirtualTextureEnabled(
	TEXT("r.VirtualTexture"),
	1,
	TEXT("If set to 1, textures will use virtual memory so they can be partially resident."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarFlushRHIThreadOnSTreamingTextureLocks(
	TEXT("r.FlushRHIThreadOnSTreamingTextureLocks"),
	0,
	TEXT("If set to 0, we won't do any flushes for streaming textures. This is safe because the texture streamer deals with these hazards explicitly."),
	ECVF_RenderThreadSafe);

// TODO Only adding this setting to allow backwards compatibility to be forced.  The default
// behavior is to NOT do this.  This variable should be removed in the future.  #ADDED 4.13
static TAutoConsoleVariable<int32> CVarForceHighestMipOnUITexturesEnabled(
	TEXT("r.ForceHighestMipOnUITextures"),
	0,
	TEXT("If set to 1, texutres in the UI Group will have their highest mip level forced."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMobileReduceLoadedMips(
	TEXT("r.MobileReduceLoadedMips"),
	0,
	TEXT("Reduce loaded texture mipmaps for nonstreaming mobile platforms.\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMobileMaxLoadedMips(
	TEXT("r.MobileMaxLoadedMips"),
	MAX_TEXTURE_MIP_COUNT,
	TEXT("Maximum number of loaded mips for nonstreaming mobile platforms.\n"),
	ECVF_RenderThreadSafe);

static int32 MobileReduceLoadedMips(int32 NumTotalMips)
{
	int32 NumReduceMips = FMath::Max(0, CVarMobileReduceLoadedMips.GetValueOnAnyThread());
	int32 MaxLoadedMips = FMath::Clamp(CVarMobileMaxLoadedMips.GetValueOnAnyThread(), 1, GMaxTextureMipCount);

	int32 NumMips = NumTotalMips;
	// Reduce number of mips as requested
	NumMips = FMath::Max(NumMips - NumReduceMips, 1);
	// Clamp number of mips as requested
	NumMips = FMath::Min(NumMips, MaxLoadedMips);
	
	return NumMips;
}

static bool CanCreateAsVirtualTexture(uint32 TexCreateFlags)
{
#if PLATFORM_SUPPORTS_VIRTUAL_TEXTURES
	const uint32 iDisableFlags = 
		TexCreate_RenderTargetable |
		TexCreate_ResolveTargetable |
		TexCreate_DepthStencilTargetable |
		TexCreate_Dynamic |
		TexCreate_UAV |
		TexCreate_Presentable;
	const uint32 iRequiredFlags =
		TexCreate_OfflineProcessed;

	return ((TexCreateFlags & (iDisableFlags | iRequiredFlags)) == iRequiredFlags) && CVarVirtualTextureEnabled.GetValueOnRenderThread();
	
#else
	return false;
#endif
}

/** Number of times to retry to reallocate a texture before trying a panic defragmentation, the first time. */
int32 GDefragmentationRetryCounter = 10;
/** Number of times to retry to reallocate a texture before trying a panic defragmentation, subsequent times. */
int32 GDefragmentationRetryCounterLong = 100;

/** Turn on ENABLE_TEXTURE_TRACKING in ContentStreaming.cpp and setup GTrackedTextures to track specific textures through the streaming system. */
extern bool TrackTextureEvent( FStreamingTexture* StreamingTexture, UTexture2D* Texture, bool bForceMipLevelsToBeResident, const FStreamingManagerTexture* Manager );


/** Scoped debug info that provides the texture name to memory allocation and crash callstacks. */
class FTexture2DScopedDebugInfo : public FScopedDebugInfo
{
public:

	/** Initialization constructor. */
	FTexture2DScopedDebugInfo(const UTexture2D* InTexture):
		FScopedDebugInfo(0),
		Texture(InTexture)
	{}

	// FScopedDebugInfo interface.
	virtual FString GetFunctionName() const
	{
		return FString::Printf(
			TEXT("%s (%ux%u %s, %u mips, LODGroup=%u)"),
			*Texture->GetPathName(),
			Texture->GetSizeX(),
			Texture->GetSizeY(),
			GPixelFormats[Texture->GetPixelFormat()].Name,
			Texture->GetNumMips(),
			(int32)Texture->LODGroup
			);
	}
	virtual FString GetFilename() const
	{
		return FString::Printf(
			TEXT("%s../../Development/Src/Engine/%s"),
			FPlatformProcess::BaseDir(),
			ANSI_TO_TCHAR(__FILE__)
			);
	}
	virtual int32 GetLineNumber() const
	{
		return __LINE__;
	}

private:

	const UTexture2D* Texture;
};

/*-----------------------------------------------------------------------------
	FTexture2DMipMap
-----------------------------------------------------------------------------*/

void FTexture2DMipMap::Serialize(FArchive& Ar, UObject* Owner, int32 MipIdx)
{
	bool bCooked = Ar.IsCooking();
	Ar << bCooked;

	BulkData.Serialize(Ar, Owner, MipIdx);
	Ar << SizeX;
	Ar << SizeY;

#if WITH_EDITORONLY_DATA
	if (!bCooked)
	{
		Ar << DerivedDataKey;
	}
#endif // #if WITH_EDITORONLY_DATA
}

#if WITH_EDITORONLY_DATA
uint32 FTexture2DMipMap::StoreInDerivedDataCache(const FString& InDerivedDataKey)
{
	int32 BulkDataSizeInBytes = BulkData.GetBulkDataSize();
	check(BulkDataSizeInBytes > 0);

	TArray<uint8> DerivedData;
	FMemoryWriter Ar(DerivedData, /*bIsPersistent=*/ true);
	Ar << BulkDataSizeInBytes;
	{
		void* BulkMipData = BulkData.Lock(LOCK_READ_ONLY);
		Ar.Serialize(BulkMipData, BulkDataSizeInBytes);
		BulkData.Unlock();
	}
	const uint32 Result = DerivedData.Num();
	GetDerivedDataCacheRef().Put(*InDerivedDataKey, DerivedData);
	DerivedDataKey = InDerivedDataKey;
	BulkData.RemoveBulkData();
	return Result;
}
#endif // #if WITH_EDITORONLY_DATA

/*-----------------------------------------------------------------------------
	UTexture2D
-----------------------------------------------------------------------------*/

bool UTexture2D::GetResourceMemSettings(int32 FirstMipIdx, int32& OutSizeX, int32& OutSizeY, int32& OutNumMips, uint32& OutTexCreateFlags)
{
	return false;
}

void UTexture2D::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	FStripDataFlags StripDataFlags(Ar);

	bool bCooked = Ar.IsCooking();
	Ar << bCooked;

	if (Ar.IsCooking() || bCooked)
	{
		SerializeCookedPlatformData(Ar);
	}

#if WITH_EDITOR	
	if (Ar.IsLoading() && !Ar.IsTransacting() && !bCooked && !GetOutermost()->HasAnyPackageFlags(PKG_ReloadingForCooker))
	{
		// The composite texture may not have been loaded yet. We have to defer caching platform
		// data until post load.
		if (CompositeTexture == NULL || CompositeTextureMode == CTM_Disabled)
		{
			BeginCachePlatformData();
		}
	}
#endif // #if WITH_EDITOR
}

float UTexture2D::GetLastRenderTimeForStreaming()
{
	float LastRenderTime = -FLT_MAX;
	if (Resource)
	{
		// The last render time is the last time the resource was directly bound or the last
		// time the texture reference was cached in a resource table, whichever was later.
		LastRenderTime = FMath::Max<double>(Resource->LastRenderTime,TextureReference.GetLastRenderTime());
	}
	return LastRenderTime;
}

void UTexture2D::InvalidateLastRenderTimeForStreaming()
{
	if (Resource)
	{
		Resource->LastRenderTime = -FLT_MAX;
	}
	TextureReference.InvalidateLastRenderTime();
}

int32 UTexture2D::GetNumResidentMips() const
{
	FTexture2DResource* Texture2DResource = (FTexture2DResource*)Resource;
	if (Texture2DResource && PlatformData)
	{
		return PlatformData->Mips.Num() - Texture2DResource->CurrentFirstMip;
	}
	return 0;
}

int32 UTexture2D::GetNumRequestedMips() const
{
	if (PendingUpdate && !PendingUpdate->IsCancelled())
	{
		return PendingUpdate->GetNumRequestedMips();
	}
	else
	{
		return GetNumResidentMips();
	}
}

#if WITH_EDITOR
void UTexture2D::PostEditUndo()
{
	FPropertyChangedEvent Undo(NULL);
	PostEditChangeProperty(Undo);
}

void UTexture2D::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
#if WITH_EDITORONLY_DATA
	if (!Source.IsPowerOfTwo() && (PowerOfTwoMode == ETexturePowerOfTwoSetting::None))
	{
		// Force NPT textures to have no mipmaps.
		MipGenSettings = TMGS_NoMipmaps;
		NeverStream = true;
	}

	// Make sure settings are correct for LUT textures.
	if(LODGroup == TEXTUREGROUP_ColorLookupTable)
	{
		MipGenSettings = TMGS_NoMipmaps;
		SRGB = false;
	}
#endif // #if WITH_EDITORONLY_DATA
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

float UTexture2D::GetAverageBrightness(bool bIgnoreTrueBlack, bool bUseGrayscale)
{
	float AvgBrightness = -1.0f;
#if WITH_EDITOR
	TArray<uint8> RawData;
	// use the source art if it exists
	if (Source.IsValid() && Source.GetFormat() == TSF_BGRA8)
	{
		Source.GetMipData(RawData, 0);
	}
	else
	{
		UE_LOG(LogTexture, Log, TEXT("No SourceArt available for %s"), *GetPathName());
	}

	if (RawData.Num() > 0)
	{
		int32 SizeX = Source.GetSizeX();
		int32 SizeY = Source.GetSizeY();
		double PixelSum = 0.0f;
		int32 Divisor = SizeX * SizeY;
		FColor* ColorData = (FColor*)RawData.GetData();
		FLinearColor CurrentColor;
		for (int32 Y = 0; Y < SizeY; Y++)
		{
			for (int32 X = 0; X < SizeX; X++)
			{
				if ((ColorData->R == 0) && (ColorData->G == 0) && (ColorData->B == 0) && bIgnoreTrueBlack)
				{
					ColorData++;
					Divisor--;
					continue;
				}

				if (SRGB == true)
				{
					CurrentColor = bUseLegacyGamma ? FLinearColor::FromPow22Color(*ColorData) : FLinearColor(*ColorData);
				}
				else
				{
					CurrentColor.R = float(ColorData->R) / 255.0f;
					CurrentColor.G = float(ColorData->G) / 255.0f;
					CurrentColor.B = float(ColorData->B) / 255.0f;
				}

				if (bUseGrayscale == true)
				{
					PixelSum += CurrentColor.R * 0.30f + CurrentColor.G * 0.59f + CurrentColor.B * 0.11f;
				}
				else
				{
					PixelSum += FMath::Max<float>(CurrentColor.R, FMath::Max<float>(CurrentColor.G, CurrentColor.B));
				}

				ColorData++;
			}
		}
		if (Divisor > 0)
		{
			AvgBrightness = PixelSum / Divisor;
		}
	}
#endif // #if WITH_EDITOR
	return AvgBrightness;
}

void UTexture2D::LinkStreaming()
{
	if (!IsTemplate() && IStreamingManager::Get().IsTextureStreamingEnabled() && IsStreamingTexture(this))
	{
		IStreamingManager::Get().GetTextureStreamingManager().AddStreamingTexture(this);
	}
	else
	{
		StreamingIndex = INDEX_NONE;
	}
}

void UTexture2D::UnlinkStreaming()
{
	if (!IsTemplate() && IStreamingManager::Get().IsTextureStreamingEnabled())
	{
		IStreamingManager::Get().GetTextureStreamingManager().RemoveStreamingTexture(this);
	}
}

void UTexture2D::CancelPendingTextureStreaming()
{
	FlushRenderingCommands();

	for( TObjectIterator<UTexture2D> It; It; ++It )
	{
		UTexture2D* CurrentTexture = *It;
		CurrentTexture->CancelPendingMipChangeRequest();
	}

	FlushResourceStreaming();
}

void UTexture2D::PostLoad()
{
#if WITH_EDITOR
	ImportedSize = FIntPoint(Source.GetSizeX(),Source.GetSizeY());

	if (FApp::CanEverRender())
	{
		FinishCachePlatformData();
	}
#endif // #if WITH_EDITOR

	// Route postload, which will update bIsStreamable as UTexture::PostLoad calls UpdateResource.
	Super::PostLoad();
}

void UTexture2D::PreSave(const class ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);
#if WITH_EDITOR
	if( bTemporarilyDisableStreaming )
	{
		bTemporarilyDisableStreaming = false;
		UpdateResource();
	}
#endif
}
void UTexture2D::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
#if WITH_EDITOR
	int32 SizeX = Source.GetSizeX();
	int32 SizeY = Source.GetSizeY();
#else
	int32 SizeX = 0;
	int32 SizeY = 0;
#endif

	const FString DimensionsStr = FString::Printf(TEXT("%dx%d"), SizeX, SizeY);
	OutTags.Add( FAssetRegistryTag("Dimensions", DimensionsStr, FAssetRegistryTag::TT_Dimensional) );
	OutTags.Add( FAssetRegistryTag("HasAlphaChannel", HasAlphaChannel() ? TEXT("True") : TEXT("False"), FAssetRegistryTag::TT_Alphabetical) );
	OutTags.Add( FAssetRegistryTag("Format", GPixelFormats[GetPixelFormat()].Name, FAssetRegistryTag::TT_Alphabetical) );

	Super::GetAssetRegistryTags(OutTags);
}

void UTexture2D::UpdateResource()
{
	// Make sure there are no pending requests in flight.
	while( UpdateStreamingStatus() == true )
	{
		// Give up timeslice.
		FPlatformProcess::Sleep(0);
	}

#if WITH_EDITOR
	// Recache platform data if the source has changed.
	CachePlatformData();
	// clear all the cooked cached platform data if the source could have changed... 
	ClearAllCachedCookedPlatformData();
#endif // #if WITH_EDITOR

	// Route to super.
	Super::UpdateResource();
}


#if WITH_EDITOR
void UTexture2D::PostLinkerChange()
{
	// Changing the linker requires re-creating the resource to make sure streaming behavior is right.
	if( !HasAnyFlags( RF_BeginDestroyed | RF_NeedLoad | RF_NeedPostLoad ) && !IsUnreachable() )
	{
		// Update the resource.
		UpdateResource();
	}
}
#endif

void UTexture2D::BeginDestroy()
{
	// Route BeginDestroy.
	Super::BeginDestroy();

	// Cancel any in flight IO requests
	CancelPendingMipChangeRequest();

	// Safely unlink texture from list of streamable ones.
	UnlinkStreaming();

	TrackTextureEvent( NULL, this, false, 0 );
}

FString UTexture2D::GetDesc()
{
	const int32 MaxResMipBias = GetNumMips() - GetNumMipsAllowed(false);
	return FString::Printf( TEXT("%s %dx%d [%s]"), 
		NeverStream ? TEXT("NeverStreamed") : TEXT("Streamed"), 
		FMath::Max<int32>(GetSizeX() >> MaxResMipBias, 1), 
		FMath::Max<int32>(GetSizeY() >> MaxResMipBias, 1), 
		GPixelFormats[GetPixelFormat()].Name
		);
}

bool UTexture2D::IsReadyForStreaming() const
{
	FTexture2DResource* Texture2DResource = (FTexture2DResource*)Resource;
	return Texture2DResource && Texture2DResource->bReadyForStreaming;
}


void UTexture2D::WaitForStreaming()
{
	// Make sure there are no pending requests in flight otherwise calling UpdateIndividualTexture could be prevented to defined a new requested mip.
	while (	!IsReadyForStreaming() || UpdateStreamingStatus() ) 
	{
		// Give up timeslice.
		FPlatformProcess::Sleep(0);
	}

	// Update the wanted mip and stream in..		
	if (IStreamingManager::Get().IsTextureStreamingEnabled())
	{
		IStreamingManager::Get().GetTextureStreamingManager().UpdateIndividualTexture( this );

		while (	UpdateStreamingStatus() ) 
		{
			// Give up timeslice.
			FPlatformProcess::Sleep(0);
		}
	}
}

bool UTexture2D::UpdateStreamingStatus( bool bWaitForMipFading /*= false*/ )
{
	// if resident and requested mip counts match then no pending request is in flight
	if (PendingUpdate)
	{
		if (GIsRequestingExit || !Resource)
		{
			PendingUpdate->Abort();
		}

		PendingUpdate->Tick(this, FTexture2DUpdate::TT_None);
		if (!PendingUpdate->IsCompleted())
		{
			return true;
		}

#if WITH_EDITOR
		const bool bRebuildPlatformData = PendingUpdate->DDCIsInvalid() && !IsPendingKillOrUnreachable();
#endif

		delete PendingUpdate;
		PendingUpdate = nullptr;

#if WITH_EDITOR
		// When all the requested mips are streamed in, generate an empty property changed event, to force the
		// ResourceSize asset registry tag to be recalculated.
		FPropertyChangedEvent EmptyPropertyChangedEvent(nullptr);
		FCoreUObjectDelegates::OnObjectPropertyChanged.Broadcast(this, EmptyPropertyChangedEvent);

		// We can't load the source art from a bulk data object if the texture itself is pending kill because the linker will have been detached.
		// In this case we don't rebuild the data and instead let the streaming request be cancelled. This will let the garbage collector finish
		// destroying the object.
		if (bRebuildPlatformData)
		{
			ForceRebuildPlatformData();
			// @TODO this can not be called from this callstack since the entry needs to be removed completely from the streamer.
			// UpdateResource();
		}
#endif
	}

	FTexture2DResource* Texture2DResource = (FTexture2DResource*)Resource;
	if (bWaitForMipFading && Texture2DResource && Texture2DResource->bReadyForStreaming)
	{
		return Texture2DResource->MipBiasFade.IsFading();
	}

	return false;
}

bool UTexture2D::CancelPendingMipChangeRequest()
{
	if (PendingUpdate)
	{
		if (!PendingUpdate->IsCancelled())
		{
			PendingUpdate->Abort();
		}
		return true;
	}
	return false;
}

int32 UTexture2D::CalcTextureMemorySize( int32 MipCount ) const
{
	int32 Size = 0;
	if (PlatformData)
	{
		int32 SizeX = GetSizeX();
		int32 SizeY = GetSizeY();
		int32 NumMips = GetNumMips();
		EPixelFormat Format = GetPixelFormat();

		// Figure out what the first mip to use is.
		int32 FirstMip	= FMath::Max( 0, NumMips - MipCount );		
		FIntPoint MipExtents = CalcMipMapExtent(SizeX, SizeY, Format, FirstMip);

		uint32 TextureAlign = 0;
		uint64 TextureSize = RHICalcTexture2DPlatformSize(MipExtents.X, MipExtents.Y, Format, MipCount, 1, 0, TextureAlign);
		Size = (int32)TextureSize;
	}
	return Size;
}

int32 UTexture2D::GetNumMipsAllowed(bool bIgnoreMinResidency) const
{
	const int32 NumMips = GetNumMips();

	// Compute the number of mips that will be available after cooking, as some mips get cooked out.
	// See the logic around FirstMipToSerialize in TextureDerivedData.cpp, SerializePlatformData().
	const int32 LODBiasNoCinematics = UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->CalculateLODBias(this, false);
	const int32 CookedMips = FMath::Clamp<int32>(NumMips - LODBiasNoCinematics, 1, GMaxTextureMipCount);

	// If the data is already cooked, then mips bellow min resident can't be stripped out.
	// This would happen if the data is cooked with some texture group settings, but launched
	// with other settings, adding more constraints on the cooked data.
	if (bIgnoreMinResidency && !FPlatformProperties::RequiresCookedData())
	{
		return CookedMips;
	}
	else if (NumMips > GetMinTextureResidentMipCount())
	{
		// In non cooked, the engine can not partially load the resident mips.
		return FMath::Max<int32>(CookedMips, GetMinTextureResidentMipCount());
	}
	else
	{
		return NumMips;
	}
}


uint32 UTexture2D::CalcTextureMemorySizeEnum( ETextureMipCount Enum ) const
{
	if ( Enum == TMC_ResidentMips )
	{
		return CalcTextureMemorySize( GetNumResidentMips() );
	}
	else if( Enum == TMC_AllMipsBiased)
	{
		return CalcTextureMemorySize( GetNumMipsAllowed(false) );
	}
	else
	{
		return CalcTextureMemorySize( GetNumMips() );
	}
}


bool UTexture2D::GetSourceArtCRC(uint32& OutSourceCRC)
{
	bool bResult = false;
	TArray<uint8> RawData;
#if WITH_EDITOR
	// use the source art if it exists
	if (Source.IsValid())
	{
		// Decompress source art.
		Source.GetMipData(RawData, 0);
	}
	else
	{
		UE_LOG(LogTexture, Log, TEXT("No SourceArt available for %s"), *GetPathName());
	}

	if (RawData.Num() > 0)
	{
		OutSourceCRC = FCrc::MemCrc_DEPRECATED((void*)(RawData.GetData()), RawData.Num());
		bResult = true;
	}
#endif // #if WITH_EDITOR
	return bResult;
}

bool UTexture2D::HasSameSourceArt(UTexture2D* InTexture)
{
	bool bResult = false;
#if WITH_EDITOR
	TArray<uint8> RawData1;
	TArray<uint8> RawData2;
	int32 SizeX = 0;
	int32 SizeY = 0;

	if ((Source.GetSizeX() == InTexture->Source.GetSizeX()) && 
		(Source.GetSizeY() == InTexture->Source.GetSizeY()) &&
		(Source.GetNumMips() == InTexture->Source.GetNumMips()) &&
		(Source.GetNumMips() == 1) &&
		(Source.GetFormat() == InTexture->Source.GetFormat()) &&
		(SRGB == InTexture->SRGB))
	{
		Source.GetMipData(RawData1, 0);
		InTexture->Source.GetMipData(RawData2, 0);
	}

	if ((RawData1.Num() > 0) && (RawData1.Num() == RawData2.Num()))
	{
		if (RawData1 == RawData2)
		{
			bResult = true;
		}
	}
#endif // #if WITH_EDITOR
	return bResult;
}

bool UTexture2D::HasAlphaChannel() const
{
	if (PlatformData && (PlatformData->PixelFormat != PF_DXT1) && (PlatformData->PixelFormat != PF_ATC_RGB))
	{
		return true;
	}
	return false;
}

int32 UTexture2D::GetNumNonStreamingMips() const
{
	int32 NumNonStreamingMips = 0;

	if (PlatformData)
	{
		NumNonStreamingMips = PlatformData->GetNumNonStreamingMips();
	}
	else
	{
		int32 MipCount = GetNumMips();
		NumNonStreamingMips = FMath::Max(0, MipCount - GetMipTailBaseIndex());

		// Take in to account the min resident limit.
		NumNonStreamingMips = FMath::Max(NumNonStreamingMips, UTexture2D::GetMinTextureResidentMipCount());
		NumNonStreamingMips = FMath::Min(NumNonStreamingMips, MipCount);
	}

	return NumNonStreamingMips;
}

void UTexture2D::CalcAllowedMips( int32 MipCount, int32 NumNonStreamingMips, int32 LODBias, int32& OutMinAllowedMips, int32& OutMaxAllowedMips )
{
	// Calculate the minimum number of mip-levels required.
	int32 MinAllowedMips = UTexture2D::GetMinTextureResidentMipCount();
	MinAllowedMips = FMath::Max( MinAllowedMips, MipCount - LODBias );
	MinAllowedMips = FMath::Min( MinAllowedMips, NumNonStreamingMips );
	MinAllowedMips = FMath::Min( MinAllowedMips, MipCount );

	// Calculate the maximum number of mip-levels.
	int32 MaxAllowedMips = FMath::Max( MipCount - LODBias, MinAllowedMips );
	MaxAllowedMips = FMath::Min( MaxAllowedMips, GMaxTextureMipCount );

	// Make sure min <= max
	MinAllowedMips = FMath::Min(MinAllowedMips, MaxAllowedMips);

	// Return results.
	OutMinAllowedMips = MinAllowedMips;
	OutMaxAllowedMips = MaxAllowedMips;
}

FTextureResource* UTexture2D::CreateResource()
{
	FLinker* Linker = GetLinker();
	int32 NumMips = GetNumMips();
	int32 RequestedMips = 0;

	// Determine whether or not this texture can be streamed.
	bIsStreamable = 
#if PLATFORM_SUPPORTS_TEXTURE_STREAMING
					IStreamingManager::Get().IsTextureStreamingEnabled() &&
#endif
					!NeverStream && 
					(NumMips > 1) && 
					(LODGroup != TEXTUREGROUP_UI) && 
					!bTemporarilyDisableStreaming;
	
	if (bIsStreamable && NumMips > 0)
	{
		// Check to see if at least one mip can be streamed.
		bIsStreamable = false;
		const TIndirectArray<FTexture2DMipMap>& Mips = GetPlatformMips();
		for (int32 MipIndex = 0; MipIndex < Mips.Num(); ++MipIndex)
		{
			const FTexture2DMipMap& Mip = Mips[MipIndex];
			bool bMipIsInDerivedDataCache = false;
#if WITH_EDITORONLY_DATA
			bMipIsInDerivedDataCache = Mip.DerivedDataKey.IsEmpty() == false;
#endif
			if (bMipIsInDerivedDataCache ||		// Can stream from the DDC.
				Mip.BulkData.CanLoadFromDisk())	// Can stream from disk.
			{
				bIsStreamable = true;
				break;
			}
		}
	}

	const EPixelFormat PixelFormat = GetPixelFormat();
	const bool bIncompatibleTexture = (NumMips == 0);
	const bool bTextureTooLarge = FMath::Max(GetSizeX(), GetSizeY()) > (int32)GetMax2DTextureDimension();
	// Too large textures with full mip chains are OK as we load up to max supported mip.
	const bool bNotSupportedByRHI = NumMips == 1 && bTextureTooLarge;
	const bool bFormatNotSupported = !(GPixelFormats[PixelFormat].Supported);

	if (bIncompatibleTexture || bNotSupportedByRHI || bFormatNotSupported)
	{
		if (bFormatNotSupported)
		{
			UE_LOG(LogTexture, Error, TEXT("%s is %s which is not supported."), *GetFullName(), GPixelFormats[PixelFormat].Name);
		}
		else if (bNotSupportedByRHI)
		{
			UE_LOG(LogTexture, Warning, TEXT("%s cannot be created, exceeds this rhi's maximum dimension (%d) and has no mip chain to fall back on."), *GetFullName(), GetMax2DTextureDimension());
		}
		else if (bIncompatibleTexture)
		{
			UE_LOG(LogTexture, Error, TEXT("%s contains no miplevels! Please delete. (Format: %d)"), *GetFullName(), (int)GetPixelFormat());
		}
	}
	else
	{
		int32 NumNonStreamingMips = NumMips;

		// Handle streaming textures.
		if( bIsStreamable )
		{
			// Only request lower miplevels and let texture streaming code load the rest.
			NumNonStreamingMips = GetNumNonStreamingMips();

#if !PLATFORM_SUPPORTS_TEXTURE_STREAMING // eg, Android
			NumNonStreamingMips = MobileReduceLoadedMips(NumNonStreamingMips);
#endif

			RequestedMips = NumNonStreamingMips;
		}
		// Handle non- streaming textures.
		else
		{
			// Request all miplevels allowed by device. LOD settings are taken into account below.
			RequestedMips = GMaxTextureMipCount;
		}

		// Take allowed mip counts in to account.
		int32 MinAllowedMips = NumNonStreamingMips;
		int32 MaxAllowedMips = NumMips;
		CalcAllowedMips( NumMips, NumNonStreamingMips, GetCachedLODBias(), MinAllowedMips, MaxAllowedMips );
		RequestedMips = FMath::Min( MaxAllowedMips, RequestedMips );
		RequestedMips = FMath::Max( MinAllowedMips, RequestedMips );

		// should be as big as the mips we have already directly loaded into GPU mem
		if( ResourceMem )
		{	
			RequestedMips = FMath::Max( RequestedMips, ResourceMem->GetNumMips() );
		}
		RequestedMips	= FMath::Max( RequestedMips, 1 );
	}

	FTexture2DResource* Texture2DResource = NULL;

	// Create and return 2D resource if there are any miplevels.
	if( RequestedMips > 0 )
	{
		Texture2DResource = new FTexture2DResource(this, RequestedMips);
		// preallocated memory for the UTexture2D resource is now owned by this resource
		// and will be freed by the RHI resource or when the FTexture2DResource is deleted
		ResourceMem = NULL;
	}
	else
	{
		// Streaming requires that we have a resource with a base number of mips.
		bIsStreamable = false;
	}

	// Unlink and relink if streamable.
	UnlinkStreaming();
	if( bIsStreamable )
	{
		LinkStreaming();
	}

	return Texture2DResource;
}


void UTexture2D::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	if (CumulativeResourceSize.GetResourceSizeMode() == EResourceSizeMode::Exclusive)
	{
		CumulativeResourceSize.AddUnknownMemoryBytes(CalcTextureMemorySize(GetNumResidentMips()));
	}
	else
	{
		if (PlatformData)
		{
			CumulativeResourceSize.AddUnknownMemoryBytes(CalcTextureSize(GetSizeX(), GetSizeY(), GetPixelFormat(), GetNumMips()));
		}
	}
}


bool UTexture2D::ShouldMipLevelsBeForcedResident() const
{
	if ( bGlobalForceMipLevelsToBeResident || bForceMiplevelsToBeResident || LODGroup == TEXTUREGROUP_Skybox )
	{
		return true;
	}
	if ( ForceMipLevelsToBeResidentTimestamp >= FApp::GetCurrentTime() )
	{
		return true;
	}
	return false;
}


bool UTexture2D::IsFullyStreamedIn()
{
	// Non-streamable textures are considered to be fully streamed in.
	bool bIsFullyStreamedIn = true;
	if( bIsStreamable )
	{
		// Calculate maximum number of mips potentially being resident based on LOD settings and device max texture count.
		int32 MaxResidentMips = FMath::Max( 1, FMath::Min( GetNumMips() - GetCachedLODBias(), GMaxTextureMipCount ) );
		// >= as LOD settings can change dynamically and we consider a texture that is about to lose miplevels to still
		// be fully streamed.
		bIsFullyStreamedIn = GetNumResidentMips() >= MaxResidentMips;
	}
	return bIsFullyStreamedIn;
}


UTexture2D* UTexture2D::CreateTransient(int32 InSizeX, int32 InSizeY, EPixelFormat InFormat)
{
	UTexture2D* NewTexture = NULL;
	if (InSizeX > 0 && InSizeY > 0 &&
		(InSizeX % GPixelFormats[InFormat].BlockSizeX) == 0 &&
		(InSizeY % GPixelFormats[InFormat].BlockSizeY) == 0)
	{
		NewTexture = NewObject<UTexture2D>(
			GetTransientPackage(),
			NAME_None,
			RF_Transient
			);

		NewTexture->PlatformData = new FTexturePlatformData();
		NewTexture->PlatformData->SizeX = InSizeX;
		NewTexture->PlatformData->SizeY = InSizeY;
		NewTexture->PlatformData->PixelFormat = InFormat;

		// Allocate first mipmap.
		int32 NumBlocksX = InSizeX / GPixelFormats[InFormat].BlockSizeX;
		int32 NumBlocksY = InSizeY / GPixelFormats[InFormat].BlockSizeY;
		FTexture2DMipMap* Mip = new(NewTexture->PlatformData->Mips) FTexture2DMipMap();
		Mip->SizeX = InSizeX;
		Mip->SizeY = InSizeY;
		Mip->BulkData.Lock(LOCK_READ_WRITE);
		Mip->BulkData.Realloc(NumBlocksX * NumBlocksY * GPixelFormats[InFormat].BlockBytes);
		Mip->BulkData.Unlock();
	}
	else
	{
		UE_LOG(LogTexture, Warning, TEXT("Invalid parameters specified for UTexture2D::Create()"));
	}
	return NewTexture;
}

void UTexture2D::SetForceMipLevelsToBeResident( float Seconds, int32 CinematicTextureGroups )
{
	uint32 TextureGroupBitfield = (uint32) CinematicTextureGroups;
	uint32 MyTextureGroup = FMath::BitFlag[LODGroup];
	bUseCinematicMipLevels = (TextureGroupBitfield & MyTextureGroup) ? true : false;
	ForceMipLevelsToBeResidentTimestamp = FApp::GetCurrentTime() + Seconds;
}

int32 UTexture2D::Blueprint_GetSizeX() const
{
	return GetSizeX();
}

int32 UTexture2D::Blueprint_GetSizeY() const
{
	return GetSizeY();
}

void UTexture2D::UpdateTextureRegions(int32 MipIndex, uint32 NumRegions, const FUpdateTextureRegion2D* Regions, uint32 SrcPitch, uint32 SrcBpp, uint8* SrcData, TFunction<void(uint8* SrcData, const FUpdateTextureRegion2D* Regions)> DataCleanupFunc)
{
	if (!bTemporarilyDisableStreaming && bIsStreamable)
	{
		UE_LOG(LogTexture, Log, TEXT("UpdateTextureRegions called for %s without calling TemporarilyDisableStreaming"), *GetPathName());
	}
	else
	if (Resource)
	{
		struct FUpdateTextureRegionsData
		{
			FTexture2DResource* Texture2DResource;
			int32 MipIndex;
			uint32 NumRegions;
			const FUpdateTextureRegion2D* Regions;
			uint32 SrcPitch;
			uint32 SrcBpp;
			uint8* SrcData;
		};

		FUpdateTextureRegionsData* RegionData = new FUpdateTextureRegionsData;

		RegionData->Texture2DResource = (FTexture2DResource*)Resource;
		RegionData->MipIndex = MipIndex;
		RegionData->NumRegions = NumRegions;
		RegionData->Regions = Regions;
		RegionData->SrcPitch = SrcPitch;
		RegionData->SrcBpp = SrcBpp;
		RegionData->SrcData = SrcData;

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			UpdateTextureRegionsData,
			FUpdateTextureRegionsData*, RegionData, RegionData,			
			TFunction<void(uint8* SrcData, const FUpdateTextureRegion2D* Regions)>, DataCleanupFunc, DataCleanupFunc,
			{
			for (uint32 RegionIndex = 0; RegionIndex < RegionData->NumRegions; ++RegionIndex)
			{
				int32 CurrentFirstMip = RegionData->Texture2DResource->GetCurrentFirstMip();
				if (RegionData->MipIndex >= CurrentFirstMip)
				{
					RHIUpdateTexture2D(
						RegionData->Texture2DResource->GetTexture2DRHI(),
						RegionData->MipIndex - CurrentFirstMip,
						RegionData->Regions[RegionIndex],
						RegionData->SrcPitch,
						RegionData->SrcData
						+ RegionData->Regions[RegionIndex].SrcY * RegionData->SrcPitch
						+ RegionData->Regions[RegionIndex].SrcX * RegionData->SrcBpp
						);
				}
			}
			DataCleanupFunc(RegionData->SrcData, RegionData->Regions);
			delete RegionData;
		});
	}
}

#if WITH_EDITOR
void UTexture2D::TemporarilyDisableStreaming()
{
	if( !bTemporarilyDisableStreaming )
	{
		bTemporarilyDisableStreaming = true;
		UpdateResource();
	}
}

#endif



float UTexture2D::GetGlobalMipMapLODBias()
{
	float BiasOffset = CVarSetMipMapLODBias.GetValueOnAnyThread(); // called from multiple threads.
	return FMath::Clamp(BiasOffset, -15.0f, 15.0f);
}

void UTexture2D::RefreshSamplerStates()
{
	if (Resource == nullptr)
	{
		return;
	}

	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		RefreshSamplerStatesCommand,
		FTexture2DResource*, Texture2DResource, ((FTexture2DResource*)Resource),
	{
		Texture2DResource->RefreshSamplerStates();
	});
}

/*-----------------------------------------------------------------------------
	FTexture2DResource implementation.
-----------------------------------------------------------------------------*/

/**
 * Minimal initialization constructor.
 *
 * @param InOwner			UTexture2D which this FTexture2DResource represents.
 * @param InitialMipCount	Initial number of miplevels to upload to card
 * @param InFilename		Filename to read data from
 */
FTexture2DResource::FTexture2DResource( UTexture2D* InOwner, int32 InitialMipCount )
:	Owner( InOwner )
,	ResourceMem( InOwner->ResourceMem )
,	bReadyForStreaming(false)
,	bUseVirtualUpdatePath(false)
#if STATS
,	TextureSize( 0 )
#endif
{
	MipFadeSetting = (Owner->LODGroup == TEXTUREGROUP_Lightmap || Owner->LODGroup == TEXTUREGROUP_Shadowmap) ? MipFade_Slow : MipFade_Normal;

	// HDR images are stored in linear but still require gamma correction to display correctly.
	bIgnoreGammaConversions = !Owner->SRGB && Owner->CompressionSettings != TC_HDR;
	bSRGB = InOwner->SRGB;

	check(InitialMipCount>0);
	check(ARRAY_COUNT(MipData)>=GMaxTextureMipCount);

	// Keep track of first miplevel to use.
	CurrentFirstMip = InOwner->GetNumMips() - InitialMipCount;

	check(CurrentFirstMip>=0);
	// texture must be as big as base miptail level
	check(CurrentFirstMip<=Owner->GetMipTailBaseIndex());

	// Retrieve initial mip data.
	FMemory::Memzero(MipData, sizeof(MipData));
	InOwner->GetMipData(CurrentFirstMip, &MipData[CurrentFirstMip]);
	STAT( TextureSize = Owner->CalcTextureMemorySize( InitialMipCount ) );
	STAT( LODGroupStatName = TextureGroupStatFNames[Owner->LODGroup] );
}

/**
 * Destructor, freeing MipData in the case of resource being destroyed without ever 
 * having been initialized by the rendering thread via InitRHI.
 */
FTexture2DResource::~FTexture2DResource()
{
	// free resource memory that was preallocated
	// The deletion needs to happen in the rendering thread.
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		DeleteResourceMem,
		FTexture2DResourceMem*,ResourceMem,ResourceMem,
		{
			delete ResourceMem;
		});

	// Make sure we're not leaking memory if InitRHI has never been called.
	for( int32 MipIndex=0; MipIndex<ARRAY_COUNT(MipData); MipIndex++ )
	{
		// free any mip data that was copied 
		if( MipData[MipIndex] )
		{
			FMemory::Free( MipData[MipIndex] );
		}
		MipData[MipIndex] = NULL;
	}
}

/**
 * Called when the resource is initialized. This is only called by the rendering thread.
 */
void FTexture2DResource::InitRHI()
{
	FTexture2DScopedDebugInfo ScopedDebugInfo(Owner);
	INC_DWORD_STAT_BY( STAT_TextureMemory, TextureSize );
	INC_DWORD_STAT_FNAME_BY( LODGroupStatName, TextureSize );

	const TIndirectArray<FTexture2DMipMap>& OwnerMips = Owner->GetPlatformMips();
	const int32 RequestedMips = OwnerMips.Num() - CurrentFirstMip;
	uint32 SizeX = OwnerMips[CurrentFirstMip].SizeX;
	uint32 SizeY = OwnerMips[CurrentFirstMip].SizeY;

	// Create the RHI texture.
	uint32 TexCreateFlags = (Owner->SRGB ? TexCreate_SRGB : 0) | TexCreate_OfflineProcessed | TexCreate_Streamable;
	// if no miptail is available then create the texture without a packed miptail
	if( Owner->GetMipTailBaseIndex() == -1 )
	{
		TexCreateFlags |= TexCreate_NoMipTail;
	}
	// disable tiled format if needed
	if( Owner->bNoTiling )
	{
		TexCreateFlags |= TexCreate_NoTiling;
	}

	// Determine if this texture should use the virtual update path when streaming in and out mips. 
	// Note that because of "r.VirtualTextureReducedMemory" it might use a virtual allocation initially.
	bUseVirtualUpdatePath = CanCreateAsVirtualTexture(TexCreateFlags);

	EPixelFormat EffectiveFormat = Owner->GetPixelFormat();

	CreateSamplerStates(UTexture2D::GetGlobalMipMapLODBias() + GetDefaultMipMapBias());

	// Set the greyscale format flag appropriately.
	bGreyScaleFormat = (EffectiveFormat == PF_G8) || (EffectiveFormat == PF_BC4);
	
	// Check if this is the initial creation of the texture, or if we're recreating a texture that was released by ReleaseRHI.
	if( !bReadyForStreaming )
	{
		bool bSkipRHITextureCreation = false; //Owner->bIsCompositingSource;
		// PVS - Studio has pointed out that bSkipRHITextureCreation is a codesmell (!bSkipRHITextureCreation is always true)
		// for now, we have disabled the warning, but consider removing the variable if requirements indicate it will never be used.
		if (GIsEditor || (!bSkipRHITextureCreation)) //-V560 
		{
			static auto CVarVirtualTextureReducedMemoryEnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VirtualTextureReducedMemory"));
			check(CVarVirtualTextureReducedMemoryEnabled);

			if ( Owner->bIsStreamable && bUseVirtualUpdatePath && (CVarVirtualTextureReducedMemoryEnabled->GetValueOnRenderThread() == 0 || RequestedMips > UTexture2D::GetMinTextureResidentMipCount()) )
			{
				TexCreateFlags |= TexCreate_Virtual;

				FRHIResourceCreateInfo CreateInfo(ResourceMem);
				Texture2DRHI	= RHICreateTexture2D( OwnerMips[0].SizeX, OwnerMips[0].SizeY, EffectiveFormat, OwnerMips.Num(), 1, TexCreateFlags, CreateInfo);
				RHIVirtualTextureSetFirstMipInMemory(Texture2DRHI, CurrentFirstMip);
				RHIVirtualTextureSetFirstMipVisible(Texture2DRHI, CurrentFirstMip);

				check( ResourceMem == nullptr );

				// Read the resident mip-levels into the RHI texture.
				for( int32 MipIndex=CurrentFirstMip; MipIndex < OwnerMips.Num(); MipIndex++ )
				{
					if ( MipData[MipIndex] != NULL )
					{
						uint32 DestPitch;
						void* TheMipData = RHILockTexture2D( Texture2DRHI, MipIndex, RLM_WriteOnly, DestPitch, false );
						GetData( MipIndex, TheMipData, DestPitch );
						RHIUnlockTexture2D( Texture2DRHI, MipIndex, false );
					}
				}

				// Update mip-level fading.
				MipBiasFade.SetNewMipCount( RequestedMips, RequestedMips, LastRenderTime, MipFadeSetting );

				TextureRHI = Texture2DRHI;
				TextureRHI->SetName(Owner->GetFName());
				RHIBindDebugLabelName(TextureRHI, *Owner->GetName());
				RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI,TextureRHI);

				// We're done with initialization.
				bReadyForStreaming = true;

				return;
			}

			// create texture with ResourceMem data when available
			FRHIResourceCreateInfo CreateInfo(ResourceMem);
			Texture2DRHI	= RHICreateTexture2D( SizeX, SizeY, EffectiveFormat, RequestedMips, 1, TexCreateFlags, CreateInfo);
			TextureRHI		= Texture2DRHI;
			TextureRHI->SetName(Owner->GetFName());
			RHIBindDebugLabelName(TextureRHI, *Owner->GetName());
			RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI,TextureRHI);

			check(Owner->PlatformData->Mips[CurrentFirstMip].SizeX == Texture2DRHI->GetSizeX() && Owner->PlatformData->Mips[CurrentFirstMip].SizeY == Texture2DRHI->GetSizeY());

			if( ResourceMem )
			{
				// when using resource memory the RHI texture has already been initialized with data and won't need to have mips copied
				check(RequestedMips == ResourceMem->GetNumMips());
				check(SizeX == ResourceMem->GetSizeX() && SizeY == ResourceMem->GetSizeY());
				for( int32 MipIndex=0; MipIndex<Owner->PlatformData->Mips.Num(); MipIndex++ )
				{
					MipData[MipIndex] = NULL;
				}
			}
			else
			{
				// Read the resident mip-levels into the RHI texture.
				for( int32 MipIndex=CurrentFirstMip; MipIndex<Owner->PlatformData->Mips.Num(); MipIndex++ )
				{
					if( MipData[MipIndex] != NULL )
					{
						uint32 DestPitch;
						void* TheMipData = RHILockTexture2D( Texture2DRHI, MipIndex - CurrentFirstMip, RLM_WriteOnly, DestPitch, false );
						GetData( MipIndex, TheMipData, DestPitch );
						RHIUnlockTexture2D( Texture2DRHI, MipIndex - CurrentFirstMip, false );
					}
				}
			}
		}

		// Update mip-level fading.
		MipBiasFade.SetNewMipCount( RequestedMips, RequestedMips, LastRenderTime, MipFadeSetting );

		// We're done with initialization.
		bReadyForStreaming = true;
	}
	else
	{
		// Recreate the texture from the texture contents that were saved by ReleaseRHI.
		bool bSkipRHITextureCreation = false; //Owner->bIsCompositingSource;
		// PVS - Studio has pointed out that bSkipRHITextureCreation is a codesmell (!bSkipRHITextureCreation is always true)
		// for now, we have disabled the warning, but consider removing the variable if requirements indicate it will never be used.
		if (GIsEditor || (!bSkipRHITextureCreation)) //-V560
		{
			FRHIResourceCreateInfo CreateInfo;
			Texture2DRHI	= RHICreateTexture2D( SizeX, SizeY, EffectiveFormat, RequestedMips, 1, TexCreateFlags, CreateInfo );
			TextureRHI		= Texture2DRHI;
			TextureRHI->SetName(Owner->GetFName());
			RHIBindDebugLabelName(TextureRHI, *Owner->GetName());
			RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI,TextureRHI);
			for( int32 MipIndex=CurrentFirstMip; MipIndex<OwnerMips.Num(); MipIndex++ )
			{
				if( MipData[MipIndex] != NULL )
				{
					uint32 DestPitch;
					void* TheMipData = RHILockTexture2D( Texture2DRHI, MipIndex - CurrentFirstMip, RLM_WriteOnly, DestPitch, false );
					GetData( MipIndex, TheMipData, DestPitch );
					RHIUnlockTexture2D( Texture2DRHI, MipIndex - CurrentFirstMip, false );
				}
			}
		}
	}
}

/**
 * Called when the resource is released. This is only called by the rendering thread.
 */
void FTexture2DResource::ReleaseRHI()
{
	const TIndirectArray<FTexture2DMipMap>& OwnerMips = Owner->GetPlatformMips();

	// It should be safe to release the texture.
	check(!Owner->HasPendingUpdate());

	if ( (Texture2DRHI->GetFlags() & TexCreate_Virtual) != TexCreate_Virtual )
	{
		check(OwnerMips[CurrentFirstMip].SizeX == Texture2DRHI->GetSizeX() && OwnerMips[CurrentFirstMip].SizeY == Texture2DRHI->GetSizeY());
	}

	DEC_DWORD_STAT_BY( STAT_TextureMemory, TextureSize );
	DEC_DWORD_STAT_FNAME_BY( LODGroupStatName, TextureSize );

	FTextureResource::ReleaseRHI();
	Texture2DRHI.SafeRelease();
	RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI,FTextureRHIParamRef());
}

void FTexture2DResource::CreateSamplerStates(float MipMapBias)
{
	// Create the sampler state RHI resource.
	FSamplerStateInitializerRHI SamplerStateInitializer
	(
	  (ESamplerFilter)UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->GetSamplerFilter(Owner),
	  Owner->AddressX == TA_Wrap ? AM_Wrap : (Owner->AddressX == TA_Clamp ? AM_Clamp : AM_Mirror),
	  Owner->AddressY == TA_Wrap ? AM_Wrap : (Owner->AddressY == TA_Clamp ? AM_Clamp : AM_Mirror),
	  AM_Wrap,
	  MipMapBias
	);
	SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);

	// Create a custom sampler state for using this texture in a deferred pass, where ddx / ddy are discontinuous
	FSamplerStateInitializerRHI DeferredPassSamplerStateInitializer
	(
	  (ESamplerFilter)UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->GetSamplerFilter(Owner),
	  Owner->AddressX == TA_Wrap ? AM_Wrap : (Owner->AddressX == TA_Clamp ? AM_Clamp : AM_Mirror),
	  Owner->AddressY == TA_Wrap ? AM_Wrap : (Owner->AddressY == TA_Clamp ? AM_Clamp : AM_Mirror),
	  AM_Wrap,
	  MipMapBias,
	  // Disable anisotropic filtering, since aniso doesn't respect MaxLOD
	  1,
	  0,
	  // Prevent the less detailed mip levels from being used, which hides artifacts on silhouettes due to ddx / ddy being very large
	  // This has the side effect that it increases minification aliasing on light functions
	  2
	);

	DeferredPassSamplerStateRHI = RHICreateSamplerState(DeferredPassSamplerStateInitializer);
}

// Recreate the sampler states (used when updating mip map lod bias offset)
void FTexture2DResource::RefreshSamplerStates()
{
	DeferredPassSamplerStateRHI.SafeRelease();
	SamplerStateRHI.SafeRelease();

	CreateSamplerStates(UTexture2D::GetGlobalMipMapLODBias() + GetDefaultMipMapBias());
}

/** Returns the width of the texture in pixels. */
uint32 FTexture2DResource::GetSizeX() const
{
	return Owner->GetSizeX();
}

/** Returns the height of the texture in pixels. */
uint32 FTexture2DResource::GetSizeY() const
{
	return Owner->GetSizeY();
}

/** Returns the default mip bias for this texture. */
int32 FTexture2DResource::GetDefaultMipMapBias() const
{
	if ( Owner->LODGroup == TEXTUREGROUP_UI )
	{
		if ( CVarForceHighestMipOnUITexturesEnabled.GetValueOnAnyThread() > 0 )
		{
			const TIndirectArray<FTexture2DMipMap>& OwnerMips = Owner->GetPlatformMips();
			return -OwnerMips.Num();
		}
	}
	
	return 0;
}

/**
 * Writes the data for a single mip-level into a destination buffer.
 *
 * @param MipIndex		Index of the mip-level to read.
 * @param Dest			Address of the destination buffer to receive the mip-level's data.
 * @param DestPitch		Number of bytes per row
 */
void FTexture2DResource::GetData( uint32 MipIndex, void* Dest, uint32 DestPitch )
{
	const FTexture2DMipMap& MipMap = Owner->PlatformData->Mips[MipIndex];
	check( MipData[MipIndex] );

	// for platforms that returned 0 pitch from Lock, we need to just use the bulk data directly, never do 
	// runtime block size checking, conversion, or the like
	if (DestPitch == 0)
	{
		FMemory::Memcpy(Dest, MipData[MipIndex], MipMap.BulkData.GetBulkDataSize());
	}
	else
	{
		EPixelFormat PixelFormat = Owner->GetPixelFormat();
		const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;		// Block width in pixels
		const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;		// Block height in pixels
		const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
		uint32 NumColumns		= (MipMap.SizeX + BlockSizeX - 1) / BlockSizeX;	// Num-of columns in the source data (in blocks)
		uint32 NumRows			= (MipMap.SizeY + BlockSizeY - 1) / BlockSizeY;	// Num-of rows in the source data (in blocks)
		if ( PixelFormat == PF_PVRTC2 || PixelFormat == PF_PVRTC4 )
		{
			// PVRTC has minimum 2 blocks width and height
			NumColumns = FMath::Max<uint32>(NumColumns, 2);
			NumRows = FMath::Max<uint32>(NumRows, 2);
		}
		const uint32 SrcPitch   = NumColumns * BlockBytes;						// Num-of bytes per row in the source data
		const uint32 EffectiveSize = BlockBytes*NumColumns*NumRows;

	#if !WITH_EDITORONLY_DATA
		// on console we don't want onload conversions
		checkf(EffectiveSize == (uint32)MipMap.BulkData.GetBulkDataSize(), 
			TEXT("Texture '%s', mip %d, has a BulkDataSize [%d] that doesn't match calculated size [%d]. Texture size %dx%d, format %d"),
			*Owner->GetPathName(), MipIndex, MipMap.BulkData.GetBulkDataSize(), EffectiveSize, Owner->GetSizeX(), Owner->GetSizeY(), (int32)Owner->GetPixelFormat());
	#endif

		// Copy the texture data.
		CopyTextureData2D(MipData[MipIndex],Dest,MipMap.SizeY,PixelFormat,SrcPitch,DestPitch);
	}
	
	// Free data retrieved via GetCopy inside constructor.
	FMemory::Free(MipData[MipIndex]);
	MipData[MipIndex] = NULL;
}

bool UTexture2D::StreamIn(int32 NewMipCount, bool bHighPrio)
{
	check(IsInGameThread());
	FTexture2DResource* Texture2DResource = (FTexture2DResource*)Resource;
	if (bIsStreamable && !PendingUpdate && Texture2DResource && Texture2DResource->bReadyForStreaming && NewMipCount > GetNumResidentMips())
	{
#if WITH_EDITORONLY_DATA
		if (FPlatformProperties::HasEditorOnlyData())
		{
			if (GRHISupportsAsyncTextureCreation)
			{
				PendingUpdate = new FTexture2DStreamIn_DDC_AsyncCreate(this, NewMipCount);
			}
			else
			{
				PendingUpdate = new FTexture2DStreamIn_DDC_AsyncReallocate(this, NewMipCount);
			}
		}
		else
#endif
		{
			// If the future texture is to be a virtual texture, use the virtual stream in path.
			if (Texture2DResource->bUseVirtualUpdatePath)
			{
				PendingUpdate = new FTexture2DStreamIn_IO_Virtual(this, NewMipCount, bHighPrio);
			}
			// If the platform supports creating the new texture on an async thread, use that path.
			else if (GRHISupportsAsyncTextureCreation)
			{
				PendingUpdate = new FTexture2DStreamIn_IO_AsyncCreate(this, NewMipCount,bHighPrio);
			}
			// Otherwise use the default path.
			else
			{
				PendingUpdate = new FTexture2DStreamIn_IO_AsyncReallocate(this, NewMipCount, bHighPrio);
			}
		}

		// The object starts in the locked state while it is being initialized.
		PendingUpdate->DoUnlock();
		return !PendingUpdate->IsCancelled();
	}
	return false;
}

bool UTexture2D::StreamOut(int32 NewMipCount)
{
	check(IsInGameThread());
	FTexture2DResource* Texture2DResource = (FTexture2DResource*)Resource;
	if (bIsStreamable && !PendingUpdate && Texture2DResource && Texture2DResource->bReadyForStreaming && NewMipCount < GetNumResidentMips())
	{
		if (Texture2DResource->bUseVirtualUpdatePath)
		{
			PendingUpdate = new FTexture2DStreamOut_Virtual(this, NewMipCount);
		}
		else
		{
			PendingUpdate = new FTexture2DStreamOut_AsyncReallocate(this, NewMipCount);
		}

		// The object starts in the locked state while it is being initialized.
		PendingUpdate->DoUnlock();

		return !PendingUpdate->IsCancelled();
	}
	return false;
}

void FTexture2DResource::UpdateTexture(FTexture2DRHIRef& InTextureRHI, int32 InNewFirstMip)
{
	checkSlow(IsInRenderingThread() && IsValidRef(InTextureRHI));

	if (Owner)
	{
		// Update mip-level fading.
		if (CurrentFirstMip != InNewFirstMip)
		{
			const int32 NumMips = Owner->GetNumMips();
			const int32 ResidentMips = NumMips - CurrentFirstMip;
			const int32 RequestedMips = NumMips - InNewFirstMip;
			MipBiasFade.SetNewMipCount(FMath::Max<int32>(RequestedMips, ResidentMips), RequestedMips, LastRenderTime, MipFadeSetting);

			DEC_DWORD_STAT_BY( STAT_TextureMemory, TextureSize );
			DEC_DWORD_STAT_FNAME_BY( LODGroupStatName, TextureSize );
			STAT( TextureSize = Owner->CalcTextureMemorySize(RequestedMips) );
			INC_DWORD_STAT_BY( STAT_TextureMemory, TextureSize );
			INC_DWORD_STAT_FNAME_BY( LODGroupStatName, TextureSize );
		}

		TextureRHI		= InTextureRHI;
		Texture2DRHI	= InTextureRHI;
		CurrentFirstMip = InNewFirstMip;
		RHIUpdateTextureReference(Owner->TextureReference.TextureReferenceRHI, TextureRHI);
	}
}

FString FTexture2DResource::GetFriendlyName() const
{
	return Owner->GetPathName();
}

void FTexture2DArrayResource::InitRHI()
{
	// Create the RHI texture.
	const uint32 TexCreateFlags = (bSRGB ? TexCreate_SRGB : 0) | TexCreate_OfflineProcessed;
	FRHIResourceCreateInfo CreateInfo;
	FTexture2DArrayRHIRef TextureArray = RHICreateTexture2DArray(SizeX, SizeY, GetNumValidTextures(), Format, NumMips, TexCreateFlags, CreateInfo);
	TextureRHI = TextureArray;

	// Read the mip-levels into the RHI texture.
	int32 TextureIndex = 0;
	for (TMap<const UTexture2D*, FTextureArrayDataEntry>::TConstIterator It(CachedData); It; ++It)
	{
		const FTextureArrayDataEntry& CurrentDataEntry = It.Value();
		if (CurrentDataEntry.MipData.Num() > 0)
		{
			check(CurrentDataEntry.MipData.Num() == NumMips);
			for (int32 MipIndex = 0; MipIndex < CurrentDataEntry.MipData.Num(); MipIndex++)
			{
				if (CurrentDataEntry.MipData[MipIndex].Data.Num() > 0)
				{
					uint32 DestStride;
					void* TheMipData = RHILockTexture2DArray(TextureArray, TextureIndex, MipIndex, RLM_WriteOnly, DestStride, false);
					GetData(CurrentDataEntry, MipIndex, TheMipData, DestStride);
					RHIUnlockTexture2DArray(TextureArray, TextureIndex, MipIndex, false);
				}
			}
			TextureIndex++;
		}
	}

	// Create the sampler state RHI resource.
	FSamplerStateInitializerRHI SamplerStateInitializer
	(
		Filter,
		AM_Clamp,
		AM_Clamp,
		AM_Clamp
	);
	SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
}

FIncomingTextureArrayDataEntry::FIncomingTextureArrayDataEntry(UTexture2D* InTexture)
{
	// Can only access these UTexture members on the game thread
	checkSlow(IsInGameThread());

	SizeX = InTexture->GetSizeX();
	SizeY = InTexture->GetSizeY();
	NumMips = InTexture->GetNumMips();
	LODGroup = (TextureGroup)InTexture->LODGroup;
	Format = InTexture->GetPixelFormat();
	Filter = (ESamplerFilter)UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->GetSamplerFilter(InTexture);
	bSRGB = InTexture->SRGB;

	MipData.Empty(NumMips);
	MipData.AddZeroed(NumMips);
	for (int32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
	{
		FTexture2DMipMap& Mip = InTexture->PlatformData->Mips[MipIndex];
		if (MipIndex < NumMips && Mip.BulkData.IsAvailableForUse())
		{
			MipData[MipIndex].SizeX = Mip.SizeX;
			MipData[MipIndex].SizeY = Mip.SizeY;
			
			const int32 MipDataSize = Mip.BulkData.GetElementCount() * Mip.BulkData.GetElementSize();
			MipData[MipIndex].Data.Empty(MipDataSize);
			MipData[MipIndex].Data.AddUninitialized(MipDataSize);
			// Get copy of data, potentially loading array or using already loaded version.
			void* MipDataPtr = MipData[MipIndex].Data.GetData();
			Mip.BulkData.GetCopy(&MipDataPtr, false);
		}
	}
}

/** 
 * Adds a texture to the texture array.  
 * This is called on the rendering thread, so it must not dereference NewTexture.
 */
void FTexture2DArrayResource::AddTexture2D(UTexture2D* NewTexture, const FIncomingTextureArrayDataEntry* InEntry)
{
	bool bValidTexture = false;
	if (CachedData.Num() == 0)
	{
		// Copy the UTexture's parameters so that we won't need to dereference it outside of this function,
		// Since the texture may be deleted outside of this function.
		SizeX = InEntry->SizeX;
		SizeY = InEntry->SizeY;
		NumMips = InEntry->NumMips;
		LODGroup = InEntry->LODGroup;
		Format = InEntry->Format;
		Filter = InEntry->Filter;
		bSRGB = InEntry->bSRGB;

		bValidTexture = true;
	}
	else if (SizeX == InEntry->SizeX
		&& SizeY == InEntry->SizeY
		&& NumMips == InEntry->NumMips
		&& LODGroup == InEntry->LODGroup
		&& Format == InEntry->Format
		&& bSRGB == InEntry->bSRGB)
	{
		bValidTexture = true;
	}

	FTextureArrayDataEntry* FoundEntry = CachedData.Find(NewTexture);
	if (!FoundEntry)
	{
		// Add a new entry for this texture
		FoundEntry = &CachedData.Add(NewTexture, FTextureArrayDataEntry());
	}

	if (bValidTexture && FoundEntry->MipData.Num() == 0)
	{
		FoundEntry->MipData = InEntry->MipData;
		bDirty = true;
	}
	
	FoundEntry->NumRefs++;

	delete InEntry;
}

/** Removes a texture from the texture array, and potentially removes the CachedData entry if the last ref was removed. */
void FTexture2DArrayResource::RemoveTexture2D(const UTexture2D* NewTexture)
{
	FTextureArrayDataEntry* FoundEntry = CachedData.Find(NewTexture);
	if (FoundEntry)
	{
		check(FoundEntry->NumRefs > 0);
		FoundEntry->NumRefs--;
		if (FoundEntry->NumRefs == 0)
		{
			CachedData.Remove(NewTexture);
			bDirty = true;
		}
	}
}

/** Updates a CachedData entry (if one exists for this texture), with a new texture. */
void FTexture2DArrayResource::UpdateTexture2D(UTexture2D* NewTexture, const FIncomingTextureArrayDataEntry* InEntry)
{
	FTextureArrayDataEntry* FoundEntry = CachedData.Find(NewTexture);
	if (FoundEntry)
	{
		const int32 OldNumRefs = FoundEntry->NumRefs;
		FoundEntry->MipData.Empty();
		bDirty = true;
		AddTexture2D(NewTexture, InEntry);
		FoundEntry->NumRefs = OldNumRefs;
	}
}

/** Initializes the texture array resource if needed, and re-initializes if the texture array has been made dirty since the last init. */
void FTexture2DArrayResource::UpdateResource()
{
	if (bDirty)
	{
		if (IsInitialized())
		{
			ReleaseResource();
		}

		if (GetNumValidTextures() > 0)
		{
			InitResource();
		}

		bDirty = false;
	}
}

/** Returns the index of a given texture in the texture array. */
int32 FTexture2DArrayResource::GetTextureIndex(const UTexture2D* Texture) const
{
	int32 TextureIndex = 0;
	for (TMap<const UTexture2D*, FTextureArrayDataEntry>::TConstIterator It(CachedData); It; ++It)
	{
		if (It.Key() == Texture && It.Value().MipData.Num() > 0)
		{
			return TextureIndex;
		}
		// Don't count invalid (empty mip data) entries toward the index
		if (It.Value().MipData.Num() > 0)
		{
			TextureIndex++;
		}
	}
	return INDEX_NONE;
}

int32 FTexture2DArrayResource::GetNumValidTextures() const
{
	int32 NumValidTextures = 0;
	for (TMap<const UTexture2D*, FTextureArrayDataEntry>::TConstIterator It(CachedData); It; ++It)
	{
		if (It.Value().MipData.Num() > 0)
		{
			NumValidTextures++;
		}
	}
	return NumValidTextures;
}

/** Prevents reallocation from removals of the texture array until EndPreventReallocation is called. */
void FTexture2DArrayResource::BeginPreventReallocation()
{
	for (TMap<const UTexture2D*, FTextureArrayDataEntry>::TIterator It(CachedData); It; ++It)
	{
		FTextureArrayDataEntry& CurrentEntry = It.Value();
		CurrentEntry.NumRefs++;
	}
	bPreventingReallocation = true;
}

/** Restores the ability to reallocate the texture array. */
void FTexture2DArrayResource::EndPreventReallocation()
{
	check(bPreventingReallocation);
	bPreventingReallocation = false;
	for (TMap<const UTexture2D*, FTextureArrayDataEntry>::TIterator It(CachedData); It; ++It)
	{
		FTextureArrayDataEntry& CurrentEntry = It.Value();
		CurrentEntry.NumRefs--;
		if (CurrentEntry.NumRefs == 0)
		{
			It.RemoveCurrent();
			bDirty = true;
		}
	}
}

/** Copies data from DataEntry into Dest, taking stride into account. */
void FTexture2DArrayResource::GetData(const FTextureArrayDataEntry& DataEntry, int32 MipIndex, void* Dest, uint32 DestPitch)
{
	check(DataEntry.MipData[MipIndex].Data.Num() > 0);

	uint32 NumRows = 0;
	uint32 SrcPitch = 0;
	uint32 BlockSizeX = GPixelFormats[Format].BlockSizeX;	// Block width in pixels
	uint32 BlockSizeY = GPixelFormats[Format].BlockSizeY;	// Block height in pixels
	uint32 BlockBytes = GPixelFormats[Format].BlockBytes;
	uint32 NumColumns = (DataEntry.MipData[MipIndex].SizeX + BlockSizeX - 1) / BlockSizeX;	// Num-of columns in the source data (in blocks)
	NumRows = (DataEntry.MipData[MipIndex].SizeY + BlockSizeY - 1) / BlockSizeY;	// Num-of rows in the source data (in blocks)
	SrcPitch = NumColumns * BlockBytes;		// Num-of bytes per row in the source data

	if (SrcPitch == DestPitch)
	{
		// Copy data, not taking into account stride!
		FMemory::Memcpy(Dest, DataEntry.MipData[MipIndex].Data.GetData(), DataEntry.MipData[MipIndex].Data.Num());
	}
	else
	{
		// Copy data, taking the stride into account!
		uint8 *Src = (uint8*)DataEntry.MipData[MipIndex].Data.GetData();
		uint8 *Dst = (uint8*)Dest;
		for (uint32 Row = 0; Row < NumRows; ++Row)
		{
			FMemory::Memcpy(Dst, Src, SrcPitch);
			Src += SrcPitch;
			Dst += DestPitch;
		}
		check((PTRINT(Src) - PTRINT(DataEntry.MipData[MipIndex].Data.GetData())) == PTRINT(DataEntry.MipData[MipIndex].Data.Num()));
	}
}
