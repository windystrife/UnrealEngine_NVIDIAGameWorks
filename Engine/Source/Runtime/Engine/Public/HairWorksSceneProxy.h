// @third party code - BEGIN HairWorks
#pragma once

#include <Nv/HairWorks/NvHairSdk.h>
#include "PrimitiveSceneProxy.h"

class FSkeletalMeshObjectGPUSkin;

/**
* HairWorks component scene proxy.
*/
class ENGINE_API FHairWorksSceneProxy : public FPrimitiveSceneProxy, public TIntrusiveLinkedList<FHairWorksSceneProxy>
{
public:
	enum class EDrawType
	{
		Normal,
		Shadow,
		Visualization,
	};

	struct FPinMesh
	{
		FMatrix LocalTransform = FMatrix::Identity;	// Relative transform to parent HairWorks component
		FPrimitiveSceneProxy* Mesh = nullptr;
	};

	struct FDynamicRenderData
	{
		NvHair::InstanceDescriptor HairInstanceDesc;
		bool bSimulateInWorldSpace = false;
		TArray<UTexture2D*> Textures;
		TArray<TArray<FPinMesh>> PinMeshes;
		TArray<FMatrix> BoneMatrices;
		FSkeletalMeshObjectGPUSkin* ParentSkin = nullptr;
	};

	FHairWorksSceneProxy(const UPrimitiveComponent* InComponent, NvHair::AssetId HairAssetId);
	~FHairWorksSceneProxy();
	
	//~ Begin FPrimitiveSceneProxy interface.
	virtual uint32 GetMemoryFootprint(void) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)const override;
	virtual void CreateRenderThreadResources() override;
	virtual void OnTransformChanged()override;
	//~ End FPrimitiveSceneProxy interface.

	void UpdateDynamicData_RenderThread(FDynamicRenderData & DynamicData);
	void UpdateMorphIndices_RenderThread(const TArray<int32>& MorphIndices);

	void PreSimulate(FRHICommandList& RHICmdList);

	void Draw(FRHICommandList& RHICmdList, EDrawType DrawType)const;

	NvHair::InstanceId GetHairInstanceId()const { return HairInstanceId; }
	const TArray<FTexture2DRHIRef>& GetTextures()const { return HairTextures; }
	TArray<TArray<FPinMesh>>& GetPinMeshes() { return HairPinMeshes; }
	void SetPinMatrices(const TArray<FMatrix>& PinMatrices);
	const TArray<FMatrix>& GetPinMatrices();
	const TArray<FMatrix>& GetSkinningMatrices()const { return CurrentSkinningMatrices; }
	const TArray<FMatrix>& GetPrevSkinningMatrices()const { return PrevSkinningMatrices; }

	static FHairWorksSceneProxy* GetHairInstances();

	uint32 HairIdInStencil = 0;

protected:
	//** The hair */
	NvHair::InstanceId HairInstanceId;

	//** The hair asset */
	NvHair::AssetId HairAssetId;

	//** Control textures */
	TArray<FTexture2DRHIRef> HairTextures;

	//** Pin meshes*/
	TArray<TArray<FPinMesh>> HairPinMeshes;

	//** Pin matrices*/
	TArray<FMatrix> HairPinMatrices;

	//** Used to transfer data from rendering thread to game thread*/
	FCriticalSection ThreadLock;

	//** Skinning matrices, mainly for interpolated rendering*/
	TArray<FMatrix> CurrentSkinningMatrices;
	TArray<FMatrix> PrevSkinningMatrices;

	//** For morph targets*/
	uint32 MorphVertexUpdateFrameNumber = 0;
	FReadBuffer MorphIndexBuffer;
	FShaderResourceViewRHIRef MorphVertexBuffer;
	FRWBufferStructured MorphPositionDeltaBuffer;
	FRWBufferStructured MorphNormalDeltaBuffer;

	//** All created hair instances*/
	static FHairWorksSceneProxy* HairInstances;
};
// @third party code - END HairWorks
