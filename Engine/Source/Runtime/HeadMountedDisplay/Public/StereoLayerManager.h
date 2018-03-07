// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"
#include "IStereoLayers.h"

/**
	Partial implementation of the Layer management code for the IStereoLayers interface.
	Implements adding, deleting and updating layers regardless of how they are rendered.

	A class that wishes to implement the IStereoLayer interface can extend this class instead.
	The template argument should be a type for storing layer data. It should have a constructor matching the following:
		LayerType(const FLayerDesc& InLayerDesc);
	... and implement the following function overloads:
		bool GetLayerDescMember(LayerType& Layer, FLayerDesc& OutLayerDesc),
		void SetLayerDescMember(LayerType& Layer, const FLayerDesc& Desc), and
		void MarkLayerTextureForUpdate(LayerType& Layer)
	
	To perform additional bookkeeping each time individual layers are changed, you can override the following protected method:
		UpdateLayer(LayerType& Layer, uint32 LayerId, bool bIsValid)
	It is called whenever CreateLayer, DestroyLayer, SetLayerDesc and MarkTextureForUpdate are called.

	Simple implementations that do not to track additional data per layer may use FLayerDesc directly.
	The FSimpleLayerManager subclass can be used in that case and it implements all the required glue functions listed above.

	To access the layer data from your subclass, you have the following protected interface:
		bool GetStereoLayersDirty() -- Returns true if layer data have changed since the status was last cleared
		ForEachLayer(...) -- pass in a lambda to iterate through each existing layer.
		CopyLayers(TArray<LayerType>& OutArray, bool bMarkClean = true) -- Copies the layers into OutArray. 
	The last two methods will clear the layer dirty flag unless you pass in false as the optional final argument.
	
	Thread safety:
	Updates and the two protected access methods use a critical section to ensure atomic access to the layer structures.
	Therefore, it is probably better to copy layers before performing time consuming operations using CopyLayers and reserve
	ForEachLayer for simple processing or operations where you need to know the user-facing layer id.

*/

template<typename LayerType>
class TStereoLayerManager : public IStereoLayers
{
private:
	mutable FCriticalSection LayerCritSect;
	TMap<uint32, LayerType> StereoLayers;
	uint32 NextLayerId;
	bool bStereoLayersDirty;

protected:

	virtual void UpdateLayer(LayerType& Layer, uint32 LayerId, bool bIsValid) const
	{}

	bool GetStereoLayersDirty()
	{
		return bStereoLayersDirty;
	}


	void ForEachLayer(TFunction<void(uint32, LayerType&)> Func, bool bMarkClean = true)
	{
		FScopeLock LockLayers(&LayerCritSect);
		for (auto& Pair : StereoLayers)
		{
			Func(Pair.Key, Pair.Value);
		}

		if (bMarkClean)
		{
			bStereoLayersDirty = false;
		}
	}

	void CopyLayers(TArray<LayerType>& OutArray, bool bMarkClean = true)
	{
		FScopeLock LockLayers(&LayerCritSect);
		StereoLayers.GenerateValueArray(OutArray);

		if (bMarkClean)
		{
			bStereoLayersDirty = false;
		}
	}

public:

	TStereoLayerManager()
		: NextLayerId(1)
		, bStereoLayersDirty(false)
	{}

	virtual ~TStereoLayerManager()
	{}

	virtual uint32 CreateLayer(const FLayerDesc& InLayerDesc) override
	{
		FScopeLock LockLayers(&LayerCritSect);

		uint32 LayerId = NextLayerId++;
		check(LayerId > 0);
		LayerType& NewLayer = StereoLayers.Emplace(LayerId, InLayerDesc);
		UpdateLayer(NewLayer, LayerId, InLayerDesc.Texture != nullptr);
		bStereoLayersDirty = true;
		return LayerId;
	}

	virtual void DestroyLayer(uint32 LayerId) override
	{
		if (!LayerId)
		{
			return;
		}

		FScopeLock LockLayers(&LayerCritSect);
		UpdateLayer(StereoLayers[LayerId], LayerId, false);
		StereoLayers.Remove(LayerId);
		bStereoLayersDirty = true;
	}

	virtual void SetLayerDesc(uint32 LayerId, const FLayerDesc& InLayerDesc) override
	{
		if (!LayerId)
		{
			return;
		}

		FScopeLock LockLayers(&LayerCritSect);

		LayerType& Layer = StereoLayers[LayerId];
		SetLayerDescMember(Layer, InLayerDesc);
		UpdateLayer(Layer, LayerId, InLayerDesc.Texture != nullptr);
		bStereoLayersDirty = true;
	}

	virtual bool GetLayerDesc(uint32 LayerId, FLayerDesc& OutLayerDesc) override
	{
		if (!LayerId)
		{
			return false;
		}
		FScopeLock LockLayers(&LayerCritSect);

		return GetLayerDescMember(StereoLayers[LayerId], OutLayerDesc);
	}

	virtual void MarkTextureForUpdate(uint32 LayerId) override 
	{
		if (!LayerId)
		{
			return;
		}
		FScopeLock LockLayers(&LayerCritSect);
		MarkLayerTextureForUpdate(StereoLayers[LayerId]);
	}
};

class HEADMOUNTEDDISPLAY_API FSimpleLayerManager : public TStereoLayerManager<IStereoLayers::FLayerDesc>
{
protected:
	virtual void MarkTextureForUpdate(uint32 LayerId) override
	{}
};

bool GetLayerDescMember(IStereoLayers::FLayerDesc& Layer, IStereoLayers::FLayerDesc& OutLayerDesc);
void SetLayerDescMember(IStereoLayers::FLayerDesc& OutLayer, const IStereoLayers::FLayerDesc& InLayerDesc);
void MarkLayerTextureForUpdate(IStereoLayers::FLayerDesc& Layer);
