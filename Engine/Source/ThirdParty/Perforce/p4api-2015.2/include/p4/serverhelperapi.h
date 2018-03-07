/*
 * Copyright 1995, 2015 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

# include "clientapi.h"
# include "strtable.h"

/*
 * ServerHelperApi - the Perforce Server manipulation API class
 *
 * Basic GetClient flow:
 *
 *	ClientUser ui;
 *	Error e;
 *	ServerHelperApi server( &e );
 *
 *	if( e.Test() )
 *	    return 0;
 *
 *	// Either SetPort() or SetInitRoot() must be called before GetClient()
 *
 *	server.SetPort( "1666", &e ); //optional
 *	server.SetDvcsDir( "/path/to/dvcs", &e ); //optional
 *
 *	server.SetProtocol( "var", "value" ); //optional
 *	server.SetProg( "MyApp" );  // optional
 *	server.SetVersion( "version" ); // optional
 *
 *	ClientApi *client = server.GetClient( &e );
 *
 *	if( !client || e.Test() )
 *	    return 0;
 *
 *	while( !client.Dropped() )
 *	{
 *	    client.SetArgv( argc, argv );
 *	    client.Run( func, &ui );
 *	}
 *
 *	int res = client.Final( &e );
 *	delete client;
 *	return res;
 *
 *
 * Basic "p4 init" flow:
 *
 *	ClientUser ui;
 *	Error e;
 *	ServerHelperApi server( &e );
 *
 *	if( e.Test() )
 *	    return 0;
 *
 *	server.SetDvcsDir( "/path/to/dvcs", &e );
 *	server.SetProg( "MyApp" ); // optional
 *	server.SetVersion( "version" ); // optional
 *
 *	if( server.Exists() )
 *	    return 0;
 *
 *	// The unicode and case-sensitivity options must be set _before_
 *	// InitLocalServer() is called. These can be set manually or
 *	// discovered.
 *
 *	server.SetUnicode( true );
 *	server.SetCaseSensitivity( "-C0", &e );
 *
 *	if( !server.InitLocalServer( &ui ) )
 *	    return 0;
 *
 *
 * Basic "p4 clone" flow:
 *
 *	ClientUser ui;
 *	Error e;
 *	ServerHelperApi localServer( &e );
 *
 *	if( e.Test() )
 *	    return 0;
 *
 *	localServer.SetDvcsDir( "/path/to/dvcs", &e );
 *	localServer.SetProg( "MyApp" ); // optional
 *	localServer.SetVersion( "version" ); // optional
 *
 *	if( localServer.Exists() )
 *	    return 0;
 *
 *	ServerHelperApi remoteServer( &e );
 *	remoteServer.SetPort( "1666" );
 *	remoteServer.SetProg( "MyApp" ); // optional
 *	remoteServer.SetVersion( "version" ); // optional
 *
 *	// Fetch the remote spec
 *
 *	if( !localServer.PrepareToCloneRemote( &remoteServer, remote, &ui ) )
 *	    return 0;
 *
 *	// Create the local server
 *	// This returns the exit code from p4d, so 0 is success
 *
 *	if( localServer.InitLocalServer( &ui ) )
 *	    return 0;
 *
 *	// Fetch from the remote
 *
 *	if( !localServer.CloneFromRemote( 0, 0, &ui ) )
 *	    return 0;
 *
 *
 * Public methods:
 *
 *	ServerHelperApi::SetDvcsDir()	- Set the path to the initroot
 *
 *	ServerHelperApi::SetProg()	- Set the client program name
 *	ServerHelperApi::SetVersion()	- Set the client version string
 *
 *	ServerHelperApi::SetUser()	- Set the username of the user
 *	    This username will be used when creating a new Perforce Server and
 *	    when fetching changes from remote Perforce Servers.
 *
 *	ServerHelperApi::SetClient()	- Set the name of the client workspace
 *	    When a new Perforce Server ins initailised, a workspace with this
 *	    name will be created. This will also be used as the serverId.
 *
 *	ServerHelperApi::SetDefaultStream() - Sets the default strema name
 *	    This sets the depot and mainline stream that a new Perforce Server
 *	    will be initialised with; however, this may be overriden a remote
 *	    spec created by MakeRemote() or loaded with LoadRemote().
 *
 *	ServerHelperApi::SetCaseFlag()	- Sets the case sensitivity flag
 *	    This sets the case sensitivity flag used when initialising a new
 *	    Perforce Server. It can be set to '-C0' or '-C1'. The value will be
 *	    overriden by Discover(), MakeRemote() and LoadRemote().
 *
 *	ServerHelperApi::SetUnicode()	- Sets the unicode flag
 *	    This sets the unicode flag used when initialising a new Perforce
 *	    Server. It can be set to '0' or '1'. The value will be overriden by
 *	    Discover(), MakeRemote() and LoadRemote().
 *
 *
 *	ServerHelperApi::Exists()	- Checks for a Perforce Server
 *	    You cannot initialise a new Perforce Server if one already exists.
 *	    You cannot create a client for a local Perforce Server unless it
 *	    has already been initialised.
 *
 *	ServerHelperApi::CopyConfiguration() - Copies settings from server
 *	    This can only be run if the Perforce Server does not exist.
 *	    Gets the CaseSensitivity and Unicode settings from a remote server.
 *
 *	ServerHelperApi::PrepareToCloneRemote()	- Loads a remote spec
 *	    This checks that the named remote spec exists on the remote server
 *	    and loads it into this server helper object.
 *
 *	ServerHelperApi::PrepareToCloneFilepath() - Creates a new remote spec
 *	    This creates a new remote spec based on the provided filepath.
 *	    It also checks that 'p4 fetch' is allowed on the remote server
 *	    specified.
 *
 *	ServerHelperApi::InitLocalServer() - Creates a local server
 *	    Writes the P4CONFIG and P4IGNORE files and creates the .p4root dir.
 *	    The P4D is started for the first time with the case/unicode flags.
 *	    The serverId is set and the server spec is populated.
 *	    The protections table is populated, restrictig access to localhost.
 *	    A streams depot is created and switch is used to create a client.
 *
 *	ServerHelperApi::CloneFromRemote() - Saves the remote and fetchs
 *	    If a remote spec has been loaded or created, that spec is written
 *	    to the new local server as the origin remote. A 'p4 fetch' is then
 *	    invoked.
 *
 *	ServerHelperApi::SetProtocol()	- Adds protocol tags to GetClient()
 *
 *	ServerHelperApi::GetClient()	- Creates and init's a ClientAPI object
 *	    Creates a new client in the context of the local Perforce Server.
 */

class ClientApi;
class ServerHelper;

class ServerHelperApi
{
    public:
			ServerHelperApi( Error *e );
			~ServerHelperApi();

	// Server API operations

	int		Exists( ClientUser *ui, Error *e );
	int		CopyConfiguration( ServerHelperApi *remoteServer,
			    ClientUser *ui, Error *e );
	int		PrepareToCloneRemote( ServerHelperApi *remoteServer,
			    const char *remote, ClientUser *ui, Error *e );
	int		PrepareToCloneRemote( ServerHelperApi *remoteServer,
			    const StrPtr *remote, ClientUser *ui, Error *e );
	int		PrepareToCloneFilepath( ServerHelperApi *remoteServer,
			    const StrPtr *filePath, ClientUser *ui, Error *e );
	int		PrepareToCloneFilepath( ServerHelperApi *remoteServer,
			    const char *filePath, ClientUser *ui, Error *e );
	int		InitLocalServer( ClientUser *ui, Error *e );
	int		CloneFromRemote( int depth,
			            int noArchivesFlag,
			            const StrPtr *debugFlag,
			            ClientUser *ui, Error *e );
	int		CloneFromRemote( int depth,
			            int noArchivesFlag,
			            const char *debugFlag,
			            ClientUser *ui, Error *e );
	

	// Server API behavior modifiers
	
	void		SetDebug( StrPtr *v );
	void		SetQuiet();
	int		GetQuiet();
	
	// Sets the default mainline stream
	// Must be called before CopyConfiguration() and InitLocalServer().
	void		SetDefaultStream( const char *s, Error *e );
	void		SetDefaultStream( const StrPtr *s, Error *e );

	// Alternatives to Discover()
	void		SetCaseFlag( const StrPtr *c, Error *e );
	void		SetCaseFlag( const char *c, Error *e );
	void		SetUnicode( int u );
	StrPtr		GetCaseFlag();
	int		GetUnicode();

	// Generic Getters/Setters
	int		SetDvcsDir( const char *c, Error *e );
	void		SetServerExecutable( const char *c );
	int		SetPort( const char *c, Error *e );
	void		SetUser( const char *c );
	void		SetClient( const char *c );
	void		SetPassword( const char *c );
	void		SetProg( const char *c );
	void		SetVersion( const char *c );
	
	int		SetDvcsDir( const StrPtr *c, Error *e );
	void		SetServerExecutable( const StrPtr *c );
	int		SetPort( const StrPtr *c, Error *e );
	void		SetUser( const StrPtr *c );
	void		SetClient( const StrPtr *c );
	void		SetPassword( const StrPtr *c );
	void		SetProg( const StrPtr *c );
	void		SetVersion( const StrPtr *c );
	
	const StrPtr	&GetDvcsDir();
	const StrPtr	&GetServerExecutable();
	const StrPtr	&GetPort();
	const StrPtr	&GetUser();
	const StrPtr	&GetClient();
	const StrPtr	&GetPassword();
	const StrPtr	&GetProg();
	const StrPtr	&GetVersion();

	// Helper functions

	ClientApi	*GetClient( Error *e );

	void		SetProtocol( const char *p, const char *v );
	void		SetProtocolV( const char *p );
	void		ClearProtocol();


    private:
	ServerHelper	*server;
	StrBufDict	protocol;
	StrBuf		port;
} ;

