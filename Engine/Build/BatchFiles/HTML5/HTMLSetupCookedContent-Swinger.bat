set GAME=Samples\SampleGames\SwingNinja
set DEST=%GAME%\Saved\Cooked\HTML5

pushd ..\..\..\..\
xcopy /y /i /d Engine\Config\*.ini %DEST%\Engine\Config
xcopy /y /i /d Engine\Config\HTML5\*.ini %DEST%\Engine\Config\HTML5
xcopy /y /i /d Engine\Content\HTML5\*.sb %DEST%\Engine\Content\HTML5
xcopy /y /i /d /s Engine\Config\*.ini %DEST%\Engine\Config
xcopy /y /i /d /s Engine\Content\Localization\*.* %DEST%\Engine\Content\Localization\*.*
xcopy /y /i /d /s Engine\Content\Editor\Slate\*.png %DEST%\Engine\Content\Editor\Slate

xcopy /y /i /d %GAME%\Config\*.ini %DEST%\%GAME%\Config
xcopy /y /i /d %GAME%\Config\HTML5\*.ini %DEST%\%GAME%\Config\HTML5
xcopy /y /i /d %GAME%\Content\Localization\*.* %DEST%\%GAME%\Content\Localization\*.*

%EMSCRIPTEN%\tools\file_packager.py Engine\Binaries\HTML5\SwingNinja.data --preload  %GAME%\Config Engine\Config  %GAME%\Saved\Cooked\HTML5@\ --pre-run --js-output=Engine\Binaries\HTML5\SwingNinja.data.js

popd
