//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "SListView.h"
#include "SharedPointer.h"
#include "IndirectBaker.h"

class STableViewBase;
class UPhononSourceComponent;
class SDockTab;
class ITableRow;
class SButton;
class FSpawnTabArgs;

namespace SteamAudio
{
	/**
	 * Stores information about a baked source, used for display purposes.
	 */
	struct FBakedSource
	{
		FName Name;
		uint32 DataSize;
		UPhononSourceComponent* PhononSourceComponent;

		FBakedSource();
		FBakedSource(const FName& Name, const uint32 DataSize, UPhononSourceComponent* PhononSourceComponent);
	};

	/**
	 * Provides users with a comprehensive view of all sources that may be baked.
	 */
	class FBakeIndirectWindow : public TSharedFromThis<FBakeIndirectWindow>
	{
	public:
		FBakeIndirectWindow();
		~FBakeIndirectWindow();

		void Invoke();
		TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& TabSpawnArgs);
		TSharedRef<ITableRow> OnGenerateBakedSourceRow(TSharedPtr<FBakedSource> Item, const TSharedRef<STableViewBase>& OwnerTable);
		void OnBakedSourceUpdated(FName UniqueIdentifier);

	private:
		void RefreshBakedSources();
		FReply OnBakeSelected();
		bool IsBakeEnabled() const;
		
		TSharedPtr<SButton> BakeSelectedButton;
		TArray<TSharedPtr<FBakedSource>> BakedSources;
		TSharedPtr<SListView<TSharedPtr<FBakedSource>>> BakedSourcesListView;
	};
}