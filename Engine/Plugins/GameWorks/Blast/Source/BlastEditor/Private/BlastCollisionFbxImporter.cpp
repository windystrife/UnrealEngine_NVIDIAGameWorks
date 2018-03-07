#include "BlastCollisionFbxImporter.h"
#include "BlastMeshFactory.h"
#include "BlastMesh.h"

#define LOCTEXT_NAMESPACE "Blast"

FBlastCollisionFbxImporter::~FBlastCollisionFbxImporter()
{
	for (FbxNode* CollisionNode : CollisionNodes)
	{
		//Remove the collision nodes, they are orphaned since we unparented them
		CollisionNode->Destroy(true);
	}
}

void FBlastCollisionFbxImporter::FindCollisionNodes(FbxNode* root, bool bWarnIfNotPresent)
{
	CollisionNodes.Reset();
	CollisionParentIndices.Reset();

	CollisionDisplayLayer = root->GetScene()->FindMember<FbxDisplayLayer>("Collision");

	if (CollisionDisplayLayer)
	{
		getFbxMeshes(root);
	}
	else if (bWarnIfNotPresent)
	{
		UnFbx::FFbxImporter::GetInstance()->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("BlastImport_NoCollisionWarning", "You selected the option to import collision data, but the file contains no \"Collision\" Display Layer.")), "BlastImportCollision");
	}
}

void FBlastCollisionFbxImporter::RemoveCollisionNodesFromImportList(TArray< TArray<FbxNode*>* >& SkelMeshArray)
{
	for (auto NodeList : SkelMeshArray)
	{
		if (NodeList)
		{
			NodeList->RemoveAll([this](FbxNode* Node) { return CollisionNodes.Contains(Node); });
		}
	}
	SkelMeshArray.RemoveAll([](TArray<FbxNode*>* NodeList)
	{
		if (NodeList == nullptr)
		{
			return true;
		}
		if (NodeList->Num() == 0)
		{
			delete NodeList;
			return true;
		}
		return false;
	});
}

void FBlastCollisionFbxImporter::ReadMeshSpaceCollisionHullsFromFBX(UBlastMesh* BlastMesh, TMap<FName, TArray<FBlastCollisionHull>>& hulls)
{
	hulls.Empty();

	const auto& BoneNames = BlastMesh->GetChunkIndexToBoneName();
	for (int32_t p = 0; p < CollisionNodes.Num(); ++p)
	{
		int32_t parentIndex = CollisionParentIndices[p];
		FName BoneName = BoneNames[parentIndex];

		auto& chullList = hulls.FindOrAdd(BoneName);
		chullList.Push(FBlastCollisionHull());
		FBlastCollisionHull& chull = chullList.Last();

		FbxNode* FbxMeshNode = CollisionNodes[p];
		FbxMesh* FbxMesh = FbxMeshNode->GetMesh();
		chull.Points.SetNum(FbxMesh->GetControlPointsCount());
		FbxVector4* vpos = FbxMesh->GetControlPoints();

		FbxAMatrix TotalMatrix;
		FbxAMatrix TotalMatrixForNormal;
		//This code purposely doesn't check the import options flags like ComputeSkeletalMeshTotalMatrix or ComputeTotalMatrix does, since these positions always need to be in file/component-space

		FbxAMatrix Geometry;
		FbxVector4 Translation, Rotation, Scaling;
		Translation = FbxMeshNode->GetGeometricTranslation(FbxNode::eSourcePivot);
		Rotation = FbxMeshNode->GetGeometricRotation(FbxNode::eSourcePivot);
		Scaling = FbxMeshNode->GetGeometricScaling(FbxNode::eSourcePivot);
		Geometry.SetT(Translation);
		Geometry.SetR(Rotation);
		Geometry.SetS(Scaling);

		TotalMatrix = CollisionNodeWorldTransforms[p] * Geometry;
		TotalMatrixForNormal = TotalMatrix.Inverse();
		TotalMatrixForNormal = TotalMatrixForNormal.Transpose();

		/**
		Copy control points from FBX.
		*/
		for (int32_t i = 0; i < FbxMesh->GetControlPointsCount(); ++i)
		{
			FbxVector4 WorldPos = TotalMatrix.MultT(*vpos);
			chull.Points[i].X = (float)WorldPos[0];
			chull.Points[i].Y = -(float)WorldPos[1];
			chull.Points[i].Z = (float)WorldPos[2];
			vpos++;
		}

		uint32_t polyCount = FbxMesh->GetPolygonCount();
		chull.PolygonData.SetNum(polyCount);
		FbxGeometryElementNormal* nrm = FbxMesh->GetElementNormal();
		FbxLayerElementArray& narr = nrm->GetDirectArray();

		for (uint32_t poly = 0; poly < polyCount; ++poly)
		{
			int32_t vInPolyCount = FbxMesh->GetPolygonSize(poly);
			chull.PolygonData[poly].IndexBase = (uint16_t)chull.Indices.Num();
			chull.PolygonData[poly].NbVerts = (uint16_t)vInPolyCount;
			int32_t* ind = &FbxMesh->GetPolygonVertices()[FbxMesh->GetPolygonVertexIndex(poly)];

			for (int32_t v = 0; v < vInPolyCount; ++v)
			{
				chull.Indices.Push(*ind);
				++ind;
			}
			FbxVector4 normal;
			narr.GetAt(poly, &normal);
			normal = TotalMatrixForNormal.MultT(normal);

			chull.PolygonData[poly].Plane[0] = (float)normal[0];
			chull.PolygonData[poly].Plane[1] = (float)-normal[1];
			chull.PolygonData[poly].Plane[2] = (float)normal[2];
			FVector polyLastVertex = chull.Points[chull.Indices.Last()];
			chull.PolygonData[poly].Plane[3] = -((float)(polyLastVertex.X * normal[0] + polyLastVertex.Y * -normal[1] + polyLastVertex.Z * normal[2]));
		}
	}
}



int32 FBlastCollisionFbxImporter::getChunkIndexForNode(FbxNode* node, bool includeParents /*= true*/)
{
	FString NodeName(UTF8_TO_TCHAR(node->GetNameOnly()));

	if (NodeName.StartsWith(UBlastMesh::ChunkPrefix))
	{
		return FCString::Atoi(*NodeName.RightChop(UBlastMesh::ChunkPrefix.Len()));
	}

	if (includeParents && node->GetParent())
	{
		return getChunkIndexForNode(node->GetParent(), true);
	}
	//Found nothing
	return INDEX_NONE;;
}

void FBlastCollisionFbxImporter::getFbxMeshes(FbxNode* node)
{
	//do children first to remove bottom up
	int childCount = node->GetChildCount();
	TArray<FbxNode*> nodeChildren;
	nodeChildren.SetNumUninitialized(childCount);

	//Cache these before we recurse since the list could change due to removal
	for (int i = 0; i < childCount; i++)
	{
		nodeChildren[i] = node->GetChild(i);
	}

	for (int i = 0; i < childCount; i++)
	{
		if (nodeChildren[i])
		{
			getFbxMeshes(nodeChildren[i]);
		}
	}

	FbxMesh* mesh = node->GetMesh();
	if (mesh != nullptr && CollisionDisplayLayer->IsMember(node))
	{
		int32 ChunkIndex = getChunkIndexForNode(node);
		if (ChunkIndex != INDEX_NONE)
		{
			CollisionNodes.Add(node);
			CollisionParentIndices.Add(ChunkIndex);
			//Cache this before we remove the parent
			CollisionNodeWorldTransforms.Add(node->EvaluateGlobalTransform());

			//Detach it so it doesn't show up in the skeleton
			FbxNode* Parent = node->GetParent();
			if (Parent)
			{
				Parent->RemoveChild(node);
			}
		}
	}
	
}

#undef LOCTEXT_NAMESPACE