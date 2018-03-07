# Fix Mono and engine dependencies if needed
START_DIR=`pwd`
cd "$1"
sh FixMonoFiles.sh
sh FixDependencyFiles.sh

IS_MONO_INSTALLED=0
MONO_VERSION_PATH=`which mono` || true
if [ ! $MONO_VERSION_PATH == "" ] && [ -f $MONO_VERSION_PATH ]; then
	# If Mono is installed, check if it's 4.0.2 or higher
	MONO_VERSION_PREFIX="Mono JIT compiler version "
	MONO_VERSION_PREFIX_LEN=${#MONO_VERSION_PREFIX}
	MONO_VERSION=`"${MONO_VERSION_PATH}" --version |grep "$MONO_VERSION_PREFIX"`
	MONO_VERSION=(`echo ${MONO_VERSION:MONO_VERSION_PREFIX_LEN} |tr '.' ' '`)
	if [ ${MONO_VERSION[0]} -ge 4 ]; then
		if [ ${MONO_VERSION[1]} -eq 0 ] && [ ${MONO_VERSION[2]} -ge 2 ]; then
			IS_MONO_INSTALLED=1
		elif [ ${MONO_VERSION[1]} -gt 0 ] && [ ${MONO_VERSION[1]} -lt 6 ]; then # Mono 4.6 has issues on macOS 10.12
			IS_MONO_INSTALLED=1
		fi
	fi
fi

# Setup bundled Mono if cannot use installed one
if [ $IS_MONO_INSTALLED -eq 0 ]; then
	echo Setting up Mono
	CUR_DIR=`pwd`
	export UE_MONO_DIR=$CUR_DIR/../../../Binaries/ThirdParty/Mono/Mac
	export PATH=$UE_MONO_DIR/bin:$PATH
	export MONO_PATH=$UE_MONO_DIR/lib:$MONO_PATH
	export LD_LIBRARY_PATH=$UE_MONO_DIR/lib:$LD_LIBRARY_PATH
fi

cd "$START_DIR"
