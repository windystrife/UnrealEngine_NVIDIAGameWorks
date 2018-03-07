// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "PaperSprite.h"
#include "PaperSpriteSheet.generated.h"

UCLASS(BlueprintType, meta = (DisplayThumbnail = "true"))
class UPaperSpriteSheet : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	// The names of sprites during import
	UPROPERTY(VisibleAnywhere, Category=Data)
	TArray<FString> SpriteNames;

	UPROPERTY(VisibleAnywhere, Category=Data)
	TArray< TSoftObjectPtr<class UPaperSprite> > Sprites;

	// The name of the default or diffuse texture during import
	UPROPERTY(VisibleAnywhere, Category=Data)
	FString TextureName;

	// The asset that was created for TextureName
	UPROPERTY(VisibleAnywhere, Category=Data)
	UTexture2D* Texture;

	// The name of the normal map texture during import (if any)
	UPROPERTY(VisibleAnywhere, Category=Data)
	FString NormalMapTextureName;

	// The asset that was created for NormalMapTextureName (if any)
	UPROPERTY(VisibleAnywhere, Category=Data)
	UTexture2D* NormalMapTexture;

#if WITH_EDITORONLY_DATA
	// Import data for this 
	UPROPERTY(VisibleAnywhere, Instanced, Category=ImportSettings)
	class UAssetImportData* AssetImportData;

	// UObject interface
	virtual void PostInitProperties() override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual void Serialize(FArchive& Ar) override;
	// End of UObject interface
#endif
};
