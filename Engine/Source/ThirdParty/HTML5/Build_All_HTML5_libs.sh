#!/bin/sh

# build all ThirdParty libs for HTML5
# from the simplest to build to the most complex
TPS_HTML5=$(pwd)

cd "$TPS_HTML5"/../zlib/zlib-1.2.5/Src/HTML5
	./build_html5.sh

cd "$TPS_HTML5"/../libPNG/libPNG-1.5.2/projects/HTML5
	./build_html5.sh

cd "$TPS_HTML5"/../FreeType2/FreeType2-2.6/Builds/html5
	./build_html5.sh

cd "$TPS_HTML5"/../Ogg/libogg-1.2.2/build/HTML5
	./build_html5.sh

cd "$TPS_HTML5"/../Vorbis/libvorbis-1.3.2/build/HTML5
	./build_html5.sh

cd "$TPS_HTML5"/../ICU/icu4c-53_1
	./BuildForHTML5.sh

cd "$TPS_HTML5"/../HarfBuzz/harfbuzz-1.2.4/BuildForUE/HTML5
	./BuildForHTML5.sh

cd "$TPS_HTML5"/../PhysX/PhysX_3.4/Source/compiler/HTML5
	./BuildForHTML5.sh


cd "$TPS_HTML5"
