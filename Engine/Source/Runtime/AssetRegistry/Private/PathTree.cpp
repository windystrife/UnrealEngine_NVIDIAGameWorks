// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PathTree.h"

bool FPathTree::CachePath(FName Path)
{
	if (Path.IsNone())
	{
		return false;
	}

	if (ParentPathToChildPaths.Contains(Path))
	{
		// Already cached - nothing more to do
		return false;
	}

	FString PathStr = Path.ToString();
	check(PathStr.Len() >= 2);	// Must be at least "/A"
	check(PathStr[0] == '/');	// Must start with a "/"

	// Paths are cached without their trailing slash, so if the given path has a trailing slash, test it again now as it may already be cached
	if (PathStr[PathStr.Len() - 1] == '/')
	{
		PathStr.RemoveAt(PathStr.Len() - 1, 1, /*bAllowShrinking*/false);
		Path = *PathStr;

		if (ParentPathToChildPaths.Contains(Path))
		{
			// Already cached - nothing more to do
			return false;
		}
	}

	FName LastPath;

	// Ensure an entry for the root of the path
	{
		static const FName PathRoot = "/";

		if (!ParentPathToChildPaths.Contains(PathRoot))
		{
			ParentPathToChildPaths.Add(PathRoot);
		}

		LastPath = PathRoot;
	}

	// Walk each part of the path, adding known path entries if required
	// This manipulates PathStr in-place to avoid making any string copies
	TCHAR* PathCharPtr = &PathStr[1]; // Skip over the first / when scanning
	for (;;)
	{
		const TCHAR PathChar = *PathCharPtr;
		if (PathChar == '/' || PathChar == 0)
		{
			// We've found a path separator (or the end of the string), so process this part of the path
			(*PathCharPtr) = 0;			// Null terminate this part of the string so we can create an FName from it
			const FName CurrentPath = *PathStr;
			(*PathCharPtr) = PathChar;	// Restore the original character now

			check(!CurrentPath.IsNone());	// Path parts cannot be empty
			check(*(PathCharPtr-1) != '/'); // The previous character cannot be a /, as that would suggest a malformed path such as "/Game//MyAsset"

			if (!ParentPathToChildPaths.Contains(CurrentPath))
			{
				ParentPathToChildPaths.Add(CurrentPath);
			}

			if (!LastPath.IsNone())
			{
				// Add us as a known child of our parent path
				TSet<FName>& ChildPaths = ParentPathToChildPaths.FindChecked(LastPath);
				ChildPaths.Add(CurrentPath);

				// Make sure we know how to find our parent again later on
				ChildPathToParentPath.Add(CurrentPath, LastPath);
			}

			LastPath = CurrentPath;
		}

		if (PathChar == 0)
		{
			// End of the string
			break;
		}

		++PathCharPtr;
	}

	return true;
}

bool FPathTree::RemovePath(FName Path)
{
	if (Path.IsNone())
	{
		return false;
	}

	if (!ParentPathToChildPaths.Contains(Path))
	{
		// Paths are cached without their trailing slash, so if the given path has a trailing slash, test it again now as it may already be cached
		// We do this after the initial map test as: a) Most paths are well formed, b) This avoids string allocations until we know we need them
		FString PathStr = Path.ToString();
		if (PathStr[PathStr.Len() - 1] == '/')
		{
			PathStr.RemoveAt(PathStr.Len() - 1, 1, /*bAllowShrinking*/false);
			Path = *PathStr;

			if (!ParentPathToChildPaths.Contains(Path))
			{
				// Doesn't exist - nothing more to do
				return false;
			}
		}
		else
		{
			// Doesn't exist - nothing more to do
			return false;
		}
	}

	// We also need to gather up and remove any children of this path
	TSet<FName> SubPathsToRemove;
	GetSubPaths(Path, SubPathsToRemove, /*bRecurse=*/true);

	// Simply remove sub-paths from both maps
	for (const FName& SubPathToRemove : SubPathsToRemove)
	{
		ParentPathToChildPaths.Remove(SubPathToRemove);
		ChildPathToParentPath.Remove(SubPathToRemove);
	}

	// We also need to remove ourself from our parent list before removing ourself from the maps
	{
		const FName* ParentPathPtr = ChildPathToParentPath.Find(Path);
		if (ParentPathPtr)
		{
			TSet<FName>& ChildPaths = ParentPathToChildPaths.FindChecked(*ParentPathPtr);
			ChildPaths.Remove(Path);
		}
	}

	ParentPathToChildPaths.Remove(Path);
	ChildPathToParentPath.Remove(Path);

	return true;
}

bool FPathTree::GetAllPaths(TSet<FName>& OutPaths) const
{
	OutPaths.Reset();
	OutPaths.Reserve(ParentPathToChildPaths.Num());

	for (const auto& PathPair : ParentPathToChildPaths)
	{
		OutPaths.Add(PathPair.Key);
	}

	return OutPaths.Num() > 0;
}

bool FPathTree::GetSubPaths(FName BasePath, TSet<FName>& OutPaths, bool bRecurse) const
{
	if (BasePath.IsNone())
	{
		return false;
	}

	const TSet<FName>* ChildPathsPtr = ParentPathToChildPaths.Find(BasePath);
	if (!ChildPathsPtr)
	{
		// Paths are cached without their trailing slash, so if the given path has a trailing slash, test it again now as it may already be cached
		// We do this after the initial map test as: a) Most paths are well formed, b) This avoids string allocations until we know we need them
		FString BasePathStr = BasePath.ToString();
		if (BasePathStr[BasePathStr.Len() - 1] == '/')
		{
			BasePathStr.RemoveAt(BasePathStr.Len() - 1, 1, /*bAllowShrinking*/false);
			BasePath = *BasePathStr;

			ChildPathsPtr = ParentPathToChildPaths.Find(BasePath);
			if (!ChildPathsPtr)
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	const int32 OutPathsOriginalNum = OutPaths.Num();

	for (const FName& ChildPath : *ChildPathsPtr)
	{
		check(ParentPathToChildPaths.Contains(ChildPath)); // This failing is an integrity violation as this entry lists a child that we don't know about

		OutPaths.Add(ChildPath);

		if (bRecurse)
		{
			GetSubPaths(ChildPath, OutPaths, /*bRecurse=*/true);
		}
	}

	return OutPaths.Num() > OutPathsOriginalNum;
}
