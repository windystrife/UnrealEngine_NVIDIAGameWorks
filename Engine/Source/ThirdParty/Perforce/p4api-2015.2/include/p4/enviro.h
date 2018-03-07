/*
 * Copyright 1995, 1997 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * enviro.h - get/set environment variables/registry entries
 *
 * Note that there is no longer a global environment.  If
 * multiple threads wish to share the same enviroment, they'll
 * have to call Reload() to see any changes.  On UNIX, there
 * is no setting the environment so that isn't an issue.
 *
 * Public methods:
 *
 *	Enviro::BeServer() - get and set "system level"/service(NT) variables
 *	Enviro::Get() - get a variable from the environment
 *	Enviro::Set() - set a variable in the environment (NT only)
 *	Enviro::Config() - load $P4CONFIG file (if set)
 *	Enviro::List() - list variables in the environment 
 *	Enviro::Reload() - flush values cached from NT registry
 *	Enviro::GetConfig() - get the name of the $P4CONFIG file (if set)
 */

class EnviroTable;
struct EnviroItem;
class Error;
class StrBuf;
class StrPtr;
class StrArray;
class FileSys;
struct KeyPair;

class Enviro {

    public:
			Enviro();
			~Enviro();

	enum ItemType { 
		NEW,	// not looked up yet
		UNSET,	// looked up and is empty
		UPDATE,	// set via the Update call
		ENV,	// set in environment
		CONFIG,	// via P4CONFIG
		ENVIRO,	// P4ENVIRO file
		SVC,	// set in service-specific registry
		USER,	// set in user registry
		SYS 	// set is machine registry
	};

	int		BeServer( const StrPtr *name = 0, int checkName = 0 );

	const char      *ServiceName();
	static const StrPtr *GetCachedServerName();
	void		OsServer();

	void		List( int quiet = 0 );
	int		FormatVariable( int i, StrBuf *sb );
	int		HasVariable( int i );
	static int	IsKnown( const char *nm );
	void		GetVarName( int i, StrBuf &sb );
	void		GetVarValue( int i, StrBuf &sb );
	void		Format( const char *var, StrBuf *sb, int quiet = 0 );

	void		Print( const char *var, int quiet = 0 );
	char		*Get( const char *var );
	void		Set( const char *var, const char *value, Error *e );
	void		Update( const char *var, const char *value );

	ItemType	GetType( const char *var );
	int		FromRegistry( const char *var );

	void		Config( const StrPtr &cwd );
	void		LoadConfig( const StrPtr &cwd, int checkSyntax = 1 );
	void		LoadEnviro( int checkSyntax = 1 );

	void		Reload();

	void		SetCharSet( int );	// for i18n support
	int		GetCharSet();
	
	const StrPtr	&GetConfig();
	const StrArray	*GetConfigs();
	void		SetEnviroFile( const char * );
	const StrPtr	*GetEnviroFile();
	int		GetHome( StrBuf &result );

    private:

	EnviroTable	*symbolTab;
	EnviroItem	*GetItem( const char *var );
	void		ReadConfig( FileSys *, Error *, int, ItemType );
	void		Setup();

	bool		ReadItemPlatform( ItemType type, const char *var, EnviroItem * item );
	int		SetEnviro( const char *var, const char *value, Error *e );
	
	StrBuf		configFile;
	StrArray	*configFiles;
	StrBuf		enviroFile;
	StrBuf		serviceName;

	// used for netsslcredentials to get at service name
	static const StrPtr *sServiceNameStrP;

# ifdef OS_NT
	KeyPair		*setKey;
	KeyPair		*serviceKey;
	StrBuf		serviceKeyName;
	int		charset;
# endif /* OS_NT */

} ;

