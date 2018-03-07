#include "BlastScratch.h"
#include "NvBlastGlobals.h"
#include "NvBlast.h"

void FBlastFractureScratch::ensureFractureBuffersSize(int32 chunkCount, int32 bondCount)
{
	if (BondFractureData.Max() < bondCount)
	{
		BondFractureData.SetNumUninitialized(bondCount);
	}

	if (ChunkFractureData.Max() < chunkCount)
	{
		ChunkFractureData.SetNumUninitialized(chunkCount);
	}

	check(BondFractureData.Max() >= bondCount);
	check(ChunkFractureData.Max() >= chunkCount);
}


void FBlastFractureScratch::getFractureBuffers(NvBlastFractureBuffers& buffers)
{
	buffers.chunkFractureCount = ChunkFractureData.Max();
	buffers.chunkFractures = ChunkFractureData.GetData();
	buffers.bondFractureCount = BondFractureData.Max();
	buffers.bondFractures = BondFractureData.GetData();
}
