// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SlateRHIResourceManager.h"
#include "RenderingThread.h"
#include "Engine/Texture.h"
#include "Misc/FileHelper.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/ISlateStyle.h"
#include "Rendering/SlateRenderer.h"
#include "EngineGlobals.h"
#include "Engine/Texture2D.h"
#include "RenderUtils.h"
#include "Engine/Engine.h"
#include "Slate/SlateTextures.h"
#include "SlateRHITextureAtlas.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "SlateNativeTextureResource.h"
#include "SlateUTextureResource.h"
#include "SlateMaterialResource.h"
#include "Slate/SlateTextureAtlasInterface.h"
#include "SlateAtlasedTextureResource.h"
#include "ImageUtils.h"

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Texture Atlases"), STAT_SlateNumTextureAtlases, STATGROUP_SlateMemory);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Non-Atlased Textures"), STAT_SlateNumNonAtlasedTextures, STATGROUP_SlateMemory);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Dynamic Textures"), STAT_SlateNumDynamicTextures, STATGROUP_SlateMemory);
DECLARE_CYCLE_STAT(TEXT("GetResource Time"), STAT_SlateGetResourceTime, STATGROUP_Slate);

FDynamicResourceMap::FDynamicResourceMap()
	: TextureMemorySincePurge(0)
	, LastExpiredMaterialNumMarker(0)
{

}

TSharedPtr<FSlateDynamicTextureResource> FDynamicResourceMap::GetDynamicTextureResource( FName ResourceName ) const
{
	return NativeTextureMap.FindRef( ResourceName );
}

TSharedPtr<FSlateUTextureResource> FDynamicResourceMap::GetUTextureResource( UTexture* TextureObject ) const
{
	if(TextureObject)
	{
		return TextureMap.FindRef(TextureObject);
	}

	return nullptr;
}

TSharedPtr<FSlateAtlasedTextureResource> FDynamicResourceMap::GetAtlasedTextureResource(UTexture* InObject) const
{
	if ( InObject )
	{
		return ObjectMap.FindRef(InObject);
	}

	return nullptr;
}

TSharedPtr<FSlateMaterialResource> FDynamicResourceMap::GetMaterialResource( const FMaterialKey& InKey ) const
{
	return MaterialMap.FindRef( InKey );
}


void FDynamicResourceMap::AddDynamicTextureResource( FName ResourceName, TSharedRef<FSlateDynamicTextureResource> InResource )
{
	NativeTextureMap.Add( ResourceName, InResource );
}

void FDynamicResourceMap::AddUTextureResource( UTexture* TextureObject, TSharedRef<FSlateUTextureResource> InResource)
{
	if ( TextureObject )
	{
		check(TextureObject == InResource->TextureObject);
		TextureMap.Add(TextureObject, InResource);

		TextureMemorySincePurge += TextureObject->GetResourceSizeBytes(EResourceSizeMode::Inclusive);
	}
}

void FDynamicResourceMap::AddMaterialResource( const FMaterialKey& InKey, TSharedRef<FSlateMaterialResource> InMaterialResource )
{
	check(InKey.Material == InMaterialResource->GetMaterialObject() );
	MaterialMap.Add(InKey, InMaterialResource);
}

void FDynamicResourceMap::RemoveDynamicTextureResource(FName ResourceName)
{
	NativeTextureMap.Remove(ResourceName);
}

void FDynamicResourceMap::RemoveUTextureResource( UTexture* TextureObject )
{
	if(TextureObject)
	{
		TextureMap.Remove(TextureObject);
		TextureMemorySincePurge -= TextureObject->GetResourceSizeBytes(EResourceSizeMode::Inclusive);
	}
}

void FDynamicResourceMap::RemoveMaterialResource( const FMaterialKey& InKey )
{
	MaterialMap.Remove(InKey);
}

void FDynamicResourceMap::AddAtlasedTextureResource(UTexture* TextureObject, TSharedRef<FSlateAtlasedTextureResource> InResource)
{
	if ( TextureObject )
	{
		ObjectMap.Add(TextureObject, InResource);
	}
}

void FDynamicResourceMap::RemoveAtlasedTextureResource(UTexture* TextureObject)
{
	ObjectMap.Remove(TextureObject);
}

void FDynamicResourceMap::Empty()
{
	EmptyUTextureResources();
	EmptyMaterialResources();
	EmptyDynamicTextureResources();
}

void FDynamicResourceMap::EmptyDynamicTextureResources()
{
	NativeTextureMap.Empty();
}

void FDynamicResourceMap::EmptyUTextureResources()
{
	TextureMap.Empty();
	TextureMemorySincePurge = 0;
}

void FDynamicResourceMap::EmptyMaterialResources()
{
	MaterialMap.Empty();
}

void FDynamicResourceMap::ReleaseResources()
{
	for (TMap<FName, TSharedPtr<FSlateDynamicTextureResource> >::TIterator It(NativeTextureMap); It; ++It)
	{
		BeginReleaseResource(It.Value()->RHIRefTexture);
	}
	
	for ( FTextureResourceMap::TIterator It(TextureMap); It; ++It )
	{
		It.Value()->UpdateRenderResource(nullptr);
	}
}

void FDynamicResourceMap::RemoveExpiredTextureResources(TArray< TSharedPtr<FSlateUTextureResource> >& RemovedTextures)
{
	// We attempt to purge every 10Mb of accumulated textures.
	static const uint64 PurgeAfterAddingNewBytes = 1024 * 1024 * 10; // 10Mb

	if ( TextureMemorySincePurge >= PurgeAfterAddingNewBytes )
	{
		for ( FTextureResourceMap::TIterator It(TextureMap); It; ++It )
		{
			TWeakObjectPtr<UTexture>& Key = It.Key();
			if ( !Key.IsValid() )
			{
				RemovedTextures.Push(It.Value());

				It.RemoveCurrent();
			}
		}

		TextureMemorySincePurge = 0;
	}
}

void FDynamicResourceMap::RemoveExpiredMaterialResources(TArray< TSharedPtr<FSlateMaterialResource> >& RemovedMaterials)
{
	static const int32 CheckingIncrement = 20;

	if ( MaterialMap.Num() > ( LastExpiredMaterialNumMarker + CheckingIncrement ) )
	{
		for ( FMaterialResourceMap::TIterator It(MaterialMap); It; ++It )
		{
			FMaterialKey& Key = It.Key();
			if ( !Key.Material.IsValid() )
			{
				RemovedMaterials.Push(It.Value());

				It.RemoveCurrent();
			}
		}

		LastExpiredMaterialNumMarker = MaterialMap.Num();
	}
}

FSlateRHIResourceManager::FSlateRHIResourceManager()
	: CurrentAccessedUObject(nullptr)
	, BadResourceTexture(nullptr)
{
	FCoreDelegates::OnPreExit.AddRaw( this, &FSlateRHIResourceManager::OnAppExit );

	MaxAltasedTextureSize = FIntPoint(256,256);
	if( GIsEditor )
	{
		AtlasSize = 2048;
	}
	else
	{
		AtlasSize = 1024;
		if( GConfig )
		{
			int32 RequestedSize = 1024;
			GConfig->GetInt( TEXT("SlateRenderer"), TEXT("TextureAtlasSize"), RequestedSize, GEngineIni );
			AtlasSize = FMath::Clamp<uint32>( RequestedSize, 0, 2048 );

			int32 MaxAtlasedTextureWidth = 256;
			int32 MaxAtlasedTextureHeight = 256;
			GConfig->GetInt( TEXT("SlateRenderer"), TEXT("MaxAtlasedTextureWidth"), MaxAtlasedTextureWidth, GEngineIni );
			GConfig->GetInt( TEXT("SlateRenderer"), TEXT("MaxAtlasedTextureHeight"),MaxAtlasedTextureHeight, GEngineIni );

			// Max texture size cannot be larger than the max size of the atlas
			MaxAltasedTextureSize.X = FMath::Clamp<int32>( MaxAtlasedTextureWidth, 0, AtlasSize );
			MaxAltasedTextureSize.Y = FMath::Clamp<int32>( MaxAtlasedTextureHeight, 0, AtlasSize );
		}
	}
}

FSlateRHIResourceManager::~FSlateRHIResourceManager()
{
	FCoreDelegates::OnPreExit.RemoveAll( this );

	if ( GIsRHIInitialized )
	{
		FlushRenderingCommands();

		DeleteResources();
	}
}

int32 FSlateRHIResourceManager::GetNumAtlasPages() const
{
	return TextureAtlases.Num();
}

FIntPoint FSlateRHIResourceManager::GetAtlasPageSize() const
{
	return FIntPoint(1024, 1024);
}

FSlateShaderResource* FSlateRHIResourceManager::GetAtlasPageResource(const int32 InIndex) const
{
	return TextureAtlases[InIndex]->GetAtlasTexture();
}

bool FSlateRHIResourceManager::IsAtlasPageResourceAlphaOnly() const
{
	return false;
}

void FSlateRHIResourceManager::Tick(float DeltaSeconds)
{
	// Don't need to do this if there's no RHI thread.
	if (IsRunningRHIInSeparateThread())
	{
		struct FDeleteCachedRenderDataContext
		{
			FSlateRHIResourceManager* ResourceManager;
		};
		FDeleteCachedRenderDataContext DeleteCachedRenderDataContext =
		{
			this,
		};
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			DeleteCachedRenderData,
			FDeleteCachedRenderDataContext, Context, DeleteCachedRenderDataContext,
			{
				// Go through the pending delete buffers and see if any of their fences has cleared
				// the RHI thread, if so, they should be safe to delete now.
				for ( int32 BufferIndex = Context.ResourceManager->PooledBuffersPendingRelease.Num() - 1; BufferIndex >= 0; BufferIndex-- )
				{
					FCachedRenderBuffers* PooledBuffer = Context.ResourceManager->PooledBuffersPendingRelease[BufferIndex];

					if ( PooledBuffer->ReleaseResourcesFence->IsComplete() )
					{
						PooledBuffer->VertexBuffer.Destroy();
						PooledBuffer->IndexBuffer.Destroy();
						delete PooledBuffer;

						Context.ResourceManager->PooledBuffersPendingRelease.RemoveAt(BufferIndex);
					}
				}
			});
	}
}

void FSlateRHIResourceManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	for ( TSet<UObject*>* AccessedUObjects : AllAccessedUObject )
	{
		Collector.AddReferencedObjects(*AccessedUObjects);
	}
}

void FSlateRHIResourceManager::CreateTextures( const TArray< const FSlateBrush* >& Resources )
{
	TMap<FName,FNewTextureInfo> TextureInfoMap;

	const uint32 Stride = GPixelFormats[PF_R8G8B8A8].BlockBytes;
	for( int32 ResourceIndex = 0; ResourceIndex < Resources.Num(); ++ResourceIndex )
	{
		const FSlateBrush& Brush = *Resources[ResourceIndex];
		const FName TextureName = Brush.GetResourceName();
		if( TextureName != NAME_None && !Brush.HasUObject() && !Brush.IsDynamicallyLoaded() && !ResourceMap.Contains(TextureName) )
		{
			// Find the texture or add it if it doesnt exist (only load the texture once)
			FNewTextureInfo& Info = TextureInfoMap.FindOrAdd( TextureName );
	
			Info.bSrgb = (Brush.ImageType != ESlateBrushImageType::Linear);

			// Only atlas the texture if none of the brushes that use it tile it and the image is srgb
		
			Info.bShouldAtlas &= ( Brush.Tiling == ESlateBrushTileType::NoTile && Info.bSrgb && AtlasSize > 0 );

			// Texture has been loaded if the texture data is valid
			if( !Info.TextureData.IsValid() )
			{
				uint32 Width = 0;
				uint32 Height = 0;
				TArray<uint8> RawData;
				bool bSucceeded = LoadTexture( Brush, Width, Height, RawData );

				Info.TextureData = MakeShareable( new FSlateTextureData( Width, Height, Stride, RawData ) );

				const bool bTooLargeForAtlas = (Width >= (uint32)MaxAltasedTextureSize.X || Height >= (uint32)MaxAltasedTextureSize.Y || Width >= AtlasSize || Height >= AtlasSize );

				Info.bShouldAtlas &= !bTooLargeForAtlas;

				if( !bSucceeded || !ensureMsgf( Info.TextureData->GetRawBytes().Num() > 0, TEXT("Slate resource: (%s) contains no data"), *TextureName.ToString() ) )
				{
					TextureInfoMap.Remove( TextureName );
				}
			}
		}
	}

	// Sort textures by size.  The largest textures are atlased first which creates a more compact atlas
	TextureInfoMap.ValueSort( FCompareFNewTextureInfoByTextureSize() );

	for( TMap<FName,FNewTextureInfo>::TConstIterator It(TextureInfoMap); It; ++It )
	{
		const FNewTextureInfo& Info = It.Value();
		FName TextureName = It.Key();
		FString NameStr = TextureName.ToString();

		checkSlow( TextureName != NAME_None );

		FSlateShaderResourceProxy* NewTexture = GenerateTextureResource( Info );

		ResourceMap.Add( TextureName, NewTexture );
	}
}

bool FSlateRHIResourceManager::LoadTexture( const FSlateBrush& InBrush, uint32& Width, uint32& Height, TArray<uint8>& DecodedImage )
{
	FString ResourcePath = GetResourcePath( InBrush );

	return LoadTexture(InBrush.GetResourceName(), ResourcePath, Width, Height, DecodedImage);
}

/** 
 * Loads a UTexture2D from a package and stores it in the cache
 *
 * @param TextureName	The name of the texture to load
 */
bool FSlateRHIResourceManager::LoadTexture( const FName& TextureName, const FString& ResourcePath, uint32& Width, uint32& Height, TArray<uint8>& DecodedImage )
{
	checkSlow( IsThreadSafeForSlateRendering() );

	bool bSucceeded = true;
	uint32 BytesPerPixel = 4;

	TArray<uint8> RawFileData;
	if( FFileHelper::LoadFileToArray( RawFileData, *ResourcePath ) )
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );
		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper( EImageFormat::PNG );
		if ( ImageWrapper.IsValid() && ImageWrapper->SetCompressed( RawFileData.GetData(), RawFileData.Num() ) )
		{
			Width = ImageWrapper->GetWidth();
			Height = ImageWrapper->GetHeight();
			
			const TArray<uint8>* RawData = NULL;
			if (ImageWrapper->GetRaw( ERGBFormat::BGRA, 8, RawData))
			{
				DecodedImage.AddUninitialized( Width*Height*BytesPerPixel );
				DecodedImage = *RawData;
			}
			else
			{
				UE_LOG(LogSlate, Log, TEXT("Invalid texture format for Slate resource only RGBA and RGB pngs are supported: %s"), *TextureName.ToString() );
				bSucceeded = false;
			}
		}
		else
		{
			UE_LOG(LogSlate, Log, TEXT("Only pngs are supported in Slate"));
			bSucceeded = false;
		}
	}
	else
	{
		UE_LOG(LogSlate, Log, TEXT("Could not find file for Slate resource: %s"), *TextureName.ToString() );
		bSucceeded = false;
	}

	return bSucceeded;
}

FSlateShaderResourceProxy* FSlateRHIResourceManager::GenerateTextureResource( const FNewTextureInfo& Info )
{
	FSlateShaderResourceProxy* NewProxy = NULL;
	const uint32 Width = Info.TextureData->GetWidth();
	const uint32 Height = Info.TextureData->GetHeight();

	if( Info.bShouldAtlas )
	{
		const FAtlasedTextureSlot* NewSlot = NULL;
		FSlateTextureAtlasRHI* Atlas = NULL;

		// See if any atlases can hold the texture
		for( int32 AtlasIndex = 0; AtlasIndex < TextureAtlases.Num() && !NewSlot; ++AtlasIndex )
		{
			Atlas = TextureAtlases[AtlasIndex];
			NewSlot = Atlas->AddTexture( Width, Height, Info.TextureData->GetRawBytes() );
		}

		if( !NewSlot )
		{
			INC_DWORD_STAT_BY(STAT_SlateNumTextureAtlases, 1);

			Atlas = new FSlateTextureAtlasRHI( AtlasSize, AtlasSize, ESlateTextureAtlasPaddingStyle::DilateBorder );
			TextureAtlases.Add( Atlas );
			NewSlot = TextureAtlases.Last()->AddTexture( Width, Height, Info.TextureData->GetRawBytes() );
		}
		
		check( Atlas && NewSlot );

		// Create a proxy to the atlased texture. The texture being used is the atlas itself with sub uvs to access the correct texture
		NewProxy = new FSlateShaderResourceProxy;
		NewProxy->Resource = Atlas->GetAtlasTexture();
		const uint32 Padding = NewSlot->Padding;
		NewProxy->StartUV = FVector2D((float)(NewSlot->X + Padding) / Atlas->GetWidth(), (float)(NewSlot->Y + Padding) / Atlas->GetHeight());
		NewProxy->SizeUV = FVector2D( (float)(NewSlot->Width-Padding*2) / Atlas->GetWidth(), (float)(NewSlot->Height-Padding*2) / Atlas->GetHeight() );
		NewProxy->ActualSize = FIntPoint( Width, Height );
	}
	else
	{
		NewProxy = new FSlateShaderResourceProxy;

		// Create a new standalone texture because we can't atlas this one
		FSlateTexture2DRHIRef* Texture = new FSlateTexture2DRHIRef( Width, Height, PF_B8G8R8A8, Info.TextureData, Info.bSrgb ? TexCreate_SRGB : TexCreate_None );
		// Add it to the list of non atlased textures that we must clean up later
		NonAtlasedTextures.Add( Texture );

		INC_DWORD_STAT_BY( STAT_SlateNumNonAtlasedTextures, 1 );

		BeginInitResource( Texture );

		// The texture proxy only contains a single texture
		NewProxy->Resource = Texture;
		NewProxy->StartUV = FVector2D(0.0f, 0.0f);
		NewProxy->SizeUV = FVector2D(1.0f, 1.0f);
		NewProxy->ActualSize = FIntPoint( Width, Height );
	}

	return NewProxy;
}

static void LoadUObjectForBrush( const FSlateBrush& InBrush )
{
	// Load the utexture
	FString Path = InBrush.GetResourceName().ToString();

	if (!Path.IsEmpty() && Path.StartsWith(FSlateBrush::UTextureIdentifier()))
	{
		FString NewPath = Path.RightChop(FSlateBrush::UTextureIdentifier().Len());
		UObject* TextureObject = LoadObject<UTexture2D>(NULL, *NewPath, NULL, LOAD_None, NULL);
		FSlateBrush* Brush = const_cast<FSlateBrush*>(&InBrush);

		// Set the texture object to a default texture to prevent constant loading of missing textures
		if( !TextureObject )
		{
			UE_LOG(LogSlate, Warning, TEXT("Error loading loading UTexture from path: %s not found"), *Path);
			TextureObject = GEngine->DefaultTexture;
		}
		else
		{
			// We do this here because this deprecated system of loading textures will not report references and we dont want the Slate RHI resource manager to manage references
			TextureObject->AddToRoot();
		}

		
		Brush->SetResourceObject(TextureObject);

		UE_LOG(LogSlate, Warning, TEXT("The texture:// method of loading UTextures for use in Slate is deprecated.  Please convert %s to a Brush Asset"), *Path);
	}
}

FSlateShaderResourceProxy* FSlateRHIResourceManager::GetShaderResource( const FSlateBrush& InBrush )
{
	SCOPE_CYCLE_COUNTER( STAT_SlateGetResourceTime );

	checkSlow( IsThreadSafeForSlateRendering() );

	UObject* ResourceObject = InBrush.GetResourceObject();
	FSlateShaderResourceProxy* Resource = nullptr;
	if(ResourceObject != nullptr && (ResourceObject->IsPendingKill() || ResourceObject->IsUnreachable() || ResourceObject->HasAnyFlags(RF_BeginDestroyed)))
	{
		UE_LOG(LogSlate, Warning, TEXT("Attempted to access resource for %s which is pending kill, unreachable or pending destroy"), *ResourceObject->GetName());
	}
	else
	{
		if(!InBrush.IsDynamicallyLoaded() && !InBrush.HasUObject())
		{
			Resource = ResourceMap.FindRef(InBrush.GetResourceName());
		}
		else if(ResourceObject && ResourceObject->IsA<UMaterialInterface>())
		{
			FSlateMaterialResource* MaterialResource = GetMaterialResource(ResourceObject, InBrush.ImageSize, nullptr, 0);
			Resource = MaterialResource->SlateProxy;
		}
		else if(InBrush.IsDynamicallyLoaded() || (InBrush.HasUObject()))
		{
			if(InBrush.HasUObject() && ResourceObject == nullptr)
			{
				// Hack for loading via the deprecated path
				LoadUObjectForBrush(InBrush);
			}

			Resource = FindOrCreateDynamicTextureResource(InBrush);
		}
	}

	return Resource;
}

FSlateShaderResource* FSlateRHIResourceManager::GetFontShaderResource( int32 InTextureAtlasIndex, FSlateShaderResource* FontTextureAtlas, const class UObject* FontMaterial )
{
	if( FontMaterial == nullptr )
	{
		return FontTextureAtlas;
	}
	else
	{
		return GetMaterialResource( FontMaterial, FVector2D::ZeroVector, FontTextureAtlas, InTextureAtlasIndex );
	}
}

ISlateAtlasProvider* FSlateRHIResourceManager::GetTextureAtlasProvider()
{
	return this;
}

TSharedPtr<FSlateDynamicTextureResource> FSlateRHIResourceManager::MakeDynamicTextureResource(FName ResourceName, uint32 Width, uint32 Height, const TArray< uint8 >& Bytes)
{
	// Make storage for the image
	FSlateTextureDataRef TextureStorage = MakeShareable(new FSlateTextureData(Width, Height, GPixelFormats[PF_B8G8R8A8].BlockBytes, Bytes));
	return MakeDynamicTextureResource(ResourceName, TextureStorage);
}

TSharedPtr<FSlateDynamicTextureResource> FSlateRHIResourceManager::MakeDynamicTextureResource(FName ResourceName, FSlateTextureDataRef TextureData)
{
	TSharedPtr<FSlateDynamicTextureResource> TextureResource;
	// Get a resource from the free list if possible
	if(DynamicTextureFreeList.Num() > 0)
	{
		TextureResource = DynamicTextureFreeList.Pop(/*bAllowShrinking=*/ false);
	}
	else
	{
		// Free list is empty, we have to allocate a new resource
		TextureResource = MakeShareable(new FSlateDynamicTextureResource(nullptr));
	}

	TextureResource->Proxy->ActualSize = FIntPoint(TextureData->GetWidth(), TextureData->GetHeight());


	// Init render thread data
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(InitNewSlateDynamicTextureResource,
		FSlateDynamicTextureResource*, TextureResource, TextureResource.Get(),
		FSlateTextureDataPtr, InNewTextureData, TextureData,
	{
		if(InNewTextureData.IsValid())
		{
			// Set the texture to use as the texture we just loaded
			TextureResource->RHIRefTexture->SetTextureData(InNewTextureData, PF_B8G8R8A8, TexCreate_SRGB);
		}

		// Initialize and link the rendering resource
		TextureResource->RHIRefTexture->InitResource();
	});

	// Map the new resource so we don't have to load again
	DynamicResourceMap.AddDynamicTextureResource( ResourceName, TextureResource.ToSharedRef() );
	INC_DWORD_STAT_BY(STAT_SlateNumDynamicTextures, 1);

	return TextureResource;
}

TSharedPtr<FSlateDynamicTextureResource> FSlateRHIResourceManager::GetDynamicTextureResourceByName( FName ResourceName )
{
	return DynamicResourceMap.GetDynamicTextureResource( ResourceName );
}

TSharedPtr<FSlateUTextureResource> FSlateRHIResourceManager::MakeDynamicUTextureResource(UTexture* InTextureObject)
{
	// Generated texture resource
	TSharedPtr<FSlateUTextureResource> TextureResource;

	// Data for a loaded disk image
	FNewTextureInfo Info;

	bool bUsingDeprecatedUTexturePath = false;

	bool bSucceeded = false;
	if( InTextureObject != NULL )
	{
		TextureResource = DynamicResourceMap.GetUTextureResource( InTextureObject );
		if( TextureResource.IsValid() )
		{
			// Bail out of the resource is already loaded
			return TextureResource;
		}

		bSucceeded = true;
	}
	
	if( bSucceeded )
	{
		// Get a resource from the free list if possible
		if (UTextureFreeList.Num() > 0)
		{
			TextureResource = UTextureFreeList.Pop(/*bAllowShrinking=*/ false);
			TextureResource->TextureObject = InTextureObject;
		}
		else
		{
			// Free list is empty, we have to allocate a new resource
			TextureResource = MakeShareable(new FSlateUTextureResource(InTextureObject));
		}

		TextureResource->Proxy->ActualSize = FIntPoint(InTextureObject->GetSurfaceWidth(), InTextureObject->GetSurfaceHeight());

		checkSlow(!GetAccessedUObjects().Contains(InTextureObject));
	}
	else
	{
		// Add the null texture so we don't continuously try to load it.
		TextureResource = FSlateUTextureResource::NullResource;
	}

	DynamicResourceMap.AddUTextureResource(InTextureObject, TextureResource.ToSharedRef());

	return TextureResource;
}


FSlateShaderResourceProxy* FSlateRHIResourceManager::FindOrCreateDynamicTextureResource(const FSlateBrush& InBrush)
{
	checkSlow( IsThreadSafeForSlateRendering() );

	const FName ResourceName = InBrush.GetResourceName();
	if ( ResourceName.IsValid() && ResourceName != NAME_None )
	{
		if ( UObject* ResourceObject = InBrush.GetResourceObject() )
		{
			if ( UTexture* TextureObject = Cast<UTexture>(ResourceObject) )
			{
				TSharedPtr<FSlateUTextureResource> TextureResource = DynamicResourceMap.GetUTextureResource(TextureObject);

				if ( !TextureResource.IsValid() )
				{
					TextureResource = MakeDynamicUTextureResource(TextureObject);
					if ( TextureResource.IsValid() )
					{
						INC_DWORD_STAT_BY(STAT_SlateNumDynamicTextures, 1);
					}
				}

				if ( TextureResource.IsValid() && TextureResource->TextureObject && TextureResource->TextureObject->Resource )
				{
					TextureResource->UpdateRenderResource(TextureObject->Resource);
					GetAccessedUObjects().Add(TextureResource->TextureObject);
					return TextureResource->Proxy;
				}
			}
			else if ( ISlateTextureAtlasInterface* AtlasedTextureObject = Cast<ISlateTextureAtlasInterface>(ResourceObject) )
			{
				const FSlateAtlasData& AtlasData = AtlasedTextureObject->GetSlateAtlasData();
				if ( AtlasData.AtlasTexture )
				{
					TSharedPtr<FSlateAtlasedTextureResource> AtlasResource = DynamicResourceMap.GetAtlasedTextureResource(AtlasData.AtlasTexture);

					if ( !AtlasResource.IsValid() )
					{
						AtlasResource = MakeShareable(new FSlateAtlasedTextureResource(AtlasData.AtlasTexture));
						DynamicResourceMap.AddAtlasedTextureResource(AtlasData.AtlasTexture, AtlasResource.ToSharedRef());
					}

					FSlateShaderResourceProxy* AtlasedProxy = AtlasResource->FindOrCreateAtlasedProxy(ResourceObject, AtlasData);

					GetAccessedUObjects().Add(ResourceObject);

					return AtlasedProxy;
				}

				return nullptr;
			}
			else
			{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				static TSet<UObject*> FailedTextures;
				if ( !FailedTextures.Contains(ResourceObject) )
				{
					FailedTextures.Add(ResourceObject);
					ensureMsgf(false, TEXT("Slate RHI Error - Invalid Texture2D '%s'."), *ResourceName.ToString());
				}
				ResourceObject = GetBadResourceTexture();
#else
				return nullptr;
#endif
			}
		}
		else
		{
			TSharedPtr<FSlateDynamicTextureResource> TextureResource = DynamicResourceMap.GetDynamicTextureResource( ResourceName );

			if( !TextureResource.IsValid() )
			{
				uint32 Width; 
				uint32 Height;
				TArray<uint8> RawData;

				// Load the image from disk
				bool bSucceeded = LoadTexture(ResourceName, ResourceName.ToString(), Width, Height, RawData);
				if(bSucceeded)
				{
					TextureResource = MakeDynamicTextureResource(ResourceName, Width, Height, RawData);
				}
			}

			if(TextureResource.IsValid())
			{
				return TextureResource->Proxy;
			}
		}
	}

	// dynamic texture was not found or loaded
	return nullptr;
}

FSlateMaterialResource* FSlateRHIResourceManager::GetMaterialResource(const UObject* InMaterial, FVector2D ImageSize, FSlateShaderResource* TextureMask, int32 InMaskKey )
{
	checkSlow(IsThreadSafeForSlateRendering());

	const UMaterialInterface* Material = CastChecked<UMaterialInterface>(InMaterial);

	FMaterialKey Key(Material, InMaskKey);

	TSharedPtr<FSlateMaterialResource> MaterialResource = DynamicResourceMap.GetMaterialResource(Key);
	if (!MaterialResource.IsValid())
	{
		// Get a resource from the free list if possible
		if(MaterialResourceFreeList.Num() > 0)
		{
			MaterialResource = MaterialResourceFreeList.Pop();
		
			MaterialResource->UpdateMaterial( *Material, ImageSize, TextureMask );
		}
		else
		{
			MaterialResource = MakeShareable(new FSlateMaterialResource(*Material, ImageSize, TextureMask));
		}
		
		DynamicResourceMap.AddMaterialResource(Key, MaterialResource.ToSharedRef());
	}
	else
	{
		MaterialResource->UpdateMaterial( *Material, ImageSize, TextureMask );
	}

	GetAccessedUObjects().Add(const_cast<UMaterialInterface*>( Material ));

	return MaterialResource.Get();
}

void FSlateRHIResourceManager::OnAppExit()
{
	FlushRenderingCommands();

	ReleaseResources();

	FlushRenderingCommands();

	DeleteResources();
}

bool FSlateRHIResourceManager::ContainsTexture( const FName& ResourceName ) const
{
	return ResourceMap.Contains( ResourceName );
}

void FSlateRHIResourceManager::ReleaseDynamicResource( const FSlateBrush& InBrush )
{
	checkSlow( IsThreadSafeForSlateRendering() )

	// Note: Only dynamically loaded or utexture brushes can be dynamically released
	if( InBrush.HasUObject() || InBrush.IsDynamicallyLoaded() )
	{
		FName ResourceName = InBrush.GetResourceName();

		UObject* ResourceObject = InBrush.GetResourceObject();

		if( ResourceObject && DynamicResourceMap.GetNumObjectResources() > 0 )
		{
			TSharedPtr<FSlateUTextureResource> TextureResource = DynamicResourceMap.GetUTextureResource(Cast<UTexture>(ResourceObject));

			if(TextureResource.IsValid())
			{
				//remove it from the accessed textures
				GetAccessedUObjects().Remove(TextureResource->TextureObject);
				DynamicResourceMap.RemoveUTextureResource(TextureResource->TextureObject);

				UTextureFreeList.Add(TextureResource);

				DEC_DWORD_STAT_BY(STAT_SlateNumDynamicTextures, 1);
			}
			else
			{
				UMaterialInterface* Material = Cast<UMaterialInterface>(ResourceObject);

				FMaterialKey Key(Material, 0);

				TSharedPtr<FSlateMaterialResource> MaterialResource = DynamicResourceMap.GetMaterialResource(Key);
				
				DynamicResourceMap.RemoveMaterialResource(Key);

				if (MaterialResource.IsValid())
				{
					MaterialResource->ResetMaterial();
					MaterialResourceFreeList.Add( MaterialResource );
				}
			}
		}
		else if( !ResourceObject )
		{
			TSharedPtr<FSlateDynamicTextureResource> TextureResource = DynamicResourceMap.GetDynamicTextureResource(ResourceName);

			if( TextureResource.IsValid() )
			{
				// Release the rendering resource, its no longer being used
				BeginReleaseResource(TextureResource->RHIRefTexture);

				//remove it from the texture map
				DynamicResourceMap.RemoveDynamicTextureResource(ResourceName);

				DynamicTextureFreeList.Add( TextureResource );

				DEC_DWORD_STAT_BY(STAT_SlateNumDynamicTextures, 1);
			}
		}
	}
}

void FSlateRHIResourceManager::LoadUsedTextures()
{
	TArray< const FSlateBrush* > Resources;
	FSlateStyleRegistry::GetAllResources( Resources );
	 
	CreateTextures( Resources );
}

void FSlateRHIResourceManager::LoadStyleResources( const ISlateStyle& Style )
{
	TArray< const FSlateBrush* > Resources;
	Style.GetResources( Resources );

	CreateTextures( Resources );
}

void FSlateRHIResourceManager::BeginReleasingAccessedResources(bool bImmediatelyFlush)
{
	// IsInGameThread returns true when you're in the slate loading thread
	if ( IsInGameThread() && !IsInSlateThread() )
	{
		DynamicResourceMap.RemoveExpiredTextureResources(UTextureFreeList);
		DynamicResourceMap.RemoveExpiredMaterialResources(MaterialResourceFreeList);

		if ( CurrentAccessedUObject )
		{
			DirtyAccessedObjectSets.Enqueue(CurrentAccessedUObject);

			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(UpdateSlateUTextureResource,
				FSlateRHIResourceManager*, InManager, this,
				{
					TSet<UObject*>* Objects = nullptr;
					InManager->DirtyAccessedObjectSets.Dequeue(Objects);
					InManager->CleanAccessedObjectSets.Enqueue(Objects);
				});

			CurrentAccessedUObject = nullptr;
		}

		if ( bImmediatelyFlush )
		{
			// Release all accessed object sets, we only manipulate the set on the main
			// thread, so this is fine.
			for ( TSet<UObject*>* AccessedUObjects : AllAccessedUObject )
			{
				AccessedUObjects->Empty();
			}
		}
	}
}

TSet<UObject*>& FSlateRHIResourceManager::GetAccessedUObjects()
{
	// If the current CurrentAccessedUObject is nullptr, that means we need a fresh
	// one from the clean queue, or we need to create one.
	if ( CurrentAccessedUObject == nullptr )
	{
		if ( CleanAccessedObjectSets.Dequeue(CurrentAccessedUObject) )
		{
			CurrentAccessedUObject->Empty();
		}
		else
		{
			AllAccessedUObject.Add(new TSet<UObject*>());
			CurrentAccessedUObject = AllAccessedUObject.Last();
		}
	}

	return *CurrentAccessedUObject;
}

void FSlateRHIResourceManager::UpdateTextureAtlases()
{
	for( int32 AtlasIndex = 0; AtlasIndex < TextureAtlases.Num(); ++AtlasIndex )
	{
		TextureAtlases[AtlasIndex]->ConditionalUpdateTexture();
	}
}

FCachedRenderBuffers* FSlateRHIResourceManager::FindOrCreateCachedBuffersForHandle(const TSharedRef<FSlateRenderDataHandle, ESPMode::ThreadSafe>& RenderHandle)
{
	// Should only be called by the rendering thread
	check(IsInRenderingThread());

	FCachedRenderBuffers* Buffers = CachedBuffers.FindRef(&RenderHandle.Get());
	if ( Buffers == nullptr )
	{
		// Rather than having a global pool, we associate the pools with a particular layout cacher.
		// If we don't do this, all buffers eventually become as larger as the largest buffer, and it
		// would be much better to keep the pools coherent with the sizes typically associated with
		// a particular caching panel.
		const ILayoutCache* LayoutCacher = RenderHandle->GetCacher();
		TArray< FCachedRenderBuffers* >& Pool = CachedBufferPool.FindOrAdd(LayoutCacher);

		// If the cached buffer pool is empty, time to create a new one!
		if ( Pool.Num() == 0 )
		{
			Buffers = new FCachedRenderBuffers();
			Buffers->VertexBuffer.Init(100);
			Buffers->IndexBuffer.Init(100);
		}
		else
		{
			// If we found one in the pool, lets use it!
			Buffers = Pool[0];
			Pool.RemoveAtSwap(0, 1, false);
		}

		CachedBuffers.Add(&RenderHandle.Get(), Buffers);
	}

	return Buffers;
}

void FSlateRHIResourceManager::BeginReleasingRenderData(const FSlateRenderDataHandle* RenderHandle)
{
	struct FReleaseCachedRenderDataContext
	{
		FSlateRHIResourceManager* ResourceManager;
		const FSlateRenderDataHandle* RenderDataHandle;
		const ILayoutCache* LayoutCacher;
	};
	FReleaseCachedRenderDataContext ReleaseCachedRenderDataContext =
	{
		this,
		RenderHandle,
		RenderHandle->GetCacher()
	};
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ReleaseCachedRenderData,
		FReleaseCachedRenderDataContext, Context, ReleaseCachedRenderDataContext,
		{
			Context.ResourceManager->ReleaseCachedRenderData(RHICmdList, Context.RenderDataHandle, Context.LayoutCacher);
		});
}

void FSlateRHIResourceManager::ReleaseCachedRenderData(FRHICommandListImmediate& RHICmdList, const FSlateRenderDataHandle* RenderHandle, const ILayoutCache* LayoutCacher)
{
	check(IsInRenderingThread());
	check(RenderHandle);

	FCachedRenderBuffers* PooledBuffer = CachedBuffers.FindRef(RenderHandle);
	if ( ensure(PooledBuffer != nullptr) )
	{
		TArray< FCachedRenderBuffers* >* Pool = CachedBufferPool.Find(LayoutCacher);
		if ( Pool )
		{
			Pool->Add(PooledBuffer);
		}
		else
		{
			ReleaseCachedBuffer(RHICmdList, PooledBuffer);
		}

		CachedBuffers.Remove(RenderHandle);
	}
}

void FSlateRHIResourceManager::ReleaseCachingResourcesFor(FRHICommandListImmediate& RHICmdList, const ILayoutCache* Cacher)
{
	check(IsInRenderingThread());

	TArray< FCachedRenderBuffers* >* Pool = CachedBufferPool.Find(Cacher);
	if ( Pool )
	{
		for ( FCachedRenderBuffers* PooledBuffer : *Pool )
		{
			ReleaseCachedBuffer(RHICmdList, PooledBuffer);
		}

		CachedBufferPool.Remove(Cacher);
	}
}

void FSlateRHIResourceManager::ReleaseCachedBuffer(FRHICommandListImmediate& RHICmdList, FCachedRenderBuffers* PooledBuffer)
{
	check(IsInRenderingThread());

	if (IsRunningRHIInSeparateThread())
	{
		PooledBuffersPendingRelease.Add(PooledBuffer);
		PooledBuffer->ReleaseResourcesFence = RHICmdList.RHIThreadFence();
	}
	else
	{
		PooledBuffer->VertexBuffer.Destroy();
		PooledBuffer->IndexBuffer.Destroy();
		delete PooledBuffer;
	}
}

void FSlateRHIResourceManager::ReleaseResources()
{
	checkSlow( IsThreadSafeForSlateRendering() );

	for( int32 AtlasIndex = 0; AtlasIndex < TextureAtlases.Num(); ++AtlasIndex )
	{
		TextureAtlases[AtlasIndex]->ReleaseAtlasTexture();
	}

	for( int32 ResourceIndex = 0; ResourceIndex < NonAtlasedTextures.Num(); ++ResourceIndex )
	{
		BeginReleaseResource( NonAtlasedTextures[ResourceIndex] );
	}

	DynamicResourceMap.ReleaseResources();

	for ( TCachedBufferMap::TIterator BufferIt(CachedBuffers); BufferIt; ++BufferIt )
	{
		FSlateRenderDataHandle* Handle = BufferIt.Key();
		FCachedRenderBuffers* Buffer = BufferIt.Value();

		Handle->Disconnect();

		Buffer->VertexBuffer.Destroy();
		Buffer->IndexBuffer.Destroy();
	}

	for ( TCachedBufferPoolMap::TIterator BufferIt(CachedBufferPool); BufferIt; ++BufferIt )
	{
		TArray< FCachedRenderBuffers* >& Pool = BufferIt.Value();
		for ( FCachedRenderBuffers* PooledBuffer : Pool )
		{
			PooledBuffer->VertexBuffer.Destroy();
			PooledBuffer->IndexBuffer.Destroy();
		}
	}

	// Note the base class has texture proxies only which do not need to be released
}

void FSlateRHIResourceManager::DeleteResources()
{
	for( int32 AtlasIndex = 0; AtlasIndex < TextureAtlases.Num(); ++AtlasIndex )
	{
		delete TextureAtlases[AtlasIndex];
	}

	for( int32 ResourceIndex = 0; ResourceIndex < NonAtlasedTextures.Num(); ++ResourceIndex )
	{
		delete NonAtlasedTextures[ResourceIndex];
	}

	SET_DWORD_STAT(STAT_SlateNumNonAtlasedTextures, 0);
	SET_DWORD_STAT(STAT_SlateNumTextureAtlases, 0);
	SET_DWORD_STAT(STAT_SlateNumDynamicTextures, 0);

	// Verify rendering commands were flushed by ensuring there's nothing left to be processed
	// in the dirty queue, they should all be in clean.
	check(DirtyAccessedObjectSets.IsEmpty());

	// Remove everything from the clean set.
	TSet<UObject*>* DummyObjects;
	while ( CleanAccessedObjectSets.Dequeue(DummyObjects) ) { }

	// Release all accessed object sets.
	for ( TSet<UObject*>* AccessedUObjects : AllAccessedUObject )
	{
		AccessedUObjects->Empty();
		delete AccessedUObjects;
	}

	AllAccessedUObject.Empty();

	DynamicResourceMap.Empty();
	TextureAtlases.Empty();
	NonAtlasedTextures.Empty();
	DynamicTextureFreeList.Empty();
	MaterialResourceFreeList.Empty();
	UTextureFreeList.Empty();

	// Clean up mapping to texture
	ClearTextureMap();

	for ( TCachedBufferMap::TIterator BufferIt(CachedBuffers); BufferIt; ++BufferIt )
	{
		FCachedRenderBuffers* Buffer = BufferIt.Value();
		delete Buffer;
	}

	CachedBuffers.Empty();

	for ( TCachedBufferPoolMap::TIterator BufferIt(CachedBufferPool); BufferIt; ++BufferIt )
	{
		TArray< FCachedRenderBuffers* >& Pool = BufferIt.Value();
		for ( FCachedRenderBuffers* PooledBuffer : Pool )
		{
			delete PooledBuffer;
		}
	}

	CachedBufferPool.Empty();
}

void FSlateRHIResourceManager::ReloadTextures()
{
	checkSlow( IsThreadSafeForSlateRendering() );

	// Release rendering resources
	ReleaseResources();

	// wait for all rendering resources to be released
	FlushRenderingCommands();

	// Delete allocated resources (cpu)
	DeleteResources();

	// Reload everythng
	LoadUsedTextures();
}

UTexture* FSlateRHIResourceManager::GetBadResourceTexture()
{
	if ( BadResourceTexture == nullptr )
	{
		BadResourceTexture = FImageUtils::CreateCheckerboardTexture(FColor(255, 0, 255), FColor(255, 255, 0));
		BadResourceTexture->AddToRoot();
	}

	return BadResourceTexture;
}


int32 FSlateRHIResourceManager::GetSceneCount()
{
	checkSlow(IsInRenderingThread());
	return ActiveScenes.Num();
}

FSceneInterface* FSlateRHIResourceManager::GetSceneAt(int32 Index)
{
	checkSlow(IsInRenderingThread());
	return ActiveScenes[Index];
}

void FSlateRHIResourceManager::AddSceneAt(FSceneInterface* Scene, int32 Index)
{
	checkSlow(IsInRenderingThread());
	if (ActiveScenes.Num() <= Index)
	{
		ActiveScenes.SetNumZeroed(Index + 1);
	}
	ActiveScenes[Index] = Scene;
}

void FSlateRHIResourceManager::ClearScenes()
{
	checkSlow(IsInRenderingThread());
	ActiveScenes.Empty();
}
