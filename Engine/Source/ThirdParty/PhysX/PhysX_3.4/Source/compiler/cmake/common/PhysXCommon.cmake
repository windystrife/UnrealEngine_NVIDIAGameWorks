#
# Build PhysXCommon common
#

SET(PHYSX_COMMON_SOURCE
	${COMMON_SRC_DIR}/CmBoxPruning.cpp
	${COMMON_SRC_DIR}/CmCollection.cpp
	${COMMON_SRC_DIR}/CmMathUtils.cpp
	${COMMON_SRC_DIR}/CmPtrTable.cpp
	${COMMON_SRC_DIR}/CmRadixSort.cpp
	${COMMON_SRC_DIR}/CmRadixSortBuffered.cpp
	${COMMON_SRC_DIR}/CmRenderOutput.cpp
	${COMMON_SRC_DIR}/CmVisualization.cpp
	${COMMON_SRC_DIR}/CmBitMap.h
	${COMMON_SRC_DIR}/CmBoxPruning.h
	${COMMON_SRC_DIR}/CmCollection.h
	${COMMON_SRC_DIR}/CmConeLimitHelper.h
	${COMMON_SRC_DIR}/CmFlushPool.h
	${COMMON_SRC_DIR}/CmIDPool.h
	${COMMON_SRC_DIR}/CmIO.h
	${COMMON_SRC_DIR}/CmMatrix34.h
	${COMMON_SRC_DIR}/CmPhysXCommon.h
	${COMMON_SRC_DIR}/CmPool.h
	${COMMON_SRC_DIR}/CmPreallocatingPool.h
	${COMMON_SRC_DIR}/CmPriorityQueue.h
	${COMMON_SRC_DIR}/CmPtrTable.h
	${COMMON_SRC_DIR}/CmQueue.h
	${COMMON_SRC_DIR}/CmRadixSort.h
	${COMMON_SRC_DIR}/CmRadixSortBuffered.h
	${COMMON_SRC_DIR}/CmReaderWriterLock.h
	${COMMON_SRC_DIR}/CmRefCountable.h
	${COMMON_SRC_DIR}/CmRenderBuffer.h
	${COMMON_SRC_DIR}/CmRenderOutput.h
	${COMMON_SRC_DIR}/CmScaling.h
	${COMMON_SRC_DIR}/CmSpatialVector.h
	${COMMON_SRC_DIR}/CmTask.h
	${COMMON_SRC_DIR}/CmTaskPool.h
	${COMMON_SRC_DIR}/CmTmpMem.h
	${COMMON_SRC_DIR}/CmTransformUtils.h
	${COMMON_SRC_DIR}/CmUtils.h
	${COMMON_SRC_DIR}/CmVisualization.h
)
SOURCE_GROUP(common\\src FILES ${PHYSX_COMMON_SOURCE})

SET(PHYSXCOMMON_COMMON_HEADERS
	${PHYSX_ROOT_DIR}/Include/common/PxBase.h
	${PHYSX_ROOT_DIR}/Include/common/PxCollection.h
	${PHYSX_ROOT_DIR}/Include/common/PxCoreUtilityTypes.h
	${PHYSX_ROOT_DIR}/Include/common/PxMetaData.h
	${PHYSX_ROOT_DIR}/Include/common/PxMetaDataFlags.h
	${PHYSX_ROOT_DIR}/Include/common/PxPhysicsInsertionCallback.h
	${PHYSX_ROOT_DIR}/Include/common/PxPhysXCommonConfig.h
	${PHYSX_ROOT_DIR}/Include/common/PxRenderBuffer.h
	${PHYSX_ROOT_DIR}/Include/common/PxSerialFramework.h
	${PHYSX_ROOT_DIR}/Include/common/PxSerializer.h
	${PHYSX_ROOT_DIR}/Include/common/PxStringTable.h
	${PHYSX_ROOT_DIR}/Include/common/PxTolerancesScale.h
	${PHYSX_ROOT_DIR}/Include/common/PxTypeInfo.h
)
SOURCE_GROUP(include\\common FILES ${PHYSXCOMMON_COMMON_HEADERS})

SET(PHYSXCOMMON_GEOMETRY_HEADERS
	${PHYSX_ROOT_DIR}/Include/geometry/PxBoxGeometry.h
	${PHYSX_ROOT_DIR}/Include/geometry/PxCapsuleGeometry.h
	${PHYSX_ROOT_DIR}/Include/geometry/PxConvexMesh.h
	${PHYSX_ROOT_DIR}/Include/geometry/PxConvexMeshGeometry.h
	${PHYSX_ROOT_DIR}/Include/geometry/PxGeometry.h
	${PHYSX_ROOT_DIR}/Include/geometry/PxGeometryHelpers.h
	${PHYSX_ROOT_DIR}/Include/geometry/PxGeometryQuery.h
	${PHYSX_ROOT_DIR}/Include/geometry/PxHeightField.h
	${PHYSX_ROOT_DIR}/Include/geometry/PxHeightFieldDesc.h
	${PHYSX_ROOT_DIR}/Include/geometry/PxHeightFieldFlag.h
	${PHYSX_ROOT_DIR}/Include/geometry/PxHeightFieldGeometry.h
	${PHYSX_ROOT_DIR}/Include/geometry/PxHeightFieldSample.h
	${PHYSX_ROOT_DIR}/Include/geometry/PxMeshQuery.h
	${PHYSX_ROOT_DIR}/Include/geometry/PxMeshScale.h
	${PHYSX_ROOT_DIR}/Include/geometry/PxPlaneGeometry.h
	${PHYSX_ROOT_DIR}/Include/geometry/PxSimpleTriangleMesh.h
	${PHYSX_ROOT_DIR}/Include/geometry/PxSphereGeometry.h
	${PHYSX_ROOT_DIR}/Include/geometry/PxTriangle.h
	${PHYSX_ROOT_DIR}/Include/geometry/PxTriangleMesh.h
	${PHYSX_ROOT_DIR}/Include/geometry/PxTriangleMeshGeometry.h
)
SOURCE_GROUP(include\\geometry FILES ${PHYSXCOMMON_GEOMETRY_HEADERS})

SET(PHYSXCOMMON_GU_HEADERS
	${GU_SOURCE_DIR}/headers/GuAxes.h
	${GU_SOURCE_DIR}/headers/GuBox.h
	${GU_SOURCE_DIR}/headers/GuDistanceSegmentBox.h
	${GU_SOURCE_DIR}/headers/GuDistanceSegmentSegment.h
	${GU_SOURCE_DIR}/headers/GuIntersectionBoxBox.h
	${GU_SOURCE_DIR}/headers/GuIntersectionTriangleBox.h
	${GU_SOURCE_DIR}/headers/GuRaycastTests.h
	${GU_SOURCE_DIR}/headers/GuSegment.h
	${GU_SOURCE_DIR}/headers/GuSIMDHelpers.h
)
SOURCE_GROUP(geomutils\\headers FILES ${PHYSXCOMMON_GU_HEADERS})

SET(PHYSXCOMMON_GU_PXHEADERS
	${PHYSX_ROOT_DIR}/Include/GeomUtils/GuContactBuffer.h
	${PHYSX_ROOT_DIR}/Include/GeomUtils/GuContactPoint.h
)
SOURCE_GROUP(geomutils\\include FILES ${PHYSXCOMMON_GU_PXHEADERS})

SET(PHYSXCOMMON_GU_SOURCE
	${GU_SOURCE_DIR}/src/GuBounds.cpp
	${GU_SOURCE_DIR}/src/GuBox.cpp
	${GU_SOURCE_DIR}/src/GuCapsule.cpp
	${GU_SOURCE_DIR}/src/GuCCTSweepTests.cpp	
	${GU_SOURCE_DIR}/src/GuGeometryQuery.cpp
	${GU_SOURCE_DIR}/src/GuGeometryUnion.cpp
	${GU_SOURCE_DIR}/src/GuInternal.cpp
	${GU_SOURCE_DIR}/src/GuMeshFactory.cpp
	${GU_SOURCE_DIR}/src/GuMetaData.cpp
	${GU_SOURCE_DIR}/src/GuMTD.cpp
	${GU_SOURCE_DIR}/src/GuOverlapTests.cpp
	${GU_SOURCE_DIR}/src/GuRaycastTests.cpp
	${GU_SOURCE_DIR}/src/GuSerialize.cpp
	${GU_SOURCE_DIR}/src/GuSweepMTD.cpp
	${GU_SOURCE_DIR}/src/GuSweepSharedTests.cpp
	${GU_SOURCE_DIR}/src/GuSweepTests.cpp
	${GU_SOURCE_DIR}/src/GuBounds.h
	${GU_SOURCE_DIR}/src/GuCapsule.h
	${GU_SOURCE_DIR}/src/GuCenterExtents.h
	${GU_SOURCE_DIR}/src/GuGeometryUnion.h
	${GU_SOURCE_DIR}/src/GuInternal.h
	${GU_SOURCE_DIR}/src/GuMeshFactory.h
	${GU_SOURCE_DIR}/src/GuMTD.h
	${GU_SOURCE_DIR}/src/GuOverlapTests.h
	${GU_SOURCE_DIR}/src/GuSerialize.h
	${GU_SOURCE_DIR}/src/GuSphere.h
	${GU_SOURCE_DIR}/src/GuSweepMTD.h
	${GU_SOURCE_DIR}/src/GuSweepSharedTests.h
	${GU_SOURCE_DIR}/src/GuSweepTests.h
)
SOURCE_GROUP(geomutils\\src FILES ${PHYSXCOMMON_GU_SOURCE})

SET(PHYSXCOMMON_GU_CCD_SOURCE
	${GU_SOURCE_DIR}/src/ccd/GuCCDSweepConvexMesh.cpp
	${GU_SOURCE_DIR}/src/ccd/GuCCDSweepPrimitives.cpp
	${GU_SOURCE_DIR}/src/ccd/GuCCDSweepConvexMesh.h
)
SOURCE_GROUP(geomutils\\src\\ccd FILES ${PHYSXCOMMON_GU_CCD_SOURCE})

SET(PHYSXCOMMON_GU_COMMON_SOURCE
	${GU_SOURCE_DIR}/src/common/GuBarycentricCoordinates.cpp
	${GU_SOURCE_DIR}/src/common/GuSeparatingAxes.cpp
	${GU_SOURCE_DIR}/src/common/GuBarycentricCoordinates.h
	${GU_SOURCE_DIR}/src/common/GuBoxConversion.h
	${GU_SOURCE_DIR}/src/common/GuEdgeCache.h
	${GU_SOURCE_DIR}/src/common/GuEdgeListData.h
	${GU_SOURCE_DIR}/src/common/GuSeparatingAxes.h
)
SOURCE_GROUP(geomutils\\src\\common FILES ${PHYSXCOMMON_GU_COMMON_SOURCE})

SET(PHYSXCOMMON_GU_CONTACT_SOURCE
	${GU_SOURCE_DIR}/src/contact/GuContactBoxBox.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactCapsuleBox.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactCapsuleCapsule.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactCapsuleConvex.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactCapsuleMesh.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactConvexConvex.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactConvexMesh.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactPlaneBox.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactPlaneCapsule.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactPlaneConvex.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactPolygonPolygon.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactSphereBox.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactSphereCapsule.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactSphereMesh.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactSpherePlane.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactSphereSphere.cpp
	${GU_SOURCE_DIR}/src/contact/GuFeatureCode.cpp
	${GU_SOURCE_DIR}/src/contact/GuLegacyContactBoxHeightField.cpp
	${GU_SOURCE_DIR}/src/contact/GuLegacyContactCapsuleHeightField.cpp
	${GU_SOURCE_DIR}/src/contact/GuLegacyContactConvexHeightField.cpp
	${GU_SOURCE_DIR}/src/contact/GuLegacyContactSphereHeightField.cpp
	${GU_SOURCE_DIR}/src/contact/GuContactMethodImpl.h
	${GU_SOURCE_DIR}/src/contact/GuContactPolygonPolygon.h
	${GU_SOURCE_DIR}/src/contact/GuFeatureCode.h
	${GU_SOURCE_DIR}/src/contact/GuLegacyTraceLineCallback.h
)
SOURCE_GROUP(geomutils\\src\\contact FILES ${PHYSXCOMMON_GU_CONTACT_SOURCE})

SET(PHYSXCOMMON_GU_CONVEX_SOURCE
	${GU_SOURCE_DIR}/src/convex/GuBigConvexData.cpp
	${GU_SOURCE_DIR}/src/convex/GuConvexHelper.cpp
	${GU_SOURCE_DIR}/src/convex/GuConvexMesh.cpp
	${GU_SOURCE_DIR}/src/convex/GuConvexSupportTable.cpp
	${GU_SOURCE_DIR}/src/convex/GuConvexUtilsInternal.cpp
	${GU_SOURCE_DIR}/src/convex/GuHillClimbing.cpp
	${GU_SOURCE_DIR}/src/convex/GuShapeConvex.cpp
	${GU_SOURCE_DIR}/src/convex/GuBigConvexData.h
	${GU_SOURCE_DIR}/src/convex/GuBigConvexData2.h
	${GU_SOURCE_DIR}/src/convex/GuConvexEdgeFlags.h
	${GU_SOURCE_DIR}/src/convex/GuConvexHelper.h
	${GU_SOURCE_DIR}/src/convex/GuConvexMesh.h
	${GU_SOURCE_DIR}/src/convex/GuConvexMeshData.h
	${GU_SOURCE_DIR}/src/convex/GuConvexSupportTable.h
	${GU_SOURCE_DIR}/src/convex/GuConvexUtilsInternal.h
	${GU_SOURCE_DIR}/src/convex/GuCubeIndex.h
	${GU_SOURCE_DIR}/src/convex/GuHillClimbing.h
	${GU_SOURCE_DIR}/src/convex/GuShapeConvex.h
)
SOURCE_GROUP(geomutils\\src\\convex FILES ${PHYSXCOMMON_GU_CONVEX_SOURCE})

SET(PHYSXCOMMON_GU_DISTANCE_SOURCE
	${GU_SOURCE_DIR}/src/distance/GuDistancePointBox.cpp
	${GU_SOURCE_DIR}/src/distance/GuDistancePointTriangle.cpp
	${GU_SOURCE_DIR}/src/distance/GuDistanceSegmentBox.cpp
	${GU_SOURCE_DIR}/src/distance/GuDistanceSegmentSegment.cpp
	${GU_SOURCE_DIR}/src/distance/GuDistanceSegmentTriangle.cpp
	${GU_SOURCE_DIR}/src/distance/GuDistancePointBox.h
	${GU_SOURCE_DIR}/src/distance/GuDistancePointSegment.h
	${GU_SOURCE_DIR}/src/distance/GuDistancePointTriangle.h
	${GU_SOURCE_DIR}/src/distance/GuDistancePointTriangleSIMD.h
	${GU_SOURCE_DIR}/src/distance/GuDistanceSegmentSegmentSIMD.h
	${GU_SOURCE_DIR}/src/distance/GuDistanceSegmentTriangle.h
	${GU_SOURCE_DIR}/src/distance/GuDistanceSegmentTriangleSIMD.h
)
SOURCE_GROUP(geomutils\\src\\distance FILES ${PHYSXCOMMON_GU_DISTANCE_SOURCE})

SET(PHYSXCOMMON_GU_GJK_SOURCE
	${GU_SOURCE_DIR}/src/gjk/GuEPA.cpp
	${GU_SOURCE_DIR}/src/gjk/GuGJKSimplex.cpp
	${GU_SOURCE_DIR}/src/gjk/GuGJKTest.cpp
	${GU_SOURCE_DIR}/src/gjk/GuEPA.h
	${GU_SOURCE_DIR}/src/gjk/GuEPAFacet.h
	${GU_SOURCE_DIR}/src/gjk/GuGJK.h
	${GU_SOURCE_DIR}/src/gjk/GuGJKPenetration.h
	${GU_SOURCE_DIR}/src/gjk/GuGJKRaycast.h
	${GU_SOURCE_DIR}/src/gjk/GuGJKSimplex.h
	${GU_SOURCE_DIR}/src/gjk/GuGJKTest.h
	${GU_SOURCE_DIR}/src/gjk/GuGJKType.h
	${GU_SOURCE_DIR}/src/gjk/GuGJKUtil.h
	${GU_SOURCE_DIR}/src/gjk/GuVecBox.h
	${GU_SOURCE_DIR}/src/gjk/GuVecCapsule.h
	${GU_SOURCE_DIR}/src/gjk/GuVecConvex.h
	${GU_SOURCE_DIR}/src/gjk/GuVecConvexHull.h
	${GU_SOURCE_DIR}/src/gjk/GuVecConvexHullNoScale.h
	${GU_SOURCE_DIR}/src/gjk/GuVecPlane.h
	${GU_SOURCE_DIR}/src/gjk/GuVecShrunkBox.h
	${GU_SOURCE_DIR}/src/gjk/GuVecShrunkConvexHull.h
	${GU_SOURCE_DIR}/src/gjk/GuVecShrunkConvexHullNoScale.h
	${GU_SOURCE_DIR}/src/gjk/GuVecSphere.h
	${GU_SOURCE_DIR}/src/gjk/GuVecTriangle.h
)
SOURCE_GROUP(geomutils\\src\\gjk FILES ${PHYSXCOMMON_GU_GJK_SOURCE})

SET(PHYSXCOMMON_GU_HF_SOURCE
	${GU_SOURCE_DIR}/src/hf/GuHeightField.cpp
	${GU_SOURCE_DIR}/src/hf/GuHeightFieldUtil.cpp
	${GU_SOURCE_DIR}/src/hf/GuOverlapTestsHF.cpp
	${GU_SOURCE_DIR}/src/hf/GuSweepsHF.cpp
	${GU_SOURCE_DIR}/src/hf/GuEntityReport.h
	${GU_SOURCE_DIR}/src/hf/GuHeightField.h
	${GU_SOURCE_DIR}/src/hf/GuHeightFieldData.h
	${GU_SOURCE_DIR}/src/hf/GuHeightFieldUtil.h
)
SOURCE_GROUP(geomutils\\src\\hf FILES ${PHYSXCOMMON_GU_HF_SOURCE})

SET(PHYSXCOMMON_GU_INTERSECTION_SOURCE
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionBoxBox.cpp
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionCapsuleTriangle.cpp
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionEdgeEdge.cpp
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionRayBox.cpp
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionRayCapsule.cpp
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionRaySphere.cpp
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionSphereBox.cpp
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionTriangleBox.cpp
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionCapsuleTriangle.h
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionEdgeEdge.h
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionRay.h
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionRayBox.h
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionRayBoxSIMD.h
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionRayCapsule.h
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionRayPlane.h
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionRaySphere.h
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionRayTriangle.h
	${GU_SOURCE_DIR}/src/intersection/GuIntersectionSphereBox.h
)
SOURCE_GROUP(geomutils\\src\\intersection FILES ${PHYSXCOMMON_GU_INTERSECTION_SOURCE})

SET(PXCOMMON_BVH4_FILES
	${GU_SOURCE_DIR}/src/mesh/GuBV4_AABBSweep.cpp
	${GU_SOURCE_DIR}/src/mesh/GuBV4_BoxOverlap.cpp
	${GU_SOURCE_DIR}/src/mesh/GuBV4_CapsuleSweep.cpp
	${GU_SOURCE_DIR}/src/mesh/GuBV4_CapsuleSweepAA.cpp
	${GU_SOURCE_DIR}/src/mesh/GuBV4_OBBSweep.cpp
	${GU_SOURCE_DIR}/src/mesh/GuBV4_Raycast.cpp
	${GU_SOURCE_DIR}/src/mesh/GuBV4_SphereOverlap.cpp
	${GU_SOURCE_DIR}/src/mesh/GuBV4_SphereSweep.cpp
	${GU_SOURCE_DIR}/src/mesh/GuMidphaseBV4.cpp
)

SET(PHYSXCOMMON_GU_MESH_SOURCE
	${GU_SOURCE_DIR}/src/mesh/GuBV4.cpp
	${GU_SOURCE_DIR}/src/mesh/GuBV4Build.cpp

	${PXCOMMON_BVH4_FILES}

	${GU_SOURCE_DIR}/src/mesh/GuMeshQuery.cpp
	${GU_SOURCE_DIR}/src/mesh/GuMidphaseRTree.cpp
	${GU_SOURCE_DIR}/src/mesh/GuOverlapTestsMesh.cpp
	${GU_SOURCE_DIR}/src/mesh/GuRTree.cpp
	${GU_SOURCE_DIR}/src/mesh/GuRTreeQueries.cpp
	${GU_SOURCE_DIR}/src/mesh/GuSweepsMesh.cpp
	${GU_SOURCE_DIR}/src/mesh/GuTriangleMesh.cpp
	${GU_SOURCE_DIR}/src/mesh/GuTriangleMeshBV4.cpp
	${GU_SOURCE_DIR}/src/mesh/GuTriangleMeshRTree.cpp
	${GU_SOURCE_DIR}/src/mesh/GuBV32.cpp
	${GU_SOURCE_DIR}/src/mesh/GuBV32Build.cpp	
	${GU_SOURCE_DIR}/src/mesh/GuBV32.h
	${GU_SOURCE_DIR}/src/mesh/GuBV32Build.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4Build.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4Settings.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4_AABBAABBSweepTest.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4_BoxBoxOverlapTest.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4_BoxOverlap_Internal.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4_BoxSweep_Internal.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4_BoxSweep_Params.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4_CapsuleSweep_Internal.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4_Common.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4_Internal.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4_ProcessStreamNoOrder_OBBOBB.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4_ProcessStreamNoOrder_SegmentAABB.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4_ProcessStreamNoOrder_SegmentAABB_Inflated.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4_ProcessStreamNoOrder_SphereAABB.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4_ProcessStreamOrdered_OBBOBB.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4_ProcessStreamOrdered_SegmentAABB.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4_ProcessStreamOrdered_SegmentAABB_Inflated.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4_Slabs.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4_Slabs_KajiyaNoOrder.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4_Slabs_KajiyaOrdered.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4_Slabs_SwizzledNoOrder.h
	${GU_SOURCE_DIR}/src/mesh/GuBV4_Slabs_SwizzledOrdered.h
	${GU_SOURCE_DIR}/src/mesh/GuBVConstants.h
	${GU_SOURCE_DIR}/src/mesh/GuMeshData.h
	${GU_SOURCE_DIR}/src/mesh/GuMidphaseInterface.h
	${GU_SOURCE_DIR}/src/mesh/GuRTree.h
	${GU_SOURCE_DIR}/src/mesh/GuSweepConvexTri.h
	${GU_SOURCE_DIR}/src/mesh/GuSweepMesh.h
	${GU_SOURCE_DIR}/src/mesh/GuTriangle32.h
	${GU_SOURCE_DIR}/src/mesh/GuTriangleCache.h
	${GU_SOURCE_DIR}/src/mesh/GuTriangleMesh.h
	${GU_SOURCE_DIR}/src/mesh/GuTriangleMeshBV4.h
	${GU_SOURCE_DIR}/src/mesh/GuTriangleMeshRTree.h
	${GU_SOURCE_DIR}/src/mesh/GuTriangleVertexPointers.h
)
SOURCE_GROUP(geomutils\\src\\mesh FILES ${PHYSXCOMMON_GU_MESH_SOURCE})

SET(PHYSXCOMMON_GU_PCM_SOURCE
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactBoxBox.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactBoxConvex.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactCapsuleBox.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactCapsuleCapsule.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactCapsuleConvex.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactCapsuleHeightField.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactCapsuleMesh.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactConvexCommon.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactConvexConvex.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactConvexHeightField.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactConvexMesh.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactGenBoxConvex.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactGenSphereCapsule.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactPlaneBox.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactPlaneCapsule.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactPlaneConvex.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactSphereBox.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactSphereCapsule.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactSphereConvex.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactSphereHeightField.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactSphereMesh.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactSpherePlane.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactSphereSphere.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMShapeConvex.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMTriangleContactGen.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPersistentContactManifold.cpp
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactConvexCommon.h
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactGen.h
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactGenUtil.h
	${GU_SOURCE_DIR}/src/pcm/GuPCMContactMeshCallback.h
	${GU_SOURCE_DIR}/src/pcm/GuPCMShapeConvex.h
	${GU_SOURCE_DIR}/src/pcm/GuPCMTriangleContactGen.h
	${GU_SOURCE_DIR}/src/pcm/GuPersistentContactManifold.h
)
SOURCE_GROUP(geomutils\\src\\pcm FILES ${PHYSXCOMMON_GU_PCM_SOURCE})

SET(PHYSXCOMMON_GU_SWEEP_SOURCE
	${GU_SOURCE_DIR}/src/sweep/GuSweepBoxBox.cpp
	${GU_SOURCE_DIR}/src/sweep/GuSweepBoxSphere.cpp
	${GU_SOURCE_DIR}/src/sweep/GuSweepBoxTriangle_FeatureBased.cpp
	${GU_SOURCE_DIR}/src/sweep/GuSweepBoxTriangle_SAT.cpp
	${GU_SOURCE_DIR}/src/sweep/GuSweepCapsuleBox.cpp
	${GU_SOURCE_DIR}/src/sweep/GuSweepCapsuleCapsule.cpp
	${GU_SOURCE_DIR}/src/sweep/GuSweepCapsuleTriangle.cpp
	${GU_SOURCE_DIR}/src/sweep/GuSweepSphereCapsule.cpp
	${GU_SOURCE_DIR}/src/sweep/GuSweepSphereSphere.cpp
	${GU_SOURCE_DIR}/src/sweep/GuSweepSphereTriangle.cpp
	${GU_SOURCE_DIR}/src/sweep/GuSweepTriangleUtils.cpp
	${GU_SOURCE_DIR}/src/sweep/GuSweepBoxBox.h
	${GU_SOURCE_DIR}/src/sweep/GuSweepBoxSphere.h
	${GU_SOURCE_DIR}/src/sweep/GuSweepBoxTriangle_FeatureBased.h
	${GU_SOURCE_DIR}/src/sweep/GuSweepBoxTriangle_SAT.h
	${GU_SOURCE_DIR}/src/sweep/GuSweepCapsuleBox.h
	${GU_SOURCE_DIR}/src/sweep/GuSweepCapsuleCapsule.h
	${GU_SOURCE_DIR}/src/sweep/GuSweepCapsuleTriangle.h
	${GU_SOURCE_DIR}/src/sweep/GuSweepSphereCapsule.h
	${GU_SOURCE_DIR}/src/sweep/GuSweepSphereSphere.h
	${GU_SOURCE_DIR}/src/sweep/GuSweepSphereTriangle.h
	${GU_SOURCE_DIR}/src/sweep/GuSweepTriangleUtils.h
)
SOURCE_GROUP(geomutils\\src\\sweep FILES ${PHYSXCOMMON_GU_SWEEP_SOURCE})

ADD_LIBRARY(PhysXCommon ${PHYSXCOMMON_LIBTYPE} 
	${PHYSX_COMMON_SOURCE}
	
	${PHYSXCOMMON_COMMON_HEADERS}
	${PHYSXCOMMON_GEOMETRY_HEADERS}
	
	${PXCOMMON_PLATFORM_SRC_FILES}
	
	${PHYSXCOMMON_GU_HEADERS}
	${PHYSXCOMMON_GU_PXHEADERS}
	
	${PHYSXCOMMON_GU_SOURCE}
	${PHYSXCOMMON_GU_CCD_SOURCE}
	${PHYSXCOMMON_GU_COMMON_SOURCE}
	${PHYSXCOMMON_GU_CONTACT_SOURCE}
	${PHYSXCOMMON_GU_CONVEX_SOURCE}
	${PHYSXCOMMON_GU_DISTANCE_SOURCE}
	${PHYSXCOMMON_GU_GJK_SOURCE}
	${PHYSXCOMMON_GU_HF_SOURCE}
	${PHYSXCOMMON_GU_INTERSECTION_SOURCE}
	${PHYSXCOMMON_GU_MESH_SOURCE}
	${PHYSXCOMMON_GU_PCM_SOURCE}
	${PHYSXCOMMON_GU_SWEEP_SOURCE}
)

TARGET_INCLUDE_DIRECTORIES(PhysXCommon 
	PRIVATE ${PXCOMMON_PLATFORM_INCLUDES}

	PRIVATE ${PXSHARED_ROOT_DIR}/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/foundation/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/fastxml/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/pvd/include

	PUBLIC ${PHYSX_ROOT_DIR}/Include
	PUBLIC ${PHYSX_ROOT_DIR}/Include/common
	PUBLIC ${PHYSX_ROOT_DIR}/Include/geometry
	PUBLIC ${PHYSX_ROOT_DIR}/Include/GeomUtils

	PRIVATE ${PHYSX_SOURCE_DIR}/Common/include
	PRIVATE ${PHYSX_SOURCE_DIR}/Common/src
	
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXProfile/include
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXProfile/src
	
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/headers
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/contact
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/common
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/convex
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/distance
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/sweep
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/gjk
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/intersection
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/mesh
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/hf
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/pcm
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/ccd
	
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXGpu/include	
)

TARGET_COMPILE_DEFINITIONS(PhysXCommon 
	PRIVATE ${PXCOMMON_COMPILE_DEFS}
)

SET_TARGET_PROPERTIES(PhysXCommon PROPERTIES
	OUTPUT_NAME PhysX3Common
)

SET_TARGET_PROPERTIES(PhysXCommon PROPERTIES 
	COMPILE_PDB_NAME_DEBUG "PhysX3Common${CMAKE_DEBUG_POSTFIX}"
	COMPILE_PDB_NAME_CHECKED "PhysX3Common${CMAKE_CHECKED_POSTFIX}"
	COMPILE_PDB_NAME_PROFILE "PhysX3Common${CMAKE_PROFILE_POSTFIX}"
	COMPILE_PDB_NAME_RELEASE "PhysX3Common${CMAKE_RELEASE_POSTFIX}"
)
