// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FbxAnimUtils.h"
#include "Misc/Paths.h"
#include "EditorDirectories.h"
#include "FbxExporter.h"
#include "Animation/AnimTypes.h"
#include "Curves/RichCurve.h"
#include "Engine/CurveTable.h"

namespace FbxAnimUtils
{
	void ExportAnimFbx( const FString& ExportFilename, UAnimSequence* AnimSequence, USkeletalMesh* Mesh, bool bSaveSkeletalMesh, bool BatchMode, bool &OutExportAll, bool &OutCancelExport )
	{
		if( !ExportFilename.IsEmpty() && AnimSequence && Mesh )
		{
			FString FileName = ExportFilename;
			FEditorDirectories::Get().SetLastDirectory(ELastDirectory::FBX_ANIM, FPaths::GetPath(FileName)); // Save path as default for next time.

			UnFbx::FFbxExporter* Exporter = UnFbx::FFbxExporter::GetInstance();
			//Show the fbx export dialog options
			Exporter->FillExportOptions(BatchMode, (!BatchMode || !OutExportAll), ExportFilename, OutCancelExport, OutExportAll);
			if (!OutCancelExport)
			{
				Exporter->CreateDocument();

				Exporter->ExportAnimSequence(AnimSequence, Mesh, bSaveSkeletalMesh);

				// Save to disk
				Exporter->WriteToFile(*ExportFilename);
			}
		}
	}

	static FbxNode* FindCurveNodeRecursive(FbxNode* NodeToQuery, const FString& InCurveNodeName)
	{
		if (InCurveNodeName == UTF8_TO_TCHAR(NodeToQuery->GetName()) && NodeToQuery->GetNodeAttribute() && NodeToQuery->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eMesh)
		{
			return NodeToQuery;
		}

		const int32 NodeCount = NodeToQuery->GetChildCount();
		for (int32 NodeIndex = 0; NodeIndex < NodeCount; ++NodeIndex)
		{
			FbxNode* FoundNode = FindCurveNodeRecursive(NodeToQuery->GetChild(NodeIndex), InCurveNodeName);
			if (FoundNode != nullptr)
			{
				return FoundNode;
			}
		}

		return nullptr;
	}

	static FbxNode* FindCurveNode(UnFbx::FFbxImporter* FbxImporter, const FString& InCurveNodeName)
	{
		FbxNode *RootNode = FbxImporter->Scene->GetRootNode();
		return FindCurveNodeRecursive(RootNode, InCurveNodeName);
	}

	bool ImportCurveTableFromNode(const FString& InFbxFilename, const FString& InCurveNodeName, UCurveTable* InOutCurveTable, float& OutPreRoll)
	{
		UnFbx::FFbxImporter* FbxImporter = UnFbx::FFbxImporter::GetInstance();

		const FString FileExtension = FPaths::GetExtension(InFbxFilename);
		if (FbxImporter->ImportFromFile(*InFbxFilename, FileExtension, true))
		{
			if (FbxImporter->Scene != nullptr)
			{
				// merge animation layer at first (@TODO: do I need to do this?)
				FbxAnimStack* AnimStack = FbxImporter->Scene->GetMember<FbxAnimStack>(0);
				if (AnimStack != nullptr)
				{
					FbxImporter->MergeAllLayerAnimation(AnimStack, FbxTime::GetFrameRate(FbxImporter->Scene->GetGlobalSettings().GetTimeMode()));

					FbxTimeSpan AnimTimeSpan = FbxImporter->GetAnimationTimeSpan(FbxImporter->Scene->GetRootNode(), AnimStack, DEFAULT_SAMPLERATE);

					// Grab the start time, as we might have a preroll
					OutPreRoll = FMath::Abs((float)AnimTimeSpan.GetStart().GetSecondDouble());

					// @TODO: do I need this check?
					FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>(0);
					if (AnimLayer != nullptr)
					{
						FbxNode* Node = FindCurveNode(FbxImporter, InCurveNodeName);
						if (Node != nullptr)
						{
							// We have a node, so clear the curve table
							InOutCurveTable->RowMap.Empty();

							FbxGeometry* Geometry = (FbxGeometry*)Node->GetNodeAttribute();
							int32 BlendShapeDeformerCount = Geometry->GetDeformerCount(FbxDeformer::eBlendShape);
							for (int32 BlendShapeIndex = 0; BlendShapeIndex < BlendShapeDeformerCount; ++BlendShapeIndex)
							{
								FbxBlendShape* BlendShape = (FbxBlendShape*)Geometry->GetDeformer(BlendShapeIndex, FbxDeformer::eBlendShape);

								const int32 BlendShapeChannelCount = BlendShape->GetBlendShapeChannelCount();

								FString BlendShapeName = UTF8_TO_TCHAR(FbxImporter->MakeName(BlendShape->GetName()));

								for (int32 ChannelIndex = 0; ChannelIndex < BlendShapeChannelCount; ++ChannelIndex)
								{
									FbxBlendShapeChannel* Channel = BlendShape->GetBlendShapeChannel(ChannelIndex);

									if (Channel)
									{
										FString ChannelName = UTF8_TO_TCHAR(FbxImporter->MakeName(Channel->GetName()));

										// Maya adds the name of the blendshape and an underscore to the front of the channel name, so remove it
										if (ChannelName.StartsWith(BlendShapeName))
										{
											ChannelName = ChannelName.Right(ChannelName.Len() - (BlendShapeName.Len() + 1));
										}

										FbxAnimCurve* Curve = Geometry->GetShapeChannel(BlendShapeIndex, ChannelIndex, (FbxAnimLayer*)AnimStack->GetMember(0));
										if (Curve)
										{
											FRichCurve& RichCurve = *InOutCurveTable->RowMap.Add(*ChannelName, new FRichCurve());
											RichCurve.Reset();

											FbxImporter->ImportCurve(Curve, RichCurve, AnimTimeSpan, 0.01f);
										}
									}
								}
							}

							return true;
						}
					}
				}
			}
		}

		FbxImporter->ReleaseScene();

		return false;
	}
}
