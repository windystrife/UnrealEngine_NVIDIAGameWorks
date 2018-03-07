// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
TextureInstanceManager.h: Definitions of classes used for texture streaming.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Streaming/TextureInstanceState.h"

/** An interface to manage the TextureInstanceState from a group of components with similar properties, allowing to add/remove components incrementally and update their content. */
class ITextureInstanceManager
{
public:

	virtual ~ITextureInstanceManager() {}

	/** Return whether the component is referenced. */
	virtual bool IsReferenced(const UPrimitiveComponent* Component) const = 0;

	/** Return whether this component is be managed by this manager. */
	virtual bool CanManage(const UPrimitiveComponent* Component) const = 0;

	/** Refresh component data (bounds, last render time, min and max view distance) - see TextureInstanceView. */
	virtual void Refresh(float Percentage) = 0;

	/** Add a component streaming data, the LevelContext gives support for precompiled data. */
	virtual bool Add(const UPrimitiveComponent* Component, FStreamingTextureLevelContext& LevelContext) = 0;

	/** Remove a component, the RemoveTextures is the list of textures not referred anymore. */
	virtual void Remove(const UPrimitiveComponent* Component, FRemovedTextureArray& RemovedTextures) = 0;

	/** Notify the manager that an async view will be requested on the next frame. */
	virtual void PrepareAsyncView() = 0;

	/** Return a view of the data that has to be 100% thread safe. The content is allowed to be updated, but not memory must be reallocated. */
	virtual const FTextureInstanceView* GetAsyncView(bool bCreateIfNull) = 0;

	/** Return the size taken for sub-allocation. */
	virtual uint32 GetAllocatedSize() const = 0;
};
