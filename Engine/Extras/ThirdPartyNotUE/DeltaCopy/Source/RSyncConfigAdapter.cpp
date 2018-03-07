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


#pragma hdrstop

#include <Dialogs.hpp>
#include "RSyncConfigAdapter.h"

//---------------------------------------------------------------------------

#pragma package(smart_init)


TRsyncConfigAdapter::TRsyncConfigAdapter(){
    modules = new TObjectList();
    ReadConfig();
}
//---------------------------------------------------------------------------
TRsyncConfigAdapter::~TRsyncConfigAdapter(){
    delete modules;
}

//---------------------------------------------------------------------------
void TRsyncConfigAdapter::AddANewModule(AnsiString moduleName){

    AnsiString fixedName;
    moduleName = moduleName.Trim();

    for(int i = 1; i <= moduleName.Length(); i++){
        if( isalnum(moduleName[i]) ){
            fixedName += moduleName[i];
        }
    }

    TModuleHolder* currentModule = new TModuleHolder();
    currentModule->moduleName = fixedName;
    currentModule->nvp->Add("");
    currentModule->nvp->Add("    path = ");
	currentModule->nvp->Add("    comment = ");
	currentModule->nvp->Add("    read only = false");
	currentModule->nvp->Add("    auth users = ");
	currentModule->nvp->Add("    secrets file = ");
    currentModule->nvp->Add("");
    modules->Add(currentModule);
}
//---------------------------------------------------------------------------
AnsiString TRsyncConfigAdapter::FetchModuleLine(AnsiString oneLine){

    oneLine = oneLine.Trim();

    if(oneLine.Length() < 3){
        return "";
    }

    if(oneLine[1] == '[' && oneLine[oneLine.Length()] == ']'){
        return oneLine.SubString(2, oneLine.Length() - 2);
    }
    return "";
}



//---------------------------------------------------------------------------
TModuleHolder* TRsyncConfigAdapter::GetModuleHolder(AnsiString name){
    if(name == "") return NULL;

    for(int i = 0; i < modules->Count; i++){
        TModuleHolder* aHolder = (TModuleHolder*)modules->Items[i];
        if(aHolder->moduleName == name){
            return aHolder;
        }
    }
    return NULL;
}
//---------------------------------------------------------------------------
int TRsyncConfigAdapter::GetModuleNames(TStrings* answer){

    if(answer == NULL){
        return modules->Count;
    }
    for(int i = 0; i < modules->Count; i++){
        TModuleHolder* aHolder = (TModuleHolder*)modules->Items[i];
        if(aHolder->moduleName == GLOBAL_MODULE){
            continue;
        }
        answer->Add(aHolder->moduleName);
    }
    return modules->Count;
}

//---------------------------------------------------------------------------
AnsiString TRsyncConfigAdapter::GetParamValue(AnsiString moduleName, AnsiString paramName){
    TModuleHolder* aHolder = GetModuleHolder(moduleName);

    if(aHolder == NULL){
        return "Invalid module name";
    }


    TStringList* params = aHolder->nvp;

    for(int i = 0; i < params->Count; i++){
        AnsiString oneLine = params->Strings[i];
        AnsiString sName, sValue;

        if(ParseNameValue(oneLine, sName, sValue)){
            if(sName == paramName){
                return sValue;
            }
        }

    }
    return "";
}

//---------------------------------------------------------------------------
bool TRsyncConfigAdapter::ParseNameValue(AnsiString input,
                AnsiString& name, AnsiString& value){

    input = input.Trim();

    if(input == "") return false;
    if(input[1] == '#') return false;

    int pos = input.Pos("=");

    if(pos == 0) return false;


    name = input.SubString(1, pos - 1).Trim();
    value = input.SubString(pos + 1, input.Length()).Trim();

    return true;

}
//---------------------------------------------------------------------------
void TRsyncConfigAdapter::ReadConfig(){

    if(!FileExists(CONFIG_FILE)){
        ShowMessage("Config " CONFIG_FILE " file not found");
        return;
    }


    TStringList* lines = new TStringList();
    lines->LoadFromFile(CONFIG_FILE);


    TModuleHolder* currentModule = new TModuleHolder();
    currentModule->moduleName = GLOBAL_MODULE;
    modules->Add(currentModule);

    for(int i = 0; i < lines->Count; i++){
        AnsiString moduleName = FetchModuleLine(lines->Strings[i]);

        if(moduleName.Length() > 0){
            currentModule = new TModuleHolder();
            modules->Add(currentModule);
            currentModule->moduleName = moduleName;
            continue;
        }

        currentModule->nvp->Add(lines->Strings[i]);
    }

    delete lines;
}
//---------------------------------------------------------------------------
void TRsyncConfigAdapter::RemoveModule(AnsiString name){
    if(name == "") return;

    for(int i = 0; i < modules->Count; i++){
        TModuleHolder* aHolder = (TModuleHolder*)modules->Items[i];
        if(aHolder->moduleName == name){
            modules->Delete(i);
            return;
        }
    }
}
//---------------------------------------------------------------------------
void TRsyncConfigAdapter::RenameModule(AnsiString oldName, AnsiString newName){
    TModuleHolder* holder = GetModuleHolder(oldName);
    if(holder != NULL){
        holder->moduleName = newName;
    }
}
//---------------------------------------------------------------------------
void TRsyncConfigAdapter::SaveConfig(){



    TStringList* lines = new TStringList();



    TModuleHolder* currentModule = GetModuleHolder(GLOBAL_MODULE);


    for(int i = 0; i < currentModule->nvp->Count; i++){
        lines->Add(currentModule->nvp->Strings[i]);
    }

    //Now get remaining modules
    for(int j = 0; j < modules->Count; j++){
        currentModule = (TModuleHolder*)modules->Items[j];
        if(currentModule->moduleName == GLOBAL_MODULE){
            continue;
        }

        lines->Add("[" + currentModule->moduleName + "]");
        for(int i = 0; i < currentModule->nvp->Count; i++){
            lines->Add(currentModule->nvp->Strings[i]);
        }
    }

    lines->SaveToFile(CONFIG_FILE);


    delete lines;
}
//---------------------------------------------------------------------------
void TRsyncConfigAdapter::SetParamValue(AnsiString moduleName, AnsiString paramName, AnsiString paramValue){
    TModuleHolder* aHolder = GetModuleHolder(moduleName);

    if(aHolder == NULL){
        return; //"Invalid module name";
    }


    TStringList* params = aHolder->nvp;

	bool newModule = true;
    for(int i = 0; i < params->Count; i++){
        AnsiString oneLine = params->Strings[i];
        AnsiString sName, sValue;

        if(ParseNameValue(oneLine, sName, sValue)){
            if(sName == paramName){
				params->Strings[i] = "    " + paramName + " = " + paramValue;
				newModule = false;
                break;
            }
        }

	}

	if(newModule){
		params->Add("    " + paramName + " = " + paramValue);
	}

}
