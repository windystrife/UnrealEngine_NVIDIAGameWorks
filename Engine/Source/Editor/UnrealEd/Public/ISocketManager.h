// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class IStaticMeshEditor;
class UStaticMeshSocket;

class ISocketManager : public SCompoundWidget
{
public:
	/** Retrieves the selected socket. */
	virtual UStaticMeshSocket* GetSelectedSocket() const = 0;

	/** Sets the selected socket. */
	virtual void SetSelectedSocket(UStaticMeshSocket* InSelectedSocket) = 0;

	/** Deletes the selected socket. */
	virtual void DeleteSelectedSocket() = 0;

	/** Duplicate the selected socket */
	virtual void DuplicateSelectedSocket() = 0;

	/** Request a rename on the selected socket */
	virtual void RequestRenameSelectedSocket() = 0;

	/** Updates the StaticMesh currently being edited */
	virtual void UpdateStaticMesh() = 0;

	/** Creates a socket manager instance. */
	UNREALED_API static TSharedPtr<ISocketManager> CreateSocketManager(TSharedPtr<class IStaticMeshEditor> InStaticMeshEditor, FSimpleDelegate InOnSocketSelectionChanged );
};
