setlocal

REM update the blast dlls and headers

REM First commandline argument must be the path to the Blast SDK to use, e.g.:

@SET BLAST_SOURCE_DIR=%~1
@SET BLAST_PLUGIN_DIR=%~sdp0\

robocopy %BLAST_SOURCE_DIR%\bin\vc14win64-cmake %BLAST_PLUGIN_DIR%\Libraries\Win64 ^
    NvBlast_x64.dll ^
    NvBlast_x64.pdb ^
    NvBlastDEBUG_x64.dll ^
    NvBlastDEBUG_x64.pdb ^
    NvBlastPROFILE_x64.dll ^
    NvBlastPROFILE_x64.pdb ^
    NvBlastGlobals_x64.dll ^
    NvBlastGlobals_x64.pdb ^
    NvBlastGlobalsDEBUG_x64.dll ^
    NvBlastGlobalsDEBUG_x64.pdb ^
    NvBlastGlobalsPROFILE_x64.dll ^
    NvBlastGlobalsPROFILE_x64.pdb ^
    NvBlastExtAssetUtils_x64.dll ^
    NvBlastExtAssetUtils_x64.pdb ^
    NvBlastExtAssetUtilsDEBUG_x64.dll ^
    NvBlastExtAssetUtilsDEBUG_x64.pdb ^
    NvBlastExtAssetUtilsPROFILE_x64.dll ^
    NvBlastExtAssetUtilsPROFILE_x64.pdb ^
    NvBlastExtAuthoring_x64.dll ^
    NvBlastExtAuthoring_x64.pdb ^
    NvBlastExtAuthoringDEBUG_x64.dll ^
    NvBlastExtAuthoringDEBUG_x64.pdb ^
    NvBlastExtAuthoringPROFILE_x64.dll ^
    NvBlastExtAuthoringPROFILE_x64.pdb ^
    NvBlastExtSerialization_x64.dll ^
    NvBlastExtSerialization_x64.pdb ^
    NvBlastExtSerializationDEBUG_x64.dll ^
    NvBlastExtSerializationDEBUG_x64.pdb ^
    NvBlastExtSerializationPROFILE_x64.dll ^
    NvBlastExtSerializationPROFILE_x64.pdb ^
    NvBlastExtShaders_x64.dll ^
    NvBlastExtShaders_x64.pdb ^
    NvBlastExtShadersDEBUG_x64.dll ^
    NvBlastExtShadersDEBUG_x64.pdb ^
    NvBlastExtShadersPROFILE_x64.dll ^
    NvBlastExtShadersPROFILE_x64.pdb ^
    NvBlastExtStress_x64.dll ^
    NvBlastExtStress_x64.pdb ^
    NvBlastExtStressDEBUG_x64.dll ^
    NvBlastExtStressDEBUG_x64.pdb ^
    NvBlastExtStressPROFILE_x64.dll ^
    NvBlastExtStressPROFILE_x64.pdb

robocopy %BLAST_SOURCE_DIR%\lib\vc14win64-cmake %BLAST_PLUGIN_DIR%\Libraries\Win64 ^
    NvBlast_x64.lib ^
    NvBlastDEBUG_x64.lib ^
    NvBlastPROFILE_x64.lib ^
    NvBlastGlobals_x64.lib ^
    NvBlastGlobalsDEBUG_x64.lib ^
    NvBlastGlobalsPROFILE_x64.lib ^
    NvBlastExtAssetUtils_x64.lib ^
    NvBlastExtAssetUtilsDEBUG_x64.lib ^
    NvBlastExtAssetUtilsPROFILE_x64.lib ^
    NvBlastExtAuthoring_x64.lib ^
    NvBlastExtAuthoringDEBUG_x64.lib ^
    NvBlastExtAuthoringPROFILE_x64.lib ^
    NvBlastExtSerialization_x64.lib ^
    NvBlastExtSerializationDEBUG_x64.lib ^
    NvBlastExtSerializationPROFILE_x64.lib ^
    NvBlastExtShaders_x64.lib ^
    NvBlastExtShadersDEBUG_x64.lib ^
    NvBlastExtShadersPROFILE_x64.lib ^
    NvBlastExtStress_x64.lib ^
    NvBlastExtStressDEBUG_x64.lib ^
    NvBlastExtStressPROFILE_x64.lib

robocopy %BLAST_SOURCE_DIR%\bin\linux64-UE4 %BLAST_PLUGIN_DIR%\Libraries\Linux ^
    libNvBlast.so ^
    libNvBlastDEBUG.so ^
    libNvBlastGlobals.so ^
    libNvBlastGlobalsDEBUG.so ^
    libNvBlastExtAssetUtils.so ^
    libNvBlastExtAssetUtilsDEBUG.so ^
    libNvBlastExtAuthoring.so ^
    libNvBlastExtAuthoringDEBUG.so ^
    libNvBlastExtSerialization.so ^
    libNvBlastExtSerializationDEBUG.so ^
    libNvBlastExtShaders.so ^
    libNvBlastExtShadersDEBUG.so ^
    libNvBlastExtStress.so ^
    libNvBlastExtStressDEBUG.so

robocopy %BLAST_SOURCE_DIR%\sdk\lowlevel\include %BLAST_PLUGIN_DIR%\Source\Blast\Public\lowlevel\include *.h /PURGE
robocopy %BLAST_SOURCE_DIR%\sdk\globals\include %BLAST_PLUGIN_DIR%\Source\Blast\Public\globals\include *.h /PURGE
robocopy %BLAST_SOURCE_DIR%\sdk\extensions\assetutils\include %BLAST_PLUGIN_DIR%\Source\Blast\Public\extensions\assetutils\include *.h /PURGE
robocopy %BLAST_SOURCE_DIR%\sdk\extensions\authoring\include %BLAST_PLUGIN_DIR%\Source\Blast\Public\extensions\authoring\include *.h /PURGE
robocopy %BLAST_SOURCE_DIR%\sdk\extensions\serialization\include %BLAST_PLUGIN_DIR%\Source\Blast\Public\extensions\serialization\include *.h /PURGE
robocopy %BLAST_SOURCE_DIR%\sdk\extensions\shaders\include %BLAST_PLUGIN_DIR%\Source\Blast\Public\extensions\shaders\include *.h /PURGE
robocopy %BLAST_SOURCE_DIR%\sdk\extensions\stress\include %BLAST_PLUGIN_DIR%\Source\Blast\Public\extensions\stress\include *.h /PURGE
