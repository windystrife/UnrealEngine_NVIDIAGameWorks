#pragma once

#include "TickableEditorObject.h"



class FNiagaraShaderQueueTickable : FTickableEditorObject
{
public:
	static void ProcessQueue();

	virtual bool IsTickable() const override
	{
		return true;
	}

	virtual void Tick(float DeltaSeconds) override;

	virtual TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FNiagaraShaderQueueTickable, STATGROUP_Tickables);
	}
};