#!/bin/bash
# This script will unzip the downloaded archives and convert to Unix format where needed.  
# It tries to clean up any previous versions using 'git clean' on certain directories.
# WARNING: This script can be destructive so use with caution if you have
# unsaved local changes.
#
# This script does not handle building of any of the ThirdParty dependencies.
# (See BuildThirdParty.sh).

# Make sure that the current working directory is the root the git checkout,
# which is four levels up from the this script.
SCRIPT_DIR=$(cd "$(dirname "$BASH_SOURCE")" ; pwd)
TOP_DIR=$(cd $SCRIPT_DIR/../../../.. ; pwd)
cd ${TOP_DIR}
set -e
trap "echo '==> An error occured while running UpdateDeps.sh'" ERR

CleanRepo() 
{
  # Clean any files installed from .zip files.
  echo "==> Cleaning existing dependencies"
  echo "About to cleanup previous zip contents with git clean."
  echo "WARNING: This operation can be destructive: It removes any untracked"
  echo "files in parts of the tree (e.g. Source/ThirdParty)."
  while true; do
    read -p "Do you want to continue? (y/n) " yn
    case $yn in
      [Yy] ) break;;
      [Nn] ) exit;;
      * ) echo "Please answer y or n.";;
    esac
  done

  CLEAN_DIRS="
    Engine/Binaries
    Engine/Source/ThirdParty
    Engine/Plugins/Script/ScriptGeneratorPlugin/Binaries
    Engine/Plugins/Messaging/UdpMessaging/Binaries
    Engine/Content
    Engine/Documentation
    Engine/Extras
    Samples/StarterContent
    Templates"
  for Dir in $CLEAN_DIRS; do
    git clean -xfd $Dir
  done
}

Unixify()
{
  echo "==> Converting to unix line endings"
  cd Engine/Source/ThirdParty
  echo "PWD=$PWD"

  cd libOpus/opus-1.1
  dos2unix missing
  dos2unix depcomp
  dos2unix ltmain.sh
  dos2unix config.*
  dos2unix package_version
  dos2unix configure.ac
  dos2unix `find -name "*.in"`
  dos2unix `find -name "*.m4"`
  cd -

  cd MCPP/mcpp-2.7.2/
  chmod +x configure
  dos2unix configure
  dos2unix src/config.h.in config/*
  cd -

  cd hlslcc/hlslcc
  dos2unix src/hlslcc_lib/hlslcc.h
  dos2unix src/hlslcc_lib/hlslcc_private.h
  dos2unix src/hlslcc_lib/glsl/ir_gen_glsl.h
  cd -

  cd Vorbis/libvorbis-1.3.2/
  dos2unix configure
  dos2unix missing
  dos2unix ltmain.sh
  dos2unix depcomp
  dos2unix config.*
  dos2unix `find -name Makefile.in`
  cd -

  cd Ogg/libogg-1.2.2/
  dos2unix -f configure
  dos2unix missing
  dos2unix ltmain.sh
  dos2unix depcomp
  dos2unix config.*
  dos2unix `find -name Makefile.in`
  cd -

  cd ${TOP_DIR}
}

#CleanRepo
Unixify

echo "********** SUCCESS ****************"

