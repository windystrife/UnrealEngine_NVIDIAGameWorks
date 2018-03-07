// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/MonolithicHeaderBoilerplate.h"
MONOLITHIC_HEADER_BOILERPLATE()

// Generic Text Layout
#include "Framework/Text/TextRange.h"
#include "Framework/Text/TextRunRenderer.h"
#include "Framework/Text/TextLineHighlight.h"

#include "Framework/Text/TextHitPoint.h"
#include "Framework/Text/ShapedTextCacheFwd.h"
#include "Framework/Text/IRun.h"
#include "Framework/Text/IRunRenderer.h"
#include "Framework/Text/ILineHighlighter.h"
#include "Framework/Text/ILayoutBlock.h"

#include "Framework/Text/TextLayout.h"
#include "Framework/Text/DefaultLayoutBlock.h"
#include "Framework/Text/WidgetLayoutBlock.h"


// Slate Specific Text Layout
#include "Framework/Text/ISlateRun.h"
#include "Framework/Text/ISlateRunRenderer.h"
#include "Framework/Text/ISlateLineHighlighter.h"

#include "Framework/Text/SlateTextLayout.h"

#include "Framework/Text/SlateTextRun.h"
#include "Framework/Text/SlateHyperlinkRun.h"
#include "Framework/Text/SlateImageRun.h"
#include "Framework/Text/SlateWidgetRun.h"
