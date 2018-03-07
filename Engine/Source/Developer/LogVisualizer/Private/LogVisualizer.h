// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

/* Dependencies
*****************************************************************************/

#include "GenericPlatform/GenericPlatformFile.h"
#include "Layout/ArrangedChildren.h"
#include "Textures/TextureAtlas.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SToolTip.h"
#include "Dom/JsonObject.h"
#include "Framework/MultiBox/MultiBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Runtime/Slate/Private/Framework/Docking/SDockingTabStack.h"

/* Private includes
*****************************************************************************/

#include "GameFramework/DamageType.h"
#include "TimerManager.h"
#include "HAL/RunnableThread.h"
#include "Engine/World.h"
#include "SVisualLogger.h"
#include "LogVisualizerPrivate.h"
#include "Developer/LogVisualizer/Private/SVisualLoggerToolbar.h"
#include "Developer/LogVisualizer/Private/SVisualLoggerFilters.h"
#include "Widgets/Layout/SScrollBarTrack.h"
#include "Developer/LogVisualizer/Private/SVisualLoggerView.h"
#include "Developer/LogVisualizer/Private/SVisualLoggerLogsList.h"
#include "Developer/LogVisualizer/Private/SVisualLoggerStatusView.h"
#include "SVisualLoggerTimeline.h"
#include "Developer/LogVisualizer/Private/SVisualLoggerTimelineBar.h"
#include "MovieScene.h"
#include "Developer/LogVisualizer/Private/VisualLoggerTimeSliderController.h"
