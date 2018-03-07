// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Fonts/CompositeFont.h"
#include "SlateGlobals.h"
#include "UObject/EditorObjectVersion.h"
#include "Fonts/FontFaceInterface.h"
#include "Templates/Casts.h"
#include "Fonts/FontBulkData.h"

// The total true type memory we are using for resident font faces
DECLARE_MEMORY_STAT(TEXT("Resident Font Memory (TTF/OTF)"), STAT_SlateRawFontDataMemory, STATGROUP_SlateMemory);

void FFontFaceData::TrackMemoryUsage() const
{
	INC_DWORD_STAT_BY(STAT_SlateRawFontDataMemory, Data.GetAllocatedSize());
}

void FFontFaceData::UntrackMemoryUsage() const
{
	DEC_DWORD_STAT_BY(STAT_SlateRawFontDataMemory, Data.GetAllocatedSize());
}

FFontData::FFontData()
	: FontFilename()
	, Hinting(EFontHinting::Default)
	, LoadingPolicy(EFontLoadingPolicy::LazyLoad)
	, FontFaceAsset(nullptr)
#if WITH_EDITORONLY_DATA
	, BulkDataPtr_DEPRECATED(nullptr)
	, FontData_DEPRECATED()
#endif // WITH_EDITORONLY_DATA
{
}

FFontData::FFontData(const UObject* const InFontFaceAsset)
	: FontFilename()
	, Hinting(EFontHinting::Default)
	, LoadingPolicy(EFontLoadingPolicy::LazyLoad)
	, FontFaceAsset(InFontFaceAsset)
#if WITH_EDITORONLY_DATA
	, BulkDataPtr_DEPRECATED(nullptr)
	, FontData_DEPRECATED()
#endif // WITH_EDITORONLY_DATA
{
	if (FontFaceAsset)
	{
		CastChecked<const IFontFaceInterface>(FontFaceAsset);
	}
}

FFontData::FFontData(FString InFontFilename, const EFontHinting InHinting, const EFontLoadingPolicy InLoadingPolicy)
	: FontFilename(MoveTemp(InFontFilename))
	, Hinting(InHinting)
	, LoadingPolicy(InLoadingPolicy)
	, FontFaceAsset(nullptr)
#if WITH_EDITORONLY_DATA
	, BulkDataPtr_DEPRECATED(nullptr)
	, FontData_DEPRECATED()
#endif // WITH_EDITORONLY_DATA
{
	check(InLoadingPolicy != EFontLoadingPolicy::Inline);
}

bool FFontData::HasFont() const
{
	FFontFaceDataConstPtr FontFaceData = GetFontFaceData();
	return (FontFaceData.IsValid() && FontFaceData->HasData()) || (!GetFontFilename().IsEmpty());
}

const FString& FFontData::GetFontFilename() const
{
	if (FontFaceAsset)
	{
		const IFontFaceInterface* FontFace = CastChecked<const IFontFaceInterface>(FontFaceAsset);
		return FontFace->GetFontFilename();
	}
	return FontFilename;
}

EFontHinting FFontData::GetHinting() const
{
	if (FontFaceAsset)
	{
		const IFontFaceInterface* FontFace = CastChecked<const IFontFaceInterface>(FontFaceAsset);
		return FontFace->GetHinting();
	}
	return Hinting;
}

EFontLoadingPolicy FFontData::GetLoadingPolicy() const
{
	if (FontFaceAsset)
	{
		const IFontFaceInterface* FontFace = CastChecked<const IFontFaceInterface>(FontFaceAsset);
		return FontFace->GetLoadingPolicy();
	}
	return LoadingPolicy;
}

EFontLayoutMethod FFontData::GetLayoutMethod() const
{
	if (FontFaceAsset)
	{
		const IFontFaceInterface* FontFace = CastChecked<const IFontFaceInterface>(FontFaceAsset);
		return FontFace->GetLayoutMethod();
	}
	return EFontLayoutMethod::Metrics;
}

FFontFaceDataConstPtr FFontData::GetFontFaceData() const
{
	if (FontFaceAsset)
	{
		const IFontFaceInterface* FontFace = CastChecked<const IFontFaceInterface>(FontFaceAsset);
		return FontFace->GetFontFaceData();
	}
	return nullptr;
}

const UObject* FFontData::GetFontFaceAsset() const
{
	return FontFaceAsset;
}

#if WITH_EDITORONLY_DATA
bool FFontData::HasLegacyData() const
{
	return FontData_DEPRECATED.Num() > 0 || BulkDataPtr_DEPRECATED;
}

void FFontData::ConditionalUpgradeFontDataToBulkData(UObject* InOuter)
{
	if (FontData_DEPRECATED.Num() > 0)
	{
		UFontBulkData* NewBulkData = NewObject<UFontBulkData>(InOuter);
		BulkDataPtr_DEPRECATED = NewBulkData;

		NewBulkData->Initialize(FontData_DEPRECATED.GetData(), FontData_DEPRECATED.Num());
		FontData_DEPRECATED.Empty();
	}
}

void FFontData::ConditionalUpgradeBulkDataToFontFace(UObject* InOuter, UClass* InFontFaceClass, const FName InFontFaceName)
{
	if (BulkDataPtr_DEPRECATED)
	{
		int32 RawBulkDataSizeBytes = 0;
		const void* RawBulkData = BulkDataPtr_DEPRECATED->Lock(RawBulkDataSizeBytes);
		if (RawBulkDataSizeBytes > 0)
		{
			UObject* NewFontFaceAsset = NewObject<UObject>(InOuter, InFontFaceClass, InFontFaceName);
			FontFaceAsset = NewFontFaceAsset;

			IFontFaceInterface* NewFontFace = CastChecked<IFontFaceInterface>(NewFontFaceAsset);
			NewFontFace->InitializeFromBulkData(FontFilename, Hinting, RawBulkData, RawBulkDataSizeBytes);
		}
		BulkDataPtr_DEPRECATED->Unlock();
		BulkDataPtr_DEPRECATED = nullptr;
	}
}
#endif // WITH_EDITORONLY_DATA

bool FFontData::operator==(const FFontData& Other) const
{
	if (FontFaceAsset != Other.FontFaceAsset)
	{
		// Using different assets
		return false;
	}
	if (FontFaceAsset && FontFaceAsset == Other.FontFaceAsset)
	{
		// Using the same asset
		return true;
	}

	// Compare inline properties
	return FontFilename == Other.FontFilename
		&& Hinting == Other.Hinting
		&& LoadingPolicy == Other.LoadingPolicy;
}

bool FFontData::operator!=(const FFontData& Other) const
{
	return !(*this == Other);
}

bool FFontData::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FEditorObjectVersion::GUID);

	if (Ar.CustomVer(FEditorObjectVersion::GUID) < FEditorObjectVersion::AddedFontFaceAssets)
	{
		// Too old, so use the default serialization
		return false;
	}

	bool bIsCooked = Ar.IsCooking();
	Ar << bIsCooked;

	if (bIsCooked)
	{
		// Cooked data uses a more compact format
		UObject* LocalFontFaceAsset = const_cast<UObject*>(FontFaceAsset);
		Ar << LocalFontFaceAsset;
		FontFaceAsset = LocalFontFaceAsset;
		
		if (!FontFaceAsset)
		{
			// Only need to serialize the other properties when we lack a font face asset
			Ar << FontFilename;
			Ar << Hinting;
			Ar << LoadingPolicy;
		}
	}
	else
	{
		// Uncooked data uses the standard struct serialization to avoid versioning on editor-only data
		UScriptStruct* FontDataStruct = StaticStruct();
		if (FontDataStruct->UseBinarySerialization(Ar))
		{
			FontDataStruct->SerializeBin(Ar, (uint8*)this);
		}
		else
		{
			FontDataStruct->SerializeTaggedProperties(Ar, (uint8*)this, FontDataStruct, nullptr);
		}
	}

	return true;
}

void FFontData::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(FontFaceAsset);
#if WITH_EDITORONLY_DATA
	Collector.AddReferencedObject(BulkDataPtr_DEPRECATED);
#endif // WITH_EDITORONLY_DATA
}

void FStandaloneCompositeFont::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (FTypefaceEntry& TypefaceEntry : DefaultTypeface.Fonts)
	{
		TypefaceEntry.Font.AddReferencedObjects(Collector);
	}

	for (FCompositeSubFont& SubFont : SubTypefaces)
	{
		for (FTypefaceEntry& TypefaceEntry : SubFont.Typeface.Fonts)
		{
			TypefaceEntry.Font.AddReferencedObjects(Collector);
		}
	}
}
