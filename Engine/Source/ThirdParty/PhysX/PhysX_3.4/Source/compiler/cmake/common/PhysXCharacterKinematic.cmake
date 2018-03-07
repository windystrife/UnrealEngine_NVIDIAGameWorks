#
# Build PhysXCharacterKinematic common
#

SET(PHYSXCCT_HEADERS
	${PHYSX_ROOT_DIR}/Include/characterkinematic/PxBoxController.h
	${PHYSX_ROOT_DIR}/Include/characterkinematic/PxCapsuleController.h
	${PHYSX_ROOT_DIR}/Include/characterkinematic/PxCharacter.h
	${PHYSX_ROOT_DIR}/Include/characterkinematic/PxController.h
	${PHYSX_ROOT_DIR}/Include/characterkinematic/PxControllerBehavior.h
	${PHYSX_ROOT_DIR}/Include/characterkinematic/PxControllerManager.h
	${PHYSX_ROOT_DIR}/Include/characterkinematic/PxControllerObstacles.h
	${PHYSX_ROOT_DIR}/Include/characterkinematic/PxExtended.h
)
SOURCE_GROUP(include FILES ${PHYSXCCT_HEADERS})

SET(PHYSXCCT_SOURCE
	${LL_SOURCE_DIR}/CctBoxController.cpp
	${LL_SOURCE_DIR}/CctCapsuleController.cpp
	${LL_SOURCE_DIR}/CctCharacterController.cpp
	${LL_SOURCE_DIR}/CctCharacterControllerCallbacks.cpp
	${LL_SOURCE_DIR}/CctCharacterControllerManager.cpp
	${LL_SOURCE_DIR}/CctController.cpp
	${LL_SOURCE_DIR}/CctObstacleContext.cpp
	${LL_SOURCE_DIR}/CctSweptBox.cpp
	${LL_SOURCE_DIR}/CctSweptCapsule.cpp
	${LL_SOURCE_DIR}/CctSweptVolume.cpp
	${LL_SOURCE_DIR}/CctBoxController.h
	${LL_SOURCE_DIR}/CctCapsuleController.h
	${LL_SOURCE_DIR}/CctCharacterController.h
	${LL_SOURCE_DIR}/CctCharacterControllerManager.h
	${LL_SOURCE_DIR}/CctController.h
	${LL_SOURCE_DIR}/CctInternalStructs.h
	${LL_SOURCE_DIR}/CctObstacleContext.h
	${LL_SOURCE_DIR}/CctSweptBox.h
	${LL_SOURCE_DIR}/CctSweptCapsule.h
	${LL_SOURCE_DIR}/CctSweptVolume.h
	${LL_SOURCE_DIR}/CctUtils.h
)
SOURCE_GROUP(src FILES ${PHYSXCCT_SOURCE})

ADD_LIBRARY(PhysXCharacterKinematic ${PHYSXCHARACTERKINEMATIC_LIBTYPE}
	${PHYSXCCT_HEADERS}
	${PHYSXCCT_SOURCE}
)

TARGET_INCLUDE_DIRECTORIES(PhysXCharacterKinematic 

	PRIVATE ${PHYSXCHARACTERKINEMATICS_PLATFORM_INCLUDES}

	PRIVATE ${PXSHARED_ROOT_DIR}/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/foundation/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/fastxml/include
	PRIVATE ${PXSHARED_ROOT_DIR}/src/pvd/include

	PRIVATE ${PHYSX_ROOT_DIR}/Include
	PRIVATE ${PHYSX_ROOT_DIR}/Include/common
	PRIVATE ${PHYSX_ROOT_DIR}/Include/geometry
	PRIVATE ${PHYSX_ROOT_DIR}/Include/characterkinematic
	PRIVATE ${PHYSX_ROOT_DIR}/Include/extensions
	PRIVATE ${PHYSX_ROOT_DIR}/Include/GeomUtils
	
	PRIVATE ${PHYSX_SOURCE_DIR}/Common/include
	PRIVATE ${PHYSX_SOURCE_DIR}/Common/src
	
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/headers
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/contact
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/common
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/convex
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/distance
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/gjk
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/intersection
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/mesh
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/hf
	PRIVATE ${PHYSX_SOURCE_DIR}/GeomUtils/src/pcm
)

TARGET_COMPILE_DEFINITIONS(PhysXCharacterKinematic 

	# Common to all configurations
	PRIVATE ${PHYSXCHARACTERKINEMATICS_COMPILE_DEFS}
)

SET_TARGET_PROPERTIES(PhysXCharacterKinematic PROPERTIES
	OUTPUT_NAME PhysX3CharacterKinematic
)

SET_TARGET_PROPERTIES(PhysXCharacterKinematic PROPERTIES 
	COMPILE_PDB_NAME_DEBUG "PhysX3CharacterKinematic${CMAKE_DEBUG_POSTFIX}"
	COMPILE_PDB_NAME_CHECKED "PhysX3CharacterKinematic${CMAKE_CHECKED_POSTFIX}"
	COMPILE_PDB_NAME_PROFILE "PhysX3CharacterKinematic${CMAKE_PROFILE_POSTFIX}"
	COMPILE_PDB_NAME_RELEASE "PhysX3CharacterKinematic${CMAKE_RELEASE_POSTFIX}"
)