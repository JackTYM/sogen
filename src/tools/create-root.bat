@ECHO OFF

NET SESSIONS > NUL 2>&1
IF %ERRORLEVEL% NEQ 0 (
	ECHO Error: This script requires administrative privileges.
	EXIT /B 1
)

SET SYSDIR="%WINDIR%\System32"
SET SYSDIR_WOW64="%WINDIR%\SysWOW64"

SET EMU_ROOT=root
SET EMU_FILESYS=%EMU_ROOT%\filesys
SET EMU_WINDIR=%EMU_FILESYS%\c\windows
SET EMU_SYSDIR=%EMU_WINDIR%\system32
SET EMU_SYSDIR_WOW64=%EMU_WINDIR%\syswow64
SET EMU_CURSORDIR=%EMU_WINDIR%\cursors
SET EMU_REGDIR=%EMU_ROOT%\registry

MKDIR %EMU_SYSDIR%
MKDIR %EMU_SYSDIR_WOW64%
MKDIR %EMU_CURSORDIR%
MKDIR %EMU_REGDIR%

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0create-profile-dirs.ps1" "%EMU_FILESYS%"

REG SAVE HKLM\HARDWARE %EMU_REGDIR%\HARDWARE /Y
REG SAVE HKLM\SAM %EMU_REGDIR%\SAM /Y
REG SAVE HKLM\SECURITY %EMU_REGDIR%\SECURITY /Y
REG SAVE HKLM\SOFTWARE %EMU_REGDIR%\SOFTWARE /Y
REG SAVE HKLM\SYSTEM %EMU_REGDIR%\SYSTEM /Y
COPY /B /Y C:\Users\Default\NTUSER.DAT "%EMU_REGDIR%\NTUSER.DAT"

CALL :collect archiveint.dll
CALL :collect avicap32.dll
CALL :collect avifil32.dll
CALL :collect concrt140.dll
CALL :collect dciman32.dll
CALL :collect dfscli.dll
CALL :collect fontsub.dll
CALL :collect hlink.dll
CALL :collect mfc140.dll
CALL :collect mfc140u.dll
CALL :collect msls31.dll
CALL :collect msvcp140_1.dll
CALL :collect msvcp140_2.dll
CALL :collect msvcp140_codecvt_ids.dll
CALL :collect msvfw32.dll
CALL :collect prntvpt.dll
CALL :collect riched20.dll
CALL :collect shfolder.dll
CALL :collect snmpapi.dll
CALL :collect stdole2.tlb
CALL :collect stdole32.tlb
CALL :collect sxs.dll
CALL :collect authz.dll
CALL :collect credssp.dll
CALL :collect fastprox.dll
CALL :collect pcacli.dll
CALL :collect rpcrtremote.dll
CALL :collect scecli.dll
CALL :collect schannel.dll
CALL :collect sfc.dll
CALL :collect spinf.dll
CALL :collect srpapi.dll
CALL :collect tsappcmp.dll
CALL :collect vssapi.dll
CALL :collect wmiclnt.dll
CALL :collect windows.staterepository.dll
CALL :collect windows.staterepositorycore.dll
CALL :collect windows.staterepositoryps.dll
CALL :collect windows.staterepositorybroker.dll
CALL :collect windows.staterepositoryclient.dll
CALL :collect windows.staterepositoryupgrade.dll
CALL :collect staterepository.core.dll
CALL :collect vccorlib140.dll
CALL :collect vcomp140.dll
CALL :collect wmasf.dll
CALL :collect wmvcore.dll
CALL :collect xmllite.dll
CALL :collect zipfldr.dll
CALL :collect l_intl.nls
CALL :collect advapi32.dll
CALL :collect bcrypt.dll
CALL :collect bcryptprimitives.dll
CALL :collect cabinet.dll
CALL :collect cfgmgr32.dll
CALL :collect ci.dll
CALL :collect coloradapterclient.dll
CALL :collect combase.dll
CALL :collect comctl32.dll
CALL :collect comdlg32.dll
CALL :collect crypt32.dll
CALL :collect cryptbase.dll
CALL :collect cryptsp.dll
CALL :collect d3d10.dll
CALL :collect d3d10core.dll
CALL :collect d3d11.dll
CALL :collect d3d12.dll
CALL :collect d3d9.dll
CALL :collect d3dcompiler_43.dll
CALL :collect d3dcompiler_47.dll
CALL :collect dbghelp.dll
CALL :collect dbgcore.dll
CALL :collect devobj.dll
CALL :collect diagnosticdatasettings.dll
CALL :collect dinput8.dll
CALL :collect dnsapi.dll
CALL :collect dsound.dll
CALL :collect dwmapi.dll
CALL :collect dxcore.dll
CALL :collect dxgi.dll
CALL :collect dxva2.dll
CALL :collect fwpuclnt.dll
CALL :collect gdi32.dll
CALL :collect gdi32full.dll
CALL :collect gdiplus.dll
CALL :collect glu32.dll
CALL :collect hal.dll
CALL :collect hid.dll
CALL :collect imm32.dll
CALL :collect imagehlp.dll
CALL :collect iphlpapi.dll
CALL :collect kdcom.dll
CALL :collect kernel.appcore.dll
CALL :collect kernel32.dll
CALL :collect kernelbase.dll
CALL :collect ktmw32.dll
CALL :collect mfplat.dll
CALL :collect mfreadwrite.dll
CALL :collect mobilenetworking.dll
CALL :collect mpr.dll
CALL :collect msacm32.dll
CALL :collect msasn1.dll
CALL :collect mscms.dll
CALL :collect mscoree.dll
CALL :collect msdmo.dll
CALL :collect msvcp140.dll
CALL :collect msvcp140d.dll
CALL :collect msvcp60.dll
CALL :collect msvcp_win.dll
CALL :collect msvcr120_clr0400.dll
CALL :collect msvcrt.dll
CALL :collect mswsock.dll
CALL :collect ncrypt.dll
CALL :collect netapi32.dll
CALL :collect netmsg.dll
CALL :collect netutils.dll
CALL :collect nlsbres.dll
CALL :collect normaliz.dll
CALL :collect nsi.dll
CALL :collect ntasn1.dll
CALL :collect ntdll.dll
CALL :collect ole32.dll
CALL :collect oleaut32.dll
CALL :collect opengl32.dll
CALL :collect pdh.dll
CALL :collect powrprof.dll
CALL :collect profapi.dll
CALL :collect propsys.dll
CALL :collect psapi.dll
CALL :collect rasadhlp.dll
CALL :collect resampledmo.dll
CALL :collect rpcrt4.dll
CALL :collect rstrtmgr.dll
CALL :collect rsaenh.dll
CALL :collect sechost.dll
CALL :collect setupapi.dll
CALL :collect shcore.dll
CALL :collect shell32.dll
CALL :collect shlwapi.dll
CALL :collect slwga.dll
CALL :collect sppc.dll
CALL :collect srvcli.dll
CALL :collect sspicli.dll
CALL :collect ucrtbase.dll
CALL :collect ucrtbased.dll
CALL :collect uiautomationcore.dll
CALL :collect umpdc.dll
CALL :collect urlmon.dll
CALL :collect user32.dll
CALL :collect userenv.dll
CALL :collect usp10.dll
CALL :collect uxtheme.dll
CALL :collect vcruntime140.dll
CALL :collect vcruntime140_1.dll
CALL :collect vcruntime140_1d.dll
CALL :collect vcruntime140d.dll
CALL :collect version.dll
CALL :collect wer.dll
CALL :collect win32u.dll
CALL :collect windows.storage.dll
CALL :collect windowscodecs.dll
CALL :collect winhttp.dll
CALL :collect wininet.dll
CALL :collect winmm.dll
CALL :collect winmmbase.dll
CALL :collect winnlsres.dll
CALL :collect wintrust.dll
CALL :collect wintypes.dll
CALL :collect wlanapi.dll
CALL :collect wldap32.dll
CALL :collect wow64.dll
CALL :collect wow64base.dll
CALL :collect wow64con.dll
CALL :collect wow64cpu.dll
CALL :collect wow64win.dll
CALL :collect ws2_32.dll
CALL :collect wsock32.dll
CALL :collect wtsapi32.dll
CALL :collect x3daudio1_7.dll
CALL :collect xapofx1_5.dll
CALL :collect xinput1_3.dll
CALL :collect xinput1_4.dll
CALL :collect xinput9_1_0.dll

CALL :collect locale.nls
CALL :collect c_1252.nls
CALL :collect c_437.nls
CALL :collect c_850.nls

CALL :collect_file "%WINDIR%\Cursors", aero_arrow.cur, %EMU_CURSORDIR%

REM sogen-debug-loop harvest: the 64-bit counterpart of every SysWOW64 DLL, in a separate folder (not part of the root),
REM so missing 64-bit system DLLs can be staged without another CI round.
IF NOT EXIST "%EMU_ROOT%\sys64all" MKDIR "%EMU_ROOT%\sys64all"
FOR %%F IN ("%WINDIR%\SysWOW64\*.dll") DO IF EXIST "%WINDIR%\System32\%%~nxF" COPY /B /Y "%WINDIR%\System32\%%~nxF" "%EMU_ROOT%\sys64all\%%~nxF" >NUL
REM sogen-debug-loop harvest: the 64-bit-only System32 DLLs (no SysWOW64 twin, so sys64all misses them: service DLLs
REM such as nsisvc.dll, and winnsi/tokenbinding-style helpers), in a separate folder (not part of the root).
IF NOT EXIST "%EMU_ROOT%\sys64rest" MKDIR "%EMU_ROOT%\sys64rest"
FOR %%F IN ("%WINDIR%\System32\*.dll") DO IF NOT EXIST "%WINDIR%\SysWOW64\%%~nxF" COPY /B /Y "%%F" "%EMU_ROOT%\sys64rest\%%~nxF" >NUL
REM sogen-debug-loop probe (harvest only): the network store (NSI) requests iphlpapi and cryptnet make on this Windows
REM build and the records they get back, so the emulator can model a network adapter (src\tools\nsiprobe\nsiprobe.c).
IF NOT EXIST "%EMU_ROOT%\probe" MKDIR "%EMU_ROOT%\probe"
"%~dp0nsiprobe\nsiprobe.exe" "%EMU_ROOT%\probe\nsiprobe.txt" > "%EMU_ROOT%\probe\nsiprobe-console.txt" 2>&1
REM sogen-debug-loop harvest: English MUI files for System32 and for comctl32 v6 (its resources assembly in WinSxS).
IF NOT EXIST "%EMU_ROOT%\sys64all\en-US" MKDIR "%EMU_ROOT%\sys64all\en-US"
COPY /B /Y "%WINDIR%\System32\en-US\*.mui" "%EMU_ROOT%\sys64all\en-US\" >NUL
IF NOT EXIST "%EMU_ROOT%\sys64all\comctl32v6-en-US" MKDIR "%EMU_ROOT%\sys64all\comctl32v6-en-US"
FOR /D %%D IN ("%WINDIR%\WinSxS\amd64_*controls.resources_6595b64144ccf1df_6.0.*_en-us_*") DO COPY /B /Y "%%D\*.mui" "%EMU_ROOT%\sys64all\comctl32v6-en-US\" >NUL
DIR /B /S "%WINDIR%\WinSxS\amd64_*controls.resources_6595b64144ccf1df_6.0.*_en-us_*" > "%EMU_ROOT%\sys64all\comctl32v6-en-US\sources.txt" 2>&1
REM sogen-debug-loop harvest: C:\Windows\SystemResources (*.mun: the resource sections of system DLLs since
REM Windows 10 1903; propsys.dll loads its schema resources from PROPSYS.dll.mun, missing in every root so far).
XCOPY "%WINDIR%\SystemResources" "%EMU_WINDIR%\SystemResources\" /E /I /Q /H /Y >NUL
REM sogen-debug-loop harvest: the WMI folders of this Windows build (System32\wbem and SysWOW64\wbem with their
REM MUIs and the repository), in a separate folder (not part of the root). The roots so far carry 26100 WMI binaries
REM on a 20348 ntdll (wmiprvsd.dll imports RtlGetImageFileMachines, which 20348 lacks). Winmgmt is stopped while the
REM repository is copied, then started again.
IF NOT EXIST "%EMU_ROOT%\wbem20348" MKDIR "%EMU_ROOT%\wbem20348"
NET STOP winmgmt /y > "%EMU_ROOT%\wbem20348\net-stop.txt" 2>&1
XCOPY "%WINDIR%\System32\wbem" "%EMU_ROOT%\wbem20348\System32\wbem\" /E /I /Q /H /Y /C > "%EMU_ROOT%\wbem20348\xcopy-sys32.txt" 2>&1
XCOPY "%WINDIR%\SysWOW64\wbem" "%EMU_ROOT%\wbem20348\SysWOW64\wbem\" /E /I /Q /H /Y /C > "%EMU_ROOT%\wbem20348\xcopy-wow64.txt" 2>&1
NET START winmgmt >> "%EMU_ROOT%\wbem20348\net-stop.txt" 2>&1
VER > "%EMU_ROOT%\wbem20348\ver.txt"
REM sogen-debug-loop harvest: every SysWOW64 DLL of this Windows build with its en-US MUIs and a version list, in a
REM separate folder (not part of the root). The roots so far carry 26100 x86 TLS DLLs (ncryptsslp, ncryptprov, scksp,
REM webio, iertutil, schannel) on a 20348 ncrypt/bcrypt: x86 ncryptsslp imports NCryptDecapsulate, which 20348 lacks.
IF NOT EXIST "%EMU_ROOT%\wow64all\en-US" MKDIR "%EMU_ROOT%\wow64all\en-US"
COPY /B /Y "%WINDIR%\SysWOW64\*.dll" "%EMU_ROOT%\wow64all\" >NUL
COPY /B /Y "%WINDIR%\SysWOW64\en-US\*.mui" "%EMU_ROOT%\wow64all\en-US\" >NUL
powershell -NoProfile -Command "Get-ChildItem -File (Join-Path $env:WINDIR 'SysWOW64\*.dll'),(Join-Path $env:WINDIR 'SysWOW64\en-US\*.mui') | ForEach-Object { $v = $_.VersionInfo; '{0},{1}.{2}.{3}.{4},{5}' -f $_.FullName.Substring($env:WINDIR.Length + 10), $v.FileMajorPart, $v.FileMinorPart, $v.FileBuildPart, $v.FilePrivatePart, $_.Length }" > "%EMU_ROOT%\wow64all\versions.csv" 2>&1
VER > "%EMU_ROOT%\wow64all\ver.txt"
REM sogen-debug-loop harvest: the 64-bit .NET Framework 4 native image cache with the IL assemblies and runtime it was
REM generated from (a native image is only used with the exact IL it was compiled from), plus their versions.
IF NOT EXIST "%EMU_ROOT%\net4x64" MKDIR "%EMU_ROOT%\net4x64"
XCOPY "%WINDIR%\assembly\NativeImages_v4.0.30319_64" "%EMU_ROOT%\net4x64\NativeImages_v4.0.30319_64\" /E /I /Q /H /Y >NUL
XCOPY "%WINDIR%\Microsoft.NET\assembly\GAC_64" "%EMU_ROOT%\net4x64\GAC_64\" /E /I /Q /H /Y >NUL
XCOPY "%WINDIR%\Microsoft.NET\assembly\GAC_MSIL" "%EMU_ROOT%\net4x64\GAC_MSIL\" /E /I /Q /H /Y >NUL
XCOPY "%WINDIR%\Microsoft.NET\Framework64\v4.0.30319" "%EMU_ROOT%\net4x64\Framework64\" /E /I /Q /H /Y >NUL
powershell -NoProfile -Command "foreach ($f in 'Microsoft.NET\Framework64\v4.0.30319\mscorlib.dll','Microsoft.NET\Framework64\v4.0.30319\clr.dll','Microsoft.NET\Framework64\v4.0.30319\ngen.exe') { $p = Join-Path $env:WINDIR $f; '{0} {1}' -f $f, (Get-Item $p).VersionInfo.FileVersion }" > "%EMU_ROOT%\net4x64\versions.txt" 2>&1
DIR /S "%WINDIR%\assembly\NativeImages_v4.0.30319_64\mscorlib" >> "%EMU_ROOT%\net4x64\versions.txt" 2>&1

EXIT /B 0

:normpath
SET %1=%~dpfn2
EXIT /B

:collect_file
CALL :normpath SRC, %~1\%~2
CALL :normpath DST, %~3\%~2

IF EXIST %SRC% (
	ECHO %SRC% -^> %DST%
	COPY /B /Y "%SRC%" "%DST%" >NUL
)
EXIT /B

:collect
CALL :collect_file %SYSDIR%, %~1, %EMU_SYSDIR%
CALL :collect_file %SYSDIR_WOW64%, %~1, %EMU_SYSDIR_WOW64%
EXIT /B

