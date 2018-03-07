// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Windows/D3D/SlateD3DTextureManager.h"
#include "Windows/D3D/SlateD3DRenderer.h"

#include "StandaloneRendererPrivate.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"


DEFINE_LOG_CATEGORY_STATIC(LogSlateD3D, Log, All);


FSlateD3DTextureManager::FDynamicTextureResource::FDynamicTextureResource( FSlateD3DTexture* ExistingTexture )
	: Proxy( new FSlateShaderResourceProxy )
	, D3DTexture( ExistingTexture  )
{
}

FSlateD3DTextureManager::FDynamicTextureResource::~FDynamicTextureResource()
{
	if( Proxy )
	{
		delete Proxy;
	}

	if( D3DTexture )
	{
		delete D3DTexture;
	}
}

FSlateD3DTextureManager::FSlateD3DTextureManager()
{
	
}

FSlateD3DTextureManager::~FSlateD3DTextureManager()
{
	for( int32 AtlasIndex = 0; AtlasIndex < TextureAtlases.Num(); ++AtlasIndex )
	{
		delete TextureAtlases[AtlasIndex];
	}

	for( int32 ResourceIndex = 0; ResourceIndex < NonAtlasedTextures.Num(); ++ResourceIndex )
	{
		delete NonAtlasedTextures[ResourceIndex];
	}
}

int32 FSlateD3DTextureManager::GetNumAtlasPages() const
{
	return TextureAtlases.Num();
}

FIntPoint FSlateD3DTextureManager::GetAtlasPageSize() const
{
	return FIntPoint(1024, 1024);
}

FSlateShaderResource* FSlateD3DTextureManager::GetAtlasPageResource(const int32 InIndex) const
{
	return TextureAtlases[InIndex]->GetAtlasTexture();
}

bool FSlateD3DTextureManager::IsAtlasPageResourceAlphaOnly() const
{
	return false;
}

void FSlateD3DTextureManager::LoadUsedTextures()
{
	TArray< const FSlateBrush* > Resources;
	FSlateStyleRegistry::GetAllResources( Resources );

	CreateTextures( Resources );

	for( int32 AtlasIndex = 0; AtlasIndex < TextureAtlases.Num(); ++AtlasIndex )
	{
		TextureAtlases[AtlasIndex]->InitAtlasTexture( AtlasIndex );
	}
}

void FSlateD3DTextureManager::LoadStyleResources( const ISlateStyle& Style )
{
	TArray< const FSlateBrush* > Resources;
	Style.GetResources( Resources );

	CreateTextures( Resources );

	for( int32 AtlasIndex = 0; AtlasIndex < TextureAtlases.Num(); ++AtlasIndex )
	{
		TextureAtlases[AtlasIndex]->InitAtlasTexture( AtlasIndex );
	}
}


void FSlateD3DTextureManager::CreateTextures( const TArray< const FSlateBrush* >& Resources )
{
	TMap<FName,FNewTextureInfo> TextureInfoMap;
	
	for( int32 ResourceIndex = 0; ResourceIndex < Resources.Num(); ++ResourceIndex )
	{
		const FSlateBrush& Brush = *Resources[ResourceIndex];
		const FName TextureName = Brush.GetResourceName();

		if( TextureName != NAME_None && !ResourceMap.Contains(TextureName) )
		{
			// Find the texture or add it if it doesn't exist (only load the texture once)
			FNewTextureInfo& Info = TextureInfoMap.FindOrAdd( TextureName );

			Info.bSrgb = (Brush.ImageType != ESlateBrushImageType::Linear);

			// Only atlas the texture if none of the brushes that use it tile it
			Info.bShouldAtlas &= (Brush.Tiling == ESlateBrushTileType::NoTile && Info.bSrgb );


			if( !Info.TextureData.IsValid())
			{
				uint32 Width = 0;
				uint32 Height = 0;
				TArray<uint8> RawData;
				bool bSucceeded = LoadTexture( Brush, Width, Height, RawData );

				const uint32 Stride = 4; // RGBA

				Info.TextureData = MakeShareable( new FSlateTextureData( Width, Height, Stride, RawData ) );

				const bool bTooLargeForAtlas = (Width >= 256 || Height >= 256);

				Info.bShouldAtlas &= !bTooLargeForAtlas;

				if( !bSucceeded )
				{
					TextureInfoMap.Remove( TextureName );
				}
			}
		}
	}

	TextureInfoMap.ValueSort( FCompareFNewTextureInfoByTextureSize() );

	for( TMap<FName,FNewTextureInfo>::TConstIterator It(TextureInfoMap); It; ++It )
	{
		const FNewTextureInfo& Info = It.Value();
		FName TextureName = It.Key();
		FString NameStr = TextureName.ToString();

		FSlateShaderResourceProxy* NewTexture = GenerateTextureResource( Info );

		ResourceMap.Add( TextureName, NewTexture );
	}
}

void FSlateD3DTextureManager::CreateTextureNoAtlas( const FSlateBrush& InBrush )
{
	const FName TextureName = InBrush.GetResourceName();
	if( TextureName != NAME_None && GetShaderResource( InBrush ) == NULL )
	{
		FNewTextureInfo Info;
		Info.bShouldAtlas = false;

		uint32 Width = 0;
		uint32 Height = 0;
		TArray<uint8> RawData;
		const uint32 Stride = 4; // RGBA

		bool bSucceeded = LoadTexture( InBrush, Width, Height, RawData );

		if( bSucceeded )
		{
			Info.TextureData = MakeShareable( new FSlateTextureData( Width, Height, Stride, RawData ) );

			FSlateShaderResourceProxy* NewTexture = GenerateTextureResource( Info );

			ResourceMap.Add( TextureName, NewTexture );
		}
	}
}
/**
 * Returns a texture with the passed in name or NULL if it cannot be found.
 */
FSlateShaderResourceProxy* FSlateD3DTextureManager::GetShaderResource( const FSlateBrush& InBrush )
{
	FSlateShaderResourceProxy* Texture = NULL;
	if( InBrush.IsDynamicallyLoaded() )
	{
		Texture = GetDynamicTextureResource( InBrush );
	}
	else
	{
		Texture = ResourceMap.FindRef( InBrush.GetResourceName() );
	}

	return Texture;
}

ISlateAtlasProvider* FSlateD3DTextureManager::GetTextureAtlasProvider()
{
	return this;
}

bool FSlateD3DTextureManager::LoadTexture( const FSlateBrush& InBrush, uint32& OutWidth, uint32& OutHeight, TArray<uint8>& OutDecodedImage )
{
	FString ResourcePath = GetResourcePath( InBrush );

	uint32 BytesPerPixel = 4;
	bool bSucceeded = true;
	TArray<uint8> RawFileData;
	if( FFileHelper::LoadFileToArray( RawFileData, *ResourcePath ) )
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );
		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper( EImageFormat::PNG );
		if ( ImageWrapper.IsValid() && ImageWrapper->SetCompressed( RawFileData.GetData(), RawFileData.Num() ) )
		{
			OutWidth = ImageWrapper->GetWidth();
			OutHeight = ImageWrapper->GetHeight();

			const TArray<uint8>* RawData = NULL;
			if (ImageWrapper->GetRaw(ERGBFormat::RGBA, 8, RawData) == false)
			{
				UE_LOG(LogSlateD3D, Log, TEXT("Invalid texture format for Slate resource only RGBA and RGB pngs are supported: %s"), *InBrush.GetResourceName().ToString() );
				bSucceeded = false;
			}
			else
			{
				OutDecodedImage = *RawData;
			}
		}
		else
		{
			UE_LOG(LogSlateD3D, Log, TEXT("Only pngs are supported in Slate. [%s] '%s'"), *InBrush.GetResourceName().ToString(), *ResourcePath);
			bSucceeded = false;
		}
	}
	else
	{
		UE_LOG(LogSlateD3D, Log, TEXT("Could not find file for Slate resource: [%s] '%s'"), *InBrush.GetResourceName().ToString(), *ResourcePath);
		bSucceeded = false;
	}

	return bSucceeded;
}

FSlateShaderResourceProxy* FSlateD3DTextureManager::CreateColorTexture( const FName TextureName, FColor InColor )
{
	FNewTextureInfo Info;

	TArray<uint8> RawData;
	RawData.AddUninitialized(4);
	RawData[0] = InColor.R;
	RawData[1] = InColor.G;
	RawData[2] = InColor.B;
	RawData[3] = InColor.A;
	Info.bShouldAtlas = false;
	
	uint32 Width = 1;
	uint32 Height = 1;
	uint32 Stride = 4;
	Info.TextureData = MakeShareable( new FSlateTextureData( Width, Height, Stride, RawData ) );


	FSlateShaderResourceProxy* NewTexture = GenerateTextureResource( Info );
	checkSlow( !ResourceMap.Contains( TextureName ) );
	ResourceMap.Add( TextureName, NewTexture );

	return NewTexture;
}

FSlateShaderResourceProxy* FSlateD3DTextureManager::GetDynamicTextureResource( const FSlateBrush& InBrush )
{
	const FName ResourceName = InBrush.GetResourceName();

	// Bail out if we already have this texture loaded
	TSharedPtr<FDynamicTextureResource> TextureResource = DynamicTextureMap.FindRef(ResourceName);
	if (TextureResource.IsValid())
	{
		return TextureResource->Proxy;
	}

	if( InBrush.IsDynamicallyLoaded() )
	{		
		uint32 Width = 0;
		uint32 Height = 0;
		TArray<uint8> RawData;
		bool bSucceeded = LoadTexture( InBrush, Width, Height, RawData );

		if( bSucceeded )
		{
			return CreateDynamicTextureResource(ResourceName, Width, Height, RawData);
		}
		else
		{
			TextureResource = MakeShareable( new FDynamicTextureResource( NULL ) );

			// Add the null texture so we dont continuously try to load it.
			DynamicTextureMap.Add( ResourceName, TextureResource );
		}
	}

	if( TextureResource.IsValid() )
	{
		return TextureResource->Proxy;
	}

	// dynamic texture was not found or loaded
	return  NULL;
}

FSlateShaderResourceProxy* FSlateD3DTextureManager::CreateDynamicTextureResource(FName ResourceName, uint32 Width, uint32 Height, const TArray< uint8 >& RawData)
{
	// Bail out if we already have this texture loaded
	TSharedPtr<FDynamicTextureResource> TextureResource = DynamicTextureMap.FindRef(ResourceName);
	if (TextureResource.IsValid())
	{
		return TextureResource->Proxy;
	}

	// Keep track of non-atlased textures so we can free their resources later
	FSlateD3DTexture* LoadedTexture = new FSlateD3DTexture(Width, Height);

	TextureResource = MakeShareable(new FDynamicTextureResource(LoadedTexture));

	D3D11_SUBRESOURCE_DATA InitData;
	InitData.pSysMem = RawData.GetData();
	InitData.SysMemPitch = Width * 4;

	FNewTextureInfo Info;
	Info.bShouldAtlas = false;
	LoadedTexture->Init(Info.bSrgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM, &InitData);

	TextureResource->Proxy->ActualSize = FIntPoint(Width, Height);
	TextureResource->Proxy->Resource = TextureResource->D3DTexture;

	// Map the new resource for the UTexture so we don't have to load again
	DynamicTextureMap.Add(ResourceName, TextureResource);

	return TextureResource->Proxy;
}

void FSlateD3DTextureManager::ReleaseDynamicTextureResource( const FSlateBrush& InBrush )
{
	// Note: Only dynamically loaded or utexture brushes can be dynamically released
	if( InBrush.IsDynamicallyLoaded() )
	{
		const FName ResourceName = InBrush.GetResourceName();
		TSharedPtr<FDynamicTextureResource> TextureResource = DynamicTextureMap.FindRef(ResourceName);
		if( TextureResource.IsValid() )
		{
			//remove it from the texture map
			DynamicTextureMap.Remove( ResourceName );
			
			check( TextureResource.IsUnique() );
		}
	}
}

FSlateShaderResourceProxy* FSlateD3DTextureManager::GenerateTextureResource( const FNewTextureInfo& Info )
{
	FSlateShaderResourceProxy* NewProxy = NULL;

	const uint32 Width = Info.TextureData->GetWidth();
	const uint32 Height = Info.TextureData->GetHeight();

	if( Info.bShouldAtlas )
	{
		const uint32 AtlasSize = 1024;
		// 4 bytes per pixel
		const uint32 AtlasStride = sizeof(uint8)*4; 
		// always use one pixel padding.
		const uint8 Padding = 1;
		const FAtlasedTextureSlot* NewSlot = NULL;

		FSlateTextureAtlasD3D* Atlas = NULL;

		// Get the last atlas and find a slot for the texture
		for( int32 AtlasIndex = 0; AtlasIndex < TextureAtlases.Num(); ++AtlasIndex )
		{
			Atlas = TextureAtlases[AtlasIndex];
			NewSlot = Atlas->AddTexture( Width, Height, Info.TextureData->GetRawBytes() );
			if( NewSlot )
			{
				break;
			}
		}

		// No new slot was found in any atlas so we have to make another one
		if( !NewSlot )
		{
			// A new slot in the atlas could not be found, make a new atlas and add the texture to it
			Atlas = new FSlateTextureAtlasD3D( AtlasSize, AtlasSize, AtlasStride, ESlateTextureAtlasPaddingStyle::DilateBorder );
			TextureAtlases.Add( Atlas );
			NewSlot = TextureAtlases.Last()->AddTexture( Width, Height, Info.TextureData->GetRawBytes() );
		}

		check( Atlas && NewSlot );

		// Create a proxy representing this texture in the atlas
		NewProxy = new FSlateShaderResourceProxy;
		NewProxy->Resource = Atlas->GetAtlasTexture();
		// Compute the sub-uvs for the location of this texture in the atlas, accounting for padding
		NewProxy->StartUV = FVector2D( (float)(NewSlot->X+Padding) / Atlas->GetWidth(), (float)(NewSlot->Y+Padding) / Atlas->GetHeight() );
		NewProxy->SizeUV = FVector2D( (float)(NewSlot->Width-Padding*2) / Atlas->GetWidth(), (float)(NewSlot->Height-Padding*2) / Atlas->GetHeight() );
		NewProxy->ActualSize = FIntPoint( Width, Height );
	}
	else
	{
		// The texture is not atlased create a new texture proxy and just point it to the actual texture
		NewProxy = new FSlateShaderResourceProxy;

		// Keep track of non-atlased textures so we can free their resources later
		FSlateD3DTexture* Texture = new FSlateD3DTexture( Width, Height );
		NonAtlasedTextures.Add( Texture );

		NewProxy->Resource = Texture;
		NewProxy->StartUV = FVector2D(0.0f,0.0f);
		NewProxy->SizeUV = FVector2D(1.0f, 1.0f);
		NewProxy->ActualSize = FIntPoint( Width, Height );

		D3D11_SUBRESOURCE_DATA InitData;
		InitData.pSysMem = Info.TextureData->GetRawBytes().GetData();
		InitData.SysMemPitch = Width * 4;

		Texture->Init( Info.bSrgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM, &InitData );
	}

	return NewProxy;
}
