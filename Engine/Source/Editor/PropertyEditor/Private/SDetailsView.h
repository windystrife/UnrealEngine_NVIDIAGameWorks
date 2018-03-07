// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "PropertyNode.h"
#include "IDetailsView.h"
#include "AssetSelection.h"
#include "Widgets/Layout/SScrollBar.h"
#include "SDetailsViewBase.h"

class AActor;
class IDetailRootObjectCustomization;

class SDetailsView : public SDetailsViewBase
{
	friend class FPropertyDetailsUtilities;
public:

	SLATE_BEGIN_ARGS( SDetailsView )
		: _DetailsViewArgs()
		{}
		/** The user defined args for the details view */
		SLATE_ARGUMENT( FDetailsViewArgs, DetailsViewArgs )
	SLATE_END_ARGS()

	virtual ~SDetailsView();

	/** Causes the details view to be refreshed (new widgets generated) with the current set of objects */
	virtual void ForceRefresh() override;

	/** Move the scrolling offset (by item), but do not refresh the tree*/
	void MoveScrollOffset(int32 DeltaOffset) override;


	/**
	 * Constructs the property view widgets                   
	 */
	void Construct(const FArguments& InArgs);

	/** IDetailsView interface */
	virtual void SetObjects( const TArray<UObject*>& InObjects, bool bForceRefresh = false, bool bOverrideLock = false ) override;
	virtual void SetObjects( const TArray< TWeakObjectPtr< UObject > >& InObjects, bool bForceRefresh = false, bool bOverrideLock = false ) override;
	virtual void SetObject( UObject* InObject, bool bForceRefresh = false ) override;
	virtual void RemoveInvalidObjects() override;
	virtual void SetObjectPackageOverrides(const TMap<TWeakObjectPtr<UObject>, TWeakObjectPtr<UPackage>>& InMapping) override;
	virtual void SetRootObjectCustomizationInstance(TSharedPtr<IDetailRootObjectCustomization> InRootObjectCustomization) override;
	virtual void ClearSearch() override;

	/**
	 * Replaces objects being observed by the view with new objects
	 *
	 * @param OldToNewObjectMap	Mapping from objects to replace to their replacement
	 */
	void ReplaceObjects( const TMap<UObject*, UObject*>& OldToNewObjectMap );

	/**
	 * Removes objects from the view because they are about to be deleted
	 *
	 * @param DeletedObjects	The objects to delete
	 */
	void RemoveDeletedObjects( const TArray<UObject*>& DeletedObjects );

	/** Sets the callback for when the property view changes */
	virtual void SetOnObjectArrayChanged( FOnObjectArrayChanged OnObjectArrayChangedDelegate) override;

	/** @return	Returns list of selected objects we're inspecting */
	virtual const TArray< TWeakObjectPtr<UObject> >& GetSelectedObjects() const override
	{
		return SelectedObjects;
	} 

	/** @return	Returns list of selected actors we're inspecting */
	virtual const TArray< TWeakObjectPtr<AActor> >& GetSelectedActors() const override
	{
		return SelectedActors;
	}

	/** @return Returns information about the selected set of actors */
	virtual const FSelectedActorInfo& GetSelectedActorInfo() const override
	{
		return SelectedActorInfo;
	}

	virtual bool HasClassDefaultObject() const override
	{
		return bViewingClassDefaultObject;
	}

	virtual bool IsConnected() const override;

	virtual FRootPropertyNodeList& GetRootNodes() override
	{
		return RootPropertyNodes;
	}

	virtual bool DontUpdateValueWhileEditing() const override
	{ 
		return false; 
	}

	virtual bool ContainsMultipleTopLevelObjects() const override
	{
		return DetailsViewArgs.bAllowMultipleTopLevelObjects && GetNumObjects() > 1;
	}

	virtual TSharedPtr<IDetailRootObjectCustomization> GetRootObjectCustomization() const override
	{
		return RootObjectCustomization;
	}
private:
	void SetObjectArrayPrivate( const TArray< TWeakObjectPtr< UObject > >& InObjects );

	TSharedRef<SDetailTree> ConstructTreeView( TSharedRef<SScrollBar>& ScrollBar );

	/**
	 * Returns whether or not new objects need to be set. If the new objects being set are identical to the objects 
	 * already in the details panel, nothing needs to be set
	 *
	 * @param InObjects The potential new objects to set
	 * @return true if the new objects should be set
	 */
	bool ShouldSetNewObjects( const TArray< TWeakObjectPtr< UObject > >& InObjects ) const;

	/**
	 * Returns the number of objects being edited by this details panel.
	 */
	int32 GetNumObjects() const;

	/** Called before during SetObjectArray before we change the objects being observed */
	void PreSetObject(int32 InNewNumObjects);

	/** Called at the end of SetObjectArray after we change the objects being observed */
	void PostSetObject();
	
	/** Called when the filter button is clicked */
	void OnFilterButtonClicked();

	/** Called to get the visibility of the actor name area */
	EVisibility GetActorNameAreaVisibility() const;

	/** Called to get the visibility of the scrollbar */
	EVisibility GetScrollBarVisibility() const;

	/** Returns the name of the image used for the icon on the locked button */
	const FSlateBrush* OnGetLockButtonImageResource() const;

	/** Whether the property matrix button should be enabled */
	bool CanOpenRawPropertyEditor() const;

	/**
	 * Called to open the raw property editor (property matrix)                                                              
	 */
	FReply OnOpenRawPropertyEditorClicked();

	/** @return Returns true if show hidden properties while playing is checked */
	bool IsShowHiddenPropertiesWhilePlayingChecked() const;

	/** Called when show hidden properties while playing is clicked */
	void OnShowHiddenPropertiesWhilePlayingClicked();

private:
	/** Information about the current set of selected actors */
	FSelectedActorInfo SelectedActorInfo;
	/** Selected objects for this detail view.  */
	TArray< TWeakObjectPtr<UObject> > SelectedObjects;

	/** 
	 * Selected actors for this detail view.  Note that this is not necessarily the same editor selected actor set.  If this detail view is locked
	 * It will only contain actors from when it was locked 
	 */
	TArray< TWeakObjectPtr<AActor> > SelectedActors;
	/** The root property nodes of the property tree for a specific set of UObjects */
	TArray<TSharedPtr<FComplexPropertyNode>> RootPropertyNodes;
	/** Callback to send when the property view changes */
	FOnObjectArrayChanged OnObjectArrayChanged;
	/** Customization instance used when there are multiple top level objects in this view */
	TSharedPtr<IDetailRootObjectCustomization> RootObjectCustomization;
	/** True if at least one viewed object is a CDO (blueprint editing) */
	bool bViewingClassDefaultObject;
};
