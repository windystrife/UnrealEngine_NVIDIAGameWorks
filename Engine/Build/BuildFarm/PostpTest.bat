@echo off
if [%1]==[] goto invalid_args
if [%2]==[] goto invalid_args

ec-perl PostpFilter.pl < %2 | postp --load=./PostpExtensions.pl --jobStepId=%1
goto :eof

:invalid_args
echo Syntax: PostpTest [JobStepId] [LogFile]
