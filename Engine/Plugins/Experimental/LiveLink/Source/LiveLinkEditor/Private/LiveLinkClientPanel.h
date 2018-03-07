// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "SListView.h"

struct FLiveLinkSourceUIEntry;
class FLiveLinkClient;
class ULiveLinkSourceFactory;
class ITableRow;
class STableViewBase;
class FMenuBuilder;
class FUICommandList;

typedef TSharedPtr<FLiveLinkSourceUIEntry> FLiveLinkSourceUIEntryPtr;

class SLiveLinkClientPanel : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SLiveLinkClientPanel)
	{
	}

	SLATE_END_ARGS()

	~SLiveLinkClientPanel();

	void Construct(const FArguments& Args, FLiveLinkClient* InClient);

private:

	void BindCommands();

	void RefreshSourceData(bool bRefreshUI);

	TSharedRef< SWidget > GenerateSourceMenu();

	void RetrieveFactorySourcePanel(FMenuBuilder& MenuBuilder, ULiveLinkSourceFactory* FactoryCDO);

	FReply OnCloseSourceSelectionPanel(ULiveLinkSourceFactory* FactoryCDO, bool bMakeSource);

	void HandleOnAddFromFactory(ULiveLinkSourceFactory* InSourceFactory);

	void HandleRemoveSource();

	bool CanRemoveSource();

	void HandleRemoveAllSources();

	// Registered with the client and called when clients sources changes
	void OnSourcesChangedHandler();

private:
	const TArray<FLiveLinkSourceUIEntryPtr>& GetCurrentSources() const;

	TSharedRef<ITableRow> MakeSourceListViewWidget(FLiveLinkSourceUIEntryPtr Entry, const TSharedRef<STableViewBase>& OwnerTable) const;
	
	void OnSourceListSelectionChanged(FLiveLinkSourceUIEntryPtr Entry, ESelectInfo::Type SelectionType) const;
	
	TSharedPtr<SListView<FLiveLinkSourceUIEntryPtr>> ListView;

	TArray<FLiveLinkSourceUIEntryPtr> SourceData;

	TSharedPtr<FUICommandList> CommandList;

	FLiveLinkClient* Client;

	TMap<ULiveLinkSourceFactory*, TSharedPtr<SWidget>> SourcePanels;

	// Reference to connection settings struct details panel
	TSharedPtr<class IStructureDetailsView> StructureDetailsView;

	// Handle to delegate registered with client so we can update when a source disappears
	FDelegateHandle OnSourcesChangedHandle;
};