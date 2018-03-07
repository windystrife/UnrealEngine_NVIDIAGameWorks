// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SWidgetSnapshotVisualizer.h"
#include "HAL/FileManager.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/ArrayWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Framework/Layout/ScrollyZoomy.h"
#include "Misc/Base64.h"

#if SLATE_REFLECTOR_HAS_DESKTOP_PLATFORM
#include "DesktopPlatformModule.h"
#endif // SLATE_REFLECTOR_HAS_DESKTOP_PLATFORM

#define LOCTEXT_NAMESPACE "WidgetSnapshotVisualizer"

class SScrollableSnapshotImage : public IScrollableZoomable, public SPanel
{
public:
	class FScrollableSnapshotImageSlot : public TSupportsOneChildMixin<FScrollableSnapshotImageSlot>
	{
	public:
		FScrollableSnapshotImageSlot()
			: TSupportsOneChildMixin<FScrollableSnapshotImageSlot>()
		{
		}
	};

	SLATE_BEGIN_ARGS(SScrollableSnapshotImage)
		: _SnapshotData(nullptr)
		{
			_Visibility = EVisibility::Visible;
		}
		
		SLATE_ARGUMENT(const FWidgetSnapshotData*, SnapshotData)

		SLATE_EVENT(SWidgetSnapshotVisualizer::FOnWidgetPathPicked, OnWidgetPathPicked);

	SLATE_END_ARGS()

	SScrollableSnapshotImage()
		: PhysicalOffset(ForceInitToZero)
		, CachedSize(ForceInitToZero)
		, ChildSlot()
		, ScrollyZoomy(false)
		, SnapshotDataPtr(nullptr)
		, bIsPicking(false)
	{
	}

	void Construct(const FArguments& InArgs)
	{
		SnapshotDataPtr = InArgs._SnapshotData;
		check(SnapshotDataPtr);

		SelectedWindowIndex = INDEX_NONE;

		OnWidgetPathPicked = InArgs._OnWidgetPathPicked;

		ChildSlot
		[
			SNew(SImage)
			.Image(this, &SScrollableSnapshotImage::GetSelectedWindowTextureBrush)
		];
	}

	void SetSelectedWindowIndex(const int32 InIndex)
	{
		SelectedWindowIndex = InIndex;
		PickedWidgets.Reset();
		PhysicalOffset = FVector2D::ZeroVector;
	}

	int32 GetSelectedWindowIndex() const
	{
		return SelectedWindowIndex;
	}

	const FSlateBrush* GetSelectedWindowTextureBrush() const
	{
		return SnapshotDataPtr->GetBrush(SelectedWindowIndex);
	}

	void SetIsPicking(const bool InIsPicking)
	{
		bIsPicking = InIsPicking;
	}

	bool GetIsPicking() const
	{
		return bIsPicking;
	}

	void SetSelectedWidgets(const TArray<TSharedRef<FWidgetReflectorNodeBase>>& InSelectedWidgets)
	{
		SelectedWidgets = InSelectedWidgets;
	}

	virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override
	{
		CachedSize = AllottedGeometry.GetLocalSize();

		const TSharedRef<SWidget>& ChildWidget = ChildSlot.GetWidget();
		if (ChildWidget->GetVisibility() != EVisibility::Collapsed)
		{
			const FVector2D& WidgetDesiredSize = ChildWidget->GetDesiredSize();

			// Clamp the pan offset based on our current geometry
			SScrollableSnapshotImage* const NonConstThis = const_cast<SScrollableSnapshotImage*>(this);
			NonConstThis->ClampViewOffset(WidgetDesiredSize, CachedSize);

			ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(ChildWidget, PhysicalOffset, WidgetDesiredSize));
		}
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		FVector2D ThisDesiredSize = FVector2D::ZeroVector;

		const TSharedRef<SWidget>& ChildWidget = ChildSlot.GetWidget();
		if (ChildWidget->GetVisibility() != EVisibility::Collapsed)
		{
			ThisDesiredSize = ChildWidget->GetDesiredSize();
		}

		return ThisDesiredSize;
	}

	virtual FChildren* GetChildren() override
	{
		return &ChildSlot;
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		ScrollyZoomy.Tick(InDeltaTime, *this);
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return ScrollyZoomy.OnMouseButtonDown(MouseEvent);
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return ScrollyZoomy.OnMouseButtonUp(AsShared(), MyGeometry, MouseEvent);
	}
		
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		struct FWidgetPicker
		{
			static bool FindWidgetsUnderPoint(const FVector2D& InHitTestPoint, const FVector2D& InWindowPosition, const TSharedRef<FWidgetReflectorNodeBase>& InWidget, TArray<TSharedRef<FWidgetReflectorNodeBase>>& OutWidgets)
			{
				const bool bNeedsHitTesting = InWidget->GetHitTestInfo().IsHitTestVisible || InWidget->GetHitTestInfo().AreChildrenHitTestVisible;
				if (bNeedsHitTesting)
				{
					const FSlateRect HitTestRect = FSlateRect::FromPointAndExtent(
						InWidget->GetAccumulatedLayoutTransform().GetTranslation() - InWindowPosition, 
						TransformPoint(InWidget->GetAccumulatedLayoutTransform().GetScale(), InWidget->GetLocalSize())
						);

					if (HitTestRect.ContainsPoint(InHitTestPoint))
					{
						OutWidgets.Add(InWidget);

						if (InWidget->GetHitTestInfo().AreChildrenHitTestVisible)
						{
							for (const auto& ChildWidget : InWidget->GetChildNodes())
							{
								if (FindWidgetsUnderPoint(InHitTestPoint, InWindowPosition, ChildWidget, OutWidgets))
								{
									return true;
								}
							}
						}

						return InWidget->GetHitTestInfo().IsHitTestVisible;
					}
				}

				return false;
			}
		};

		if (bIsPicking)
		{
			// We need to pick in the snapshot window space, so convert the mouse co-ordinates to be relative to our top-left position
			const FVector2D& ScreenMousePos = MouseEvent.GetScreenSpacePosition();
			const FVector2D LocalMousePos = MyGeometry.AbsoluteToLocal(ScreenMousePos);
			const FVector2D ScrolledPos = LocalMousePos - PhysicalOffset;

			PickedWidgets.Reset();

			TSharedPtr<FWidgetReflectorNodeBase> Window = SnapshotDataPtr->GetWindow(SelectedWindowIndex);
			if (Window.IsValid())
			{
				FWidgetPicker::FindWidgetsUnderPoint(
					ScrolledPos, 
					Window->GetAccumulatedLayoutTransform().GetTranslation(), 
					Window.ToSharedRef(), 
					PickedWidgets
					);
			}

			if (PickedWidgets.Num() > 0)
			{
				OnWidgetPathPicked.ExecuteIfBound(PickedWidgets);
			}
		}

		return ScrollyZoomy.OnMouseMove(AsShared(), *this, MyGeometry, MouseEvent);
	}

	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
	{
		ScrollyZoomy.OnMouseLeave(AsShared(), MouseEvent);
	}
		
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return ScrollyZoomy.OnMouseWheel(MouseEvent, *this);
	}

	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override
	{
		FCursorReply Reply = ScrollyZoomy.OnCursorQuery();
		
		if (!Reply.IsEventHandled() && !bIsPicking)
		{
			Reply = FCursorReply::Cursor(EMouseCursor::GrabHand);
		}

		return Reply;
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		LayerId = SPanel::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
		LayerId = ScrollyZoomy.PaintSoftwareCursorIfNeeded(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId);

		TSharedPtr<FWidgetReflectorNodeBase> Window = SnapshotDataPtr->GetWindow(SelectedWindowIndex);
		if (Window.IsValid())
		{
			const FVector2D RootDrawOffset = PhysicalOffset - Window->GetAccumulatedLayoutTransform().GetTranslation();
			if (bIsPicking)
			{
				const FLinearColor TopmostWidgetColor(1.0f, 0.0f, 0.0f);
				const FLinearColor LeafmostWidgetColor(0.0f, 1.0f, 0.0f);

				for (int32 WidgetIndex = 0; WidgetIndex < PickedWidgets.Num(); ++WidgetIndex)
				{
					const TSharedRef<FWidgetReflectorNodeBase>& PickedWidget = PickedWidgets[WidgetIndex];
					const float ColorFactor = static_cast<float>(WidgetIndex)/PickedWidgets.Num();
					const FLinearColor Tint(1.0f - ColorFactor, ColorFactor, 0.0f, 1.0f);

					FSlateDrawElement::MakeBox(
						OutDrawElements,
						++LayerId,
						AllottedGeometry.ToPaintGeometry(RootDrawOffset + PickedWidget->GetAccumulatedLayoutTransform().GetTranslation(), TransformPoint(PickedWidget->GetAccumulatedLayoutTransform().GetScale(), PickedWidget->GetLocalSize())),
						FCoreStyle::Get().GetBrush(TEXT("Debug.Border")),
						ESlateDrawEffect::None,
						FMath::Lerp(TopmostWidgetColor, LeafmostWidgetColor, ColorFactor)
					);
				}
			}
			else
			{
				for (const auto& SelectedWidget : SelectedWidgets)
				{
					FSlateDrawElement::MakeBox(
						OutDrawElements,
						++LayerId,
						AllottedGeometry.ToPaintGeometry(RootDrawOffset + SelectedWidget->GetAccumulatedLayoutTransform().GetTranslation(), TransformPoint(SelectedWidget->GetAccumulatedLayoutTransform().GetScale(), SelectedWidget->GetLocalSize())),
						FCoreStyle::Get().GetBrush(TEXT("Debug.Border")),
						ESlateDrawEffect::None,
						SelectedWidget->GetTint()
					);
				}
			}
		}

		return LayerId;
	}

	virtual bool ScrollBy(const FVector2D& Offset) override
	{
		const FVector2D PrevPhysicalOffset = PhysicalOffset;
		PhysicalOffset += Offset;

		const TSharedRef<SWidget>& ChildWidget = ChildSlot.GetWidget();
		const FVector2D& WidgetDesiredSize = ChildWidget->GetDesiredSize();
		ClampViewOffset(WidgetDesiredSize, CachedSize);

		return PhysicalOffset != PrevPhysicalOffset;
	}

	virtual bool ZoomBy(const float Amount) override
	{
		return false;
	}

	float GetZoomLevel() const
	{
		return 1.0f;
	}

private:
	void ClampViewOffset(const FVector2D& ViewportSize, const FVector2D& LocalSize)
	{
		PhysicalOffset.X = ClampViewOffsetAxis(ViewportSize.X, LocalSize.X, PhysicalOffset.X);
		PhysicalOffset.Y = ClampViewOffsetAxis(ViewportSize.Y, LocalSize.Y, PhysicalOffset.Y);
	}

	float ClampViewOffsetAxis(const float ViewportSize, const float LocalSize, const float CurrentOffset)
	{
		if (ViewportSize <= LocalSize)
		{
			// If the viewport is smaller than the available size, then we can't be scrolled
			return 0.0f;
		}

		// Given the size of the viewport, and the current size of the window, work how far we can scroll
		// Note: This number is negative since scrolling down/right moves the viewport up/left
		const float MaxScrollOffset = LocalSize - ViewportSize;

		// Clamp the left/top edge
		if (CurrentOffset < MaxScrollOffset)
		{
			return MaxScrollOffset;
		}

		// Clamp the right/bottom edge
		if (CurrentOffset > 0.0f)
		{
			return 0.0f;
		}

		return CurrentOffset;
	}

	FVector2D PhysicalOffset;
	mutable FVector2D CachedSize;

	FScrollableSnapshotImageSlot ChildSlot;
	FScrollyZoomy ScrollyZoomy;

	/** Snapshot data we're visualizing */
	const FWidgetSnapshotData* SnapshotDataPtr;

	/** Index of the window we're currently viewing */
	int32 SelectedWindowIndex;

	SWidgetSnapshotVisualizer::FOnWidgetPathPicked OnWidgetPathPicked;

	bool bIsPicking;
	TArray<TSharedRef<FWidgetReflectorNodeBase>> PickedWidgets;

	TArray<TSharedRef<FWidgetReflectorNodeBase>> SelectedWidgets;
};


FWidgetSnapshotData::~FWidgetSnapshotData()
{
	DestroyBrushes();
}

void FWidgetSnapshotData::ClearSnapshot()
{
	Reset();
}

void FWidgetSnapshotData::TakeSnapshot()
{
	TArray<TSharedRef<SWindow>> VisibleWindows;
	FSlateApplication::Get().GetAllVisibleWindowsOrdered(VisibleWindows);

	CreateSnapshot(VisibleWindows);
}

void FWidgetSnapshotData::CreateSnapshot(const TArray<TSharedRef<SWindow>>& VisibleWindows)
{
	Reset();
	Reserve(VisibleWindows.Num());

	for (const auto& VisibleWindow : VisibleWindows)
	{
		// Snapshot the current state of this window widget hierarchy
		Windows.Add(FWidgetReflectorNodeUtils::NewSnapshotNodeTreeFrom(FArrangedWidget(VisibleWindow, VisibleWindow->GetWindowGeometryInScreen())));

		// Screenshot the current window so we can pick against its current state
		FWidgetSnapshotTextureData& TextureData = WindowTextureData[WindowTextureData.AddDefaulted()];
		FSlateApplication::Get().TakeScreenshot(VisibleWindow, TextureData.ColorData, TextureData.Dimensions);
	}

	CreateBrushes();
}

bool FWidgetSnapshotData::SaveSnapshotToFile(const FString& InFilename) const
{
	TSharedRef<FJsonObject> RootJsonObject = SaveSnapshotAsJson();

	FArchive* const FileAr = IFileManager::Get().CreateFileWriter(*InFilename);
	if (FileAr)
	{
		typedef TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>> FStringWriter;
		typedef TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>> FStringWriterFactory;

		TSharedRef<FStringWriter> Writer = FStringWriterFactory::Create(FileAr);
		FJsonSerializer::Serialize(RootJsonObject, Writer);
		FileAr->Close();
		return true;
	}

	return false;
}

void FWidgetSnapshotData::SaveSnapshotToBuffer(TArray<uint8>& OutData) const
{
	TSharedRef<FJsonObject> RootJsonObject = SaveSnapshotAsJson();

	typedef TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>> FStringWriter;
	typedef TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>> FStringWriterFactory;

	FArrayWriter TmpJsonData;
	TSharedRef<FStringWriter> Writer = FStringWriterFactory::Create(&TmpJsonData);
	FJsonSerializer::Serialize(RootJsonObject, Writer);

	OutData.Reset();
	FMemoryWriter BufferWriter(OutData);

	int32 UncompressedDataSize = TmpJsonData.Num();
	BufferWriter << UncompressedDataSize;

	BufferWriter.SerializeCompressed(TmpJsonData.GetData(), TmpJsonData.Num(), COMPRESS_ZLIB);
}

TSharedRef<FJsonObject> FWidgetSnapshotData::SaveSnapshotAsJson() const
{
	check(Windows.Num() == WindowTextureData.Num());

	TSharedRef<FJsonObject> RootJsonObject = MakeShareable(new FJsonObject());

	{
		TArray<TSharedPtr<FJsonValue>> WindowsJsonArray;
		for (const auto& Window : Windows)
		{
			check(Window->GetNodeType() == EWidgetReflectorNodeType::Snapshot);
			WindowsJsonArray.Add(FSnapshotWidgetReflectorNode::ToJson(StaticCastSharedRef<FSnapshotWidgetReflectorNode>(Window.ToSharedRef())));
		}
		RootJsonObject->SetArrayField(TEXT("Windows"), WindowsJsonArray);
	}

	{
		TArray<TSharedPtr<FJsonValue>> TexturesJsonArray;
		for (const FWidgetSnapshotTextureData& TextureData : WindowTextureData)
		{
			TSharedRef<FJsonObject> TextureDataJsonObject = MakeShareable(new FJsonObject());

			{
				TArray<TSharedPtr<FJsonValue>> StructJsonArray;
				StructJsonArray.Add(MakeShareable(new FJsonValueNumber(TextureData.Dimensions.X)));
				StructJsonArray.Add(MakeShareable(new FJsonValueNumber(TextureData.Dimensions.Y)));
				TextureDataJsonObject->SetArrayField(TEXT("Dimensions"), StructJsonArray);
			}

			{
				// This is raw texture data - compress it before we encode it to save space
				const int32 UncompressedDataSizeBytes = TextureData.ColorData.Num() * sizeof(FColor);
				TArray<uint8> CompressedDataBuffer;
				CompressedDataBuffer.AddZeroed(FCompression::CompressMemoryBound(COMPRESS_ZLIB, UncompressedDataSizeBytes));
				int32 CompressedDataSize = CompressedDataBuffer.Num();
				if (FCompression::CompressMemory(COMPRESS_ZLIB, CompressedDataBuffer.GetData(), CompressedDataSize, TextureData.ColorData.GetData(), UncompressedDataSizeBytes))
				{
					TextureDataJsonObject->SetBoolField(TEXT("IsCompressed"), true);
					TextureDataJsonObject->SetNumberField(TEXT("UncompressedSize"), UncompressedDataSizeBytes);

					// FCompression::CompressMemory updates CompressedDataSize with the actual size - we may have to shrink our buffer count now
					CompressedDataBuffer.SetNum(CompressedDataSize, false);

					const FString EncodedTextureData = FBase64::Encode(CompressedDataBuffer);
					TextureDataJsonObject->SetStringField(TEXT("TextureData"), EncodedTextureData);
				}
				else
				{
					TextureDataJsonObject->SetBoolField(TEXT("IsCompressed"), false);

					// Failed to compress... use the raw texture data
					TArray<uint8> TextureDataBytes;
					TextureDataBytes.Append(reinterpret_cast<const uint8*>(TextureData.ColorData.GetData()), TextureData.ColorData.Num() * sizeof(FColor));

					const FString EncodedTextureData = FBase64::Encode(TextureDataBytes);
					TextureDataJsonObject->SetStringField(TEXT("TextureData"), EncodedTextureData);
				}
			}

			TexturesJsonArray.Add(MakeShareable(new FJsonValueObject(TextureDataJsonObject)));
		}
		RootJsonObject->SetArrayField(TEXT("Textures"), TexturesJsonArray);
	}

	return RootJsonObject;
}

bool FWidgetSnapshotData::LoadSnapshotFromFile(const FString& InFilename)
{
	bool bJsonLoaded = false;
	TSharedPtr<FJsonObject> RootJsonObject;

	{
		FArchive* FileAr = IFileManager::Get().CreateFileReader(*InFilename);
		if (FileAr)
		{
			typedef TJsonReader<TCHAR> FJsonReader;
			typedef TJsonReaderFactory<TCHAR> FJsonReaderFactory;

			TSharedRef<FJsonReader> Reader = FJsonReaderFactory::Create(FileAr);
			bJsonLoaded = FJsonSerializer::Deserialize(Reader, RootJsonObject);
			FileAr->Close();
			FileAr = nullptr;
		}
	}

	if (bJsonLoaded)
	{
		check(RootJsonObject.IsValid());
		LoadSnapshotFromJson(RootJsonObject.ToSharedRef());
		return true;
	}

	return false;
}

void FWidgetSnapshotData::LoadSnapshotFromBuffer(const TArray<uint8>& InData)
{
	int32 UncompressedDataSize = 0;
	TArray<uint8> UncompressedData;

	{
		FMemoryReader BufferReader(InData);

		BufferReader << UncompressedDataSize;

		UncompressedData.AddZeroed(UncompressedDataSize);
		BufferReader.SerializeCompressed(UncompressedData.GetData(), 0, COMPRESS_ZLIB);
	}

	bool bJsonLoaded = false;
	TSharedPtr<FJsonObject> RootJsonObject;

	if (UncompressedData.Num() > 0)
	{
		typedef TJsonReader<TCHAR> FJsonReader;
		typedef TJsonReaderFactory<TCHAR> FJsonReaderFactory;

		FMemoryReader UncompressedDataReader(UncompressedData);
		TSharedRef<FJsonReader> Reader = FJsonReaderFactory::Create(&UncompressedDataReader);
		bJsonLoaded = FJsonSerializer::Deserialize(Reader, RootJsonObject);
	}

	if (bJsonLoaded)
	{
		check(RootJsonObject.IsValid());
		LoadSnapshotFromJson(RootJsonObject.ToSharedRef());
	}
}

void FWidgetSnapshotData::LoadSnapshotFromJson(const TSharedRef<FJsonObject>& InRootJsonObject)
{
	Reset();

	{
		const TArray<TSharedPtr<FJsonValue>>& WindowsJsonArray = InRootJsonObject->GetArrayField(TEXT("Windows"));
		for (const TSharedPtr<FJsonValue>& WindowJsonValue : WindowsJsonArray)
		{
			Windows.Add(FSnapshotWidgetReflectorNode::FromJson(WindowJsonValue.ToSharedRef()));
		}
	}

	{
		const TArray<TSharedPtr<FJsonValue>>& TexturesJsonArray = InRootJsonObject->GetArrayField(TEXT("Textures"));
		for (const TSharedPtr<FJsonValue>& TextureDataJsonValue : TexturesJsonArray)
		{
			const TSharedPtr<FJsonObject>& TextureDataJsonObject = TextureDataJsonValue->AsObject();
			check(TextureDataJsonObject.IsValid());

			FWidgetSnapshotTextureData TextureData;

			{
				const TArray<TSharedPtr<FJsonValue>>& StructJsonArray = TextureDataJsonObject->GetArrayField(TEXT("Dimensions"));
				check(StructJsonArray.Num() == 2);

				TextureData.Dimensions.X = FMath::TruncToInt(StructJsonArray[0]->AsNumber());
				TextureData.Dimensions.Y = FMath::TruncToInt(StructJsonArray[1]->AsNumber());
			}

			{
				const FString EncodedTextureData = TextureDataJsonObject->GetStringField(TEXT("TextureData"));
				TArray<uint8> DecodedTextureDataBytes;
				FBase64::Decode(EncodedTextureData, DecodedTextureDataBytes);

				const bool bIsCompressed = TextureDataJsonObject->GetBoolField(TEXT("IsCompressed"));
				if (bIsCompressed)
				{
					const int32 UncompressedDataSizeBytes = FMath::TruncToInt(TextureDataJsonObject->GetNumberField(TEXT("UncompressedSize")));
					TextureData.ColorData.AddZeroed(UncompressedDataSizeBytes / sizeof(FColor));

					FCompression::UncompressMemory(COMPRESS_ZLIB, TextureData.ColorData.GetData(), UncompressedDataSizeBytes, DecodedTextureDataBytes.GetData(), DecodedTextureDataBytes.Num());
				}
				else
				{
					TextureData.ColorData.Append(reinterpret_cast<const FColor*>(DecodedTextureDataBytes.GetData()), DecodedTextureDataBytes.Num() / sizeof(FColor));
				}
			}

			WindowTextureData.Add(TextureData);
		}
	}

	CreateBrushes();
}

bool FWidgetSnapshotData::IsEmpty() const
{
	return Windows.Num() == 0;
}

int32 FWidgetSnapshotData::Num() const
{
	return Windows.Num();
}

const TArray<TSharedPtr<FWidgetReflectorNodeBase>>& FWidgetSnapshotData::GetWindowsPtr() const
{
	return Windows;
}

TArray<TSharedRef<FWidgetReflectorNodeBase>> FWidgetSnapshotData::GetWindowsRef() const
{
	TArray<TSharedRef<FWidgetReflectorNodeBase>> RetWindows;
	RetWindows.Reserve(Windows.Num());
	for (const auto& Window : Windows)
	{
		RetWindows.Add(Window.ToSharedRef());
	}
	return RetWindows;
}

TSharedPtr<FWidgetReflectorNodeBase> FWidgetSnapshotData::GetWindow(const int32 WindowIndex) const
{
	return (Windows.IsValidIndex(WindowIndex)) 
		? TSharedPtr<FWidgetReflectorNodeBase>(Windows[WindowIndex]) 
		: TSharedPtr<FWidgetReflectorNodeBase>(nullptr);
}

const FSlateBrush* FWidgetSnapshotData::GetBrush(const int32 WindowIndex) const
{
	return (WindowTextureBrushes.IsValidIndex(WindowIndex)) ? WindowTextureBrushes[WindowIndex].Get() : nullptr;
}

void FWidgetSnapshotData::CreateBrushes()
{
	DestroyBrushes();

	WindowTextureBrushes.Reserve(WindowTextureData.Num());

	static int32 TextureIndex = 0;
	for (const auto& TextureData : WindowTextureData)
	{
		if (TextureData.ColorData.Num() > 0)
		{
			TArray<uint8> TextureDataAsBGRABytes;
			TextureDataAsBGRABytes.Reserve(TextureData.ColorData.Num() * 4);
			for (const FColor& PixelColor : TextureData.ColorData)
			{
				TextureDataAsBGRABytes.Add(PixelColor.B);
				TextureDataAsBGRABytes.Add(PixelColor.G);
				TextureDataAsBGRABytes.Add(PixelColor.R);
				TextureDataAsBGRABytes.Add(PixelColor.A);
			}

			WindowTextureBrushes.Add(FSlateDynamicImageBrush::CreateWithImageData(
				*FString::Printf(TEXT("FWidgetSnapshotData_WindowTextureBrush_%d"), TextureIndex++), 
				FVector2D(TextureData.Dimensions.X, TextureData.Dimensions.Y), 
				TextureDataAsBGRABytes
				));
		}
		else
		{
			WindowTextureBrushes.Add(nullptr);
		}
	}
}

void FWidgetSnapshotData::DestroyBrushes()
{
	for (const auto& WindowTextureBrush : WindowTextureBrushes)
	{
		if (WindowTextureBrush.IsValid())
		{
			WindowTextureBrush->ReleaseResource();
		}
	}

	WindowTextureBrushes.Reset();
}

void FWidgetSnapshotData::Reserve(const int32 NumWindows)
{
	Windows.Reserve(NumWindows);
	WindowTextureData.Reserve(NumWindows);
	WindowTextureBrushes.Reserve(NumWindows);
}

void FWidgetSnapshotData::Reset()
{
	DestroyBrushes();

	Windows.Reset();
	WindowTextureData.Reset();
	WindowTextureBrushes.Reset();
}


void SWidgetSnapshotVisualizer::Construct(const FArguments& InArgs)
{
	SnapshotDataPtr = InArgs._SnapshotData;
	check(SnapshotDataPtr);

	ChildSlot
	[
		SNew(SVerticalBox)

		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(5.0f, 0.0f))
			[
				SAssignNew(WindowPickerCombo, SComboBox<TSharedPtr<FWidgetReflectorNodeBase>>)
				.OptionsSource(&SnapshotDataPtr->GetWindowsPtr())
				.OnSelectionChanged(this, &SWidgetSnapshotVisualizer::OnWindowSelectionChanged)
				.OnGenerateWidget(this, &SWidgetSnapshotVisualizer::GenerateWindowPickerComboItem)
				[
					SNew(STextBlock)
					.Text(this, &SWidgetSnapshotVisualizer::GetSelectedWindowComboItemText)
				]
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(5.0f, 0.0f))
			[
				SNew(SButton)
				.Text(this, &SWidgetSnapshotVisualizer::GetPickWidgetText)
				.ButtonColorAndOpacity(this, &SWidgetSnapshotVisualizer::GetPickWidgetColor)
				.OnClicked(this, &SWidgetSnapshotVisualizer::OnPickWidgetClicked)
			]

#if SLATE_REFLECTOR_HAS_DESKTOP_PLATFORM
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(5.0f, 0.0f))
			[
				SNew(SButton)
				.Text(LOCTEXT("SaveSnapshotButtonText", "Save Snapshot"))
				.OnClicked(this, &SWidgetSnapshotVisualizer::OnSaveSnapshotClicked)
			]
#endif // SLATE_REFLECTOR_HAS_DESKTOP_PLATFORM
		]

		+SVerticalBox::Slot()
		[
			SAssignNew(SnapshotImage, SScrollableSnapshotImage)
			.SnapshotData(InArgs._SnapshotData)
			.OnWidgetPathPicked(InArgs._OnWidgetPathPicked)
		]
	];

	SnapshotDataUpdated();
}

void SWidgetSnapshotVisualizer::SnapshotDataUpdated()
{
	if (SnapshotImage.IsValid())
	{
		SnapshotImage->SetSelectedWindowIndex(0);
	}

	if (WindowPickerCombo.IsValid())
	{
		WindowPickerCombo->RefreshOptions();
		WindowPickerCombo->SetSelectedItem(SnapshotDataPtr->GetWindow(0));
	}
}

void SWidgetSnapshotVisualizer::SetSelectedWidgets(const TArray<TSharedRef<FWidgetReflectorNodeBase>>& InSelectedWidgets)
{
	if (SnapshotImage.IsValid())
	{
		SnapshotImage->SetSelectedWidgets(InSelectedWidgets);
	}
}

FReply SWidgetSnapshotVisualizer::OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (SnapshotImage.IsValid() && InKeyEvent.GetKey() == EKeys::Escape)
	{
		SnapshotImage->SetIsPicking(false);
	}

	return FReply::Unhandled();
}

void SWidgetSnapshotVisualizer::OnWindowSelectionChanged(TSharedPtr<FWidgetReflectorNodeBase> InWindow, ESelectInfo::Type InReason)
{
	int32 SelectedWindowIndex = INDEX_NONE;
	SnapshotDataPtr->GetWindowsPtr().Find(InWindow, SelectedWindowIndex);

	if (SnapshotImage.IsValid())
	{
		SnapshotImage->SetSelectedWindowIndex(SelectedWindowIndex);
	}
}

FText SWidgetSnapshotVisualizer::GetWindowPickerComboItemText(TSharedPtr<FWidgetReflectorNodeBase> InWindow)
{
	return FText::Format(LOCTEXT("WidgetComboItemFmt", "{0} - {1}"), InWindow->GetWidgetType(), InWindow->GetWidgetReadableLocation());
}

FText SWidgetSnapshotVisualizer::GetSelectedWindowComboItemText() const
{
	const int32 SelectedWindowIndex = SnapshotImage.IsValid() ? SnapshotImage->GetSelectedWindowIndex() : INDEX_NONE;
	TSharedPtr<FWidgetReflectorNodeBase> SelectedWindowPtr = SnapshotDataPtr->GetWindow(SelectedWindowIndex);
	return (SelectedWindowPtr.IsValid()) ? GetWindowPickerComboItemText(SelectedWindowPtr) : FText::GetEmpty();
}

TSharedRef<SWidget> SWidgetSnapshotVisualizer::GenerateWindowPickerComboItem(TSharedPtr<FWidgetReflectorNodeBase> InWindow) const
{
	return SNew(STextBlock)
		.Text(GetWindowPickerComboItemText(InWindow));
}

FText SWidgetSnapshotVisualizer::GetPickWidgetText() const
{
	const bool bIsPicking = SnapshotImage.IsValid() && SnapshotImage->GetIsPicking();
	return (bIsPicking) ? LOCTEXT("PickingWidget", "Picking (Esc to Stop)") : LOCTEXT("PickSnapshotWidget", "Pick Snapshot Widget");
}

FSlateColor SWidgetSnapshotVisualizer::GetPickWidgetColor() const
{
	static const FName SelectionColor("SelectionColor");

	const bool bIsPicking = SnapshotImage.IsValid() && SnapshotImage->GetIsPicking();
	return bIsPicking
		? FCoreStyle::Get().GetSlateColor(SelectionColor)
		: FLinearColor::White;
}

FReply SWidgetSnapshotVisualizer::OnPickWidgetClicked()
{
	if (SnapshotImage.IsValid())
	{
		SnapshotImage->SetIsPicking(!SnapshotImage->GetIsPicking());
	}
	return FReply::Handled();
}

#if SLATE_REFLECTOR_HAS_DESKTOP_PLATFORM

FReply SWidgetSnapshotVisualizer::OnSaveSnapshotClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	if (DesktopPlatform)
	{
		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(SharedThis(this));

		TArray<FString> SaveFilenames;
		const bool bOpened = DesktopPlatform->SaveFileDialog(
			(ParentWindow.IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr,
			LOCTEXT("SaveSnapshotDialogTitle", "Save Widget Snapshot").ToString(),
			FPaths::GameAgnosticSavedDir(),
			TEXT(""),
			TEXT("Slate Widget Snapshot (*.widgetsnapshot)|*.widgetsnapshot"),
			EFileDialogFlags::None,
			SaveFilenames
			);

		if (SaveFilenames.Num() > 0)
		{
			SnapshotDataPtr->SaveSnapshotToFile(SaveFilenames[0]);
		}
	}

	return FReply::Handled();
}

#endif // SLATE_REFLECTOR_HAS_DESKTOP_PLATFORM

#undef LOCTEXT_NAMESPACE
