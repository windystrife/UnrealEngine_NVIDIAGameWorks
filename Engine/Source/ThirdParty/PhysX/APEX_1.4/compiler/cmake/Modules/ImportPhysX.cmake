# Import the PhysX library!
#  PHYSX_ROOT_DIR should be set
#  Importing these libraries into their proper configurations allows us to link to them correctly.

SET(IMPORTED_CONFIGURATIONS debug checked profile release)
SET(IMPORTED_CONFIGURATIONS "${IMPORTED_CONFIGURATIONS}" CACHE STRING
	"Reset config to what we need" 
	FORCE)

