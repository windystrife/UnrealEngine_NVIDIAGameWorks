// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "Misc/SecureHash.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "Factories/FbxImportUI.h"
#include "Logging/TokenizedMessage.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Factories/FbxTextureImportData.h"
#include "Factories/FbxSceneImportFactory.h"

class AActor;
class ACameraActor;
class ALight;
class AMatineeActor;
class Error;
class FSkeletalMeshImportData;
class UActorComponent;
class UAnimSequence;
class UFbxSkeletalMeshImportData;
class UInterpGroupInst;
class UInterpTrackMove;
class UInterpTrackMoveAxis;
class ULightComponent;
class UMaterial;
class UMaterialInstanceConstant;
class UMaterialInterface;
class UPhysicsAsset;
class USkeletalMesh;
class USkeleton;
class UStaticMesh;
class UTexture;
struct FExpressionInput;
struct FRawMesh;
struct FRichCurve;

// Temporarily disable a few warnings due to virtual function abuse in FBX source files
#pragma warning( push )

#pragma warning( disable : 4263 ) // 'function' : member function does not override any base class virtual member function
#pragma warning( disable : 4264 ) // 'virtual_function' : no override available for virtual member function from base 'class'; function is hidden

// Include the fbx sdk header
// temp undef/redef of _O_RDONLY because kfbxcache.h (included by fbxsdk.h) does
// a weird use of these identifiers inside an enum.
#ifdef _O_RDONLY
#define TMP_UNFBX_BACKUP_O_RDONLY _O_RDONLY
#define TMP_UNFBX_BACKUP_O_WRONLY _O_WRONLY
#undef _O_RDONLY
#undef _O_WRONLY
#endif

//Robert G. : Packing was only set for the 64bits platform, but we also need it for 32bits.
//This was found while trying to trace a loop that iterate through all character links.
//The memory didn't match what the debugger displayed, obviously since the packing was not right.
#pragma pack(push,8)

#if PLATFORM_WINDOWS
// _CRT_SECURE_NO_DEPRECATE is defined but is not enough to suppress the deprecation
// warning for vsprintf and stricmp in VS2010.  Since FBX is able to properly handle the non-deprecated
// versions on the appropriate platforms, _CRT_SECURE_NO_DEPRECATE is temporarily undefined before
// including the FBX headers

// The following is a hack to make the FBX header files compile correctly under Visual Studio 2012 and Visual Studio 2013
#if _MSC_VER >= 1700
	#define FBX_DLL_MSC_VER 1600
#endif


#endif // PLATFORM_WINDOWS

// FBX casts null pointer to a reference
THIRD_PARTY_INCLUDES_START
#include <fbxsdk.h>
THIRD_PARTY_INCLUDES_END

#pragma pack(pop)




#ifdef TMP_UNFBX_BACKUP_O_RDONLY
#define _O_RDONLY TMP_FBX_BACKUP_O_RDONLY
#define _O_WRONLY TMP_FBX_BACKUP_O_WRONLY
#undef TMP_UNFBX_BACKUP_O_RDONLY
#undef TMP_UNFBX_BACKUP_O_WRONLY
#endif

#pragma warning( pop )

class FSkeletalMeshImportData;
class FSkelMeshOptionalImportData;
class ASkeletalMeshActor;
class UInterpTrackMoveAxis;
struct FbxSceneInfo;

DECLARE_LOG_CATEGORY_EXTERN(LogFbx, Log, All);

#define DEBUG_FBX_NODE( Prepend, FbxNode ) FPlatformMisc::LowLevelOutputDebugStringf( TEXT("%s %s\n"), ANSI_TO_TCHAR(Prepend), ANSI_TO_TCHAR( FbxNode->GetName() ) )

namespace UnFbx
{

struct FBXImportOptions
{
	// General options
	bool bImportScene;
	bool bImportMaterials;
	bool bResetMaterialSlots;
	bool bInvertNormalMap;
	bool bImportTextures;
	bool bImportLOD;
	bool bUsedAsFullName;
	bool bConvertScene;
	bool bForceFrontXAxis;
	bool bConvertSceneUnit;
	bool bRemoveNameSpace;
	FVector ImportTranslation;
	FRotator ImportRotation;
	float ImportUniformScale;
	EFBXNormalImportMethod NormalImportMethod;
	EFBXNormalGenerationMethod::Type NormalGenerationMethod;
	bool bTransformVertexToAbsolute;
	bool bBakePivotInVertex;
	EFBXImportType ImportType;
	// Static Mesh options
	bool bCombineToSingle;
	EVertexColorImportOption::Type VertexColorImportOption;
	FColor VertexOverrideColor;
	bool bRemoveDegenerates;
	bool bBuildAdjacencyBuffer;
	bool bBuildReversedIndexBuffer;
	bool bGenerateLightmapUVs;
	bool bOneConvexHullPerUCX;
	bool bAutoGenerateCollision;
	FName StaticMeshLODGroup;
	bool bImportStaticMeshLODs;
	bool bAutoComputeLodDistances;
	TArray<float> LodDistances;
	int32 MinimumLodNumber;
	int32 LodNumber;
	// Material import options
	class UMaterialInterface *BaseMaterial;
	FString BaseColorName;
	FString BaseDiffuseTextureName;
	FString BaseEmissiveColorName;
	FString BaseNormalTextureName;
	FString BaseEmmisiveTextureName;
	FString BaseSpecularTextureName;
	EMaterialSearchLocation MaterialSearchLocation;
	// Skeletal Mesh options
	bool bImportMorph;
	bool bImportAnimations;
	bool bUpdateSkeletonReferencePose;
	bool bResample;
	bool bImportRigidMesh;
	bool bUseT0AsRefPose;
	bool bPreserveSmoothingGroups;
	bool bKeepOverlappingVertices;
	bool bImportMeshesInBoneHierarchy;
	bool bCreatePhysicsAsset;
	UPhysicsAsset *PhysicsAsset;
	bool bImportSkeletalMeshLODs;
	// Animation option
	USkeleton* SkeletonForAnimation;
	EFBXAnimationLengthImportType AnimationLengthImportType;
	struct FIntPoint AnimationRange;
	FString AnimationName;
	bool	bPreserveLocalTransform;
	bool	bDeleteExistingMorphTargetCurves;
	bool	bImportCustomAttribute;
	bool	bSetMaterialDriveParameterOnCustomAttribute;
	bool	bRemoveRedundantKeys;
	bool	bDoNotImportCurveWithZero;
	TArray<FString> MaterialCurveSuffixes;

	/** This allow to add a prefix to the material name when unreal material get created.	
	*   This prefix can just modify the name of the asset for materials (i.e. TEXT("Mat"))
	*   This prefix can modify the package path for materials (i.e. TEXT("/Materials/")).
	*   Or both (i.e. TEXT("/Materials/Mat"))
	*/
	FName MaterialBasePath;

	//This data allow to override some fbx Material(point by the uint64 id) with existing unreal material asset
	TMap<uint64, class UMaterialInterface*> OverrideMaterials;

	//The importer is importing a preview
	bool bIsReimportPreview;

	bool ShouldImportNormals()
	{
		return NormalImportMethod == FBXNIM_ImportNormals || NormalImportMethod == FBXNIM_ImportNormalsAndTangents;
	}

	bool ShouldImportTangents()
	{
		return NormalImportMethod == FBXNIM_ImportNormalsAndTangents;
	}

	void ResetForReimportAnimation()
	{
		bImportMorph = true;
		AnimationLengthImportType = FBXALIT_ExportedTime;
	}

	static void ResetOptions(FBXImportOptions *OptionsToReset)
	{
		check(OptionsToReset != nullptr);
		*OptionsToReset = FBXImportOptions();
	}
};

#define INVALID_UNIQUE_ID 0xFFFFFFFFFFFFFFFF

class FFbxAnimCurveHandle
{
public:
	enum CurveTypeDescription
	{
		Transform_Translation_X,
		Transform_Translation_Y,
		Transform_Translation_Z,
		Transform_Rotation_X,
		Transform_Rotation_Y,
		Transform_Rotation_Z,
		Transform_Scaling_X,
		Transform_Scaling_Y,
		Transform_Scaling_Z,
		NotTransform,
	};

	FFbxAnimCurveHandle()
	{
		UniqueId = INVALID_UNIQUE_ID;
		Name.Empty();
		ChannelIndex = 0;
		CompositeIndex = 0;
		KeyNumber = 0;
		AnimationTimeSecond = 0.0f;
		AnimCurve = nullptr;
		CurveType = NotTransform;
	}

	FFbxAnimCurveHandle(const FFbxAnimCurveHandle &CurveHandle)
	{
		UniqueId = CurveHandle.UniqueId;
		Name = CurveHandle.Name;
		ChannelIndex = CurveHandle.ChannelIndex;
		CompositeIndex = CurveHandle.CompositeIndex;
		KeyNumber = CurveHandle.KeyNumber;
		AnimationTimeSecond = CurveHandle.AnimationTimeSecond;
		AnimCurve = CurveHandle.AnimCurve;
		CurveType = CurveHandle.CurveType;
	}

	//Identity Data
	uint64 UniqueId;
	FString Name;
	int32 ChannelIndex;
	int32 CompositeIndex;

	//Curve Information
	int32 KeyNumber;
	float AnimationTimeSecond;

	//Pointer to the curve data
	FbxAnimCurve* AnimCurve;

	CurveTypeDescription CurveType;
};

class FFbxAnimPropertyHandle
{
public:
	FFbxAnimPropertyHandle()
	{
		Name.Empty();
		DataType = eFbxFloat;
	}

	FFbxAnimPropertyHandle(const FFbxAnimPropertyHandle &PropertyHandle)
	{
		Name = PropertyHandle.Name;
		DataType = PropertyHandle.DataType;
		CurveHandles = PropertyHandle.CurveHandles;
	}

	FString Name;
	EFbxType DataType;
	TArray<FFbxAnimCurveHandle> CurveHandles;
};

class FFbxAnimNodeHandle
{
public:
	FFbxAnimNodeHandle()
	{
		UniqueId = INVALID_UNIQUE_ID;
		Name.Empty();
		AttributeUniqueId = INVALID_UNIQUE_ID;
		AttributeType = FbxNodeAttribute::eUnknown;
	}

	FFbxAnimNodeHandle(const FFbxAnimNodeHandle &NodeHandle)
	{
		UniqueId = NodeHandle.UniqueId;
		Name = NodeHandle.Name;
		AttributeUniqueId = NodeHandle.AttributeUniqueId;
		AttributeType = NodeHandle.AttributeType;
		NodeProperties = NodeHandle.NodeProperties;
		AttributeProperties = NodeHandle.AttributeProperties;
	}

	uint64 UniqueId;
	FString Name;
	TMap<FString, FFbxAnimPropertyHandle> NodeProperties;

	uint64 AttributeUniqueId;
	FbxNodeAttribute::EType AttributeType;
	TMap<FString, FFbxAnimPropertyHandle> AttributeProperties;
};

class FFbxCurvesAPI
{
public:
	FFbxCurvesAPI()
	{
		Scene = nullptr;
	}
	//Name API
	UNREALED_API void GetAllNodeNameArray(TArray<FString> &AllNodeNames) const;
	UNREALED_API void GetAnimatedNodeNameArray(TArray<FString> &AnimatedNodeNames) const;
	UNREALED_API void GetNodeAnimatedPropertyNameArray(const FString &NodeName, TArray<FString> &AnimatedPropertyNames) const;
	UNREALED_API void GetCurveData(const FString& NodeName, const FString& PropertyName, int32 ChannelIndex, int32 CompositeIndex, FInterpCurveFloat& CurveData, bool bNegative) const;
	UNREALED_API void GetBakeCurveData(const FString& NodeName, const FString& PropertyName, int32 ChannelIndex, int32 CompositeIndex, TArray<float>& CurveData, float PeriodTime, float StartTime = 0.0f, float StopTime= -1.0f, bool bNegative = false) const;

	//Handle API
	UNREALED_API void GetAllNodePropertyCurveHandles(const FString& NodeName, const FString& PropertyName, TArray<FFbxAnimCurveHandle> &PropertyCurveHandles) const;
	UNREALED_API void GetCurveHandle(const FString& NodeName, const FString& PropertyName, int32 ChannelIndex, int32 CompositeIndex, FFbxAnimCurveHandle &CurveHandle) const;
	UNREALED_API void GetCurveData(const FFbxAnimCurveHandle &CurveHandle, FInterpCurveFloat& CurveData, bool bNegative) const;
	UNREALED_API void GetBakeCurveData(const FFbxAnimCurveHandle &CurveHandle, TArray<float>& CurveData, float PeriodTime, float StartTime = 0.0f, float StopTime = -1.0f, bool bNegative = false) const;

	//Conversion API
	UNREALED_API void GetConvertedTransformCurveData(const FString& NodeName, FInterpCurveFloat& TranslationX, FInterpCurveFloat& TranslationY, FInterpCurveFloat& TranslationZ,
													 FInterpCurveFloat& EulerRotationX, FInterpCurveFloat& EulerRotationY, FInterpCurveFloat& EulerRotationZ, 
													 FInterpCurveFloat& ScaleX, FInterpCurveFloat& ScaleY, FInterpCurveFloat& ScaleZ,
													 FTransform& DefaultTransform) const;

	FbxScene* Scene;
	TMap<uint64, FFbxAnimNodeHandle> CurvesData;
	TMap<uint64, FTransform> TransformData;

private:
	EInterpCurveMode GetUnrealInterpMode(FbxAnimCurveKey FbxKey) const;
};

struct FbxMeshInfo
{
	FString Name;
	uint64 UniqueId;
	int32 FaceNum;
	int32 VertexNum;
	bool bTriangulated;
	int32 MaterialNum;
	bool bIsSkelMesh;
	FString SkeletonRoot;
	int32 SkeletonElemNum;
	FString LODGroup;
	int32 LODLevel;
	int32 MorphNum;
};

//Node use to store the scene hierarchy transform will be relative to the parent
struct FbxNodeInfo
{
	const char* ObjectName;
	uint64 UniqueId;
	FbxAMatrix Transform;
	FbxVector4 RotationPivot;
	FbxVector4 ScalePivot;
	
	const char* AttributeName;
	uint64 AttributeUniqueId;
	const char* AttributeType;

	const char* ParentName;
	uint64 ParentUniqueId;
};

struct FbxSceneInfo
{
	// data for static mesh
	int32 NonSkinnedMeshNum;
	
	//data for skeletal mesh
	int32 SkinnedMeshNum;

	// common data
	int32 TotalGeometryNum;
	int32 TotalMaterialNum;
	int32 TotalTextureNum;
	
	TArray<FbxMeshInfo> MeshInfo;
	TArray<FbxNodeInfo> HierarchyInfo;
	
	/* true if it has animation */
	bool bHasAnimation;
	double FrameRate;
	double TotalTime;

	void Reset()
	{
		NonSkinnedMeshNum = 0;
		SkinnedMeshNum = 0;
		TotalGeometryNum = 0;
		TotalMaterialNum = 0;
		TotalTextureNum = 0;
		MeshInfo.Empty();
		HierarchyInfo.Empty();
		bHasAnimation = false;
		FrameRate = 0.0;
		TotalTime = 0.0;
	}
};

/**
* FBX basic data conversion class.
*/
class FFbxDataConverter
{
public:
	static void SetJointPostConversionMatrix(FbxAMatrix ConversionMatrix) { JointPostConversionMatrix = ConversionMatrix; }
	static const FbxAMatrix &GetJointPostConversionMatrix() { return JointPostConversionMatrix; }

	static FVector ConvertPos(FbxVector4 Vector);
	static FVector ConvertDir(FbxVector4 Vector);
	static FRotator ConvertEuler(FbxDouble3 Euler);
	static FVector ConvertScale(FbxDouble3 Vector);
	static FVector ConvertScale(FbxVector4 Vector);
	static FRotator ConvertRotation(FbxQuaternion Quaternion);
	static FVector ConvertRotationToFVect(FbxQuaternion Quaternion, bool bInvertRot);
	static FQuat ConvertRotToQuat(FbxQuaternion Quaternion);
	static float ConvertDist(FbxDouble Distance);
	static bool ConvertPropertyValue(FbxProperty& FbxProperty, UProperty& UnrealProperty, union UPropertyValue& OutUnrealPropertyValue);
	static FTransform ConvertTransform(FbxAMatrix Matrix);
	static FMatrix ConvertMatrix(FbxAMatrix Matrix);

	/*
	 * Convert fbx linear space color to sRGB FColor
	 */
	static FColor ConvertColor(FbxDouble3 Color);

	static FbxVector4 ConvertToFbxPos(FVector Vector);
	static FbxVector4 ConvertToFbxRot(FVector Vector);
	static FbxVector4 ConvertToFbxScale(FVector Vector);
	
	/*
	* Convert sRGB FColor to fbx linear space color
	*/
	static FbxDouble3   ConvertToFbxColor(FColor Color);
	static FbxString	ConvertToFbxString(FName Name);
	static FbxString	ConvertToFbxString(const FString& String);

	// FbxCamera with no rotation faces X with Y-up while ours faces X with Z-up so add a -90 degrees roll to compensate
	static FRotator GetCameraRotation() { return FRotator(0.f, 0.f, -90.f); }

	// FbxLight with no rotation faces -Z while ours faces Y so add a 90 degrees pitch to compensate
	static FRotator GetLightRotation() { return FRotator(0.f, 90.f, 0.f); }

private:
	static FbxAMatrix JointPostConversionMatrix;
};

FBXImportOptions* GetImportOptions( class FFbxImporter* FbxImporter, UFbxImportUI* ImportUI, bool bShowOptionDialog, bool bIsAutomated, const FString& FullPath, bool& OutOperationCanceled, bool& OutImportAll, bool bIsObjFormat, bool bForceImportType = false, EFBXImportType ImportType = FBXIT_StaticMesh, UObject* ReimportObject = nullptr);
//#nv begin #Blast Engine export
UNREALED_API void ApplyImportUIToImportOptions(UFbxImportUI* ImportUI, FBXImportOptions& InOutImportOptions);
//nv end

struct FImportedMaterialData
{
public:
	void AddImportedMaterial( FbxSurfaceMaterial& FbxMaterial, UMaterialInterface& UnrealMaterial );
	bool IsUnique( FbxSurfaceMaterial& FbxMaterial, FName ImportedMaterialName ) const;
	UMaterialInterface* GetUnrealMaterial( const FbxSurfaceMaterial& FbxMaterial ) const;
	void Clear();
private:
	/** Mapping of FBX material to Unreal material.  Some materials in FBX have the same name so we use this map to determine if materials are unique */
	TMap<FbxSurfaceMaterial*, TWeakObjectPtr<UMaterialInterface> > FbxToUnrealMaterialMap;
	TSet<FName> ImportedMaterialNames;
};

enum EFbxCreator
{
	Blender,
	Unknow
};

/**
 * Main FBX Importer class.
 */
class FFbxImporter
{
public:
	~FFbxImporter();
	/**
	 * Returns the importer singleton. It will be created on the first request.
	 */
	UNREALED_API static FFbxImporter* GetInstance();
	static void DeleteInstance();

	static FFbxImporter* GetPreviewInstance();
	static void DeletePreviewInstance();

	/**
	 * Detect if the FBX file has skeletal mesh model. If there is deformer definition, then there is skeletal mesh.
	 * In this function, we don't need to import the scene. But the open process is time-consume if the file is large.
	 *
	 * @param InFilename	FBX file name. 
	 * @return int32 -1 if parse failed; 0 if geometry ; 1 if there are deformers; 2 otherwise
	 */
	int32 GetImportType(const FString& InFilename);

	/**
	 * Get detail infomation in the Fbx scene
	 *
	 * @param Filename Fbx file name
	 * @param SceneInfo return the scene info
	 * @return bool true if get scene info successfully
	 */
	bool GetSceneInfo(FString Filename, FbxSceneInfo& SceneInfo, bool bPreventMaterialNameClash = false);

	/**
	 * Initialize Fbx file for import.
	 *
	 * @param Filename
	 * @param bParseStatistics
	 * @return bool
	 */
	bool OpenFile( FString Filename, bool bParseStatistics, bool bForSceneInfo = false );
	
	/**
	 * Import Fbx file.
	 *
	 * @param Filename
	 * @return bool
	 */
	bool ImportFile(FString Filename, bool bPreventMaterialNameClash = false);
	
	/**
	 * Convert the scene from the current options.
	 * The scene will be converted to RH -Y or RH X depending if we force a front X axis or not
	 */
	void ConvertScene();

	/**
	 * Attempt to load an FBX scene from a given filename.
	 *
	 * @param Filename FBX file name to import.
	 * @returns true on success.
	 */
	UNREALED_API bool ImportFromFile(const FString& Filename, const FString& Type, bool bPreventMaterialNameClash = false);

	/**
	 * Retrieve the FBX loader's error message explaining its failure to read a given FBX file.
	 * Note that the message should be valid even if the parser is successful and may contain warnings.
	 *
	 * @ return TCHAR*	the error message
	 */
	const TCHAR* GetErrorMessage() const
	{
		return *ErrorMessage;
	}

	/**
	 * Retrieve the object inside the FBX scene from the name
	 *
	 * @param ObjectName	Fbx object name
	 * @param Root	Root node, retrieve from it
	 * @return FbxNode*	Fbx object node
	 */
	FbxNode* RetrieveObjectFromName(const TCHAR* ObjectName, FbxNode* Root = NULL);

	/**
	* Find the first node containing a mesh attribute for the specified LOD index.
	*
	* @param NodeLodGroup	The LOD group fbx node
	* @param LodIndex		The index of the LOD we search the mesh node
	*/
	FbxNode* FindLODGroupNode(FbxNode* NodeLodGroup, int32 LodIndex, FbxNode *NodeToFind = nullptr);

	/**
	* Find the all the node containing a mesh attribute for the specified LOD index.
	*
	* @param OutNodeInLod   All the mesh node under the lod group
	* @param NodeLodGroup	The LOD group fbx node
	* @param LodIndex		The index of the LOD we search the mesh node
	*/
	UNREALED_API void FindAllLODGroupNode(TArray<FbxNode*> &OutNodeInLod, FbxNode* NodeLodGroup, int32 LodIndex);

	/**
	* Find the first parent node containing a eLODGroup attribute.
	*
	* @param ParentNode		The node where to start the search.
	*/
	FbxNode *RecursiveFindParentLodGroup(FbxNode *ParentNode);

	/**
	 * Creates a static mesh with the given name and flags, imported from within the FBX scene.
	 * @param InParent
	 * @param Node	Fbx Node to import
	 * @param Name	the Unreal Mesh name after import
	 * @param Flags
	 * @param InStaticMesh	if LODIndex is not 0, this is the base mesh object. otherwise is NULL
	 * @param LODIndex	 LOD level to import to
	 *
	 * @returns UObject*	the UStaticMesh object.
	 */
	UNREALED_API UStaticMesh* ImportStaticMesh(UObject* InParent, FbxNode* Node, const FName& Name, EObjectFlags Flags, UFbxStaticMeshImportData* ImportData, UStaticMesh* InStaticMesh = NULL, int LODIndex = 0, void *ExistMeshDataPtr = nullptr);

	/**
	* Creates a static mesh from all the meshes in FBX scene with the given name and flags.
	*
	* @param InParent
	* @param MeshNodeArray	Fbx Nodes to import
	* @param Name	the Unreal Mesh name after import
	* @param Flags
	* @param InStaticMesh	if LODIndex is not 0, this is the base mesh object. otherwise is NULL
	* @param LODIndex	 LOD level to import to
	* @param OrderedMaterialNames  If not null, the original fbx ordered materials name will be use to reorder the section of the mesh we currently import
	*
	* @returns UObject*	the UStaticMesh object.
	*/
	UNREALED_API UStaticMesh* ImportStaticMeshAsSingle(UObject* InParent, TArray<FbxNode*>& MeshNodeArray, const FName InName, EObjectFlags Flags, UFbxStaticMeshImportData* TemplateImportData, UStaticMesh* InStaticMesh, int LODIndex = 0, void *ExistMeshDataPtr = nullptr);

	/**
	* Finish the import of the staticmesh after all LOD have been process (cannot be call before all LOD are imported). There is two main operation done by this function
	* 1. Build the staticmesh render data
	* 2. Reorder the material array to follow the fbx file material order
	*/
	UNREALED_API void PostImportStaticMesh(UStaticMesh* StaticMesh, TArray<FbxNode*>& MeshNodeArray);
    
	static void UpdateStaticMeshImportData(UStaticMesh *StaticMesh, UFbxStaticMeshImportData* StaticMeshImportData);
	static void UpdateSkeletalMeshImportData(USkeletalMesh *SkeletalMesh, UFbxSkeletalMeshImportData* SkeletalMeshImportData, int32 SpecificLod, TArray<FName> *ImportMaterialOriginalNameData, TArray<FImportMeshLodSectionsData> *ImportMeshLodData);
	void ImportStaticMeshGlobalSockets( UStaticMesh* StaticMesh );
	void ImportStaticMeshLocalSockets( UStaticMesh* StaticMesh, TArray<FbxNode*>& MeshNodeArray);

	/**
	 * re-import Unreal static mesh from updated Fbx file
	 * if the Fbx mesh is in LODGroup, the LOD of mesh will be updated
	 *
	 * @param Mesh the original Unreal static mesh object
	 * @return UObject* the new Unreal mesh object
	 */
	UStaticMesh* ReimportStaticMesh(UStaticMesh* Mesh, UFbxStaticMeshImportData* TemplateImportData);

	/**
	* re-import Unreal static mesh from updated scene Fbx file
	* if the Fbx mesh is in LODGroup, the LOD of mesh will be updated
	*
	* @param Mesh the original Unreal static mesh object
	* @return UObject* the new Unreal mesh object
	*/
	UStaticMesh* ReimportSceneStaticMesh(uint64 FbxNodeUniqueId, uint64 FbxMeshUniqueId, UStaticMesh* Mesh, UFbxStaticMeshImportData* TemplateImportData);
	
	/**
	* re-import Unreal skeletal mesh from updated Fbx file
	* If the Fbx mesh is in LODGroup, the LOD of mesh will be updated.
	* If the FBX mesh contains morph, the morph is updated.
	* Materials, textures and animation attached in the FBX mesh will not be updated.
	*
	* @param Mesh the original Unreal skeletal mesh object
	* @return UObject* the new Unreal mesh object
	*/
	USkeletalMesh* ReimportSkeletalMesh(USkeletalMesh* Mesh, UFbxSkeletalMeshImportData* TemplateImportData, uint64 SkeletalMeshFbxUID = 0xFFFFFFFFFFFFFFFF, TArray<FbxNode*> *OutSkeletalMeshArray = nullptr);

	/**
	 * Creates a skeletal mesh from Fbx Nodes with the given name and flags, imported from within the FBX scene.
	 * These Fbx Nodes bind to same skeleton. We need to bind them to one skeletal mesh.
	 *
	 * @param InParent
	 * @param NodeArray	Fbx Nodes to import
	 * @param Name	the Unreal Mesh name after import
	 * @param Flags
	 * @param FbxShapeArray	Fbx Morph objects.
	 * @param OutData - Optional import data to populate
	 * @param bCreateRenderData - Whether or not skeletal mesh rendering data will be created.
	 * @param OrderedMaterialNames  If not null, the original fbx ordered materials name will be use to reorder the section of the mesh we currently import
	 *
	 * @return The USkeletalMesh object created
	 */

	class FImportSkeletalMeshArgs
	{
	public:
		FImportSkeletalMeshArgs()
			: InParent(nullptr)
			, NodeArray()
			, Name(NAME_None)
			, Flags(RF_NoFlags)
			, TemplateImportData(nullptr)
			, LodIndex(0)
			, bCancelOperation(nullptr)
			, FbxShapeArray(nullptr)
			, OutData(nullptr)
			, bCreateRenderData(true)
			, OrderedMaterialNames(nullptr)
			, ImportMaterialOriginalNameData(nullptr)
			, ImportMeshSectionsData(nullptr)
		{}

		UObject* InParent;
		TArray<FbxNode*> NodeArray;
		FName Name;
		EObjectFlags Flags;
		UFbxSkeletalMeshImportData* TemplateImportData;
		int32 LodIndex;
		bool* bCancelOperation;
		TArray<FbxShape*> *FbxShapeArray;
		FSkeletalMeshImportData* OutData;
		bool bCreateRenderData;
		TArray<FName> *OrderedMaterialNames;

		TArray<FName> *ImportMaterialOriginalNameData;
		FImportMeshLodSectionsData *ImportMeshSectionsData;
	};

	UNREALED_API USkeletalMesh* ImportSkeletalMesh(FImportSkeletalMeshArgs &ImportSkeletalMeshArgs);

	/**
	 * Add to the animation set, the animations contained within the FBX scene, for the given skeletal mesh
	 *
	 * @param Skeleton	Skeleton that the animation belong to
	 * @param SortedLinks	skeleton nodes which are sorted
	 * @param Filename	Fbx file name
	 * @param NodeArray node array of FBX meshes
	 */
	UAnimSequence* ImportAnimations(USkeleton* Skeleton, UObject* Outer, TArray<FbxNode*>& SortedLinks, const FString& Name, UFbxAnimSequenceImportData* TemplateImportData, TArray<FbxNode*>& NodeArray);

	/**
	 * Get Animation Time Span - duration of the animation
	 */
	FbxTimeSpan GetAnimationTimeSpan(FbxNode* RootNode, FbxAnimStack* AnimStack, int32 ResampleRate);

	/**
	 * Import one animation from CurAnimStack
	 *
	 * @param Skeleton	Skeleton that the animation belong to
	 * @param DestSeq 	Sequence it's overwriting data to
	 * @param Filename	Fbx file name	(not whole path)
	 * @param SortedLinks	skeleton nodes which are sorted
	 * @param NodeArray node array of FBX meshes
	 * @param CurAnimStack 	Animation Data
	 * @param ResampleRate	Resample Rate for data
	 * @param AnimTimeSpan	AnimTimeSpan
	 */
	bool ImportAnimation(USkeleton* Skeleton, UAnimSequence* DestSeq, const FString& FileName, TArray<FbxNode*>& SortedLinks, TArray<FbxNode*>& NodeArray, FbxAnimStack* CurAnimStack, const int32 ResampleRate, const FbxTimeSpan AnimTimeSpan);
	/**
	 * Calculate Max Sample Rate - separate out of the original ImportAnimations
	 *
	 * @param SortedLinks	skeleton nodes which are sorted
	 * @param NodeArray node array of FBX meshes
	 */
	int32 GetMaxSampleRate(TArray<FbxNode*>& SortedLinks, TArray<FbxNode*>& NodeArray);
	/**
	 * Validate Anim Stack - multiple check for validating animstack
	 *
	 * @param SortedLinks	skeleton nodes which are sorted
	 * @param NodeArray node array of FBX meshes
	 * @param CurAnimStack 	Animation Data
	 * @param ResampleRate	Resample Rate for data
	 * @param AnimTimeSpan	AnimTimeSpan	 
	 */	
	bool ValidateAnimStack(TArray<FbxNode*>& SortedLinks, TArray<FbxNode*>& NodeArray, FbxAnimStack* CurAnimStack, int32 ResampleRate, bool bImportMorph, FbxTimeSpan &AnimTimeSpan);

	/**
	 * Import Fbx Morph object for the Skeletal Mesh.
	 * In Fbx, morph object is a property of the Fbx Node.
	 *
	 * @param SkelMeshNodeArray - Fbx Nodes that the base Skeletal Mesh construct from
	 * @param BaseSkelMesh - base Skeletal Mesh
	 * @param LODIndex - LOD index
	 */
//#nv begin #Blast Engine export
	UNREALED_API void ImportFbxMorphTarget(TArray<FbxNode*> &SkelMeshNodeArray, USkeletalMesh* BaseSkelMesh, UObject* Parent, int32 LODIndex, const FSkeletalMeshImportData &BaseSkeletalMeshImportData);
//nv end

	/**
	 * Import LOD object for skeletal mesh
	 *
	 * @param InSkeletalMesh - LOD mesh object
	 * @param BaseSkeletalMesh - base mesh object
	 * @param DesiredLOD - LOD level
	 * @param bNeedToReregister - if true, re-register this skeletal mesh to shut down the skeletal mesh component that is previewing this mesh. 
									But you can set this to false when in the first loading before rendering this mesh for a performance issue 
	   @param ReregisterAssociatedComponents - if NULL, just re-registers all SkinnedMeshComponents but if you set the specific components, will only re-registers those components
	 */
//#nv begin #Blast Engine export
	UNREALED_API bool ImportSkeletalMeshLOD(USkeletalMesh* InSkeletalMesh, USkeletalMesh* BaseSkeletalMesh, int32 DesiredLOD, bool bNeedToReregister = true, TArray<UActorComponent*>* ReregisterAssociatedComponents = NULL, UFbxSkeletalMeshImportData* TemplateImportData = nullptr);
//nv end

	/**
	 * Empties the FBX scene, releasing its memory.
	 * Currently, we can't release KFbxSdkManager because Fbx Sdk2010.2 has a bug that FBX can only has one global sdkmanager.
	 * From Fbx Sdk2011, we can create multiple KFbxSdkManager, then we can release it.
	 */
	UNREALED_API void ReleaseScene();

	/**
	 * If the node model is a collision model, then fill it into collision model list
	 *
	 * @param Node Fbx node
	 * @return true if the node is a collision model
	 */
	bool FillCollisionModelList(FbxNode* Node);

	/**
	 * Import collision models for one static mesh if it has collision models
	 *
	 * @param StaticMesh - mesh object to import collision models
	 * @param NodeName - name of Fbx node that the static mesh constructed from
	 * @return return true if the static mesh has collision model and import successfully
	 */
	bool ImportCollisionModels(UStaticMesh* StaticMesh, const FbxString& NodeName);

	//help
	ANSICHAR* MakeName(const ANSICHAR* name);
	FString MakeString(const ANSICHAR* Name);
	FName MakeNameForMesh(FString InName, FbxObject* FbxObject);

	// meshes
	
	/**
	* Get all Fbx skeletal mesh objects in the scene. these meshes are grouped by skeleton they bind to
	*
	* @param Node Root node to find skeletal meshes
	* @param outSkelMeshArray return Fbx meshes they are grouped by skeleton
	*/
	UNREALED_API void FillFbxSkelMeshArrayInScene(FbxNode* Node, TArray< TArray<FbxNode*>* >& outSkelMeshArray, bool ExpandLOD, bool bForceFindRigid = false);
	
	/**
	 * Find FBX meshes that match Unreal skeletal mesh according to the bone of mesh
	 *
	 * @param FillInMesh     Unreal skeletal mesh
	 * @param bExpandLOD     flag that if expand FBX LOD group when get the FBX node
	 * @param OutFBXMeshNodeArray  return FBX mesh nodes that match the Unreal skeletal mesh
	 * 
	 * @return the root bone that bind to the FBX skeletal meshes
	 */
	FbxNode* FindFBXMeshesByBone(const FName& RootBoneName, bool bExpandLOD, TArray<FbxNode*>& OutFBXMeshNodeArray);
	
	/**
	* Get mesh count (including static mesh and skeletal mesh, except collision models) and find collision models
	*
	* @param Node			Root node to find meshes
	* @param bCountLODs		Whether or not to count meshes in LOD groups
	* @return int32 mesh count
	*/
	int32 GetFbxMeshCount(FbxNode* Node,bool bCountLODs, int32 &OutNumLODGroups );
	
	/**
	* Fill the collision models array by going through all mesh node recursively
	*
	* @param Node Root node to find collision meshes
	*/
	UNREALED_API void FillFbxCollisionMeshArray(FbxNode* Node);

	/**
	* Get all Fbx mesh objects
	*
	* @param Node Root node to find meshes
	* @param outMeshArray return Fbx meshes
	*/
	UNREALED_API void FillFbxMeshArray(FbxNode* Node, TArray<FbxNode*>& outMeshArray, UnFbx::FFbxImporter* FFbxImporter);
	
	/**
	* Get all Fbx mesh objects not under a LOD group and all LOD group node
	*
	* @param Node Root node to find meshes
	* @param outLODGroupArray return Fbx LOD group
	* @param outMeshArray return Fbx meshes with no LOD group
	*/
	UNREALED_API void FillFbxMeshAndLODGroupArray(FbxNode* Node, TArray<FbxNode*>& outLODGroupArray, TArray<FbxNode*>& outMeshArray);

	/**
	* Fill FBX skeletons to OutSortedLinks recursively
	*
	* @param Link Fbx node of skeleton root
	* @param OutSortedLinks
	*/
	void RecursiveBuildSkeleton(FbxNode* Link, TArray<FbxNode*>& OutSortedLinks);

	/**
	 * Fill FBX skeletons to OutSortedLinks
	 *
	 * @param ClusterArray Fbx clusters of FBX skeletal meshes
	 * @param OutSortedLinks
	 */
	void BuildSkeletonSystem(TArray<FbxCluster*>& ClusterArray, TArray<FbxNode*>& OutSortedLinks);

	/**
	 * Get Unreal skeleton root from the FBX skeleton node.
	 * Mesh and dummy can be used as skeleton.
	 *
	 * @param Link one FBX skeleton node
	 */
	FbxNode* GetRootSkeleton(FbxNode* Link);
	
	/**
	 * Get the object of import options
	 *
	 * @return FBXImportOptions
	 */
	UNREALED_API FBXImportOptions* GetImportOptions() const;

	/*
	* This function show a dialog to let the user know what will be change if the fbx is imported
	*/
	void ShowFbxReimportPreview(UObject *ReimportObj, UFbxImportUI* ImportUI, const FString& FullPath);

	/*
	* Function use to retrieve general fbx information for the preview
	*/
	void FillGeneralFbxFileInformation(void *GeneralInfoPtr);

	/** helper function **/
	UNREALED_API static void DumpFBXNode(FbxNode* Node);

	/**
	 * Apply asset import settings for transform to an FBX node
	 *
	 * @param Node Node to apply transform settings too
	 * @param AssetData the asset data object to get transform data from
	 */
	void ApplyTransformSettingsToFbxNode(FbxNode* Node, UFbxAssetImportData* AssetData);

	/**
	 * Remove asset import settings for transform to an FBX node
	 *
	 * @param Node Node to apply transform settings too
	 * @param AssetData the asset data object to get transform data from
	 */
	void RemoveTransformSettingsFromFbxNode(FbxNode* Node, UFbxAssetImportData* AssetData);

	/**
	 * Populate the given matrix with the correct information for the asset data, in
	 * a format that matches FBX internals or without conversion
	 *
	 * @param OutMatrix The matrix to fill
	 * @param AssetData The asset data to extract the transform info from
	 */
	void BuildFbxMatrixForImportTransform(FbxAMatrix& OutMatrix, UFbxAssetImportData* AssetData);

	/**
	 * Import FbxCurve to Curve
	 */
	bool ImportCurve(const FbxAnimCurve* FbxCurve, FRichCurve& RichCurve, const FbxTimeSpan &AnimTimeSpan, const float ValueScale = 1.f) const;

	/**
	 * Merge all layers of one AnimStack to one layer.
	 *
	 * @param AnimStack     AnimStack which layers will be merged
	 * @param ResampleRate  resample rate for the animation
	 */
	void MergeAllLayerAnimation(FbxAnimStack* AnimStack, int32 ResampleRate);

private:
	/**
	* This function fill the last imported Material name. Those named are used to reorder the mesh sections
	* during a re-import. In case material names use the skinxx workflow the LastImportedMaterialNames array
	* will be empty to let the system reorder the mesh sections with the skinxx workflow.
	*
	* @param LastImportedMaterialNames	This array will be filled with the BaseSkelMesh Material original imported names
	* @param BaseSkelMesh				Skeletal mesh holding the last imported material names. If null the LastImportedMaterialNames will be empty;
	* @param OrderedMaterialNames		if not null, it will be used to fill the LastImportedMaterialNames array. except if the names are using the _skinxx workflow
	*/
	void FillLastImportMaterialNames(TArray<FName> &LastImportedMaterialNames, USkeletalMesh* BaseSkelMesh, TArray<FName> *OrderedMaterialNames);

	/**
	* Verify that all meshes are also reference by a fbx hierarchy node. If it found some Geometry
	* not reference it will add a tokenized error.
	*/
	void ValidateAllMeshesAreReferenceByNodeAttribute();

	/**
	* Recursive search for a node having a mesh attribute
	*
	* @param Node	The node from which we start the search for the first node containing a mesh attribute
	*/
	FbxNode *RecursiveGetFirstMeshNode(FbxNode* Node, FbxNode* NodeToFind = nullptr);

	/**
	* Recursive search for a node having a mesh attribute
	*
	* @param Node	The node from which we start the search for the first node containing a mesh attribute
	*/
	void RecursiveGetAllMeshNode(TArray<FbxNode *> &OutAllNode, FbxNode* Node);

	/**
	 * ActorX plug-in can export mesh and dummy as skeleton.
	 * For the mesh and dummy in the skeleton hierarchy, convert them to FBX skeleton.
	 *
	 * @param Node          root skeleton node
	 * @param SkelMeshes    skeletal meshes that bind to this skeleton
	 * @param bImportNestedMeshes	if true we will import meshes nested in bone hierarchies instead of converting them to bones
	 */
	void RecursiveFixSkeleton(FbxNode* Node, TArray<FbxNode*> &SkelMeshes, bool bImportNestedMeshes );
	
	/**
	* Get all Fbx skeletal mesh objects which are grouped by skeleton they bind to
	*
	* @param Node Root node to find skeletal meshes
	* @param outSkelMeshArray return Fbx meshes they are grouped by skeleton
	* @param SkeletonArray
	* @param ExpandLOD flag of expanding LOD to get each mesh
	*/
	void RecursiveFindFbxSkelMesh(FbxNode* Node, TArray< TArray<FbxNode*>* >& outSkelMeshArray, TArray<FbxNode*>& SkeletonArray, bool ExpandLOD);
	
	/**
	* Get all Fbx rigid mesh objects which are grouped by skeleton hierarchy
	*
	* @param Node Root node to find skeletal meshes
	* @param outSkelMeshArray return Fbx meshes they are grouped by skeleton hierarchy
	* @param SkeletonArray
	* @param ExpandLOD flag of expanding LOD to get each mesh
	*/
	void RecursiveFindRigidMesh(FbxNode* Node, TArray< TArray<FbxNode*>* >& outSkelMeshArray, TArray<FbxNode*>& SkeletonArray, bool ExpandLOD);

	/**
	 * Import Fbx Morph object for the Skeletal Mesh.  Each morph target import processing occurs in a different thread 
	 *
	 * @param SkelMeshNodeArray - Fbx Nodes that the base Skeletal Mesh construct from
	 * @param BaseSkelMesh - base Skeletal Mesh
	 * @param LODIndex - LOD index of the skeletal mesh
	 */
	void ImportMorphTargetsInternal( TArray<FbxNode*>& SkelMeshNodeArray, USkeletalMesh* BaseSkelMesh, UObject* Parent, int32 LODIndex, const FSkeletalMeshImportData &BaseSkeletalMeshImportData);

	/**
	* sub-method called from ImportSkeletalMeshLOD method
	*
	* @param InSkeletalMesh - newly created mesh used as LOD
	* @param BaseSkeletalMesh - the destination mesh object 
	* @param DesiredLOD - the LOD index to import into. A new LOD entry is created if one doesn't exist
	*/
	void InsertNewLODToBaseSkeletalMesh(USkeletalMesh* InSkeletalMesh, USkeletalMesh* BaseSkeletalMesh, int32 DesiredLOD, UFbxSkeletalMeshImportData* TemplateImportData);

	/**
	* Method used to verify if the geometry is valid. For example, if the bounding box is tiny we should warn
	* @param StaticMesh - The imported static mesh which we'd like to verify
	*/
	void VerifyGeometry(UStaticMesh* StaticMesh);

	/**
	* When there is some materials with the same name we add a clash suffixe _ncl1_x.
	* Example, if we have 3 materials name shader we will get (shader, shader_ncl1_1, shader_ncl1_2).
	*/
	void FixMaterialClashName();

public:
	// current Fbx scene we are importing. Make sure to release it after import
	FbxScene* Scene;
	FBXImportOptions* ImportOptions;

	//We cache the hash of the file when we open the file. This is to avoid calculating the hash many time when importing many asset in one fbx file.
	FMD5Hash Md5Hash;

	struct FFbxMaterial
	{
		FbxSurfaceMaterial* FbxMaterial;
		UMaterialInterface* Material;

		FString GetName() const { return FbxMaterial ? ANSI_TO_TCHAR(FbxMaterial->GetName()) : TEXT("None"); }
	};

	/**
	* Make material Unreal asset name from the Fbx material
	*
	* @param FbxMaterial Material from the Fbx node
	* @return Sanitized asset name
	*/
	FString GetMaterialFullName(FbxSurfaceMaterial& FbxMaterial);

	FbxGeometryConverter* GetGeometryConverter() { return GeometryConverter; }

protected:
	enum IMPORTPHASE
	{
		NOTSTARTED,
		FILEOPENED,
		IMPORTED
	};
	
	static TSharedPtr<FFbxImporter> StaticInstance;
	static TSharedPtr<FFbxImporter> StaticPreviewInstance;
	
	//make sure we are not applying two time the option transform to the same node
	TArray<FbxNode*> TransformSettingsToFbxApply;

	// scene management
	FFbxDataConverter Converter;
	FbxGeometryConverter* GeometryConverter;
	FbxManager* SdkManager;
	FbxImporter* Importer;
	IMPORTPHASE CurPhase;
	FString ErrorMessage;
	// base path of fbx file
	FString FileBasePath;
	TWeakObjectPtr<UObject> Parent;
	FString FbxFileVersion;

	//Original File Info
	FbxAxisSystem FileAxisSystem;
	FbxSystemUnit FileUnitSystem;

	// Flag that the mesh is the first mesh to import in current FBX scene
	// FBX scene may contain multiple meshes, importer can import them at one time.
	// Initialized as true when start to import a FBX scene
	bool bFirstMesh;
	
	//Value is true if the file was create by blender
	EFbxCreator FbxCreator;
	
	// Set when importing skeletal meshes if the merge bones step fails. Used to track
	// YesToAll and NoToAll for an entire scene
	EAppReturnType::Type LastMergeBonesChoice;

	/**
	 * Collision model list. The key is fbx node name
	 * If there is an collision model with old name format, the key is empty string("").
	 */
	FbxMap<FbxString, TSharedPtr< FbxArray<FbxNode* > > > CollisionModels;
	 
	FFbxImporter();


	/**
	 * Set up the static mesh data from Fbx Mesh.
	 *
	 * @param StaticMesh Unreal static mesh object to fill data into
	 * @param LODIndex	LOD level to set up for StaticMesh
	 * @return bool true if set up successfully
	 */
	bool BuildStaticMeshFromGeometry(FbxNode* Node, UStaticMesh* StaticMesh, TArray<FFbxMaterial>& MeshMaterials, int LODIndex, FRawMesh& RawMesh,
									 EVertexColorImportOption::Type VertexColorImportOption, const TMap<FVector, FColor>& ExistingVertexColorData, const FColor& VertexOverrideColor);
	
	/**
	 * Clean up for destroy the Importer.
	 */
	void CleanUp();

	/**
	* Compute the global matrix for Fbx Node
	* If we import scene it will return identity plus the pivot if we turn the bake pivot option
	*
	* @param Node	Fbx Node
	* @return KFbxXMatrix*	The global transform matrix
	*/
	FbxAMatrix ComputeTotalMatrix(FbxNode* Node);
	
	/**
	* Compute the matrix for skeletal Fbx Node
	* If we import don't import a scene it will call ComputeTotalMatrix with Node as the parameter. If we import a scene
	* it will return the relative transform between the RootSkeletalNode and Node.
	*
	* @param Node	Fbx Node
	* @param Node	Fbx RootSkeletalNode
	* @return KFbxXMatrix*	The global transform matrix
	*/
	FbxAMatrix ComputeSkeletalMeshTotalMatrix(FbxNode* Node, FbxNode *RootSkeletalNode);
	/**
	 * Check if there are negative scale in the transform matrix and its number is odd.
	 * @return bool True if there are negative scale and its number is 1 or 3. 
	 */
	bool IsOddNegativeScale(FbxAMatrix& TotalMatrix);

	// various actors, current the Fbx importer don't importe them
	/**
	 * Import Fbx light
	 *
	 * @param FbxLight fbx light object 
	 * @param World in which to create the light
	 * @return ALight*
	 */
	ALight* CreateLight(FbxLight* InLight, UWorld* InWorld );	
	/**
	* Import Light detail info
	*
	* @param FbxLight
	* @param UnrealLight
	* @return  bool
	*/
	bool FillLightComponent(FbxLight* Light, ULightComponent* UnrealLight);
	/**
	* Import Fbx Camera object
	*
	* @param FbxCamera Fbx camera object
	* @param World in which to create the camera
	* @return ACameraActor*
	*/
	ACameraActor* CreateCamera(FbxCamera* InCamera, UWorld* InWorld);

	// meshes
	/**
	* Fill skeletal mesh data from Fbx Nodes.  If this function needs to triangulate the mesh, then it could invalidate the
	* original FbxMesh pointer.  Hence FbxMesh is a reference so this function can set the new pointer if need be.  
	*
	* @param ImportData object to store skeletal mesh data
	* @param FbxMesh	Fbx mesh object belonging to Node
	* @param FbxSkin	Fbx Skin object belonging to FbxMesh
	* @param FbxShape	Fbx Morph object, if not NULL, we are importing a morph object.
	* @param SortedLinks    Fbx Links(bones) of this skeletal mesh
	* @param FbxMatList  All material names of the skeletal mesh
	* @param RootNode       The skeletal mesh root fbx node.
	*
	* @returns bool*	true if import successfully.
	*/
    bool FillSkelMeshImporterFromFbx(FSkeletalMeshImportData& ImportData, FbxMesh*& Mesh, FbxSkin* Skin, 
										FbxShape* Shape, TArray<FbxNode*> &SortedLinks, const TArray<FbxSurfaceMaterial*>& FbxMaterials, FbxNode *RootNode);
public:

	/**
	* Fill FSkeletalMeshIMportData from Fbx Nodes and FbxShape Array if exists.  
	*
	* @param NodeArray	Fbx node array to look at
	* @param TemplateImportData template import data 
	* @param FbxShapeArray	Fbx Morph object, if not NULL, we are importing a morph object.
	* @param OutData    FSkeletalMeshImportData output data
	*
	* @returns bool*	true if import successfully.
	*/
	bool FillSkeletalMeshImportData(TArray<FbxNode*>& NodeArray, UFbxSkeletalMeshImportData* TemplateImportData, TArray<FbxShape*> *FbxShapeArray, FSkeletalMeshImportData* OutData, TArray<FName> &LastImportedMaterialNames);

protected:
	/**
	* Fill the Points in FSkeletalMeshIMportData from a Fbx Node and a FbxShape if it exists.
	*
	* @param OutData    FSkeletalMeshImportData output data
	* @param RootNode	The root node of the Fbx
	* @param Node		The node to get the points from
	* @param FbxShape	Fbx Morph object, if not NULL, we are importing a morph object.
	*
	* @returns bool		true if import successfully.
	*/
	bool FillSkeletalMeshImportPoints(FSkeletalMeshImportData* OutData, FbxNode* RootNode, FbxNode* Node, FbxShape* FbxShape);

	/**
	* Fill the Points in FSkeletalMeshIMportData from Fbx Nodes and FbxShape Array if it exists.
	*
	* @param OutData		FSkeletalMeshImportData output data
	* @param NodeArray		Fbx node array to look at
	* @param FbxShapeArray	Fbx Morph object, if not NULL, we are importing a morph object.
	* @param ModifiedPoints	Set of points indices for which we've modified the value in OutData
	*
	* @returns bool			true if import successfully.
	*/
	bool GatherPointsForMorphTarget(FSkeletalMeshImportData* OutData, TArray<FbxNode*>& NodeArray, TArray< FbxShape* >* FbxShapeArray, TSet<uint32>& ModifiedPoints);

	/**
	 * Import bones from skeletons that NodeArray bind to.
	 *
	 * @param NodeArray Fbx Nodes to import, they are bound to the same skeleton system
	 * @param ImportData object to store skeletal mesh data
	 * @param OutSortedLinks return all skeletons sorted by depth traversal
	 * @param bOutDiffPose
	 * @param bDisableMissingBindPoseWarning
	 * @param bUseTime0AsRefPose	in/out - Use Time 0 as Ref Pose 
	 */
	bool ImportBone(TArray<FbxNode*>& NodeArray, FSkeletalMeshImportData &ImportData, UFbxSkeletalMeshImportData* TemplateData, TArray<FbxNode*> &OutSortedLinks, bool& bOutDiffPose, bool bDisableMissingBindPoseWarning, bool & bUseTime0AsRefPose, FbxNode *SkeletalMeshNode);
	
	/**
	 * Skins the control points of the given mesh or shape using either the default pose for skinning or the first frame of the
	 * default animation.  The results are saved as the last X verts in the given FSkeletalMeshBinaryImport
	 *
	 * @param SkelMeshImporter object to store skeletal mesh data
	 * @param FbxMesh	The Fbx mesh object with the control points to skin
	 * @param FbxShape	If a shape (aka morph) is provided, its control points will be used instead of the given meshes
	 * @param bUseT0	If true, then the pose at time=0 will be used instead of the ref pose
	 */
	void SkinControlPointsToPose(FSkeletalMeshImportData &ImportData, FbxMesh* Mesh, FbxShape* Shape, bool bUseT0 );
	
	// anims
	/**
	 * Check if the Fbx node contains animation
	 *
	 * @param Node Fbx node
	 * @return bool true if the Fbx node contains animation.
	 */
	//bool IsAnimated(FbxNode* Node);

	/**
	* Fill each Trace for AnimSequence with Fbx skeleton animation by key
	*
	* @param Node   Fbx skeleton node
	* @param AnimSequence
	* @param TakeName
	* @param bIsRoot if the Fbx skeleton node is root skeleton
	* @param Scale scale factor for this skeleton node
	*/
	bool FillAnimSequenceByKey(FbxNode* Node, UAnimSequence* AnimSequence, const char* TakeName, FbxTime& Start, FbxTime& End, bool bIsRoot, FbxVector4 Scale);
	/*bool CreateMatineeSkeletalAnimation(ASkeletalMeshActor* Actor, UAnimSet* AnimSet);
	bool CreateMatineeAnimation(FbxNode* Node, AActor* Actor, bool bInvertOrient, bool bAddDirectorTrack);*/


	// material
	/**
	 * Import each material Input from Fbx Material
	 *
	 * @param FbxMaterial	Fbx material object
	 * @param UnrealMaterial
	 * @param MaterialProperty The material component to import
	 * @param MaterialInput
	 * @param bSetupAsNormalMap
	 * @param UVSet
	 * @return bool	
	 */
	bool CreateAndLinkExpressionForMaterialProperty(	FbxSurfaceMaterial& FbxMaterial,
														UMaterial* UnrealMaterial,
														const char* MaterialProperty ,
														FExpressionInput& MaterialInput, 
														bool bSetupAsNormalMap,
														TArray<FString>& UVSet,
														const FVector2D& Location );
	/**
	* Create and link texture to the right material parameter value
	*
	* @param FbxMaterial	Fbx material object
	* @param UnrealMaterial
	* @param MaterialProperty The material component to import
	* @param ParameterValue
	* @param bSetupAsNormalMap
	* @return bool
	*/
	bool LinkMaterialProperty(FbxSurfaceMaterial& FbxMaterial,
		UMaterialInstanceConstant* UnrealMaterial,
		const char* MaterialProperty,
		FName ParameterValue,
		bool bSetupAsNormalMap);
	/**
	 * Add a basic white diffuse color if no expression is linked to diffuse input.
	 *
	 * @param unMaterial Unreal material object.
	 */
	void FixupMaterial( FbxSurfaceMaterial& FbxMaterial, UMaterial* unMaterial);
	
	/**
	 * Get material mapping array according "Skinxx" flag in material name
	 *
	 * @param FSkeletalMeshBinaryImport& The unreal skeletal mesh.
	 */
	void SetMaterialSkinXXOrder(FSkeletalMeshImportData& ImportData);
	
	void SetMaterialOrderByName(FSkeletalMeshImportData& ImportData, TArray<FName> LastImportedMaterialNames);

	/**
	* Make sure there is no unused material in the raw data. Unused material are material refer by node but not refer by any geometry face
	*
	* @param FSkeletalMeshBinaryImport& The unreal skeletal mesh.
	*/
	void CleanUpUnusedMaterials(FSkeletalMeshImportData& ImportData);

	/**
	 * Create materials from Fbx node.
	 * Only setup channels that connect to texture, and setup the UV coordinate of texture.
	 * If diffuse channel has no texture, one default node will be created with constant.
	 *
	 * @param FbxNode  Fbx node
	 * @param outMaterials Unreal Materials we created
	 * @param UVSets UV set name list
	 * @return int32 material count that created from the Fbx node
	 */
	int32 CreateNodeMaterials(FbxNode* FbxNode, TArray<UMaterialInterface*>& outMaterials, TArray<FString>& UVSets, bool bForSkeletalMesh);

	/**
	 * Create Unreal material from Fbx material.
	 * Only setup channels that connect to texture, and setup the UV coordinate of texture.
	 * If diffuse channel has no texture, one default node will be created with constant.
	 *
	 * @param KFbxSurfaceMaterial*  Fbx material
	 * @param outMaterials Unreal Materials we created
	 * @param outUVSets
	 */
	void CreateUnrealMaterial(FbxSurfaceMaterial& FbxMaterial, TArray<UMaterialInterface*>& OutMaterials, TArray<FString>& UVSets, bool bForSkeletalMesh);

	/**
	 * Visit all materials of one node, import textures from materials.
	 *
	 * @param Node FBX node.
	 */
	void ImportTexturesFromNode(FbxNode* Node);
	
	/**
	 * Generate Unreal texture object from FBX texture.
	 *
	 * @param FbxTexture FBX texture object to import.
	 * @param bSetupAsNormalMap Flag to import this texture as normal map.
	 * @return UTexture* Unreal texture object generated.
	 */
	UTexture* ImportTexture(FbxFileTexture* FbxTexture, bool bSetupAsNormalMap);
	
	/**
	 *
	 *
	 * @param
	 * @return UMaterial*
	 */
	//UMaterial* GetImportedMaterial(KFbxSurfaceMaterial* pMaterial);

	/**
	* Check if the meshes in FBX scene contain smoothing group info.
	* It's enough to only check one of mesh in the scene because "Export smoothing group" option affects all meshes when export from DCC.
	* To ensure only check one time, use flag bFirstMesh to record if this is the first mesh to check.
	*
	* @param FbxMesh Fbx mesh to import
	*/
	void CheckSmoothingInfo(FbxMesh* FbxMesh);

	/**
	 * check if two faces belongs to same smoothing group
	 *
	 * @param ImportData
	 * @param Face1 one face of the skeletal mesh
	 * @param Face2 another face
	 * @return bool true if two faces belongs to same group
	 */
	bool FacesAreSmoothlyConnected( FSkeletalMeshImportData &ImportData, int32 Face1, int32 Face2 );

	/**
	 * Make un-smooth faces work.
	 *
	 * @param ImportData
	 * @return int32 number of points that added when process unsmooth faces
	*/
	int32 DoUnSmoothVerts(FSkeletalMeshImportData &ImportData, bool bDuplicateUnSmoothWedges = true);
	
	/**
	* Fill the FbxNodeInfo structure recursively to reflect the FbxNode hierarchy. The result will be an array sorted with the parent first
	*
	* @param SceneInfo   The scene info to modify
	* @param Parent      The parent FbxNode
	* @param ParentInfo  The parent FbxNodeInfo
	*/
	void TraverseHierarchyNodeRecursively(FbxSceneInfo& SceneInfo, FbxNode *ParentNode, FbxNodeInfo &ParentInfo);


	//
	// for sequencer import
	//
public:
	UNREALED_API void PopulateAnimatedCurveData(FFbxCurvesAPI &CurvesAPI);

protected:
	void LoadNodeKeyframeAnimationRecursively(FFbxCurvesAPI &CurvesAPI, FbxNode* NodeToQuery);
	void LoadNodeKeyframeAnimation(FbxNode* NodeToQuery, FFbxCurvesAPI &CurvesAPI);
	void SetupTransformForNode(FbxNode *Node);


	//
	// for matinee export
	//
public:
	/**
	 * Retrieves whether there are any unknown camera instances within the FBX document.
	 */
	UNREALED_API bool HasUnknownCameras( AMatineeActor* InMatineeActor ) const;
	
	/**
	 * Sets the camera creation flag. Call this function before the import in order to enforce
	 * the creation of FBX camera instances that do not exist in the current scene.
	 */
	inline void SetProcessUnknownCameras(bool bCreateMissingCameras)
	{
		bCreateUnknownCameras = bCreateMissingCameras;
	}
	
	/**
	 * Modifies the Matinee actor with the animations found in the FBX document.
	 * 
	 * @return	true, if sucessful
	 */
	UNREALED_API bool ImportMatineeSequence(AMatineeActor* InMatineeActor);


	/** Create a new asset from the package and objectname and class */
	static UObject* CreateAssetOfClass(UClass* AssetClass, FString ParentPackageName, FString ObjectName, bool bAllowReplace = false);

	/* Templated function to create an asset with given package and name */
	template< class T> 
	static T * CreateAsset( FString ParentPackageName, FString ObjectName, bool bAllowReplace = false )
	{
		return (T*)CreateAssetOfClass(T::StaticClass(), ParentPackageName, ObjectName, bAllowReplace);
	}

protected:
	bool bCreateUnknownCameras;
	
	/**
	 * Creates a Matinee group for a given actor within a given Matinee actor.
	 */
	UInterpGroupInst* CreateMatineeGroup(AMatineeActor* InMatineeActor, AActor* Actor, FString GroupName);
	/**
	 * Imports a FBX scene node into a Matinee actor group.
	 */
	float ImportMatineeActor(FbxNode* FbxNode, UInterpGroupInst* MatineeGroup);

	/**
	 * Imports an FBX transform curve into a movement subtrack
	 */
	void ImportMoveSubTrack( FbxAnimCurve* FbxCurve, int32 FbxDimension, UInterpTrackMoveAxis* SubTrack, int32 CurveIndex, bool bNegative, FbxAnimCurve* RealCurve, float DefaultVal );

	/**
	 * Imports a FBX animated element into a Matinee track.
	 */
	void ImportMatineeAnimated(FbxAnimCurve* FbxCurve, FInterpCurveVector& Curve, int32 CurveIndex, bool bNegative, FbxAnimCurve* RealCurve, float DefaultVal);
	/**
	 * Imports a FBX camera into properties tracks of a Matinee group for a camera actor.
	 */
	void ImportCamera(ACameraActor* Actor, UInterpGroupInst* MatineeGroup, FbxCamera* Camera);
	/**
	 * Imports a FBX animated value into a property track of a Matinee group.
	 */
	void ImportAnimatedProperty(float* Value, const TCHAR* ValueName, UInterpGroupInst* MatineeGroup, const float FbxValue, FbxProperty Property, bool bImportFOV = false, FbxCamera* Camera = NULL );
	/**
	 * Check if FBX node has transform animation (translation and rotation, not check scale animation)
	 */
	bool IsNodeAnimated(FbxNode* FbxNode, FbxAnimLayer* AnimLayer = NULL);

	/** 
	 * As movement tracks in Unreal cannot have differing interpolation modes for position & rotation,
	 * we consolidate the two modes here.
	 */
	void ConsolidateMovementTrackInterpModes(UInterpTrackMove* MovementTrack);

	/**
	 * Get Unreal Interpolation mode from FBX interpolation mode
	 */
	EInterpCurveMode GetUnrealInterpMode(FbxAnimCurveKey FbxKey);

	/**
	 * Fill up and verify bone names for animation 
	 */
	void FillAndVerifyBoneNames(USkeleton* Skeleton, TArray<FbxNode*>& SortedLinks, TArray<FName> & OutRawBoneNames, FString Filename);
	/**
	 * Is valid animation data
	 */
	bool IsValidAnimationData(TArray<FbxNode*>& SortedLinks, TArray<FbxNode*>& NodeArray, int32& ValidTakeCount);

	/**
	 * Retrieve pose array from bind pose
	 *
	 * Iterate through Scene:Poses, and find valid bind pose for NodeArray, and return those Pose if valid
	 *
	 */
	bool RetrievePoseFromBindPose(const TArray<FbxNode*>& NodeArray, FbxArray<FbxPose*> & PoseArray) const;

public:
	/** Import and set up animation related data from mesh **/
	void SetupAnimationDataFromMesh(USkeletalMesh * SkeletalMesh, UObject* InParent, TArray<FbxNode*>& NodeArray, UFbxAnimSequenceImportData* ImportData, const FString& Filename);

	/** error message handler */
	UNREALED_API void AddTokenizedErrorMessage(TSharedRef<FTokenizedMessage> Error, FName FbxErrorName );
	void ClearTokenizedErrorMessages();
	void FlushToTokenizedErrorMessage(enum EMessageSeverity::Type Severity);

private:
	friend class FFbxLoggerSetter;

	// logger set/clear function
	class FFbxLogger * Logger;
	void SetLogger(class FFbxLogger * InLogger);
	void ClearLogger();

	FImportedMaterialData ImportedMaterialData;

	//Cache to create unique name for mesh. This is use to fix name clash
	TArray<FString> MeshNamesCache;

private:



	/**
	 * Import FbxCurve to anim sequence
	 */
	bool ImportCurveToAnimSequence(class UAnimSequence * TargetSequence, const FString& CurveName, const FbxAnimCurve* FbxCurve, int32 CurveFlags,const FbxTimeSpan AnimTimeSpan, const float ValueScale = 1.f) const;
};


/** message Logger for FBX. Saves all the messages and prints when it's destroyed */
class UNREALED_API FFbxLogger
{
	FFbxLogger();
	~FFbxLogger();

	/** Error messages **/
	TArray<TSharedRef<FTokenizedMessage>> TokenizedErrorMessages;

	/* The logger will show the LogMessage only if at least one TokenizedErrorMessage have a severity of Error or CriticalError*/
	bool ShowLogMessageOnlyIfError;

	friend class FFbxImporter;
	friend class FFbxLoggerSetter;
};

/**
* This class is to make sure Logger isn't used by outside of purpose.
* We add this only top level of functions where it needs to be handled
* if the importer already has logger set, it won't set anymore
*/
class UNREALED_API FFbxLoggerSetter
{
	class FFbxLogger Logger;
	FFbxImporter * Importer;

public:
	FFbxLoggerSetter(FFbxImporter * InImpoter, bool ShowLogMessageOnlyIfError = false)
		: Importer(InImpoter)
	{
		// if impoter doesn't have logger, sets it
		if(Importer->Logger == NULL)
		{
			Logger.ShowLogMessageOnlyIfError = ShowLogMessageOnlyIfError;
			Importer->SetLogger(&Logger);
		}
		else
		{
			// if impoter already has logger set
			// invalidated Importer to make sure it doesn't clear
			Importer = NULL;
		}
	}

	~FFbxLoggerSetter()
	{
		if(Importer)
		{
			Importer->ClearLogger();
		}
	}
};
} // namespace UnFbx


