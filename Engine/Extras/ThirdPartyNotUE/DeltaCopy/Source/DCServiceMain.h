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
#ifndef DCServiceMainH
#define DCServiceMainH
//---------------------------------------------------------------------------
#include <SysUtils.hpp>
#include <Classes.hpp>
#include <SvcMgr.hpp>
#include <vcl.h>


#include "Logger.h"
#include "ConsoleRunner.h"


struct ChildProcessInfo{
    HANDLE childHandle;
    DWORD  childPid;
};
//---------------------------------------------------------------------------
class TDeltaCopyService : public TService
{
__published:    // IDE-managed Components
    void __fastcall ServiceExecute(TService *Sender);
private:        // User declarations

    ChildProcessInfo childInfo;
    TConsoleRunner cRunner;
    vector<string> results;

    void SetCurrentDir();

public:         // User declarations
	__fastcall TDeltaCopyService(TComponent* Owner);
	TServiceController __fastcall GetServiceController(void);

	friend void __stdcall ServiceController(unsigned CtrlCode);

    void SpawnChildProcess();
    
};

class TChildProcessThread : public TThread{
    TDeltaCopyService* worker;
public:
    TChildProcessThread(TDeltaCopyService* worker) : TThread(false) {this->worker = worker;}
    void __fastcall Execute(){ worker->SpawnChildProcess();}

};

//---------------------------------------------------------------------------
extern PACKAGE TDeltaCopyService *DeltaCopyService;
//---------------------------------------------------------------------------
#endif
