/* 
   Copyright (C) Synametrics Technologies, Inc 2005
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/
//---------------------------------------------------------------------------

#ifndef ServiceStatusH
#define ServiceStatusH
//---------------------------------------------------------------------------

#include <Classes.hpp>

#define STATUS_ERROR    0
#define STATUS_SUCCESS 1

class TServiceInfo{

    int lastError;
    AnsiString lastErrorStr;
    SC_HANDLE   hSC;

public:
    TServiceInfo();
    TServiceInfo(bool forCreateService);


    int CheckStatus(AnsiString serviceName);
    int CreateNewService(AnsiString serviceName,
            AnsiString displayName, AnsiString path,
            AnsiString userId, AnsiString pwd);
    int DeleteExistingService(AnsiString serviceName);
    AnsiString GetLastErrorStr(){return lastErrorStr;}
    int GetMostRecentError(){ return lastError;}
    int RunService(AnsiString serviceName);
    int StopService(AnsiString serviceName);

    
};
#endif
 