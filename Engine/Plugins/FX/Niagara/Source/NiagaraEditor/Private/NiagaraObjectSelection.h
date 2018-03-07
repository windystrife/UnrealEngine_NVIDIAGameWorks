// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraEditorUtilities.h"

/** A set of selected objects which calls a delegate any time it is changed. */
template<typename SelectedItemType>
class TNiagaraSelection
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnSelectedObjectsChanged);

public:
	/** Gets the set of selected objects. */
	const TSet<SelectedItemType>& GetSelectedObjects() const
	{
		return SelectedObjects;
	}

	/** Replaces the currently selected set of objects with the supplied object. */
	void SetSelectedObject(SelectedItemType SelectedObject)
	{
		if (SelectedObjects.Num() == 1 && SelectedObjects.Contains(SelectedObject))
		{
			return;
		}

		SelectedObjects.Empty();
		SelectedObjects.Add(SelectedObject);
		OnSelectedObjectsChangedDelegate.Broadcast();
	}

	/** Replaces the currently selected set of objects with the supplied set. */
	void SetSelectedObjects(const TSet<SelectedItemType>& InSelectedObjects)
	{
		if (FNiagaraEditorUtilities::SetsMatch(SelectedObjects, InSelectedObjects) == false)
		{
			SelectedObjects.Empty();
			SelectedObjects = InSelectedObjects;
			OnSelectedObjectsChangedDelegate.Broadcast();
		}
	}

	/** Replaces the currently selected set of objects with the supplied array. */
	void SetSelectedObjects(const TArray<SelectedItemType>& InSelectedObjects)
	{
		if (FNiagaraEditorUtilities::ArrayMatchesSet(InSelectedObjects, SelectedObjects) == false)
		{
			SelectedObjects.Empty();
			SelectedObjects.Append(InSelectedObjects);
			OnSelectedObjectsChangedDelegate.Broadcast();
		}
	}

	/** Empties the currently selected set of objects. */
	void ClearSelectedObjects()
	{
		if (SelectedObjects.Num() > 0)
		{
			SelectedObjects.Empty();
			OnSelectedObjectsChangedDelegate.Broadcast();
		}
	}

	/** Gets a multicast delegate which is called any time the set of selected objects is changed. */
	FOnSelectedObjectsChanged& OnSelectedObjectsChanged()
	{
		return OnSelectedObjectsChangedDelegate;
	}

private:
	/** The set of selected objects. */
	TSet<SelectedItemType> SelectedObjects;

	/** The delegate which is called whenever the set of selected objects changes. */
	FOnSelectedObjectsChanged OnSelectedObjectsChangedDelegate;
};

class FNiagaraObjectSelection : public TNiagaraSelection<UObject*>
{
};