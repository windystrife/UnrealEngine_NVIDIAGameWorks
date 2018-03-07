/*
 * Copyright (c) 2014-2017, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#pragma once

#include "NvFlowContext.h"
#include "NvFlowShader.h"

// -------------------------- NvFlowGrid -------------------------------
///@defgroup NvFlowGrid
///@{

//! An adaptive sparse grid
struct NvFlowGrid;

//! Interface to expose read access to grid simulation data
struct NvFlowGridExport;

//! Grid texture channel, four components per channel
enum NvFlowGridTextureChannel
{
	eNvFlowGridTextureChannelVelocity = 0,
	eNvFlowGridTextureChannelDensity = 1,
	eNvFlowGridTextureChannelDensityCoarse = 2,

	eNvFlowGridTextureChannelCount
};

//! Enumeration used to describe density texture channel resolution relative to velocity resolution
enum NvFlowMultiRes
{
	eNvFlowMultiRes1x1x1 = 0,
	eNvFlowMultiRes2x2x2 = 1
};

//! Description required to create a grid
struct NvFlowGridDesc
{
	NvFlowFloat3 initialLocation;		//!< Initial location of axis aligned bounding box
	NvFlowFloat3 halfSize;				//!< Initial half size of axis aligned bounding box

	NvFlowDim virtualDim;				//!< Resolution of virtual address space inside of bounding box
	NvFlowMultiRes densityMultiRes;		//!< Number of density cells per velocity cell

	float residentScale;				//!< Fraction of virtual cells to allocate memory for
	float coarseResidentScaleFactor;	//!< Allows relative increase of resident scale for coarse sparse textures

	bool enableVTR;						//!< Enable use of volume tiled resources, if supported
	bool lowLatencyMapping;				//!< Faster mapping updates, more mapping overhead but less prediction required
};

/**
 * Allows the application to request a default grid description from Flow.
 *
 * @param[out] desc The description for Flow to fill out.
 */
NV_FLOW_API void NvFlowGridDescDefaults(NvFlowGridDesc* desc);

/**
 * Creates a grid.
 *
 * @param[in] context The context to use to create the new grid.
 * @param[in] desc The grid description.
 *
 * @return The created grid.
 */
NV_FLOW_API NvFlowGrid* NvFlowCreateGrid(NvFlowContext* context, const NvFlowGridDesc* desc);

/**
 * Releases a grid.
 *
 * @param[in] grid The grid to be released.
 */
NV_FLOW_API void NvFlowReleaseGrid(NvFlowGrid* grid);

//! Description required to reset a NvFlowGrid
struct NvFlowGridResetDesc
{
	NvFlowFloat3 initialLocation;		//!< Initial location of axis aligned bounding box
	NvFlowFloat3 halfSize;				//!< Initial half size of axis aligned bounding box
};

/**
 * Allows the application to request a default grid reset description from Flow.
 *
 * @param[out] desc The description for Flow to fill out.
 */
NV_FLOW_API void NvFlowGridResetDescDefaults(NvFlowGridResetDesc* desc);

/**
 * Submits a request to reset a grid, preserving memory allocations
 *
 * @param[in] grid The grid to reset.
 * @param[in] desc The grid reset description.
 */
NV_FLOW_API void NvFlowGridReset(NvFlowGrid* grid, const NvFlowGridResetDesc* desc);

/**
 * Allows the application to request the grid move to a new location.
 *
 * @param[in] grid The grid to move.
 * @param[in] targetLocation The location the center of the grid should make a best effort attempt to reach.
 */
NV_FLOW_API void NvFlowGridSetTargetLocation(NvFlowGrid* grid, NvFlowFloat3 targetLocation);

//! Flags to control grid debug visualization
enum NvFlowGridDebugVisFlags
{
	eNvFlowGridDebugVisDisabled = 0x00,		//!< No debug visualization
	eNvFlowGridDebugVisBlocks = 0x01,		//!< Simulation block visualization, no overhead
	eNvFlowGridDebugVisEmitBounds = 0x02,	//!< Emitter bounds visualization, adds overhead
	eNvFlowGridDebugVisShapesSimple = 0x04, //!< Visualize sphere/capsule/box shapes, adds overhead

	eNvFlowGridDebugVisCount
};

//! Parameters controlling grid behavior
struct NvFlowGridParams
{
	NvFlowFloat3 gravity;					//!< Gravity vector for use by buoyancy

	bool singlePassAdvection;				//!< If true, enables single pass advection

	bool pressureLegacyMode;				//!< If true, run older less accurate pressure solver

	bool bigEffectMode;						//!< Tweaks block allocation for better big effect behavior
	float bigEffectPredictTime;				//!< Time constant to tune big effect prediction

	NvFlowGridDebugVisFlags debugVisFlags;	//!< Flags to control what debug visualization information is generated
};

/**
 * Allows the application to request default grid parameters from Flow.
 *
 * @param[out] params The parameters for Flow to fill out.
 */
NV_FLOW_API void NvFlowGridParamsDefaults(NvFlowGridParams* params);

/**
 * Sets grid simulation parameters, persistent over multiple grid updates.
 *
 * @param[in] grid The grid to set parameters on.
 * @param[in] params The new parameter values.
 */
NV_FLOW_API void NvFlowGridSetParams(NvFlowGrid* grid, const NvFlowGridParams* params);

//! Description of feature support on the queried Flow context GPU.
struct NvFlowSupport
{
	bool supportsVTR;		//!< True if volume tiled resources are supported
};

/**
 * Queries support for features that depend on hardware/OS.
 *
 * @param[in] grid The grid to query for support.
 * @param[in] context The context the grid was created against.
 * @param[out] support Description of what is supported.
 *
 * @return Returns eNvFlowSuccess if information is available.
 */
NV_FLOW_API NvFlowResult NvFlowGridQuerySupport(NvFlowGrid* grid, NvFlowContext* context, NvFlowSupport* support);

//! CPU/GPU timing info
struct NvFlowQueryTime
{
	float simulation;
};

/**
 * Queries simulation timing data.
 *
 * @param[in] grid The grid to query for timing.
 * @param[out] gpuTime Simulation overhead on GPU.
 * @param[out] cpuTime Simulation overhead on CPU.
 *
 * @return Returns eNvFlowSuccess if information is available.
 */
NV_FLOW_API NvFlowResult NvFlowGridQueryTime(NvFlowGrid* grid, NvFlowQueryTime* gpuTime, NvFlowQueryTime* cpuTime);

/**
 * Queries simulation GPU memory usage.
 *
 * @param[in] grid The grid to query for memory usage.
 * @param[out] numBytes GPU memory allocated in bytes.
 */
NV_FLOW_API void NvFlowGridGPUMemUsage(NvFlowGrid* grid, NvFlowUint64* numBytes);

/**
 * Steps the simulation dt forward in time.
 *
 * @param[in] grid The grid to update.
 * @param[in] context The context to perform the update.
 * @param[in] dt The time step, typically in seconds.
 */
NV_FLOW_API void NvFlowGridUpdate(NvFlowGrid* grid, NvFlowContext* context, float dt);

/**
 * Get read interface to the grid simulation results
 *
 * @param[in] context The context the grid was created with.
 * @param[in] grid The grid to read.
 *
 * @return Returns gridExport interface.
 */
NV_FLOW_API NvFlowGridExport* NvFlowGridGetGridExport(NvFlowContext* context, NvFlowGrid* grid);

///@}
// -------------------------- NvFlowGridMaterial -------------------------------
///@defgroup NvFlowGridMaterial
///@{

//! Handle provided by grid to reference materials
struct NvFlowGridMaterialHandle
{
	NvFlowGrid* grid;		//!< The grid that created this material handle
	NvFlowUint64 uid;		//!< Unique id for this material
};

//! Grid component IDs
enum NvFlowGridComponent
{
	eNvFlowGridComponentVelocity = 0,
	eNvFlowGridComponentSmoke = 1,
	eNvFlowGridComponentTemperature = 2,
	eNvFlowGridComponentFuel = 3,

	eNvFlowGridNumComponents = 4
};

//! Grid material per component parameters
struct NvFlowGridMaterialPerComponent
{
	float damping;						//!< Higher values reduce component value faster (exponential decay curve)
	float fade;							//!< Fade component value rate in units / sec
	float macCormackBlendFactor;		//!< Higher values make a sharper appearance, but with more artifacts
	float macCormackBlendThreshold;		//!< Minimum absolute value to apply MacCormack correction. Increasing can improve performance
	float allocWeight;					//!< Relative importance of component value for allocation, 0.0 means not important
	float allocThreshold;				//!< Minimum component value magnitude that is considered relevant
};

//! Grid material parameters
struct NvFlowGridMaterialParams
{
	NvFlowGridMaterialPerComponent velocity;	//!< Velocity component parameters
	NvFlowGridMaterialPerComponent smoke;		//!< Smoke component parameters
	NvFlowGridMaterialPerComponent temperature;	//!< Temperature component parameters
	NvFlowGridMaterialPerComponent fuel;		//!< Fuel component parameters

	float vorticityStrength;					//!< Higher values increase rotation, reduce laminar flow
	float vorticityVelocityMask;				//!< 0.f disabled; 1.0f higher velocities, higher strength; -1.0f for inverse
	float vorticityTemperatureMask;				//!< 0.f disabled; 1.0f higher temperatures, higher strength; -1.0f for inverse
	float vorticitySmokeMask;					//!< 0.f disabled; 1.0f higher smoke, higher strength; -1.0f for inverse
	float vorticityFuelMask;					//!< 0.f disabled; 1.0f higher fuel, higher strength; -1.0f for inverse
	float vorticityConstantMask;				//!< Works as other masks, provides fixed offset

	float ignitionTemp;							//!< Minimum temperature for combustion
	float burnPerTemp;							//!< Burn amount per unit temperature above ignitionTemp
	float fuelPerBurn;							//!< Fuel consumed per unit burn
	float tempPerBurn;							//!< Temperature increase per unit burn
	float smokePerBurn;							//!< Smoke increase per unit burn
	float divergencePerBurn;					//!< Expansion per unit burn
	float buoyancyPerTemp;						//!< Buoyant force per unit temperature
	float coolingRate;							//!< Cooling rate, exponential
};

/**
 * Allows the application to request default grid material parameters from Flow.
 *
 * @param[out] params The parameters for Flow to fill out.
 */
NV_FLOW_API void NvFlowGridMaterialParamsDefaults(NvFlowGridMaterialParams* params);

/**
 * Gets a handle to the default grid material.
 *
 * @param[in] grid The grid to return its default grid material.
 *
 * @return Returns default grid material for grid.
 */
NV_FLOW_API NvFlowGridMaterialHandle NvFlowGridGetDefaultMaterial(NvFlowGrid* grid);

/**
 * Creates new grid material, initializes to params.
 *
 * @param[in] grid The Flow grid to set parameters on.
 * @param[in] params The new parameter values.
 *
 * @return Returns handle to newly create grid material.
 */
NV_FLOW_API NvFlowGridMaterialHandle NvFlowGridCreateMaterial(NvFlowGrid* grid, const NvFlowGridMaterialParams* params);

/**
 * Release grid material
 *
 * @param[in] grid The grid to set parameters on.
 * @param[in] material Handle to material to release.
 */
NV_FLOW_API void NvFlowGridReleaseMaterial(NvFlowGrid* grid, NvFlowGridMaterialHandle material);

/**
 * Sets material parameters, persistent over multiple grid updates.
 *
 * @param[in] grid The grid to set parameters on.
 * @param[in] material Handle to material to update.
 * @param[in] params The new parameter values.
 */
NV_FLOW_API void NvFlowGridSetMaterialParams(NvFlowGrid* grid, NvFlowGridMaterialHandle material, const NvFlowGridMaterialParams* params);

///@}
// -------------------------- NvFlowShape -------------------------------
///@defgroup NvFlowShape
///@{

//! Types of shapes for emit/collide behavior
enum NvFlowShapeType
{
	eNvFlowShapeTypeSDF = 0,
	eNvFlowShapeTypeSphere = 1,
	eNvFlowShapeTypeBox = 2,
	eNvFlowShapeTypeCapsule = 3,
	eNvFlowShapeTypePlane = 4
};

//! A signed distance field shape object for emitters and/or collision
struct NvFlowShapeSDF;

//! Description of a signed distance field shape
struct NvFlowShapeDescSDF
{
	NvFlowUint sdfOffset;	//!< Offset in number of SDFs
};

//! Desription of a sphere
struct NvFlowShapeDescSphere
{
	float radius;			//!< Radius in local space
};

//! Description of a box
struct NvFlowShapeDescBox
{
	NvFlowFloat3 halfSize;	//!< HalfSize in local space
};

//! Description of a capsule
struct NvFlowShapeDescCapsule
{
	float radius;			//!< Radius in local space
	float length;			//!< Length in local space on x axis
};

//! Description of a plane
struct NvFlowShapeDescPlane
{
	NvFlowFloat3 normal;	//!< Normal vector of the plane in local space
	float distance;			//!< Shortest signed distance from the origin to the plane in local space
};

//! Shared type for shape descriptions
union NvFlowShapeDesc
{
	NvFlowShapeDescSDF sdf;
	NvFlowShapeDescSphere sphere;
	NvFlowShapeDescBox box;
	NvFlowShapeDescCapsule capsule;
	NvFlowShapeDescPlane plane;
};

//! Description required to create a signed distance field object.
struct NvFlowShapeSDFDesc
{
	NvFlowDim resolution;		//!< The resolution of the 3D texture used to store the signed distance field.
};

/**
 * Allows the application to request a default signed distance field object description from Flow.
 *
 * @param[out] desc The description for Flow to fill out.
 */
NV_FLOW_API void NvFlowShapeSDFDescDefaults(NvFlowShapeSDFDesc* desc);

//! Required information for writing to a CPU mapped signed distance field.
struct NvFlowShapeSDFData
{
	float* data;				//!< Pointer to mapped data
	NvFlowUint rowPitch;		//!< Row pitch in floats
	NvFlowUint depthPitch;		//!< Depth pitch in floats
	NvFlowDim dim;				//!< Dimension of the sdf texture
};

/**
 * Creates a signed distance field object with no initial data.
 *
 * @param[in] context The context to use for creation.
 * @param[in] desc A description needed for memory allocation.
 *
 * @return The created signed distance field object.
 */
NV_FLOW_API NvFlowShapeSDF* NvFlowCreateShapeSDF(NvFlowContext* context, const NvFlowShapeSDFDesc* desc);

/**
 * Creates a signed distance field object with data from a 3D texture.
 *
 * @param[in] context The context to use for creation.
 * @param[in] texture The 3D texture containing the signed distance field to use.
 *
 * @return The created signed distance field object.
 */
NV_FLOW_API NvFlowShapeSDF* NvFlowCreateShapeSDFFromTexture3D(NvFlowContext* context, NvFlowTexture3D* texture);

/**
 * Releases a signed distance field object.
 *
 * @param[in] shape The signed distance field to be released.
 */
NV_FLOW_API void NvFlowReleaseShapeSDF(NvFlowShapeSDF* shape);

/**
 * Maps a signed distance field object for CPU write access.
 *
 * @param[in] shape The signed distance field object to map.
 * @param[in] context The context used to create the Flow signed distance field.
 *
 * @return Returns the information needed to properly write to the mapped signed distance field object.
 */
NV_FLOW_API NvFlowShapeSDFData NvFlowShapeSDFMap(NvFlowShapeSDF* shape, NvFlowContext* context);

/**
 * Unmaps a signed distance field object from CPU write access, uploads update field to GPU.
 *
 * @param[in] shape The signed distance field object to unmap.
 * @param[in] context The context used to create the Flow signed distance field.
 */
NV_FLOW_API void NvFlowShapeSDFUnmap(NvFlowShapeSDF* shape, NvFlowContext* context);

///@}
// -------------------------- NvFlowGridEmit -------------------------------
///@defgroup NvFlowGridEmit
///@{

//! Emitter modes
enum NvFlowGridEmitMode
{
	eNvFlowGridEmitModeDefault = 0,					//!< Emitter will influence velocity and density channels, optionally allocate based on bounds
	eNvFlowGridEmitModeDisableVelocity = 0x01,		//!< Flag to disable emitter interaction with velocity field
	eNvFlowGridEmitModeDisableDensity = 0x02,		//!< Flag to disable emitter interaction with density field
	eNvFlowGridEmitModeDisableAlloc = 0x04,			//!< Flag to disable emitter bound allocation
	eNvFlowGridEmitModeAllocShape = 0x08,			//!< Emitter will allocate using shape to drive allocation instead of only bounds

	eNvFlowGridEmitModeAllocShapeOnly = 0x0F,		//!< Flags to configure for shape aware allocation only
};

//! Parameters for both emission and collision
struct NvFlowGridEmitParams
{
	NvFlowUint shapeRangeOffset;					//!< Start of shape range, offset in number of Shapes
	NvFlowUint shapeRangeSize;						//!< Size of shape range, in number of Shapes
	NvFlowShapeType shapeType;						//!< Type of shape in the set
	float shapeDistScale;							//!< Scale to apply to SDF value

	NvFlowFloat4x4 bounds;							//!< Transform from emitter ndc to world space
	NvFlowFloat4x4 localToWorld;					//!< Transform from shape local space to world space
	NvFlowFloat3 centerOfMass; 						//!< Center of mass in emitter bounds coordinate space

	float deltaTime;								//!< DeltaTime used to compute impulse

	NvFlowUint emitMaterialIndex;					//!< Index into material lookup defined by NvFlowGridUpdateEmitMaterials()
	NvFlowUint emitMode;							//!< Emitter behavior, based on NvFlowGridEmitMode, 0u is default

	NvFlowFloat3 allocationScale;					//!< Higher values cause more blocks to allocate around emitter; 0.f means no allocation, 1.f is default
	float allocationPredict;						//!< Higher values cause extra allocation based on linear velocity and predict velocity
	NvFlowFloat3 predictVelocity;					//!< Velocity used only for predict
	float predictVelocityWeight;					//!< Blend weight between linearVelocity and predictVelocity

	float minActiveDist;							//!< Minimum distance value for active emission
	float maxActiveDist;							//!< Maximum distance value for active emission
	float minEdgeDist;								//!< Distance from minActiveDist to 1.0 emitter opacity
	float maxEdgeDist;								//!< Distance before maxActiveDist to 0.0 emitter opacity
	float slipThickness;							//!< Thickness of slip boundary region
	float slipFactor;								//!< 0.0 => no slip, fully damped; 1.0 => full slip

	NvFlowFloat3 velocityLinear;					//!< Linear velocity, in world units, emitter direction
	NvFlowFloat3 velocityAngular;					//!< Angular velocity, in world units, emitter direction
	NvFlowFloat3 velocityCoupleRate;				//!< Rate of correction to target, inf means instantaneous

	float smoke;									//!< Target smoke
	float smokeCoupleRate;							//!< Rate of correction to target, inf means instantaneous

	float temperature;								//!< Target temperature
	float temperatureCoupleRate;					//!< Rate of correction to target, inf means instantaneous

	float fuel;										//!< Target fuel
	float fuelCoupleRate;							//!< Rate of correction to target, inf means instantaneous
	float fuelReleaseTemp;							//!< Minimum temperature to release fuelRelease additional fuel
	float fuelRelease;								//!< Fuel released when temperature exceeds release temperature
};

/**
 * Allows the application to request default emit parameters from Flow.
 *
 * @param[out] params The parameters for Flow to fill out.
 */
NV_FLOW_API void NvFlowGridEmitParamsDefaults(NvFlowGridEmitParams* params);

/**
 * Adds one or more emit events to be applied with the next grid update.
 *
 * @param[in] grid The Flow grid to apply the emit events.
 * @param[in] shapes Array of shape data referenced by emit params.
 * @param[in] numShapes Number of shapes in the array.
 * @param[in] params Array of emit event parameters.
 * @param[in] numParams Number of emit events in the array.
 */
NV_FLOW_API void NvFlowGridEmit(NvFlowGrid* grid, const NvFlowShapeDesc* shapes, NvFlowUint numShapes, const NvFlowGridEmitParams* params, NvFlowUint numParams);

/**
 * Update internal array of grid materials reference by emitMaterialIndex
 *
 * @param[in] grid The Flow grid to apply the emit events.
 * @param[in] materials Array of grid materials.
 * @param[in] numMaterials Number of grid materials in the array.
 */
NV_FLOW_API void NvFlowGridUpdateEmitMaterials(NvFlowGrid* grid, NvFlowGridMaterialHandle* materials, NvFlowUint numMaterials);

/**
 * Update internal array of SDFs that can be referenced by sdfOffset 
 *
 * @param[in] grid The Flow grid to apply the emit events.
 * @param[in] sdfs Array of shape data referenced by emit params.
 * @param[in] numSdfs Number of shapes in the array.
 */
NV_FLOW_API void NvFlowGridUpdateEmitSDFs(NvFlowGrid* grid, NvFlowShapeSDF** sdfs, NvFlowUint numSdfs);

///@}
// -------------------------- NvFlowGridEmitCustom -------------------------------
///@defgroup NvFlowGridEmitCustom
///@{

//! Necessary parameters/resources for custom grid block allocation
struct NvFlowGridEmitCustomAllocParams
{
	NvFlowResourceRW* maskResourceRW;	//!< Integer mask, write 1u where allocation is desired

	NvFlowDim maskDim;					//!< Mask dimensions

	NvFlowFloat3 gridLocation;			//!< Location of grid's axis aligned bounding box
	NvFlowFloat3 gridHalfSize;			//!< Half size of grid's axis aligned bounding box

	NvFlowGridMaterialHandle material;	//!< Grid material
};

typedef void(*NvFlowGridEmitCustomAllocFunc)(void* userdata, const NvFlowGridEmitCustomAllocParams* params);

/**
 * Sets custom allocation callback.
 *
 * @param[in] grid The grid to use the callback.
 * @param[in] func The callback function.
 * @param[in] userdata Pointer to provide to the callback function during execution.
 */
NV_FLOW_API void NvFlowGridEmitCustomRegisterAllocFunc(NvFlowGrid* grid, NvFlowGridEmitCustomAllocFunc func, void* userdata);

//! Handle for requesting per layer emitter data
struct NvFlowGridEmitCustomEmitParams
{
	NvFlowGrid* grid;			//!< The grid associated with this callback
	NvFlowUint numLayers;		//!< The number of layers to write to
	void* flowInternal;			//!< For Flow internal use, do not modify
};

//! Necessary parameters/resources for custom emit operations
struct NvFlowGridEmitCustomEmitLayerParams
{
	NvFlowResourceRW* dataRW[2u];				//!< Read/Write 3D textures for channel data
	NvFlowResource* blockTable;					//!< Table to map virtual blocks to real blocks
	NvFlowResource* blockList;					//!< List of active blocks

	NvFlowShaderPointParams shaderParams;		//!< Parameters used in GPU side operations

	NvFlowUint numBlocks;						//!< Number of active blocks
	NvFlowUint maxBlocks;						//!< Maximum possible active blocks

	NvFlowFloat3 gridLocation;					//!< Location of grid's axis aligned bounding box
	NvFlowFloat3 gridHalfSize;					//!< Half size of grid's axis aligned bounding box

	NvFlowGridMaterialHandle material;			//!< Grid material
};

typedef void(*NvFlowGridEmitCustomEmitFunc)(void* userdata, NvFlowUint* dataFrontIdx, const NvFlowGridEmitCustomEmitParams* params);

/**
 * Sets custom emit callback for given simulation channel.
 *
 * @param[in] grid The Flow grid to use the callback.
 * @param[in] channel The simulation channel for this callback.
 * @param[in] func The callback function.
 * @param[in] userdata Pointer to provide to the callback function during execution.
 */
NV_FLOW_API void NvFlowGridEmitCustomRegisterEmitFunc(NvFlowGrid* grid, NvFlowGridTextureChannel channel, NvFlowGridEmitCustomEmitFunc func, void* userdata);

/**
 * Get per layer custom emit parameters, should only be called inside the custom emit callback.
 *
 * @param[in] emitParams The custom emit parameters.
 * @param[in] layerIdx The layerIdx to fetch, should be least than emitParams->numLayers.
 * @param[out] emitLayerParams Pointer to write parameters to.
 */
NV_FLOW_API void NvFlowGridEmitCustomGetLayerParams(const NvFlowGridEmitCustomEmitParams* emitParams, NvFlowUint layerIdx, NvFlowGridEmitCustomEmitLayerParams* emitLayerParams);

///@}
// -------------------------- NvFlowGridExportImport -------------------------------
///@defgroup NvFlowGridExport
///@{

//! Description of a single exported layer
struct NvFlowGridExportImportLayerMapping
{
	NvFlowGridMaterialHandle material;			//!< Grid material associated with this layer

	NvFlowResource* blockTable;					//!< Block table for this layer
	NvFlowResource* blockList;					//!< Block list for this layer

	NvFlowUint numBlocks;						//!< Number of active blocks in this layer
};

//! Description applying to all exported layers
struct NvFlowGridExportImportLayeredMapping
{
	NvFlowShaderLinearParams shaderParams;		//!< Shader parameters for address translation
	NvFlowUint maxBlocks;						//!< Maximum blocks active, for all layers

	NvFlowUint2* layeredBlockListCPU;			//!< CPU list of active blocks, in (blockIdx, layerIdx) pairs
	NvFlowUint layeredNumBlocks;				//!< Number of blocks in layeredBlockListCPU

	NvFlowFloat4x4 modelMatrix;					//!< Transform from grid NDC to world
};

///@}
// -------------------------- NvFlowGridExport -------------------------------
///@defgroup NvFlowGridExport
///@{

//! Texture channel export handle
struct NvFlowGridExportHandle
{
	NvFlowGridExport* gridExport;			//!< GridExport that created this handle
	NvFlowGridTextureChannel channel;		//!< Grid texture channel this handle is for
	NvFlowUint numLayerViews;				//!< Numbers of layers in this grid texture channel
};

//! Description of a single exported layer
struct NvFlowGridExportLayerView
{
	NvFlowResource* data;							//!< Data resource for this layer view
	NvFlowGridExportImportLayerMapping mapping;		//!< Mapping of data to virtual space
};

//! Description applying to all exported layers
struct NvFlowGridExportLayeredView
{
	NvFlowGridExportImportLayeredMapping mapping;	//!< Mapping parameters uniform across layers
};

//! Data to visualize simple shape
struct NvFlowGridExportSimpleShape
{
	NvFlowFloat4x4 localToWorld;		//!< Transform from shape local to world space
	NvFlowShapeDesc shapeDesc;			//!< Shape desc to visualize
};

//! Debug vis data
struct NvFlowGridExportDebugVisView
{
	NvFlowGridDebugVisFlags debugVisFlags;		//!< Debug vis flags to indicate what data is valid

	NvFlowFloat4x4* bounds;						//!< Array of emitter bounds
	NvFlowUint numBounds;						//!< Number of emitter bounds in array
	NvFlowGridExportSimpleShape* spheres;		//!< Array of spheres
	NvFlowUint numSpheres;						//!< Number of spheres in array
	NvFlowGridExportSimpleShape* capsules;		//!< Array of capsules
	NvFlowUint numCapsules;						//!< Number of capsules in array
	NvFlowGridExportSimpleShape* boxes;			//!< Array of boxes
	NvFlowUint numBoxes;						//!< Number of boxes in array
};

/**
 * Get export handle for the specified grid texture channel.
 *
 * @param[in] gridExport The grid export.
 * @param[in] context The context used to create the grid export.
 * @param[in] channel The grid texture channel to return a handle for.
 *
 * @return Returns export handle.
 */
NV_FLOW_API NvFlowGridExportHandle NvFlowGridExportGetHandle(NvFlowGridExport* gridExport, NvFlowContext* context, NvFlowGridTextureChannel channel);

/**
 * Get layerView data for specified exportHandle and layer index.
 *
 * @param[in] handle The grid export handle.
 * @param[in] layerIdx The layer index to return the layerView of.
 * @param[out] layerView Destination for layerView data.
 */
NV_FLOW_API void NvFlowGridExportGetLayerView(NvFlowGridExportHandle handle, NvFlowUint layerIdx, NvFlowGridExportLayerView* layerView);

/**
 * Get layeredView data for specified exportHandle.
 *
 * @param[in] handle The grid export handle.
 * @param[out] layeredView Destination for layeredView data.
 */
NV_FLOW_API void NvFlowGridExportGetLayeredView(NvFlowGridExportHandle handle, NvFlowGridExportLayeredView* layeredView);

/**
 * Get export debug vis data.
 *
 * @param[in] gridExport The grid export.
 * @param[out] view Destination for debug visualization view data.
 */
NV_FLOW_API void NvFlowGridExportGetDebugVisView(NvFlowGridExport* gridExport, NvFlowGridExportDebugVisView* view);

///@}
// -------------------------- NvFlowGridImport -------------------------------
///@defgroup NvFlowGridImport
///@{

//! Object to expose write access to Flow grid simulation data
struct NvFlowGridImport;

//! Description required to create GridImport
struct NvFlowGridImportDesc
{
	NvFlowGridExport* gridExport;		//!< Grid export to use as template for allocation
};

//! Grid import modes
enum NvFlowGridImportMode
{
	eNvFlowGridImportModePoint = 0,		//!< Non redundant write target, conversion possible for linear sampling
	eNvFlowGridImportModeLinear = 1		//!< Redundant write target, avoids conversion
};

//! Parameters for grabbing import view
struct NvFlowGridImportParams
{
	NvFlowGridExport* gridExport;		//!< Grid export to serve as template for grid import
	NvFlowGridTextureChannel channel;	//!< Grid texture channel to generate import data for
	NvFlowGridImportMode importMode;	//!< Import mode, determines import data format
};

//! Texture channel handle
struct NvFlowGridImportHandle
{
	NvFlowGridImport* gridImport;		//!< Grid import that created this handle
	NvFlowGridTextureChannel channel;	//!< Grid texture channel this handle is for
	NvFlowUint numLayerViews;			//!< Number of layers in this grid texture channel
};

//! Description of a single imported layer
struct NvFlowGridImportLayerView
{
	NvFlowResourceRW* dataRW;						//!< This always should be written
	NvFlowResourceRW* blockTableRW;					//!< If StateCPU path is used, this needs to be written, else is nullptr
	NvFlowResourceRW* blockListRW;					//!< If StateCPU path is used, this needs to be written, else is nullptr
	NvFlowGridExportImportLayerMapping mapping;		//!< Mapping of data to virtual space
};

//! Description applying to all imported layers
struct NvFlowGridImportLayeredView
{
	NvFlowGridExportImportLayeredMapping mapping;	//!< Mapping parameters uniform across layers
};

/**
 * Create a standalone grid import.
 *
 * @param[in] context The context to use to create the new grid import.
 * @param[in] desc Description required to create grid import.
 *
 * @return Returns new grid import.
 */
NV_FLOW_API NvFlowGridImport* NvFlowCreateGridImport(NvFlowContext* context, const NvFlowGridImportDesc* desc);

/**
 * Release a standalone grid import.
 *
 * @param[in] gridImport The grid import to release.
 */
NV_FLOW_API void NvFlowReleaseGridImport(NvFlowGridImport* gridImport);

/**
 * Get import handle for the specified grid texture channel and import mode.
 *
 * @param[in] gridImport The grid import.
 * @param[in] context The context used to create the grid import.
 * @param[in] params Parameters for import handle.
 *
 * @return Returns import handle.
 */
NV_FLOW_API NvFlowGridImportHandle NvFlowGridImportGetHandle(NvFlowGridImport* gridImport, NvFlowContext* context, const NvFlowGridImportParams* params);

/**
 * Get layerView data for specified importHandle and layer index.
 *
 * @param[in] handle The grid import handle.
 * @param[in] layerIdx The layer index to return the layerView of.
 * @param[out] layerView Destination for layerView data.
 */
NV_FLOW_API void NvFlowGridImportGetLayerView(NvFlowGridImportHandle handle, NvFlowUint layerIdx, NvFlowGridImportLayerView* layerView);

/**
 * Get layeredView data for specified importHandle.
 *
 * @param[in] handle The grid import handle.
 * @param[out] layeredView Destination for layeredView data.
 */
NV_FLOW_API void NvFlowGridImportGetLayeredView(NvFlowGridImportHandle handle, NvFlowGridImportLayeredView* layeredView);

/**
 * Release grid texture channel for grid import, allowing for memory recycle.
 *
 * @param[in] gridImport The grid import.
 * @param[in] context The context used to create the grid import.
 * @param[in] channel The grid texture channel to release.
 */
NV_FLOW_API void NvFlowGridImportReleaseChannel(NvFlowGridImport* gridImport, NvFlowContext* context, NvFlowGridTextureChannel channel);

/**
 * Get grid export for read access to grid import data.
 *
 * @param[in] gridImport The grid import.
 * @param[in] context The context used to create the grid import.
 *
 * @return Returns grid export.
 */
NV_FLOW_API NvFlowGridExport* NvFlowGridImportGetGridExport(NvFlowGridImport* gridImport, NvFlowContext* context);

//! Object to hold captured CPU export state
struct NvFlowGridImportStateCPU;

//! Parameters for grabbing import view
struct NvFlowGridImportStateCPUParams
{
	NvFlowGridImportStateCPU* stateCPU;		//!< Import CPU state, captured previously with NvFlowGridImportUpdateStateCPU()
	NvFlowGridTextureChannel channel;		//!< Grid texture channel to generate import data for
	NvFlowGridImportMode importMode;		//!< Import mode, determines import data format
};

/**
 * Create a grid import CPU state object.
 *
 * @param[in] gridImport The grid import to create the CPU state against.
 *
 * @return Returns new grid import CPU state.
 */
NV_FLOW_API NvFlowGridImportStateCPU* NvFlowCreateGridImportStateCPU(NvFlowGridImport* gridImport);

/**
 * Release a grid import CPU state object.
 *
 * @param[in] stateCPU The grid import CPU state to release.
 */
NV_FLOW_API void NvFlowReleaseGridImportStateCPU(NvFlowGridImportStateCPU* stateCPU);

/**
 * Capture CPU state from the provided grid export.
 *
 * @param[in] stateCPU The grid import CPU state to update.
 * @param[in] context The context used to create the grid export.
 * @param[in] gridExport The grid export to capture the CPU state of.
 */
NV_FLOW_API void NvFlowGridImportUpdateStateCPU(NvFlowGridImportStateCPU* stateCPU, NvFlowContext* context, NvFlowGridExport* gridExport);

/**
 * Get import handle, using previously captured CPU state to control configuration.
 *
 * @param[in] gridImport The grid import.
 * @param[in] context The context used to create the grid import.
 * @param[in] params Parameters for import handle.
 *
 * @return Returns import handle.
 */
NV_FLOW_API NvFlowGridImportHandle NvFlowGridImportStateCPUGetHandle(NvFlowGridImport* gridImport, NvFlowContext* context, const NvFlowGridImportStateCPUParams* params);

///@}
// -------------------------- NvFlowGridSummary -------------------------------
///@defgroup NvFlowGridSummary
///@{

//! An object that captures coarse grid behavior and provides CPU access
struct NvFlowGridSummary;

//! Description necessary to create grid summary
struct NvFlowGridSummaryDesc
{
	NvFlowGridExport* gridExport;		//!< Grid export to use as template for allocation
};

/**
 * Creates a grid summary object.
 *
 * @param[in] context The context for GPU resource allocation.
 * @param[in] desc Description for memory allocation.
 *
 * @return The created grid summary object.
 */
NV_FLOW_API NvFlowGridSummary* NvFlowCreateGridSummary(NvFlowContext* context, const NvFlowGridSummaryDesc* desc);

/**
 * Releases a grid summary object.
 *
 * @param[in] gridSummary The grid summary object to be released.
 */
NV_FLOW_API void NvFlowReleaseGridSummary(NvFlowGridSummary* gridSummary);

//! CPU state of grid summary
struct NvFlowGridSummaryStateCPU;

/**
 * Creates a grid summary CPU state object.
 *
 * @param[in] gridSummary The grid summary this CPU state will hold data from.
 *
 * @return The created grid summary CPU state object.
 */
NV_FLOW_API NvFlowGridSummaryStateCPU* NvFlowCreateGridSummaryStateCPU(NvFlowGridSummary* gridSummary);

/**
 * Releases a grid summary CPU state object.
 *
 * @param[in] stateCPU The grid summary CPU state object to be released.
 */
NV_FLOW_API void NvFlowReleaseGridSummaryStateCPU(NvFlowGridSummaryStateCPU* stateCPU);

//! Parameters required to update summary CPU state
struct NvFlowGridSummaryUpdateParams
{
	NvFlowGridSummaryStateCPU* stateCPU;	//!< The target to store summary data to

	NvFlowGridExport* gridExport;		//!< GridExport to capture summary from
};

/**
 * Updates the specified stateCPU with the latest available summary data.
 *
 * @param[in] gridSummary The grid summary operator to perform the update.
 * @param[in] context The context the gridExport is valid on.
 * @param[in] params Parameters required to update CPU state.
 */
NV_FLOW_API void NvFlowGridSummaryUpdate(NvFlowGridSummary* gridSummary, NvFlowContext* context, const NvFlowGridSummaryUpdateParams* params);

//! Parameters to debug render the grid summary data
struct NvFlowGridSummaryDebugRenderParams
{
	NvFlowGridSummaryStateCPU* stateCPU;

	NvFlowRenderTargetView* renderTargetView;	//!< Render target to draw visualization to

	NvFlowFloat4x4 projectionMatrix;			//!< Render target projection matrix, row major
	NvFlowFloat4x4 viewMatrix;					//!< Render target view matrix, row major
};

/**
 * Renders a visualization of the specified stateCPU.
 *
 * @param[in] gridSummary The grid summary operator to perform the debug render.
 * @param[in] context The render context.
 * @param[in] params Parameters required to render the CPU state.
 */
NV_FLOW_API void NvFlowGridSummaryDebugRender(NvFlowGridSummary* gridSummary, NvFlowContext* context, const NvFlowGridSummaryDebugRenderParams* params);

//! Summary results
struct NvFlowGridSummaryResult
{
	NvFlowFloat4 worldLocation;
	NvFlowFloat4 worldHalfSize;
	NvFlowFloat3 averageVelocity;
	float averageSpeed;
	float averageTemperature;
	float averageFuel;
	float averageBurn;
	float averageSmoke;
};

/**
 * Returns the number of layers for the grid summary. This establishes the maximum number of results for a given world location.
 *
 * @param[in] stateCPU The grid summary cpu state.
 *
 * @return Returns the number of layers.
 */
NV_FLOW_API NvFlowUint NvFlowGridSummaryGetNumLayers(NvFlowGridSummaryStateCPU* stateCPU);

/**
 * Returns grid material mapped to specied layerIdx.
 *
 * @param[in] stateCPU The grid summary cpu state.
 * @param[in] layerIdx The layer index to get the material mapping of.
 *
 * @return Returns grid material.
 */
NV_FLOW_API NvFlowGridMaterialHandle NvFlowGridSummaryGetLayerMaterial(NvFlowGridSummaryStateCPU* stateCPU, NvFlowUint layerIdx);

/**
 * Returns pointer to array of summary results for the specified layer.
 *
 * @param[in] stateCPU The grid summary state to sample.
 * @param[out] results Pointer to array pointer.
 * @param[out] numResults Pointer to array size.
 * @param[in] layerIdx Layer index to return summary results array from.
 */
NV_FLOW_API void NvFlowGridSummaryGetSummaries(NvFlowGridSummaryStateCPU* stateCPU, NvFlowGridSummaryResult** results, NvFlowUint* numResults, NvFlowUint layerIdx);

///@}
// -------------------------- NvFlowRenderMaterial -------------------------------
///@defgroup NvFlowRenderMaterial
///@{

//! A pool of render materials
struct NvFlowRenderMaterialPool;

//! Description necessary to create render material
struct NvFlowRenderMaterialPoolDesc
{
	NvFlowUint colorMapResolution;		//!< Dimension of 1D texture used to store color map, 64 is a good default
};

/**
 * Creates a render material pool object.
 *
 * @param[in] context The context for GPU resource allocation.
 * @param[in] desc Description for memory allocation.
 *
 * @return The created volume render object.
 */
NV_FLOW_API NvFlowRenderMaterialPool* NvFlowCreateRenderMaterialPool(NvFlowContext* context, const NvFlowRenderMaterialPoolDesc* desc);

/**
 * Releases a volume render object.
 *
 * @param[in] pool The volume render object to be released.
 */
NV_FLOW_API void NvFlowReleaseRenderMaterialPool(NvFlowRenderMaterialPool* pool);

//! A handle to a volume render material
struct NvFlowRenderMaterialHandle
{
	NvFlowRenderMaterialPool* pool;			//!< The pool that created this material
	NvFlowUint64 uid;						//!< Unique id for the render material
};

//! Render modes
enum NvFlowVolumeRenderMode
{
	eNvFlowVolumeRenderMode_colormap = 0,		//!< Uses color map defined in render material
	eNvFlowVolumeRenderMode_raw = 1,			//!< Treats sampled value as RGBA
	eNvFlowVolumeRenderMode_rainbow = 2,		//!< Visualizes single component with rainbow color, good for density visualization
	eNvFlowVolumeRenderMode_debug = 3,			//!< Visualizes xyz components with color, good for velocity visualization

	eNvFlowVolumeRenderModeCount
};

//! Per material parameters for Flow grid rendering
struct NvFlowRenderMaterialParams
{
	NvFlowGridMaterialHandle material;			//!< Grid material to align these parameters with

	float alphaScale;							//!< Global alpha scale for adjust net opacity without color map changes, applied after saturate(alpha)
	float additiveFactor;						//!< 1.0 makes material blend fully additive

	NvFlowFloat4 colorMapCompMask;				//!< Component mask for colormap, control what channel drives color map X axis;
	NvFlowFloat4 alphaCompMask;					//!< Component mask to control which channel(s) modulation the alpha
	NvFlowFloat4 intensityCompMask;				//!< Component mask to control which channel(s) modulates the intensity

	float colorMapMinX;							//!< Minimum value on the x channel (typically temperature), maps to colorMap u = 0.0
	float colorMapMaxX;							//!< Maximum value on the x channel (typically temperature), maps to colorMap u = 1.0
	float alphaBias;							//!< Offsets alpha before saturate(alpha)
	float intensityBias;						//!< Offsets intensity before modulating color
};

/**
 * Allows the application to request default volume render material parameters from Flow.
 *
 * @param[out] params The parameters for Flow to fill out.
 */
NV_FLOW_API void NvFlowRenderMaterialParamsDefaults(NvFlowRenderMaterialParams* params);

/**
 * Get the default render material.
 *
 * @param[in] pool The pool to create/own the material.
 *
 * @return Returns a handle to the default material.
 */
NV_FLOW_API NvFlowRenderMaterialHandle NvFlowGetDefaultRenderMaterial(NvFlowRenderMaterialPool* pool);

/**
 * Create a render material.
 *
 * @param[in] context The context to use for GPU resource creation.
 * @param[in] pool The pool to create/own the material.
 * @param[in] params Material parameters.
 *
 * @return Returns a handle to the material.
 */
NV_FLOW_API NvFlowRenderMaterialHandle NvFlowCreateRenderMaterial(NvFlowContext* context, NvFlowRenderMaterialPool* pool, const NvFlowRenderMaterialParams* params);

/**
 * Release a render material.
 *
 * @param[in] handle Handle to the material to release.
 */
NV_FLOW_API void NvFlowReleaseRenderMaterial(NvFlowRenderMaterialHandle handle);

/**
 * Update a render material.
 *
 * @param[in] handle Handle to the material to update.
 * @param[in] params Material parameters.
 */
NV_FLOW_API void NvFlowRenderMaterialUpdate(NvFlowRenderMaterialHandle handle, const NvFlowRenderMaterialParams* params);

//! Required information for writing to a CPU mapped color map
struct NvFlowColorMapData
{
	NvFlowFloat4* data;			//! Red, green, blue, alpha values
	NvFlowUint dim;				//! Number of float4 elements in mapped array
};

/**
 * Map the color map associated with the material for write access.
 *
 * @param[in] context The context to use for mapping.
 * @param[in] handle Handle to the material to map.
 *
 * @return Returns a pointer and range of CPU memory for write access.
 */
NV_FLOW_API NvFlowColorMapData NvFlowRenderMaterialColorMap(NvFlowContext* context, NvFlowRenderMaterialHandle handle);

/**
 * Unmap the color map associated with the material.
 *
 * @param[in] context The context to perform unmap.
 * @param[in] handle Handle to the material to unmap.
 */
NV_FLOW_API void NvFlowRenderMaterialColorUnmap(NvFlowContext* context, NvFlowRenderMaterialHandle handle);

///@}
// -------------------------- NvFlowVolumeRender -------------------------------
///@defgroup NvFlowVolumeRender
///@{

//! A grid volume renderer
struct NvFlowVolumeRender;

//! Description needed to a create a volume render object
struct NvFlowVolumeRenderDesc
{
	NvFlowGridExport* gridExport;				//!< Grid export, for memory allocation
};

/**
 * Creates a volume render object.
 *
 * @param[in] context The context for GPU resource allocation.
 * @param[in] desc Description for memory allocation.
 *
 * @return The created volume render object.
 */
NV_FLOW_API NvFlowVolumeRender* NvFlowCreateVolumeRender(NvFlowContext* context, const NvFlowVolumeRenderDesc* desc);

/**
 * Releases a volume render object.
 *
 * @param[in] volumeRender The volume render object to be released.
 */
NV_FLOW_API void NvFlowReleaseVolumeRender(NvFlowVolumeRender* volumeRender);

//! Downsample options for offscreen ray march
enum NvFlowVolumeRenderDownsample
{
	eNvFlowVolumeRenderDownsampleNone = 0,
	eNvFlowVolumeRenderDownsample2x2 = 1
};

//! Multiple resolution options for offscreen ray march
enum NvFlowMultiResRayMarch
{
	eNvFlowMultiResRayMarchDisabled = 0,
	eNvFlowMultiResRayMarch2x2 = 1,
	eNvFlowMultiResRayMarch4x4 = 2,
	eNvFlowMultiResRayMarch8x8 = 3,
	eNvFlowMultiResRayMarch16x16 = 4,
};

//! Rendering viewport
struct NvFlowVolumeRenderViewport
{
	float topLeftX;
	float topLeftY;
	float width;
	float height;
};

//! Parameters for VRWorks multires rendering
struct NvFlowVolumeRenderMultiResParams
{
	bool enabled;							//!< If true, app render target is assumed multiRes
	float centerWidth;						//!< Width of central viewport, ranging 0.01..1, where 1 is full orignal viewport width
	float centerHeight;						//!< Height of central viewport, ranging 0.01..1, where 1 is full orignal viewport height
	float centerX;							//!< X location of central viewport, ranging 0..1, where 0.5 is the center of the screen
	float centerY;							//!< Y location of central viewport, ranging 0..1, where 0.5 is the center of the screen
	float densityScaleX[3];					//!< Pixel density scale factors: how much the linear pixel density is scaled within each column (1.0 = full density)
	float densityScaleY[3];					//!< Pixel density scale factors: how much the linear pixel density is scaled within each row (1.0 = full density)
	NvFlowVolumeRenderViewport viewport;	//!< Single viewport representing the entire region to composite against
	float nonMultiResWidth;					//!< The render target width if multires was disabled
	float nonMultiResHeight;				//!< The render target height if multires was disabled
};

//! Parameters for VRWorks lens matched shading rendering
struct NvFlowVolumeRenderLMSParams
{
	bool enabled;							//!< If true, app render target is assumed lens matched shading
	float warpLeft;							
	float warpRight;
	float warpUp;
	float warpDown;
	float sizeLeft;
	float sizeRight;
	float sizeUp;
	float sizeDown;
	NvFlowVolumeRenderViewport viewport;	//!< Single viewport representing the entire region to composite against
	float nonLMSWidth;						//!< The render target width if lens matched shading was disabled
	float nonLMSHeight;						//!< The render target height if lens matched shading was disabled
};

//! Parameters for Flow grid rendering
struct NvFlowVolumeRenderParams
{
	NvFlowFloat4x4 projectionMatrix;			//!< Projection matrix, row major
	NvFlowFloat4x4 viewMatrix;					//!< View matrix, row major
	NvFlowFloat4x4 modelMatrix;					//!< Model matrix, row major

	NvFlowDepthStencilView* depthStencilView;	//!< Depth stencil view for depth testing with ray march
	NvFlowRenderTargetView* renderTargetView;	//!< Render target view to composite ray marched result against

	NvFlowRenderMaterialPool* materialPool;		//!< Pool of materials to look for matches to GridMaterials

	NvFlowVolumeRenderMode renderMode;			//!< Render mode, see NvFlowVolumeRenderMode
	NvFlowGridTextureChannel renderChannel;		//!< GridExport channel to render

	bool debugMode;								//!< If true, wireframe visualization is rendered

	NvFlowVolumeRenderDownsample downsampleFactor;	//!< Controls size of ray marching render target relative to app render target.
	float screenPercentage;							//!< If 1.0, render at full ray march resolution, can be dynamically reduced toward 0.0 to ray march at a lower resolution
	NvFlowMultiResRayMarch multiResRayMarch;		//!< Coarsest downsample for multiple resolution ray march
	float multiResSamplingScale;					//!< 1.0 by default, increase for finer screen XY minimum sampling rate

	bool smoothColorUpsample;						//!< If true, color upsample will do extra work to remove jaggies around depth discontinuities

	bool preColorCompositeOnly;						//!< If true, do all operations except color composite
	bool colorCompositeOnly;						//!< If true, only apply color composite
	bool generateDepth;								//!< If true, generate nominal depth, and write to scene depth buffer
	bool generateDepthDebugMode;					//!< If true, visualize depth estimate
	float depthAlphaThreshold;						//!< Minimum alpha to trigger depth write
	float depthIntensityThreshold;					//!< Intensity on R or G or B to trigger depth write

	NvFlowVolumeRenderMultiResParams multiRes;			//!< Multires parameters

	NvFlowVolumeRenderLMSParams lensMatchedShading;		//!< Lens matched shading parameters
};

/**
 * Allows the application to request default volume render parameters from Flow.
 *
 * @param[out] params The parameters for Flow to fill out.
 */
NV_FLOW_API void NvFlowVolumeRenderParamsDefaults(NvFlowVolumeRenderParams* params);

//! Parameters for Flow grid lighting
struct NvFlowVolumeLightingParams
{
	NvFlowRenderMaterialPool* materialPool;		//!< Pool of materials to look for matches to GridMaterials

	NvFlowVolumeRenderMode renderMode;			//!< Render mode, see NvFlowVolumeRenderMode
	NvFlowGridTextureChannel renderChannel;		//!< GridExport channel to render
};

/**
 * Lights a grid export to produce another grid export that can be ray marched raw.
 *
 * @param[in] volumeRender The volume render object to perform the lighting.
 * @param[in] context The context that created the volume render object.
 * @param[in] gridExport The grid export to ray march.
 * @param[in] params Parameters for lighting.
 *
 * @return The lit grid view.
 */
NV_FLOW_API NvFlowGridExport* NvFlowVolumeRenderLightGridExport(NvFlowVolumeRender* volumeRender, NvFlowContext* context, NvFlowGridExport* gridExport, const NvFlowVolumeLightingParams* params);

/**
 * Renders a grid export.
 *
 * @param[in] volumeRender The volume render object to perform the rendering.
 * @param[in] context The context that created the volume render object.
 * @param[in] gridExport The grid export to ray march.
 * @param[in] params Parameters for rendering.
 */
NV_FLOW_API void NvFlowVolumeRenderGridExport(NvFlowVolumeRender* volumeRender, NvFlowContext* context, NvFlowGridExport* gridExport, const NvFlowVolumeRenderParams* params);

/**
 * Renders a 3D texture.
 *
 * @param[in] volumeRender The volume render object to perform the rendering.
 * @param[in] context The context that created the volume render object.
 * @param[in] density The 3D texture to ray march.
 * @param[in] params Parameters for rendering.
 */
NV_FLOW_API void NvFlowVolumeRenderTexture3D(NvFlowVolumeRender* volumeRender, NvFlowContext* context, NvFlowTexture3D* density, const NvFlowVolumeRenderParams* params);

///@}
// -------------------------- NvFlowVolumeShadow -------------------------------
///@defgroup NvFlowVolumeShadow
///@{

//! Object to generate shadows from grid export
struct NvFlowVolumeShadow;

//! Description required to create volume shadow object
struct NvFlowVolumeShadowDesc
{
	NvFlowGridExport* gridExport;		//!< Grid export for memory allocation

	NvFlowUint mapWidth;				//!< Virtual width of shadow voxel address space
	NvFlowUint mapHeight;				//!< Virtual height of shadow voxel address space
	NvFlowUint mapDepth;				//!< Virtual depth of shadow voxel address space

	float minResidentScale;				//!< Minimum (and initial) fraction of virtual cells to allocate memory for
	float maxResidentScale;				//!< Maximum fraction of virtual cells to allocate memory for
};

//! Parameters required to update volume shadows
struct NvFlowVolumeShadowParams
{
	NvFlowFloat4x4 projectionMatrix;			//!< Projection matrix, row major
	NvFlowFloat4x4 viewMatrix;					//!< View matrix, row major

	NvFlowRenderMaterialPool* materialPool;		//!< Pool of materials to look for matches to GridMaterials

	NvFlowVolumeRenderMode renderMode;			//!< Render mode, see NvFlowVolumeRenderMode
	NvFlowGridTextureChannel renderChannel;		//!< GridExport channel to render

	float intensityScale;						//!< Shadow intensity scale
	float minIntensity;							//!< Minimum shadow intensity

	NvFlowFloat4 shadowBlendCompMask;			//!< Component mask to control which channel(s) modulate the shadow blending
	float shadowBlendBias;						//!< Bias on shadow blend factor
};

//! Parameters required to visualize shadow block allocation
struct NvFlowVolumeShadowDebugRenderParams
{
	NvFlowRenderTargetView* renderTargetView;	//!< Render target to draw visualization to

	NvFlowFloat4x4 projectionMatrix;			//!< Render target projection matrix, row major
	NvFlowFloat4x4 viewMatrix;					//!< Render target view matrix, row major
};

//! Stats on currently active volume shadow
struct NvFlowVolumeShadowStats
{
	NvFlowUint shadowColumnsActive;
	NvFlowUint shadowBlocksActive;
	NvFlowUint shadowCellsActive;
};

/**
 * Creates a volume shadow object.
 *
 * @param[in] context The context for GPU resource allocation.
 * @param[in] desc Description for memory allocation.
 *
 * @return The created volume shadow object.
 */
NV_FLOW_API NvFlowVolumeShadow* NvFlowCreateVolumeShadow(NvFlowContext* context, const NvFlowVolumeShadowDesc* desc);

/**
 * Releases a volume shadow object.
 *
 * @param[in] volumeShadow The volume shadow object to be released.
 */
NV_FLOW_API void NvFlowReleaseVolumeShadow(NvFlowVolumeShadow* volumeShadow);

/**
 * Generate shadows from provided grid export.
 *
 * @param[in] volumeShadow The volume shadow object.
 * @param[in] context The context that created the volume shadow object.
 * @param[in] gridExport The grid export to use for generating shadows.
 * @param[in] params Parameters for shadow generation.
 */
NV_FLOW_API void NvFlowVolumeShadowUpdate(NvFlowVolumeShadow* volumeShadow, NvFlowContext* context, NvFlowGridExport* gridExport, const NvFlowVolumeShadowParams* params);

/**
 * Get grid export with shadow results. Currently, shadow results are placed in z component (the burn component).
 *
 * @param[in] volumeShadow The volume shadow object.
 * @param[in] context The context that created the volume shadow object.
 *
 * @return Returns grid export with shadow results.
 */
NV_FLOW_API NvFlowGridExport* NvFlowVolumeShadowGetGridExport(NvFlowVolumeShadow* volumeShadow, NvFlowContext* context);

/**
 * Draw debug visualization of sparse volume shadow structure.
 *
 * @param[in] volumeShadow The volume shadow object.
 * @param[in] context The context that created the volume shadow object.
 * @param[in] params Parameters for debug visualization.
 */
NV_FLOW_API void NvFlowVolumeShadowDebugRender(NvFlowVolumeShadow* volumeShadow, NvFlowContext* context, const NvFlowVolumeShadowDebugRenderParams* params);

/**
 * Get stats for latest shadow computation.
 *
 * @param[in] volumeShadow The volume shadow object.
 * @param[out] stats Destination for shadow computation stats.
 */
NV_FLOW_API void NvFlowVolumeShadowGetStats(NvFlowVolumeShadow* volumeShadow, NvFlowVolumeShadowStats* stats);

///@}
// -------------------------- NvFlowCrossSection -------------------------------
///@defgroup NvFlowCrossSection
///@{

//! Object to visualize cross section from grid export
struct NvFlowCrossSection;

//! Description required to create cross section object
struct NvFlowCrossSectionDesc
{
	NvFlowGridExport* gridExport;		//!< Grid export to serve as template for memory allocation
};

//! Parameters needed to render cross section
struct NvFlowCrossSectionParams
{
	NvFlowGridExport* gridExport;				//!< gridExport used for final rendering
	NvFlowGridExport* gridExportDebugVis;		//!< gridExport direct from simulation

	NvFlowFloat4x4 projectionMatrix;			//!< Projection matrix, row major
	NvFlowFloat4x4 viewMatrix;					//!< View matrix, row major

	NvFlowDepthStencilView* depthStencilView;	//!< Depth stencil view for depth testing with ray march
	NvFlowRenderTargetView* renderTargetView;	//!< Render target view to composite ray marched result against

	NvFlowRenderMaterialPool* materialPool;		//!< Pool of materials to look for matches to GridMaterials

	NvFlowVolumeRenderMode renderMode;			//!< Render mode, see NvFlowVolumeRenderMode
	NvFlowGridTextureChannel renderChannel;		//!< GridExport channel to render

	NvFlowUint crossSectionAxis;				//!< Cross section to visualize, 0 to 2 range
	NvFlowFloat3 crossSectionPosition;			//!< Offset in grid NDC for view
	float crossSectionScale;					//!< Scale on cross section to allow zooming

	float intensityScale;						//!< scales the visualization intensity

	bool pointFilter;							//!< If true, point filter so the cells are easy to see

	bool velocityVectors;						//!< If true, overlay geometric velocity vectors
	float velocityScale;						//!< Scale to adjust vector length as a function of velocity
	float vectorLengthScale;					//!< Controls maximum velocity vector line length

	bool outlineCells;							//!< Draw lines around cell boundaries

	bool fullscreen;							//!< If true, covers entire viewport, if false, top right corner

	NvFlowFloat4 lineColor;						//!< Color to use for any lines drawn
	NvFlowFloat4 backgroundColor;				//!< Background color
	NvFlowFloat4 cellColor;						//!< Color for cell outline
};

/**
 * Allows the application to request default cross section parameters from Flow.
 *
 * @param[out] params The parameters for Flow to fill out.
 */
NV_FLOW_API void NvFlowCrossSectionParamsDefaults(NvFlowCrossSectionParams* params);

/**
 * Creates a cross section object.
 *
 * @param[in] context The context for GPU resource allocation.
 * @param[in] desc Description for memory allocation.
 *
 * @return The created cross section object.
 */
NV_FLOW_API NvFlowCrossSection* NvFlowCreateCrossSection(NvFlowContext* context, const NvFlowCrossSectionDesc* desc);

/**
 * Releases a cross section object.
 *
 * @param[in] crossSection The cross section object to be released.
 */
NV_FLOW_API void NvFlowReleaseCrossSection(NvFlowCrossSection* crossSection);

/**
 * Renders a cross section of a grid export.
 *
 * @param[in] crossSection The cross section object.
 * @param[in] context The context that allocated the cross section object.
 * @param[in] params Parameters for cross section rendering.
 */
NV_FLOW_API void NvFlowCrossSectionRender(NvFlowCrossSection* crossSection, NvFlowContext* context, const NvFlowCrossSectionParams* params);

///@}
// -------------------------- NvFlowGridProxy -------------------------------
///@defgroup NvFlowGridProxy
///@{

//! A proxy for a grid simulated on one device to render on a different device, currently limited to Windows 10 for multi-GPU support.
struct NvFlowGridProxy;

//! Proxy types
enum NvFlowGridProxyType
{
	eNvFlowGridProxyTypePassThrough = 0,	//!< No operation, allows common code path for single versus multiple GPUs in the application
	eNvFlowGridProxyTypeMultiGPU = 1,		//!< Transports render data between GPUs
	eNvFlowGridProxyTypeInterQueue = 2,		//!< Versions grid export data for safe async compute
};

//! Parameters need to create a grid proxy
struct NvFlowGridProxyDesc
{
	NvFlowContext* gridContext;			//!< Context used to simulate grid
	NvFlowContext* renderContext;		//!< Context used to render grid
	NvFlowContext* gridCopyContext;		//!< Context with copy capability on gridContext device
	NvFlowContext* renderCopyContext;	//!< Context with copy capability on renderContext device

	NvFlowGridExport* gridExport;		//!< GridExport to base allocation on

	NvFlowGridProxyType proxyType;		//!< GridProxy type to create
};

//! Parameters need to create a multi-GPU proxy
struct NvFlowGridProxyFlushParams
{
	NvFlowContext* gridContext;			//!< Context used to simulate grid
	NvFlowContext* gridCopyContext;		//!< Context with copy capability on gridContext device
	NvFlowContext* renderCopyContext;	//!< Context with copy capability on renderContext device
};

/**
 * Creates a grid proxy.
 *
 * @param[in] desc Description required to create grid proxy.
 *
 * @return The created grid proxy.
 */
NV_FLOW_API NvFlowGridProxy* NvFlowCreateGridProxy(const NvFlowGridProxyDesc* desc);

/**
 * Releases a grid proxy.
 *
 * @param[in] proxy The grid proxy to be released.
 */
NV_FLOW_API void NvFlowReleaseGridProxy(NvFlowGridProxy* proxy);

/**
 * Pushes simulation results to the proxy, should be updated after each simulation update.
 *
 * @param[in] proxy The grid proxy to be updated.
 * @param[in] gridExport The grid export with latest simulation results.
 * @param[in] params Parameters needed to flush the data.
 */
NV_FLOW_API void NvFlowGridProxyPush(NvFlowGridProxy* proxy, NvFlowGridExport* gridExport, const NvFlowGridProxyFlushParams* params);

/**
 * Helps simulation results move faster between GPUs, should be called before each render.
 *
 * @param[in] proxy The grid proxy to be flushed.
 * @param[in] params Parameters needed to flush the data.
 */
NV_FLOW_API void NvFlowGridProxyFlush(NvFlowGridProxy* proxy, const NvFlowGridProxyFlushParams* params);

/**
 * Returns the latest grid export available on the render GPU.
 *
 * @param[in] proxy The grid proxy supplying the grid export.
 * @param[in] renderContext The context that will render the grid export.
 *
 * @return The latest grid export available from the proxy.
 */
NV_FLOW_API NvFlowGridExport* NvFlowGridProxyGetGridExport(NvFlowGridProxy* proxy, NvFlowContext* renderContext);

///@}
// -------------------------- NvFlowDevice -------------------------------
///@defgroup NvFlowDevice
///@{

//! A device exclusively for Flow simulation
struct NvFlowDevice;

//! Device Type
enum NvFlowDeviceMode
{
	eNvFlowDeviceModeProxy = 0,		//!< Exposes renderContext device
	eNvFlowDeviceModeUnique = 1,	//!< Generates unique device, not matching renderContext
};

//! Description required for creating a Flow device
struct NvFlowDeviceDesc
{
	NvFlowDeviceMode mode;			//!< Type of device to create
	bool autoSelectDevice;			//!< if true, NvFlow tries to identify best compute device
	NvFlowUint adapterIdx;			//!< preferred device index
};

/**
 * Allows the application to request a default Flow device description from Flow.
 *
 * @param[out] desc The description for Flow to fill out.
 */
NV_FLOW_API void NvFlowDeviceDescDefaults(NvFlowDeviceDesc* desc);

/**
 * Checks if a GPU is available that is not being used for application graphics work.
 *
 * @param[in] renderContext A context that maps to the application graphics GPU.
 *
 * @return Returns true if dedicated GPU is available.
 */
NV_FLOW_API bool NvFlowDedicatedDeviceAvailable(NvFlowContext* renderContext);

/**
 * Checks if a GPU can support a dedicated queue
 *
 * @param[in] renderContext A context that maps to the application graphics GPU.
 *
 * @return Returns true if dedicated device queue is available.
 */
NV_FLOW_API bool NvFlowDedicatedDeviceQueueAvailable(NvFlowContext* renderContext);

/**
 * Creates a Flow compute device.
 *
 * @param[in] renderContext A context that maps to the application graphics GPU.
 * @param[in] desc Description that controls what GPU is selected.
 *
 * @return The created Flow compute device.
 */
NV_FLOW_API NvFlowDevice* NvFlowCreateDevice(NvFlowContext* renderContext, const NvFlowDeviceDesc* desc);

/**
 * Releases a Flow compute device.
 *
 * @param[in] device The Flow compute device to be released.
 */
NV_FLOW_API void NvFlowReleaseDevice(NvFlowDevice* device);

//! A device queue created through an NvFlowDevice
struct NvFlowDeviceQueue;

//! Types of queues
enum NvFlowDeviceQueueType
{
	eNvFlowDeviceQueueTypeGraphics = 0,
	eNvFlowDeviceQueueTypeCompute = 1,
	eNvFlowDeviceQueueTypeCopy = 2
};

//! Description required for creating a Flow device queue
struct NvFlowDeviceQueueDesc
{
	NvFlowDeviceQueueType queueType;
	bool lowLatency;
};

//! Flow device queue status to allow app to throttle maximum queued work
struct NvFlowDeviceQueueStatus
{
	NvFlowUint framesInFlight;			//!< Number of flushes that have not completed work on the GPU
	NvFlowUint64 lastFenceCompleted;	//!< The last fence completed on device queue
	NvFlowUint64 nextFenceValue;		//!< The fence value signaled after flush
};

/**
 * Creates a Flow device queue.
 *
 * @param[in] device The device to create the queue on.
 * @param[in] desc Description that controls kind of device queue to create.
 *
 * @return The created Flow device queue.
 */
NV_FLOW_API NvFlowDeviceQueue* NvFlowCreateDeviceQueue(NvFlowDevice* device, const NvFlowDeviceQueueDesc* desc);

/**
 * Releases a Flow device queue.
 *
 * @param[in] deviceQueue The Flow device queue to be released.
 */
NV_FLOW_API void NvFlowReleaseDeviceQueue(NvFlowDeviceQueue* deviceQueue);

/**
 * Creates a context that uses a Flow device queue.
 *
 * @param[in] deviceQueue The Flow device queue to create the context against.
 *
 * @return The created context.
 */
NV_FLOW_API NvFlowContext* NvFlowDeviceQueueCreateContext(NvFlowDeviceQueue* deviceQueue);

/**
 * Updates a context that uses a Flow device queue.
 *
 * @param[in] deviceQueue The Flow device queue the context was created against.
 * @param[in] context The context update.
 * @param[out] status Optional queue status to update, useful to detect if queue is overloaded.
 */
NV_FLOW_API void NvFlowDeviceQueueUpdateContext(NvFlowDeviceQueue* deviceQueue, NvFlowContext* context, NvFlowDeviceQueueStatus* status);

/**
 * Flushes all submitted work to the Flow deviceQueue. Must be called to submit work to queue.
 *
 * @param[in] deviceQueue The Flow deviceQueue to flush.
 * @param[in] context The context to sync with the flush event
 */
NV_FLOW_API void NvFlowDeviceQueueFlush(NvFlowDeviceQueue* deviceQueue, NvFlowContext* context);

/**
 * Flushes all submitted work to the Flow deviceQueue if the context requests a flush.
 *
 * @param[in] deviceQueue The Flow deviceQueue to conditionally flush.
 * @param[in] context The context to sync with the flush event.
 */
NV_FLOW_API void NvFlowDeviceQueueConditionalFlush(NvFlowDeviceQueue* deviceQueue, NvFlowContext* context);

/**
 * Blocks CPU until fenceValue is reached.
 *
 * @param[in] deviceQueue The Flow deviceQueue to flush.
 * @param[in] context The context to sync with the flush event.
 * @param[in] fenceValue The fence value to wait for.
 */
NV_FLOW_API void NvFlowDeviceQueueWaitOnFence(NvFlowDeviceQueue* deviceQueue, NvFlowContext* context, NvFlowUint64 fenceValue);

///@}
// -------------------------- NvFlowSDFGenerator -------------------------------
///@defgroup NvFlowSDFGenerator
///@{

//! A signed distance field generator
struct NvFlowSDFGen;

//! Description required for creating a signed distance field generator
struct NvFlowSDFGenDesc
{
	NvFlowDim resolution;		//!< Resolution of 3D texture storing signed distance field
};

//! Simple mesh description
struct NvFlowSDFGenMeshParams
{
	NvFlowUint numVertices;			//!< Numbers of vertices in triangle mesh
	float* positions;				//!< Array of positions, stored in x, y, z order
	NvFlowUint positionStride;		//!< The distance between the beginning of one position to the beginning of the next position in array, in bytes
	float* normals;					//!< Array of normals, stored in nx, ny, nz order
	NvFlowUint normalStride;		//!< The distance between the beginning of one normal to the beginning of the next normal in array, in bytes

	NvFlowUint numIndices;			//!< Numbers of indices in triangle mesh
	NvFlowUint* indices;			//!< Array of indices

	NvFlowFloat4x4 modelMatrix;		//!< transforms from model space to SDF NDC space

	NvFlowDepthStencilView* depthStencilView;	//!< Depth stencil view to restore after voxelize work, lighter than Flow context push/pop
	NvFlowRenderTargetView* renderTargetView;	//!< Render target view to restore after voxelize work, lighter than Flow context push/pop
};

/**
 * Creates a Flow signed distance field generator.
 *
 * @param[in] context The context for GPU resource allocation.
 * @param[in] desc Description for memory allocation.
 *
 * @return The created signed distance field generator.
 */
NV_FLOW_API NvFlowSDFGen* NvFlowCreateSDFGen(NvFlowContext* context, const NvFlowSDFGenDesc* desc);

/**
 * Releases a signed distance field generator.
 *
 * @param[in] sdfGen The signed distance field generator to be released.
 */
NV_FLOW_API void NvFlowReleaseSDFGen(NvFlowSDFGen* sdfGen);

/**
 * Clears previous voxelization.
 *
 * @param[in] sdfGen The signed distance field generator to test.
 * @param[in] context The context that created sdfGen.
 */
NV_FLOW_API void NvFlowSDFGenReset(NvFlowSDFGen* sdfGen, NvFlowContext* context);

/**
 * Voxelizes triangle mesh.
 *
 * @param[in] sdfGen The signed distance field generator to perform voxelization.
 * @param[in] context The context that created sdfGen.
 * @param[in] params Parameters, including triangle mesh data.
 */
NV_FLOW_API void NvFlowSDFGenVoxelize(NvFlowSDFGen* sdfGen, NvFlowContext* context, const NvFlowSDFGenMeshParams* params);

/**
 * Generates signed distance field from latest voxelization.
 *
 * @param[in] sdfGen The signed distance field generator to update.
 * @param[in] context The context that created sdfGen.
 */
NV_FLOW_API void NvFlowSDFGenUpdate(NvFlowSDFGen* sdfGen, NvFlowContext* context);

/**
 * Provides access to signed distance field 3D Texture.
 *
 * @param[in] sdfGen The signed distance field generator.
 * @param[in] context The context that created sdfGen.
 *
 * @return The 3D texture storing the latest signed distance field.
 */
NV_FLOW_API NvFlowTexture3D* NvFlowSDFGenShape(NvFlowSDFGen* sdfGen, NvFlowContext* context);

///@}
// -------------------------- NvFlowParticleSurface -------------------------------
///@defgroup NvFlowParticleSurface
///@{

//! A particle surface generator
struct NvFlowParticleSurface;

//! Description for creation
struct NvFlowParticleSurfaceDesc
{
	NvFlowFloat3 initialLocation;		//!< Initial location of axis aligned bounding box
	NvFlowFloat3 halfSize;				//!< Initial half size of axis aligned bounding box

	NvFlowDim virtualDim;				//!< Resolution of virtual address space inside of bounding box
	float residentScale;				//!< Fraction of virtual cells to allocate memory for

	NvFlowUint maxParticles;			//!< Maximum particle count for memory allocation
};

//! Particle data
struct NvFlowParticleSurfaceData
{
	const float* positions;			//!< Array of particle positions, xyz components
	NvFlowUint positionStride;		//!< Stride in bytes between particles
	NvFlowUint numParticles;		//!< Number of particles in array
};

//! Parameters for update
struct NvFlowParticleSurfaceParams
{
	float surfaceThreshold;			//!< Threshold used to define isosurface
	float smoothRadius;				//!< Radius of smoothing kernel
	bool separableSmoothing;		//!< If true, use separable convolution for smoothing
};

//! Parameter for surface emission
struct NvFlowParticleSurfaceEmitParams
{
	float deltaTime;

	NvFlowFloat3 velocityLinear;					//!< Linear velocity, in world units, emitter direction
	NvFlowFloat3 velocityCoupleRate;				//!< Rate of correction to target, inf means instantaneous

	float smoke;									//!< Target smoke
	float smokeCoupleRate;							//!< Rate of correction to target, inf means instantaneous

	float temperature;								//!< Target temperature
	float temperatureCoupleRate;					//!< Rate of correction to target, inf means instantaneous

	float fuel;										//!< Target fuel
	float fuelCoupleRate;							//!< Rate of correction to target, inf means instantaneous
};

/**
 * Create a particle surface object.
 *
 * @param[in] context The context to use to create the new particle surface.
 * @param[in] desc Description required to create particle surface object.
 *
 * @return Returns created particle surface object.
 */
NV_FLOW_API NvFlowParticleSurface* NvFlowCreateParticleSurface(NvFlowContext* context, const NvFlowParticleSurfaceDesc* desc);

/**
 * Releases a particle surface object.
 *
 * @param[in] surface The particle surface object to be released.
 */
NV_FLOW_API void NvFlowReleaseParticleSurface(NvFlowParticleSurface* surface);

/**
 * Update particle data for particle surface.
 *
 * @param[in] surface The particle surface to update.
 * @param[in] context The context used to create the particle surface.
 * @param[in] data Particle data.
 */
NV_FLOW_API void NvFlowParticleSurfaceUpdateParticles(NvFlowParticleSurface* surface, NvFlowContext* context, const NvFlowParticleSurfaceData* data);

/**
 * Generate surface using the latest particle data.
 *
 * @param[in] surface The particle surface to update.
 * @param[in] context The context used to create the particle surface.
 * @param[in] params Parameters for surface generation.
 */
NV_FLOW_API void NvFlowParticleSurfaceUpdateSurface(NvFlowParticleSurface* surface, NvFlowContext* context, const NvFlowParticleSurfaceParams* params);

/**
 * Apply particle surface allocation to grid.
 *
 * @param[in] surface The particle surface object.
 * @param[in] context The context used to create the particle surface and the grid.
 * @param[in] params Parameters for grid custom allocation callback.
 */
NV_FLOW_API void NvFlowParticleSurfaceAllocFunc(NvFlowParticleSurface* surface, NvFlowContext* context, const NvFlowGridEmitCustomAllocParams* params);

/**
 * Apply particle surface emit operation to grid velocity texture channel.
 *
 * @param[in] surface The particle surface object.
 * @param[in] context The context used to create the particle surface and the grid.
 * @param[in] dataFrontIdx Pointer to front data index.
 * @param[in] params Parameters for grid custom emit callback.
 * @param[in] emitParams Parameters to control surface emit behavior.
 */
NV_FLOW_API void NvFlowParticleSurfaceEmitVelocityFunc(NvFlowParticleSurface* surface, NvFlowContext* context, NvFlowUint* dataFrontIdx, const NvFlowGridEmitCustomEmitParams* params, const NvFlowParticleSurfaceEmitParams* emitParams);

/**
 * Apply particle surface emit operation to grid density texture channel.
 *
 * @param[in] surface The particle surface object.
 * @param[in] context The context used to create the particle surface and the grid.
 * @param[in] dataFrontIdx Pointer to front data index.
 * @param[in] params Parameters for grid custom emit callback.
 * @param[in] emitParams Parameters to control surface emit behavior.
 */
NV_FLOW_API void NvFlowParticleSurfaceEmitDensityFunc(NvFlowParticleSurface* surface, NvFlowContext* context, NvFlowUint* dataFrontIdx, const NvFlowGridEmitCustomEmitParams* params, const NvFlowParticleSurfaceEmitParams* emitParams);

/**
 * Get grid export that can be ray marched to visualize the generated particle surface/volume.
 *
 * @param[in] surface The particle surface object.
 * @param[in] context The context used to create the particle surface.
 *
 * @return Returns the grid export.
 */
NV_FLOW_API NvFlowGridExport* NvFlowParticleSurfaceDebugGridExport(NvFlowParticleSurface* surface, NvFlowContext* context);

///@}