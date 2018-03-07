// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StompCommand.h"

#if WITH_STOMP

#define DEFINE_COMMAND(Name, Value) const FStompCommand Name ## Command = TEXT(#Value)

const FStompCommand HeartbeatCommand = FStompCommand();
DEFINE_COMMAND(Connect, CONNECT);
DEFINE_COMMAND(Connected, CONNECTED);
DEFINE_COMMAND(Send, SEND);
DEFINE_COMMAND(Subscribe, SUBSCRIBE);
DEFINE_COMMAND(Unsubscribe, UNSUBSCRIBE);
DEFINE_COMMAND(Begin, BEGIN);
DEFINE_COMMAND(Commit, COMMIT);
DEFINE_COMMAND(Abort, ABORT);
DEFINE_COMMAND(Ack, ACK);
DEFINE_COMMAND(Nack, NACK);
DEFINE_COMMAND(Disconnect, DISCONNECT);
DEFINE_COMMAND(Message, MESSAGE);
DEFINE_COMMAND(Receipt, RECEIPT);
DEFINE_COMMAND(Error, ERROR);

#undef DEFINE_COMMAND

#endif // #if WITH_STOMP
