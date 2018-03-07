// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Editor/ClassViewer/Private/ClassViewerNode.h"
#include "ClassViewerFilter.h"

class FUnloadedBlueprintData : public IUnloadedBlueprintData
{
public:
	FUnloadedBlueprintData( TWeakPtr< class FClassViewerNode > InClassViewerNode );

	virtual ~FUnloadedBlueprintData() {}

	virtual bool HasAnyClassFlags( uint32 InFlagsToCheck ) const override;

	virtual bool HasAllClassFlags( uint32 InFlagsToCheck ) const override;

	virtual void SetClassFlags(uint32 InFlags) override;

	virtual bool ImplementsInterface(const UClass* InInterface) const override;

	virtual bool IsChildOf(const UClass* InClass) const override;

	virtual bool IsA(const UClass* InClass) const override;

	virtual const UClass* GetClassWithin() const override;

	/** Retrieves the Class Viewer node this data is associated with. */
	const TWeakPtr< class FClassViewerNode > GetClassViewerNode() const;

	/** 
	 * Adds an interface class to this object.
	 *
	 * @param InClass		The class to check against.
	 */
	void AddImplementedInterfaces(FString& InterfaceName);

private:
	/** Flags for the class. */
	uint32 ClassFlags;

	/** The implemented interfaces for this class. */
	TArray< FString > ImplementedInterfaces;

	/** The node this class is contained in, used to gather hierarchical data as needed. */
	TWeakPtr< class FClassViewerNode > ClassViewerNode;
};
