//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "TickableEditorObject.h"
#include "SNotificationList.h"
#include "Queue.h"

namespace SteamAudio
{
	struct FWorkItem
	{
		FWorkItem();
		FWorkItem(const TFunction<void(FText&)>& Task, const SNotificationItem::ECompletionState InFinalState, const bool bIsFinalItem);

		TFunction<void(FText&)> Task;
		SNotificationItem::ECompletionState FinalState;
		bool bIsFinalItem;
	};

	class FTickableNotification : public FTickableEditorObject
	{
	public:
		FTickableNotification();

		void CreateNotification();
		void CreateNotificationWithCancel(const FSimpleDelegate& CancelDelegate);
		void DestroyNotification(const SNotificationItem::ECompletionState InFinalState = SNotificationItem::CS_Success);
		void SetDisplayText(const FText& InDisplayText);
		void QueueWorkItem(const FWorkItem& WorkItem);

	protected:
		virtual void Tick(float DeltaTime) override;
		virtual bool IsTickable() const override;
		virtual TStatId GetStatId() const override;

	private:
		void NotifyDestruction();

		TWeakPtr<SNotificationItem> NotificationPtr;
		FCriticalSection CriticalSection;
		FText DisplayText;
		bool bIsTicking;
		TQueue<FWorkItem> WorkQueue;
		SNotificationItem::ECompletionState FinalState;
	};
}
