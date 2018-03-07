@echo off

call python -u %~dp0\..\..\..\..\Engine\Source\ThirdParty\zlib\zlib-1.2.5\Src\HTML5\build_for_html5.py rebuild verbose
call python -u %~dp0\..\..\..\..\Engine\Source\ThirdParty\FreeType2\FreeType2-2.6\Builds\html5\build_for_html5.py rebuild verbose
call python -u %~dp0\..\..\..\..\Engine\Source\ThirdParty\HarfBuzz\harfbuzz-1.2.4\BuildForUE\HTML5\build_for_html5.py rebuild verbose
call python -u %~dp0\..\..\..\..\Engine\Source\ThirdParty\libPNG\libPNG-1.5.2\projects\HTML5\build_for_html5.py rebuild verbose
call python -u %~dp0\..\..\..\..\Engine\Source\ThirdParty\Ogg\libogg-1.2.2\build\HTML5\build_for_html5.py rebuild verbose
call python -u %~dp0\..\..\..\..\Engine\Source\ThirdParty\Vorbis\libvorbis-1.3.2\build\HTML5\build_for_html5.py rebuild verbose
call python -u %~dp0\..\..\..\..\Engine\Source\ThirdParty\PhysX\PhysX_3.4\Source\compiler\HTML5\build_for_html5.py rebuild verbose
