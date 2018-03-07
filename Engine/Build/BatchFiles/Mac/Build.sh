#!/bin/sh

# This script gets called every time Xcode does a build or clean operation, even though it's called "Build.sh".
# Values for $ACTION: "" = building, "clean" = cleaning

# Setup Mono
source Engine/Build/BatchFiles/Mac/SetupMono.sh Engine/Build/BatchFiles/Mac

# override env if action is specified on command line 
if [ $1 == "clean" ] 
	then 
		ACTION="clean" 
fi 


case $ACTION in
	"")
		echo Building $1...

                Platform=""
                AdditionalFlags=""
		
		case $CLANG_STATIC_ANALYZER_MODE in
				"deep")
					AdditionalFlags+="-SkipActionHistory"
					;;
				"shallow")
					AdditionalFlags+="-SkipActionHistory"
					;;
				esac

		case $ENABLE_THREAD_SANITIZER in
			"YES"|"1")
				# Disable TSAN atomic->non-atomic race reporting as we aren't C++11 memory-model conformant so UHT will fail
				export TSAN_OPTIONS="suppress_equal_stacks=true suppress_equal_addresses=true report_atomic_races=false"
			;;
			esac

		case $2 in 
			"iphoneos"|"IOS")
		        Platform="IOS"
				AdditionalFlags+=" -deploy -nocreatestub "
			;; 
			"appletvos")
		        Platform="TVOS"
				AdditionalFlags+=" -deploy -nocreatestub "
			;; 
  			"iphonesimulator")
		        	Platform="IOS"
		         	AdditionalFlags+=" -deploy -simulator -nocreatestub"
			;;
			"macosx")
					Platform="Mac"
			;;
			"HTML5")
				Platform="HTML5"
			;;
			*)
				Platform="$2"
				AdditionalFlags+=" -deploy " 
			;; 
		esac
		
		BuildTasks=$(defaults read com.apple.dt.Xcode IDEBuildOperationMaxNumberOfConcurrentCompileTasks)
		export NumUBTBuildTasks=$BuildTasks

		echo Running command : Engine/Binaries/DotNET/UnrealBuildTool.exe $1 $Platform $3 $AdditionalFlags "${@:4}"
		mono Engine/Binaries/DotNET/UnrealBuildTool.exe $1 $Platform $3 $AdditionalFlags "${@:4}"
		;;
	"clean")
		echo "Cleaning $2 $3 $4..."

                Platform=""
                AdditionalFlags="-clean"

		case $3 in 
			"iphoneos"|"IOS")
	        	        Platform="IOS"
				AdditionalFlags+=" -nocreatestub"
			;;
			"iphonesimulator")
			        Platform="IOS"
		        	AdditionalFlags+=" -simulator"
				AdditionalFlags+=" -nocreatestub"
			;; 
			"macosx")
					Platform="Mac"
			;;
			"HTML5")
				Platform="HTML5"
			;;
			*) 
        			Platform="$3"
			;;
		
		esac
		echo Running command: mono Engine/Binaries/DotNET/UnrealBuildTool.exe $2 $Platform $4 $AdditionalFlags "${@:5}"
		mono Engine/Binaries/DotNET/UnrealBuildTool.exe $2 $Platform $4 $AdditionalFlags "${@:5}"
		;;
esac

ExitCode=$?
if [ $ExitCode -eq 254 ] || [ $ExitCode -eq 255 ] || [ $ExitCode -eq 2 ]; then
	exit 0
else
	exit $ExitCode
fi
