/*
 * Copyright 1995, 2000 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * msgrpc.h - declarations of errors for Rpc subsystem.
 */

class MsgRpc {

    public:
	static ErrorId Stdio;

	static ErrorId TcpAccept;
	static ErrorId TcpConnect;
	static ErrorId TcpHost;
	static ErrorId TcpListen;
	static ErrorId TcpPortInvalid;
	static ErrorId TcpRecv;
	static ErrorId TcpSend;
	static ErrorId TcpService;
	static ErrorId TcpPeerSsl;

	static ErrorId Closed;
	static ErrorId Listen;
	static ErrorId NoPoss;
	static ErrorId NotP4;
	static ErrorId Operat;
	static ErrorId Read;
	static ErrorId Select;
	static ErrorId Reconn;
	static ErrorId TooBig;
	static ErrorId UnReg;
	static ErrorId Unconn;
	static ErrorId Break;
	static ErrorId MaxWait;
	static ErrorId NameResolve;

	static ErrorId SslAccept;
	static ErrorId SslConnect;
	static ErrorId SslListen;
	static ErrorId SslRecv;
	static ErrorId SslSend;
	static ErrorId SslClose;
	static ErrorId SslInvalid;
	static ErrorId SslCtx;
	static ErrorId SslShutdown;
	static ErrorId SslInit;
	static ErrorId SslCleartext;
	static ErrorId SslCertGen;
	static ErrorId SslNoSsl;
	static ErrorId SslBadKeyFile;
	static ErrorId SslGetPubKey;
	static ErrorId SslBadDir;
	static ErrorId SslBadFsSecurity;
	static ErrorId SslDirHasCreds;
	static ErrorId SslCredsBadOwner;
	static ErrorId SslCertBadDates;
	static ErrorId SslNoCredentials;
	static ErrorId SslFailGetExpire;

	static ErrorId HostKeyUnknown;
	static ErrorId HostKeyMismatch;
	static ErrorId ServiceNoTrust;
	static ErrorId SslLibMismatch;

	static ErrorId PxRemoteSvrFail;
	static ErrorId SslCfgExpire;
	static ErrorId SslCfgUnits;
	static ErrorId SslKeyNotRSA;

	static ErrorId WakeupInit;
	static ErrorId WakeupAttempt;
	static ErrorId ZksInit;
	static ErrorId ZksSend;
	static ErrorId ZksRecv;
	static ErrorId ZksDisconnect;
	static ErrorId ZksState;
	static ErrorId ZksNoZK;

	static ErrorId UnixDomainOpen;
	static ErrorId BadP4Port;
	static ErrorId NoHostnameForPort;
	static ErrorId NoConnectionToZK;

	// Retired ErrorIds. We need to keep these so that clients 
	// built with newer apis can communicate with older servers
	// still sending these.
} ;
