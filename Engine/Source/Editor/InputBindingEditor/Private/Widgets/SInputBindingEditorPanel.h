// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Widgets/Views/STableRow.h"
#include "TickableEditorObject.h"

class IDetailLayoutBuilder;

/**
 * A gesture sort functor.  Sorts by name or gesture and ascending or descending
 */
struct FChordSort
{
	FChordSort( bool bInSortName, bool bInSortUp )
		: bSortName( bInSortName )
		, bSortUp( bInSortUp )
	{ }

	bool operator()( const TSharedPtr<FUICommandInfo>& A, const TSharedPtr<FUICommandInfo>& B ) const
	{
		if( bSortName )
		{
			bool bResult = A->GetLabel().CompareTo( B->GetLabel() ) == -1;
			return bSortUp ? !bResult : bResult;
		}
		else
		{
			// Sort by binding
			bool bResult = A->GetInputText().CompareTo( B->GetInputText() ) == -1;
			return bSortUp ? !bResult : bResult;
		}
	}

private:

	/** Whether or not to sort by name.  If false we sort by binding. */
	bool bSortName;

	/** Whether or not to sort up.  If false we sort down. */
	bool bSortUp;
};

/**
* An item for the chord tree view
*/
struct FChordTreeItem
{
	// Note these are mutually exclusive
	TWeakPtr<FBindingContext> BindingContext;
	TSharedPtr<FUICommandInfo> CommandInfo;

	TSharedPtr<FBindingContext> GetBindingContext() { return BindingContext.Pin(); }

	bool IsContext() const { return BindingContext.IsValid(); }
	bool IsCommand() const { return CommandInfo.IsValid(); }
};

/**
 * The main input binding editor widget                   
 */
class FInputBindingEditorPanel : public TSharedFromThis<FInputBindingEditorPanel>, public FTickableEditorObject
{
public:

	/** Default constructor. */
	FInputBindingEditorPanel()
		: bUpdateRequested(false)
	{ }

	/** Destructor. */
	virtual ~FInputBindingEditorPanel()
	{
		FInputBindingManager::Get().SaveInputBindings();
		FBindingContext::CommandsChanged.RemoveAll( this );
	}

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The Slate argument list.
	 */
	void Initialize(class IDetailLayoutBuilder& InDetailBuilder);

	/** FTickableEditorObject interface */
	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsTickable() const override { return bUpdateRequested; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FInputBindingEditorPanel, STATGROUP_Tickables);  }
private:
	void UpdateUI();

	/**
	 * Gets children FChordTreeItems from the passed in tree item.  Note: Only contexts have children and those children are the actual gestures.
	 */
	void GetCommandsForContext( TSharedPtr<FChordTreeItem> InTreeItem, TArray< TSharedPtr< FUICommandInfo > >& OutChildren );

	/** Updates the master context list with new commands. */
	void UpdateContextMasterList();

	/** Called when new commands are registered with the input binding manager. */
	void OnCommandsChanged(const FBindingContext& ContextThatChanged);

private:
	IDetailLayoutBuilder* DetailBuilder;
	/** List of all known contexts. */
	TArray< TSharedPtr<FChordTreeItem> > ContextMasterList;
	/** If the details panel needs to be updated */
	bool bUpdateRequested;
};
