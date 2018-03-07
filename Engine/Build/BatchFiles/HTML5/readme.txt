usage: 

Setup : install emcscripten ( https://github.com/kripken/emscripten/wiki/Tutorial ). 
Edit _internal.bat and configure vcvarsall.bat location as needed. ( to find nmake ) 

BuildThirdParty.bat # build 
BuildThirdParty.bat clean # clean
BuildThirdParty.bat install # copy bc files to their respective respective locations. 
BuildThirdParty.bat uninstall # delete bc files from their respective final locations

makedir.txt: 
this text file contains the name, root dir and makefile location for individual third party projects - edit this to include more projects. 

