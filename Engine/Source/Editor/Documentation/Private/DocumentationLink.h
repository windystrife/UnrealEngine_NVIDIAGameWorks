// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IDocumentation.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Internationalization/Culture.h"
#include "UnrealEdMisc.h"

class FDocumentationLink
{
public: 

	static FString GetUrlRoot()
	{
		FString Url;
		FUnrealEdMisc::Get().GetURL( TEXT("UDNDocsURL"), Url, true );

		if ( !Url.EndsWith( TEXT( "/" ) ) )
		{
			Url += TEXT( "/" );
		}

		return Url;
	}

	static FString GetHomeUrl()
	{
		return GetHomeUrl(FInternationalization::Get().GetCurrentCulture());
	}

	static FString GetHomeUrl(const FCultureRef& Culture)
	{
		FString Url;
		FUnrealEdMisc::Get().GetURL( TEXT("UDNURL"), Url, true );

		Url.ReplaceInline(TEXT("/INT/"), *FString::Printf(TEXT("/%s/"), *(Culture->GetUnrealLegacyThreeLetterISOLanguageName())));
		return Url;
	}

	static FString ToUrl(const FString& Link, FDocumentationSourceInfo const& Source)
	{
		return ToUrl(Link, FInternationalization::Get().GetCurrentCulture(), Source);
	}

	static FString ToUrl(const FString& Link, const FCultureRef& Culture, FDocumentationSourceInfo const& Source)
	{
		FString Path;
		FString Anchor;
		FString QueryString;
		SplitLink(Link, Path, QueryString, Anchor);

		const FString PartialPath = FString::Printf(TEXT("%s%s/index.html"), *(Culture->GetUnrealLegacyThreeLetterISOLanguageName()), *Path);
		
		AddSourceInfoToQueryString(QueryString, Source);

		return GetUrlRoot() + PartialPath + QueryString + Anchor;
	}

	static FString ToFilePath( const FString& Link )
	{
		FInternationalization& I18N = FInternationalization::Get();

		FString FilePath = ToFilePath(Link, I18N.GetCurrentCulture());

		if (!FPaths::FileExists(FilePath))
		{
			const FCulturePtr FallbackCulture = I18N.GetCulture(TEXT("en"));
			if (FallbackCulture.IsValid())
			{
				const FString FallbackFilePath = ToFilePath(Link, FallbackCulture.ToSharedRef());
				if (FPaths::FileExists(FallbackFilePath))
				{
					FilePath = FallbackFilePath;
				}
			}
		}

		return FilePath;
	}

	static FString ToFilePath(const FString& Link, const FCultureRef& Culture)
	{
		FString Path;
		FString Anchor;
		FString QueryString;
		SplitLink(Link, Path, QueryString, Anchor);

		const FString PartialPath = FString::Printf(TEXT("%s%s/index.html"), *(Culture->GetUnrealLegacyThreeLetterISOLanguageName()), *Path);
		return FString::Printf(TEXT("%sDocumentation/HTML/%s"), *FPaths::ConvertRelativePathToFull(FPaths::EngineDir()), *PartialPath);
	}

	static FString ToFileUrl(const FString& Link, FDocumentationSourceInfo const& SourceInfo)
	{
		FInternationalization& I18N = FInternationalization::Get();

		FCultureRef Culture = I18N.GetCurrentCulture();
		FString FilePath = ToFilePath(Link, Culture);

		if (!FPaths::FileExists(FilePath))
		{
			const FCulturePtr FallbackCulture = I18N.GetCulture(TEXT("en"));
			if (FallbackCulture.IsValid())
			{
				const FString FallbackFilePath = ToFilePath(Link, FallbackCulture.ToSharedRef());
				if (FPaths::FileExists(FallbackFilePath))
				{
					Culture = FallbackCulture.ToSharedRef();
				}
			}
		}

		return ToFileUrl(Link, Culture, SourceInfo);
	}

	static FString ToFileUrl(const FString& Link, const FCultureRef& Culture, FDocumentationSourceInfo const& SourceInfo)
	{
		FString Path;
		FString Anchor;
		FString QueryString;
		SplitLink(Link, Path, QueryString, Anchor);

		AddSourceInfoToQueryString(QueryString, SourceInfo);

		return FString::Printf(TEXT("file:///%s%s%s"), *ToFilePath(Link, Culture), *QueryString, *Anchor);
	}

	static FString ToSourcePath(const FString& Link)
	{
		FInternationalization& I18N = FInternationalization::Get();

		FString SourcePath = ToSourcePath(Link, I18N.GetCurrentCulture());

		if (!FPaths::FileExists(SourcePath))
		{
			const FCulturePtr FallbackCulture = I18N.GetCulture(TEXT("en"));
			if (FallbackCulture.IsValid())
			{
				const FString FallbackSourcePath = ToSourcePath(Link, FallbackCulture.ToSharedRef());
				if (FPaths::FileExists(FallbackSourcePath))
				{
					SourcePath = FallbackSourcePath;
				}
			}
		}

		return SourcePath;
	}

	static FString ToSourcePath(const FString& Link, const FCultureRef& Culture)
	{
		FString Path;
		FString Anchor;
		FString QueryString;
		SplitLink(Link, Path, QueryString, Anchor);

		const FString FullDirectoryPath = FPaths::EngineDir() + TEXT( "Documentation/Source" ) + Path + "/";

		const FString WildCard = FString::Printf(TEXT("%s*.%s.udn"), *FullDirectoryPath, *(Culture->GetUnrealLegacyThreeLetterISOLanguageName()));

		TArray<FString> Filenames;
		IFileManager::Get().FindFiles(Filenames, *WildCard, true, false);

		if (Filenames.Num() > 0)
		{
			return FullDirectoryPath + Filenames[0];
		}

		// Since the source file doesn't exist already make up a valid name for a new one
		FString Category = FPaths::GetBaseFilename(Link);
		return FString::Printf(TEXT("%s%s.%s.udn"), *FullDirectoryPath, *Category, *(Culture->GetUnrealLegacyThreeLetterISOLanguageName()));
	}

private:
	static void AddSourceInfoToQueryString(FString& QueryString, FDocumentationSourceInfo const& Info)
	{
		if (Info.IsEmpty() == false)
		{
			if (QueryString.IsEmpty())
			{
				QueryString = FString::Printf(TEXT("?utm_source=%s&utm_medium=%s&utm_campaign=%s"), *Info.Source, *Info.Medium, *Info.Campaign);
			}
			else
			{
				QueryString = FString::Printf(TEXT("%s&utm_source=%s&utm_medium=%s&utm_campaign=%s"), *QueryString, *Info.Source, *Info.Medium, *Info.Campaign);
			}
		}
	}
	
	static void SplitLink( const FString& Link, /*OUT*/ FString& Path, /*OUT*/ FString& QueryString, /*OUT*/ FString& Anchor )
	{
		FString CleanedLink = Link;
		CleanedLink.TrimStartAndEndInline();

		if ( CleanedLink == TEXT("%ROOT%") )
		{
			Path.Empty();
			Anchor.Empty();
			QueryString.Empty();
		}
		else
		{
			FString PathAndQueryString;
			if ( !CleanedLink.Split( TEXT("#"), &PathAndQueryString, &Anchor ) )
			{
				PathAndQueryString = CleanedLink;
			}
			else if ( !Anchor.IsEmpty() )
			{
				// ensure leading #
				Anchor = FString( TEXT("#") ) + Anchor;
			}

			if ( Anchor.EndsWith( TEXT("/") ) )
			{
				Anchor = Anchor.Left( Anchor.Len() - 1 );
			}

			if ( PathAndQueryString.EndsWith( TEXT("/") ) )
			{
				PathAndQueryString = PathAndQueryString.Left(PathAndQueryString.Len() - 1);
			}

			if ( !PathAndQueryString.IsEmpty() && !PathAndQueryString.StartsWith( TEXT("/") ) )
			{
				PathAndQueryString = FString(TEXT("/")) + PathAndQueryString;
			}

			// split path and query string
			if (!PathAndQueryString.Split(TEXT("?"), &Path, &QueryString))
			{
				Path = PathAndQueryString;
			}
			else if (!QueryString.IsEmpty())
			{
				// ensure leading ?
				QueryString = FString(TEXT("?")) + QueryString;
			}
		}
	}
};
