// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/UnrealString.h"
#include "Misc/Paths.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPathTests, "System.Core.Misc.Paths", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

// MSVC 2015 generates bad code here due to the number of string literals, in a cooked build it was crashing on RunCollapseRelativeDirectoriesTest(TEXT("C:/."), TEXT("C:/."));
PRAGMA_DISABLE_OPTIMIZATION

bool FPathTests::RunTest( const FString& Parameters )
{
	// Directory collapsing
	{
		auto RunCollapseRelativeDirectoriesTest = [this](const FString& InPath, const TCHAR* InResult)
		{
			// Run test
			FString CollapsedPath = InPath;
			const bool bValid = FPaths::CollapseRelativeDirectories(CollapsedPath);

			if (InResult)
			{
				// If we're looking for a result, make sure it was returned correctly
				if (!bValid || CollapsedPath != InResult)
				{
					AddError(FString::Printf(TEXT("Path '%s' failed to collapse correctly (got '%s', expected '%s')."), *InPath, *CollapsedPath, InResult));
				}
			}
			else
			{
				// Otherwise, make sure it failed
				if (bValid)
				{
					AddError(FString::Printf(TEXT("Path '%s' collapsed unexpectedly."), *InPath));
				}
			}
		};

		RunCollapseRelativeDirectoriesTest(TEXT(".."),                                                   NULL);
		RunCollapseRelativeDirectoriesTest(TEXT("/.."),                                                  NULL);
		RunCollapseRelativeDirectoriesTest(TEXT("./"),                                                   TEXT(""));
		RunCollapseRelativeDirectoriesTest(TEXT("./file.txt"),                                           TEXT("file.txt"));
		RunCollapseRelativeDirectoriesTest(TEXT("/."),                                                   TEXT("/."));
		RunCollapseRelativeDirectoriesTest(TEXT("Folder"),                                               TEXT("Folder"));
		RunCollapseRelativeDirectoriesTest(TEXT("/Folder"),                                              TEXT("/Folder"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder"),                                            TEXT("C:/Folder"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder/.."),                                         TEXT("C:"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder/../"),                                        TEXT("C:/"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder/../file.txt"),                                TEXT("C:/file.txt"));
		RunCollapseRelativeDirectoriesTest(TEXT("Folder/.."),                                            TEXT(""));
		RunCollapseRelativeDirectoriesTest(TEXT("Folder/../"),                                           TEXT("/"));
		RunCollapseRelativeDirectoriesTest(TEXT("Folder/../file.txt"),                                   TEXT("/file.txt"));
		RunCollapseRelativeDirectoriesTest(TEXT("/Folder/.."),                                           TEXT(""));
		RunCollapseRelativeDirectoriesTest(TEXT("/Folder/../"),                                          TEXT("/"));
		RunCollapseRelativeDirectoriesTest(TEXT("/Folder/../file.txt"),                                  TEXT("/file.txt"));
		RunCollapseRelativeDirectoriesTest(TEXT("Folder/../.."),                                         NULL);
		RunCollapseRelativeDirectoriesTest(TEXT("Folder/../../"),                                        NULL);
		RunCollapseRelativeDirectoriesTest(TEXT("Folder/../../file.txt"),                                NULL);
		RunCollapseRelativeDirectoriesTest(TEXT("C:/.."),                                                NULL);
		RunCollapseRelativeDirectoriesTest(TEXT("C:/."),                                                 TEXT("C:/."));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/./"),                                                TEXT("C:/"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/./file.txt"),                                        TEXT("C:/file.txt"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder1/../Folder2"),                                TEXT("C:/Folder2"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder1/../Folder2/"),                               TEXT("C:/Folder2/"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder1/../Folder2/file.txt"),                       TEXT("C:/Folder2/file.txt"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder1/../Folder2/../.."),                          NULL);
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder1/../Folder2/../Folder3"),                     TEXT("C:/Folder3"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder1/../Folder2/../Folder3/"),                    TEXT("C:/Folder3/"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder1/../Folder2/../Folder3/file.txt"),            TEXT("C:/Folder3/file.txt"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder1/Folder2/../../Folder3"),                     TEXT("C:/Folder3"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder1/Folder2/../../Folder3/"),                    TEXT("C:/Folder3/"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder1/Folder2/../../Folder3/file.txt"),            TEXT("C:/Folder3/file.txt"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder1/Folder2/../../Folder3/../Folder4"),          TEXT("C:/Folder4"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder1/Folder2/../../Folder3/../Folder4/"),         TEXT("C:/Folder4/"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder1/Folder2/../../Folder3/../Folder4/file.txt"), TEXT("C:/Folder4/file.txt"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder1/Folder2/../Folder3/../../Folder4"),          TEXT("C:/Folder4"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder1/Folder2/../Folder3/../../Folder4/"),         TEXT("C:/Folder4/"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder1/Folder2/../Folder3/../../Folder4/file.txt"), TEXT("C:/Folder4/file.txt"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder1/Folder2/.././../Folder4"),                   TEXT("C:/Folder4"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder1/Folder2/.././../Folder4/"),                  TEXT("C:/Folder4/"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/Folder1/Folder2/.././../Folder4/file.txt"),          TEXT("C:/Folder4/file.txt"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/A/B/.././../C"),                                     TEXT("C:/C"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/A/B/.././../C/"),                                    TEXT("C:/C/"));
		RunCollapseRelativeDirectoriesTest(TEXT("C:/A/B/.././../C/file.txt"),                            TEXT("C:/C/file.txt"));
		RunCollapseRelativeDirectoriesTest(TEXT(".svn"),                                                 TEXT(".svn"));
		RunCollapseRelativeDirectoriesTest(TEXT("/.svn"),                                                TEXT("/.svn"));
		RunCollapseRelativeDirectoriesTest(TEXT("./Folder/.svn"),                                        TEXT("Folder/.svn"));
		RunCollapseRelativeDirectoriesTest(TEXT("./.svn/../.svn"),                                       TEXT(".svn"));
		RunCollapseRelativeDirectoriesTest(TEXT(".svn/./.svn/.././../.svn"),                             TEXT("/.svn"));
	}

	// Extension texts
	{
		auto RunGetExtensionTest = [this](const FString& InPath, const FString& InExpectedExt)
		{
			// Run test
			const FString Ext = FPaths::GetExtension(InPath);
			if (Ext != InExpectedExt)
			{
				AddError(FString::Printf(TEXT("Path '%s' failed to get the extension (got '%s', expected '%s')."), *InPath, *Ext, *InExpectedExt));
			}
		};

		RunGetExtensionTest(TEXT("file"),									TEXT(""));
		RunGetExtensionTest(TEXT("file.txt"),								TEXT("txt"));
		RunGetExtensionTest(TEXT("file.tar.gz"),							TEXT("gz"));
		RunGetExtensionTest(TEXT("C:/Folder/file"),							TEXT(""));
		RunGetExtensionTest(TEXT("C:/Folder/file.txt"),						TEXT("txt"));
		RunGetExtensionTest(TEXT("C:/Folder/file.tar.gz"),					TEXT("gz"));
		RunGetExtensionTest(TEXT("C:/Folder/First.Last/file"),				TEXT(""));
		RunGetExtensionTest(TEXT("C:/Folder/First.Last/file.txt"),			TEXT("txt"));
		RunGetExtensionTest(TEXT("C:/Folder/First.Last/file.tar.gz"),		TEXT("gz"));

		auto RunSetExtensionTest = [this](const FString& InPath, const FString& InNewExt, const FString& InExpectedPath)
		{
			// Run test
			const FString NewPath = FPaths::SetExtension(InPath, InNewExt);
			if (NewPath != InExpectedPath)
			{
				AddError(FString::Printf(TEXT("Path '%s' failed to set the extension (got '%s', expected '%s')."), *InPath, *NewPath, *InExpectedPath));
			}
		};

		RunSetExtensionTest(TEXT("file"),									TEXT("log"),	TEXT("file.log"));
		RunSetExtensionTest(TEXT("file.txt"),								TEXT("log"),	TEXT("file.log"));
		RunSetExtensionTest(TEXT("file.tar.gz"),							TEXT("gz2"),	TEXT("file.tar.gz2"));
		RunSetExtensionTest(TEXT("C:/Folder/file"),							TEXT("log"),	TEXT("C:/Folder/file.log"));
		RunSetExtensionTest(TEXT("C:/Folder/file.txt"),						TEXT("log"),	TEXT("C:/Folder/file.log"));
		RunSetExtensionTest(TEXT("C:/Folder/file.tar.gz"),					TEXT("gz2"),	TEXT("C:/Folder/file.tar.gz2"));
		RunSetExtensionTest(TEXT("C:/Folder/First.Last/file"),				TEXT("log"),	TEXT("C:/Folder/First.Last/file.log"));
		RunSetExtensionTest(TEXT("C:/Folder/First.Last/file.txt"),			TEXT("log"),	TEXT("C:/Folder/First.Last/file.log"));
		RunSetExtensionTest(TEXT("C:/Folder/First.Last/file.tar.gz"),		TEXT("gz2"),	TEXT("C:/Folder/First.Last/file.tar.gz2"));

		auto RunChangeExtensionTest = [this](const FString& InPath, const FString& InNewExt, const FString& InExpectedPath)
		{
			// Run test
			const FString NewPath = FPaths::ChangeExtension(InPath, InNewExt);
			if (NewPath != InExpectedPath)
			{
				AddError(FString::Printf(TEXT("Path '%s' failed to change the extension (got '%s', expected '%s')."), *InPath, *NewPath, *InExpectedPath));
			}
		};

		RunChangeExtensionTest(TEXT("file"),								TEXT("log"),	TEXT("file"));
		RunChangeExtensionTest(TEXT("file.txt"),							TEXT("log"),	TEXT("file.log"));
		RunChangeExtensionTest(TEXT("file.tar.gz"),							TEXT("gz2"),	TEXT("file.tar.gz2"));
		RunChangeExtensionTest(TEXT("C:/Folder/file"),						TEXT("log"),	TEXT("C:/Folder/file"));
		RunChangeExtensionTest(TEXT("C:/Folder/file.txt"),					TEXT("log"),	TEXT("C:/Folder/file.log"));
		RunChangeExtensionTest(TEXT("C:/Folder/file.tar.gz"),				TEXT("gz2"),	TEXT("C:/Folder/file.tar.gz2"));
		RunChangeExtensionTest(TEXT("C:/Folder/First.Last/file"),			TEXT("log"),	TEXT("C:/Folder/First.Last/file"));
		RunChangeExtensionTest(TEXT("C:/Folder/First.Last/file.txt"),		TEXT("log"),	TEXT("C:/Folder/First.Last/file.log"));
		RunChangeExtensionTest(TEXT("C:/Folder/First.Last/file.tar.gz"),	TEXT("gz2"),	TEXT("C:/Folder/First.Last/file.tar.gz2"));
	}

	return true;
}

PRAGMA_ENABLE_OPTIMIZATION

#endif //WITH_DEV_AUTOMATION_TESTS
