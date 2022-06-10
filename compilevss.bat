@REM Compile solution from command line, using Visual Studio 2008.
@REM The path to VS2008 may need to be edited.
call "%VS140COMNTOOLS%vsvars32.bat" 
devenv hcdisk2.sln /build "Release|Win32"