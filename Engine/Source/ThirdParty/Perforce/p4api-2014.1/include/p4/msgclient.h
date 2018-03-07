/*
 * Copyright 1995, 2000 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * msgclient.h - definitions of errors for client subsystem.
 */

struct ErrorId;    // forward declaration to keep VS 2010 IntelliSense happy

class MsgClient {

    public:

	static ErrorId Connect;
	static ErrorId BadFlag;
	static ErrorId Fatal;
	static ErrorId ClobberFile;
	static ErrorId FileOpenError;
	static ErrorId MkDir;
	static ErrorId Eof;
	static ErrorId CantEdit;
	static ErrorId NoMerger;

	static ErrorId ToolServer2;
	static ErrorId ToolServer;
	static ErrorId ToolCmdCreate;
	static ErrorId ToolCmdSend;
	static ErrorId Memory;
	static ErrorId CantFindApp;
	static ErrorId BadSignature;
	static ErrorId BadMarshalInput;
	static ErrorId LineTooLong;

	static ErrorId ResolveManually;
	static ErrorId NonTextFileMerge;

	static ErrorId MergeMsg2;
	static ErrorId MergeMsg3;
	static ErrorId MergeMsg32;
	static ErrorId MergePrompt;
	static ErrorId MergePrompt2;
	static ErrorId MergePrompt2Edit;

	static ErrorId ConfirmMarkers;
	static ErrorId ConfirmEdit;
	static ErrorId Confirm;

	static ErrorId CheckFileAssume;
	static ErrorId CheckFileAssumeWild;
	static ErrorId CheckFileSubst;
	static ErrorId CheckFileCant;

	static ErrorId FileExists;
	static ErrorId NoSuchFile;

	static ErrorId LoginPrintTicket;
	static ErrorId DigestMisMatch;
	static ErrorId NotUnderPath;

	// Retired ErrorIds. We need to keep these so that clients 
	// built with newer apis can commnunicate with older servers 
	// still sending these.

	static ErrorId ZCResolve; // DEPRECATED 2013.1 removed ZeroConf
} ;
