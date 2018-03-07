// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Skeletal mesh creation from FBX data.
	Largely based on SkeletalMeshImport.cpp
=============================================================================*/

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "Misc/MessageDialog.h"
#include "Containers/IndirectArray.h"
#include "Stats/Stats.h"
#include "Async/AsyncWork.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/PackageName.h"
#include "SkeletalMeshTypes.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "AnimEncoding.h"
#include "Factories/Factory.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "Animation/MorphTarget.h"
#include "PhysicsAssetUtils.h"

#include "SkelImport.h"
#include "Logging/TokenizedMessage.h"
#include "FbxImporter.h"

#include "AssetData.h"
#include "ARFilter.h"
#include "AssetRegistryModule.h"
#include "AssetNotifications.h"

#include "ObjectTools.h"

#include "ApexClothingUtils.h"
#include "MeshUtilities.h"

#include "IMessageLogListing.h"
#include "MessageLogModule.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "ComponentReregisterContext.h"

#include "Misc/FbxErrors.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Assets/ClothingAsset.h"

#define LOCTEXT_NAMESPACE "FBXImpoter"

using namespace UnFbx;


// Get the geometry deformation local to a node. It is never inherited by the
// children.
FbxAMatrix GetGeometry(FbxNode* pNode) 
{
	FbxVector4 lT, lR, lS;
	FbxAMatrix lGeometry;

	lT = pNode->GetGeometricTranslation(FbxNode::eSourcePivot);
	lR = pNode->GetGeometricRotation(FbxNode::eSourcePivot);
	lS = pNode->GetGeometricScaling(FbxNode::eSourcePivot);

	lGeometry.SetT(lT);
	lGeometry.SetR(lR);
	lGeometry.SetS(lS);

	return lGeometry;
}

// Scale all the elements of a matrix.
void MatrixScale(FbxAMatrix& pMatrix, double pValue)
{
	int32 i,j;

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			pMatrix[i][j] *= pValue;
		}
	}
}

// Add a value to all the elements in the diagonal of the matrix.
void MatrixAddToDiagonal(FbxAMatrix& pMatrix, double pValue)
{
	pMatrix[0][0] += pValue;
	pMatrix[1][1] += pValue;
	pMatrix[2][2] += pValue;
	pMatrix[3][3] += pValue;
}


// Sum two matrices element by element.
void MatrixAdd(FbxAMatrix& pDstMatrix, FbxAMatrix& pSrcMatrix)
{
	int32 i,j;

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			pDstMatrix[i][j] += pSrcMatrix[i][j];
		}
	}
}

void FFbxImporter::SkinControlPointsToPose(FSkeletalMeshImportData& ImportData, FbxMesh* FbxMesh, FbxShape* FbxShape, bool bUseT0 )
{
	FbxTime poseTime = FBXSDK_TIME_INFINITE;
	if(bUseT0)
	{
		poseTime = 0;
	}

	int32 VertexCount = FbxMesh->GetControlPointsCount();

	// Create a copy of the vertex array to receive vertex deformations.
	FbxVector4* VertexArray = new FbxVector4[VertexCount];

	// If a shape is provided, then it is the morphed version of the mesh.
	// So we want to deform that instead of the base mesh vertices
	if (FbxShape)
	{
		check(FbxShape->GetControlPointsCount() == VertexCount);
		FMemory::Memcpy(VertexArray, FbxShape->GetControlPoints(), VertexCount * sizeof(FbxVector4));
	}
	else
	{
		FMemory::Memcpy(VertexArray, FbxMesh->GetControlPoints(), VertexCount * sizeof(FbxVector4));
	}																	 


	int32 ClusterCount = 0;
	int32 SkinCount = FbxMesh->GetDeformerCount(FbxDeformer::eSkin);
	for( int32 i=0; i< SkinCount; i++)
	{
		ClusterCount += ((FbxSkin *)(FbxMesh->GetDeformer(i, FbxDeformer::eSkin)))->GetClusterCount();
	}
	
	// Deform the vertex array with the links contained in the mesh.
	if (ClusterCount)
	{
		FbxAMatrix MeshMatrix = ComputeTotalMatrix(FbxMesh->GetNode());
		// All the links must have the same link mode.
		FbxCluster::ELinkMode lClusterMode = ((FbxSkin*)FbxMesh->GetDeformer(0, FbxDeformer::eSkin))->GetCluster(0)->GetLinkMode();

		int32 i, j;
		int32 lClusterCount=0;

		int32 lSkinCount = FbxMesh->GetDeformerCount(FbxDeformer::eSkin);

		TArray<FbxAMatrix> ClusterDeformations;
		ClusterDeformations.AddZeroed( VertexCount );

		TArray<double> ClusterWeights;
		ClusterWeights.AddZeroed( VertexCount );
	

		if (lClusterMode == FbxCluster::eAdditive)
		{
			for (i = 0; i < VertexCount; i++)
			{
				ClusterDeformations[i].SetIdentity();
			}
		}

		for ( i=0; i<lSkinCount; ++i)
		{
			lClusterCount =( (FbxSkin *)FbxMesh->GetDeformer(i, FbxDeformer::eSkin))->GetClusterCount();
			for (j=0; j<lClusterCount; ++j)
			{
				FbxCluster* Cluster =((FbxSkin *) FbxMesh->GetDeformer(i, FbxDeformer::eSkin))->GetCluster(j);
				if (!Cluster->GetLink())
					continue;
					
				FbxNode* Link = Cluster->GetLink();
				
				FbxAMatrix lReferenceGlobalInitPosition;
				FbxAMatrix lReferenceGlobalCurrentPosition;
				FbxAMatrix lClusterGlobalInitPosition;
				FbxAMatrix lClusterGlobalCurrentPosition;
				FbxAMatrix lReferenceGeometry;
				FbxAMatrix lClusterGeometry;

				FbxAMatrix lClusterRelativeInitPosition;
				FbxAMatrix lClusterRelativeCurrentPositionInverse;
				FbxAMatrix lVertexTransformMatrix;

				if (lClusterMode == FbxCluster::eAdditive && Cluster->GetAssociateModel())
				{
					Cluster->GetTransformAssociateModelMatrix(lReferenceGlobalInitPosition);
					lReferenceGlobalCurrentPosition = Scene->GetAnimationEvaluator()->GetNodeGlobalTransform(Cluster->GetAssociateModel(), poseTime);
					// Geometric transform of the model
					lReferenceGeometry = GetGeometry(Cluster->GetAssociateModel());
					lReferenceGlobalCurrentPosition *= lReferenceGeometry;
				}
				else
				{
					Cluster->GetTransformMatrix(lReferenceGlobalInitPosition);
					lReferenceGlobalCurrentPosition = MeshMatrix; //pGlobalPosition;
					// Multiply lReferenceGlobalInitPosition by Geometric Transformation
					lReferenceGeometry = GetGeometry(FbxMesh->GetNode());
					lReferenceGlobalInitPosition *= lReferenceGeometry;
				}
				// Get the link initial global position and the link current global position.
				Cluster->GetTransformLinkMatrix(lClusterGlobalInitPosition);
				lClusterGlobalCurrentPosition = Link->GetScene()->GetAnimationEvaluator()->GetNodeGlobalTransform(Link, poseTime);

				// Compute the initial position of the link relative to the reference.
				lClusterRelativeInitPosition = lClusterGlobalInitPosition.Inverse() * lReferenceGlobalInitPosition;

				// Compute the current position of the link relative to the reference.
				lClusterRelativeCurrentPositionInverse = lReferenceGlobalCurrentPosition.Inverse() * lClusterGlobalCurrentPosition;

				// Compute the shift of the link relative to the reference.
				lVertexTransformMatrix = lClusterRelativeCurrentPositionInverse * lClusterRelativeInitPosition;

				int32 k;
				int32 lVertexIndexCount = Cluster->GetControlPointIndicesCount();

				for (k = 0; k < lVertexIndexCount; ++k) 
				{
					int32 lIndex = Cluster->GetControlPointIndices()[k];

					// Sometimes, the mesh can have less points than at the time of the skinning
					// because a smooth operator was active when skinning but has been deactivated during export.
					if (lIndex >= VertexCount)
					{
						continue;
					}

					double lWeight = Cluster->GetControlPointWeights()[k];

					if (lWeight == 0.0)
					{
						continue;
					}

					// Compute the influence of the link on the vertex.
					FbxAMatrix lInfluence = lVertexTransformMatrix;
					MatrixScale(lInfluence, lWeight);

					if (lClusterMode == FbxCluster::eAdditive)
					{
						// Multiply with to the product of the deformations on the vertex.
						MatrixAddToDiagonal(lInfluence, 1.0 - lWeight);
						ClusterDeformations[lIndex] = lInfluence * ClusterDeformations[lIndex];

						// Set the link to 1.0 just to know this vertex is influenced by a link.
						ClusterWeights[lIndex] = 1.0;
					}
					else // lLinkMode == KFbxLink::eNORMALIZE || lLinkMode == KFbxLink::eTOTAL1
					{
						// Add to the sum of the deformations on the vertex.
						MatrixAdd(ClusterDeformations[lIndex], lInfluence);

						// Add to the sum of weights to either normalize or complete the vertex.
						ClusterWeights[lIndex] += lWeight;
					}

				}
			}
		}
		
		for (i = 0; i < VertexCount; i++) 
		{
			FbxVector4 lSrcVertex = VertexArray[i];
			FbxVector4& lDstVertex = VertexArray[i];
			double lWeight = ClusterWeights[i];

			// Deform the vertex if there was at least a link with an influence on the vertex,
			if (lWeight != 0.0) 
			{
				lDstVertex = ClusterDeformations[i].MultT(lSrcVertex);

				if (lClusterMode == FbxCluster::eNormalize)
				{
					// In the normalized link mode, a vertex is always totally influenced by the links. 
					lDstVertex /= lWeight;
				}
				else if (lClusterMode == FbxCluster::eTotalOne)
				{
					// In the total 1 link mode, a vertex can be partially influenced by the links. 
					lSrcVertex *= (1.0 - lWeight);
					lDstVertex += lSrcVertex;
				}
			} 
		}
		
		// change the vertex position
		int32 ExistPointNum = ImportData.Points.Num();
		int32 StartPointIndex = ExistPointNum - VertexCount;
		for(int32 ControlPointsIndex = 0 ; ControlPointsIndex < VertexCount ;ControlPointsIndex++ )
		{
			ImportData.Points[ControlPointsIndex+StartPointIndex] = Converter.ConvertPos(MeshMatrix.MultT(VertexArray[ControlPointsIndex]));
		}
		
	}
	
	delete[] VertexArray;
}

// 3 "ProcessImportMesh..." functions outputing Unreal data from a filled FSkeletalMeshBinaryImport
// and a handfull of other minor stuff needed by these 
// Fully taken from SkeletalMeshImport.cpp


struct tFaceRecord
{
	int32 FaceIndex;
	int32 HoekIndex;
	int32 WedgeIndex;
	uint32 SmoothFlags;
	uint32 FanFlags;
};

struct VertsFans
{	
	TArray<tFaceRecord> FaceRecord;
	int32 FanGroupCount;
};

struct tInfluences
{
	TArray<int32> RawInfIndices;
};

struct tWedgeList
{
	TArray<int32> WedgeList;
};

struct tFaceSet
{
	TArray<int32> Faces;
};

// Check whether faces have at least two vertices in common. These must be POINTS - don't care about wedges.
bool UnFbx::FFbxImporter::FacesAreSmoothlyConnected( FSkeletalMeshImportData &ImportData, int32 Face1, int32 Face2 )
{

	//if( ( Face1 >= Thing->SkinData.Faces.Num()) || ( Face2 >= Thing->SkinData.Faces.Num()) ) return false;

	if( Face1 == Face2 )
	{
		return true;
	}

	// Smoothing groups match at least one bit in binary AND ?
	if( ( ImportData.Faces[Face1].SmoothingGroups & ImportData.Faces[Face2].SmoothingGroups ) == 0  ) 
	{
		return false;
	}

	int32 VertMatches = 0;
	for( int32 i=0; i<3; i++)
	{
		int32 Point1 = ImportData.Wedges[ ImportData.Faces[Face1].WedgeIndex[i] ].VertexIndex;

		for( int32 j=0; j<3; j++)
		{
			int32 Point2 = ImportData.Wedges[ ImportData.Faces[Face2].WedgeIndex[j] ].VertexIndex;
			if( Point2 == Point1 )
			{
				VertMatches ++;
			}
		}
	}

	return ( VertMatches >= 2 );
}

int32	UnFbx::FFbxImporter::DoUnSmoothVerts(FSkeletalMeshImportData &ImportData, bool bDuplicateUnSmoothWedges)
{
	//
	// Connectivity: triangles with non-matching smoothing groups will be physically split.
	//
	// -> Splitting involves: the UV+material-contaning vertex AND the 3d point.
	//
	// -> Tally smoothing groups for each and every (textured) vertex.
	//
	// -> Collapse: 
	// -> start from a vertex and all its adjacent triangles - go over
	// each triangle - if any connecting one (sharing more than one vertex) gives a smoothing match,
	// accumulate it. Then IF more than one resulting section, 
	// ensure each boundary 'vert' is split _if not already_ to give each smoothing group
	// independence from all others.
	//

	int32 DuplicatedVertCount = 0;
	int32 RemappedHoeks = 0;

	int32 TotalSmoothMatches = 0;
	int32 TotalConnexChex = 0;

	// Link _all_ faces to vertices.	
	TArray<VertsFans>  Fans;
	TArray<tInfluences> PointInfluences;	
	TArray<tWedgeList>  PointWedges;

	Fans.AddZeroed( ImportData.Points.Num() );//Fans.AddExactZeroed(			Thing->SkinData.Points.Num() );
	PointInfluences.AddZeroed( ImportData.Points.Num() );//PointInfluences.AddExactZeroed( Thing->SkinData.Points.Num() );
	PointWedges.AddZeroed( ImportData.Points.Num() );//PointWedges.AddExactZeroed(	 Thing->SkinData.Points.Num() );

	// Existing points map 1:1
	ImportData.PointToRawMap.AddUninitialized( ImportData.Points.Num() );
	for(int32 i=0; i<ImportData.Points.Num(); i++)
	{
		ImportData.PointToRawMap[i] = i;
	}

	for(int32 i=0; i< ImportData.Influences.Num(); i++)
	{
		if (PointInfluences.Num() <= ImportData.Influences[i].VertexIndex)
		{
			PointInfluences.AddZeroed(ImportData.Influences[i].VertexIndex - PointInfluences.Num() + 1);
		}
		PointInfluences[ImportData.Influences[i].VertexIndex ].RawInfIndices.Add( i );
	}

	for(int32 i=0; i< ImportData.Wedges.Num(); i++)
	{
		if (uint32(PointWedges.Num()) <= ImportData.Wedges[i].VertexIndex)
		{
			PointWedges.AddZeroed(ImportData.Wedges[i].VertexIndex - PointWedges.Num() + 1);
		}

		PointWedges[ImportData.Wedges[i].VertexIndex ].WedgeList.Add( i );
	}

	for(int32 f=0; f< ImportData.Faces.Num(); f++ )
	{
		// For each face, add a pointer to that face into the Fans[vertex].
		for( int32 i=0; i<3; i++)
		{
			int32 WedgeIndex = ImportData.Faces[f].WedgeIndex[i];			
			int32 PointIndex = ImportData.Wedges[ WedgeIndex ].VertexIndex;
			tFaceRecord NewFR;

			NewFR.FaceIndex = f;
			NewFR.HoekIndex = i;			
			NewFR.WedgeIndex = WedgeIndex; // This face touches the point courtesy of Wedges[Wedgeindex].
			NewFR.SmoothFlags = ImportData.Faces[f].SmoothingGroups;
			NewFR.FanFlags = 0;
			Fans[ PointIndex ].FaceRecord.Add( NewFR );
			Fans[ PointIndex ].FanGroupCount = 0;
		}		
	}

	// Investigate connectivity and assign common group numbers (1..+) to the fans' individual FanFlags.
	for( int32 p=0; p< Fans.Num(); p++) // The fan of faces for each 3d point 'p'.
	{
		// All faces connecting.
		if( Fans[p].FaceRecord.Num() > 0 ) 
		{		
			int32 FacesProcessed = 0;
			TArray<tFaceSet> FaceSets; // Sets with indices INTO FANS, not into face array.			

			// Digest all faces connected to this vertex (p) into one or more smooth sets. only need to check 
			// all faces MINUS one..
			while( FacesProcessed < Fans[p].FaceRecord.Num()  )
			{
				// One loop per group. For the current ThisFaceIndex, tally all truly connected ones
				// and put them in a new TArray. Once no more can be connected, stop.

				int32 NewSetIndex = FaceSets.Num(); // 0 to start
				FaceSets.AddZeroed(1);						// first one will be just ThisFaceIndex.

				// Find the first non-processed face. There will be at least one.
				int32 ThisFaceFanIndex = 0;
				{
					int32 SearchIndex = 0;
					while( Fans[p].FaceRecord[SearchIndex].FanFlags == -1 ) // -1 indicates already  processed. 
					{
						SearchIndex++;
					}
					ThisFaceFanIndex = SearchIndex; //Fans[p].FaceRecord[SearchIndex].FaceIndex; 
				}

				// Initial face.
				FaceSets[ NewSetIndex ].Faces.Add( ThisFaceFanIndex );   // Add the unprocessed Face index to the "local smoothing group" [NewSetIndex].
				Fans[p].FaceRecord[ThisFaceFanIndex].FanFlags = -1;			  // Mark as processed.
				FacesProcessed++; 

				// Find all faces connected to this face, and if there's any
				// smoothing group matches, put it in current face set and mark it as processed;
				// until no more match. 
				int32 NewMatches = 0;
				do
				{
					NewMatches = 0;
					// Go over all current faces in this faceset and set if the FaceRecord (local smoothing groups) has any matches.
					// there will be at least one face already in this faceset - the first face in the fan.
					for( int32 n=0; n< FaceSets[NewSetIndex].Faces.Num(); n++)
					{				
						int32 HookFaceIdx = Fans[p].FaceRecord[ FaceSets[NewSetIndex].Faces[n] ].FaceIndex;

						//Go over the fan looking for matches.
						for( int32 s=0; s< Fans[p].FaceRecord.Num(); s++)
						{
							// Skip if same face, skip if face already processed.
							if( ( HookFaceIdx != Fans[p].FaceRecord[s].FaceIndex )  && ( Fans[p].FaceRecord[s].FanFlags != -1  ))
							{
								TotalConnexChex++;
								// Process if connected with more than one vertex, AND smooth..
								if( FacesAreSmoothlyConnected( ImportData, HookFaceIdx, Fans[p].FaceRecord[s].FaceIndex ) )
								{									
									TotalSmoothMatches++;
									Fans[p].FaceRecord[s].FanFlags = -1; // Mark as processed.
									FacesProcessed++;
									// Add 
									FaceSets[NewSetIndex].Faces.Add( s ); // Store FAN index of this face index into smoothing group's faces. 
									// Tally
									NewMatches++;
								}
							} // not the same...
						}// all faces in fan
					} // all faces in FaceSet
				}while( NewMatches );	

			}// Repeat until all faces processed.

			// For the new non-initialized  face sets, 
			// Create a new point, influences, and uv-vertex(-ices) for all individual FanFlag groups with an index of 2+ and also remap
			// the face's vertex into those new ones.
			if( FaceSets.Num() > 1 )
			{
				for( int32 f=1; f<FaceSets.Num(); f++ )
				{				
					check(ImportData.Points.Num() == ImportData.PointToRawMap.Num());

					// We duplicate the current vertex. (3d point)
					int32 NewPointIndex = ImportData.Points.Num();
					ImportData.Points.AddUninitialized();
					ImportData.Points[NewPointIndex] = ImportData.Points[p] ;

					ImportData.PointToRawMap.AddUninitialized();
					ImportData.PointToRawMap[NewPointIndex] = p;
					
					DuplicatedVertCount++;

					// Duplicate all related weights.
					for( int32 t=0; t< PointInfluences[p].RawInfIndices.Num(); t++ )
					{
						// Add new weight
						int32 NewWeightIndex = ImportData.Influences.Num();
						ImportData.Influences.AddUninitialized();
						ImportData.Influences[NewWeightIndex] = ImportData.Influences[ PointInfluences[p].RawInfIndices[t] ];
						ImportData.Influences[NewWeightIndex].VertexIndex = NewPointIndex;
					}

					// Duplicate any and all Wedges associated with it; and all Faces' wedges involved.					
					for( int32 w=0; w< PointWedges[p].WedgeList.Num(); w++)
					{
						int32 OldWedgeIndex = PointWedges[p].WedgeList[w];
						int32 NewWedgeIndex = ImportData.Wedges.Num();

						if( bDuplicateUnSmoothWedges )
						{
							ImportData.Wedges.AddUninitialized();
							ImportData.Wedges[NewWedgeIndex] = ImportData.Wedges[ OldWedgeIndex ];
							ImportData.Wedges[ NewWedgeIndex ].VertexIndex = NewPointIndex; 

							//  Update relevant face's Wedges. Inelegant: just check all associated faces for every new wedge.
							for( int32 s=0; s< FaceSets[f].Faces.Num(); s++)
							{
								int32 FanIndex = FaceSets[f].Faces[s];
								if( Fans[p].FaceRecord[ FanIndex ].WedgeIndex == OldWedgeIndex )
								{
									// Update just the right one for this face (HoekIndex!) 
									ImportData.Faces[ Fans[p].FaceRecord[ FanIndex].FaceIndex ].WedgeIndex[ Fans[p].FaceRecord[ FanIndex ].HoekIndex ] = NewWedgeIndex;
									RemappedHoeks++;
								}
							}
						}
						else
						{
							ImportData.Wedges[OldWedgeIndex].VertexIndex = NewPointIndex; 
						}
					}
				}
			} //  if FaceSets.Num(). -> duplicate stuff
		}//	while( FacesProcessed < Fans[p].FaceRecord.Num() )
	} // Fans for each 3d point

	check(ImportData.Points.Num() == ImportData.PointToRawMap.Num());

	return DuplicatedVertCount; 
}

bool IsUnrealBone(FbxNode* Link)
{
	FbxNodeAttribute* Attr = Link->GetNodeAttribute();
	if (Attr)
	{
		FbxNodeAttribute::EType AttrType = Attr->GetAttributeType();
		if ( AttrType == FbxNodeAttribute::eSkeleton ||
			AttrType == FbxNodeAttribute::eMesh ||
			AttrType == FbxNodeAttribute::eNull )
		{
			return true;
		}
	}
	
	return false;
}


void UnFbx::FFbxImporter::RecursiveBuildSkeleton(FbxNode* Link, TArray<FbxNode*>& OutSortedLinks)
{
	if (IsUnrealBone(Link))
	{
		OutSortedLinks.Add(Link);
		int32 ChildIndex;
		for (ChildIndex=0; ChildIndex<Link->GetChildCount(); ChildIndex++)
		{
			RecursiveBuildSkeleton(Link->GetChild(ChildIndex),OutSortedLinks);
		}
	}
}

void UnFbx::FFbxImporter::BuildSkeletonSystem(TArray<FbxCluster*>& ClusterArray, TArray<FbxNode*>& OutSortedLinks)
{
	FbxNode* Link;
	TArray<FbxNode*> RootLinks;
	int32 ClusterIndex;
	for (ClusterIndex = 0; ClusterIndex < ClusterArray.Num(); ClusterIndex++)
	{
		Link = ClusterArray[ClusterIndex]->GetLink();
		if (Link)
		{
			Link = GetRootSkeleton(Link);
			int32 LinkIndex;
			for (LinkIndex = 0; LinkIndex < RootLinks.Num(); LinkIndex++)
			{
				if (Link == RootLinks[LinkIndex])
				{
					break;
				}
			}

			// this link is a new root, add it
			if (LinkIndex == RootLinks.Num())
			{
				RootLinks.Add(Link);
			}
		}
	}

	for (int32 LinkIndex=0; LinkIndex<RootLinks.Num(); LinkIndex++)
	{
		RecursiveBuildSkeleton(RootLinks[LinkIndex], OutSortedLinks);
	}
}

bool UnFbx::FFbxImporter::RetrievePoseFromBindPose(const TArray<FbxNode*>& NodeArray, FbxArray<FbxPose*> & PoseArray) const
{
	const int32 PoseCount = Scene->GetPoseCount();
	for(int32 PoseIndex = 0; PoseIndex < PoseCount; PoseIndex++)
	{
		FbxPose* CurrentPose = Scene->GetPose(PoseIndex);

		// current pose is bind pose, 
		if(CurrentPose && CurrentPose->IsBindPose())
		{
			// IsValidBindPose doesn't work reliably
			// It checks all the parent chain(regardless root given), and if the parent doesn't have correct bind pose, it fails
			// It causes more false positive issues than the real issue we have to worry about
			// If you'd like to try this, set CHECK_VALID_BIND_POSE to 1, and try the error message
			// when Autodesk fixes this bug, then we might be able to re-open this
			FString PoseName = CurrentPose->GetName();
			// all error report status
			FbxStatus Status;

			// it does not make any difference of checking with different node
			// it is possible pose 0 -> node array 2, but isValidBindPose function returns true even with node array 0
			for(auto Current : NodeArray)
			{
				FString CurrentName = Current->GetName();
				NodeList pMissingAncestors, pMissingDeformers, pMissingDeformersAncestors, pWrongMatrices;

				if(CurrentPose->IsValidBindPoseVerbose(Current, pMissingAncestors, pMissingDeformers, pMissingDeformersAncestors, pWrongMatrices, 0.0001, &Status))
				{
					PoseArray.Add(CurrentPose);
					UE_LOG(LogFbx, Log, TEXT("Valid bind pose for Pose (%s) - %s"), *PoseName, *FString(Current->GetName()));
					break;
				}
				else
				{
					// first try to fix up
					// add missing ancestors
					for(int i = 0; i < pMissingAncestors.GetCount(); i++)
					{
						FbxAMatrix mat = pMissingAncestors.GetAt(i)->EvaluateGlobalTransform(FBXSDK_TIME_ZERO);
						CurrentPose->Add(pMissingAncestors.GetAt(i), mat);
					}

					pMissingAncestors.Clear();
					pMissingDeformers.Clear();
					pMissingDeformersAncestors.Clear();
					pWrongMatrices.Clear();

					// check it again
					if(CurrentPose->IsValidBindPose(Current))
					{
						PoseArray.Add(CurrentPose);
						UE_LOG(LogFbx, Log, TEXT("Valid bind pose for Pose (%s) - %s"), *PoseName, *FString(Current->GetName()));
						break;
					}
					else
					{
						// first try to find parent who is null group and see if you can try test it again
						FbxNode * ParentNode = Current->GetParent();
						while(ParentNode)
						{
							FbxNodeAttribute* Attr = ParentNode->GetNodeAttribute();
							if(Attr && Attr->GetAttributeType() == FbxNodeAttribute::eNull)
							{
								// found it 
								break;
							}

							// find next parent
							ParentNode = ParentNode->GetParent();
						}

						if(ParentNode && CurrentPose->IsValidBindPose(ParentNode))
						{
							PoseArray.Add(CurrentPose);
							UE_LOG(LogFbx, Log, TEXT("Valid bind pose for Pose (%s) - %s"), *PoseName, *FString(Current->GetName()));
							break;
						}
						else
						{
							FString ErrorString = Status.GetErrorString();
							if (!GIsAutomationTesting)
								UE_LOG(LogFbx, Warning, TEXT("Not valid bind pose for Pose (%s) - Node %s : %s"), *PoseName, *FString(Current->GetName()), *ErrorString);
						}
					}
				}
			}
		}
	}

	return (PoseArray.Size() > 0);
}

bool UnFbx::FFbxImporter::ImportBone(TArray<FbxNode*>& NodeArray, FSkeletalMeshImportData &ImportData, UFbxSkeletalMeshImportData* TemplateData ,TArray<FbxNode*> &SortedLinks, bool& bOutDiffPose, bool bDisableMissingBindPoseWarning, bool & bUseTime0AsRefPose, FbxNode *SkeletalMeshNode)
{
	bOutDiffPose = false;
	int32 SkelType = 0; // 0 for skeletal mesh, 1 for rigid mesh
	FbxNode* Link = NULL;
	FbxArray<FbxPose*> PoseArray;
	TArray<FbxCluster*> ClusterArray;
	
	if (NodeArray[0]->GetMesh()->GetDeformerCount(FbxDeformer::eSkin) == 0)
	{
		SkelType = 1;
		Link = NodeArray[0];
		RecursiveBuildSkeleton(GetRootSkeleton(Link),SortedLinks);
	}
	else
	{
		// get bindpose and clusters from FBX skeleton
		
		// let's put the elements to their bind pose! (and we restore them after
		// we have built the ClusterInformation.
		int32 Default_NbPoses = SdkManager->GetBindPoseCount(Scene);
		// If there are no BindPoses, the following will generate them.
		//SdkManager->CreateMissingBindPoses(Scene);

		//if we created missing bind poses, update the number of bind poses
		int32 NbPoses = SdkManager->GetBindPoseCount(Scene);

		if ( NbPoses != Default_NbPoses)
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("FbxSkeletaLMeshimport_SceneMissingBinding", "The imported scene has no initial binding position (Bind Pose) for the skin. The plug-in will compute one automatically. However, it may create unexpected results.")), FFbxErrors::SkeletalMesh_NoBindPoseInScene);
		}

		//
		// create the bones / skinning
		//

		for ( int32 i = 0; i < NodeArray.Num(); i++)
		{
			FbxMesh* FbxMesh = NodeArray[i]->GetMesh();
			const int32 SkinDeformerCount = FbxMesh->GetDeformerCount(FbxDeformer::eSkin);
			for (int32 DeformerIndex=0; DeformerIndex < SkinDeformerCount; DeformerIndex++)
			{
				FbxSkin* Skin = (FbxSkin*)FbxMesh->GetDeformer(DeformerIndex, FbxDeformer::eSkin);
				for ( int32 ClusterIndex = 0; ClusterIndex < Skin->GetClusterCount(); ClusterIndex++)
				{
					ClusterArray.Add(Skin->GetCluster(ClusterIndex));
				}
			}
		}

		if (ClusterArray.Num() == 0)
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("FbxSkeletaLMeshimport_NoAssociatedCluster", "No associated clusters")), FFbxErrors::SkeletalMesh_NoAssociatedCluster);
			return false;
		}

		// get bind pose
		if(RetrievePoseFromBindPose(NodeArray, PoseArray) == false)
		{
			if (!GIsAutomationTesting)
				UE_LOG(LogFbx, Warning, TEXT("Getting valid bind pose failed. Try to recreate bind pose"));
			// if failed, delete bind pose, and retry.
			const int32 PoseCount = Scene->GetPoseCount();
			for(int32 PoseIndex = PoseCount-1; PoseIndex >= 0; --PoseIndex)
			{
				FbxPose* CurrentPose = Scene->GetPose(PoseIndex);

				// current pose is bind pose, 
				if(CurrentPose && CurrentPose->IsBindPose())
				{
					Scene->RemovePose(PoseIndex);
					CurrentPose->Destroy();
				}
			}

			SdkManager->CreateMissingBindPoses(Scene);
			if ( RetrievePoseFromBindPose(NodeArray, PoseArray) == false)
			{
				if (!GIsAutomationTesting)
					UE_LOG(LogFbx, Warning, TEXT("Recreating bind pose failed."));
			}
			else
			{
				if (!GIsAutomationTesting)
					UE_LOG(LogFbx, Warning, TEXT("Recreating bind pose succeeded."));
			}
		}

		// recurse through skeleton and build ordered table
		BuildSkeletonSystem(ClusterArray, SortedLinks);
	}

	// error check
	// if no bond is found
	if(SortedLinks.Num() == 0)
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_NoBone", "'{0}' has no bones"), FText::FromString(NodeArray[0]->GetName()))), FFbxErrors::SkeletalMesh_NoBoneFound);
		return false;
	}

	// if no bind pose is found
	if (!bUseTime0AsRefPose && PoseArray.GetCount() == 0)
	{
		// add to tokenized error message
		if (ImportOptions->bImportScene)
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_InvalidBindPose", "Skeletal Mesh '{0}' dont have a bind pose. Scene import do not support yet time 0 as bind pose, there will be no bind pose import"), FText::FromString(NodeArray[0]->GetName()))), FFbxErrors::SkeletalMesh_InvalidBindPose);
		}
		else
		{
			if (!GIsAutomationTesting)
			{
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("FbxSkeletaLMeshimport_MissingBindPose", "Could not find the bind pose.  It will use time 0 as bind pose.")), FFbxErrors::SkeletalMesh_InvalidBindPose);
			}
			bUseTime0AsRefPose = true;
		}
	}

	int32 LinkIndex;

	// Check for duplicate bone names and issue a warning if found
	for(LinkIndex = 0; LinkIndex < SortedLinks.Num(); ++LinkIndex)
	{
		Link = SortedLinks[LinkIndex];

		for(int32 AltLinkIndex = LinkIndex+1; AltLinkIndex < SortedLinks.Num(); ++AltLinkIndex)
		{
			FbxNode* AltLink = SortedLinks[AltLinkIndex];

			if(FCStringAnsi::Strcmp(Link->GetName(), AltLink->GetName()) == 0)
			{
				FString RawBoneName = UTF8_TO_TCHAR(Link->GetName());
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("Error_DuplicateBoneName", "Error, Could not import {0}.\nDuplicate bone name found ('{1}'). Each bone must have a unique name."),
					FText::FromString(NodeArray[0]->GetName()), FText::FromString(RawBoneName))), FFbxErrors::SkeletalMesh_DuplicateBones);
				return false;
			}
		}
	}

	FbxArray<FbxAMatrix> GlobalsPerLink;
	GlobalsPerLink.Grow(SortedLinks.Num());
	GlobalsPerLink[0].SetIdentity();

	bool GlobalLinkFoundFlag;
	FbxVector4 LocalLinkT;
	FbxQuaternion LocalLinkQ;
	FbxVector4	LocalLinkS;

	bool bAnyLinksNotInBindPose = false;
	FString LinksWithoutBindPoses;
	int32 NumberOfRoot = 0;

	int32 RootIdx = -1;

	for (LinkIndex=0; LinkIndex<SortedLinks.Num(); LinkIndex++)
	{
		// Add a bone for each FBX Link
		ImportData.RefBonesBinary.Add( VBone() );

		Link = SortedLinks[LinkIndex];

		// get the link parent and children
		int32 ParentIndex = INDEX_NONE; // base value for root if no parent found
		FbxNode* LinkParent = Link->GetParent();
		if (LinkIndex)
		{
			for (int32 ll=0; ll<LinkIndex; ++ll) // <LinkIndex because parent is guaranteed to be before child in sortedLink
			{
				FbxNode* Otherlink	= SortedLinks[ll];
				if (Otherlink == LinkParent)
				{
					ParentIndex = ll;
					break;
				}
			}
		}
		
		// see how many root this has
		// if more than 
		if (ParentIndex == INDEX_NONE)
		{
			++NumberOfRoot;
			RootIdx = LinkIndex;
			if (NumberOfRoot > 1)
			{
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("MultipleRootsFound", "Multiple roots are found in the bone hierarchy. We only support single root bone.")), FFbxErrors::SkeletalMesh_MultipleRoots);
				return false;
			}
		}

		GlobalLinkFoundFlag = false;
		if (!SkelType) //skeletal mesh
		{
			// there are some links, they have no cluster, but in bindpose
			if (PoseArray.GetCount())
			{
				for (int32 PoseIndex = 0; PoseIndex < PoseArray.GetCount(); PoseIndex++)
				{
					int32 PoseLinkIndex = PoseArray[PoseIndex]->Find(Link);
					if (PoseLinkIndex>=0)
					{
						FbxMatrix NoneAffineMatrix = PoseArray[PoseIndex]->GetMatrix(PoseLinkIndex);
						FbxAMatrix Matrix = *(FbxAMatrix*)(double*)&NoneAffineMatrix;
						GlobalsPerLink[LinkIndex] = Matrix;
						GlobalLinkFoundFlag = true;
						break;
					}
				}
			}

			if (!GlobalLinkFoundFlag)
			{
				// since now we set use time 0 as ref pose this won't unlikely happen
				// but leaving it just in case it still has case where it's missing partial bind pose
				if(!bUseTime0AsRefPose && !bDisableMissingBindPoseWarning)
				{
					bAnyLinksNotInBindPose = true;
					LinksWithoutBindPoses +=  UTF8_TO_TCHAR(Link->GetName());
					LinksWithoutBindPoses +=  TEXT("  \n");
				}

				for (int32 ClusterIndex=0; ClusterIndex<ClusterArray.Num(); ClusterIndex++)
				{
					FbxCluster* Cluster = ClusterArray[ClusterIndex];
					if (Link == Cluster->GetLink())
					{
						Cluster->GetTransformLinkMatrix(GlobalsPerLink[LinkIndex]);
						GlobalLinkFoundFlag = true;
						break;
					}
				}
			}
		}

		if (!GlobalLinkFoundFlag)
		{
			GlobalsPerLink[LinkIndex] = Link->EvaluateGlobalTransform();
		}
		
		if (bUseTime0AsRefPose && !ImportOptions->bImportScene)
		{
			FbxAMatrix& T0Matrix = Scene->GetAnimationEvaluator()->GetNodeGlobalTransform(Link, 0);
			if (GlobalsPerLink[LinkIndex] != T0Matrix)
			{
				bOutDiffPose = true;
			}
			
			GlobalsPerLink[LinkIndex] = T0Matrix;
		}

		//Add the join orientation
		GlobalsPerLink[LinkIndex] = GlobalsPerLink[LinkIndex] * FFbxDataConverter::GetJointPostConversionMatrix();
		if (LinkIndex)
		{
			FbxAMatrix	Matrix;
			Matrix = GlobalsPerLink[ParentIndex].Inverse() * GlobalsPerLink[LinkIndex];
			LocalLinkT = Matrix.GetT();
			LocalLinkQ = Matrix.GetQ();
			LocalLinkS = Matrix.GetS();
		}
		else	// skeleton root
		{
			// for root, this is global coordinate
			LocalLinkT = GlobalsPerLink[LinkIndex].GetT();
			LocalLinkQ = GlobalsPerLink[LinkIndex].GetQ();
			LocalLinkS = GlobalsPerLink[LinkIndex].GetS();
		}
		
		// set bone
		VBone& Bone = ImportData.RefBonesBinary[LinkIndex];
		FString BoneName;

		const char* LinkName = Link->GetName();
		BoneName = UTF8_TO_TCHAR( MakeName( LinkName ) );
		Bone.Name = BoneName;

		VJointPos& JointMatrix = Bone.BonePos;
		FbxSkeleton* Skeleton = Link->GetSkeleton();
		if (Skeleton)
		{
			JointMatrix.Length = Converter.ConvertDist(Skeleton->LimbLength.Get());
			JointMatrix.XSize = Converter.ConvertDist(Skeleton->Size.Get());
			JointMatrix.YSize = Converter.ConvertDist(Skeleton->Size.Get());
			JointMatrix.ZSize = Converter.ConvertDist(Skeleton->Size.Get());
		}
		else
		{
			JointMatrix.Length = 1. ;
			JointMatrix.XSize = 100. ;
			JointMatrix.YSize = 100. ;
			JointMatrix.ZSize = 100. ;
		}

		// get the link parent and children
		Bone.ParentIndex = ParentIndex;
		Bone.NumChildren = 0;
		for (int32 ChildIndex=0; ChildIndex<Link->GetChildCount(); ChildIndex++)
		{
			FbxNode* Child = Link->GetChild(ChildIndex);
			if (IsUnrealBone(Child))
			{
				Bone.NumChildren++;
			}
		}

		JointMatrix.Transform.SetTranslation(Converter.ConvertPos(LocalLinkT));
		JointMatrix.Transform.SetRotation(Converter.ConvertRotToQuat(LocalLinkQ));
		JointMatrix.Transform.SetScale3D(Converter.ConvertScale(LocalLinkS));
	}
	
	//In case we do a scene import we need a relative to skeletal mesh transform instead of a global
	if (ImportOptions->bImportScene && !ImportOptions->bTransformVertexToAbsolute)
	{
		FbxAMatrix GlobalSkeletalNodeFbx = Scene->GetAnimationEvaluator()->GetNodeGlobalTransform(SkeletalMeshNode, 0);
		FTransform GlobalSkeletalNode;
		GlobalSkeletalNode.SetFromMatrix(Converter.ConvertMatrix(GlobalSkeletalNodeFbx.Inverse()));

		VBone& RootBone = ImportData.RefBonesBinary[RootIdx];
		FTransform& RootTransform = RootBone.BonePos.Transform;
		RootTransform.SetFromMatrix(RootTransform.ToMatrixWithScale() * GlobalSkeletalNode.ToMatrixWithScale());
	}

	if(TemplateData)
	{
		FbxAMatrix FbxAddedMatrix;
		BuildFbxMatrixForImportTransform(FbxAddedMatrix, TemplateData);
		FMatrix AddedMatrix = Converter.ConvertMatrix(FbxAddedMatrix);

		VBone& RootBone = ImportData.RefBonesBinary[RootIdx];
		FTransform& RootTransform = RootBone.BonePos.Transform;
		RootTransform.SetFromMatrix(RootTransform.ToMatrixWithScale() * AddedMatrix);
	}
	
	if(bAnyLinksNotInBindPose)
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_BonesAreMissingFromBindPose", "The following bones are missing from the bind pose:\n{0}\nThis can happen for bones that are not vert weighted. If they are not in the correct orientation after importing,\nplease set the \"Use T0 as ref pose\" option or add them to the bind pose and reimport the skeletal mesh."), FText::FromString(LinksWithoutBindPoses))), FFbxErrors::SkeletalMesh_BonesAreMissingFromBindPose);
	}
	
	return true;
}

bool UnFbx::FFbxImporter::FillSkeletalMeshImportData(TArray<FbxNode*>& NodeArray, UFbxSkeletalMeshImportData* TemplateImportData, TArray<FbxShape*> *FbxShapeArray, FSkeletalMeshImportData* OutData, TArray<FName> &LastImportedMaterialNames)
{
	if (NodeArray.Num() == 0)
	{
		return false;
	}

	int32 SkelType = 0; // 0 for skeletal mesh, 1 for rigid mesh

	FbxNode* Node = NodeArray[0];
	// find the mesh by its name
	FbxMesh* FbxMesh = Node->GetMesh();

	if (OutData == nullptr)
	{
		return false;
	}

	if (FbxMesh->GetDeformerCount(FbxDeformer::eSkin) == 0)
	{
		SkelType = 1;
	}

	// Fill with data from buffer - contains the full .FBX file. 	
	FSkeletalMeshImportData* SkelMeshImportDataPtr = OutData;


	TArray<FbxNode*> SortedLinkArray;
	FbxArray<FbxAMatrix> GlobalsPerLink;

	SkelMeshImportDataPtr->bUseT0AsRefPose = ImportOptions->bUseT0AsRefPose;
	// Note: importing morph data causes additional passes through this function, so disable the warning dialogs
	// from popping up again on each additional pass.  
	if (!ImportBone(NodeArray, *SkelMeshImportDataPtr, TemplateImportData, SortedLinkArray, SkelMeshImportDataPtr->bDiffPose, (FbxShapeArray != nullptr), SkelMeshImportDataPtr->bUseT0AsRefPose, Node))
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FbxSkeletaLMeshimport_MultipleRootFound", "Multiple roots found")), FFbxErrors::SkeletalMesh_MultipleRoots);
		return false;
	}

 	FbxNode* SceneRootNode = Scene->GetRootNode();
 	if(SceneRootNode && TemplateImportData)
 	{
 		ApplyTransformSettingsToFbxNode(SceneRootNode, TemplateImportData);
 	}

	// Create a list of all unique fbx materials.  This needs to be done as a separate pass before reading geometry
	// so that we know about all possible materials before assigning material indices to each triangle
	TArray<FbxSurfaceMaterial*> FbxMaterials;
	for (int32 NodeIndex = 0; NodeIndex < NodeArray.Num(); ++NodeIndex)
	{
		Node = NodeArray[NodeIndex];

		int32 MaterialCount = Node->GetMaterialCount();

		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			FbxSurfaceMaterial* FbxMaterial = Node->GetMaterial(MaterialIndex);
			if (!FbxMaterials.Contains(FbxMaterial))
			{
				FbxMaterials.Add(FbxMaterial);

				VMaterial NewMaterial;

				NewMaterial.MaterialImportName = UTF8_TO_TCHAR(MakeName(FbxMaterial->GetName()));
				// Add an entry for each unique material
				SkelMeshImportDataPtr->Materials.Add(NewMaterial);
			}
		}
	}

	for (int32 NodeIndex = 0; NodeIndex < NodeArray.Num(); ++NodeIndex)
	{
		Node = NodeArray[NodeIndex];
		FbxNode *RootNode = NodeArray[0];
		FbxMesh = Node->GetMesh();
		FbxSkin* Skin = (FbxSkin*)FbxMesh->GetDeformer(0, FbxDeformer::eSkin);
		FbxShape* FbxShape = nullptr;
		if (FbxShapeArray)
		{
			FbxShape = (*FbxShapeArray)[NodeIndex];
		}

		// NOTE: This function may invalidate FbxMesh and set it to point to a an updated version
		if (!FillSkelMeshImporterFromFbx( *SkelMeshImportDataPtr, FbxMesh, Skin, FbxShape, SortedLinkArray, FbxMaterials, RootNode) )
		{
			return false;
		}

		if (SkelMeshImportDataPtr->bUseT0AsRefPose && SkelMeshImportDataPtr->bDiffPose && !ImportOptions->bImportScene)
		{
			// deform skin vertex to the frame 0 from bind pose
			SkinControlPointsToPose(*SkelMeshImportDataPtr, FbxMesh, FbxShape, true);
		}
	}

	CleanUpUnusedMaterials(*SkelMeshImportDataPtr);

	if (LastImportedMaterialNames.Num() > 0)
	{
		SetMaterialOrderByName(*SkelMeshImportDataPtr, LastImportedMaterialNames);
	}
	else
	{
		// reorder material according to "SKinXX" in material name
		SetMaterialSkinXXOrder(*SkelMeshImportDataPtr);
	}

	if (ImportOptions->bPreserveSmoothingGroups)
	{
		bool bDuplicateUnSmoothWedges = (ImportOptions->NormalGenerationMethod != EFBXNormalGenerationMethod::MikkTSpace);
		DoUnSmoothVerts(*SkelMeshImportDataPtr, bDuplicateUnSmoothWedges);
	}
	else
	{
		SkelMeshImportDataPtr->PointToRawMap.AddUninitialized(SkelMeshImportDataPtr->Points.Num());
		for (int32 PointIdx = 0; PointIdx < SkelMeshImportDataPtr->Points.Num(); PointIdx++)
		{
			SkelMeshImportDataPtr->PointToRawMap[PointIdx] = PointIdx;
		}
	}

	return true;
}

bool UnFbx::FFbxImporter::FillSkeletalMeshImportPoints(FSkeletalMeshImportData* OutData, FbxNode* RootNode, FbxNode* Node, FbxShape* FbxShape)
{
	FbxMesh* FbxMesh = Node->GetMesh();

	const int32 ControlPointsCount = FbxMesh->GetControlPointsCount();
	const int32 ExistPointNum = OutData->Points.Num();
	OutData->Points.AddUninitialized(ControlPointsCount);

	// Construct the matrices for the conversion from right handed to left handed system
	FbxAMatrix TotalMatrix = ComputeSkeletalMeshTotalMatrix(Node, RootNode);

	int32 ControlPointsIndex;
	bool bInvalidPositionFound = false;
	for( ControlPointsIndex = 0 ; ControlPointsIndex < ControlPointsCount ;ControlPointsIndex++ )
	{
		FbxVector4 Position;
		if (FbxShape)
		{
			Position = FbxShape->GetControlPoints()[ControlPointsIndex];
		}
		else
		{
			Position = FbxMesh->GetControlPoints()[ControlPointsIndex];
		}																	 

		FbxVector4 FinalPosition;
		FinalPosition = TotalMatrix.MultT(Position);
		FVector ConvertedPosition = Converter.ConvertPos(FinalPosition);

		// ensure user when this happens if attached to debugger
		if (!ensure(ConvertedPosition.ContainsNaN() == false))
		{
			if (!bInvalidPositionFound)
			{
				bInvalidPositionFound = true;
			}

			ConvertedPosition = FVector::ZeroVector;
		}

		OutData->Points[ ControlPointsIndex + ExistPointNum ] = ConvertedPosition;
	}

	if (bInvalidPositionFound)
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning,
			FText::Format(LOCTEXT("FbxSkeletaLMeshimport_InvalidPosition", "Invalid position (NaN or Inf) found from source position for mesh '{0}'. Please verify if the source asset contains valid position. "),
				FText::FromString(FbxMesh->GetName()))), FFbxErrors::SkeletalMesh_InvalidPosition);
	}

	return true;
}

bool UnFbx::FFbxImporter::GatherPointsForMorphTarget(FSkeletalMeshImportData* OutData, TArray<FbxNode*>& NodeArray, TArray< FbxShape* >* FbxShapeArray, TSet<uint32>& ModifiedPoints)
{
	FSkeletalMeshImportData NewImportData = *OutData;
	NewImportData.Points.Empty();

	FbxNode* const RootNode = NodeArray[0];

	for ( int32 NodeIndex = 0; NodeIndex < NodeArray.Num(); ++NodeIndex )
	{
		FbxNode* Node = NodeArray[NodeIndex];
		FbxMesh* FbxMesh = Node->GetMesh();

		FbxShape* FbxShape = nullptr;
		if (FbxShapeArray)
		{
			FbxShape = (*FbxShapeArray)[NodeIndex];
		}

		FillSkeletalMeshImportPoints( &NewImportData, RootNode, Node, FbxShape );

		if (OutData->bUseT0AsRefPose && OutData->bDiffPose && !ImportOptions->bImportScene)
		{
			// deform skin vertex to the frame 0 from bind pose
			SkinControlPointsToPose(NewImportData, FbxMesh, FbxShape, true);
		}
	}

	for ( int32 PointIdx = 0; PointIdx < OutData->Points.Num(); ++PointIdx )
	{
		int32 OriginalPointIdx = OutData->PointToRawMap[ PointIdx ];

		if ( ( NewImportData.Points[ OriginalPointIdx ] - OutData->Points[ PointIdx ] ).SizeSquared() > FMath::Square( THRESH_VECTORS_ARE_NEAR ) ) // THRESH_POINTS_ARE_NEAR is too big, we might not be recomputing some normals that can have changed significantly
		{
			ModifiedPoints.Add( PointIdx );
		}

		OutData->Points[ PointIdx ] = NewImportData.Points[ OriginalPointIdx ];
	}

	return true;
}

void UnFbx::FFbxImporter::FillLastImportMaterialNames(TArray<FName> &LastImportedMaterialNames, USkeletalMesh* BaseSkelMesh, TArray<FName> *OrderedMaterialNames)
{
	if (OrderedMaterialNames == nullptr && BaseSkelMesh)
	{
		int32 NoneNameCount = 0;
		for (const FSkeletalMaterial &Material : BaseSkelMesh->Materials)
		{
			if (Material.ImportedMaterialSlotName == NAME_None)
			{
				NoneNameCount++;
			}
			LastImportedMaterialNames.Add(Material.ImportedMaterialSlotName);
		}
		if (NoneNameCount >= LastImportedMaterialNames.Num())
		{
			LastImportedMaterialNames.Empty();
		}
	}
	else if (OrderedMaterialNames)
	{
		//Copy the ordered material name parameter
		LastImportedMaterialNames = (*OrderedMaterialNames);
	}

	//If the imported model is using skinxx workflow just empty LastImportedMaterialNames array
	if (LastImportedMaterialNames.Num() > 0)
	{
		int32 SkinXXNameCount = 0;
		for (FName MaterialName : LastImportedMaterialNames)
		{
			if (MaterialName == NAME_None)
			{
				continue;
			}
			FString ImportedMaterialName = MaterialName.ToString();
			int32 Offset = ImportedMaterialName.Find(TEXT("_SKIN"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (Offset != INDEX_NONE)
			{
				FString SkinXXNumber = ImportedMaterialName.Right(ImportedMaterialName.Len() - (Offset + 1)).RightChop(4);

				if (SkinXXNumber.IsNumeric())
				{
					SkinXXNameCount++;
				}
			}
		}
		//If we have some skinxx suffixe we don't use the name to reorder
		if (SkinXXNameCount == LastImportedMaterialNames.Num())
		{
			LastImportedMaterialNames.Empty();
		}
	}
}

USkeletalMesh* UnFbx::FFbxImporter::ImportSkeletalMesh(FImportSkeletalMeshArgs &ImportSkeletalMeshArgs)
{
	if (ImportSkeletalMeshArgs.NodeArray.Num() == 0)
	{
		return nullptr;
	}

	int32 SkelType = 0; // 0 for skeletal mesh, 1 for rigid mesh
	
	FbxNode* Node = ImportSkeletalMeshArgs.NodeArray[0];
	// find the mesh by its name
	FbxMesh* FbxMesh = Node->GetMesh();

	if( !FbxMesh )
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_NodeInvalidSkeletalMesh", "Fbx node: '{0}' is not a valid skeletal mesh"), FText::FromString(Node->GetName()))), FFbxErrors::Generic_Mesh_MeshNotFound);
		return nullptr;
	}
	if (FbxMesh->GetDeformerCount(FbxDeformer::eSkin) == 0)
	{
		SkelType = 1;
	}

	// warning for missing smoothing group info
	CheckSmoothingInfo(FbxMesh);

	Parent = ImportSkeletalMeshArgs.InParent;
	

	struct ExistingSkelMeshData* ExistSkelMeshDataPtr = nullptr;

	USkeletalMesh* ExistingSkelMesh = nullptr;
	if ( !ImportSkeletalMeshArgs.FbxShapeArray  )
	{
		UObject* ExistingObject = StaticFindObjectFast(UObject::StaticClass(), ImportSkeletalMeshArgs.InParent, ImportSkeletalMeshArgs.Name, false, false, RF_NoFlags, EInternalObjectFlags::PendingKill);
		ExistingSkelMesh = Cast<USkeletalMesh>(ExistingObject);

		if (!ExistingSkelMesh && ExistingObject)
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_OverlappingName", "Same name but different class: '{0}' already exists"), FText::FromString(ExistingObject->GetName()))), FFbxErrors::Generic_SameNameAssetExists);
			return NULL;
		}
	}

	USkeletalMesh* SkeletalMesh = nullptr;
	if (!ExistingSkelMesh)
	{
		// When we are not re-importing we want to create the mesh here to be sure there is no material
		// or texture that will be create with the same name
		SkeletalMesh = NewObject<USkeletalMesh>(ImportSkeletalMeshArgs.InParent, ImportSkeletalMeshArgs.Name, ImportSkeletalMeshArgs.Flags);
	}

	FSkeletalMeshImportData TempData;
	// Fill with data from buffer - contains the full .FBX file. 	
	FSkeletalMeshImportData* SkelMeshImportDataPtr = &TempData;
	if( ImportSkeletalMeshArgs.OutData )
	{
		SkelMeshImportDataPtr = ImportSkeletalMeshArgs.OutData;
	}
	
	TArray<FName> LastImportedMaterialNames;
	if (!ImportOptions->bResetMaterialSlots)
	{
		FillLastImportMaterialNames(LastImportedMaterialNames, ExistingSkelMesh, ImportSkeletalMeshArgs.OrderedMaterialNames);
	}

	//////////////////////////////////////////////////////////////////////////
	// We must do a maximum of fail test before backing up the data since the backup is destructive on the existing skeletal mesh.
	// See the comment later when we call the following function (SaveExistingSkelMeshData)

	if (FillSkeletalMeshImportData(ImportSkeletalMeshArgs.NodeArray, ImportSkeletalMeshArgs.TemplateImportData, ImportSkeletalMeshArgs.FbxShapeArray, SkelMeshImportDataPtr, LastImportedMaterialNames) == false)
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("FbxSkeletaLMeshimport_FillupImportData", "Get Import Data has failed.")), FFbxErrors::SkeletalMesh_FillImportDataFailed);
		if (SkeletalMesh)
		{
			SkeletalMesh->ClearFlags(RF_Standalone);
			SkeletalMesh->Rename(NULL, GetTransientPackage(), REN_DontCreateRedirectors);
		}
		return nullptr;
	}

	// Create initial bounding box based on expanded version of reference pose for meshes without physics assets. Can be overridden by artist.
	FBox BoundingBox(SkelMeshImportDataPtr->Points.GetData(), SkelMeshImportDataPtr->Points.Num());
	FBox Temp = BoundingBox;
	FVector MidMesh = 0.5f*(Temp.Min + Temp.Max);
	BoundingBox.Min = Temp.Min + 1.0f*(Temp.Min - MidMesh);
	BoundingBox.Max = Temp.Max + 1.0f*(Temp.Max - MidMesh);
	// Tuck up the bottom as this rarely extends lower than a reference pose's (e.g. having its feet on the floor).
	// Maya has Y in the vertical, other packages have Z.
	//BEN const int32 CoordToTuck = bAssumeMayaCoordinates ? 1 : 2;
	//BEN BoundingBox.Min[CoordToTuck]	= Temp.Min[CoordToTuck] + 0.1f*(Temp.Min[CoordToTuck] - MidMesh[CoordToTuck]);
	BoundingBox.Min[2] = Temp.Min[2] + 0.1f*(Temp.Min[2] - MidMesh[2]);
	const FVector BoundingBoxSize = BoundingBox.GetSize();

	if (SkelMeshImportDataPtr->Points.Num() > 2 && BoundingBoxSize.X < THRESH_POINTS_ARE_SAME && BoundingBoxSize.Y < THRESH_POINTS_ARE_SAME && BoundingBoxSize.Z < THRESH_POINTS_ARE_SAME)
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_ErrorMeshTooSmall", "Cannot import this mesh, the bounding box of this mesh is smaller then the supported threshold[{0}]."), FText::FromString(FString::Printf(TEXT("%f"), THRESH_POINTS_ARE_SAME)))), FFbxErrors::SkeletalMesh_FillImportDataFailed);
		if (SkeletalMesh)
		{
			SkeletalMesh->ClearFlags(RF_Standalone);
			SkeletalMesh->Rename(NULL, GetTransientPackage(), REN_DontCreateRedirectors);
		}
		return nullptr;
	}

	TArray<ClothingAssetUtils::FClothingAssetMeshBinding> ClothingBindings;

	//Backup the data before importing the new one
	if (ExistingSkelMesh)
	{
		ClothingAssetUtils::GetMeshClothingAssetBindings(ExistingSkelMesh, ClothingBindings);

		for(ClothingAssetUtils::FClothingAssetMeshBinding& Binding : ClothingBindings)
		{
			Binding.Asset->UnbindFromSkeletalMesh(ExistingSkelMesh, Binding.LODIndex);
		}

		ExistingSkelMesh->PreEditChange(NULL);
		//The backup of the skeletal mesh data empty the LOD array in the ImportedResource of the skeletal mesh
		//If the import fail after this step the editor can crash when updating the bone later since the LODModel will not exist anymore
		ExistSkelMeshDataPtr = SaveExistingSkelMeshData(ExistingSkelMesh, !ImportOptions->bImportMaterials, ImportSkeletalMeshArgs.LodIndex);
	}

	if (SkeletalMesh == nullptr)
	{
		// Create the new mesh after saving the old data, since it will replace the old skeletalmesh
		// This should happen only when doing a re-import. Otherwise the skeletal mesh is create before.
		SkeletalMesh = NewObject<USkeletalMesh>(ImportSkeletalMeshArgs.InParent, ImportSkeletalMeshArgs.Name, ImportSkeletalMeshArgs.Flags);
	}

	SkeletalMesh->PreEditChange(NULL);

	// process materials from import data
	ProcessImportMeshMaterials(SkeletalMesh->Materials, *SkelMeshImportDataPtr);

	// process reference skeleton from import data
	int32 SkeletalDepth = 0;
	if (!ProcessImportMeshSkeleton(SkeletalMesh->Skeleton, SkeletalMesh->RefSkeleton, SkeletalDepth, *SkelMeshImportDataPtr))
	{
		SkeletalMesh->ClearFlags(RF_Standalone);
		SkeletalMesh->Rename(NULL, GetTransientPackage());
		return nullptr;
	}
	
	if (!GIsAutomationTesting)
		UE_LOG(LogFbx, Warning, TEXT("Bones digested - %i  Depth of hierarchy - %i"), SkeletalMesh->RefSkeleton.GetNum(), SkeletalDepth);

	// process bone influences from import data
	ProcessImportMeshInfluences(*SkelMeshImportDataPtr);

	FSkeletalMeshResource* ImportedResource = SkeletalMesh->GetImportedResource();
	check(ImportedResource->LODModels.Num() == 0);
	ImportedResource->LODModels.Empty();
	new(ImportedResource->LODModels)FStaticLODModel();

	SkeletalMesh->LODInfo.Empty();
	SkeletalMesh->LODInfo.AddZeroed();
	SkeletalMesh->LODInfo[0].LODHysteresis = 0.02f;
	FSkeletalMeshOptimizationSettings Settings;
	// set default reduction settings values
	SkeletalMesh->LODInfo[0].ReductionSettings = Settings;

	SkeletalMesh->SetImportedBounds(FBoxSphereBounds(BoundingBox));

	// Store whether or not this mesh has vertex colors
	SkeletalMesh->bHasVertexColors = SkelMeshImportDataPtr->bHasVertexColors;

	FStaticLODModel& LODModel = ImportedResource->LODModels[0];
	
	// Pass the number of texture coordinate sets to the LODModel.  Ensure there is at least one UV coord
	LODModel.NumTexCoords = FMath::Max<uint32>(1, SkelMeshImportDataPtr->NumTexCoords);

	// Array of re-import contexts for components using this mesh
	// Will unregister before import, then re-register afterwards
	TIndirectArray<FComponentReregisterContext> ComponentContexts;

	if(ImportSkeletalMeshArgs.bCreateRenderData )
	{
		TArray<FVector> LODPoints;
		TArray<FMeshWedge> LODWedges;
		TArray<FMeshFace> LODFaces;
		TArray<FVertInfluence> LODInfluences;
		TArray<int32> LODPointToRawMap;
		SkelMeshImportDataPtr->CopyLODImportData(LODPoints,LODWedges,LODFaces,LODInfluences,LODPointToRawMap);

		IMeshUtilities::MeshBuildOptions BuildOptions;
		BuildOptions.bKeepOverlappingVertices = ImportOptions->bKeepOverlappingVertices;
		BuildOptions.bComputeNormals = !ImportOptions->ShouldImportNormals() || !SkelMeshImportDataPtr->bHasNormals;
		BuildOptions.bComputeTangents = !ImportOptions->ShouldImportTangents() || !SkelMeshImportDataPtr->bHasTangents;
		BuildOptions.bUseMikkTSpace = (ImportOptions->NormalGenerationMethod == EFBXNormalGenerationMethod::MikkTSpace) && (!ImportOptions->ShouldImportNormals() || !ImportOptions->ShouldImportTangents());
		BuildOptions.bRemoveDegenerateTriangles = false;

		IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
		
		TArray<FText> WarningMessages;
		TArray<FName> WarningNames;
		// Create actual rendering data.
		bool bBuildSuccess = MeshUtilities.BuildSkeletalMesh(ImportedResource->LODModels[0],SkeletalMesh->RefSkeleton,LODInfluences,LODWedges,LODFaces,LODPoints,LODPointToRawMap,BuildOptions, &WarningMessages, &WarningNames);

		// temporary hack of message/names, should be one token or a struct
		if(WarningMessages.Num() > 0 && WarningNames.Num() == WarningMessages.Num())
		{
			EMessageSeverity::Type MessageSeverity = bBuildSuccess ? EMessageSeverity::Warning : EMessageSeverity::Error;

			for(int32 MessageIdx = 0; MessageIdx<WarningMessages.Num(); ++MessageIdx)
			{
				AddTokenizedErrorMessage(FTokenizedMessage::Create(MessageSeverity, WarningMessages[MessageIdx]), WarningNames[MessageIdx]);
			}
		}

		if( !bBuildSuccess )
		{
			SkeletalMesh->MarkPendingKill();
			return NULL;
		}

		// Presize the per-section shadow casting array with the number of sections in the imported LOD.
		const int32 NumSections = LODModel.Sections.Num();
		for ( int32 SectionIndex = 0 ; SectionIndex < NumSections ; ++SectionIndex )
		{
			SkeletalMesh->LODInfo[0].TriangleSortSettings.AddZeroed();
		}

		//Get the last fbx file data need for reimport
		if (ImportSkeletalMeshArgs.ImportMaterialOriginalNameData)
		{
			for (FSkeletalMaterial SkeletalMaterial : SkeletalMesh->Materials)
			{
				ImportSkeletalMeshArgs.ImportMaterialOriginalNameData->Add(SkeletalMaterial.ImportedMaterialSlotName);
			}
		}
		if (ImportSkeletalMeshArgs.ImportMeshSectionsData)
		{
			if (SkeletalMesh->LODInfo[0].LODMaterialMap.Num() >= NumSections)
			{
				for (int32 SectionMaterialIndex : SkeletalMesh->LODInfo[0].LODMaterialMap)
				{
					ImportSkeletalMeshArgs.ImportMeshSectionsData->SectionOriginalMaterialName.Add(SkeletalMesh->Materials[SectionMaterialIndex].ImportedMaterialSlotName);
				}
			}
			else
			{
				for (const FSkelMeshSection &SkelMeshSection : ImportedResource->LODModels[0].Sections)
				{
					ImportSkeletalMeshArgs.ImportMeshSectionsData->SectionOriginalMaterialName.Add(SkeletalMesh->Materials[SkelMeshSection.MaterialIndex].ImportedMaterialSlotName);
				}
			}
		}

		// Store the current file path and timestamp for re-import purposes
		UFbxSkeletalMeshImportData* ImportData = UFbxSkeletalMeshImportData::GetImportDataForSkeletalMesh(SkeletalMesh, ImportSkeletalMeshArgs.TemplateImportData);
		SkeletalMesh->AssetImportData->Update(UFactory::GetCurrentFilename(), &Md5Hash);

		

		if (ExistSkelMeshDataPtr)
		{
			RestoreExistingSkelMeshData(ExistSkelMeshDataPtr, SkeletalMesh, ImportSkeletalMeshArgs.LodIndex, ImportOptions->bResetMaterialSlots, ImportOptions->bIsReimportPreview);
		}

		SkeletalMesh->CalculateInvRefMatrices();
		SkeletalMesh->PostEditChange();
		SkeletalMesh->MarkPackageDirty();

		// Now iterate over all skeletal mesh components re-initialising them.
		for (TObjectIterator<USkinnedMeshComponent> It; It; ++It)
		{
			USkinnedMeshComponent* SkinComp = *It;
			if (SkinComp->SkeletalMesh == SkeletalMesh)
			{
				new(ComponentContexts) FComponentReregisterContext(SkinComp);
			}
		}
	}

	if(ImportSkeletalMeshArgs.LodIndex == 0)
	{
		// see if we have skeleton set up
		// if creating skeleton, create skeleeton
		USkeleton* Skeleton = ImportOptions->SkeletonForAnimation;
		if (Skeleton == NULL)
		{
			FString ObjectName = FString::Printf(TEXT("%s_Skeleton"), *SkeletalMesh->GetName());
			Skeleton = CreateAsset<USkeleton>(ImportSkeletalMeshArgs.InParent->GetName(), ObjectName, true);
			if (!Skeleton)
			{
				// same object exists, try to see if it's skeleton, if so, load
				Skeleton = LoadObject<USkeleton>(ImportSkeletalMeshArgs.InParent, *ObjectName);

				// if not skeleton, we're done, we can't create skeleton with same name
				// @todo in the future, we'll allow them to rename
				if (!Skeleton)
				{
					AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_SkeletonRecreateError", "'{0}' already exists. It fails to recreate it."), FText::FromString(ObjectName))), FFbxErrors::SkeletalMesh_SkeletonRecreateError);
					return SkeletalMesh;
				}
			}
		}

		// merge bones to the selected skeleton
		if ( !Skeleton->MergeAllBonesToBoneTree( SkeletalMesh ) )
		{
			// We should only show the skeleton save toast once, not as many times as we have nodes to import
			bool bToastSaveMessage = false;
			if(bFirstMesh || (LastMergeBonesChoice != EAppReturnType::NoAll && LastMergeBonesChoice != EAppReturnType::YesAll))
			{
				if (!ImportOptions->bIsReimportPreview)
				{
				LastMergeBonesChoice = FMessageDialog::Open(EAppMsgType::YesNoYesAllNoAllCancel,
					LOCTEXT("SkeletonFailed_BoneMerge", "FAILED TO MERGE BONES:\n\n This could happen if significant hierarchical changes have been made\n"
						"e.g. inserting a bone between nodes.\nWould you like to regenerate the Skeleton from this mesh?\n\n"
						"***WARNING: THIS MAY INVALIDATE OR REQUIRE RECOMPRESSION OF ANIMATION DATA.***\n"));
				}
				else
				{
					LastMergeBonesChoice = EAppReturnType::NoAll;
				}
				bToastSaveMessage = true;
			}
			
			if(LastMergeBonesChoice == EAppReturnType::Cancel)
			{
				// User wants to cancel further importing
				if(ImportSkeletalMeshArgs.bCancelOperation != nullptr)
				{
					// Report back to calling code if we have a flag to set
					*ImportSkeletalMeshArgs.bCancelOperation = true;
				}
				return nullptr;
			}

			if (LastMergeBonesChoice == EAppReturnType::Yes || LastMergeBonesChoice == EAppReturnType::YesAll) 
			{
				if ( Skeleton->RecreateBoneTree( SkeletalMesh ) && bToastSaveMessage)
				{
					// @todo: this is a lot of message box but this requires user input and it can be very annoying to miss
					// make sure to go through all skeletalmesh and merge them also to recreate the issue. 
					if (FMessageDialog::Open(EAppMsgType::YesNo,
						LOCTEXT("Skeleton_ReAddAllMeshes", "Would you like to merge all SkeletalMeshes using this skeleton to ensure all bones are merged? This will require to load those SkeletalMeshes.")) == EAppReturnType::Yes)
					{
						FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
						TArray<FAssetData> SkeletalMeshAssetData;
						
						FARFilter ARFilter;
						ARFilter.ClassNames.Add(*USkeletalMesh::StaticClass()->GetName());
						FString& Value = ARFilter.TagsAndValues.Add(TEXT("Skeleton"));
						Value = FAssetData(Skeleton).GetExportTextName();

						IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
						if (AssetRegistry.GetAssets(ARFilter, SkeletalMeshAssetData))
						{
							// look through all skeletalmeshes that uses this skeleton
							for (int32 AssetId = 0; AssetId < SkeletalMeshAssetData.Num(); ++AssetId)
							{
								FAssetData& CurAssetData = SkeletalMeshAssetData[AssetId];
								const USkeletalMesh* ExtraSkeletalMesh = Cast<USkeletalMesh>(CurAssetData.GetAsset());
								if (SkeletalMesh != ExtraSkeletalMesh && ExtraSkeletalMesh && ExtraSkeletalMesh->IsPendingKill() == false)
								{
									// merge still can fail, then print message box
									if (Skeleton->MergeAllBonesToBoneTree(ExtraSkeletalMesh) == false)
									{
										// print warning
										FMessageDialog::Open(EAppMsgType::Ok,
											FText::Format(LOCTEXT("SkeletonRegenError_RemergingBones", "Failed to merge SkeletalMesh '{0}'."), FText::FromString(ExtraSkeletalMesh->GetName())));
									}
								}
							}
						}
					}

					FAssetNotifications::SkeletonNeedsToBeSaved(Skeleton);
				}
			}
		}
		else
		{
			// ask if they'd like to update their position form this mesh
			if ( ImportOptions->SkeletonForAnimation && ImportOptions->bUpdateSkeletonReferencePose )
			{
				Skeleton->UpdateReferencePoseFromMesh(SkeletalMesh);
				FAssetNotifications::SkeletonNeedsToBeSaved(Skeleton);
			}
		}

		if (SkeletalMesh->Skeleton != Skeleton)
		{
			SkeletalMesh->Skeleton = Skeleton;
			SkeletalMesh->MarkPackageDirty();
		}
		// Create PhysicsAsset if requested and if physics asset is null
		// We create the physic asset after we create the skeleton since we need the skeleton to correctly build it
		if (ImportOptions->bCreatePhysicsAsset)
		{
			if (SkeletalMesh->PhysicsAsset == NULL)
			{
				FString ObjectName = FString::Printf(TEXT("%s_PhysicsAsset"), *SkeletalMesh->GetName());
				UPhysicsAsset * NewPhysicsAsset = CreateAsset<UPhysicsAsset>(ImportSkeletalMeshArgs.InParent->GetName(), ObjectName, true);
				if (!NewPhysicsAsset)
				{
					AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_CouldNotCreatePhysicsAsset", "Could not create Physics Asset ('{0}') for '{1}'"), FText::FromString(ObjectName), FText::FromString(SkeletalMesh->GetName()))), FFbxErrors::SkeletalMesh_FailedToCreatePhyscisAsset);
				}
				else
				{
					FPhysAssetCreateParams NewBodyData;
					FText CreationErrorMessage;
					bool bSuccess = FPhysicsAssetUtils::CreateFromSkeletalMesh(NewPhysicsAsset, SkeletalMesh, NewBodyData, CreationErrorMessage);
					if (!bSuccess)
					{
						AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, CreationErrorMessage), FFbxErrors::SkeletalMesh_FailedToCreatePhyscisAsset);
						// delete the asset since we could not have create physics asset
						TArray<UObject*> ObjectsToDelete;
						ObjectsToDelete.Add(NewPhysicsAsset);
						ObjectTools::DeleteObjects(ObjectsToDelete, false);
					}
				}
			}
		}
		// if physics asset is selected
		else if (ImportOptions->PhysicsAsset)
		{
			SkeletalMesh->PhysicsAsset = ImportOptions->PhysicsAsset;
		}
	}

	// Reapply any clothing assets we had before the import
	FSkeletalMeshResource* NewMeshResource = SkeletalMesh->GetImportedResource();
	if(NewMeshResource)
	{
		for(ClothingAssetUtils::FClothingAssetMeshBinding& Binding : ClothingBindings)
		{
			if(NewMeshResource->LODModels.IsValidIndex(Binding.LODIndex) &&
			   NewMeshResource->LODModels[Binding.LODIndex].Sections.IsValidIndex(Binding.SectionIndex))
			{
				Binding.Asset->BindToSkeletalMesh(SkeletalMesh, Binding.LODIndex, Binding.SectionIndex, Binding.AssetInternalLodIndex);
			}
		}
	}

	// ComponentContexts will now go out of scope, causing components to be re-registered

	// ComponentContexts will now go out of scope, causing components to be re-registered

	return SkeletalMesh;
}

void UnFbx::FFbxImporter::UpdateSkeletalMeshImportData(USkeletalMesh *SkeletalMesh, UFbxSkeletalMeshImportData* SkeletalMeshImportData, int32 SpecificLod, TArray<FName> *ImportMaterialOriginalNameData, TArray<FImportMeshLodSectionsData> *ImportMeshLodData)
{
	if (SkeletalMesh == nullptr)
	{
		return;
	}

	UFbxSkeletalMeshImportData* ImportData = Cast<UFbxSkeletalMeshImportData>(SkeletalMesh->AssetImportData);
	if (!ImportData && SkeletalMeshImportData)
	{
		ImportData = UFbxSkeletalMeshImportData::GetImportDataForSkeletalMesh(SkeletalMesh, SkeletalMeshImportData);
	}
	if (ImportData)
	{
		ImportData->ImportMaterialOriginalNameData.Empty();
		if (ImportMaterialOriginalNameData && ImportMeshLodData)
		{
			if (SpecificLod == INDEX_NONE && ImportMeshLodData->Num() == SkeletalMesh->LODInfo.Num())
			{
				//Copy the material array
				ImportData->ImportMaterialOriginalNameData = (*ImportMaterialOriginalNameData);

				ImportData->ImportMeshLodData.Empty();
				for (const FImportMeshLodSectionsData &ImportMeshLodSectionsData : (*ImportMeshLodData))
				{
					ImportData->ImportMeshLodData.Add(ImportMeshLodSectionsData);
				}
			}
			else
			{
				for (FName MaterialImportNameLOD : (*ImportMaterialOriginalNameData))
				{
					bool bFoundMaterial = false;
					for (FName MaterialImportName : ImportData->ImportMaterialOriginalNameData)
					{
						if (MaterialImportNameLOD == MaterialImportName)
						{
							bFoundMaterial = true;
							break;
						}
					}
					if (!bFoundMaterial)
					{
						//Add the LOD material at the end of the original array
						ImportData->ImportMaterialOriginalNameData.Add(MaterialImportNameLOD);
					}
				}

				if (SpecificLod == INDEX_NONE)
				{
					for (int32 UpdateLodIndex = 0; UpdateLodIndex < (*ImportMeshLodData).Num(); ++UpdateLodIndex)
					{
						if (ImportData->ImportMeshLodData.Num() <= UpdateLodIndex)
						{
							ImportData->ImportMeshLodData.AddZeroed();
						}
						ImportData->ImportMeshLodData[UpdateLodIndex] = (*ImportMeshLodData)[UpdateLodIndex];
					}
				}
				else
				{
					if (ImportData->ImportMeshLodData.Num() <= SpecificLod)
					{
						ImportData->ImportMeshLodData.AddZeroed(1 + SpecificLod - ImportData->ImportMeshLodData.Num());
					}
					ImportData->ImportMeshLodData[SpecificLod] = (*ImportMeshLodData)[0];
				}
			}
		}
		else
		{
			//This is not a re-import or we reimport an old asset containing no data
			//In this case we update from the skeletal mesh import
			ImportData->ImportMaterialOriginalNameData.Empty();
			ImportData->ImportMeshLodData.Empty();
			for (const FSkeletalMaterial &Material : SkeletalMesh->Materials)
			{
				ImportData->ImportMaterialOriginalNameData.Add(Material.ImportedMaterialSlotName);
			}
			FSkeletalMeshResource* ImportedResource = SkeletalMesh->GetImportedResource();
			for (int32 LODResoureceIndex = 0; LODResoureceIndex < ImportedResource->LODModels.Num(); ++LODResoureceIndex)
			{
				ImportData->ImportMeshLodData.AddZeroed();
				const FSkeletalMeshLODInfo& LODInfo = SkeletalMesh->LODInfo[LODResoureceIndex];
				const FStaticLODModel& LODModel = ImportedResource->LODModels[LODResoureceIndex];
				int32 NumSections = LODModel.Sections.Num();
				for (int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
				{
					int32 MaterialLodSectionIndex = LODModel.Sections[SectionIndex].MaterialIndex;
					//Is this LOD use the LODMaterialMap override
					if (LODInfo.LODMaterialMap.Num() > SectionIndex)
					{
						MaterialLodSectionIndex = LODInfo.LODMaterialMap[SectionIndex];
					}

					if (ImportData->ImportMaterialOriginalNameData.IsValidIndex(MaterialLodSectionIndex))
					{
						ImportData->ImportMeshLodData[LODResoureceIndex].SectionOriginalMaterialName.Add(ImportData->ImportMaterialOriginalNameData[MaterialLodSectionIndex]);
					}
					else
					{
						ImportData->ImportMeshLodData[LODResoureceIndex].SectionOriginalMaterialName.Add(TEXT("InvalidMaterialIndex"));
					}
				}
			}
		}
	}
}


UObject* UnFbx::FFbxImporter::CreateAssetOfClass(UClass* AssetClass, FString ParentPackageName, FString ObjectName, bool bAllowReplace)
{
	// See if this sequence already exists.
	UObject* 	ParentPkg = CreatePackage(NULL, *ParentPackageName);
	FString 	ParentPath = FString::Printf(TEXT("%s/%s"), *FPackageName::GetLongPackagePath(*ParentPackageName), *ObjectName);
	UObject* 	Parent = CreatePackage(NULL, *ParentPath);
	// See if an object with this name exists
	UObject* Object = LoadObject<UObject>(Parent, *ObjectName, NULL, LOAD_NoWarn | LOAD_Quiet, NULL);

	// if object with same name but different class exists, warn user
	if ((Object != NULL) && (Object->GetClass() != AssetClass))
	{
		UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
		FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("Error_AssetExist", "Asset with same name exists. Can't overwrite another asset")), FFbxErrors::Generic_SameNameAssetExists);
		return NULL;
	}

	// if object with same name exists, warn user
	if ( Object != NULL && !bAllowReplace )
	{
		// until we have proper error message handling so we don't ask every time, but once, I'm disabling it. 
		// 		if ( EAppReturnType::Yes != FMessageDialog::Open( EAppMsgType::YesNo, LocalizeSecure(NSLOCTEXT("UnrealEd", "Error_AssetExistAsk", "Asset with the name (`~) exists. Would you like to overwrite?").ToString(), *ParentPath) ) ) 
		// 		{
		// 			return NULL;
		// 		}
		UnFbx::FFbxImporter* FFbxImporter = UnFbx::FFbxImporter::GetInstance();
		FFbxImporter->AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_SameNameExist", "Asset with the name ('{0}') exists. Overwriting..."), FText::FromString(ParentPath))), FFbxErrors::Generic_SameNameAssetOverriding);
	}

	if (Object == NULL)
	{
		// add it to the set
		// do not add to the set, now create independent asset
		Object = NewObject<UObject>(Parent, AssetClass, *ObjectName, RF_Public | RF_Standalone);
		Object->MarkPackageDirty();
		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(Object);
	}

	return Object;
}

void UnFbx::FFbxImporter::SetupAnimationDataFromMesh(USkeletalMesh* SkeletalMesh, UObject* InParent, TArray<FbxNode*>& NodeArray, UFbxAnimSequenceImportData* TemplateImportData, const FString& Name)
{
	USkeleton* Skeleton = SkeletalMesh->Skeleton;

	if (Scene->GetSrcObjectCount<FbxAnimStack>() > 0)
	{
		if ( ensure ( Skeleton != NULL ) )
		{
			TArray<FbxNode*> FBXMeshNodeArray;
			FbxNode* SkeletonRoot = FindFBXMeshesByBone(SkeletalMesh->Skeleton->GetReferenceSkeleton().GetBoneName(0), true, FBXMeshNodeArray);

			if (SkeletonRoot != nullptr)
			{
				TArray<FbxNode*> SortedLinks;
				RecursiveBuildSkeleton(SkeletonRoot, SortedLinks);

				// when importing animation from SkeletalMesh, add new Group Anim, a lot of times they're same name
				UPackage * OuterPackage = InParent->GetOutermost();
				FString AnimName = (ImportOptions->AnimationName != "") ? ImportOptions->AnimationName : Name + TEXT("_Anim");
				// give animouter as outer
				ImportAnimations(Skeleton, OuterPackage, SortedLinks, AnimName, TemplateImportData, NodeArray);
			}
			else
			{
				//Cannot import animations if the skeleton do not match
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_SkeletonNotMatching_no_anim_import", "Specified Skeleton '{0}' do not match fbx imported skeleton. Cannot import animations with this skeleton."), FText::FromName(Skeleton->GetFName()) )), FFbxErrors::Animation_InvalidData);
			}
		}
	}
}

USkeletalMesh* UnFbx::FFbxImporter::ReimportSkeletalMesh(USkeletalMesh* Mesh, UFbxSkeletalMeshImportData* TemplateImportData, uint64 SkeletalMeshFbxUID, TArray<FbxNode*> *OutSkeletalMeshArray)
{
	if ( !ensure(Mesh) )
	{
		// You need a mesh in order to reimport
		return NULL;
	}

	if ( !ensure(TemplateImportData) )
	{
		// You need import data in order to reimport
		return NULL;
	}

	char MeshName[1024];
	FCStringAnsi::Strcpy(MeshName, TCHAR_TO_UTF8(*Mesh->GetName()));
	TArray<FbxNode*>* FbxNodes = NULL;
	USkeletalMesh* NewMesh = NULL;

	bool Old_ImportRigidMesh = ImportOptions->bImportRigidMesh;
	bool Old_ImportMaterials = ImportOptions->bImportMaterials;
	bool Old_ImportTextures = ImportOptions->bImportTextures;
	bool Old_ImportAnimations = ImportOptions->bImportAnimations;

	// support to update rigid animation mesh
	ImportOptions->bImportRigidMesh = true;

	// get meshes in Fbx file
	//the function also fill the collision models, so we can update collision models correctly
	TArray< TArray<FbxNode*>* > FbxSkelMeshArray;
	FillFbxSkelMeshArrayInScene(Scene->GetRootNode(), FbxSkelMeshArray, false, ImportOptions->bImportScene);

	if(SkeletalMeshFbxUID != 0xFFFFFFFFFFFFFFFF)
	{
		//Scene reimport know which skeletal mesh we want to reimport
		for (TArray<FbxNode*>*SkeletalMeshNodes : FbxSkelMeshArray)
		{
			if (SkeletalMeshNodes->Num() > 0)
			{
				FbxNode *Node = (*SkeletalMeshNodes)[0];
				FbxNode *SkeletalMeshNode = Node;
				if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
				{
					TArray<FbxNode*> NodeInLod;
					FindAllLODGroupNode(NodeInLod, Node, 0);
					for (FbxNode *MeshNode : NodeInLod)
					{
						if (MeshNode != nullptr && MeshNode->GetNodeAttribute() && MeshNode->GetNodeAttribute()->GetUniqueID() == SkeletalMeshFbxUID)
						{
							FbxNodes = SkeletalMeshNodes;
							if (OutSkeletalMeshArray != nullptr)
							{
								for (FbxNode *NodeReimport : (*SkeletalMeshNodes))
								{
									OutSkeletalMeshArray->Add(NodeReimport);
								}
							}
							break;
						}
					}
				}
				else
				{
					if (SkeletalMeshNode != nullptr && SkeletalMeshNode->GetNodeAttribute() && SkeletalMeshNode->GetNodeAttribute()->GetUniqueID() == SkeletalMeshFbxUID)
					{
						FbxNodes = SkeletalMeshNodes;
						if (OutSkeletalMeshArray != nullptr)
						{
							for (FbxNode *NodeReimport : (*SkeletalMeshNodes))
							{
								OutSkeletalMeshArray->Add(NodeReimport);
							}
						}
						break;
					}
				}
			}
			if (FbxNodes != nullptr)
				break;
		}
		if (FbxNodes == nullptr)
			return nullptr;
	}
	else
	{
		// if there is only one mesh, use it without name checking 
		// (because the "Used As Full Name" option enables users name the Unreal mesh by themselves
		if (FbxSkelMeshArray.Num() > 0)
		{
			FbxNodes = FbxSkelMeshArray[0];
		}
		else
		{
			// @todo - FBX Importing - We need proper support for reimport if the file contains more than one skeletal mesh. 

		}
	}

	if (FbxNodes)
	{
		// set import options, how about others?
		if (!ImportOptions->bImportScene)
		{
			ImportOptions->bImportMaterials = false;
			ImportOptions->bImportTextures = false;
		}
		//In case of a scene reimport animations are reimport later so its ok to hardcode animation to false here
		ImportOptions->bImportAnimations = false;
		// check if there is LODGroup for this skeletal mesh
		int32 MaxLODLevel = 1;
		int32 NumPrevLODs = Mesh->LODInfo.Num();

		for (int32 j = 0; j < (*FbxNodes).Num(); j++)
		{
			FbxNode* Node = (*FbxNodes)[j];
			if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
			{
				// get max LODgroup level
				if (MaxLODLevel < Node->GetChildCount())
				{
					MaxLODLevel = Node->GetChildCount();
				}
			}
		}

		//Original fbx data storage
		TArray<FName> ImportMaterialOriginalNameData;
		TArray<FImportMeshLodSectionsData> ImportMeshLodData;

		int32 LODIndex;
		for (LODIndex = 0; LODIndex < MaxLODLevel; LODIndex++)
		{
			TArray<FbxNode*> SkelMeshNodeArray;
			for (int32 j = 0; j < (*FbxNodes).Num(); j++)
			{
				FbxNode* Node = (*FbxNodes)[j];
				if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGroup)
				{
					TArray<FbxNode*> NodeInLod;
					if (Node->GetChildCount() > LODIndex)
					{
						FindAllLODGroupNode(NodeInLod, Node, LODIndex);
					}
					else // in less some LODGroups have less level, use the last level
					{
						FindAllLODGroupNode(NodeInLod, Node, Node->GetChildCount() - 1);
					}
					for (FbxNode *MeshNode : NodeInLod)
					{
						SkelMeshNodeArray.Add(MeshNode);
					}
				}
				else
				{
					SkelMeshNodeArray.Add(Node);
				}
			}
			FSkeletalMeshImportData OutData;
			if (LODIndex == 0)
			{
				ImportMeshLodData.AddZeroed();
				UnFbx::FFbxImporter::FImportSkeletalMeshArgs ImportSkeletalMeshArgs;
				ImportSkeletalMeshArgs.InParent = Mesh->GetOuter();
				ImportSkeletalMeshArgs.NodeArray = SkelMeshNodeArray;
				ImportSkeletalMeshArgs.Name = *Mesh->GetName();
				ImportSkeletalMeshArgs.Flags = RF_Public | RF_Standalone;
				ImportSkeletalMeshArgs.TemplateImportData = TemplateImportData;
				ImportSkeletalMeshArgs.LodIndex = LODIndex;
				ImportSkeletalMeshArgs.ImportMaterialOriginalNameData = &ImportMaterialOriginalNameData;
				ImportSkeletalMeshArgs.ImportMeshSectionsData = &ImportMeshLodData[0];
				ImportSkeletalMeshArgs.OutData = &OutData;

				NewMesh = ImportSkeletalMesh( ImportSkeletalMeshArgs );
			}
			else if (NewMesh && ImportOptions->bImportSkeletalMeshLODs) // the base skeletal mesh is imported successfully
			{
				TArray<FName> ImportMaterialOriginalNameDataLOD;
				ImportMeshLodData.AddZeroed();
				USkeletalMesh* BaseSkeletalMesh = NewMesh;
				
				UnFbx::FFbxImporter::FImportSkeletalMeshArgs ImportSkeletalMeshArgs;
				ImportSkeletalMeshArgs.InParent = NewMesh->GetOutermost();
				ImportSkeletalMeshArgs.NodeArray = SkelMeshNodeArray;
				ImportSkeletalMeshArgs.Name = NAME_None;
				ImportSkeletalMeshArgs.Flags = RF_Transient;
				ImportSkeletalMeshArgs.TemplateImportData = TemplateImportData;
				ImportSkeletalMeshArgs.LodIndex = LODIndex;
				ImportSkeletalMeshArgs.ImportMaterialOriginalNameData = &ImportMaterialOriginalNameDataLOD;
				ImportSkeletalMeshArgs.ImportMeshSectionsData = &ImportMeshLodData[LODIndex];
				ImportSkeletalMeshArgs.OutData = &OutData;

				UObject *LODObject = ImportSkeletalMesh( ImportSkeletalMeshArgs );
				ImportSkeletalMeshLOD( Cast<USkeletalMesh>(LODObject), BaseSkeletalMesh, LODIndex, true, nullptr, TemplateImportData);

				for (FName MaterialImportNameLOD : ImportMaterialOriginalNameDataLOD)
				{
					bool bFoundMaterial = false;
					for (FName MaterialImportName : ImportMaterialOriginalNameData)
					{
						if (MaterialImportNameLOD == MaterialImportName)
						{
							bFoundMaterial = true;
							break;
						}
					}
					if (!bFoundMaterial)
					{
						//Add the LOD material at the end of the original array
						ImportMaterialOriginalNameData.Add(MaterialImportNameLOD);
					}
				}

				// Set LOD Model's DisplayFactor
				// if this LOD is newly added, then set DisplayFactor
				// Don't override DispalyFactor of existing LODs
				if(LODIndex >= NumPrevLODs)
				{
					BaseSkeletalMesh->LODInfo[LODIndex].ScreenSize = 1.0f / (MaxLODLevel * LODIndex);
				}
			}

			// import morph target
			if ((ImportOptions->bImportSkeletalMeshLODs || LODIndex == 0) &&
				NewMesh &&
				ImportOptions->bImportMorph &&
				NewMesh->GetImportedResource() &&
				NewMesh->GetImportedResource()->LODModels.IsValidIndex(LODIndex))
			{
				// @fixme: @question : where do they import this morph? where to? What morph target sets?
				ImportFbxMorphTarget(SkelMeshNodeArray, NewMesh, NewMesh->GetOutermost(), LODIndex, OutData);
			}
		}
		if (NewMesh)
		{
			//Update the import data so we can re-import correctly
			UpdateSkeletalMeshImportData(NewMesh, TemplateImportData, INDEX_NONE, &ImportMaterialOriginalNameData, &ImportMeshLodData);
			//If we have import some morph target we have to rebuild the render resources since morph target are now using GPU
			if (NewMesh->MorphTargets.Num() > 0)
			{
				NewMesh->ReleaseResources();
				//Rebuild the resources with a post edit change since we have added some morph targets
				NewMesh->PostEditChange();
			}
		}
	}
	else
	{
		// no mesh found in the FBX file
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_NoFBXMeshMatch", "No FBX mesh matches the Unreal mesh '{0}'."), FText::FromString(Mesh->GetName()))), FFbxErrors::Generic_Mesh_MeshNotFound);
	}

	ImportOptions->bImportRigidMesh = Old_ImportRigidMesh;
	ImportOptions->bImportMaterials = Old_ImportMaterials;
	ImportOptions->bImportTextures = Old_ImportTextures;
	ImportOptions->bImportAnimations = Old_ImportAnimations;

	return NewMesh;
}

void UnFbx::FFbxImporter::SetMaterialSkinXXOrder(FSkeletalMeshImportData& ImportData)
{
	TArray<int32> MaterialIndexToSkinIndex;
	TMap<int32, int32> SkinIndexToMaterialIndex;
	TArray<int32> MissingSkinSuffixMaterial;
	TMap<int32, int32> SkinIndexGreaterThenMaterialArraySize;
	{
		int32 MaterialCount = ImportData.Materials.Num();

		bool bNeedsReorder = false;
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			// get skin index
			FString MatName = ImportData.Materials[MaterialIndex].MaterialImportName;

			if (MatName.Len() > 6)
			{
				int32 Offset = MatName.Find(TEXT("_SKIN"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
				if (Offset != INDEX_NONE)
				{
					// Chop off the material name so we are left with the number in _SKINXX
					FString SkinXXNumber = MatName.Right(MatName.Len() - (Offset + 1)).RightChop(4);

					if (SkinXXNumber.IsNumeric())
					{
						bNeedsReorder = true;
						int32 TmpIndex = FPlatformString::Atoi(*SkinXXNumber);
						if (TmpIndex < MaterialCount)
						{
							SkinIndexToMaterialIndex.Add(TmpIndex, MaterialIndex);
						}
						else
						{
							SkinIndexGreaterThenMaterialArraySize.Add(TmpIndex, MaterialIndex);
						}
						
					}
				}
				else
				{
					MissingSkinSuffixMaterial.Add(MaterialIndex);
				}
			}
			else
			{
				MissingSkinSuffixMaterial.Add(MaterialIndex);
			}
		}

		if (bNeedsReorder && MissingSkinSuffixMaterial.Num() > 0)
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FbxSkeletaLMeshimport_Skinxx_missing", "Cannot mix skinxx suffix materials with no skinxx material, mesh section order will not be right.")), FFbxErrors::Generic_Mesh_SkinxxNameError);
			return;
		}

		//Add greater then material array skinxx at the end sorted by integer the index will be remap correctly in the case of a LOD import
		if (SkinIndexGreaterThenMaterialArraySize.Num() > 0)
		{
			int32 MaxAvailableKey = SkinIndexToMaterialIndex.Num();
			for (int32 AvailableKey = 0; AvailableKey < MaxAvailableKey; ++AvailableKey)
			{
				if (SkinIndexToMaterialIndex.Contains(AvailableKey))
					continue;

				TMap<int32, int32> TempSkinIndexToMaterialIndex;
				for (auto KvpSkinToMat : SkinIndexToMaterialIndex)
				{
					if (KvpSkinToMat.Key > AvailableKey)
					{
						TempSkinIndexToMaterialIndex.Add(KvpSkinToMat.Key - 1, KvpSkinToMat.Value);
					}
					else
					{
						TempSkinIndexToMaterialIndex.Add(KvpSkinToMat.Key, KvpSkinToMat.Value);
					}
				}
				//move all the later key of the array to fill the available index
				SkinIndexToMaterialIndex = TempSkinIndexToMaterialIndex;
				AvailableKey--; //We need to retest the same index it can be empty
			}
			//Reorder the array
			SkinIndexGreaterThenMaterialArraySize.KeySort(TLess<int32>());
			for (auto Kvp : SkinIndexGreaterThenMaterialArraySize)
			{
				SkinIndexToMaterialIndex.Add(SkinIndexToMaterialIndex.Num(), Kvp.Value);
			}
		}

		//Fill the array MaterialIndexToSkinIndex so we order material by _skinXX order
		//This ensure we support skinxx suffixe that are not increment by one like _skin00, skin_01, skin_03, skin_04, skin_08... 
		for (auto kvp : SkinIndexToMaterialIndex)
		{
			int32 MatIndexToInsert = 0;
			for (MatIndexToInsert = 0; MatIndexToInsert < MaterialIndexToSkinIndex.Num(); ++MatIndexToInsert)
			{
				if (*(SkinIndexToMaterialIndex.Find(MaterialIndexToSkinIndex[MatIndexToInsert])) >= kvp.Value)
				{
					break;
				}
			}
			MaterialIndexToSkinIndex.Insert(kvp.Key, MatIndexToInsert);
		}

		if (bNeedsReorder)
		{
			// re-order the materials
			TArray< VMaterial > ExistingMatList = ImportData.Materials;
			for (int32 MissingIndex : MissingSkinSuffixMaterial)
			{
				MaterialIndexToSkinIndex.Insert(MaterialIndexToSkinIndex.Num(), MissingIndex);
			}
			for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
			{
				if (MaterialIndex < MaterialIndexToSkinIndex.Num())
				{
					int32 NewIndex = MaterialIndexToSkinIndex[MaterialIndex];
					if (ExistingMatList.IsValidIndex(NewIndex))
					{
						ImportData.Materials[NewIndex] = ExistingMatList[MaterialIndex];
					}
				}
			}

			// remapping the material index for each triangle
			int32 FaceNum = ImportData.Faces.Num();
			for (int32 TriangleIndex = 0; TriangleIndex < FaceNum; TriangleIndex++)
			{
				VTriangle& Triangle = ImportData.Faces[TriangleIndex];
				if (Triangle.MatIndex < MaterialIndexToSkinIndex.Num())
				{
					Triangle.MatIndex = MaterialIndexToSkinIndex[Triangle.MatIndex];
				}
			}
		}
	}
}

void UnFbx::FFbxImporter::SetMaterialOrderByName(FSkeletalMeshImportData& ImportData, TArray<FName> LastImportedMaterialNames)
{
	TArray<int32> MaterialIndexToNameIndex;
	TMap<int32, int32> NameIndexToMaterialIndex;
	TArray<int32> MissingNameSuffixMaterial;
	TMap<int32, int32> NameIndexGreaterThenMaterialArraySize;
	{
		int32 MaterialCount = ImportData.Materials.Num();
		int32 MaxMaterialOrderedCount = 0;
		bool bNeedsReorder = false;
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			FName MatName = FName(*(ImportData.Materials[MaterialIndex].MaterialImportName));
			bool bFoundValidName = false;
			for (int32 OrderedIndex = 0; OrderedIndex < LastImportedMaterialNames.Num(); ++OrderedIndex)
			{
				FName OrderedMaterialName = LastImportedMaterialNames[OrderedIndex];
				if (OrderedMaterialName == NAME_None)
				{
					continue;
				}
				if (OrderedMaterialName == MatName)
				{
					if (OrderedIndex < MaterialCount)
					{
						MaxMaterialOrderedCount = FMath::Max(MaxMaterialOrderedCount, OrderedIndex+1);
						NameIndexToMaterialIndex.Add(OrderedIndex, MaterialIndex);
					}
					else
					{
						NameIndexGreaterThenMaterialArraySize.Add(OrderedIndex, MaterialIndex);
					}
					bFoundValidName = true;
					bNeedsReorder = true;
					break;
				}
			}
			if (!bFoundValidName)
			{
				MissingNameSuffixMaterial.Add(MaterialIndex);
				MaxMaterialOrderedCount = FMath::Max(MaxMaterialOrderedCount, MaterialIndex+1);
			}
		}

		if (bNeedsReorder && MissingNameSuffixMaterial.Num() > 0)
		{
			//Add the missing name material at the end to not disturb the existing order
			TArray<int32> OrderedListMissing;
			OrderedListMissing.AddZeroed(MaxMaterialOrderedCount);
			for (auto Kvp : NameIndexToMaterialIndex)
			{
				OrderedListMissing[Kvp.Key] = -1;
			}
			for (int32 OrderedListMissingIndex = 0; OrderedListMissingIndex < OrderedListMissing.Num(); ++OrderedListMissingIndex)
			{
				if (MissingNameSuffixMaterial.Num() <= 0)
				{
					break;
				}

				if (OrderedListMissing[OrderedListMissingIndex] != 0)
					continue;

				NameIndexToMaterialIndex.Add(OrderedListMissingIndex, MissingNameSuffixMaterial.Pop());
			}
		}

		//Add greater then material array slot index at the end sorted by integer the index will be remap correctly in the case of a LOD import
		if (NameIndexGreaterThenMaterialArraySize.Num() > 0)
		{
			int32 MaxAvailableKey = NameIndexToMaterialIndex.Num();
			for (int32 AvailableKey = 0; AvailableKey < MaxAvailableKey; ++AvailableKey)
			{
				if (NameIndexToMaterialIndex.Contains(AvailableKey))
					continue;

				TMap<int32, int32> TempSkinIndexToMaterialIndex;
				for (auto KvpSkinToMat : NameIndexToMaterialIndex)
				{
					if (KvpSkinToMat.Key > AvailableKey)
					{
						TempSkinIndexToMaterialIndex.Add(KvpSkinToMat.Key - 1, KvpSkinToMat.Value);
					}
					else
					{
						TempSkinIndexToMaterialIndex.Add(KvpSkinToMat.Key, KvpSkinToMat.Value);
					}
				}
				//move all the later key of the array to fill the available index
				NameIndexToMaterialIndex = TempSkinIndexToMaterialIndex;
				AvailableKey--; //We need to retest the same index it can be empty
			}
			//Reorder the array
			NameIndexGreaterThenMaterialArraySize.KeySort(TLess<int32>());
			for (auto Kvp : NameIndexGreaterThenMaterialArraySize)
			{
				NameIndexToMaterialIndex.Add(NameIndexToMaterialIndex.Num(), Kvp.Value);
			}
		}

		//Fill the array MaterialIndexToNameIndex so we order material by ordered index
		for (auto kvp : NameIndexToMaterialIndex)
		{
			int32 MatIndexToInsert = 0;
			for (MatIndexToInsert = 0; MatIndexToInsert < MaterialIndexToNameIndex.Num(); ++MatIndexToInsert)
			{
				if (*(NameIndexToMaterialIndex.Find(MaterialIndexToNameIndex[MatIndexToInsert])) >= kvp.Value)
				{
					break;
				}
			}
			MaterialIndexToNameIndex.Insert(kvp.Key, MatIndexToInsert);
		}

		if (bNeedsReorder)
		{
			// re-order the materials
			TArray< VMaterial > ExistingMatList = ImportData.Materials;
			
			for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
			{
				if (MaterialIndex < MaterialIndexToNameIndex.Num())
				{
					int32 NewIndex = MaterialIndexToNameIndex[MaterialIndex];
					if (ExistingMatList.IsValidIndex(NewIndex))
					{
						ImportData.Materials[NewIndex] = ExistingMatList[MaterialIndex];
					}
				}
			}

			// remapping the material index for each triangle
			int32 FaceNum = ImportData.Faces.Num();
			for (int32 TriangleIndex = 0; TriangleIndex < FaceNum; TriangleIndex++)
			{
				VTriangle& Triangle = ImportData.Faces[TriangleIndex];
				if (Triangle.MatIndex < MaterialIndexToNameIndex.Num())
				{
					Triangle.MatIndex = MaterialIndexToNameIndex[Triangle.MatIndex];
				}
			}
		}
	}
}

void UnFbx::FFbxImporter::CleanUpUnusedMaterials(FSkeletalMeshImportData& ImportData)
{
	if (ImportData.Materials.Num() <= 0)
	{
		return;
	}

	TArray< VMaterial > ExistingMatList = ImportData.Materials;

	TArray<uint8> UsedMaterialIndex;
	// Find all material that are use by the mesh faces
	int32 FaceNum = ImportData.Faces.Num();
	for (int32 TriangleIndex = 0; TriangleIndex < FaceNum; TriangleIndex++)
	{
		VTriangle& Triangle = ImportData.Faces[TriangleIndex];
		UsedMaterialIndex.AddUnique(Triangle.MatIndex);
	}
	//Remove any unused material.
	if (UsedMaterialIndex.Num() < ExistingMatList.Num())
	{
		TArray<int32> RemapIndex;
		TArray< VMaterial > &NewMatList = ImportData.Materials;
		NewMatList.Empty();
		for (int32 ExistingMatIndex = 0; ExistingMatIndex < ExistingMatList.Num(); ++ExistingMatIndex)
		{
			if (UsedMaterialIndex.Contains((uint8)ExistingMatIndex))
			{
				RemapIndex.Add(NewMatList.Add(ExistingMatList[ExistingMatIndex]));
			}
			else
			{
				RemapIndex.Add(INDEX_NONE);
			}
		}
		ImportData.MaxMaterialIndex = 0;
		//Remap the face material index
		for (int32 TriangleIndex = 0; TriangleIndex < FaceNum; TriangleIndex++)
		{
			VTriangle& Triangle = ImportData.Faces[TriangleIndex];
			check(RemapIndex[Triangle.MatIndex] != INDEX_NONE);
			Triangle.MatIndex = RemapIndex[Triangle.MatIndex];
			ImportData.MaxMaterialIndex = FMath::Max<uint32>(ImportData.MaxMaterialIndex, Triangle.MatIndex);
		}
	}
}

bool UnFbx::FFbxImporter::FillSkelMeshImporterFromFbx( FSkeletalMeshImportData& ImportData, FbxMesh*& Mesh, FbxSkin* Skin, FbxShape* FbxShape, TArray<FbxNode*> &SortedLinks, const TArray<FbxSurfaceMaterial*>& FbxMaterials, FbxNode *RootNode)
{
	FbxNode* Node = Mesh->GetNode();

	//remove the bad polygons before getting any data from mesh
	Mesh->RemoveBadPolygons();

	//Get the base layer of the mesh
	FbxLayer* BaseLayer = Mesh->GetLayer(0);
	if (BaseLayer == NULL)
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, LOCTEXT("FbxSkeletaLMeshimport_NoGeometry", "There is no geometry information in mesh")), FFbxErrors::Generic_Mesh_NoGeometry);
		return false;
	}

	// Do some checks before proceeding, check to make sure the number of bones does not exceed the maximum supported
	if(SortedLinks.Num() > MAX_BONES)
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_ExceedsMaxBoneCount", "'{0}' mesh has '{1}' bones which exceeds the maximum allowed bone count of {2}."), FText::FromString(Node->GetName()), FText::AsNumber(SortedLinks.Num()), FText::AsNumber(MAX_BONES))), FFbxErrors::SkeletalMesh_ExceedsMaxBoneCount);
		return false;
	}

	//
	//	store the UVs in arrays for fast access in the later looping of triangles 
	//
	// mapping from UVSets to Fbx LayerElementUV
	// Fbx UVSets may be duplicated, remove the duplicated UVSets in the mapping 
	int32 LayerCount = Mesh->GetLayerCount();
	TArray<FString> UVSets;

	UVSets.Empty();
	if (LayerCount > 0)
	{
		int32 UVLayerIndex;
		for (UVLayerIndex = 0; UVLayerIndex<LayerCount; UVLayerIndex++)
		{
			FbxLayer* lLayer = Mesh->GetLayer(UVLayerIndex);
			int32 UVSetCount = lLayer->GetUVSetCount();
			if(UVSetCount)
			{
				FbxArray<FbxLayerElementUV const*> EleUVs = lLayer->GetUVSets();
				for (int32 UVIndex = 0; UVIndex<UVSetCount; UVIndex++)
				{
					FbxLayerElementUV const* ElementUV = EleUVs[UVIndex];
					if (ElementUV)
					{
						const char* UVSetName = ElementUV->GetName();
						FString LocalUVSetName = UTF8_TO_TCHAR(UVSetName);
						if (LocalUVSetName.IsEmpty())
						{
							LocalUVSetName = TEXT("UVmap_") + FString::FromInt(UVLayerIndex);
						}
						UVSets.AddUnique(LocalUVSetName);
					}
				}
			}
		}
	}

	// If the the UV sets are named using the following format (UVChannel_X; where X ranges from 1 to 4)
	// we will re-order them based on these names.  Any UV sets that do not follow this naming convention
	// will be slotted into available spaces.
	if( UVSets.Num() > 0 )
	{
		for(int32 ChannelNumIdx = 0; ChannelNumIdx < 4; ChannelNumIdx++)
		{
			FString ChannelName = FString::Printf( TEXT("UVChannel_%d"), ChannelNumIdx+1 );
			int32 SetIdx = UVSets.Find( ChannelName );

			// If the specially formatted UVSet name appears in the list and it is in the wrong spot,
			// we will swap it into the correct spot.
			if( SetIdx != INDEX_NONE && SetIdx != ChannelNumIdx )
			{
				// If we are going to swap to a position that is outside the bounds of the
				// array, then we pad out to that spot with empty data.
				for(int32 ArrSize = UVSets.Num(); ArrSize < ChannelNumIdx+1; ArrSize++)
				{
					UVSets.Add( FString(TEXT("")) );
				}
				//Swap the entry into the appropriate spot.
				UVSets.Swap( SetIdx, ChannelNumIdx );
			}
		}
	}


	TArray<UMaterialInterface*> Materials;
	if (ImportOptions->bImportMaterials)
	{
		bool bForSkeletalMesh = true;
		CreateNodeMaterials(Node, Materials, UVSets, bForSkeletalMesh);
	}
	else if (ImportOptions->bImportTextures)
	{
		ImportTexturesFromNode(Node);
	}

	// Maps local mesh material index to global material index
	TArray<int32> MaterialMapping;

	int32 MaterialCount = Node->GetMaterialCount();

	MaterialMapping.AddUninitialized(MaterialCount);

	for( int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex )
	{
		FbxSurfaceMaterial* FbxMaterial = Node->GetMaterial( MaterialIndex );

		int32 ExistingMatIndex = INDEX_NONE;
		FbxMaterials.Find( FbxMaterial, ExistingMatIndex ); 
		if( ExistingMatIndex != INDEX_NONE )
		{
			// Reuse existing material
			MaterialMapping[MaterialIndex] = ExistingMatIndex;

			if (Materials.IsValidIndex(MaterialIndex) )
			{
				ImportData.Materials[ExistingMatIndex].Material = Materials[MaterialIndex];
			}
		}
		else
		{
			MaterialMapping[MaterialIndex] = 0;
		}
	}

	if( LayerCount > 0 && ImportOptions->bPreserveSmoothingGroups )
	{
		// Check and see if the smooothing data is valid.  If not generate it from the normals
		BaseLayer = Mesh->GetLayer(0);
		if( BaseLayer )
		{
			const FbxLayerElementSmoothing* SmoothingLayer = BaseLayer->GetSmoothing();

			if( SmoothingLayer )
			{
				bool bValidSmoothingData = false;
				FbxLayerElementArrayTemplate<int32>& Array = SmoothingLayer->GetDirectArray();
				for( int32 SmoothingIndex = 0; SmoothingIndex < Array.GetCount(); ++SmoothingIndex )
				{
					if( Array[SmoothingIndex] != 0 )
					{
						bValidSmoothingData = true;
						break;
					}
				}

				if( !bValidSmoothingData && Mesh->GetPolygonVertexCount() > 0 )
				{
					GeometryConverter->ComputeEdgeSmoothingFromNormals(Mesh);
				}
			}
		}
	}

	// Must do this before triangulating the mesh due to an FBX bug in TriangulateMeshAdvance
	int32 LayerSmoothingCount = Mesh->GetLayerCount(FbxLayerElement::eSmoothing);
	for(int32 i = 0; i < LayerSmoothingCount; i++)
	{
		GeometryConverter->ComputePolygonSmoothingFromEdgeSmoothing (Mesh, i);
	}

	//
	// Convert data format to unreal-compatible
	//

	if (!Mesh->IsTriangleMesh())
	{
		UE_LOG(LogFbx, Log, TEXT("Triangulating skeletal mesh %s"), UTF8_TO_TCHAR(Node->GetName()));
		
		const bool bReplace = true;
		FbxNodeAttribute* ConvertedNode = GeometryConverter->Triangulate(Mesh, bReplace);
		if( ConvertedNode != NULL && ConvertedNode->GetAttributeType() == FbxNodeAttribute::eMesh )
		{
			Mesh = ConvertedNode->GetNode()->GetMesh();
		}
		else
		{
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_TriangulatingFailed", "Unable to triangulate mesh '{0}'. Check detail for Ouput Log."), FText::FromString(Node->GetName()))), FFbxErrors::Generic_Mesh_TriangulationFailed);
			return false;
		}
	}
	
	// renew the base layer
	BaseLayer = Mesh->GetLayer(0);
	Skin = (FbxSkin*)static_cast<FbxGeometry*>(Mesh)->GetDeformer(0, FbxDeformer::eSkin);

	//
	//	store the UVs in arrays for fast access in the later looping of triangles 
	//
	uint32 UniqueUVCount = UVSets.Num();
	FbxLayerElementUV** LayerElementUV = NULL;
	FbxLayerElement::EReferenceMode* UVReferenceMode = NULL;
	FbxLayerElement::EMappingMode* UVMappingMode = NULL;
	if (UniqueUVCount > 0)
	{
		LayerElementUV = new FbxLayerElementUV*[UniqueUVCount];
		UVReferenceMode = new FbxLayerElement::EReferenceMode[UniqueUVCount];
		UVMappingMode = new FbxLayerElement::EMappingMode[UniqueUVCount];
	}
	else
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_NoUVSet", "Mesh '{0}' has no UV set. Creating a default set."), FText::FromString(Node->GetName()))), FFbxErrors::SkeletalMesh_NoUVSet);
	}
	
	LayerCount = Mesh->GetLayerCount();
	for (uint32 UVIndex = 0; UVIndex < UniqueUVCount; UVIndex++)
	{
		LayerElementUV[UVIndex] = NULL;
		for (int32 UVLayerIndex = 0; UVLayerIndex<LayerCount; UVLayerIndex++)
		{
			FbxLayer* lLayer = Mesh->GetLayer(UVLayerIndex);
			int32 UVSetCount = lLayer->GetUVSetCount();
			if(UVSetCount)
			{
				FbxArray<FbxLayerElementUV const*> EleUVs = lLayer->GetUVSets();
				for (int32 FbxUVIndex = 0; FbxUVIndex<UVSetCount; FbxUVIndex++)
				{
					FbxLayerElementUV const* ElementUV = EleUVs[FbxUVIndex];
					if (ElementUV)
					{
						const char* UVSetName = ElementUV->GetName();
						FString LocalUVSetName = UTF8_TO_TCHAR(UVSetName);
						if (LocalUVSetName.IsEmpty())
						{
							LocalUVSetName = TEXT("UVmap_") + FString::FromInt(UVLayerIndex);
						}
						if (LocalUVSetName == UVSets[UVIndex])
						{
							LayerElementUV[UVIndex] = const_cast<FbxLayerElementUV*>(ElementUV);
							UVReferenceMode[UVIndex] = LayerElementUV[UVIndex]->GetReferenceMode();
							UVMappingMode[UVIndex] = LayerElementUV[UVIndex]->GetMappingMode();
							break;
						}
					}
				}
			}
		}
	}

	//
	// get the smoothing group layer
	//
	bool bSmoothingAvailable = false;

	FbxLayerElementSmoothing const* SmoothingInfo = BaseLayer->GetSmoothing();
	FbxLayerElement::EReferenceMode SmoothingReferenceMode(FbxLayerElement::eDirect);
	FbxLayerElement::EMappingMode SmoothingMappingMode(FbxLayerElement::eByEdge);
	if (SmoothingInfo)
	{
 		if( SmoothingInfo->GetMappingMode() == FbxLayerElement::eByEdge )
		{
			if (!GeometryConverter->ComputePolygonSmoothingFromEdgeSmoothing(Mesh))
			{
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_ConvertSmoothingGroupFailed", "Unable to fully convert the smoothing groups for mesh '{0}'"), FText::FromString(Mesh->GetName()))), FFbxErrors::Generic_Mesh_ConvertSmoothingGroupFailed);
				bSmoothingAvailable = false;
			}
		}

		if( SmoothingInfo->GetMappingMode() == FbxLayerElement::eByPolygon )
		{
			bSmoothingAvailable = true;
		}


		SmoothingReferenceMode = SmoothingInfo->GetReferenceMode();
		SmoothingMappingMode = SmoothingInfo->GetMappingMode();
	}


	//
	//	get the "material index" layer
	//
	FbxLayerElementMaterial* LayerElementMaterial = BaseLayer->GetMaterials();
	FbxLayerElement::EMappingMode MaterialMappingMode = LayerElementMaterial ? 
		LayerElementMaterial->GetMappingMode() : FbxLayerElement::eByPolygon;

	UniqueUVCount = FMath::Min<uint32>( UniqueUVCount, MAX_TEXCOORDS );

	// One UV set is required but only import up to MAX_TEXCOORDS number of uv layers
	ImportData.NumTexCoords = FMath::Max<uint32>( ImportData.NumTexCoords, UniqueUVCount );

	//
	// get the first vertex color layer
	//
	FbxLayerElementVertexColor* LayerElementVertexColor = BaseLayer->GetVertexColors();
	FbxLayerElement::EReferenceMode VertexColorReferenceMode(FbxLayerElement::eDirect);
	FbxLayerElement::EMappingMode VertexColorMappingMode(FbxLayerElement::eByControlPoint);
	if (LayerElementVertexColor)
	{
		VertexColorReferenceMode = LayerElementVertexColor->GetReferenceMode();
		VertexColorMappingMode = LayerElementVertexColor->GetMappingMode();
		ImportData.bHasVertexColors = true;
	}

	//
	// get the first normal layer
	//
	FbxLayerElementNormal* LayerElementNormal = BaseLayer->GetNormals();
	FbxLayerElementTangent* LayerElementTangent = BaseLayer->GetTangents();
	FbxLayerElementBinormal* LayerElementBinormal = BaseLayer->GetBinormals();

	//whether there is normal, tangent and binormal data in this mesh
	bool bHasNormalInformation = LayerElementNormal != NULL;
	bool bHasTangentInformation = LayerElementTangent != NULL && LayerElementBinormal != NULL;

	ImportData.bHasNormals = bHasNormalInformation;
	ImportData.bHasTangents = bHasTangentInformation;

	FbxLayerElement::EReferenceMode NormalReferenceMode(FbxLayerElement::eDirect);
	FbxLayerElement::EMappingMode NormalMappingMode(FbxLayerElement::eByControlPoint);
	if (LayerElementNormal)
	{
		NormalReferenceMode = LayerElementNormal->GetReferenceMode();
		NormalMappingMode = LayerElementNormal->GetMappingMode();
	}

	FbxLayerElement::EReferenceMode TangentReferenceMode(FbxLayerElement::eDirect);
	FbxLayerElement::EMappingMode TangentMappingMode(FbxLayerElement::eByControlPoint);
	if (LayerElementTangent)
	{
		TangentReferenceMode = LayerElementTangent->GetReferenceMode();
		TangentMappingMode = LayerElementTangent->GetMappingMode();
	}

	//
	// create the points / wedges / faces
	//
	int32 ControlPointsCount = Mesh->GetControlPointsCount();
	int32 ExistPointNum = ImportData.Points.Num();
	FillSkeletalMeshImportPoints( &ImportData, RootNode, Node, FbxShape );

	// Construct the matrices for the conversion from right handed to left handed system
	FbxAMatrix TotalMatrix;
	FbxAMatrix TotalMatrixForNormal;
	TotalMatrix = ComputeSkeletalMeshTotalMatrix(Node, RootNode);
	TotalMatrixForNormal = TotalMatrix.Inverse();
	TotalMatrixForNormal = TotalMatrixForNormal.Transpose();

	bool OddNegativeScale = IsOddNegativeScale(TotalMatrix);
	
	int32 TriangleCount = Mesh->GetPolygonCount();
	int32 ExistFaceNum = ImportData.Faces.Num();
	ImportData.Faces.AddUninitialized( TriangleCount );
	int32 ExistWedgesNum = ImportData.Wedges.Num();
	VVertex TmpWedges[3];

	for( int32 TriangleIndex = ExistFaceNum, LocalIndex = 0 ; TriangleIndex < ExistFaceNum+TriangleCount ; TriangleIndex++, LocalIndex++ )
	{

		VTriangle& Triangle = ImportData.Faces[TriangleIndex];

		//
		// smoothing mask
		//
		// set the face smoothing by default. It could be any number, but not zero
		Triangle.SmoothingGroups = 255; 
		if ( bSmoothingAvailable)
		{
			if (SmoothingInfo)
			{
				if (SmoothingMappingMode == FbxLayerElement::eByPolygon)
				{
					int32 lSmoothingIndex = (SmoothingReferenceMode == FbxLayerElement::eDirect) ? LocalIndex : SmoothingInfo->GetIndexArray().GetAt(LocalIndex);
					Triangle.SmoothingGroups = SmoothingInfo->GetDirectArray().GetAt(lSmoothingIndex);
				}
				else
				{
					AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("FbxSkeletaLMeshimport_Unsupportingsmoothinggroup", "Unsupported Smoothing group mapping mode on mesh '{0}'"), FText::FromString(Mesh->GetName()))), FFbxErrors::Generic_Mesh_UnsupportingSmoothingGroup);
				}
			}
		}

		for (int32 VertexIndex=0; VertexIndex<3; VertexIndex++)
		{
			// If there are odd number negative scale, invert the vertex order for triangles
			int32 UnrealVertexIndex = OddNegativeScale ? 2 - VertexIndex : VertexIndex;

			int32 ControlPointIndex = Mesh->GetPolygonVertex(LocalIndex, VertexIndex);
			//
			// normals, tangents and binormals
			//
			if( ImportOptions->ShouldImportNormals() && bHasNormalInformation )
			{
				int32 TmpIndex = LocalIndex*3 + VertexIndex;
				//normals may have different reference and mapping mode than tangents and binormals
				int32 NormalMapIndex = (NormalMappingMode == FbxLayerElement::eByControlPoint) ? 
										ControlPointIndex : TmpIndex;
				int32 NormalValueIndex = (NormalReferenceMode == FbxLayerElement::eDirect) ? 
										NormalMapIndex : LayerElementNormal->GetIndexArray().GetAt(NormalMapIndex);

				//tangents and binormals share the same reference, mapping mode and index array
				int32 TangentMapIndex = TmpIndex;

				FbxVector4 TempValue;

				if( ImportOptions->ShouldImportTangents() && bHasTangentInformation )
				{
					TempValue = LayerElementTangent->GetDirectArray().GetAt(TangentMapIndex);
					TempValue = TotalMatrixForNormal.MultT(TempValue);
					Triangle.TangentX[ UnrealVertexIndex ] = Converter.ConvertDir(TempValue);
					Triangle.TangentX[ UnrealVertexIndex ].Normalize();

					TempValue = LayerElementBinormal->GetDirectArray().GetAt(TangentMapIndex);
					TempValue = TotalMatrixForNormal.MultT(TempValue);
					Triangle.TangentY[ UnrealVertexIndex ] = -Converter.ConvertDir(TempValue);
					Triangle.TangentY[ UnrealVertexIndex ].Normalize();
				}

				TempValue = LayerElementNormal->GetDirectArray().GetAt(NormalValueIndex);
				TempValue = TotalMatrixForNormal.MultT(TempValue);
				Triangle.TangentZ[ UnrealVertexIndex ] = Converter.ConvertDir(TempValue);
				Triangle.TangentZ[ UnrealVertexIndex ].Normalize();
			
			}
			else
			{
				int32 NormalIndex;
				for( NormalIndex = 0; NormalIndex < 3; ++NormalIndex )
				{
					Triangle.TangentX[ NormalIndex ] = FVector::ZeroVector;
					Triangle.TangentY[ NormalIndex ] = FVector::ZeroVector;
					Triangle.TangentZ[ NormalIndex ] = FVector::ZeroVector;
				}
			}
		}
		
		//
		// material index
		//
		Triangle.MatIndex = 0; // default value
		if (MaterialCount>0)
		{
			if (LayerElementMaterial)
			{
				switch(MaterialMappingMode)
				{
				// material index is stored in the IndexArray, not the DirectArray (which is irrelevant with 2009.1)
				case FbxLayerElement::eAllSame:
					{	
						Triangle.MatIndex = MaterialMapping[ LayerElementMaterial->GetIndexArray().GetAt(0) ];
					}
					break;
				case FbxLayerElement::eByPolygon:
					{	
						int32 Index = LayerElementMaterial->GetIndexArray().GetAt(LocalIndex);							
						if (!MaterialMapping.IsValidIndex(Index))
						{
							AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("FbxSkeletaLMeshimport_MaterialIndexInconsistency", "Face material index inconsistency - forcing to 0")), FFbxErrors::Generic_Mesh_MaterialIndexInconsistency);
						}
						else
						{
							Triangle.MatIndex = MaterialMapping[Index];
						}
					}
					break;
				}
			}

			// When import morph, we don't check the material index 
			// because we don't import material for morph, so the ImportData.Materials contains zero material
			if ( !FbxShape && (Triangle.MatIndex < 0 ||  Triangle.MatIndex >= FbxMaterials.Num() ) )
			{
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("FbxSkeletaLMeshimport_MaterialIndexInconsistency", "Face material index inconsistency - forcing to 0")), FFbxErrors::Generic_Mesh_MaterialIndexInconsistency);
				Triangle.MatIndex = 0;
			}
		}
		ImportData.MaxMaterialIndex = FMath::Max<uint32>( ImportData.MaxMaterialIndex, Triangle.MatIndex );

		Triangle.AuxMatIndex = 0;
		for (int32 VertexIndex=0; VertexIndex<3; VertexIndex++)
		{
			// If there are odd number negative scale, invert the vertex order for triangles
			int32 UnrealVertexIndex = OddNegativeScale ? 2 - VertexIndex : VertexIndex;

			TmpWedges[UnrealVertexIndex].MatIndex = Triangle.MatIndex;
			TmpWedges[UnrealVertexIndex].VertexIndex = ExistPointNum + Mesh->GetPolygonVertex(LocalIndex,VertexIndex);
			// Initialize all colors to white.
			TmpWedges[UnrealVertexIndex].Color = FColor::White;
		}

		//
		// uvs
		//
		uint32 UVLayerIndex;
		// Some FBX meshes can have no UV sets, so also check the UniqueUVCount
		for ( UVLayerIndex = 0; UVLayerIndex< UniqueUVCount; UVLayerIndex++ )
		{
			// ensure the layer has data
			if (LayerElementUV[UVLayerIndex] != NULL) 
			{
				// Get each UV from the layer
				for (int32 VertexIndex=0;VertexIndex<3;VertexIndex++)
				{
					// If there are odd number negative scale, invert the vertex order for triangles
					int32 UnrealVertexIndex = OddNegativeScale ? 2 - VertexIndex : VertexIndex;

					int32 lControlPointIndex = Mesh->GetPolygonVertex(LocalIndex, VertexIndex);
					int32 UVMapIndex = (UVMappingMode[UVLayerIndex] == FbxLayerElement::eByControlPoint) ? 
							lControlPointIndex : LocalIndex*3+VertexIndex;
					int32 UVIndex = (UVReferenceMode[UVLayerIndex] == FbxLayerElement::eDirect) ? 
							UVMapIndex : LayerElementUV[UVLayerIndex]->GetIndexArray().GetAt(UVMapIndex);
					FbxVector2	UVVector = LayerElementUV[UVLayerIndex]->GetDirectArray().GetAt(UVIndex);

					TmpWedges[UnrealVertexIndex].UVs[ UVLayerIndex ].X = static_cast<float>(UVVector[0]);
					TmpWedges[UnrealVertexIndex].UVs[ UVLayerIndex ].Y = 1.f - static_cast<float>(UVVector[1]);
				}
			}
			else if( UVLayerIndex == 0 )
			{
				// Set all UV's to zero.  If we are here the mesh had no UV sets so we only need to do this for the
				// first UV set which always exists.

				for (int32 VertexIndex=0; VertexIndex<3; VertexIndex++)
				{
					TmpWedges[VertexIndex].UVs[UVLayerIndex].X = 0.0f;
					TmpWedges[VertexIndex].UVs[UVLayerIndex].Y = 0.0f;
				}
			}
		}

		// Read vertex colors if they exist.
		if( LayerElementVertexColor )
		{
			switch(VertexColorMappingMode)
			{
			case FbxLayerElement::eByControlPoint:
				{
					for (int32 VertexIndex=0;VertexIndex<3;VertexIndex++)
					{
						int32 UnrealVertexIndex = OddNegativeScale ? 2 - VertexIndex : VertexIndex;

						FbxColor VertexColor = (VertexColorReferenceMode == FbxLayerElement::eDirect)
							?	LayerElementVertexColor->GetDirectArray().GetAt(Mesh->GetPolygonVertex(LocalIndex,VertexIndex))
							:	LayerElementVertexColor->GetDirectArray().GetAt(LayerElementVertexColor->GetIndexArray().GetAt(Mesh->GetPolygonVertex(LocalIndex,VertexIndex)));

						TmpWedges[UnrealVertexIndex].Color =	FColor(	uint8(255.f*VertexColor.mRed),
																uint8(255.f*VertexColor.mGreen),
																uint8(255.f*VertexColor.mBlue),
																uint8(255.f*VertexColor.mAlpha));
					}
				}
				break;
			case FbxLayerElement::eByPolygonVertex:
				{	
					for (int32 VertexIndex=0;VertexIndex<3;VertexIndex++)
					{
						int32 UnrealVertexIndex = OddNegativeScale ? 2 - VertexIndex : VertexIndex;

						FbxColor VertexColor = (VertexColorReferenceMode == FbxLayerElement::eDirect)
							?	LayerElementVertexColor->GetDirectArray().GetAt(LocalIndex*3+VertexIndex)
							:	LayerElementVertexColor->GetDirectArray().GetAt(LayerElementVertexColor->GetIndexArray().GetAt(LocalIndex*3+VertexIndex));

						TmpWedges[UnrealVertexIndex].Color =	FColor(	uint8(255.f*VertexColor.mRed),
																uint8(255.f*VertexColor.mGreen),
																uint8(255.f*VertexColor.mBlue),
																uint8(255.f*VertexColor.mAlpha));
					}
				}
				break;
			}
		}
		
		//
		// basic wedges matching : 3 unique per face. TODO Can we do better ?
		//
		for (int32 VertexIndex=0; VertexIndex<3; VertexIndex++)
		{
			int32 w;
			
			w = ImportData.Wedges.AddUninitialized();
			ImportData.Wedges[w].VertexIndex = TmpWedges[VertexIndex].VertexIndex;
			ImportData.Wedges[w].MatIndex = TmpWedges[VertexIndex].MatIndex;
			ImportData.Wedges[w].Color = TmpWedges[VertexIndex].Color;
			ImportData.Wedges[w].Reserved = 0;
			FMemory::Memcpy( ImportData.Wedges[w].UVs, TmpWedges[VertexIndex].UVs, sizeof(FVector2D)*MAX_TEXCOORDS );
			
			Triangle.WedgeIndex[VertexIndex] = w;
		}
		
	}
	
	// now we can work on a per-cluster basis with good ordering
	if (Skin) // skeletal mesh
	{
		// create influences for each cluster
		int32 ClusterIndex;
		for (ClusterIndex=0; ClusterIndex<Skin->GetClusterCount(); ClusterIndex++)
		{
			FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
			// When Maya plug-in exports rigid binding, it will generate "CompensationCluster" for each ancestor links.
			// FBX writes these "CompensationCluster" out. The CompensationCluster also has weight 1 for vertices.
			// Unreal importer should skip these clusters.
			if(!Cluster || (FCStringAnsi::Strcmp(Cluster->GetUserDataID(), "Maya_ClusterHint") == 0 && FCStringAnsi::Strcmp(Cluster->GetUserData(), "CompensationCluster")  == 0))
			{
				continue;
			}
			
			FbxNode* Link = Cluster->GetLink();
			// find the bone index
			int32 BoneIndex = -1;
			for (int32 LinkIndex = 0; LinkIndex < SortedLinks.Num(); LinkIndex++)
			{
				if (Link == SortedLinks[LinkIndex])
				{
					BoneIndex = LinkIndex;
					break;
				}
			}

			//	get the vertex indices
			int32 ControlPointIndicesCount = Cluster->GetControlPointIndicesCount();
			int32* ControlPointIndices = Cluster->GetControlPointIndices();
			double* Weights = Cluster->GetControlPointWeights();

			//	for each vertex index in the cluster
			for (int32 ControlPointIndex = 0; ControlPointIndex < ControlPointIndicesCount; ++ControlPointIndex) 
			{
				ImportData.Influences.AddUninitialized();
				ImportData.Influences.Last().BoneIndex = BoneIndex;
				ImportData.Influences.Last().Weight = static_cast<float>(Weights[ControlPointIndex]);
				ImportData.Influences.Last().VertexIndex = ExistPointNum + ControlPointIndices[ControlPointIndex];
			}
		}
	}
	else // for rigid mesh
	{
		// find the bone index
		int32 BoneIndex = -1;
		for (int32 LinkIndex = 0; LinkIndex < SortedLinks.Num(); LinkIndex++)
		{
			// the bone is the node itself
			if (Node == SortedLinks[LinkIndex])
			{
				BoneIndex = LinkIndex;
				break;
			}
		}
		
		//	for each vertex in the mesh
		for (int32 ControlPointIndex = 0; ControlPointIndex < ControlPointsCount; ++ControlPointIndex) 
		{
			ImportData.Influences.AddUninitialized();
			ImportData.Influences.Last().BoneIndex = BoneIndex;
			ImportData.Influences.Last().Weight = 1.0;
			ImportData.Influences.Last().VertexIndex = ExistPointNum + ControlPointIndex;
		}
	}

	//
	// clean up
	//
	if (UniqueUVCount > 0)
	{
		delete[] LayerElementUV;
		delete[] UVReferenceMode;
		delete[] UVMappingMode;
	}
	
	return true;
}

void UnFbx::FFbxImporter::InsertNewLODToBaseSkeletalMesh(USkeletalMesh* InSkeletalMesh, USkeletalMesh* BaseSkeletalMesh, int32 DesiredLOD, UFbxSkeletalMeshImportData* TemplateImportData)
{
	FSkeletalMeshResource* ImportedResource = InSkeletalMesh->GetImportedResource();
	FSkeletalMeshResource* DestImportedResource = BaseSkeletalMesh->GetImportedResource();

	FStaticLODModel& NewLODModel = ImportedResource->LODModels[0];


	//Fill the data we need to recover the user section material slot assignation
	TArray<FName> ExistingMeshSectionSlotNames;
	TArray<FName> OriginalImportMeshSectionSlotNames;
	UFbxSkeletalMeshImportData* ImportData = nullptr;
	bool HasReimportData = (TemplateImportData != nullptr && DesiredLOD != 0 && BaseSkeletalMesh->LODInfo.Num() > DesiredLOD);
	if (HasReimportData)
	{
		ImportData = UFbxSkeletalMeshImportData::GetImportDataForSkeletalMesh(BaseSkeletalMesh, TemplateImportData);
		HasReimportData = (ImportData != nullptr && ImportData->ImportMeshLodData.Num() > DesiredLOD);
		if (HasReimportData)
		{
			const FImportMeshLodSectionsData& OriginalImportMeshLodSectionsData = ImportData->ImportMeshLodData[DesiredLOD];
			const FSkeletalMeshLODInfo &ExistingSkelMeshLodInfo = BaseSkeletalMesh->LODInfo[DesiredLOD];
			//Restore the section changes from the old import data
			for (int32 SectionIndex = 0; SectionIndex < ExistingSkelMeshLodInfo.LODMaterialMap.Num(); SectionIndex++)
			{
				if (ExistingSkelMeshLodInfo.LODMaterialMap.Num() <= SectionIndex || OriginalImportMeshLodSectionsData.SectionOriginalMaterialName.Num() <= SectionIndex)
				{
					break;
				}
				//Get the current skelmesh section slot import name
				int32 ExistRemapMaterialIndex = ExistingSkelMeshLodInfo.LODMaterialMap[SectionIndex];
				ExistingMeshSectionSlotNames.Add(BaseSkeletalMesh->Materials[ExistRemapMaterialIndex].ImportedMaterialSlotName);

				//Get the Last imported skelmesh section slot import name
				OriginalImportMeshSectionSlotNames.Add(OriginalImportMeshLodSectionsData.SectionOriginalMaterialName[SectionIndex]);
			}
		}
	}

	// If we want to add this as a new LOD to this mesh - add to LODModels/LODInfo array.
	if (DesiredLOD == DestImportedResource->LODModels.Num())
	{
		new(DestImportedResource->LODModels)FStaticLODModel();

		// Add element to LODInfo array.
		BaseSkeletalMesh->LODInfo.AddZeroed();
		check(BaseSkeletalMesh->LODInfo.Num() == DestImportedResource->LODModels.Num());
		BaseSkeletalMesh->LODInfo[DesiredLOD] = InSkeletalMesh->LODInfo[0];
	}
	else
	{
		// if it's overwriting existing LOD, need to update section information
		// update to the right # of sections 
		// Set up LODMaterialMap to number of materials in new mesh.
		// ImportedResource->LOD 0 is the newly imported mesh
		FSkeletalMeshLODInfo& LODInfo = BaseSkeletalMesh->LODInfo[DesiredLOD];
		// if section # has been changed
		if (LODInfo.TriangleSortSettings.Num() != NewLODModel.Sections.Num())
		{
			// Save old information so that I can copy it over
			TArray<FTriangleSortSettings> OldTriangleSortSettings;

			OldTriangleSortSettings = LODInfo.TriangleSortSettings;

			// resize to the correct number
			LODInfo.TriangleSortSettings.Empty(NewLODModel.Sections.Num());
			// fill up data
			for (int32 SectionIndex = 0; SectionIndex < NewLODModel.Sections.Num(); ++SectionIndex)
			{
				// if found from previous data, copy over
				if (SectionIndex < OldTriangleSortSettings.Num())
				{
					LODInfo.TriangleSortSettings.Add(OldTriangleSortSettings[SectionIndex]);
				}
				else
				{
					// if not add default data
					LODInfo.TriangleSortSettings.AddZeroed();
				}
			}
		}
	}

	// Set up LODMaterialMap to number of materials in new mesh.
	FSkeletalMeshLODInfo& LODInfo = BaseSkeletalMesh->LODInfo[DesiredLOD];
	
	LODInfo.LODMaterialMap.Empty();
	// Now set up the material mapping array.
	for (int32 MatIdx = 0; MatIdx < InSkeletalMesh->Materials.Num(); MatIdx++)
	{
		// Try and find the auto-assigned material in the array.
		int32 LODMatIndex = INDEX_NONE;
		//First try to match by name
		for (int32 BaseMaterialIndex = 0; BaseMaterialIndex < BaseSkeletalMesh->Materials.Num(); ++BaseMaterialIndex)
		{
			const FSkeletalMaterial& SkeletalMaterial = BaseSkeletalMesh->Materials[BaseMaterialIndex];
			if (SkeletalMaterial.ImportedMaterialSlotName != NAME_None && SkeletalMaterial.ImportedMaterialSlotName == InSkeletalMesh->Materials[MatIdx].ImportedMaterialSlotName)
			{
				LODMatIndex = BaseMaterialIndex;
				break;
			}
		}

		//Then try to match by material
		if (LODMatIndex == INDEX_NONE && InSkeletalMesh->Materials[MatIdx].MaterialInterface != NULL)
		{
			LODMatIndex = BaseSkeletalMesh->Materials.Find(InSkeletalMesh->Materials[MatIdx]);
		}

		// If we didn't just use the index - but make sure its within range of the Materials array.
		if (LODMatIndex == INDEX_NONE)
		{
			LODMatIndex = FMath::Clamp(MatIdx, 0, BaseSkeletalMesh->Materials.Num() - 1);
		}

		LODInfo.LODMaterialMap.Add(LODMatIndex);
	}

	// if new LOD has more material slot, add the extra to main skeletal
	if (BaseSkeletalMesh->Materials.Num() < InSkeletalMesh->Materials.Num())
	{
		BaseSkeletalMesh->Materials.AddZeroed(InSkeletalMesh->Materials.Num() - BaseSkeletalMesh->Materials.Num());
	}

	// same from here as FbxImporter

	// Release all resources before replacing the model
	BaseSkeletalMesh->PreEditChange(NULL);

	// Index buffer will be destroyed when we copy the LOD model so we must copy the index buffer and reinitialize it after the copy
	FMultiSizeIndexContainerData Data;
	NewLODModel.MultiSizeIndexContainer.GetIndexBufferData(Data);
	FMultiSizeIndexContainerData AdjacencyData;
	NewLODModel.AdjacencyMultiSizeIndexContainer.GetIndexBufferData(AdjacencyData);

	// Assign new FStaticLODModel to desired slot in selected skeletal mesh.
	DestImportedResource->LODModels[DesiredLOD] = NewLODModel;

	DestImportedResource->LODModels[DesiredLOD].RebuildIndexBuffer(&Data, &AdjacencyData);

	// If this LOD had been generated previously by automatic mesh reduction, clear that flag.
	LODInfo.bHasBeenSimplified = false;

	//Set back the user data
	if (HasReimportData && BaseSkeletalMesh->LODInfo.Num() > DesiredLOD)
	{
		FSkeletalMeshLODInfo &NewSkelMeshLodInfo = BaseSkeletalMesh->LODInfo[DesiredLOD];
		//Restore the section changes from the old import data
		for (int32 SectionIndex = 0; SectionIndex < NewLODModel.Sections.Num(); SectionIndex++)
		{
			if (ExistingMeshSectionSlotNames.Num() <= SectionIndex || OriginalImportMeshSectionSlotNames.Num() <= SectionIndex)
			{
				break;
			}
			//Get the current skelmesh section slot import name
			FName ExistMeshSectionSlotName = ExistingMeshSectionSlotNames[SectionIndex];
			//Get the new skelmesh section slot import name
			int32 NewRemapMaterialIndex = NewSkelMeshLodInfo.LODMaterialMap[SectionIndex];
			FName NewMeshSectionSlotName = BaseSkeletalMesh->Materials[NewRemapMaterialIndex].ImportedMaterialSlotName;
			//Get the Last imported skelmesh section slot import name
			FName OriginalImportMeshSectionSlotName = OriginalImportMeshSectionSlotNames[SectionIndex];

			if (OriginalImportMeshSectionSlotName == NewMeshSectionSlotName && ExistMeshSectionSlotName != OriginalImportMeshSectionSlotName)
			{
				//The last import slot name match the New import slot name, but the Exist slot name is different then the last import slot name.
				//This mean the user has change the section assign slot and the fbx file did not change it
				//Override the new section material index to use the one that the user set
				for (int32 RemapMaterialIndex = 0; RemapMaterialIndex < BaseSkeletalMesh->Materials.Num(); ++RemapMaterialIndex)
				{
					const FSkeletalMaterial &NewSectionMaterial = BaseSkeletalMesh->Materials[RemapMaterialIndex];
					if (NewSectionMaterial.ImportedMaterialSlotName == ExistMeshSectionSlotName)
					{
						NewSkelMeshLodInfo.LODMaterialMap[SectionIndex] = RemapMaterialIndex;
						break;
					}
				}
			}
		}
	}

	// rebuild vertex buffers and reinit RHI resources
	BaseSkeletalMesh->PostEditChange();
}

bool UnFbx::FFbxImporter::ImportSkeletalMeshLOD(USkeletalMesh* InSkeletalMesh, USkeletalMesh* BaseSkeletalMesh, int32 DesiredLOD, bool bNeedToReregister, TArray<UActorComponent*>* ReregisterAssociatedComponents, UFbxSkeletalMeshImportData* TemplateImportData)
{
	check(InSkeletalMesh);
	check(BaseSkeletalMesh);

	FSkeletalMeshResource* ImportedResource = InSkeletalMesh->GetImportedResource();
	FSkeletalMeshResource* DestImportedResource = BaseSkeletalMesh->GetImportedResource();

	// Now we copy the base FStaticLODModel from the imported skeletal mesh as the new LOD in the selected mesh.
	check(ImportedResource->LODModels.Num() == 1);

	// Names of root bones must match.
	// If the names of root bones don't match, the LOD Mesh does not share skeleton with base Mesh. 
	if (InSkeletalMesh->RefSkeleton.GetBoneName(0) != BaseSkeletalMesh->RefSkeleton.GetBoneName(0))
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("LODRootNameIncorrect", "Root bone in LOD is '{0}' instead of '{1}'.\nImport failed."),
			FText::FromName(InSkeletalMesh->RefSkeleton.GetBoneName(0)), FText::FromName(BaseSkeletalMesh->RefSkeleton.GetBoneName(0)))), FFbxErrors::SkeletalMesh_LOD_RootNameIncorrect);

		return false;
	}

	// We do some checking here that for every bone in the mesh we just imported, it's in our base ref skeleton, and the parent is the same.
	for (int32 i = 0; i < InSkeletalMesh->RefSkeleton.GetRawBoneNum(); i++)
	{
		int32 LODBoneIndex = i;
		FName LODBoneName = InSkeletalMesh->RefSkeleton.GetBoneName(LODBoneIndex);
		int32 BaseBoneIndex = BaseSkeletalMesh->RefSkeleton.FindBoneIndex(LODBoneName);
		if (BaseBoneIndex == INDEX_NONE)
		{
			// If we could not find the bone from this LOD in base mesh - we fail.
			AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("LODBoneDoesNotMatch", "Bone '{0}' not found in base SkeletalMesh '{1}'.\nImport failed."),
				FText::FromName(LODBoneName), FText::FromString(BaseSkeletalMesh->GetName()))), FFbxErrors::SkeletalMesh_LOD_BonesDoNotMatch);

			return false;
		}

		if (i > 0)
		{
			int32 LODParentIndex = InSkeletalMesh->RefSkeleton.GetParentIndex(LODBoneIndex);
			FName LODParentName = InSkeletalMesh->RefSkeleton.GetBoneName(LODParentIndex);

			int32 BaseParentIndex = BaseSkeletalMesh->RefSkeleton.GetParentIndex(BaseBoneIndex);
			FName BaseParentName = BaseSkeletalMesh->RefSkeleton.GetBoneName(BaseParentIndex);

			if (LODParentName != BaseParentName)
			{
				// If bone has different parents, display an error and don't allow import.
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("LODBoneHasIncorrectParent", "Bone '{0}' in LOD has parent '{1}' instead of '{2}'"),
					FText::FromName(LODBoneName), FText::FromName(LODParentName), FText::FromName(BaseParentName))), FFbxErrors::SkeletalMesh_LOD_IncorrectParent);

				return false;
			}
		}
	}

	FStaticLODModel& NewLODModel = ImportedResource->LODModels[0];

	// Enforce LODs having only single-influence vertices.
	bool bCheckSingleInfluence;
	GConfig->GetBool(TEXT("ImportSetting"), TEXT("CheckSingleInfluenceLOD"), bCheckSingleInfluence, GEditorIni);
	if (bCheckSingleInfluence &&
		DesiredLOD > 0)
	{
		for (int32 SectionIndex = 0; SectionIndex < NewLODModel.Sections.Num(); SectionIndex++)
		{
			if (NewLODModel.Sections[SectionIndex].SoftVertices.Num() > 0)
			{
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Warning, FText(LOCTEXT("LODHasSoftVertices", "Warning: The mesh LOD you are importing has some vertices with more than one influence."))), FFbxErrors::SkeletalMesh_LOD_HasSoftVerts);
			}
		}
	}

	// If this LOD is going to be the lowest one, we check all bones we have sockets on are present in it.
	if (DesiredLOD == DestImportedResource->LODModels.Num() ||
		DesiredLOD == DestImportedResource->LODModels.Num() - 1)
	{
		const TArray<USkeletalMeshSocket*>& Sockets = BaseSkeletalMesh->GetMeshOnlySocketList();

		for (int32 i = 0; i < Sockets.Num(); i++)
		{
			// Find bone index the socket is attached to.
			USkeletalMeshSocket* Socket = Sockets[i];
			int32 SocketBoneIndex = InSkeletalMesh->RefSkeleton.FindBoneIndex(Socket->BoneName);

			// If this LOD does not contain the socket bone, abort import.
			if (SocketBoneIndex == INDEX_NONE)
			{
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("LODMissingSocketBone", "This LOD is missing bone '{0}' used by socket '{1}'.\nAborting import."),
					FText::FromName(Socket->BoneName), FText::FromName(Socket->SocketName))), FFbxErrors::SkeletalMesh_LOD_MissingSocketBone);

				return false;
			}
		}
	}

	// Fix up the ActiveBoneIndices array.
	for (int32 i = 0; i < NewLODModel.ActiveBoneIndices.Num(); i++)
	{
		int32 LODBoneIndex = NewLODModel.ActiveBoneIndices[i];
		FName LODBoneName = InSkeletalMesh->RefSkeleton.GetBoneName(LODBoneIndex);
		int32 BaseBoneIndex = BaseSkeletalMesh->RefSkeleton.FindBoneIndex(LODBoneName);
		NewLODModel.ActiveBoneIndices[i] = BaseBoneIndex;
	}

	// Fix up the chunk BoneMaps.
	for (int32 SectionIndex = 0; SectionIndex < NewLODModel.Sections.Num(); SectionIndex++)
	{
		FSkelMeshSection& Section = NewLODModel.Sections[SectionIndex];
		for (int32 i = 0; i < Section.BoneMap.Num(); i++)
		{
			int32 LODBoneIndex = Section.BoneMap[i];
			FName LODBoneName = InSkeletalMesh->RefSkeleton.GetBoneName(LODBoneIndex);
			int32 BaseBoneIndex = BaseSkeletalMesh->RefSkeleton.FindBoneIndex(LODBoneName);
			Section.BoneMap[i] = BaseBoneIndex;
		}
	}

	// Create the RequiredBones array in the LODModel from the ref skeleton.
	for (int32 i = 0; i < NewLODModel.RequiredBones.Num(); i++)
	{
		FName LODBoneName = InSkeletalMesh->RefSkeleton.GetBoneName(NewLODModel.RequiredBones[i]);
		int32 BaseBoneIndex = BaseSkeletalMesh->RefSkeleton.FindBoneIndex(LODBoneName);
		if (BaseBoneIndex != INDEX_NONE)
		{
			NewLODModel.RequiredBones[i] = BaseBoneIndex;
		}
		else
		{
			NewLODModel.RequiredBones.RemoveAt(i--);
		}
	}

	// Also sort the RequiredBones array to be strictly increasing.
	NewLODModel.RequiredBones.Sort();	
	BaseSkeletalMesh->RefSkeleton.EnsureParentsExistAndSort(NewLODModel.ActiveBoneIndices);

	// To be extra-nice, we apply the difference between the root transform of the meshes to the verts.
	FMatrix LODToBaseTransform = InSkeletalMesh->GetRefPoseMatrix(0).InverseFast() * BaseSkeletalMesh->GetRefPoseMatrix(0);

	for (int32 SectionIndex = 0; SectionIndex < NewLODModel.Sections.Num(); SectionIndex++)
	{
		FSkelMeshSection& Section = NewLODModel.Sections[SectionIndex];

		// Fix up soft verts.
		for (int32 i = 0; i < Section.SoftVertices.Num(); i++)
		{
			Section.SoftVertices[i].Position = LODToBaseTransform.TransformPosition(Section.SoftVertices[i].Position);
			Section.SoftVertices[i].TangentX = LODToBaseTransform.TransformVector(Section.SoftVertices[i].TangentX);
			Section.SoftVertices[i].TangentY = LODToBaseTransform.TransformVector(Section.SoftVertices[i].TangentY);
			Section.SoftVertices[i].TangentZ = LODToBaseTransform.TransformVector(Section.SoftVertices[i].TangentZ);
		}
	}

	if (bNeedToReregister)
	{
		// Shut down the skeletal mesh component that is previewing this mesh.
		if (ReregisterAssociatedComponents)
		{
			FMultiComponentReregisterContext ReregisterContext(*ReregisterAssociatedComponents);
			// wait until resources are released
			FlushRenderingCommands();

			InsertNewLODToBaseSkeletalMesh(InSkeletalMesh, BaseSkeletalMesh, DesiredLOD, TemplateImportData);

			// ReregisterContexts go out of scope here, reregistering associated components with the scene.
		}
		else
		{
			TComponentReregisterContext<USkinnedMeshComponent> ComponentReregisterContext;

			InsertNewLODToBaseSkeletalMesh(InSkeletalMesh, BaseSkeletalMesh, DesiredLOD, TemplateImportData);

			// ReregisterContexts go out of scope here, reregistering skel components with the scene.
		}
	}
	else
	{
		InsertNewLODToBaseSkeletalMesh(InSkeletalMesh, BaseSkeletalMesh, DesiredLOD, TemplateImportData);
	}

	return true;
}

static void ConvertSkeletonImportDataToMeshData( const FSkeletalMeshImportData& ImportData, TArray< FVector >& OutVertices, TArray< uint32 >& OutIndices, TArray< FVector2D >& OutUVs, TArray< uint32 >& OutSmoothingGroups )
{
	for ( const VTriangle& Face : ImportData.Faces )
	{
		for ( int32 i = 0; i < 3; ++i )
		{
			const VVertex& Wedge = ImportData.Wedges[ Face.WedgeIndex[i] ];

			OutIndices.Add( Wedge.VertexIndex );
			OutUVs.Add( Wedge.UVs[0] );
		}

		OutSmoothingGroups.Add( Face.SmoothingGroups );
	}

	OutVertices = ImportData.Points;
}

/**
* A class encapsulating morph target processing that occurs during import on a separate thread
*/
class FAsyncImportMorphTargetWork : public FNonAbandonableTask
{
public:
	FAsyncImportMorphTargetWork(FStaticLODModel* InLODModel, const FReferenceSkeleton& InRefSkeleton, const FSkeletalMeshImportData& InBaseImportData, TArray<FVector>&& InMorphLODPoints,
		FBXImportOptions* InImportOptions, TArray< FMorphTargetDelta >& InMorphDeltas, FMultiSizeIndexContainerData& InBaseIndexData, TArray< uint32 >& InBaseWedgePointIndices,
		TMap<uint32,uint32>& InWedgePointToVertexIndexMap, const TMultiMap<int32, int32>& InOverlappingCorners,
		const TSet<uint32> InModifiedPoints, const TMultiMap< int32, int32 >& InWedgeToFaces, const TArray<FVector>& InTangentX, const TArray<FVector>& InTangentY, const TArray<FVector>& InTangentZ)
		: LODModel(InLODModel)
		, RefSkeleton(InRefSkeleton)
		, BaseImportData(InBaseImportData)
		, MorphLODPoints(InMorphLODPoints)
		, ImportOptions(InImportOptions)
		, MorphTargetDeltas(InMorphDeltas)
		, BaseIndexData (InBaseIndexData)
		, BaseWedgePointIndices(InBaseWedgePointIndices)
		, WedgePointToVertexIndexMap(InWedgePointToVertexIndexMap)
		, OverlappingCorners(InOverlappingCorners)
		, ModifiedPoints(InModifiedPoints)
		, WedgeToFaces(InWedgeToFaces)
		, BaseTangentZ(InTangentZ)
		, TangentX(InTangentX)
		, TangentY(InTangentY)
		, TangentZ(InTangentZ)
	{
		MeshUtilities = &FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	}

	void PrepareTangents()
	{
		TArray<bool> WasProcessed;
		WasProcessed.Empty( BaseImportData.Wedges.Num() );
		WasProcessed.AddZeroed( BaseImportData.Wedges.Num() );

		TArray< int32 > OverlappingWedges;
		TArray< int32 > WedgeFaces;

		// For each ModifiedPoints, reset the tangents for the affected wedges
		for ( int32 WedgeIdx = 0; WedgeIdx < BaseImportData.Wedges.Num(); ++WedgeIdx )
		{
			int32 PointIdx = BaseImportData.Wedges[ WedgeIdx ].VertexIndex;

			if ( ModifiedPoints.Find( PointIdx ) != nullptr )
			{
				TangentX[ WedgeIdx ] = FVector::ZeroVector;
				TangentY[ WedgeIdx ] = FVector::ZeroVector;
				TangentZ[ WedgeIdx ] = FVector::ZeroVector;

				OverlappingWedges.Reset();
				OverlappingCorners.MultiFind( WedgeIdx, OverlappingWedges );
				OverlappingWedges.Add( WedgeIdx );

				for ( const int32 OverlappingWedgeIndex : OverlappingWedges )
				{
					if ( WasProcessed[ OverlappingWedgeIndex ] )
					{
						continue;
					}

					WasProcessed[ OverlappingWedgeIndex ] = true;

					WedgeFaces.Reset();
					WedgeToFaces.MultiFind( OverlappingWedgeIndex, WedgeFaces );

					for ( const int32 FaceIndex : WedgeFaces )
					{
						for ( int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex )
						{
							int32 WedgeIndex = BaseImportData.Faces[ FaceIndex ].WedgeIndex[ CornerIndex ];

							TangentX[ WedgeIndex ] = FVector::ZeroVector;
							TangentY[ WedgeIndex ] = FVector::ZeroVector;
							TangentZ[ WedgeIndex ] = FVector::ZeroVector;

							TArray< int32 > OtherOverlappingWedges;
							OverlappingCorners.MultiFind( WedgeIndex, OtherOverlappingWedges );
							OtherOverlappingWedges.Add( WedgeIndex );

							for ( const int32 OtherDupVert : OtherOverlappingWedges )
							{
								TArray< int32 > OtherWedgeFaces;
								WedgeToFaces.MultiFind( OtherDupVert, OtherWedgeFaces );

								for ( const int32 OtherFaceIndex : OtherWedgeFaces )
								{
									for ( int32 OtherCornerIndex = 0; OtherCornerIndex < 3; ++OtherCornerIndex )
									{
										int32 OtherWedgeIndex = BaseImportData.Faces[ OtherFaceIndex ].WedgeIndex[ OtherCornerIndex ];

										TangentX[ OtherWedgeIndex ] = FVector::ZeroVector;
										TangentY[ OtherWedgeIndex ] = FVector::ZeroVector;
										TangentZ[ OtherWedgeIndex ] = FVector::ZeroVector;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	void ComputeTangents()
	{
		bool bComputeNormals = !ImportOptions->ShouldImportNormals() || !BaseImportData.bHasNormals;
		bool bComputeTangents = !ImportOptions->ShouldImportTangents() || !BaseImportData.bHasTangents;
		bool bUseMikkTSpace = (ImportOptions->NormalGenerationMethod == EFBXNormalGenerationMethod::MikkTSpace) && (!ImportOptions->ShouldImportNormals() || !ImportOptions->ShouldImportTangents());

		TArray<FVector> DummyPoints; // Use MorphLODPoints instead
		TArray<uint32> Indices;
		TArray<FVector2D> UVs;
		TArray<uint32> SmoothingGroups;

		ConvertSkeletonImportDataToMeshData( BaseImportData, DummyPoints, Indices, UVs, SmoothingGroups );
		check(MorphLODPoints.Num() == DummyPoints.Num());

		ETangentOptions::Type TangentOptions = ETangentOptions::BlendOverlappingNormals;

		// MikkTSpace should be use only when the user want to recompute the normals or tangents otherwise should always fallback on builtin
		if ( bUseMikkTSpace && ( bComputeNormals || bComputeTangents ) )
		{
			TangentOptions = (ETangentOptions::Type)(TangentOptions | ETangentOptions::UseMikkTSpace);
		}

		MeshUtilities->CalculateTangents( MorphLODPoints, Indices, UVs, SmoothingGroups, TangentOptions, TangentX, TangentY, TangentZ );
	}

	void ComputeMorphDeltas()
	{
		TArray<bool> WasProcessed;
		WasProcessed.Empty(LODModel->NumVertices );
		WasProcessed.AddZeroed(LODModel->NumVertices);

		for ( int32 Idx = 0; Idx < BaseIndexData.Indices.Num(); ++Idx )
		{
			uint32 BaseVertIdx = BaseIndexData.Indices[Idx];
			// check for duplicate processing
			if( !WasProcessed[BaseVertIdx] )
			{
				// mark this base vertex as already processed
				WasProcessed[BaseVertIdx] = true;

				// clothing can add extra verts, and we won't have source point, so we ignore those
				if ( BaseWedgePointIndices.IsValidIndex(BaseVertIdx) )
				{
					// get the base mesh's original wedge point index
					uint32 BasePointIdx = BaseWedgePointIndices[ BaseVertIdx ];
					if (BaseImportData.Points.IsValidIndex(BasePointIdx) && MorphLODPoints.IsValidIndex(BasePointIdx))
					{
						FVector BasePosition = BaseImportData.Points[BasePointIdx];
						FVector TargetPosition = MorphLODPoints[BasePointIdx];

					FVector PositionDelta = TargetPosition - BasePosition;

						uint32* VertexIdx = WedgePointToVertexIndexMap.Find(BasePointIdx);

					FVector NormalDeltaZ = FVector::ZeroVector;

						if (VertexIdx != nullptr)
					{
							FVector BaseNormal = BaseTangentZ[*VertexIdx];
							FVector TargetNormal = TangentZ[*VertexIdx];

						NormalDeltaZ = TargetNormal - BaseNormal;
					}

					// check if position actually changed much
						if (PositionDelta.SizeSquared() > FMath::Square(THRESH_POINTS_ARE_NEAR) ||
						// since we can't get imported morphtarget normal from FBX
						// we can't compare normal unless it's calculated
						// this is special flag to ignore normal diff
							((ImportOptions->ShouldImportNormals() == false) && NormalDeltaZ.SizeSquared() > 0.01f))
					{
						// create a new entry
						FMorphTargetDelta NewVertex;
						// position delta
						NewVertex.PositionDelta = PositionDelta;
						// normal delta
						NewVertex.TangentZDelta = NormalDeltaZ;
						// index of base mesh vert this entry is to modify
						NewVertex.SourceIdx = BaseVertIdx;

						// add it to the list of changed verts
							MorphTargetDeltas.Add(NewVertex);
						}
					}
				}
			}
		}
	}

	void DoWork()
	{
		PrepareTangents();
		ComputeTangents();
		ComputeMorphDeltas();
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncImportMorphTargetWork, STATGROUP_ThreadPoolAsyncTasks);
	}

private:
	FStaticLODModel* LODModel;
	// @todo not thread safe
	const FReferenceSkeleton& RefSkeleton;
	const FSkeletalMeshImportData& BaseImportData;
	const TArray<FVector> MorphLODPoints;

	IMeshUtilities* MeshUtilities;
	FBXImportOptions* ImportOptions;

	TArray< FMorphTargetDelta >& MorphTargetDeltas;
	FMultiSizeIndexContainerData& BaseIndexData;
	TArray< uint32 >& BaseWedgePointIndices;
	TMap<uint32,uint32>& WedgePointToVertexIndexMap;

	const TMultiMap<int32, int32>& OverlappingCorners;
	const TSet<uint32> ModifiedPoints;
	const TMultiMap< int32, int32 >& WedgeToFaces;

	const TArray<FVector>& BaseTangentZ;
	TArray<FVector> TangentX;
	TArray<FVector> TangentY;
	TArray<FVector> TangentZ;
};

void UnFbx::FFbxImporter::ImportMorphTargetsInternal( TArray<FbxNode*>& SkelMeshNodeArray, USkeletalMesh* BaseSkelMesh, UObject* InParent, int32 LODIndex, const FSkeletalMeshImportData &BaseImportData)
{
	FbxString ShapeNodeName;
	TMap<FString, TArray<FbxShape*>> ShapeNameToShapeArray;

	// Temp arrays to keep track of data being used by threads
	TArray< TArray< FMorphTargetDelta >* > Results;
	TArray<UMorphTarget*> MorphTargets;

	// Array of pending tasks that are not complete
	TIndirectArray<FAsyncTask<FAsyncImportMorphTargetWork> > PendingWork;

	GWarn->BeginSlowTask( NSLOCTEXT("FbxImporter", "BeginGeneratingMorphModelsTask", "Generating Morph Models"), true);

	// For each morph in FBX geometries, we create one morph target for the Unreal skeletal mesh
	for (int32 NodeIndex = 0; NodeIndex < SkelMeshNodeArray.Num(); NodeIndex++)
	{
		FbxGeometry* Geometry = (FbxGeometry*)SkelMeshNodeArray[NodeIndex]->GetNodeAttribute();
		if (Geometry)
		{
			const int32 BlendShapeDeformerCount = Geometry->GetDeformerCount(FbxDeformer::eBlendShape);

			/************************************************************************/
			/* collect all the shapes                                               */
			/************************************************************************/
			for(int32 BlendShapeIndex = 0; BlendShapeIndex<BlendShapeDeformerCount; ++BlendShapeIndex)
			{
				FbxBlendShape* BlendShape = (FbxBlendShape*)Geometry->GetDeformer(BlendShapeIndex, FbxDeformer::eBlendShape);
				const int32 BlendShapeChannelCount = BlendShape->GetBlendShapeChannelCount();

				FString BlendShapeName = UTF8_TO_TCHAR(MakeName(BlendShape->GetName()));

				// see below where this is used for explanation...
				const bool bMightBeBadMAXFile = (BlendShapeName == FString("Morpher"));

				for(int32 ChannelIndex = 0; ChannelIndex<BlendShapeChannelCount; ++ChannelIndex)
				{
					FbxBlendShapeChannel* Channel = BlendShape->GetBlendShapeChannel(ChannelIndex);
					if(Channel)
					{
						//Find which shape should we use according to the weight.
						const int32 CurrentChannelShapeCount = Channel->GetTargetShapeCount();
						
						FString ChannelName = UTF8_TO_TCHAR(MakeName(Channel->GetName()));

						// Maya adds the name of the blendshape and an underscore to the front of the channel name, so remove it
						if(ChannelName.StartsWith(BlendShapeName))
						{
							ChannelName = ChannelName.Right(ChannelName.Len() - (BlendShapeName.Len()+1));
						}

						for(int32 ShapeIndex = 0; ShapeIndex<CurrentChannelShapeCount; ++ShapeIndex)
						{
							FbxShape* Shape = Channel->GetTargetShape(ShapeIndex);

							FString ShapeName;
							if( CurrentChannelShapeCount > 1 )
							{
								ShapeName = UTF8_TO_TCHAR(MakeName(Shape->GetName() ) );
							}
							else
							{
								if (bMightBeBadMAXFile)
								{
									ShapeName = UTF8_TO_TCHAR(MakeName(Shape->GetName()));
								}
								else
								{
									// Maya concatenates the number of the shape to the end of its name, so instead use the name of the channel
									ShapeName = ChannelName;
								}
							}

							TArray<FbxShape*> & ShapeArray = ShapeNameToShapeArray.FindOrAdd(ShapeName);
							if (ShapeArray.Num() == 0)
							{
								ShapeArray.AddZeroed(SkelMeshNodeArray.Num());
							}

							ShapeArray[NodeIndex] = Shape;
						}
					}
				}
			}
		}
	} // for NodeIndex

	// Prepare base data
	TArray<FVector> TangentX;
	TArray<FVector> TangentY;
	TArray<FVector> TangentZ;

	FStaticLODModel& BaseLODModel = BaseSkelMesh->GetImportedResource()->LODModels[ LODIndex ];

	// Calculate overlapping corners and tangents
	bool bComputeNormals = !ImportOptions->ShouldImportNormals() || !BaseImportData.bHasNormals;
	bool bComputeTangents = !ImportOptions->ShouldImportTangents() || !BaseImportData.bHasTangents;
	bool bUseMikkTSpace = (ImportOptions->NormalGenerationMethod == EFBXNormalGenerationMethod::MikkTSpace) && (!ImportOptions->ShouldImportNormals() || !ImportOptions->ShouldImportTangents());

	TArray<FVector> Points;
	TArray<uint32> Indices;
	TArray<FVector2D> UVs;
	TArray<uint32> SmoothingGroups;

	ConvertSkeletonImportDataToMeshData( BaseImportData, Points, Indices, UVs, SmoothingGroups );

	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");

	ETangentOptions::Type TangentOptions = ETangentOptions::BlendOverlappingNormals;

	// MikkTSpace should be use only when the user want to recompute the normals or tangents otherwise should always fallback on builtin
	if ( bUseMikkTSpace && ( bComputeNormals || bComputeTangents ) )
	{
		TangentOptions = (ETangentOptions::Type)(TangentOptions | ETangentOptions::UseMikkTSpace);
	}

	TMultiMap<int32, int32> OverlappingVertices;
	MeshUtilities.CalculateOverlappingCorners( Points, Indices, false, OverlappingVertices );
	MeshUtilities.CalculateTangents( Points, Indices, UVs, SmoothingGroups, TangentOptions, TangentX, TangentY, TangentZ );

	TArray< uint32 > BaseWedgePointIndices;
	if (BaseLODModel.RawPointIndices.GetBulkDataSize())
	{
		BaseWedgePointIndices.Empty(BaseLODModel.RawPointIndices.GetElementCount());
		BaseWedgePointIndices.AddUninitialized(BaseLODModel.RawPointIndices.GetElementCount());
		FMemory::Memcpy(BaseWedgePointIndices.GetData(), BaseLODModel.RawPointIndices.Lock(LOCK_READ_ONLY), BaseLODModel.RawPointIndices.GetBulkDataSize());
		BaseLODModel.RawPointIndices.Unlock();
	}

	FMultiSizeIndexContainerData BaseIndexData;
	BaseLODModel.MultiSizeIndexContainer.GetIndexBufferData(BaseIndexData);

	TMap<uint32,uint32> WedgePointToVertexIndexMap;
	// Build a mapping of wedge point indices to vertex indices for fast lookup later.
	for( int32 Idx = 0; Idx < BaseImportData.Wedges.Num(); ++Idx )
	{
		WedgePointToVertexIndexMap.Add( BaseImportData.Wedges[Idx].VertexIndex, Idx);
	}

	// Create a map from wedge indices to faces
	TMultiMap< int32, int32 > WedgeToFaces;
	for (int32 FaceIndex = 0; FaceIndex < BaseImportData.Faces.Num(); FaceIndex++)
	{
		const VTriangle& Face = BaseImportData.Faces[FaceIndex];
		for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
		{
			WedgeToFaces.AddUnique(Face.WedgeIndex[CornerIndex], FaceIndex);
		}
	}

	int32 ShapeIndex = 0;
	int32 TotalShapeCount = ShapeNameToShapeArray.Num();
	// iterate through shapename, and create morphtarget
	for (auto Iter = ShapeNameToShapeArray.CreateIterator(); Iter; ++Iter)
	{
		FString ShapeName = Iter.Key();
		TArray< FbxShape* >& ShapeArray = Iter.Value();

		FFormatNamedArguments Args;
		Args.Add(TEXT("ShapeName"), FText::FromString(ShapeName));
		Args.Add(TEXT("CurrentShapeIndex"), ShapeIndex + 1);
		Args.Add(TEXT("TotalShapes"), TotalShapeCount);
		const FText StatusUpate = FText::Format(NSLOCTEXT("FbxImporter", "GeneratingMorphTargetMeshStatus", "Generating morph target mesh {ShapeName} ({CurrentShapeIndex} of {TotalShapes})"), Args);

		GWarn->StatusUpdate(ShapeIndex + 1, TotalShapeCount, StatusUpate);

		FSkeletalMeshImportData ShapeImportData = BaseImportData;

		TSet<uint32> ModifiedPoints;
		GatherPointsForMorphTarget( &ShapeImportData, SkelMeshNodeArray, &ShapeArray, ModifiedPoints );

		// See if this morph target already exists.
		UMorphTarget * MorphTarget = FindObject<UMorphTarget>(BaseSkelMesh, *ShapeName);
		// we only create new one for LOD0, otherwise don't create new one
		if (!MorphTarget)
		{
			if (LODIndex == 0)
			{
				MorphTarget = NewObject<UMorphTarget>(BaseSkelMesh, FName(*ShapeName));
			}
			else
			{
				AddTokenizedErrorMessage(FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(FText::FromString("Could not find the {0} morphtarget for LOD {1}. \
					Make sure the name for morphtarget matches with LOD 0"), FText::FromString(ShapeName), FText::FromString(FString::FromInt(LODIndex)))),
					FFbxErrors::SkeletalMesh_LOD_MissingMorphTarget);
			}
		}

		if (MorphTarget)
		{
			MorphTargets.Add( MorphTarget );
			int32 NewMorphDeltasIdx = Results.Add( new TArray< FMorphTargetDelta >() );

			TArray< FMorphTargetDelta >* Deltas = Results[ NewMorphDeltasIdx ];

			FAsyncTask<FAsyncImportMorphTargetWork>* NewWork = new (PendingWork)FAsyncTask<FAsyncImportMorphTargetWork>( &BaseLODModel, BaseSkelMesh->RefSkeleton, BaseImportData,
				MoveTemp( ShapeImportData.Points ), ImportOptions, *Deltas, BaseIndexData, BaseWedgePointIndices, WedgePointToVertexIndexMap, OverlappingVertices, MoveTemp( ModifiedPoints ), WedgeToFaces,
				TangentX, TangentY, TangentZ);

			NewWork->StartBackgroundTask();
		}

		++ShapeIndex;;
	}

	// Wait for all importing tasks to complete
	int32 NumCompleted = 0;
	int32 NumTasks = PendingWork.Num();

	for ( int32 TaskIndex = 0; TaskIndex < PendingWork.Num(); ++TaskIndex )
	{
		PendingWork[TaskIndex].EnsureCompletion();

		++NumCompleted;

		FFormatNamedArguments Args;
		Args.Add( TEXT("NumCompleted"), NumCompleted );
		Args.Add( TEXT("NumTasks"), NumTasks );
		GWarn->StatusUpdate( NumCompleted, NumTasks, FText::Format( LOCTEXT("ImportingMorphTargetStatus", "Importing Morph Target: {NumCompleted} of {NumTasks}"), Args ) );
	}

	const FMorphMeshRawSource BaseMeshRawData( BaseSkelMesh, LODIndex );

	// Create morph streams for each morph target we are importing.
	// This has to happen on a single thread since the skeletal meshes' bulk data is locked and cant be accessed by multiple threads simultaneously
	for (int32 Index = 0; Index < MorphTargets.Num(); Index++)
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("NumCompleted"), Index+1 );
		Args.Add(TEXT("NumTasks"), MorphTargets.Num());
		GWarn->StatusUpdate(Index + 1, MorphTargets.Num(), FText::Format(LOCTEXT("BuildingMorphTargetRenderDataStatus", "Building Morph Target Render Data: {NumCompleted} of {NumTasks}"), Args));

		UMorphTarget* MorphTarget = MorphTargets[Index];

		MorphTarget->PopulateDeltas(*Results[Index], LODIndex, ImportOptions->ShouldImportNormals() == false);

		// register does mark package as dirty
		if (MorphTarget->HasValidData())
		{
			BaseSkelMesh->RegisterMorphTarget(MorphTarget);
		}

		delete Results[Index];
		Results[Index] = nullptr;
	}

	GWarn->EndSlowTask();
}

// Import Morph target
void UnFbx::FFbxImporter::ImportFbxMorphTarget(TArray<FbxNode*> &SkelMeshNodeArray, USkeletalMesh* BaseSkelMesh, UObject* InParent, int32 LODIndex, const FSkeletalMeshImportData &BaseSkeletalMeshImportData)
{
	bool bHasMorph = false;
	int32 NodeIndex;
	// check if there are morph in this geometry
	for (NodeIndex = 0; NodeIndex < SkelMeshNodeArray.Num(); NodeIndex++)
	{
		FbxGeometry* Geometry = (FbxGeometry*)SkelMeshNodeArray[NodeIndex]->GetNodeAttribute();
		if (Geometry)
		{
			bHasMorph = Geometry->GetDeformerCount(FbxDeformer::eBlendShape) > 0;
			if (bHasMorph)
			{
				break;
			}
		}
	}
	
	if (bHasMorph)
	{
		ImportMorphTargetsInternal( SkelMeshNodeArray, BaseSkelMesh, InParent, LODIndex, BaseSkeletalMeshImportData );
	}
}

void UnFbx::FFbxImporter::AddTokenizedErrorMessage(TSharedRef<FTokenizedMessage> Error, FName FbxErrorName)
{
	// check to see if Logger exists, this way, we guarantee only prints to FBX import
	// when we meant to print
	if (Logger)
	{
		Logger->TokenizedErrorMessages.Add(Error);

		if(FbxErrorName != NAME_None)
		{
			Error->AddToken(FFbxErrorToken::Create(FbxErrorName));
		}
	}
	else
	{
		// if not found, use normal log
		UE_LOG(LogFbx, Warning, TEXT("%s"), *(Error->ToText().ToString()));
	}
}

void UnFbx::FFbxImporter::ClearTokenizedErrorMessages()
{
	if(Logger)
	{
		Logger->TokenizedErrorMessages.Empty();
	}
}

void UnFbx::FFbxImporter::FlushToTokenizedErrorMessage(EMessageSeverity::Type Severity)
{
	if (!ErrorMessage.IsEmpty())
	{
		AddTokenizedErrorMessage(FTokenizedMessage::Create(Severity, FText::Format(FText::FromString("{0}"), FText::FromString(ErrorMessage))), NAME_None);
	}
}

void UnFbx::FFbxImporter::SetLogger(class FFbxLogger * InLogger)
{
	// this should be only called by top level functions
	// if you set it you can't set it again. Otherwise, you'll lose all log information
	check(Logger == NULL);
	Logger = InLogger;
}

// just in case if DeleteScene/CleanUp is getting called too late
void UnFbx::FFbxImporter::ClearLogger()
{
	Logger = NULL;
}

FFbxLogger::FFbxLogger()
{
}

FFbxLogger::~FFbxLogger()
{
	bool ShowLogMessage = !ShowLogMessageOnlyIfError;
	if (ShowLogMessageOnlyIfError)
	{
		for (TSharedRef<FTokenizedMessage> TokenMessage : TokenizedErrorMessages)
		{
			if (TokenMessage->GetSeverity() == EMessageSeverity::CriticalError || TokenMessage->GetSeverity() == EMessageSeverity::Error)
			{
				ShowLogMessage = true;
				break;
			}
		}
	}

	//Always clear the old message after an import or re-import
	const TCHAR* LogTitle = TEXT("FBXImport");
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	TSharedPtr<class IMessageLogListing> LogListing = MessageLogModule.GetLogListing(LogTitle);
	LogListing->SetLabel(FText::FromString("FBX Import"));
	LogListing->ClearMessages();

	if(TokenizedErrorMessages.Num() > 0)
	{
		LogListing->AddMessages(TokenizedErrorMessages);
		if (ShowLogMessage)
		{
			MessageLogModule.OpenMessageLog(LogTitle);
		}
	}
}
#undef LOCTEXT_NAMESPACE
