#pragma once
#include "CoreMinimal.h"

#include "EditorFramework/AssetImportData.h"
#include "BlastAssetImportOptions.h"
#include "BlastAssetImportData.generated.h"


UCLASS(HideCategories=Object)
class BLAST_API UBlastAssetImportData : public UAssetImportData
{
	GENERATED_BODY()
public:
	UBlastAssetImportData(const FObjectInitializer& ObjectInitializer);
	virtual ~UBlastAssetImportData() = default;

	UPROPERTY(EditAnywhere, Category="Import Settings")
	FBlastAssetImportOptions		ImportOptions;
};