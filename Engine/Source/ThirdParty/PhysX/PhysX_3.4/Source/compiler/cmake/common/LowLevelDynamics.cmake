#
# Build LowLevelDynamics common
#

SET(LLDYNAMICS_BASE_DIR ${PHYSX_ROOT_DIR}/Source/LowLevelDynamics)
SET(LLDYNAMICS_INCLUDES		
	${LLDYNAMICS_BASE_DIR}/include/DyArticulation.h
	${LLDYNAMICS_BASE_DIR}/include/DyConstraint.h
	${LLDYNAMICS_BASE_DIR}/include/DyConstraintWriteBack.h
	${LLDYNAMICS_BASE_DIR}/include/DyContext.h
	${LLDYNAMICS_BASE_DIR}/include/DyGpuAPI.h
	${LLDYNAMICS_BASE_DIR}/include/DySleepingConfigulation.h
	${LLDYNAMICS_BASE_DIR}/include/DyThresholdTable.h
)
SOURCE_GROUP("Dynamics Includes" FILES ${LLDYNAMICS_INCLUDES})

SET(LLDYNAMICS_SOURCE		
	${LLDYNAMICS_BASE_DIR}/src/DyArticulation.cpp
	${LLDYNAMICS_BASE_DIR}/src/DyArticulationContactPrep.cpp
	${LLDYNAMICS_BASE_DIR}/src/DyArticulationContactPrepPF.cpp
	${LLDYNAMICS_BASE_DIR}/src/DyArticulationHelper.cpp
	${LLDYNAMICS_BASE_DIR}/src/DyArticulationScalar.cpp
	${LLDYNAMICS_BASE_DIR}/src/DyArticulationSIMD.cpp
	${LLDYNAMICS_BASE_DIR}/src/DyConstraintPartition.cpp
	${LLDYNAMICS_BASE_DIR}/src/DyConstraintSetup.cpp
	${LLDYNAMICS_BASE_DIR}/src/DyConstraintSetupBlock.cpp
	${LLDYNAMICS_BASE_DIR}/src/DyContactPrep.cpp
	${LLDYNAMICS_BASE_DIR}/src/DyContactPrep4.cpp
	${LLDYNAMICS_BASE_DIR}/src/DyContactPrep4PF.cpp
	${LLDYNAMICS_BASE_DIR}/src/DyContactPrepPF.cpp
	${LLDYNAMICS_BASE_DIR}/src/DyDynamics.cpp
	${LLDYNAMICS_BASE_DIR}/src/DyFrictionCorrelation.cpp
	${LLDYNAMICS_BASE_DIR}/src/DyRigidBodyToSolverBody.cpp
	${LLDYNAMICS_BASE_DIR}/src/DySolverConstraints.cpp
	${LLDYNAMICS_BASE_DIR}/src/DySolverConstraintsBlock.cpp
	${LLDYNAMICS_BASE_DIR}/src/DySolverControl.cpp
	${LLDYNAMICS_BASE_DIR}/src/DySolverControlPF.cpp
	${LLDYNAMICS_BASE_DIR}/src/DySolverPFConstraints.cpp
	${LLDYNAMICS_BASE_DIR}/src/DySolverPFConstraintsBlock.cpp
	${LLDYNAMICS_BASE_DIR}/src/DyThreadContext.cpp
	${LLDYNAMICS_BASE_DIR}/src/DyThresholdTable.cpp
)
SOURCE_GROUP("Dynamics Source" FILES ${LLDYNAMICS_SOURCE})

SET(LLDYNAMICS_INTERNAL_INCLUDES			
	${LLDYNAMICS_BASE_DIR}/src/DyArticulationContactPrep.h
	${LLDYNAMICS_BASE_DIR}/src/DyArticulationFnsDebug.h
	${LLDYNAMICS_BASE_DIR}/src/DyArticulationFnsScalar.h
	${LLDYNAMICS_BASE_DIR}/src/DyArticulationFnsSimd.h
	${LLDYNAMICS_BASE_DIR}/src/DyArticulationHelper.h
	${LLDYNAMICS_BASE_DIR}/src/DyArticulationPImpl.h
	${LLDYNAMICS_BASE_DIR}/src/DyArticulationReference.h
	${LLDYNAMICS_BASE_DIR}/src/DyArticulationScalar.h
	${LLDYNAMICS_BASE_DIR}/src/DyArticulationUtils.h
	${LLDYNAMICS_BASE_DIR}/src/DyBodyCoreIntegrator.h
	${LLDYNAMICS_BASE_DIR}/src/DyConstraintPartition.h
	${LLDYNAMICS_BASE_DIR}/src/DyConstraintPrep.h
	${LLDYNAMICS_BASE_DIR}/src/DyContactPrep.h
	${LLDYNAMICS_BASE_DIR}/src/DyContactPrepShared.h
	${LLDYNAMICS_BASE_DIR}/src/DyContactReduction.h
	${LLDYNAMICS_BASE_DIR}/src/DyCorrelationBuffer.h
	${LLDYNAMICS_BASE_DIR}/src/DyDynamics.h
	${LLDYNAMICS_BASE_DIR}/src/DyFrictionPatch.h
	${LLDYNAMICS_BASE_DIR}/src/DyFrictionPatchStreamPair.h
	${LLDYNAMICS_BASE_DIR}/src/DySolverBody.h
	${LLDYNAMICS_BASE_DIR}/src/DySolverConstraint1D.h
	${LLDYNAMICS_BASE_DIR}/src/DySolverConstraint1D4.h
	${LLDYNAMICS_BASE_DIR}/src/DySolverConstraintDesc.h
	${LLDYNAMICS_BASE_DIR}/src/DySolverConstraintExtShared.h
	${LLDYNAMICS_BASE_DIR}/src/DySolverConstraintsShared.h
	${LLDYNAMICS_BASE_DIR}/src/DySolverConstraintTypes.h
	${LLDYNAMICS_BASE_DIR}/src/DySolverContact.h
	${LLDYNAMICS_BASE_DIR}/src/DySolverContact4.h
	${LLDYNAMICS_BASE_DIR}/src/DySolverContactPF.h
	${LLDYNAMICS_BASE_DIR}/src/DySolverContactPF4.h
	${LLDYNAMICS_BASE_DIR}/src/DySolverContext.h
	${LLDYNAMICS_BASE_DIR}/src/DySolverControl.h
	${LLDYNAMICS_BASE_DIR}/src/DySolverControlPF.h
	${LLDYNAMICS_BASE_DIR}/src/DySolverCore.h
	${LLDYNAMICS_BASE_DIR}/src/DySolverExt.h
	${LLDYNAMICS_BASE_DIR}/src/DySpatial.h
	${LLDYNAMICS_BASE_DIR}/src/DyThreadContext.h
)
SOURCE_GROUP("Dynamics Internal Includes" FILES ${LLDYNAMICS_INTERNAL_INCLUDES})

ADD_LIBRARY(LowLevelDynamics ${LOWLEVELDYNAMICS_LIBTYPE}
	${LLDYNAMICS_INCLUDES}
	${LLDYNAMICS_SOURCE}
	${LLDYNAMICS_INTERNAL_INCLUDES}
)

TARGET_INCLUDE_DIRECTORIES(LowLevelDynamics 
	PRIVATE ${LOWLEVELDYNAMICS_PLATFORM_INCLUDES}
	PRIVATE ${PXSHARED_ROOT_DIR}/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/foundation/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/fastxml/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/pvd/include

	PRIVATE ${PHYSX_ROOT_DIR}/Include
	PRIVATE ${PHYSX_ROOT_DIR}/Include/common
	PRIVATE ${PHYSX_ROOT_DIR}/Include/geometry
	PRIVATE ${PHYSX_ROOT_DIR}/Include/GeomUtils

	PRIVATE ${PHYSX_SOURCE_DIR}/Common/src
	
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXProfile/include
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXProfile/src
	
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/contact
	
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/API/include
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/common/include
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/common/include/pipeline
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/common/include/utils
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevel/software/include

	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevelDynamics/include
	PRIVATE ${PHYSX_SOURCE_DIR}/LowLevelDynamics/src
	
	PRIVATE ${PHYSX_SOURCE_DIR}/PhysXGpu/include
)

# Use generator expressions to set config specific preprocessor definitions
TARGET_COMPILE_DEFINITIONS(LowLevelDynamics 

	# Common to all configurations
	PRIVATE ${LOWLEVELDYNAMICS_COMPILE_DEFS}
)

IF(NOT ${LOWLEVELDYNAMICS_LIBTYPE} STREQUAL "OBJECT")
	SET_TARGET_PROPERTIES(LowLevelDynamics PROPERTIES 
		COMPILE_PDB_NAME_DEBUG "LowLevelDynamics${CMAKE_DEBUG_POSTFIX}"
		COMPILE_PDB_NAME_CHECKED "LowLevelDynamics${CMAKE_CHECKED_POSTFIX}"
		COMPILE_PDB_NAME_PROFILE "LowLevelDynamics${CMAKE_PROFILE_POSTFIX}"
		COMPILE_PDB_NAME_RELEASE "LowLevelDynamics${CMAKE_RELEASE_POSTFIX}"
	)
ENDIF()