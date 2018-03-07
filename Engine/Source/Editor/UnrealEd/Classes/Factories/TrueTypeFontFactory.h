// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// TrueTypeFontFactory
//~=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EditorReimportHandler.h"
#include "Factories/TextureFactory.h"
#include "TrueTypeFontFactory.generated.h"

class UFont;
class UTexture2D;

UCLASS(hidecategories=Object, collapsecategories)
class UNREALED_API UTrueTypeFontFactory : public UTextureFactory, public FReimportHandler
{
	GENERATED_UCLASS_BODY()

	/** Import options for this font */
	UPROPERTY(EditAnywhere, Instanced, Category = TrueTypeFontFactory, meta = (ToolTip = "Import options for the font"))
	class UFontImportOptions* ImportOptions;

	/** True when the font dialog was shown for this factory during the non-legacy creation process */
	UPROPERTY()
	bool bPropertiesConfigured;

	/** True if a font was selected during the non-legacy creation process */
	UPROPERTY()
	bool bFontSelected;

	//~ Begin UObject Interface
	virtual void PostInitProperties() override;
	//~ End UObject Interface

	//~ Begin UFactory Interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn) override;
	virtual bool ShouldShowInNewMenu() const override
	{
		// Don't show this factory in the content browser menu; it's invoked manually when changing the UFont cache type to "Offline" 
		return false;
	}
	//~ Begin UFactory Interface	

	//~ Begin FReimportHandler Interface
	virtual bool CanReimport( UObject* Obj, TArray<FString>& OutFilenames ) override;
	virtual void SetReimportPaths( UObject* Obj, const TArray<FString>& NewReimportPaths ) override;
	virtual EReimportResult::Type Reimport( UObject* Obj ) override;
	virtual int32 GetPriority() const override;
	//~ End FReimportHandler Interface

	/** Creates the import options structure for this font */
	void SetupFontImportOptions();

#if PLATFORM_WINDOWS
	/**
	 * Win32 Platform Only: Creates a single font texture using the Windows GDI
	 *
	 * @param Font (In/Out) The font we're operating with
	 * @param dc The device context configured to paint this font
	 * @param RowHeight Height of a font row in pixels
	 * @param TextureNum The current texture index
	 *
	 * @return Returns the newly created texture, if successful, otherwise NULL
	 */
	UTexture2D* CreateTextureFromDC( UFont* Font, Windows::HDC dc, int32 RowHeight, int32 TextureNum );
#endif

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
	void* LoadFontFace( void* FTLibrary, int32 Height, FFeedbackContext* Warn, void** OutFontData );
	UTexture2D* CreateTextureFromBitmap( UFont* Font, uint8* BitmapData, int32 Height, int32 TextureNum );
	bool CreateFontTexture( UFont* Font, FFeedbackContext* Warn, const int32 NumResolutions, const int32 CharsPerPage,
		const TMap<TCHAR,TCHAR>& InverseMap, const TArray< float >& ResHeights );

#if PLATFORM_WINDOWS
	FString FindBitmapFontFile();
#endif

	/**
	 * Windows/Mac Platform Only: Imports a TrueType font
	 *
	 * @param Font (In/Out) The font object that we're importing into
	 * @param Warn Feedback context for displaying warnings and errors
	 * @param NumResolutions Number of resolution pages we should generate 
	 * @param ResHeights Font height for each resolution (one array entry per resolution)
	 *
	 * @return true if successful
	 */
	bool ImportTrueTypeFont( UFont* Font, FFeedbackContext* Warn, const int32 NumResolutions, const TArray< float >& ResHeights );
#endif
};



