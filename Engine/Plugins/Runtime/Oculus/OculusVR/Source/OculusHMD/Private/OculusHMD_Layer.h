// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OculusHMDPrivate.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "OculusHMD_CustomPresent.h"
#include "OculusHMD_TextureSetProxy.h"

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FOvrpLayer
//-------------------------------------------------------------------------------------------------

class FOvrpLayer : public TSharedFromThis<FOvrpLayer, ESPMode::ThreadSafe>
{
public:
	FOvrpLayer(uint32 InOvrpLayerId);
	~FOvrpLayer();

protected:
	uint32 OvrpLayerId;
};

typedef TSharedPtr<FOvrpLayer, ESPMode::ThreadSafe> FOvrpLayerPtr;


//-------------------------------------------------------------------------------------------------
// FLayer
//-------------------------------------------------------------------------------------------------

class FLayer : public TSharedFromThis<FLayer, ESPMode::ThreadSafe>
{
public:
	FLayer(uint32 InId, const IStereoLayers::FLayerDesc& InDesc);
	FLayer(const FLayer& InLayer);
	~FLayer();

	uint32 GetId() const { return Id; }
	void SetDesc(const IStereoLayers::FLayerDesc& InDesc);
	const IStereoLayers::FLayerDesc& GetDesc() const { return Desc; }
	void SetEyeLayerDesc(const ovrpLayerDesc_EyeFov& InEyeLayerDesc, const ovrpRecti InViewportRect[ovrpEye_Count]);
	const FTextureSetProxyPtr& GetTextureSetProxy() const { return TextureSetProxy; }
	const FTextureSetProxyPtr& GetDepthTextureSetProxy() const { return DepthTextureSetProxy; }
	void MarkTextureForUpdate() { bUpdateTexture = true; }
	bool NeedsPokeAHole() { return (Desc.Flags & IStereoLayers::LAYER_FLAG_SUPPORT_DEPTH) != 0; }
	FTextureRHIRef GetTexture() { return Desc.Texture; }

	TSharedPtr<FLayer, ESPMode::ThreadSafe> Clone() const;

	bool IsCompatibleLayerDesc(const ovrpLayerDescUnion& OvrpLayerDescA, const ovrpLayerDescUnion& OvrpLayerDescB) const;
	void Initialize_RenderThread(FCustomPresent* CustomPresent, FRHICommandListImmediate& RHICmdList, const FLayer* InLayer = nullptr);
	void UpdateTexture_RenderThread(FCustomPresent* CustomPresent, FRHICommandListImmediate& RHICmdList);

	const ovrpLayerSubmit* UpdateLayer_RHIThread(const FSettings* Settings, const FGameFrame* Frame);
	void IncrementSwapChainIndex_RHIThread(FCustomPresent* CustomPresent);
	void ReleaseResources_RHIThread();

	void DrawPokeAHoleMesh(FRHICommandList& RHICmdList, const FMatrix& matrix, float scale, bool invertCoords);

protected:
	uint32 Id;
	IStereoLayers::FLayerDesc Desc;
	int OvrpLayerId;
	ovrpLayerDescUnion OvrpLayerDesc;
	ovrpLayerSubmitUnion OvrpLayerSubmit;
	FOvrpLayerPtr OvrpLayer;
	FTextureSetProxyPtr TextureSetProxy;
	FTextureSetProxyPtr DepthTextureSetProxy;
	FTextureSetProxyPtr RightTextureSetProxy;
	FTextureSetProxyPtr RightDepthTextureSetProxy;
	bool bUpdateTexture;
	bool bInvertY;
	bool bHasDepth;
};

typedef TSharedPtr<FLayer, ESPMode::ThreadSafe> FLayerPtr;


//-------------------------------------------------------------------------------------------------
// FLayerPtr_CompareId
//-------------------------------------------------------------------------------------------------

struct FLayerPtr_CompareId
{
	FORCEINLINE bool operator()(const FLayerPtr& A, const FLayerPtr& B) const
	{
		return A->GetId() < B->GetId();
	}
};


//-------------------------------------------------------------------------------------------------
// FLayerPtr_ComparePriority
//-------------------------------------------------------------------------------------------------

struct FLayerPtr_ComparePriority
{
	FORCEINLINE bool operator()(const FLayerPtr& A, const FLayerPtr& B) const
	{
		if (A->GetDesc().Priority < B->GetDesc().Priority)
			return true;
		if (A->GetDesc().Priority > B->GetDesc().Priority)
			return false;

		return A->GetId() < B->GetId();
	}
};

struct FLayerPtr_CompareTotal
{
	FORCEINLINE bool operator()(const FLayerPtr& A, const FLayerPtr& B) const
	{
		if ((A->GetDesc().Flags & IStereoLayers::ELayerFlags::LAYER_FLAG_SUPPORT_DEPTH) != (B->GetDesc().Flags & IStereoLayers::ELayerFlags::LAYER_FLAG_SUPPORT_DEPTH))
			return (A->GetDesc().Flags & IStereoLayers::ELayerFlags::LAYER_FLAG_SUPPORT_DEPTH) != 0;

		if (A->GetDesc().Priority < B->GetDesc().Priority)
			return true;
		if (A->GetDesc().Priority > B->GetDesc().Priority)
			return false;

		return A->GetId() < B->GetId();
	}
};

} // namespace OculusHMD

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS
