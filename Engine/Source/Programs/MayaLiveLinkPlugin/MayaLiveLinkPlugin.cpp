// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "RequiredProgramMainCPPInclude.h"
#include "CommandLine.h"
#include "TaskGraphInterfaces.h"
#include "ModuleManager.h"
#include "Object.h"
#include "ConfigCacheIni.h"

#include "LiveLinkProvider.h"
#include "LiveLinkRefSkeleton.h"
#include "LiveLinkTypes.h"
#include "OutputDevice.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlankMayaPlugin, Log, All);

IMPLEMENT_APPLICATION(MayaLiveLinkPlugin, "MayaLiveLinkPlugin");


// Maya includes
#define DWORD BananaFritters
#include <maya/MObject.h>
#include <maya/MGlobal.h>
#include <maya/MFnPlugin.h>
#include <maya/MPxCommand.h> //command
#include <maya/MPxNode.h> //node
#include <maya/MFnNumericAttribute.h>
#include <maya/MCallbackIdArray.h>
#include <maya/MEventMessage.h>
#include <maya/MDagMessage.h>
#include <maya/MItDag.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MMatrix.h>
#include <maya/MTransformationMatrix.h>
#include <maya/MQuaternion.h>
#include <maya/MVector.h>
#include <maya/MFnTransform.h>
#include <maya/MFnIkJoint.h>
#include <maya/MEulerRotation.h>
#include <maya/MSelectionList.h>
#include <maya/MAnimControl.h>
#include <maya/MTimerMessage.h>
#include <maya/MDGMessage.h>
#include <maya/MNodeMessage.h>
#include <maya/MSceneMessage.h>

#undef DWORD

TSharedPtr<ILiveLinkProvider> LiveLinkProvider;

MCallbackIdArray myCallbackIds;

MCallbackIdArray dagUpdateCallbackIds;

MSpace::Space G_TransformSpace = MSpace::kTransform;

MMatrix GetScale(MFnIkJoint& Joint)
{
	double Scale[3];
	Joint.getScale(Scale);
	MTransformationMatrix M;
	M.setScale(Scale, G_TransformSpace);
	return M.asMatrix();
}

MMatrix GetRotationOrientation(MFnIkJoint& Joint, MTransformationMatrix::RotationOrder& RotOrder)
{
	double ScaleOrientation[3];
	Joint.getScaleOrientation(ScaleOrientation, RotOrder);
	MTransformationMatrix M;
	M.setRotation(ScaleOrientation, RotOrder);
	return M.asMatrix();
}

MMatrix GetRotation(MFnIkJoint& Joint, MTransformationMatrix::RotationOrder& RotOrder)
{
	double Rotation[3];
	Joint.getRotation(Rotation, RotOrder);
	MTransformationMatrix M;
	M.setRotation(Rotation, RotOrder);
	return M.asMatrix();
}

MMatrix GetJointOrientation(MFnIkJoint& Joint, MTransformationMatrix::RotationOrder& RotOrder)
{
	double JointOrientation[3];
	Joint.getOrientation(JointOrientation, RotOrder);
	MTransformationMatrix M;
	M.setRotation(JointOrientation, RotOrder);
	return M.asMatrix();
}

MMatrix GetTranslation(MFnIkJoint& Joint)
{
	MVector Translation = Joint.getTranslation(G_TransformSpace);
	MTransformationMatrix M;
	M.setTranslation(Translation,G_TransformSpace);
	return M.asMatrix();
}

double RadToDeg(double Rad)
{
	const double E_PI = 3.1415926535897932384626433832795028841971693993751058209749445923078164062;
	return (Rad*180.0) / E_PI;
}

void OutputRotation(const MMatrix& M)
{
	MTransformationMatrix TM(M);

	MEulerRotation Euler = TM.eulerRotation();

	FVector V;

	V.X = RadToDeg(Euler[0]);
	V.Y = RadToDeg(Euler[1]);
	V.Z = RadToDeg(Euler[2]);
	MGlobal::displayInfo(*V.ToString());
}

struct FStreamHierarchy
{
	FName JointName;
	MFnIkJoint JointObject;
	int32 ParentIndex;

	FStreamHierarchy() {}

	FStreamHierarchy(const FStreamHierarchy& Other)
		: JointName(Other.JointName)
		, JointObject(Other.JointObject.dagPath())
		, ParentIndex(Other.ParentIndex)
	{}

	FStreamHierarchy(FName InJointName, const MDagPath& InJointPath, int32 InParentIndex)
		: JointName(InJointName)
		, JointObject(InJointPath)
		, ParentIndex(InParentIndex)
	{}
};

TArray<FStreamHierarchy> JointsToStream;

double LastStreamSeconds = 0.f;

MTime CurrentTime;

bool bNeedsHierarchy = true;

void BuildStreamHierarchyData();

void StreamJoints()
{
	if (bNeedsHierarchy)
	{
		BuildStreamHierarchyData();
	}

	CurrentTime = MAnimControl::currentTime();

	double NewSeconds = FPlatformTime::Seconds();
	if (NewSeconds - LastStreamSeconds < (1.0 / 90.0))
	{
		return;
	}
	LastStreamSeconds = NewSeconds;

	TArray<FTransform> JointTransforms;
	JointTransforms.Reserve(JointsToStream.Num());

	TArray<MMatrix> InverseScales;
	InverseScales.Reserve(JointsToStream.Num());

	for (int32 Idx = 0; Idx < JointsToStream.Num(); ++Idx)
	{
		FStreamHierarchy& H = JointsToStream[Idx];

		MTransformationMatrix::RotationOrder RotOrder = H.JointObject.rotationOrder();

		MMatrix JointScale = GetScale(H.JointObject);
		InverseScales.Add(JointScale.inverse());

		MMatrix ParentInverseScale = (H.ParentIndex == -1) ? MMatrix::identity : InverseScales[H.ParentIndex];

		MMatrix MayaSpaceJointMatrix = JointScale *
			GetRotationOrientation(H.JointObject, RotOrder) *
			GetRotation(H.JointObject, RotOrder) *
			GetJointOrientation(H.JointObject, RotOrder) *
			ParentInverseScale *
			GetTranslation(H.JointObject);

		//OutputRotation(GetRotation(jointObject, RotOrder));
		//OutputRotation(GetRotationOrientation(jointObject, RotOrder));
		//OutputRotation(GetJointOrientation(jointObject, RotOrder));
		//OutputRotation(TempJointMatrix);

		MMatrix UnrealSpaceJointMatrix;

		// from FFbxDataConverter::ConvertMatrix
		for (int i = 0; i < 4; ++i)
		{
			double* Row = MayaSpaceJointMatrix[i];
			if (i == 1)
			{
				UnrealSpaceJointMatrix[i][0] = -Row[0];
				UnrealSpaceJointMatrix[i][1] = Row[1];
				UnrealSpaceJointMatrix[i][2] = -Row[2];
				UnrealSpaceJointMatrix[i][3] = -Row[3];
			}
			else
			{
				UnrealSpaceJointMatrix[i][0] = Row[0];
				UnrealSpaceJointMatrix[i][1] = -Row[1];
				UnrealSpaceJointMatrix[i][2] = Row[2];
				UnrealSpaceJointMatrix[i][3] = Row[3];
			}
		}

		//OutputRotation(FinalJointMatrix);

		MTransformationMatrix UnrealSpaceJointTransform(UnrealSpaceJointMatrix);


		// getRotation is MSpace::kTransform
		double tx, ty, tz, tw;
		UnrealSpaceJointTransform.getRotationQuaternion(tx, ty, tz, tw, MSpace::kWorld);

		FTransform UETrans;
		UETrans.SetRotation(FQuat(tx, ty, tz, tw));

		MVector Translation = UnrealSpaceJointTransform.getTranslation(MSpace::kWorld);
		UETrans.SetTranslation(FVector(Translation.x, Translation.y, Translation.z));

		double Scale[3];
		UnrealSpaceJointTransform.getScale(Scale, MSpace::kWorld);
		UETrans.SetScale3D(FVector((float)Scale[0], (float)Scale[1], (float)Scale[2]));

		JointTransforms.Add(UETrans);
	}

	TArray<FLiveLinkCurveElement> Curves;
	const FName SubjectName(TEXT("Maya"));

#if 0
	double CurFrame = CurrentTime.value();
	double CurveValue = CurFrame / 200.0;

	Curves.AddDefaulted();
	Curves[0].CurveName = FName(TEXT("Test"));
	Curves[0].CurveValue = static_cast<float>(FMath::Clamp(CurveValue, 0.0, 1.0));

	if (CurFrame > 100.0)
	{
		double Curve2Value = (CurFrame - 100.0) / 100.0;
		Curves.AddDefaulted();
		Curves[1].CurveName = FName(TEXT("Test2"));
		Curves[1].CurveValue = static_cast<float>(FMath::Clamp(Curve2Value, 0.0, 1.0));
	}
	//MGlobal::displayInfo(MString("CURVE TEST:") + CurFrame + " " + CurveValue);

	if (CurFrame > 201.0)
	{
		LiveLinkProvider->ClearSubject(SubjectName);
		bNeedsHierarchy = true;
	}
	else
	{
		LiveLinkProvider->UpdateSubjectFrame(SubjectName, JointTransforms, Curves, FPlatformTime::Seconds(), CurrentTime.value());
	}
#else
	LiveLinkProvider->UpdateSubjectFrame(SubjectName, JointTransforms, Curves, FPlatformTime::Seconds(), CurrentTime.value());
#endif
}

void OnDagChangedAll(MObject& transformNode, MDagMessage::MatrixModifiedFlags& modified, void *clientData)
{
	if (!MAnimControl::isPlaying() /*&& MAnimControl::currentTime() == CurrentTime*/)
	{
		StreamJoints();
	}
}

MCallbackIdArray StreamHierarchyCallbackIds;

void BuildStreamHierarchyData()
{
	bNeedsHierarchy = false;

	if (StreamHierarchyCallbackIds.length() != 0)
	{
		// Make sure we remove all the callbacks we added
		MMessage::removeCallbacks(StreamHierarchyCallbackIds);
	}

	StreamHierarchyCallbackIds.clear();

	JointsToStream.Reset();

	MItDag::TraversalType traversalType = MItDag::kBreadthFirst;
	MFn::Type filter = MFn::kJoint;

	MStatus status;
	MItDag dagIterator(traversalType, filter, &status);

	TArray<FName> JointNames;
	TArray<int32> JointParents;

	for (; !dagIterator.isDone(); dagIterator.next())
	{
		MDagPath dagPath;
		status = dagIterator.getPath(dagPath);
		if (!status)
		{
			status.perror("MItDag::getPath");
			continue;
		}

		MFnDagNode dagNode(dagPath, &status);

		FString Name(dagNode.name().asChar());
		if (!Name.Equals(TEXT("arcblade:root"), ESearchCase::IgnoreCase) &&
			!Name.Equals(TEXT("root"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		//Build Hierarchy
		TArray<int32> ParentIndexStack;
		ParentIndexStack.SetNum(100, false);

		MItDag JointIterator;
		JointIterator.reset(dagPath, MItDag::kDepthFirst, MFn::kJoint);

		int32 Index = 0;

		for (; !JointIterator.isDone(); JointIterator.next())
		{
			uint32 Depth = JointIterator.depth();
			if (Depth >= (uint32)ParentIndexStack.Num())
			{
				ParentIndexStack.SetNum(Depth + 1);
			}
			ParentIndexStack[Depth] = Index++;

			int32 ParentIndex = Depth == 0 ? -1 : ParentIndexStack[Depth - 1];

			MDagPath JointPath;
			status = JointIterator.getPath(JointPath);
			MFnIkJoint JointObject(JointPath);

			//MGlobal::displayInfo(MString("Iter: ") + JointPath.fullPathName() + JointIterator.depth());

			MGlobal::displayInfo(MString("Register Callback: ") + JointPath.fullPathName());
			MCallbackId NewCB = MDagMessage::addWorldMatrixModifiedCallback(JointPath, (MDagMessage::MWorldMatrixModifiedFunction)OnDagChangedAll);
			StreamHierarchyCallbackIds.append(NewCB);

			FName JointName(JointObject.name().asChar());

			JointsToStream.Add(FStreamHierarchy(JointName, JointPath, ParentIndex));
			JointNames.Add(JointName);
			JointParents.Add(ParentIndex);
		}
	}

	const FName SubjectName(TEXT("Maya"));
	LiveLinkProvider->UpdateSubject(SubjectName, JointNames, JointParents);

}
void RecurseJoint(MFnDagNode& Joint, const MMatrix& ParentInverseScale, TArray<FName>& JointNames, TArray<FTransform>& JointTransform)
{
	MStringArray SA;
	Joint.name().split(':', SA);
	int32 LastName = SA.length()-1;

	JointNames.Add(FName( SA[LastName].asChar() ));
	MMatrix JointScale;

	{
		//FString& Name = JointNames.Last();
		//MGlobal::displayInfo(*Name);

		MStatus status;
		MDagPath JointPath;
		Joint.getPath(JointPath);
		MFnIkJoint jointObject(JointPath, &status);

		// From http://download.autodesk.com/us/maya/2010help/API/class_m_fn_ik_joint.html
		// Transform for join is Scale*RotationOrientation*Rotation*JointOrientation*ParentScaleInverse*Translate
		// where - RotationOrientation is ScaleOrientation
		//       - JointOrientation is Orientation
		//       - ParentScaleInverse is inverse of get scale on parent

		MTransformationMatrix::RotationOrder RotOrder = jointObject.rotationOrder();

		JointScale = GetScale(jointObject);

		MMatrix MayaSpaceJointMatrix =		JointScale *
											GetRotationOrientation(jointObject, RotOrder) *
											GetRotation(jointObject, RotOrder) *
											GetJointOrientation(jointObject, RotOrder) *
											ParentInverseScale *
											GetTranslation(jointObject);

		//OutputRotation(GetRotation(jointObject, RotOrder));
		//OutputRotation(GetRotationOrientation(jointObject, RotOrder));
		//OutputRotation(GetJointOrientation(jointObject, RotOrder));
		//OutputRotation(TempJointMatrix);

		MMatrix UnrealSpaceJointMatrix;

		// from FFbxDataConverter::ConvertMatrix
		for (int i = 0; i < 4; ++i)
		{
			double* Row = MayaSpaceJointMatrix[i];
			if (i == 1)
			{
				UnrealSpaceJointMatrix[i][0] = -Row[0];
				UnrealSpaceJointMatrix[i][1] = Row[1];
				UnrealSpaceJointMatrix[i][2] = -Row[2];
				UnrealSpaceJointMatrix[i][3] = -Row[3];
			}
			else
			{
				UnrealSpaceJointMatrix[i][0] = Row[0];
				UnrealSpaceJointMatrix[i][1] = -Row[1];
				UnrealSpaceJointMatrix[i][2] = Row[2];
				UnrealSpaceJointMatrix[i][3] = Row[3];
			}
		}

		//OutputRotation(FinalJointMatrix);
		
		MTransformationMatrix UnrealSpaceJointTransform(UnrealSpaceJointMatrix);
		
		
		// getRotation is MSpace::kTransform
		double tx, ty, tz, tw;
		UnrealSpaceJointTransform.getRotationQuaternion(tx, ty, tz, tw, MSpace::kWorld);

		FTransform UETrans;
		UETrans.SetRotation(FQuat(tx,ty,tz,tw));
		
		MVector Translation = UnrealSpaceJointTransform.getTranslation(MSpace::kWorld);
		UETrans.SetTranslation(FVector(Translation.x, Translation.y, Translation.z));

		double Scale[3];
		UnrealSpaceJointTransform.getScale(Scale, MSpace::kWorld);
		UETrans.SetScale3D(FVector((float)Scale[0], (float)Scale[1], (float)Scale[2]));

		JointTransform.Add(UETrans);
	}

	const MMatrix JointInverseScale = JointScale.inverse();

	for (unsigned int i = 0; i < Joint.childCount(); ++i)
	{

		// get the MObject for the i'th child 
		MObject child = Joint.child(i);

		// attach a function set to it
		MFnDagNode ChildJoint(child);

		// write the child name
		//MGlobal::displayInfo("Child:");
		//MGlobal::displayInfo(ChildJoint.name());
		RecurseJoint(ChildJoint, JointInverseScale, JointNames, JointTransform);
	}
}

double Seconds=0.f;

void OnTimeChanged(void* clientData) {
	StreamJoints();
	return;

	//MGlobal::displayInfo("On Time Changed!\n");
	CurrentTime = MAnimControl::currentTime();

	double NewSeconds = FPlatformTime::Seconds();
	if (NewSeconds - Seconds < (1.f / 90.f))
	{
		return;
	}
	Seconds = NewSeconds;

	MItDag::TraversalType traversalType = MItDag::kBreadthFirst;
	MFn::Type filter = MFn::kJoint;
	bool quiet = false;

	MStatus status;
	MItDag dagIterator(traversalType, filter, &status);

	TArray<FName> JointNames;
	JointNames.Reserve(200);
	TArray<FTransform> JointTransforms;
	JointTransforms.Reserve(200);

	for (; !dagIterator.isDone(); dagIterator.next())
	{
		MDagPath dagPath;
		status = dagIterator.getPath(dagPath);
		if (!status)
		{
			status.perror("MItDag::getPath");
			continue;
		}

		MFnDagNode dagNode(dagPath, &status);

		FString Name(dagNode.name().asChar());
		if (!Name.Equals(TEXT("arcblade:root"), ESearchCase::IgnoreCase) &&
			!Name.Equals(TEXT("root"), ESearchCase::IgnoreCase) )
		{
			continue;
		}

		RecurseJoint(dagNode, MMatrix::identity, JointNames, JointTransforms);
		break;
	}

	const FString SubjectName(TEXT("Maya"));

	//LiveLinkProvider->UpdateSubjectFrame(SubjectName, JointNames, JointTransforms);
	return;
}

void OnDagChanged(MObject& transformNode, MDagMessage::MatrixModifiedFlags& modified, void *clientData)
{
	if (!MAnimControl::isPlaying() && MAnimControl::currentTime() == CurrentTime)
	{
		OnTimeChanged(clientData);
	}
}

void OnAttrChanged(MNodeMessage::AttributeMessage msg, MPlug & plug, MPlug& OtherPlug, void* clientData)
{
	//OnTimeChanged(clientData);
}

void OnSelectionChanged(void* ClientData)
{
	if (dagUpdateCallbackIds.length() != 0)
	{
		// Make sure we remove all the callbacks we added
		MMessage::removeCallbacks(dagUpdateCallbackIds);
	}

	dagUpdateCallbackIds.clear();

	MSelectionList selList;
	MGlobal::getActiveSelectionList(selList);
	//MGlobal::displayInfo("OnSelectionChanged\n");
	for (uint32 Idx = 0; Idx < selList.length(); ++Idx)
	{
		MDagPath dagPath;
		
		selList.getDagPath(Idx, dagPath);

		if(dagPath.isValid())
		{
			//MGlobal::displayInfo(dagPath.fullPathName());
			MStatus status;
			MFnDagNode dagNode(dagPath, &status);

			MCallbackId NewCB = MDagMessage::addWorldMatrixModifiedCallback(dagPath, (MDagMessage::MWorldMatrixModifiedFunction)OnDagChanged);
			dagUpdateCallbackIds.append(NewCB);

			if (status.statusCode() == MStatus::kSuccess)
			{
				for (unsigned int i = 0; i < dagNode.childCount(); ++i)
				{

					// get the MObject for the i'th child 
					MObject child = dagNode.child(i);

					// attach a function set to it
					MFnDagNode ChildNode(child);
					MDagPath ChildPath;
					ChildNode.getPath(ChildPath);

					if(ChildPath.isValid())
					{
						//MGlobal::displayInfo("Child:");
						//MGlobal::displayInfo(ChildPath.fullPathName());
						MCallbackId NewChildCB = MDagMessage::addWorldMatrixModifiedCallback(ChildPath, (MDagMessage::MWorldMatrixModifiedFunction)OnDagChanged);
						dagUpdateCallbackIds.append(NewChildCB);
					}
					else
					{
						//MGlobal::displayInfo("Invalid Child Dag Path");
					}
					break;
				}
			}
		}

		/*MObject Obj;
		selList.getDependNode(Idx, Obj);
		if (!Obj.isNull())
		{
			MCallbackId NewAttrCB = MNodeMessage::addAttributeChangedCallback(Obj, (MNodeMessage::MAttr2PlugFunction)OnAttrChanged);
			dagUpdateCallbackIds.append(NewAttrCB);
		}*/
		//break; //Only do 1
	}
}

void OnTimer(float elapsedTime, float lastTime, void* clientData)
{
	OnTimeChanged(clientData);
}

void OnForceChange(MTime& time, void* clientData)
{
	OnTimeChanged(clientData);
}


class FMayaOutputDevice : public FOutputDevice
{
public:
	FMayaOutputDevice() : bAllowLogVerbosity(false) {}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		if ((bAllowLogVerbosity && Verbosity <= ELogVerbosity::Log) || (Verbosity <= ELogVerbosity::Display))
		{
			MGlobal::displayInfo(V);
		}
	}

private:

	bool bAllowLogVerbosity;

};

void OnSceneOpen(void* client)
{
	BuildStreamHierarchyData();
}

/**
 * This function is called by Maya when the plugin becomes loaded 
 *
 * @param	MayaPluginObject	The Maya object that represents our plugin
 *
 * @return	MS::kSuccess if everything went OK and the plugin is ready to use
 */
DLLEXPORT MStatus initializePlugin( MObject MayaPluginObject )
{
	MStringArray names;

	MEventMessage::getEventNames(names);
	
	for (uint i = 0; i < names.length(); ++i)
	{
		MGlobal::displayInfo(names[i]);
	}

	GEngineLoop.PreInit(TEXT("MayaLiveLinkPlugin -Messaging"));
	ProcessNewlyLoadedUObjects();
	// Tell the module manager is may now process newly-loaded UObjects when new C++ modules are loaded
	FModuleManager::Get().StartProcessingNewlyLoadedObjects();

	// Tell Maya about our plugin
	MFnPlugin MayaPlugin( 
		MayaPluginObject, 
		"MayaLiveLinkPlugin",	// @todo: Put the vendor name here.  It shows up in the "Info" dialog in Maya's Plugin Manager
		"v1.0" );			// @todo: Put version string for your plugin here

	// ... do stuff here ...

	FModuleManager::Get().LoadModule(TEXT("UdpMessaging"));
	//FModuleManager::Get().LoadModule(TEXT("LiveLink"));

	GLog->TearDown(); //clean up existing output devices
	GLog->AddOutputDevice(new FMayaOutputDevice()); //Add Maya output device

	LiveLinkProvider = ILiveLinkProvider::CreateLiveLinkProvider(TEXT("Maya Live Link"));

	//MCallbackId timeChangedCallbackId = MEventMessage::addEventCallback("timeChanged", (MMessage::MBasicFunction)OnTimeChanged);
	//myCallbackIds.append(timeChangedCallbackId);

	MCallbackId forceUpdateCallbackId = MDGMessage::addForceUpdateCallback((MMessage::MTimeFunction)OnForceChange);
	myCallbackIds.append(forceUpdateCallbackId);

	MCallbackId selectionChangedCallback = MEventMessage::addEventCallback("SelectionChanged", (MMessage::MBasicFunction)OnSelectionChanged);
	myCallbackIds.append(selectionChangedCallback);

	//MCallbackId timerCallback = MTimerMessage::addTimerCallback(1.f / 20.f, (MMessage::MElapsedTimeFunction)OnTimer);
	//myCallbackIds.append(timerCallback);

	MCallbackId SceneOpenedCallbackId = MSceneMessage::addCallback(MSceneMessage::kAfterOpen, (MMessage::MBasicFunction)OnSceneOpen);
	myCallbackIds.append(SceneOpenedCallbackId);

	UE_LOG(LogBlankMayaPlugin, Display, TEXT("MayaLiveLinkPlugin initialized"));

	// Print to Maya's output window, too!
	MGlobal::displayInfo(MString("MayaLiveLinkPlugin initialized"));

	const MStatus MayaStatusResult = MS::kSuccess;
	return MayaStatusResult;
}


/**
 * Called by Maya either at shutdown, or when the user opts to unload the plugin through the Plugin Manager 
 *
 * @param	MayaPluginObject	The Maya object that represents our plugin
 *
 * @return	MS::kSuccess if everything went OK and the plugin was fully shut down
 */
DLLEXPORT MStatus uninitializePlugin( MObject MayaPluginObject )
{
	// Get the plugin API for the plugin object
	MFnPlugin MayaPlugin( MayaPluginObject );

	// ... do stuff here ...

	if (myCallbackIds.length() != 0)
	{
		// Make sure we remove all the callbacks we added
		MMessage::removeCallbacks(myCallbackIds);
	}

	LiveLinkProvider = nullptr;

	const MStatus MayaStatusResult = MS::kSuccess;
	return MayaStatusResult;
}