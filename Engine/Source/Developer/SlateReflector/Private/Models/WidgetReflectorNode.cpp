// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Models/WidgetReflectorNode.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonTypes.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Layout/Visibility.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/WidgetPath.h"
#include "AssetRegistryModule.h"
#include "Types/ReflectionMetadata.h"

#define LOCTEXT_NAMESPACE "WidgetReflectorNode"

/**
 * -----------------------------------------------------------------------------
 * FWidgetReflectorNodeBase
 * -----------------------------------------------------------------------------
 */
FWidgetReflectorNodeBase::FWidgetReflectorNodeBase()
	: Tint(FLinearColor::White)
{
	HitTestInfo.IsHitTestVisible = false;
	HitTestInfo.AreChildrenHitTestVisible = false;
}

FWidgetReflectorNodeBase::FWidgetReflectorNodeBase(const FArrangedWidget& InArrangedWidget)
	: WidgetGeometry(InArrangedWidget.Geometry)
	, Tint(FLinearColor::White)
{
	const EVisibility WidgetVisibility = InArrangedWidget.Widget->GetVisibility();
	HitTestInfo.IsHitTestVisible = WidgetVisibility.IsHitTestVisible();
	HitTestInfo.AreChildrenHitTestVisible = WidgetVisibility.AreChildrenHitTestVisible();
}

const FGeometry& FWidgetReflectorNodeBase::GetGeometry() const
{
	return WidgetGeometry;
}

FSlateLayoutTransform FWidgetReflectorNodeBase::GetAccumulatedLayoutTransform() const
{
	return WidgetGeometry.GetAccumulatedLayoutTransform();
}

const FSlateRenderTransform& FWidgetReflectorNodeBase::GetAccumulatedRenderTransform() const
{
	return WidgetGeometry.GetAccumulatedRenderTransform();
}

const FVector2D& FWidgetReflectorNodeBase::GetLocalSize() const
{
	return WidgetGeometry.GetLocalSize();
}

const FWidgetHitTestInfo& FWidgetReflectorNodeBase::GetHitTestInfo() const
{
	return HitTestInfo;
}

const FLinearColor& FWidgetReflectorNodeBase::GetTint() const
{
	return Tint;
}

void FWidgetReflectorNodeBase::SetTint(const FLinearColor& InTint)
{
	Tint = InTint;
}

void FWidgetReflectorNodeBase::AddChildNode(TSharedRef<FWidgetReflectorNodeBase> InChildNode)
{
	ChildNodes.Add(MoveTemp(InChildNode));
}

const TArray<TSharedRef<FWidgetReflectorNodeBase>>& FWidgetReflectorNodeBase::GetChildNodes() const
{
	return ChildNodes;
}


/**
 * -----------------------------------------------------------------------------
 * FLiveWidgetReflectorNode
 * -----------------------------------------------------------------------------
 */
TSharedRef<FLiveWidgetReflectorNode> FLiveWidgetReflectorNode::Create(const FArrangedWidget& InArrangedWidget)
{
	return MakeShareable(new FLiveWidgetReflectorNode(InArrangedWidget));
}

FLiveWidgetReflectorNode::FLiveWidgetReflectorNode(const FArrangedWidget& InArrangedWidget)
	: FWidgetReflectorNodeBase(InArrangedWidget)
	, Widget(InArrangedWidget.Widget)
{
}

EWidgetReflectorNodeType FLiveWidgetReflectorNode::GetNodeType() const
{
	return EWidgetReflectorNodeType::Live;
}

TSharedPtr<SWidget> FLiveWidgetReflectorNode::GetLiveWidget() const
{
	return Widget.Pin();
}

FText FLiveWidgetReflectorNode::GetWidgetType() const
{
	return FWidgetReflectorNodeUtils::GetWidgetType(Widget.Pin());
}

FText FLiveWidgetReflectorNode::GetWidgetVisibilityText() const
{
	return FWidgetReflectorNodeUtils::GetWidgetVisibilityText(Widget.Pin());
}

FText FLiveWidgetReflectorNode::GetWidgetClippingText() const
{
	return FWidgetReflectorNodeUtils::GetWidgetClippingText(Widget.Pin());
}

bool FLiveWidgetReflectorNode::GetWidgetFocusable() const
{
	return FWidgetReflectorNodeUtils::GetWidgetFocusable(Widget.Pin());
}

FText FLiveWidgetReflectorNode::GetWidgetReadableLocation() const
{
	return FWidgetReflectorNodeUtils::GetWidgetReadableLocation(Widget.Pin());
}

FString FLiveWidgetReflectorNode::GetWidgetFile() const
{
	return FWidgetReflectorNodeUtils::GetWidgetFile(Widget.Pin());
}

int32 FLiveWidgetReflectorNode::GetWidgetLineNumber() const
{
	return FWidgetReflectorNodeUtils::GetWidgetLineNumber(Widget.Pin());
}

FAssetData FLiveWidgetReflectorNode::GetWidgetAssetData() const
{
	return FWidgetReflectorNodeUtils::GetWidgetAssetData(Widget.Pin());
}

FVector2D FLiveWidgetReflectorNode::GetWidgetDesiredSize() const
{
	return FWidgetReflectorNodeUtils::GetWidgetDesiredSize(Widget.Pin());
}

FSlateColor FLiveWidgetReflectorNode::GetWidgetForegroundColor() const
{
	return FWidgetReflectorNodeUtils::GetWidgetForegroundColor(Widget.Pin());
}

FString FLiveWidgetReflectorNode::GetWidgetAddress() const
{
	return FWidgetReflectorNodeUtils::GetWidgetAddress(Widget.Pin());
}

bool FLiveWidgetReflectorNode::GetWidgetEnabled() const
{
	return FWidgetReflectorNodeUtils::GetWidgetEnabled(Widget.Pin());
}


/**
 * -----------------------------------------------------------------------------
 * FSnapshotWidgetReflectorNode
 * -----------------------------------------------------------------------------
 */
TSharedRef<FSnapshotWidgetReflectorNode> FSnapshotWidgetReflectorNode::Create()
{
	return MakeShareable(new FSnapshotWidgetReflectorNode());
}

TSharedRef<FSnapshotWidgetReflectorNode> FSnapshotWidgetReflectorNode::Create(const FArrangedWidget& InWidgetGeometry)
{
	return MakeShareable(new FSnapshotWidgetReflectorNode(InWidgetGeometry));
}

FSnapshotWidgetReflectorNode::FSnapshotWidgetReflectorNode()
	: CachedWidgetLineNumber(0)
	, CachedWidgetEnabled(false)
{
}

FSnapshotWidgetReflectorNode::FSnapshotWidgetReflectorNode(const FArrangedWidget& InArrangedWidget)
	: FWidgetReflectorNodeBase(InArrangedWidget)
	, CachedWidgetType(FWidgetReflectorNodeUtils::GetWidgetType(InArrangedWidget.Widget))
	, CachedWidgetVisibilityText(FWidgetReflectorNodeUtils::GetWidgetVisibilityText(InArrangedWidget.Widget))
	, bCachedWidgetFocusable(FWidgetReflectorNodeUtils::GetWidgetFocusable(InArrangedWidget.Widget))
	, CachedWidgetClippingText(FWidgetReflectorNodeUtils::GetWidgetClippingText(InArrangedWidget.Widget))
	, CachedWidgetReadableLocation(FWidgetReflectorNodeUtils::GetWidgetReadableLocation(InArrangedWidget.Widget))
	, CachedWidgetFile(FWidgetReflectorNodeUtils::GetWidgetFile(InArrangedWidget.Widget))
	, CachedWidgetLineNumber(FWidgetReflectorNodeUtils::GetWidgetLineNumber(InArrangedWidget.Widget))
	, CachedWidgetAssetData(FWidgetReflectorNodeUtils::GetWidgetAssetData(InArrangedWidget.Widget))
	, CachedWidgetDesiredSize(FWidgetReflectorNodeUtils::GetWidgetDesiredSize(InArrangedWidget.Widget))
	, CachedWidgetForegroundColor(FWidgetReflectorNodeUtils::GetWidgetForegroundColor(InArrangedWidget.Widget))
	, CachedWidgetAddress(FWidgetReflectorNodeUtils::GetWidgetAddress(InArrangedWidget.Widget))
	, CachedWidgetEnabled(FWidgetReflectorNodeUtils::GetWidgetEnabled(InArrangedWidget.Widget))
{
}

EWidgetReflectorNodeType FSnapshotWidgetReflectorNode::GetNodeType() const
{
	return EWidgetReflectorNodeType::Snapshot;
}

TSharedPtr<SWidget> FSnapshotWidgetReflectorNode::GetLiveWidget() const
{
	return nullptr;
}

FText FSnapshotWidgetReflectorNode::GetWidgetType() const
{
	return CachedWidgetType;
}

FText FSnapshotWidgetReflectorNode::GetWidgetVisibilityText() const
{
	return CachedWidgetVisibilityText;
}

bool FSnapshotWidgetReflectorNode::GetWidgetFocusable() const
{
	return bCachedWidgetFocusable;
}

FText FSnapshotWidgetReflectorNode::GetWidgetClippingText() const
{
	return CachedWidgetClippingText;
}

FText FSnapshotWidgetReflectorNode::GetWidgetReadableLocation() const
{
	return CachedWidgetReadableLocation;
}

FString FSnapshotWidgetReflectorNode::GetWidgetFile() const
{
	return CachedWidgetFile;
}

int32 FSnapshotWidgetReflectorNode::GetWidgetLineNumber() const
{
	return CachedWidgetLineNumber;
}

FAssetData FSnapshotWidgetReflectorNode::GetWidgetAssetData() const
{
	return CachedWidgetAssetData;
}

FVector2D FSnapshotWidgetReflectorNode::GetWidgetDesiredSize() const
{
	return CachedWidgetDesiredSize;
}

FSlateColor FSnapshotWidgetReflectorNode::GetWidgetForegroundColor() const
{
	return CachedWidgetForegroundColor;
}

FString FSnapshotWidgetReflectorNode::GetWidgetAddress() const
{
	return CachedWidgetAddress;
}

bool FSnapshotWidgetReflectorNode::GetWidgetEnabled() const
{
	return CachedWidgetEnabled;
}

TSharedRef<FJsonValue> FSnapshotWidgetReflectorNode::ToJson(const TSharedRef<FSnapshotWidgetReflectorNode>& RootSnapshotNode)
{
	struct Internal
	{
		static TSharedRef<FJsonValue> CreateVector2DJsonValue(const FVector2D& InVec2D)
		{
			TArray<TSharedPtr<FJsonValue>> StructJsonArray;
			StructJsonArray.Add(MakeShareable(new FJsonValueNumber(InVec2D.X)));
			StructJsonArray.Add(MakeShareable(new FJsonValueNumber(InVec2D.Y)));
			return MakeShareable(new FJsonValueArray(StructJsonArray));
		}

		static TSharedRef<FJsonValue> CreateMatrix2x2JsonValue(const FMatrix2x2& InMatrix)
		{
			float m00, m01, m10, m11;
			InMatrix.GetMatrix(m00, m01, m10, m11);

			TArray<TSharedPtr<FJsonValue>> StructJsonArray;
			StructJsonArray.Add(MakeShareable(new FJsonValueNumber(m00)));
			StructJsonArray.Add(MakeShareable(new FJsonValueNumber(m01)));
			StructJsonArray.Add(MakeShareable(new FJsonValueNumber(m10)));
			StructJsonArray.Add(MakeShareable(new FJsonValueNumber(m11)));
			return MakeShareable(new FJsonValueArray(StructJsonArray));
		}

		static TSharedRef<FJsonValue> CreateSlateLayoutTransformJsonValue(const FSlateLayoutTransform& InLayoutTransform)
		{
			TSharedRef<FJsonObject> StructJsonObject = MakeShareable(new FJsonObject());
			StructJsonObject->SetNumberField(TEXT("Scale"), InLayoutTransform.GetScale());
			StructJsonObject->SetField(TEXT("Translation"), CreateVector2DJsonValue(InLayoutTransform.GetTranslation()));
			return MakeShareable(new FJsonValueObject(StructJsonObject));
		}

		static TSharedRef<FJsonValue> CreateSlateRenderTransformJsonValue(const FSlateRenderTransform& InRenderTransform)
		{
			TSharedRef<FJsonObject> StructJsonObject = MakeShareable(new FJsonObject());
			StructJsonObject->SetField(TEXT("Matrix"), CreateMatrix2x2JsonValue(InRenderTransform.GetMatrix()));
			StructJsonObject->SetField(TEXT("Translation"), CreateVector2DJsonValue(InRenderTransform.GetTranslation()));
			return MakeShareable(new FJsonValueObject(StructJsonObject));
		}

		static TSharedRef<FJsonValue> CreateLinearColorJsonValue(const FLinearColor& InColor)
		{
			TArray<TSharedPtr<FJsonValue>> StructJsonArray;
			StructJsonArray.Add(MakeShareable(new FJsonValueNumber(InColor.R)));
			StructJsonArray.Add(MakeShareable(new FJsonValueNumber(InColor.G)));
			StructJsonArray.Add(MakeShareable(new FJsonValueNumber(InColor.B)));
			StructJsonArray.Add(MakeShareable(new FJsonValueNumber(InColor.A)));
			return MakeShareable(new FJsonValueArray(StructJsonArray));
		}

		static TSharedRef<FJsonValue> CreateSlateColorJsonValue(const FSlateColor& InColor)
		{
			const bool bIsColorSpecified = InColor.IsColorSpecified();
			const FLinearColor ColorToUse = (bIsColorSpecified) ? InColor.GetSpecifiedColor() : FLinearColor::White;

			TSharedRef<FJsonObject> StructJsonObject = MakeShareable(new FJsonObject());
			StructJsonObject->SetBoolField(TEXT("IsColorSpecified"), bIsColorSpecified);
			StructJsonObject->SetField(TEXT("Color"), CreateLinearColorJsonValue(ColorToUse));
			return MakeShareable(new FJsonValueObject(StructJsonObject));
		}

		static TSharedRef<FJsonValue> CreateWidgetHitTestInfoJsonValue(const FWidgetHitTestInfo& InHitTestInfo)
		{
			TSharedRef<FJsonObject> StructJsonObject = MakeShareable(new FJsonObject());
			StructJsonObject->SetBoolField(TEXT("IsHitTestVisible"), InHitTestInfo.IsHitTestVisible);
			StructJsonObject->SetBoolField(TEXT("AreChildrenHitTestVisible"), InHitTestInfo.AreChildrenHitTestVisible);
			return MakeShareable(new FJsonValueObject(StructJsonObject));
		}
	};

	TSharedRef<FJsonObject> RootJsonObject = MakeShareable(new FJsonObject());

	RootJsonObject->SetField(TEXT("AccumulatedLayoutTransform"), Internal::CreateSlateLayoutTransformJsonValue(RootSnapshotNode->GetAccumulatedLayoutTransform()));
	RootJsonObject->SetField(TEXT("AccumulatedRenderTransform"), Internal::CreateSlateRenderTransformJsonValue(RootSnapshotNode->GetAccumulatedRenderTransform()));
	RootJsonObject->SetField(TEXT("LocalSize"), Internal::CreateVector2DJsonValue(RootSnapshotNode->GetLocalSize()));
	RootJsonObject->SetField(TEXT("HitTestInfo"), Internal::CreateWidgetHitTestInfoJsonValue(RootSnapshotNode->HitTestInfo));
	RootJsonObject->SetStringField(TEXT("WidgetType"), RootSnapshotNode->CachedWidgetType.ToString());
	RootJsonObject->SetStringField(TEXT("WidgetVisibilityText"), RootSnapshotNode->CachedWidgetVisibilityText.ToString());
	RootJsonObject->SetBoolField(TEXT("WidgetFocusable"), RootSnapshotNode->bCachedWidgetFocusable);
	RootJsonObject->SetStringField(TEXT("WidgetReadableLocation"), RootSnapshotNode->CachedWidgetReadableLocation.ToString());
	RootJsonObject->SetStringField(TEXT("WidgetFile"), RootSnapshotNode->CachedWidgetFile);
	RootJsonObject->SetNumberField(TEXT("WidgetLineNumber"), RootSnapshotNode->CachedWidgetLineNumber);
	RootJsonObject->SetStringField(TEXT("WidgetAssetPath"), RootSnapshotNode->CachedWidgetAssetData.ObjectPath.ToString());
	RootJsonObject->SetField(TEXT("WidgetDesiredSize"), Internal::CreateVector2DJsonValue(RootSnapshotNode->CachedWidgetDesiredSize));
	RootJsonObject->SetField(TEXT("WidgetForegroundColor"), Internal::CreateSlateColorJsonValue(RootSnapshotNode->CachedWidgetForegroundColor));
	RootJsonObject->SetStringField(TEXT("WidgetAddress"), RootSnapshotNode->CachedWidgetAddress);
	RootJsonObject->SetBoolField(TEXT("WidgetEnabled"), RootSnapshotNode->CachedWidgetEnabled);

	TArray<TSharedPtr<FJsonValue>> ChildNodesJsonArray;
	for (const auto& ChildReflectorNode : RootSnapshotNode->ChildNodes)
	{
		check(ChildReflectorNode->GetNodeType() == EWidgetReflectorNodeType::Snapshot);
		ChildNodesJsonArray.Add(FSnapshotWidgetReflectorNode::ToJson(StaticCastSharedRef<FSnapshotWidgetReflectorNode>(ChildReflectorNode)));
	}
	RootJsonObject->SetArrayField(TEXT("ChildNodes"), ChildNodesJsonArray);

	return MakeShareable(new FJsonValueObject(RootJsonObject));
}

TSharedRef<FSnapshotWidgetReflectorNode> FSnapshotWidgetReflectorNode::FromJson(const TSharedRef<FJsonValue>& RootJsonValue)
{
	struct Internal
	{
		static FVector2D ParseVector2DJsonValue(const TSharedPtr<FJsonValue>& InJsonValue)
		{
			check(InJsonValue.IsValid());
			const TArray<TSharedPtr<FJsonValue>>& StructJsonArray = InJsonValue->AsArray();
			check(StructJsonArray.Num() == 2);

			return FVector2D(
				StructJsonArray[0]->AsNumber(), 
				StructJsonArray[1]->AsNumber()
				);
		}

		static FMatrix2x2 ParseMatrix2x2JsonValue(const TSharedPtr<FJsonValue>& InJsonValue)
		{
			check(InJsonValue.IsValid());
			const TArray<TSharedPtr<FJsonValue>>& StructJsonArray = InJsonValue->AsArray();
			check(StructJsonArray.Num() == 4);

			return FMatrix2x2(
				StructJsonArray[0]->AsNumber(), 
				StructJsonArray[1]->AsNumber(), 
				StructJsonArray[2]->AsNumber(), 
				StructJsonArray[3]->AsNumber()
				);
		}

		static FSlateLayoutTransform ParseSlateLayoutTransformJsonValue(const TSharedPtr<FJsonValue>& InJsonValue)
		{
			check(InJsonValue.IsValid());
			const TSharedPtr<FJsonObject>& StructJsonObject = InJsonValue->AsObject();
			check(StructJsonObject.IsValid());

			return FSlateLayoutTransform(
				StructJsonObject->GetNumberField(TEXT("Scale")),
				ParseVector2DJsonValue(StructJsonObject->GetField<EJson::None>(TEXT("Translation")))
				);
		}

		static FSlateRenderTransform ParseSlateRenderTransformJsonValue(const TSharedPtr<FJsonValue>& InJsonValue)
		{
			check(InJsonValue.IsValid());
			const TSharedPtr<FJsonObject>& StructJsonObject = InJsonValue->AsObject();
			check(StructJsonObject.IsValid());

			return FSlateRenderTransform(
				ParseMatrix2x2JsonValue(StructJsonObject->GetField<EJson::None>(TEXT("Matrix"))),
				ParseVector2DJsonValue(StructJsonObject->GetField<EJson::None>(TEXT("Translation")))
				);
		}

		static FLinearColor ParseLinearColorJsonValue(const TSharedPtr<FJsonValue>& InJsonValue)
		{
			check(InJsonValue.IsValid());
			const TArray<TSharedPtr<FJsonValue>>& StructJsonArray = InJsonValue->AsArray();
			check(StructJsonArray.Num() == 4);

			return FLinearColor(
				StructJsonArray[0]->AsNumber(), 
				StructJsonArray[1]->AsNumber(), 
				StructJsonArray[2]->AsNumber(), 
				StructJsonArray[3]->AsNumber()
				);
		}

		static FSlateColor ParseSlateColorJsonValue(const TSharedPtr<FJsonValue>& InJsonValue)
		{
			check(InJsonValue.IsValid());
			const TSharedPtr<FJsonObject>& StructJsonObject = InJsonValue->AsObject();
			check(StructJsonObject.IsValid());

			const bool bIsColorSpecified = StructJsonObject->GetBoolField(TEXT("IsColorSpecified"));
			if (bIsColorSpecified)
			{
				return FSlateColor(ParseLinearColorJsonValue(StructJsonObject->GetField<EJson::None>(TEXT("Color"))));
			}
			else
			{
				return FSlateColor::UseForeground();
			}
		}

		static FWidgetHitTestInfo ParseWidgetHitTestInfoJsonValue(const TSharedPtr<FJsonValue>& InJsonValue)
		{
			check(InJsonValue.IsValid());
			const TSharedPtr<FJsonObject>& StructJsonObject = InJsonValue->AsObject();
			check(StructJsonObject.IsValid());

			FWidgetHitTestInfo HitTestInfo;
			HitTestInfo.IsHitTestVisible = StructJsonObject->GetBoolField(TEXT("IsHitTestVisible"));
			HitTestInfo.AreChildrenHitTestVisible = StructJsonObject->GetBoolField(TEXT("AreChildrenHitTestVisible"));
			return HitTestInfo;
		}
	};

	const TSharedPtr<FJsonObject>& RootJsonObject = RootJsonValue->AsObject();
	check(RootJsonObject.IsValid());

	auto RootSnapshotNode = FSnapshotWidgetReflectorNode::Create();

	const FSlateLayoutTransform LayoutTransform = Internal::ParseSlateLayoutTransformJsonValue(RootJsonObject->GetField<EJson::None>(TEXT("AccumulatedLayoutTransform")));
	const FSlateRenderTransform RenderTransform = Internal::ParseSlateRenderTransformJsonValue(RootJsonObject->GetField<EJson::None>(TEXT("AccumulatedRenderTransform")));
	const FVector2D LocalSize = Internal::ParseVector2DJsonValue(RootJsonObject->GetField<EJson::None>(TEXT("LocalSize")));
	RootSnapshotNode->WidgetGeometry = FGeometry::MakeRoot(LocalSize, LayoutTransform, RenderTransform);

	RootSnapshotNode->HitTestInfo = Internal::ParseWidgetHitTestInfoJsonValue(RootJsonObject->GetField<EJson::None>(TEXT("HitTestInfo")));
	RootSnapshotNode->CachedWidgetType = FText::FromString(RootJsonObject->GetStringField(TEXT("WidgetType")));
	RootSnapshotNode->CachedWidgetVisibilityText = FText::FromString(RootJsonObject->GetStringField(TEXT("WidgetVisibilityText")));
	RootSnapshotNode->bCachedWidgetFocusable = RootJsonObject->GetBoolField(TEXT("WidgetFocusable"));
	RootSnapshotNode->CachedWidgetReadableLocation = FText::FromString(RootJsonObject->GetStringField(TEXT("WidgetReadableLocation")));
	RootSnapshotNode->CachedWidgetFile = RootJsonObject->GetStringField(TEXT("WidgetFile"));
	RootSnapshotNode->CachedWidgetLineNumber = RootJsonObject->GetIntegerField(TEXT("WidgetLineNumber"));
	RootSnapshotNode->CachedWidgetDesiredSize = Internal::ParseVector2DJsonValue(RootJsonObject->GetField<EJson::None>(TEXT("WidgetDesiredSize")));
	RootSnapshotNode->CachedWidgetForegroundColor = Internal::ParseSlateColorJsonValue(RootJsonObject->GetField<EJson::None>(TEXT("WidgetForegroundColor")));
	RootSnapshotNode->CachedWidgetAddress = RootJsonObject->GetStringField(TEXT("WidgetAddress"));
	RootSnapshotNode->CachedWidgetEnabled = RootJsonObject->GetBoolField(TEXT("WidgetEnabled"));

	FName AssetPath(*RootJsonObject->GetStringField(TEXT("WidgetAssetPath")));
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	RootSnapshotNode->CachedWidgetAssetData = AssetRegistry.GetAssetByObjectPath(AssetPath);

	const TArray<TSharedPtr<FJsonValue>>& ChildNodesJsonArray = RootJsonObject->GetArrayField(TEXT("ChildNodes"));
	for (const TSharedPtr<FJsonValue>& ChildNodeJsonValue : ChildNodesJsonArray)
	{
		RootSnapshotNode->AddChildNode(FSnapshotWidgetReflectorNode::FromJson(ChildNodeJsonValue.ToSharedRef()));
	}

	return RootSnapshotNode;
}

/**
 * -----------------------------------------------------------------------------
 * FWidgetReflectorNodeUtils
 * -----------------------------------------------------------------------------
 */
TSharedRef<FLiveWidgetReflectorNode> FWidgetReflectorNodeUtils::NewLiveNode(const FArrangedWidget& InWidgetGeometry)
{
	return StaticCastSharedRef<FLiveWidgetReflectorNode>(NewNode(EWidgetReflectorNodeType::Live, InWidgetGeometry));
}

TSharedRef<FLiveWidgetReflectorNode> FWidgetReflectorNodeUtils::NewLiveNodeTreeFrom(const FArrangedWidget& InWidgetGeometry)
{
	return StaticCastSharedRef<FLiveWidgetReflectorNode>(NewNodeTreeFrom(EWidgetReflectorNodeType::Live, InWidgetGeometry));
}

TSharedRef<FSnapshotWidgetReflectorNode> FWidgetReflectorNodeUtils::NewSnapshotNode(const FArrangedWidget& InWidgetGeometry)
{
	return StaticCastSharedRef<FSnapshotWidgetReflectorNode>(NewNode(EWidgetReflectorNodeType::Snapshot, InWidgetGeometry));
}

TSharedRef<FSnapshotWidgetReflectorNode> FWidgetReflectorNodeUtils::NewSnapshotNodeTreeFrom(const FArrangedWidget& InWidgetGeometry)
{
	return StaticCastSharedRef<FSnapshotWidgetReflectorNode>(NewNodeTreeFrom(EWidgetReflectorNodeType::Snapshot, InWidgetGeometry));
}

TSharedRef<FWidgetReflectorNodeBase> FWidgetReflectorNodeUtils::NewNode(const EWidgetReflectorNodeType InNodeType, const FArrangedWidget& InWidgetGeometry)
{
	switch (InNodeType)
	{
	case EWidgetReflectorNodeType::Live:
		return FLiveWidgetReflectorNode::Create(InWidgetGeometry);
	case EWidgetReflectorNodeType::Snapshot:
		return FSnapshotWidgetReflectorNode::Create(InWidgetGeometry);
	default:
		// Should never reach this point, but we have to return something!
		check(false);
		return FLiveWidgetReflectorNode::Create(InWidgetGeometry);
	}
}

TSharedRef<FWidgetReflectorNodeBase> FWidgetReflectorNodeUtils::NewNodeTreeFrom(const EWidgetReflectorNodeType InNodeType, const FArrangedWidget& InWidgetGeometry)
{
	TSharedRef<FWidgetReflectorNodeBase> NewNodeInstance = NewNode(InNodeType, InWidgetGeometry);

	FArrangedChildren ArrangedChildren(EVisibility::All);
	InWidgetGeometry.Widget->ArrangeChildren(InWidgetGeometry.Geometry, ArrangedChildren);
	
	for (int32 WidgetIndex = 0; WidgetIndex < ArrangedChildren.Num(); ++WidgetIndex)
	{
		// Note that we include both visible and invisible children!
		NewNodeInstance->AddChildNode(NewNodeTreeFrom(InNodeType, ArrangedChildren[WidgetIndex]));
	}

	return NewNodeInstance;
}

void FWidgetReflectorNodeUtils::FindLiveWidgetPath(const TArray<TSharedRef<FWidgetReflectorNodeBase>>& CandidateNodes, const FWidgetPath& WidgetPathToFind, TArray<TSharedRef<FWidgetReflectorNodeBase>>& SearchResult, int32 NodeIndexToFind)
{
	if (NodeIndexToFind < WidgetPathToFind.Widgets.Num())
	{
		const FArrangedWidget& WidgetToFind = WidgetPathToFind.Widgets[NodeIndexToFind];

		for (int32 NodeIndex = 0; NodeIndex < CandidateNodes.Num(); ++NodeIndex)
		{
			if (CandidateNodes[NodeIndex]->GetWidgetAddress() == GetWidgetAddress(WidgetPathToFind.Widgets[NodeIndexToFind].Widget))
			{
				SearchResult.Add(CandidateNodes[NodeIndex]);
				FindLiveWidgetPath(CandidateNodes[NodeIndex]->GetChildNodes(), WidgetPathToFind, SearchResult, NodeIndexToFind + 1);
			}
		}
	}
}

FText FWidgetReflectorNodeUtils::GetWidgetType(const TSharedPtr<SWidget>& InWidget)
{
	return (InWidget.IsValid()) ? FText::FromString(InWidget->GetTypeAsString()) : FText::GetEmpty();
}

FText FWidgetReflectorNodeUtils::GetWidgetVisibilityText(const TSharedPtr<SWidget>& InWidget)
{
	return (InWidget.IsValid()) ? FText::FromString(InWidget->GetVisibility().ToString()) : FText::GetEmpty();
}

bool FWidgetReflectorNodeUtils::GetWidgetFocusable(const TSharedPtr<SWidget>& InWidget)
{
	return InWidget.IsValid() ? InWidget->SupportsKeyboardFocus() : false;
}


FText FWidgetReflectorNodeUtils::GetWidgetClippingText(const TSharedPtr<SWidget>& InWidget)
{
	if ( InWidget.IsValid() )
	{
		switch ( InWidget->GetClipping() )
		{
		case EWidgetClipping::Inherit:
			return LOCTEXT("WidgetClippingNo", "No");
		case EWidgetClipping::ClipToBounds:
			return LOCTEXT("WidgetClippingYes", "Yes");
		case EWidgetClipping::ClipToBoundsAlways:
			return LOCTEXT("WidgetClippingYesAlways", "Yes (Always)");
		case EWidgetClipping::ClipToBoundsWithoutIntersecting:
			return LOCTEXT("WidgetClippingYesWithoutIntersecting", "Yes (No Intersect)");
		case EWidgetClipping::OnDemand:
			return LOCTEXT("WidgetClippingOnDemand", "On Demand");
		}
	}

	return FText::GetEmpty();
}

FText FWidgetReflectorNodeUtils::GetWidgetReadableLocation(const TSharedPtr<SWidget>& InWidget)
{
	return (InWidget.IsValid()) ? FText::FromString(FReflectionMetaData::GetWidgetDebugInfo(InWidget.Get())) : FText::GetEmpty();
}

FString FWidgetReflectorNodeUtils::GetWidgetFile(const TSharedPtr<SWidget>& InWidget)
{
	return (InWidget.IsValid()) ? InWidget->GetCreatedInLocation().GetPlainNameString() : FString();
}

int32 FWidgetReflectorNodeUtils::GetWidgetLineNumber(const TSharedPtr<SWidget>& InWidget)
{
	return (InWidget.IsValid()) ? InWidget->GetCreatedInLocation().GetNumber() : 0;
}

FAssetData FWidgetReflectorNodeUtils::GetWidgetAssetData(const TSharedPtr<SWidget>& InWidget)
{
	if (InWidget.IsValid())
	{
		// UMG widgets have meta-data to help track them
		TSharedPtr<FReflectionMetaData> MetaData = InWidget->GetMetaData<FReflectionMetaData>();
		if (MetaData.IsValid() && MetaData->Asset.Get() != nullptr)
		{
			return FAssetData(MetaData->Asset.Get());
		}
	}

	return FAssetData();
}

FVector2D FWidgetReflectorNodeUtils::GetWidgetDesiredSize(const TSharedPtr<SWidget>& InWidget)
{
	return (InWidget.IsValid()) ? InWidget->GetDesiredSize() : FVector2D::ZeroVector;
}

FString FWidgetReflectorNodeUtils::GetWidgetAddress(const TSharedPtr<SWidget>& InWidget)
{
	return FString::Printf(TEXT("0x%p"), InWidget.Get());
}

FSlateColor FWidgetReflectorNodeUtils::GetWidgetForegroundColor(const TSharedPtr<SWidget>& InWidget)
{
	return (InWidget.IsValid()) ? InWidget->GetForegroundColor() : FSlateColor::UseForeground();
}

bool FWidgetReflectorNodeUtils::GetWidgetEnabled(const TSharedPtr<SWidget>& InWidget)
{
	return (InWidget.IsValid()) ? InWidget->IsEnabled() : false;
}

#undef LOCTEXT_NAMESPACE
