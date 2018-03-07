// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

template<typename TType, typename TChangeInfo>
class INotifyOnChanged
{
public:
	virtual void PreChange(const TType* Changed, TChangeInfo ChangedType) = 0;
	virtual void PostChange(const TType* Changed, TChangeInfo ChangedType) = 0;
};

template<typename TType, typename TChangeInfo>
class FListenerManager
{
public:
	typedef INotifyOnChanged<TType, TChangeInfo> BaseNotifyOnChanged;

	template<typename TManagerType>
	class InnerListenerType : public BaseNotifyOnChanged
	{
	public:
		InnerListenerType()
		{
			TManagerType::Get().AddListener(this);
		}
		virtual ~InnerListenerType()
		{
			TManagerType::Get().RemoveListener(this);
		}
	};

private:
	TSet<BaseNotifyOnChanged*> Listeners;

public:
	void AddListener(BaseNotifyOnChanged* Listener)
	{
		if (Listener)
		{
			Listeners.Add(Listener);
		}
	}

	void RemoveListener(BaseNotifyOnChanged* Listener)
	{
		Listeners.Remove(Listener);
	}

	void PreChange(const TType* Changed, TChangeInfo Info)
	{
		for (auto Listener : Listeners)
		{
			Listener->PreChange(Changed, Info);
		}
	}

	void PostChange(const TType* Changed, TChangeInfo Info)
	{
		for (auto Listener : Listeners)
		{
			Listener->PostChange(Changed, Info);
		}
	}
	
	const TSet<BaseNotifyOnChanged*>& GetListeners() const
	{
		return Listeners;
	}
};
