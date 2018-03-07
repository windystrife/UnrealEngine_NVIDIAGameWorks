// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "XmppConnection.h"

#if WITH_XMPP_JINGLE
#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
#include "AllowWindowsPlatformTypes.h"
#include "AllowWindowsPlatformAtomics.h"
#endif

THIRD_PARTY_INCLUDES_START

#pragma push_macro("NO_LOGGING")
#pragma push_macro("OVERRIDE")
#undef NO_LOGGING
#undef OVERRIDE

#include "webrtc/base/thread.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/stringencode.h"

#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/libjingle/xmpp/xmppclientsettings.h"
#include "webrtc/libjingle/xmpp/xmppengine.h"
#include "webrtc/libjingle/xmpp/xmppthread.h"
#include "webrtc/libjingle/xmpp/xmpppump.h"
#include "webrtc/libjingle/xmpp/xmppauth.h"
#include "webrtc/libjingle/xmpp/presencestatus.h"
#include "webrtc/libjingle/xmpp/presencereceivetask.h"
#include "webrtc/libjingle/xmpp/presenceouttask.h"
#include "webrtc/libjingle/xmpp/jingleinfotask.h"
#include "webrtc/libjingle/xmpp/xmpptask.h"
#include "webrtc/libjingle/xmpp/pingtask.h"
#include "webrtc/libjingle/xmpp/chatroommodule.h"
#include "webrtc/libjingle/xmpp/discoitemsquerytask.h"
#include "webrtc/libjingle/xmpp/mucroomdiscoverytask.h"

#pragma pop_macro("NO_LOGGING")
#pragma pop_macro("OVERRIDE")

THIRD_PARTY_INCLUDES_END

#if PLATFORM_WINDOWS
#include "HideWindowsPlatformAtomics.h"
#include "HideWindowsPlatformTypes.h"
#endif

/**
 * Entry for access to Xmpp connections implemented via libjingle
 */
class FXmppJingle
{
public:

	// FXmppJingle

	static void Init();
	static void Cleanup();
	static TSharedRef<class IXmppConnection> CreateConnection();

	static void ConvertToJid(FXmppUserJid& OutJid, const buzz::Jid& InJid);
	static void ConvertFromJid(buzz::Jid& OutJid, const FXmppUserJid& InJid);

	/** Adds a Correlation ID to a stanza.  If none is provided, one will be generated */
	static void AddCorrIdToStanza(buzz::XmlElement& Stanza, const TCHAR* const CorrId = nullptr);
};

#endif //WITH_XMPP_JINGLE
