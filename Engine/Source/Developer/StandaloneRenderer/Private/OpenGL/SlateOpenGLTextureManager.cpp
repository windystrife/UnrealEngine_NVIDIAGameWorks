// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OpenGL/SlateOpenGLTextureManager.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "StandaloneRendererPlatformHeaders.h"
#include "OpenGL/SlateOpenGLTextures.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"


DEFINE_LOG_CATEGORY_STATIC(LogSlateOpenGL, Log, All);


FSlateOpenGLTextureManager::FDynamicTextureResource::FDynamicTextureResource( FSlateOpenGLTexture* InOpenGLTexture )
	: Proxy( new FSlateShaderResourceProxy )
	, OpenGLTexture( InOpenGLTexture  )
{
}

FSlateOpenGLTextureManager::FDynamicTextureResource::~FDynamicTextureResource()
{
	if( Proxy )
	{
		delete Proxy;
	}

	if( OpenGLTexture )
	{
		delete OpenGLTexture;
	}
}


FSlateOpenGLTextureManager::FSlateOpenGLTextureManager()
{

}

FSlateOpenGLTextureManager::~FSlateOpenGLTextureManager()
{
	for( int32 ResourceIndex = 0; ResourceIndex < NonAtlasedTextures.Num(); ++ResourceIndex )
	{
		delete NonAtlasedTextures[ResourceIndex];
	}
}

void FSlateOpenGLTextureManager::LoadUsedTextures()
{
	TArray< const FSlateBrush* > Resources;
	FSlateStyleRegistry::GetAllResources( Resources );

	CreateTextures( Resources );
}

void FSlateOpenGLTextureManager::LoadStyleResources( const ISlateStyle& Style )
{
	TArray< const FSlateBrush* > Resources;
	Style.GetResources( Resources );

	CreateTextures( Resources );
}

void FSlateOpenGLTextureManager::CreateTextures( const TArray< const FSlateBrush* >& Resources )
{
	TMap<FName,FNewTextureInfo> TextureInfoMap;

	for( int32 ResourceIndex = 0; ResourceIndex < Resources.Num(); ++ResourceIndex )
	{			
		const FSlateBrush& Brush = *Resources[ResourceIndex];

		const FName TextureName = Brush.GetResourceName();

		if( TextureName != NAME_None  && !ResourceMap.Contains(TextureName) )
		{
			// Find the texture or add it if it doesnt exist (only load the texture once)
			FNewTextureInfo& Info = TextureInfoMap.FindOrAdd( TextureName );
			Info.bSrgb = (Brush.ImageType != ESlateBrushImageType::Linear);
			Info.bShouldAtlas = false; // Currently not supported


			if(! Info.TextureData.IsValid() )
			{
				uint32 Width = 0;
				uint32 Height = 0;
				TArray<uint8> RawData;
				bool bSucceeded = LoadTexture( Brush, Width, Height, RawData );
				const uint32 Stride = 4; //RGBA
				Info.TextureData = MakeShareable( new FSlateTextureData( Width, Height, Stride, RawData ) );

				if( !bSucceeded )
				{
					TextureInfoMap.Remove( TextureName );
				}
				else
				{
					FSlateShaderResourceProxy* TextureProxy = GenerateTextureResource( Info );

					ResourceMap.Add( TextureName, TextureProxy );
				}
			}
		}
	}
}

/** 
 * Creates a texture from a file on disk
 *
 * @param TextureName	The name of the texture to load
 */
bool FSlateOpenGLTextureManager::LoadTexture( const FSlateBrush& InBrush, uint32& OutWidth, uint32& OutHeight, TArray<uint8>& OutDecodedImage )
{
	FName TextureName = InBrush.GetResourceName();

	bool bSucceeded = false;

	// Get the path to the resource
	FString ResourcePath = GetResourcePath( InBrush );

	// Load the resource into an array
	TArray<uint8> Buffer;
	if( FFileHelper::LoadFileToArray( Buffer, *ResourcePath ) )
	{
		// We assume all resources are png for now.
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>( FName("ImageWrapper") );
		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper( EImageFormat::PNG );
		if ( ImageWrapper.IsValid() && ImageWrapper->SetCompressed( Buffer.GetData(), Buffer.Num() ) )
		{
			// Determine the block size.  This is bytes per pixel
			const uint8 BlockSize = 4;

			OutWidth = ImageWrapper->GetWidth();
			OutHeight = ImageWrapper->GetHeight();

			// Decode the png and get the data in raw rgb
			const TArray<uint8>* RawData = NULL;
			if (ImageWrapper->GetRaw(ERGBFormat::RGBA, 8, RawData))
			{
				OutDecodedImage = *RawData;
				bSucceeded = true;
			}
			else
			{
				UE_LOG(LogSlateOpenGL, Log, TEXT("Couldn't convert to raw data. [%s] '%s'"), *InBrush.GetResourceName().ToString(), *ResourcePath);
			}
		}
		else
		{
			UE_LOG(LogSlateOpenGL, Log, TEXT("Only pngs are supported in Slate. [%s] '%s'"), *InBrush.GetResourceName().ToString(), *ResourcePath);
		}
	}
	else
	{
		UE_LOG(LogSlateOpenGL, Log,  TEXT("Could not find file for Slate texture: [%s] '%s'"), *InBrush.GetResourceName().ToString(), *ResourcePath);
	}

	
	return bSucceeded;
}

FSlateOpenGLTexture* FSlateOpenGLTextureManager::CreateColorTexture( const FName TextureName, FColor InColor )
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

	FSlateShaderResourceProxy* TextureProxy = GenerateTextureResource( Info );
	
	// Cache the texture proxy for fast access later when we need the texture for rendering
	ResourceMap.Add( TextureName, TextureProxy );

	return (FSlateOpenGLTexture*)TextureProxy->Resource;
}

FSlateShaderResourceProxy* FSlateOpenGLTextureManager::GenerateTextureResource( const FNewTextureInfo& Info )
{
	uint32 Width = Info.TextureData->GetWidth();
	uint32 Height = Info.TextureData->GetHeight();

	// Create a representation of the texture for rendering
	FSlateOpenGLTexture* NewTexture = new FSlateOpenGLTexture( Width, Height );
#if !PLATFORM_USES_ES2
	NewTexture->Init( Info.bSrgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, Info.TextureData->GetRawBytes() );
#else
	NewTexture->Init( Info.bSrgb ? GL_SRGB8_ALPHA8_EXT : GL_RGBA8, Info.TextureData->GetRawBytes() );
#endif

	NonAtlasedTextures.Add( NewTexture );

	FSlateShaderResourceProxy* TextureProxy = new FSlateShaderResourceProxy;

	TextureProxy->Resource = NewTexture;
	TextureProxy->StartUV = FVector2D(0.0f,0.0f);
	TextureProxy->SizeUV = FVector2D(1.0f, 1.0f);
	TextureProxy->ActualSize = FIntPoint( Width, Height );

	return TextureProxy;
}

FSlateShaderResourceProxy* FSlateOpenGLTextureManager::GetDynamicTextureResource( const FSlateBrush& InBrush )
{
	const FName ResourceName = InBrush.GetResourceName();

	// Bail out if we already have this texture loaded
	TSharedPtr<FDynamicTextureResource> TextureResource = DynamicTextureMap.FindRef(ResourceName);
	if (TextureResource.IsValid())
	{
		return TextureResource->Proxy;
	}

	if (InBrush.IsDynamicallyLoaded())
	{
		uint32 Width = 0;
		uint32 Height = 0;
		TArray<uint8> RawData;
		bool bSucceeded = LoadTexture(InBrush, Width, Height, RawData);

		if (bSucceeded)
		{
			return CreateDynamicTextureResource(ResourceName, Width, Height, RawData);
		}
		else
		{
			TextureResource = MakeShareable(new FDynamicTextureResource(NULL));

			// Add the null texture so we dont continuously try to load it.
			DynamicTextureMap.Add(ResourceName, TextureResource);
		}
	}

	if (TextureResource.IsValid())
	{
		return TextureResource->Proxy;
	}

	// dynamic texture was not found or loaded
	return  NULL;
}

FSlateShaderResourceProxy* FSlateOpenGLTextureManager::CreateDynamicTextureResource(FName ResourceName, uint32 Width, uint32 Height, const TArray< uint8 >& RawData)
{
	// Bail out if we already have this texture loaded
	TSharedPtr<FDynamicTextureResource> TextureResource = DynamicTextureMap.FindRef(ResourceName);
	if (TextureResource.IsValid())
	{
		return TextureResource->Proxy;
	}

	// Keep track of non-atlased textures so we can free their resources later
	FSlateOpenGLTexture* LoadedTexture = new FSlateOpenGLTexture(Width, Height);

	FNewTextureInfo Info;
	Info.bShouldAtlas = false;

#if !PLATFORM_USES_ES2
	LoadedTexture->Init(Info.bSrgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, RawData);
#else
	LoadedTexture->Init(Info.bSrgb ? GL_SRGB8_ALPHA8_EXT : GL_RGBA8, RawData);
#endif

	TextureResource = MakeShareable(new FDynamicTextureResource(LoadedTexture));

	TextureResource->Proxy->ActualSize = FIntPoint(Width, Height);
	TextureResource->Proxy->Resource = TextureResource->OpenGLTexture;

	// Map the new resource for the UTexture so we don't have to load again
	DynamicTextureMap.Add(ResourceName, TextureResource);

	return TextureResource->Proxy;
}

void FSlateOpenGLTextureManager::ReleaseDynamicTextureResource(const FSlateBrush& InBrush)
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

/**
 * Returns a texture with the passed in name or NULL if it cannot be found.
 */
FSlateShaderResourceProxy* FSlateOpenGLTextureManager::GetShaderResource( const FSlateBrush& InBrush )
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

ISlateAtlasProvider* FSlateOpenGLTextureManager::GetTextureAtlasProvider()
{
	// Texture atlases aren't implemented for the standalone OpenGL renderer
	return nullptr;
}
