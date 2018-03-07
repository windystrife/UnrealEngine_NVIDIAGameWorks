call ILMBaseVS.bat
set MSBUILDEMITSOLUTION=1
"C:/Program Files (x86)/MSBuild/14.0/Bin/msbuild.exe" %cd%/build/VS2015/ilmbase/ilmbase.sln /p:Configuration=Debug;Platform=x64 
"C:/Program Files (x86)/MSBuild/14.0/Bin/msbuild.exe" %cd%/build/VS2015/ilmbase/INSTALL.vcxproj /t:Rebuild /p:Configuration=Debug;Platform=x64
"C:/Program Files (x86)/MSBuild/14.0/Bin/msbuild.exe" %cd%/build/VS2015/ilmbase/ilmbase.sln /p:Configuration=Release;Platform=x64 
"C:/Program Files (x86)/MSBuild/14.0/Bin/msbuild.exe" %cd%/build/VS2015/ilmbase/INSTALL.vcxproj /t:Rebuild /p:Configuration=Release;Platform=x64

call HDF5VS.bat
"C:/Program Files (x86)/MSBuild/14.0/Bin/msbuild.exe" %cd%/build/VS2015/HDF5/alembic.sln /p:Configuration=Debug;Platform=x64 
"C:/Program Files (x86)/MSBuild/14.0/Bin/msbuild.exe" %cd%/build/VS2015/HDF5/INSTALL.vcxproj /t:Rebuild /p:Configuration=Debug;Platform=x64
"C:/Program Files (x86)/MSBuild/14.0/Bin/msbuild.exe" %cd%/build/VS2015/HDF5/alembic.sln /p:Configuration=Release;Platform=x64 
"C:/Program Files (x86)/MSBuild/14.0/Bin/msbuild.exe" %cd%/build/VS2015/HDF5/INSTALL.vcxproj /t:Rebuild /p:Configuration=Release;Platform=x64

call AlembicVS.bat
"C:/Program Files (x86)/MSBuild/14.0/Bin/msbuild.exe" %cd%/build/VS2015/alembic/alembic.sln /p:Configuration=Debug;Platform=x64 
"C:/Program Files (x86)/MSBuild/14.0/Bin/msbuild.exe" %cd%/build/VS2015/alembic/INSTALL.vcxproj /t:Rebuild /p:Configuration=Debug;Platform=x64
"C:/Program Files (x86)/MSBuild/14.0/Bin/msbuild.exe" %cd%/build/VS2015/alembic/alembic.sln /p:Configuration=Release;Platform=x64 
"C:/Program Files (x86)/MSBuild/14.0/Bin/msbuild.exe" %cd%/build/VS2015/alembic/INSTALL.vcxproj /t:Rebuild /p:Configuration=Release;Platform=x64
