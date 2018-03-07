#pragma once
#include "CoreMinimal.h"
#include "NvBlastTypes.h"

class FBlastFractureScratch
{
public:
	static FBlastFractureScratch& getInstance()
	{
		static FBlastFractureScratch instance;
		return instance;
	}

	// Called when a component needs to make sure the FractureBuffers structure will fit it's data. This only ever makes the scratch space larger.
	void ensureFractureBuffersSize(int32 chunkCount, int32 bondCount);
	void getFractureBuffers(NvBlastFractureBuffers& buffers);
private:


	TArray<NvBlastBondFractureData>				BondFractureData;
	TArray<NvBlastChunkFractureData>			ChunkFractureData;
};
