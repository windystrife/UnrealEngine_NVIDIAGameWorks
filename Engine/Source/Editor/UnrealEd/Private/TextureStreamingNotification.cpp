// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "CoreMinimal.h"
#include "Editor.h"
#include "GlobalEditorNotification.h"
#include "ContentStreaming.h"
#include "Widgets/Notifications/SNotificationList.h"

/** Notification class for texture streaming. */
class FTextureStreamingNotificationImpl : public FGlobalEditorNotification
{
protected:
	/** FGlobalEditorNotification interface */
	virtual bool ShouldShowNotification(const bool bIsNotificationAlreadyActive) const override;
	virtual void SetNotificationText(const TSharedPtr<SNotificationItem>& InNotificationItem) const override;

private:
	static int32 GetNumStreamingTextures();
};

/** Global notification object. */
FTextureStreamingNotificationImpl GTextureStreamingNotification;

bool FTextureStreamingNotificationImpl::ShouldShowNotification(const bool bIsNotificationAlreadyActive) const
{
	// We only want to show the notification initially if there's still enough work left to do to warrant it
	// However, if we're already showing the notification, we should continue to do so until all the streaming has finished
	// Never show these notifications during PIE
	const int32 NumStreamingTextures = GetNumStreamingTextures();
	return !GEditor->PlayWorld && ((NumStreamingTextures > 300) || (bIsNotificationAlreadyActive && NumStreamingTextures > 0));
}

void FTextureStreamingNotificationImpl::SetNotificationText(const TSharedPtr<SNotificationItem>& InNotificationItem) const
{
	const int32 NumStreamingTextures = GetNumStreamingTextures();
	if (NumStreamingTextures > 0)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("NumTextures"), FText::AsNumber(NumStreamingTextures));
		const FText ProgressMessage = FText::Format(NSLOCTEXT("StreamingTextures", "StreamingTexturesInProgressFormat", "Streaming Textures ({NumTextures})"), Args);

		InNotificationItem->SetText(ProgressMessage);
	}
}

int32 FTextureStreamingNotificationImpl::GetNumStreamingTextures()
{
	FStreamingManagerCollection& StreamingManagerCollection = IStreamingManager::Get();

	if (StreamingManagerCollection.IsTextureStreamingEnabled())
	{
		ITextureStreamingManager& TextureStreamingManager = StreamingManagerCollection.GetTextureStreamingManager();
		return TextureStreamingManager.GetNumWantingResources();
	}

	return 0;
}
