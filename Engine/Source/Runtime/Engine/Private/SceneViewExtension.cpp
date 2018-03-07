// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SceneViewExtension.h"
#include "Engine/Engine.h"

//
// FSceneViewExtensionBase
//
FSceneViewExtensionBase::~FSceneViewExtensionBase()
{
	// The engine stores view extensions by TWeakPtr<ISceneViewExtension>,
	// so they will be automatically unregistered when removed.
}




//
// FSceneViewExtensions
//

void FSceneViewExtensions::RegisterExtension(const TSharedRef<class ISceneViewExtension, ESPMode::ThreadSafe>& RegisterMe)
{
	if (ensure(GEngine))
	{
		auto& KnownExtensions = GEngine->ViewExtensions->KnownExtensions;
		// Compact the list of known extensions.
		for (int i = 0; i < KnownExtensions.Num(); )
		{
			if (KnownExtensions[i].IsValid())
			{
				i++;
			}
			else
			{
				KnownExtensions.RemoveAtSwap(i);
			}
		}

		KnownExtensions.AddUnique(RegisterMe);
	}
}

// @todo viewext : We should cache all the active extensions in OnStartFrame somewhere
const TArray<TSharedRef<class ISceneViewExtension, ESPMode::ThreadSafe>> FSceneViewExtensions::GatherActiveExtensions(FViewport* InViewport /* = nullptr */) const
{
	TArray<TSharedRef<class ISceneViewExtension, ESPMode::ThreadSafe>> ActiveExtensions;
	ActiveExtensions.Reserve(KnownExtensions.Num());
	
	for (auto& ViewExtPtr : KnownExtensions)
	{
		auto ViewExt = ViewExtPtr.Pin();
		if (ViewExt.IsValid() && ViewExt->IsActiveThisFrame(InViewport))
		{
			ActiveExtensions.Add(ViewExt.ToSharedRef());
		}
	}

	struct SortPriority
	{
		bool operator () (const TSharedPtr<class ISceneViewExtension, ESPMode::ThreadSafe>& A, const TSharedPtr<class ISceneViewExtension, ESPMode::ThreadSafe>& B) const
		{
			return A->GetPriority() > B->GetPriority();
		}
	};

	Sort(ActiveExtensions.GetData(), ActiveExtensions.Num(), SortPriority());

	return ActiveExtensions;
}
