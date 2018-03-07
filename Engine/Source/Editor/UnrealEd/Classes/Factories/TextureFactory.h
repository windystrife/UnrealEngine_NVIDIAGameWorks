// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Factories/Factory.h"
#include "Engine/Texture.h"
#include "ImportSettings.h"
#include "TextureFactory.generated.h"


class UTexture2D;
class UTextureCube;

UCLASS(customconstructor, collapsecategories, hidecategories=Object)
class UNREALED_API UTextureFactory : public UFactory, public IImportSettingsParser
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	uint32 NoCompression:1;

	/** If enabled, the texture's alpha channel will be discarded during compression */
	UPROPERTY(EditAnywhere, Category=Compression, meta=(ToolTip="If enabled, the texture's alpha channel will be discarded during compression"))
	uint32 NoAlpha:1;

	/** If enabled, compression is deferred until the texture is saved */
	UPROPERTY(EditAnywhere, Category=Compression, meta=(ToolTip="If enabled, compression is deferred until the texture is saved"))
	uint32 bDeferCompression:1;

	/** Compression settings for the texture */
	UPROPERTY(EditAnywhere, Category=Compression, meta=(ToolTip="Compression settings for the texture"))
	TEnumAsByte<enum TextureCompressionSettings> CompressionSettings;

	/** If enabled, a material will automatically be created for the texture */
	UPROPERTY(EditAnywhere, Category=TextureFactory, meta=(ToolTip="If enabled, a material will automatically be created for the texture"))
	uint32 bCreateMaterial:1;

	/** If enabled, link the texture to the created material's base color */
	UPROPERTY(EditAnywhere, Category=CreateMaterial, meta=(ToolTip="If enabled, link the texture to the created material's base color"))
	uint32 bRGBToBaseColor:1;

	/** If enabled, link the texture to the created material's emissive color */
	UPROPERTY(EditAnywhere, Category=CreateMaterial, meta=(ToolTip="If enabled, link the texture to the created material's emissive color"))
	uint32 bRGBToEmissive:1;

	/** If enabled, link the texture's alpha to the created material's specular color */
	UPROPERTY(EditAnywhere, Category=CreateMaterial, meta=(ToolTip="If enabled, link the texture's alpha to the created material's roughness"))
	uint32 bAlphaToRoughness:1;

	/** If enabled, link the texture's alpha to the created material's emissive color */
	UPROPERTY(EditAnywhere, Category=CreateMaterial, meta=(ToolTip="If enabled, link the texture's alpha to the created material's emissive color"))
	uint32 bAlphaToEmissive:1;

	/** If enabled, link the texture's alpha to the created material's opacity */
	UPROPERTY(EditAnywhere, Category=CreateMaterial, meta=(ToolTip="If enabled, link the texture's alpha to the created material's opacity"))
	uint32 bAlphaToOpacity:1;

	/** If enabled, link the texture's alpha to the created material's opacity mask */
	UPROPERTY(EditAnywhere, Category=CreateMaterial, meta=(ToolTip="If enabled, link the texture's alpha to the created material's opacity mask"))
	uint32 bAlphaToOpacityMask:1;

	/** If enabled, the created material will be two-sided */
	UPROPERTY(EditAnywhere, Category=CreateMaterial, meta=(ToolTip="If enabled, the created material will be two-sided"))
	uint32 bTwoSided:1;

	/** The blend mode of the created material */
	UPROPERTY(EditAnywhere, Category=CreateMaterial, meta=(ToolTip="The blend mode of the created material"))
	TEnumAsByte<enum EBlendMode> Blending;

	/** The shading model of the created material */
	UPROPERTY(EditAnywhere, Category=CreateMaterial, meta=(ToolTip="The shading model of the created material"))
	TEnumAsByte<enum EMaterialShadingModel> ShadingModel;

	/** The mip-map generation settings for the texture; Allows customization of the content of the mip-map chain */
	UPROPERTY(EditAnywhere, Category=TextureFactory, meta=(ToolTip="The mip-map generation settings for the texture; Allows customization of the content of the mip-map chain"))
	TEnumAsByte<enum TextureMipGenSettings> MipGenSettings;

	/** The group the texture belongs to */
	UPROPERTY(EditAnywhere, Category=LODGroup, meta=(ToolTip="The group the texture belongs to"))
	TEnumAsByte<enum TextureGroup> LODGroup;

	/** If enabled, mip-map alpha values will be dithered for smooth transitions */
	UPROPERTY(EditAnywhere, Category=DitherMipMaps, meta=(ToolTip="If enabled, mip-map alpha values will be dithered for smooth transitions"))
	uint32 bDitherMipMapAlpha:1;

	/** Channel values to compare to when preserving alpha coverage from a mask. */
	UPROPERTY(EditAnywhere, Category=PreserveAlphaCoverage, meta=(ToolTip="Channel values to compare to when preserving alpha coverage from a mask for mips"))
	FVector4 AlphaCoverageThresholds;

	/** If enabled, preserve the value of border pixels when creating mip-maps */
	UPROPERTY(EditAnywhere, Category=PreserveBorder, meta=(ToolTip="If enabled, preserve the value of border pixels when creating mip-maps"))
	uint32 bPreserveBorder:1;

	/** If enabled, the texture's green channel will be inverted. This is useful for some normal maps */
	UPROPERTY(EditAnywhere, Category=NormalMap, meta=(ToolTip="If enabled, the texture's green channel will be inverted. This is useful for some normal maps"))
	uint32 bFlipNormalMapGreenChannel:1;

	/** If enabled, we are using the existing settings for a texture that already existed. */
	UPROPERTY(Transient)
	uint32 bUsingExistingSettings:1;
	
	

public:
	UTextureFactory(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin UObject Interface
	virtual void PostInitProperties() override;
	//~ End UObject Interface

	//~ Begin UFactory Interface
	virtual bool DoesSupportClass(UClass* Class) override;
	virtual UObject* FactoryCreateBinary( UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn ) override;
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual IImportSettingsParser* GetImportSettingsParser() override;

	//~ End UFactory Interface
	
	/** IImportSettingsParser interface */
	virtual void ParseFromJson(TSharedRef<class FJsonObject> ImportSettingsJson) override;


	/** Create a texture given the appropriate input parameters	*/
	virtual UTexture2D* CreateTexture2D( UObject* InParent, FName Name, EObjectFlags Flags );
	virtual UTextureCube* CreateTextureCube( UObject* InParent, FName Name, EObjectFlags Flags );

	/**
	 * Suppresses the dialog box that, when importing over an existing texture, asks if the users wishes to overwrite its settings.
	 * This is primarily for reimporting textures.
	 */
	static void SuppressImportOverwriteDialog();

	/**
	 *	Initializes the given texture from the TextureData text block supplied.
	 *	The TextureData text block is assumed to have been generated by the UTextureExporterT3D.
	 *
	 *	@param	InTexture	The texture to initialize
	 *	@param	Text		The texture data text generated by the TextureExporterT3D
	 *	@param	Warn		Where to send warnings/errors
	 *
	 *	@return	bool		true if successful, false if not
	 */
	bool InitializeFromT3DTextureDataText(UTexture* InTexture, const FString& Text, FFeedbackContext* Warn);
	
	// @todo document
	bool InitializeFromT3DTexture2DDataText(UTexture2D* InTexture2D, const TCHAR*& Buffer, FFeedbackContext* Warn);

	// @todo document
	void FindCubeMapFace(const FString& ParsedText, const FString& FaceString, UTextureCube& TextureCube, UTexture2D*& TextureFace);

	// @todo document
	bool InitializeFromT3DTextureCubeDataText(UTextureCube* InTextureCube, const TCHAR*& Buffer, FFeedbackContext* Warn);

private:
	/** This variable is static because in StaticImportObject() the type of the factory is not known. */
	static bool bSuppressImportOverwriteDialog;

	/**
	*	Tests if the given height and width specify a supported texture resolution to import; Can optionally check if the height/width are powers of two
	*
	*	@param	Width					The width of an imported texture whose validity should be checked
	*	@param	Height					The height of an imported texture whose validity should be checked
	*	@param	bAllowNonPowerOfTwo		Whether or not non-power-of-two textures are allowed
	*	@param	Warn					Where to send warnings/errors
	*
	*	@return	bool					true if the given height/width represent a supported texture resolution, false if not
	*/
	static bool IsImportResolutionValid(int32 Width, int32 Height, bool bAllowNonPowerOfTwo, FFeedbackContext* Warn);

	/** used by CreateTexture() */
	UTexture* ImportTexture(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn);

	/** Applies import settings directly to the texture after import */
	void ApplyAutoImportSettings(UTexture* Texture);
private:
	/** Texture settings from the automated importer that should be applied to the new texture */
	TSharedPtr<class FJsonObject> AutomatedImportSettings;
};
