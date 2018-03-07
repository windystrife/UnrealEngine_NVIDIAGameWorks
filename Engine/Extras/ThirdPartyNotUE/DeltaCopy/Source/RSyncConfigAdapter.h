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

#ifndef RSyncConfigAdapterH
#define RSyncConfigAdapterH
//---------------------------------------------------------------------------


#include <Classes.hpp>

#define CONFIG_FILE     "deltacd.conf"
#define GLOBAL_MODULE   "*****Global_Module*****"


class TModuleHolder : public TObject{

public:
    AnsiString moduleName;
    TStringList* nvp;

    __fastcall TModuleHolder(){
        nvp = new TStringList();
    }

     __fastcall ~TModuleHolder(){
        nvp->Clear();
        delete nvp;
    }
};

//---------------------------------------------------------------------------
class TRsyncConfigAdapter{

    TObjectList* modules;

    AnsiString FetchModuleLine(AnsiString);
    TModuleHolder* GetModuleHolder(AnsiString name);
    bool ParseNameValue(AnsiString, AnsiString&, AnsiString&);
public:
    TRsyncConfigAdapter();
    ~TRsyncConfigAdapter();


    void AddANewModule(AnsiString moduleName);
    int GetModuleNames(TStrings*);
    AnsiString GetParamValue(AnsiString moduleName, AnsiString paramName);
    void RemoveModule(AnsiString moduleName);
    void RenameModule(AnsiString oldName, AnsiString newName);
    void SaveConfig();
    void SetParamValue(AnsiString moduleName, AnsiString paramName, AnsiString paramValue);

    void ReadConfig();

};
#endif
 