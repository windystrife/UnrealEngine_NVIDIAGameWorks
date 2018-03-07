// -*- mode: C++; tab-width: 8; -*-
// vi:ts=8 sw=4 noexpandtab autoindent

/**
 * netportparser.h - Parse a P4PORT-like string ([transport:][host:]port).
 *
 * Copyright 2011 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

class NetPortParser {
public:
	enum PPOpts
	{
	    PPO_NONE = 0,
	    PPO_TRANSPORT = 1,
	    PPO_PORT = 2,
	    PPO_BOTH = PPO_TRANSPORT | PPO_PORT
	};

	// Orthodox Canonical Form (OCF) methods
	NetPortParser();

	NetPortParser(
	    const StrRef        &portstr);

	NetPortParser(
	    const char          *portstr);

	NetPortParser(
	    const NetPortParser    &rhs);

	virtual
	~NetPortParser();

	virtual const NetPortParser &
	operator=(
	    const NetPortParser    &rhs);

	virtual bool
	operator==(
	    const NetPortParser    &rhs) const;

	virtual bool
	operator!=(
	    const NetPortParser    &rhs) const;

	// accessors

	bool
	MustRfc3484() const;    // should we follow RFC 3484?

	bool
	MayIPv4() const;

	bool
	MayIPv6() const;

	bool
	PreferIPv4() const;

	bool
	PreferIPv6() const;

	// P4PORT listed IPv6 explicitly
	bool
	WantIPv6() const;

	bool
	MayJSH() const;

	bool
	MustJSH() const;

	bool
	MayRSH() const;

	bool
	MustRSH() const;

	bool
	MustSSL() const;

	bool
	MustIPv4() const;

	bool
	MustIPv6() const;

	const StrBuf &
	Transport() const
	{
	    return mTransport;
	}

	const StrPtr &
	Port() const
	{
	    return mPort;
	}

	int
	PortNum() const;

	const StrPtr &
	Host() const
	{
	    return mHost;
	}

	const StrPtr &
	HostPort() const
	{
	    return mHostPort;
	}

	const StrBuf &
	String() const
	{
	    return mPortString;
	}

	const StrPtr &
	ZoneID() const
	{
	    return mZoneID;
	}

	// construct a StrBuf from the requested pieces
	const StrBuf
	String(PPOpts opts) const;


	// mutators


	// Other methods
	void
	Parse();

	void
	Parse(const StrRef &portstr);

	bool
	IsValid(Error *e) const;

	const StrBuf
	GetQualifiedP4Port( StrBuf &serverSpecAddr, Error &e ) const;

protected:

private:
	enum PrefixType
	{
	    PT_NONE,
	    PT_JSH,	// java flavor of rsh
	    PT_RSH,
	    PT_TCP,
	    PT_TCP4,
	    PT_TCP6,
	    PT_TCP46,
	    PT_TCP64,
	    PT_SSL,
	    PT_SSL4,
	    PT_SSL6,
	    PT_SSL46,
	    PT_SSL64
	};

	struct Prefix
	{
	    const char    *mName;
	    PrefixType    mType;
	};

	StrBuf    mPortString;    // saved P4PORT (or -p) input string
	StrBuf    mTransport;     // parsed transport prefix string
	StrBuf    mHost;          // parsed host string
	StrBuf    mPort;          // parsed port string
	StrBuf    mHostPort;      // parsed host+port string
	StrBuf    mZoneID;        // parsed "%zoneid", if any
	bool      mPortColon;     // true iff there was a colon starting the port field
	Prefix    mPrefix;        // parsed transport prefix

private:

	const Prefix *
	FindPrefix(
	    const char    *prefix,
	    int           len);
};

