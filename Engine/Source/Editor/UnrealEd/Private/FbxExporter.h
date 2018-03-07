// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "EngineDefines.h"
#include "Engine/StaticMesh.h"
#include "MatineeExporter.h"
#include "MovieSceneSequenceID.h"
#include "MovieSceneFwd.h"
#include "FbxImporter.h"
#include "UObject/GCObject.h"

class ABrush;
class ACameraActor;
class ALandscapeProxy;
class ALight;
class AMatineeActor;
class ASkeletalMeshActor;
class IMovieScenePlayer;
class UAnimSequence;
class UCameraComponent;
class UInstancedStaticMeshComponent;
class UInterpTrackFloatProp;
class UInterpTrackInstMove;
class UInterpTrackMove;
class UInterpTrackMoveAxis;
class ULightComponent;
class UMaterialInterface;
class UModel;
class UMovieScene;
class UMovieScene3DTransformTrack;
class UMovieSceneFloatTrack;
class USkeletalMesh;
class USkeletalMeshComponent;
class USplineMeshComponent;
class UStaticMeshComponent;
class FColorVertexBuffer;
class UFbxExportOption;
struct FAnimControlTrackKey;
struct FExpressionInput;
struct FRichCurve;

namespace UnFbx
{

/**
 * Main FBX Exporter class.
 */
class UNREALED_API FFbxExporter  : public MatineeExporter, public FGCObject
{
public:
	/**
	 * Returns the exporter singleton. It will be created on the first request.
	 */
	static FFbxExporter* GetInstance();
	static void DeleteInstance();
	~FFbxExporter();
	
	//~ FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		if (ExportOptions != nullptr)
		{
			Collector.AddReferencedObject(ExportOptions);
		}
	}


	/**
	* Load the export option from the last save state and show the dialog if bShowOptionDialog is true.
	* FullPath is the export file path we display it in the dialog
	* If user cancel the dialog, the OutOperationCanceled will be true
	* bOutExportAll will be true if the user want to use the same option for all other asset he want to export
	*
	* The function is saving the dialog state in a user ini file and reload it from there. It is not changing the CDO.
	*/
	void FillExportOptions(bool BatchMode, bool bShowOptionDialog, const FString& FullPath, bool& OutOperationCanceled, bool& bOutExportAll);

	/**
	 * Creates and readies an empty document for export.
	 */
	virtual void CreateDocument();
	
	/**
	 * Closes the FBX document, releasing its memory.
	 */
	virtual void CloseDocument();
	
	/**
	 * Writes the FBX document to disk and releases it by calling the CloseDocument() function.
	 */
	virtual void WriteToFile(const TCHAR* Filename);
	
	/**
	 * Exports the light-specific information for a light actor.
	 */
	virtual void ExportLight( ALight* Actor, INodeNameAdapter& NodeNameAdapter );

	/**
	 * Exports the camera-specific information for a camera actor.
	 */
	virtual void ExportCamera( ACameraActor* Actor, bool bExportComponents, INodeNameAdapter& NodeNameAdapter );

	/**
	 * Exports the mesh and the actor information for a brush actor.
	 */
	virtual void ExportBrush(ABrush* Actor, UModel* InModel, bool bConvertToStaticMesh, INodeNameAdapter& NodeNameAdapter );

	/**
	 * Exports the basic scene information to the FBX document.
	 */
	virtual void ExportLevelMesh( ULevel* InLevel, bool bSelectedOnly, INodeNameAdapter& NodeNameAdapter );

	/**
	 * Exports the given Matinee sequence information into a FBX document.
	 * 
	 * @return	true, if sucessful
	 */
	virtual bool ExportMatinee(class AMatineeActor* InMatineeActor);

	/**
	* Exports the given level sequence information into a FBX document.
	*
	* @return	true, if successful
	*/
	bool ExportLevelSequence( UMovieScene* MovieScene, const TArray<FGuid>& InBindings, IMovieScenePlayer* MovieScenePlayer, FMovieSceneSequenceIDRef SequenceID );

	/**
	 * Exports all the animation sequences part of a single Group in a Matinee sequence
	 * as a single animation in the FBX document.  The animation is created by sampling the
	 * sequence at 30 updates/second and extracting the resulting bone transforms from the given
	 * skeletal mesh
	 * @param MatineeSequence The Matinee Sequence containing the group to export
	 * @param SkeletalMeshComponent The Skeletal mesh that the animations from the Matinee group are applied to
	 */
	virtual void ExportMatineeGroup(class AMatineeActor* MatineeActor, USkeletalMeshComponent* SkeletalMeshComponent);


	/**
	 * Exports the mesh and the actor information for a static mesh actor.
	 */
	virtual void ExportStaticMesh( AActor* Actor, UStaticMeshComponent* StaticMeshComponent, INodeNameAdapter& NodeNameAdapter );

	/**
	 * Exports a static mesh
	 * @param StaticMesh	The static mesh to export
	 * @param MaterialOrder	Optional ordering of materials to set up correct material ID's across multiple meshes being export such as BSP surfaces which share common materials. Should be used sparingly
	 */
	virtual void ExportStaticMesh( UStaticMesh* StaticMesh, const TArray<FStaticMaterial>* MaterialOrder = NULL );

	/**
	 * Exports BSP
	 * @param Model			 The model with BSP to export
	 * @param bSelectedOnly  true to export only selected surfaces (or brushes)
	 */
	virtual void ExportBSP( UModel* Model, bool bSelectedOnly );

	/**
	 * Exports a static mesh light map
	 */
	virtual void ExportStaticMeshLightMap( UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannel );

	/**
	 * Exports a skeletal mesh
	 */
	virtual void ExportSkeletalMesh( USkeletalMesh* SkeletalMesh );

	/**
	 * Exports the mesh and the actor information for a skeletal mesh actor.
	 */
	virtual void ExportSkeletalMesh( AActor* Actor, USkeletalMeshComponent* SkeletalMeshComponent, INodeNameAdapter& NodeNameAdapter );
	
	/**
	 * Exports the mesh and the actor information for a landscape actor.
	 */
	void ExportLandscape(ALandscapeProxy* Landscape, bool bSelectedOnly, INodeNameAdapter& NodeNameAdapter);

	/**
	 * Exports a single UAnimSequence, and optionally a skeletal mesh
	 */
	FbxNode* ExportAnimSequence( const UAnimSequence* AnimSeq, const USkeletalMesh* SkelMesh, bool bExportSkelMesh, const TCHAR* MeshNames=NULL, FbxNode* ActorRootNode=NULL);

	/**
	 * Exports the list of UAnimSequences as a single animation based on the settings in the TrackKeys
	 */
	void ExportAnimSequencesAsSingle( USkeletalMesh* SkelMesh, const ASkeletalMeshActor* SkelMeshActor, const FString& ExportName, const TArray<UAnimSequence*>& AnimSeqList, const TArray<FAnimControlTrackKey>& TrackKeys );

	/** A node name adapter for matinee. */
	class UNREALED_API FMatineeNodeNameAdapter : public INodeNameAdapter
	{
	public:
		FMatineeNodeNameAdapter( AMatineeActor* InMatineeActor );
		virtual FString GetActorNodeName(const AActor* InActor) override;
	private:
		AMatineeActor* MatineeActor;
	};

	/** A node name adapter for a level sequence. */
	class UNREALED_API FLevelSequenceNodeNameAdapter : public INodeNameAdapter
	{
	public:
		FLevelSequenceNodeNameAdapter( UMovieScene* InMovieScene, IMovieScenePlayer* InMovieScenePlayer, FMovieSceneSequenceIDRef InSequenceID);
		virtual FString GetActorNodeName(const AActor* InActor) override;
	private:
		UMovieScene* MovieScene;
		IMovieScenePlayer* MovieScenePlayer;
		FMovieSceneSequenceID SequenceID;
	};

	/* Get a valid unique name from a name */
	FString GetFbxObjectName(const FString &FbxObjectNode, INodeNameAdapter& NodeNameAdapter);

	/**
	 * Exports the basic information about an actor and buffers it.
	 * This function creates one FBX node for the actor with its placement.
	 */
	FbxNode* ExportActor(AActor* Actor, bool bExportComponents, INodeNameAdapter& NodeNameAdapter);

private:
	FFbxExporter();

	static TSharedPtr<FFbxExporter> StaticInstance;

	FbxManager* SdkManager;
	FbxScene* Scene;
	FbxAnimStack* AnimStack;
	FbxAnimLayer* AnimLayer;
	FbxCamera* DefaultCamera;
	
	FFbxDataConverter Converter;
	
	TMap<FString,int32> FbxNodeNameToIndexMap;
	TMap<const AActor*, FbxNode*> FbxActors;
	TMap<const USkeletalMeshComponent*, FbxNode*> FbxSkeletonRoots;
	TMap<const UMaterialInterface*, FbxSurfaceMaterial*> FbxMaterials;
	TMap<const UStaticMesh*, FbxMesh*> FbxMeshes;

	/** The frames-per-second (FPS) used when baking transforms */
	static const float BakeTransformsFPS;
	
	/** Whether or not to export vertices unwelded */
	static bool bStaticMeshExportUnWeldedVerts;

	UFbxExportOption *ExportOptions;

	/** Adapter interface which allows ExportAnimTrack to act on both sequencer and matinee data. */
	class IAnimTrackAdapter
	{
	public:
		/** Gets the length of the animation track. */
		virtual float GetAnimationStart() const = 0;
		virtual float GetAnimationLength() const = 0;
		/** Updates the runtime state of the animation track to the specified time. */
		virtual void UpdateAnimation( float Time ) = 0;
	};

	/** An anim track adapter for matinee. */
	class FMatineeAnimTrackAdapter : public IAnimTrackAdapter
	{
	public:
		FMatineeAnimTrackAdapter( AMatineeActor* InMatineeActor );
		virtual float GetAnimationStart() const override;
		virtual float GetAnimationLength() const override;
		virtual void UpdateAnimation( float Time ) override;

	private:
		AMatineeActor* MatineeActor;
	};

	/** An anim track adapter for a level sequence. */
	class FLevelSequenceAnimTrackAdapter : public FFbxExporter::IAnimTrackAdapter
	{
	public:
		FLevelSequenceAnimTrackAdapter( IMovieScenePlayer* InMovieScenePlayer, UMovieScene* InMovieScene );
		virtual float GetAnimationStart() const override;
		virtual float GetAnimationLength() const override;
		virtual void UpdateAnimation( float Time ) override;

	private:
		IMovieScenePlayer* MovieScenePlayer;
		UMovieScene* MovieScene;
	};

	/**
	* Export Anim Track of the given SkeletalMeshComponent
	*/
	void ExportAnimTrack( IAnimTrackAdapter& AnimTrackAdapter, AActor* Actor, USkeletalMeshComponent* SkeletalMeshComponent );

	void ExportModel(UModel* Model, FbxNode* Node, const char* Name);
	
#if WITH_PHYSX
	FbxNode* ExportCollisionMesh(const UStaticMesh* StaticMesh, const TCHAR* MeshName, FbxNode* ParentActor);
#endif

	/**
	 * Exports a static mesh
	 * @param StaticMesh	The static mesh to export
	 * @param MeshName		The name of the mesh for the FBX file
	 * @param FbxActor		The fbx node representing the mesh
	 * @param ExportLOD		The LOD of the mesh to export
	 * @param LightmapUVChannel Optional UV channel to export
	 * @param ColorBuffer	Vertex color overrides to export
	 * @param MaterialOrderOverride	Optional ordering of materials to set up correct material ID's across multiple meshes being export such as BSP surfaces which share common materials. Should be used sparingly
	 */
	FbxNode* ExportStaticMeshToFbx(const UStaticMesh* StaticMesh, int32 ExportLOD, const TCHAR* MeshName, FbxNode* FbxActor, int32 LightmapUVChannel = -1, const FColorVertexBuffer* ColorBuffer = NULL, const TArray<FStaticMaterial>* MaterialOrderOverride = NULL);

	/**
	 * Exports a spline mesh
	 * @param SplineMeshComp	The spline mesh component to export
	 * @param MeshName		The name of the mesh for the FBX file
	 * @param FbxActor		The fbx node representing the mesh
	 */
	void ExportSplineMeshToFbx(const USplineMeshComponent* SplineMeshComp, const TCHAR* MeshName, FbxNode* FbxActor);

	/**
	 * Exports an instanced mesh
	 * @param InstancedMeshComp	The instanced mesh component to export
	 * @param MeshName		The name of the mesh for the FBX file
	 * @param FbxActor		The fbx node representing the mesh
	 */
	void ExportInstancedMeshToFbx(const UInstancedStaticMeshComponent* InstancedMeshComp, const TCHAR* MeshName, FbxNode* FbxActor);

	/**
	* Exports a landscape
	* @param Landscape		The landscape to export
	* @param MeshName		The name of the mesh for the FBX file
	* @param FbxActor		The fbx node representing the mesh
	*/
	void ExportLandscapeToFbx(ALandscapeProxy* Landscape, const TCHAR* MeshName, FbxNode* FbxActor, bool bSelectedOnly);

	/**
	* Fill an fbx light with from a unreal light component
	*@param ParentNode			The parent FbxNode the one over the light node
	* @param Camera				Fbx light object
	* @param CameraComponent	Unreal light component
	*/
	void FillFbxLightAttribute(FbxLight* Light, FbxNode* FbxParentNode, ULightComponent* BaseLight);

	/**
	* Fill an fbx camera with from a unreal camera component
	* @param ParentNode			The parent FbxNode the one over the camera node
	* @param Camera				Fbx camera object
	* @param CameraComponent	Unreal camera component
	*/
	void FillFbxCameraAttribute(FbxNode* ParentNode, FbxCamera* Camera, UCameraComponent *CameraComponent);

	/**
	 * Adds FBX skeleton nodes to the FbxScene based on the skeleton in the given USkeletalMesh, and fills
	 * the given array with the nodes created
	 */
	FbxNode* CreateSkeleton(const USkeletalMesh* SkelMesh, TArray<FbxNode*>& BoneNodes);

	/**
	 * Adds an Fbx Mesh to the FBX scene based on the data in the given FStaticLODModel
	 */
	FbxNode* CreateMesh(const USkeletalMesh* SkelMesh, const TCHAR* MeshName);

	/**
	 * Adds Fbx Clusters necessary to skin a skeletal mesh to the bones in the BoneNodes list
	 */
	void BindMeshToSkeleton(const USkeletalMesh* SkelMesh, FbxNode* MeshRootNode, TArray<FbxNode*>& BoneNodes);

	/**
	 * Add a bind pose to the scene based on the FbxMesh and skinning settings of the given node
	 */
	void CreateBindPose(FbxNode* MeshRootNode);

	/**
	 * Add the given skeletal mesh to the Fbx scene in preparation for exporting.  Makes all new nodes a child of the given node
	 */
	FbxNode* ExportSkeletalMeshToFbx(const USkeletalMesh* SkelMesh, const UAnimSequence* AnimSeq, const TCHAR* MeshName, FbxNode* ActorRootNode);

	/** Export SkeletalMeshComponent */
	void ExportSkeletalMeshComponent(USkeletalMeshComponent* SkelMeshComp, const TCHAR* MeshName, FbxNode* ActorRootNode);

	/**
	 * Add the given animation sequence as rotation and translation tracks to the given list of bone nodes
	 */
	void ExportAnimSequenceToFbx(const UAnimSequence* AnimSeq, const USkeletalMesh* SkelMesh, TArray<FbxNode*>& BoneNodes, FbxAnimLayer* AnimLayer,
		float AnimStartOffset, float AnimEndOffset, float AnimPlayRate, float StartTime);

	/** 
	 * The curve code doesn't differentiate between angles and other data, so an interpolation from 179 to -179
	 * will cause the bone to rotate all the way around through 0 degrees.  So here we make a second pass over the 
	 * rotation tracks to convert the angles into a more interpolation-friendly format.  
	 */
	void CorrectAnimTrackInterpolation( TArray<FbxNode*>& BoneNodes, FbxAnimLayer* AnimLayer );

	/**
	 * Exports the Matinee movement track into the FBX animation stack.
	 */
	void ExportMatineeTrackMove(FbxNode* FbxActor, UInterpTrackInstMove* MoveTrackInst, UInterpTrackMove* MoveTrack, float InterpLength);

	/**
	 * Exports the Matinee float property track into the FBX animation stack.
	 */
	void ExportMatineeTrackFloatProp(FbxNode* FbxActor, UInterpTrackFloatProp* PropTrack);

	/**
	 * Exports a given interpolation curve into the FBX animation curve.
	 */
	void ExportAnimatedVector(FbxAnimCurve* FbxCurve, const ANSICHAR* ChannelName, UInterpTrackMove* MoveTrack, UInterpTrackInstMove* MoveTrackInst, bool bPosCurve, int32 CurveIndex, bool bNegative, float InterpLength);
	
	/**
	 * Exports a movement subtrack to an FBX curve
	 */
	void ExportMoveSubTrack(FbxAnimCurve* FbxCurve, const ANSICHAR* ChannelName, UInterpTrackMoveAxis* SubTrack, UInterpTrackInstMove* MoveTrackInst, bool bPosCurve, int32 CurveIndex, bool bNegative, float InterpLength);
	
	void ExportAnimatedFloat(FbxProperty* FbxProperty, FInterpCurveFloat* Curve, bool IsCameraFoV);

	/**
	 * Exports a level sequence 3D transform track into the FBX animation stack.
	 */
	void ExportLevelSequence3DTransformTrack( FbxNode& FbxActor, UMovieScene3DTransformTrack& TransformTrack, AActor* Actor, const TRange<float>& InPlaybackRange );

	/** 
	 * Exports a level sequence float track into the FBX animation stack. 
	 */
	void ExportLevelSequenceFloatTrack( FbxNode& FbxActor, UMovieSceneFloatTrack& FloatTrack );

	/** Defines value export modes for the EportRichCurveToFbxCurve method. */
	enum class ERichCurveValueMode
	{
		/** Export values directly */
		Default,
		/** Export fov values which get processed to focal length. */
		Fov
	};

	/** Exports an unreal rich curve to an fbx animation curve. */
	void ExportRichCurveToFbxCurve(FbxAnimCurve& InFbxCurve, FRichCurve& InRichCurve, ERichCurveValueMode ValueMode = ERichCurveValueMode::Default, bool bNegative = false);

	/**
	 * Finds the given actor in the already-exported list of structures
	 * @return FbxNode* the FBX node created from the UE4 actor
	 */
	FbxNode* FindActor(AActor* Actor);

	/**
	 * Find bone array of FbxNOdes of the given skeletalmeshcomponent  
	 */
	bool FindSkeleton(const USkeletalMeshComponent* SkelComp, TArray<FbxNode*>& BoneNodes);

	/** recursively get skeleton */
	void GetSkeleton(FbxNode* RootNode, TArray<FbxNode*>& BoneNodes);

	bool FillFbxTextureProperty(const char *PropertyName, const FExpressionInput& MaterialInput, FbxSurfaceMaterial* FbxMaterial);
	/**
	 * Exports the profile_COMMON information for a material.
	 */
	FbxSurfaceMaterial* ExportMaterial(UMaterialInterface* Material);
	
	FbxSurfaceMaterial* CreateDefaultMaterial();
	
	/**
	 * Create user property in Fbx Node.
	 * Some Unreal animatable property can't be animated in FBX property. So create user property to record the animation of property.
	 *
	 * @param Node  FBX Node the property append to.
	 * @param Value Property value.
	 * @param Name  Property name.
	 * @param Label Property label.
	 */
	void CreateAnimatableUserProperty(FbxNode* Node, float Value, const char* Name, const char* Label);
};



} // namespace UnFbx
