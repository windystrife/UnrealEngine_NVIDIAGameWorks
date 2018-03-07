#pragma once

#include "Public/FbxImporter.h"

struct FBlastCollisionHull;

class FBlastCollisionFbxImporter
{
public:
	FBlastCollisionFbxImporter() : CollisionDisplayLayer(nullptr) {}
	~FBlastCollisionFbxImporter();

	//void ImportFromFile(const FString& Filename, const FString& Type);
	void FindCollisionNodes(FbxNode* root, bool bWarnIfNotPresent);
	void RemoveCollisionNodesFromImportList(TArray< TArray<FbxNode*>* >& SkelMeshArray);

	void ReadMeshSpaceCollisionHullsFromFBX(class UBlastMesh* BlastMesh, TMap<FName, TArray<FBlastCollisionHull>>& hulls);

	const TArray<FbxNode*>& GetCollisionNodes() const { return CollisionNodes; }
private:
	void getFbxMeshes(FbxNode* node);
	int32 getChunkIndexForNode(FbxNode* node, bool includeParents = true);

	TArray<FbxNode*> CollisionNodes;
	TArray<FbxAMatrix> CollisionNodeWorldTransforms;
	TArray<int32> CollisionParentIndices;

	FbxDisplayLayer* CollisionDisplayLayer;
};
