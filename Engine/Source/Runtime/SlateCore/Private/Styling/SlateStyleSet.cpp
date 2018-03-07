// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Styling/SlateColor.h"
#include "Styling/SlateWidgetStyle.h"
#include "Styling/SlateWidgetStyleContainerBase.h"
#include "Fonts/SlateFontInfo.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Styling/StyleDefaults.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyle.h"


FSlateStyleSet::FSlateStyleSet(const FName& InStyleSetName)
: StyleSetName(InStyleSetName)
, DefaultBrush(new FSlateImageBrush(FPaths::EngineContentDir() / TEXT("Slate/Checkerboard.png"), FVector2D(16, 16), FLinearColor::White, ESlateBrushTileType::Both))
{
	// Add a mapping so that this resource will be discovered by GetStyleResources.
	Set(TEXT("Default"), GetDefaultBrush());
}

FSlateStyleSet::~FSlateStyleSet()
{
	// Delete all allocated brush resources.
	for ( TMap< FName, FSlateBrush* >::TIterator It(BrushResources); It; ++It )
	{
		if (!It.Value()->HasUObject())
		{
			delete It.Value();
		}
	}
}


const FName& FSlateStyleSet::GetStyleSetName() const
{
	return StyleSetName;
}

void FSlateStyleSet::GetResources(TArray< const FSlateBrush* >& OutResources) const
{
	// Collection for this style's brush resources.
	TArray< const FSlateBrush* > SlateBrushResources;
	for ( TMap< FName, FSlateBrush* >::TConstIterator It(BrushResources); It; ++It )
	{
		SlateBrushResources.Add(It.Value());
	}

	//Gather resources from our definitions
	for ( TMap< FName, TSharedRef< struct FSlateWidgetStyle > >::TConstIterator It(WidgetStyleValues); It; ++It )
	{
		It.Value()->GetResources(SlateBrushResources);
	}

	// Append this style's resources to OutResources.
	OutResources.Append(SlateBrushResources);
}

void FSlateStyleSet::SetContentRoot(const FString& InContentRootDir)
{
	ContentRootDir = InContentRootDir;
}

FString FSlateStyleSet::RootToContentDir(const ANSICHAR* RelativePath, const TCHAR* Extension)
{
	return ( ContentRootDir / RelativePath ) + Extension;
}

FString FSlateStyleSet::RootToContentDir(const WIDECHAR* RelativePath, const TCHAR* Extension)
{
	return ( ContentRootDir / RelativePath ) + Extension;
}

FString FSlateStyleSet::RootToContentDir(const FString& RelativePath, const TCHAR* Extension)
{
	return ( ContentRootDir / RelativePath ) + Extension;
}

FString FSlateStyleSet::RootToContentDir(const ANSICHAR* RelativePath)
{
	return ( ContentRootDir / RelativePath );
}

FString FSlateStyleSet::RootToContentDir(const WIDECHAR* RelativePath)
{
	return ( ContentRootDir / RelativePath );
}

FString FSlateStyleSet::RootToContentDir(const FString& RelativePath)
{
	return ( ContentRootDir / RelativePath );
}

void FSlateStyleSet::SetCoreContentRoot(const FString& InCoreContentRootDir)
{
	CoreContentRootDir = InCoreContentRootDir;
}

FString FSlateStyleSet::RootToCoreContentDir(const ANSICHAR* RelativePath, const TCHAR* Extension)
{
	return ( CoreContentRootDir / RelativePath ) + Extension;
}

FString FSlateStyleSet::RootToCoreContentDir(const WIDECHAR* RelativePath, const TCHAR* Extension)
{
	return ( CoreContentRootDir / RelativePath ) + Extension;
}

FString FSlateStyleSet::RootToCoreContentDir(const FString& RelativePath, const TCHAR* Extension)
{
	return ( CoreContentRootDir / RelativePath ) + Extension;
}

FString FSlateStyleSet::RootToCoreContentDir(const ANSICHAR* RelativePath)
{
	return ( CoreContentRootDir / RelativePath );
}

FString FSlateStyleSet::RootToCoreContentDir(const WIDECHAR* RelativePath)
{
	return ( CoreContentRootDir / RelativePath );
}

FString FSlateStyleSet::RootToCoreContentDir(const FString& RelativePath)
{
	return ( CoreContentRootDir / RelativePath );
}

float FSlateStyleSet::GetFloat(const FName PropertyName, const ANSICHAR* Specifier) const
{
	const float* Result = FloatValues.Find(Join(PropertyName, Specifier));

#if DO_GUARD_SLOW
	if ((Result == nullptr) && !MissingResources.Contains(PropertyName))
	{
		MissingResources.Add(PropertyName);
		Log(ISlateStyle::Warning, FText::Format(NSLOCTEXT("SlateStyleSet", "UknownSlateFloat", "Unable to find float property '{0}' in style."), FText::FromName(PropertyName)));
	}
#endif

	return Result ? *Result : FStyleDefaults::GetFloat();
}

FVector2D FSlateStyleSet::GetVector(const FName PropertyName, const ANSICHAR* Specifier) const
{
	const FVector2D* const Result = Vector2DValues.Find(Join(PropertyName, Specifier));

#if DO_GUARD_SLOW
	if ((Result == nullptr) && !MissingResources.Contains(PropertyName))
	{
		MissingResources.Add(PropertyName);
		Log(ISlateStyle::Warning, FText::Format(NSLOCTEXT("SlateStyleSet", "UknownSlateVector", "Unable to find vector property '{0}' in style."), FText::FromName(PropertyName)));
	}
#endif

	return Result ? *Result : FStyleDefaults::GetVector2D();
}

const FLinearColor& FSlateStyleSet::GetColor(const FName PropertyName, const ANSICHAR* Specifier) const
{
	const FName LookupName(Join(PropertyName, Specifier));
	const FLinearColor* Result = ColorValues.Find(LookupName);

#if DO_GUARD_SLOW
	if ( Result == nullptr && !MissingResources.Contains(LookupName) )
	{
		MissingResources.Add(LookupName);
		Log(ISlateStyle::Warning, FText::Format(NSLOCTEXT("SlateStyleSet", "UknownColor", "Unable to find Color '{0}'."), FText::FromName(LookupName)));
	}
#endif

	return Result ? *Result : FStyleDefaults::GetColor();
}

const FSlateColor FSlateStyleSet::GetSlateColor(const FName PropertyName, const ANSICHAR* Specifier) const
{
	static FSlateColor UseForegroundStatic = FSlateColor::UseForeground();

	const FName StyleName(Join(PropertyName, Specifier));
	const FSlateColor* Result = SlateColorValues.Find(StyleName);

	if ( Result == nullptr )
	{
		const FLinearColor* LinearColorLookup = ColorValues.Find(StyleName);
		if ( LinearColorLookup == nullptr )
		{
			return UseForegroundStatic;
		}

		return *LinearColorLookup;
	}

#if DO_GUARD_SLOW
	if ( Result == nullptr && !MissingResources.Contains(StyleName) )
	{
		MissingResources.Add(StyleName);
		Log(ISlateStyle::Warning, FText::Format(NSLOCTEXT("SlateStyleSet", "UknownSlateColor", "Unable to find SlateColor '{0}'."), FText::FromName(StyleName)));
	}
#endif

	return *Result;
}

const FMargin& FSlateStyleSet::GetMargin(const FName PropertyName, const ANSICHAR* Specifier) const
{
	const FName StyleName(Join(PropertyName, Specifier));
	const FMargin* const Result = MarginValues.Find(StyleName);

#if DO_GUARD_SLOW
	if ( Result == nullptr && !MissingResources.Contains(StyleName) )
	{
		MissingResources.Add(StyleName);
		Log(ISlateStyle::Warning, FText::Format(NSLOCTEXT("SlateStyleSet", "UknownMargin", "Unable to find Margin '{0}'."), FText::FromName(StyleName)));
	}
#endif

	return Result ? *Result : FStyleDefaults::GetMargin();
}

const FSlateBrush* FSlateStyleSet::GetBrush(const FName PropertyName, const ANSICHAR* Specifier) const
{
	const FName StyleName = Join(PropertyName, Specifier);
	const FSlateBrush* Result = BrushResources.FindRef(StyleName);

	if ( Result == nullptr )
	{
		TWeakPtr< FSlateDynamicImageBrush > WeakImageBrush = DynamicBrushes.FindRef(StyleName);
		TSharedPtr< FSlateDynamicImageBrush > ImageBrush = WeakImageBrush.Pin();

		if ( ImageBrush.IsValid() )
		{
			Result = ImageBrush.Get();
		}
	}

#if DO_GUARD_SLOW
	if ( Result == nullptr && !MissingResources.Contains(StyleName))
	{
		MissingResources.Add(StyleName);
		Log(ISlateStyle::Warning, FText::Format(NSLOCTEXT("SlateStyleSet", "UknownBrush", "Unable to find Brush '{0}'."), FText::FromName(StyleName)));
	}
#endif

	return Result ? Result : GetDefaultBrush();
}

const FSlateBrush* FSlateStyleSet::GetOptionalBrush(const FName PropertyName, const ANSICHAR* Specifier, const FSlateBrush* const InDefaultBrush) const
{
	const FName StyleName = Join(PropertyName, Specifier);
	const FSlateBrush* Result = BrushResources.FindRef(StyleName);

	if ( Result == nullptr )
	{
		TWeakPtr< FSlateDynamicImageBrush > WeakImageBrush = DynamicBrushes.FindRef(StyleName);
		TSharedPtr< FSlateDynamicImageBrush > ImageBrush = WeakImageBrush.Pin();

		if ( ImageBrush.IsValid() )
		{
			Result = ImageBrush.Get();
		}
	}

	return Result ? Result : InDefaultBrush;
}

const TSharedPtr< FSlateDynamicImageBrush > FSlateStyleSet::GetDynamicImageBrush(const FName BrushTemplate, const FName TextureName, const ANSICHAR* Specifier)
{
	return GetDynamicImageBrush(BrushTemplate, Specifier, nullptr, TextureName);
}

const TSharedPtr< FSlateDynamicImageBrush > FSlateStyleSet::GetDynamicImageBrush(const FName BrushTemplate, const ANSICHAR* Specifier, UTexture2D* TextureResource, const FName TextureName)
{
	return GetDynamicImageBrush(Join(BrushTemplate, Specifier), TextureResource, TextureName);
}

const TSharedPtr< FSlateDynamicImageBrush > FSlateStyleSet::GetDynamicImageBrush(const FName BrushTemplate, UTexture2D* TextureResource, const FName TextureName)
{
	//create a resource name
	FName ResourceName;
	ResourceName = TextureName == NAME_None ? BrushTemplate : FName(*( BrushTemplate.ToString() + TextureName.ToString() ));

	//see if we already have that brush
	TWeakPtr< FSlateDynamicImageBrush > WeakImageBrush = DynamicBrushes.FindRef(ResourceName);

	//if we don't have the image brush, then make it
	TSharedPtr< FSlateDynamicImageBrush > ReturnBrush = WeakImageBrush.Pin();

	if ( !ReturnBrush.IsValid() )
	{
		const FSlateBrush* Result = BrushResources.FindRef(Join(BrushTemplate, nullptr));

		if ( Result == nullptr )
		{
			Result = GetDefaultBrush();
		}

		//create the new brush
		ReturnBrush = MakeShareable(new FSlateDynamicImageBrush(TextureResource, Result->ImageSize, ResourceName));

		//add it to the dynamic brush list
		DynamicBrushes.Add(ResourceName, ReturnBrush);
	}

	return ReturnBrush;
}

FSlateBrush* FSlateStyleSet::GetDefaultBrush() const
{
	return DefaultBrush;
}

const FSlateSound& FSlateStyleSet::GetSound(const FName PropertyName, const ANSICHAR* Specifier) const
{
	const FName StyleName = Join(PropertyName, Specifier);
	const FSlateSound* Result = Sounds.Find(StyleName);

#if DO_GUARD_SLOW
	if ( Result == nullptr && !MissingResources.Contains(StyleName) )
	{
		MissingResources.Add(StyleName);
		Log(ISlateStyle::Warning, FText::Format(NSLOCTEXT("SlateStyleSet", "UknownSound", "Unable to find Sound '{0}'."), FText::FromName(StyleName)));
	}
#endif

	return Result ? *Result : FStyleDefaults::GetSound();
}

FSlateFontInfo FSlateStyleSet::GetFontStyle(const FName PropertyName, const ANSICHAR* Specifier) const
{
	const FSlateFontInfo* Result = FontInfoResources.Find(Join(PropertyName, Specifier));

	return Result ? *Result : FStyleDefaults::GetFontInfo();
}

const FSlateWidgetStyle* FSlateStyleSet::GetWidgetStyleInternal(const FName DesiredTypeName, const FName StyleName) const
{
	const TSharedRef< struct FSlateWidgetStyle >* StylePtr = WidgetStyleValues.Find(StyleName);

	if ( StylePtr == nullptr )
	{
		Log(ISlateStyle::Warning, FText::Format(NSLOCTEXT("SlateStyleSet", "UnknownWidgetStyle", "Unable to find Slate Widget Style '{0}'. Using {1} defaults instead."), FText::FromName(StyleName), FText::FromName(DesiredTypeName)));
		return nullptr;
	}

	const TSharedRef< struct FSlateWidgetStyle > Style = *StylePtr;

	if ( Style->GetTypeName() != DesiredTypeName )
	{
		Log(ISlateStyle::Error, FText::Format(NSLOCTEXT("SlateStyleSet", "WrongWidgetStyleType", "The Slate Widget Style '{0}' is not of the desired type. Desired: '{1}', Actual: '{2}'"), FText::FromName(StyleName), FText::FromName(DesiredTypeName), FText::FromName(Style->GetTypeName())));
		return nullptr;
	}

	return &Style.Get();
}

void FSlateStyleSet::Log(ISlateStyle::EStyleMessageSeverity Severity, const FText& Message) const
{
	if ( Severity == ISlateStyle::Error )
	{
		UE_LOG(LogSlateStyle, Error, TEXT("%s"), *Message.ToString());
	}
	else if ( Severity == ISlateStyle::PerformanceWarning )
	{
		UE_LOG(LogSlateStyle, Warning, TEXT("%s"), *Message.ToString());
	}
	else if ( Severity == ISlateStyle::Warning )
	{
		UE_LOG(LogSlateStyle, Warning, TEXT("%s"), *Message.ToString());
	}
	else if ( Severity == ISlateStyle::Info )
	{
		UE_LOG(LogSlateStyle, Log, TEXT("%s"), *Message.ToString());
	}
	else
	{
		UE_LOG(LogSlateStyle, Fatal, TEXT("%s"), *Message.ToString());
	}
}

void FSlateStyleSet::LogUnusedBrushResources()
{
	TArray<FString> Filenames;
	IFileManager::Get().FindFilesRecursive(Filenames, *ContentRootDir, *FString("*.png"), true, false, false);

	for ( FString& FilePath : Filenames )
	{
		bool IsUsed = false;
		for ( auto& Entry : BrushResources )
		{
			if ( IsBrushFromFile(FilePath, Entry.Value) )
			{
				IsUsed = true;
				break;
			}
		}

		if ( !IsUsed )
		{
			for ( auto& Entry : WidgetStyleValues )
			{
				TArray< const FSlateBrush* > WidgetBrushes;
				Entry.Value->GetResources(WidgetBrushes);

				for ( const FSlateBrush* Brush : WidgetBrushes )
				{
					if ( IsBrushFromFile(FilePath, Brush) )
					{
						IsUsed = true;
						break;
					}
				}

				if ( IsUsed )
				{
					break;
				}
			}
		}

		if ( !IsUsed )
		{
			Log(ISlateStyle::Warning, FText::FromString(FilePath));
		}
	}
}

bool FSlateStyleSet::IsBrushFromFile(const FString& FilePath, const FSlateBrush* Brush)
{
	FString Path = Brush->GetResourceName().ToString();
	FPaths::MakeStandardFilename(Path);
	if ( Path.Compare(FilePath, ESearchCase::IgnoreCase) == 0 )
	{
		return true;
	}
	else
	{
		const FString FullFilePath = FPaths::ConvertRelativePathToFull(FilePath);
		const FString FullPath = FPaths::ConvertRelativePathToFull(Path);
		if (FullPath.Compare(FullFilePath, ESearchCase::IgnoreCase) == 0)
		{
			return true;
		}
	}

	return false;
}
