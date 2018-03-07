#!/bin/bash

SCRIPT_DIR=$(cd "$(dirname "$BASH_SOURCE")" ; pwd)
TOP_DIR=$(cd "$SCRIPT_DIR/../../.." ; pwd)

AddGDBPrettyPrinters()
{
	echo -ne "Attempting to set up UE4 pretty printers for gdb (existing UE4Printers.py, if any, will be overwritten)...\n\t"

	# Copy the pretty printer into the appropriate folder.
	mkdir -p ~/.config/Epic/GDBPrinters/
	if [ -e ~/.config/Epic/GDBPrinters/UE4Printers.py ]; then
		chmod 644 ~/.config/Epic/GDBPrinters/UE4Printers.py 	# set R/W so we can overwrite it
	fi
	cp "$TOP_DIR/Extras/GDBPrinters/UE4Printers.py" ~/.config/Epic/GDBPrinters/
	echo -ne "updated UE4Printers.py\n\t"
	chmod 644 ~/.config/Epic/GDBPrinters/UE4Printers.py 	# set R/W again (it can be read-only if copied from Perforce)

	# Check if .gdbinit exists. If not create else add needed parts.
	if [ ! -f ~/.gdbinit ]; then
		echo "no ~/.gdbinit file found - creating a new one."
		echo -e "python \nimport sys\n\nsys.path.append('$HOME/.config/Epic/GDBPrinters/')\n\nfrom UE4Printers import register_ue4_printers\nregister_ue4_printers(None)\nprint(\"Registered pretty printers for UE4 classes\")\n\nend" >> ~/.gdbinit
	else
		if grep -q "register_ue4_printers" ~/.gdbinit; then
			echo "found necessary entries in ~/.gdbinit file, not changing it."
		else
			echo -e "cannot modify .gdbinit. Please add the below lines manually:\n\n"
			echo -e "python"
			echo -e "\timport sys"
			echo -e "\tsys.path.append('$HOME/.config/Epic/GDBPrinters/')"
			echo -e "\tfrom UE4Printers import register_ue4_printers"
			echo -e "\tregister_ue4_printers(None)"
			echo -e "\tprint(\"Registered pretty printers for UE4 classes\")"
			echo -e "\tend"
			echo -e "\n\n"
		fi
	fi
}


# args: wrong filename, correct filename
# expects to be run in Engine folder
CreateLinkIfNoneExists()
{
    WrongName=$1
    CorrectName=$2

    pushd `dirname $CorrectName` > /dev/null
    if [ ! -f `basename $CorrectName` ] && [ -f $WrongName ]; then
      echo "$WrongName -> $CorrectName"
      ln -sf $WrongName `basename $CorrectName`
    fi
    popd > /dev/null
}

# args: package
# returns 0 if installed, 1 if it needs to be installed
PackageIsInstalled()
{
    PackageName=$1

    dpkg-query -W -f='${Status}\n' $PackageName | head -n 1 | awk '{print $3;}' | grep -q '^installed$'
}

# main
set -e

TOP_DIR=$(cd "$SCRIPT_DIR/../../.." ; pwd)
cd "${TOP_DIR}"

IS_GITHUB_BUILD=true
if [ -e Build/PerforceBuild.txt ]; then
  IS_GITHUB_BUILD=false
fi

if [ -e /etc/os-release ]; then
  source /etc/os-release
  # Ubuntu/Debian/Mint
  if [[ "$ID" == "ubuntu" ]] || [[ "$ID_LIKE" == "ubuntu" ]] || [[ "$ID" == "debian" ]] || [[ "$ID_LIKE" == "debian" ]] || [[ "$ID" == "tanglu" ]] || [[ "$ID_LIKE" == "tanglu" ]]; then
    # Install the necessary dependencies (require clang-3.8 on 16.04, although 3.3 and 3.5 through 3.7 should work too for this release)
     # mono-devel is needed for making the installed build (particularly installing resgen2 tool)
    if [ -n "$VERSION_ID" ] && [[ "$VERSION_ID" < 16.04 ]]; then
     DEPS="mono-xbuild \
       mono-dmcs \
       libmono-microsoft-build-tasks-v4.0-4.0-cil \
       libmono-system-data-datasetextensions4.0-cil
       libmono-system-web-extensions4.0-cil
       libmono-system-management4.0-cil
       libmono-system-xml-linq4.0-cil
       libmono-corlib4.0-cil
       libmono-windowsbase4.0-cil
       libmono-system-io-compression4.0-cil
       libmono-system-io-compression-filesystem4.0-cil
       mono-devel
       clang-3.5
       build-essential
       "
    elif [ -n "$VERSION_ID" ] && [[ "$VERSION_ID" == 16.04 ]]; then
     DEPS="mono-xbuild \
       mono-dmcs \
       libmono-microsoft-build-tasks-v4.0-4.0-cil \
       libmono-system-data-datasetextensions4.0-cil
       libmono-system-web-extensions4.0-cil
       libmono-system-management4.0-cil
       libmono-system-xml-linq4.0-cil
       libmono-corlib4.5-cil
       libmono-windowsbase4.0-cil
       libmono-system-io-compression4.0-cil
       libmono-system-io-compression-filesystem4.0-cil
       libmono-system-runtime4.0-cil
       mono-devel
       clang-3.8
       build-essential
       "
    elif [[ $PRETTY_NAME == *sid ]] || [[ $PRETTY_NAME == *stretch ]]; then
     DEPS="mono-xbuild \
       mono-dmcs \
       libmono-microsoft-build-tasks-v4.0-4.0-cil \
       libmono-system-data-datasetextensions4.0-cil
       libmono-system-web-extensions4.0-cil
       libmono-system-management4.0-cil
       libmono-system-xml-linq4.0-cil
       libmono-corlib4.5-cil
       libmono-windowsbase4.0-cil
       libmono-system-io-compression4.0-cil
       libmono-system-io-compression-filesystem4.0-cil
       libmono-system-runtime4.0-cil
       mono-devel
       clang-3.8
       "
    else # assume the latest Ubuntu, this is going to be a moving target
     DEPS="mono-xbuild \
       mono-dmcs \
       libmono-microsoft-build-tasks-v4.0-4.0-cil \
       libmono-system-data-datasetextensions4.0-cil
       libmono-system-web-extensions4.0-cil
       libmono-system-management4.0-cil
       libmono-system-xml-linq4.0-cil
       libmono-corlib4.5-cil
       libmono-windowsbase4.0-cil
       libmono-system-io-compression4.0-cil
       libmono-system-io-compression-filesystem4.0-cil
       libmono-system-runtime4.0-cil
       mono-devel
       clang-3.9
       build-essential
       "
    fi

    # these tools are only needed to build third-party software which is prebuilt for Ubuntu.
    if [[ "$ID" != "ubuntu" ]]; then
      DEPS+="libqt4-dev \
             cmake
            "
    fi

    for DEP in $DEPS; do
      if ! PackageIsInstalled $DEP; then
        echo "Attempting installation of missing package: $DEP"
        set -x
        sudo apt-get install -y $DEP
        set +x
      fi
    done
  fi
  
  # openSUSE/SLED/SLES
  if [[ "$ID" == "opensuse" ]] || [[ "$ID_LIKE" == "suse" ]]; then
    # Install all necessary dependencies
    DEPS="mono-core
      mono-devel
      mono-mvc
      libqt4-devel
      dos2unix
      cmake
      "

    for DEP in $DEPS; do
      if ! rpm -q $DEP > /dev/null 2>&1; then
        echo "Attempting installation of missing package: $DEP"
        set -x
        sudo zypper -n install $DEP
        set +x
      fi
    done
  fi

  # Fedora
  if [[ "$ID" == "fedora" ]]; then
    # Install all necessary dependencies
    DEPS="mono-core
      mono-devel
      qt-devel
      dos2unix
      cmake
      clang
      "

    for DEP in $DEPS; do
      if ! rpm -q $DEP > /dev/null 2>&1; then
        echo "Attempting installation of missing package: $DEP"
        set -x
        sudo dnf -y install $DEP
        set +x
      fi
    done
  fi


  # Arch Linux
  if [[ "$ID" == "arch" ]] || [[ "$ID_LIKE" == "arch" ]]; then
    DEPS="clang mono python sdl2 qt4 dos2unix cmake"
    MISSING=false
    for DEP in $DEPS; do
      if ! pacman -Qs $DEP > /dev/null 2>&1; then
        MISSING=true
        break
      fi
    done
    if [ "$MISSING" = true ]; then
      echo "Attempting to install missing packages: $DEPS"
      set -x
      sudo pacman -S --needed --noconfirm $DEPS
      set +x
    fi
  fi
fi

MONO_MINIMUM_VERSION=3
MONO_VERSION=$(mono -V | sed -n 's/.* version \([0-9]\+\).*/\1/p')
if [[ "$MONO_VERSION" -lt "$MONO_MINIMUM_VERSION" ]]; then
  echo "Minimum required Mono version is $MONO_MINIMUM_VERSION. Installed is:"
  mono -V | sed -n '/version/p'
  exit 1
fi

echo 
if [ "$IS_GITHUB_BUILD" = true ]; then
	echo Github build
	echo Checking / downloading the latest archives
	Build/BatchFiles/Linux/GitDependencies.sh --prompt "$@"
else
	echo Perforce build
	echo Assuming availability of up to date third-party libraries
fi

# Fixes for case sensitive filesystem.
echo Fixing inconsistent case in filenames.
for BASE in Content/Editor/Slate Content/Slate Documentation/Source/Shared/Icons; do
  find $BASE -name "*.PNG" | while read PNG_UPPER; do
    png_lower="$(echo "$PNG_UPPER" | sed 's/.PNG$/.png/')"
    if [ ! -f $png_lower ]; then
      PNG_UPPER=$(basename $PNG_UPPER)
      echo "$png_lower -> $PNG_UPPER"
      # link, and not move, to make it usable with Perforce workspaces
      ln -sf `basename "$PNG_UPPER"` "$png_lower"
    fi
  done
done

CreateLinkIfNoneExists ../../engine/shaders/Fxaa3_11.usf  ../Engine/Shaders/Fxaa3_11.usf
CreateLinkIfNoneExists ../../Engine/shaders/Fxaa3_11.usf  ../Engine/Shaders/Fxaa3_11.usf

# Provide the hooks for locally building third party libs if needed
echo
pushd Build/BatchFiles/Linux > /dev/null
./BuildThirdParty.sh
popd > /dev/null

# Creation of user shortcuts and addition of Mime types for Ubuntu
if [ -e /etc/os-release ]; then
  source /etc/os-release
  # Ubuntu/Debian/Mint
  if [[ "$ID" == "ubuntu" ]] || [[ "$ID_LIKE" == "ubuntu" ]] || [[ "$ID" == "debian" ]] || [[ "$ID_LIKE" == "debian" ]] || [[ "$ID" == "tanglu" ]] || [[ "$ID_LIKE" == "tanglu" ]]; then
    echo "Installing UE4 project types associations"
    # Place icon in system icon folder
    if [ ! -f ~/.local/share/icons/ue4editor.png ]; then
        mkdir -p ~/.local/share/icons
        cp "$TOP_DIR/Source/Programs/UnrealVS/Resources/Preview.png" ~/.local/share/icons/ue4editor.png
    fi
    # Generate Mime type file
    if [ ! -f ~/.local/share/mime/packages/uproject.xml ]; then
        mkdir -p ~/.local/share/mime/packages/
        cp "$TOP_DIR/Build/Linux/uproject.xml" ~/.local/share/mime/packages/
        update-mime-database ~/.local/share/mime
    fi
    # Generate .desktop file
    if [ -d ~/.local/share/applications ] && [ ! -f ~/.local/share/applications/UE4Editor.desktop ]; then
        ICON_DIR=$(cd $TOP_DIR/../../.. ; pwd)
        echo "#!/usr/bin/env xdg-open
[Desktop Entry]
Version=1.0
Type=Application
Exec=$TOP_DIR/Binaries/Linux/UE4Editor %f
Path=$TOP_DIR/Binaries/Linux
Name=Unreal Engine Editor
Icon=ue4editor
Terminal=false
StartupWMClass=UE4Editor
MimeType=application/uproject;" > ~/.local/share/applications/UE4Editor.desktop
        chmod u+x ~/.local/share/applications/UE4Editor.desktop
        update-desktop-database ~/.local/share/applications
    fi
  fi
fi

# Add GDB scripts for common Unreal types.
AddGDBPrettyPrinters

echo "Setup successful."
touch Build/OneTimeSetupPerformed
