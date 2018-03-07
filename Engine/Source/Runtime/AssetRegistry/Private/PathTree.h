// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FPathTree
{
public:
	/** Adds the path to the tree relative to this node, creating nodes as needed. Returns true if the specified path was actually added (as opposed to already existed) */
	bool CachePath(FName Path);

	/** Removes the specified path in the tree relative to this node. Returns true if the path was found and removed. */
	bool RemovePath(FName Path);

	/** Get all of the paths we know about */
	bool GetAllPaths(TSet<FName>& OutPaths) const;

	/** Recursively gathers all child paths from the specified base path relative to this node */
	bool GetSubPaths(FName BasePath, TSet<FName>& OutPaths, bool bRecurse = true) const;

	uint32 GetAllocatedSize(void) const
	{
		return ParentPathToChildPaths.GetAllocatedSize() + ChildPathToParentPath.GetAllocatedSize();
	}

private:
	/** A one-to-many mapping between a parent path and its child paths. */
	TMap<FName, TSet<FName>> ParentPathToChildPaths;

	/** A one-to-one mapping between a child path and its parent path. Paths without a parent (root paths) will not appear in this map. */
	TMap<FName, FName> ChildPathToParentPath;
};
