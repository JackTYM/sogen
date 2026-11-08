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

CALL :collect advapi32.dll
CALL :collect apisetschema.dll
CALL :collect apphelp.dll
CALL :collect appresolver.dll
CALL :collect appxalluserstore.dll
CALL :collect appxapplicabilityengine.dll
CALL :collect appxdeploymentclient.dll
CALL :collect attrib.exe
CALL :collect audioeng.dll
CALL :collect audiokse.dll
CALL :collect audioses.dll
CALL :collect authz.dll
CALL :collect avifil32.dll
CALL :collect avrt.dll
CALL :collect bcp47langs.dll
CALL :collect bcrypt.dll
CALL :collect bcryptprimitives.dll
CALL :collect bthprops.cpl
CALL :collect cabinet.dll
CALL :collect certenroll.dll
CALL :collect cfgmgr32.dll
CALL :collect ci.dll
CALL :collect clbcatq.dll
CALL :collect cldapi.dll
CALL :collect cmd.exe
CALL :collect coloradapterclient.dll
CALL :collect combase.dll
CALL :collect comctl32.dll
CALL :collect comdlg32.dll
CALL :collect coml2.dll
CALL :collect coremessaging.dll
CALL :collect coreuicomponents.dll
CALL :collect credui.dll
CALL :collect crypt32.dll
CALL :collect cryptbase.dll
CALL :collect cryptdll.dll
CALL :collect cryptsp.dll
CALL :collect CRYPTUI.dll
CALL :collect cscript.exe
CALL :collect d2d1.dll
CALL :collect d3d10.dll
CALL :collect d3d10core.dll
CALL :collect d3d10warp.dll
CALL :collect d3d11.dll
CALL :collect d3d12.dll
CALL :collect d3d9.dll
CALL :collect d3dcompiler_43.dll
CALL :collect d3dcompiler_47.dll
CALL :collect dabapi.dll
CALL :collect dbghelp.dll
CALL :collect dbgcore.dll
CALL :collect dciman32.dll
CALL :collect dcomp.dll
CALL :collect ddraw.dll
CALL :collect devobj.dll
CALL :collect devrtl.dll
CALL :collect dhcpcsvc.DLL
CALL :collect dhcpcsvc6.dll
CALL :collect diagnosticdatasettings.dll
CALL :collect dinput8.dll
CALL :collect dnsapi.dll
CALL :collect dpapi.dll
CALL :collect dpapisrv.dll
CALL :collect drvstore.dll
CALL :collect dsound.dll
CALL :collect dsparse.dll
CALL :collect dsrole.dll
CALL :collect dwmapi.dll
CALL :collect dwrite.dll
CALL :collect dxcore.dll
CALL :collect dxgi.dll
CALL :collect dxva2.dll
CALL :collect efscore.dll
CALL :collect efslsaext.dll
CALL :collect ESENT.dll
CALL :collect EventAggregation.dll
CALL :collect faultrep.dll
CALL :collect firewallapi.dll
CALL :collect fsrvcli.dll
CALL :collect fveapi.dll
CALL :collect fwpuclnt.dll
CALL :collect gdi32.dll
CALL :collect gdi32full.dll
CALL :collect gdiplus.dll
CALL :collect glu32.dll
CALL :collect gpapi.dll
CALL :collect hal.dll
CALL :collect hid.dll
CALL :collect icacls.exe
CALL :collect icu.dll
CALL :collect imageres.dll
CALL :collect imm32.dll
CALL :collect imagehlp.dll
CALL :collect iertutil.dll
CALL :collect inputhost.dll
CALL :collect iphlpapi.dll
CALL :collect joinutil.dll
CALL :collect kdcom.dll
CALL :collect kdscli.dll
CALL :collect Kerb3961.dll
CALL :collect KerbClientShared.dll
CALL :collect kerberos.dll
CALL :collect kernel.appcore.dll
CALL :collect kernel32.dll
CALL :collect kernelbase.dll
CALL :collect keyiso.dll
CALL :collect ktmw32.dll
CALL :collect l_intl.nls
CALL :collect logoncli.dll
CALL :collect lsaadt.dll
CALL :collect lsasrv.dll
CALL :collect lsass.exe
CALL :collect lsm.dll
CALL :collect mdmregistration.dll
CALL :collect MF.dll
CALL :collect mfc140u.dll
CALL :collect mfperfhelper.dll
CALL :collect mfplat.dll
CALL :collect mfreadwrite.dll
CALL :collect MFSENSORGROUP.dll
CALL :collect microsoft.internal.warppal.dll
CALL :collect mmdevapi.dll
CALL :collect mobilenetworking.dll
CALL :collect mpr.dll
CALL :collect msacm32.dll
CALL :collect msasn1.dll
CALL :collect mscms.dll
CALL :collect mscoree.dll
CALL :collect msctf.dll
CALL :collect msi.dll
CALL :collect msiexec.exe
CALL :collect msimg32.dll
CALL :collect msdmo.dll
CALL :collect mshta.exe
CALL :collect msimsg.dll
CALL :collect mskeyprotect.dll
CALL :collect msprivs.dll
CALL :collect msv1_0.dll
CALL :collect msvcp120_clr0400.dll
CALL :collect msvcp140.dll
CALL :collect msvcp140_1.dll
CALL :collect msvcp140_2.dll
CALL :collect msvcp140_atomic_wait.dll
CALL :collect msvcp140_clr0400.dll
CALL :collect msvcp140d.dll
CALL :collect msvcp60.dll
CALL :collect msvcp_win.dll
CALL :collect msvcr100_clr0400.dll
CALL :collect msvcr120_clr0400.dll
CALL :collect msvcrt.dll
CALL :collect msvfw32.dll
CALL :collect mswsock.dll
CALL :collect msxml6.dll
CALL :collect msxml6r.dll
CALL :collect napinsp.dll
CALL :collect ncrypt.dll
CALL :collect NDFAPI.DLL
CALL :collect net.exe
CALL :collect netapi32.dll
CALL :collect netjoin.dll
CALL :collect netmsg.dll
CALL :collect netprofm.dll
CALL :collect netprovfw.dll
CALL :collect netsh.exe
CALL :collect netutils.dll
CALL :collect ngcrecovery.dll
CALL :collect nlsbres.dll
CALL :collect normaliz.dll
CALL :collect npmproxy.dll
CALL :collect nrtapi.dll
CALL :collect nsi.dll
CALL :collect ntasn1.dll
CALL :collect ntdll.dll
CALL :collect ntdsapi.dll
CALL :collect ntlmshared.dll
CALL :collect ntmarta.dll
CALL :collect ntoskrnl.exe
CALL :collect odbc32.dll
CALL :collect ole32.dll
CALL :collect oleacc.dll
CALL :collect oleaccrc.dll
CALL :collect oleaut32.dll
CALL :collect oledlg.dll
CALL :collect opengl32.dll
CALL :collect pdh.dll
CALL :collect popkeycli.dll
CALL :collect powrprof.dll
CALL :collect profapi.dll
CALL :collect propsys.dll
CALL :collect psapi.dll
CALL :collect psmsrv.dll
CALL :collect rasadhlp.dll
CALL :collect reg.exe
CALL :collect regsvr32.exe
CALL :collect resampledmo.dll
CALL :collect resourcepolicyclient.dll
CALL :collect rmclient.dll
CALL :collect rpcepmap.dll
CALL :collect rpcns4.dll
CALL :collect rpcrt4.dll
CALL :collect rpcss.dll
CALL :collect rstrtmgr.dll
CALL :collect rsaenh.dll
CALL :collect rtworkq.dll
CALL :collect rundll32.exe
CALL :collect samcli.dll
CALL :collect samlib.dll
CALL :collect samsrv.dll
CALL :collect sc.exe
CALL :collect scesrv.dll
CALL :collect sechost.dll
CALL :collect secur32.dll
CALL :collect services.exe
CALL :collect servicingcommon.dll
CALL :collect setupapi.dll
CALL :collect setupcl.dll
CALL :collect shcore.dll
CALL :collect shell32.dll
CALL :collect shlwapi.dll
CALL :collect slwga.dll
CALL :collect spfileq.dll
CALL :collect spinf.dll
CALL :collect sppc.dll
CALL :collect srvcli.dll
CALL :collect sspicli.dll
CALL :collect sspisrv.dll
CALL :collect staterepository.core.dll
CALL :collect svchost.exe
CALL :collect sysntfy.dll
CALL :collect SystemEventsBrokerServer.dll
CALL :collect tbs.dll
CALL :collect tdh.dll
CALL :collect textinputframework.dll
CALL :collect textshaping.dll
CALL :collect tpmcoreprovisioning.dll
CALL :collect twinapi.appcore.dll
CALL :collect ucrtbase.dll
CALL :collect ucrtbase_clr0400.dll
CALL :collect ucrtbased.dll
CALL :collect uiautomationcore.dll
CALL :collect ulib.dll
CALL :collect umpdc.dll
CALL :collect umpnpmgr.dll
CALL :collect umpo.dll
CALL :collect urlmon.dll
CALL :collect user32.dll
CALL :collect userenv.dll
CALL :collect usp10.dll
CALL :collect uxtheme.dll
CALL :collect vbsapi.dll
CALL :collect vcruntime140.dll
CALL :collect vcruntime140_1.dll
CALL :collect vcruntime140_1_clr0400.dll
CALL :collect vcruntime140_1d.dll
CALL :collect vcruntime140_clr0400.dll
CALL :collect vcruntime140d.dll
CALL :collect version.dll
CALL :collect virtdisk.dll
CALL :collect wbemcomn.dll
CALL :collect wdmaud.drv
CALL :collect webauthn.dll
CALL :collect webio.dll
CALL :collect websocket.dll
CALL :collect wer.dll
CALL :collect wevtapi.dll
CALL :collect win32u.dll
CALL :collect winbrand.dll
CALL :collect windows.internal.graphics.display.displaycolormanagement.dll
CALL :collect windows.storage.dll
CALL :collect windows.ui.dll
CALL :collect windows.ui.immersive.dll
CALL :collect windowscodecs.dll
CALL :collect winhttp.dll
CALL :collect wininet.dll
CALL :collect winmm.dll
CALL :collect winmmbase.dll
CALL :collect winnlsres.dll
CALL :collect winnsi.dll
CALL :collect winspool.drv
CALL :collect winsta.dll
CALL :collect wintrust.dll
CALL :collect wintypes.dll
CALL :collect WINUSB.DLL
CALL :collect wkscli.dll
CALL :collect wlanapi.dll
CALL :collect wldap32.dll
CALL :collect wldp.dll
CALL :collect wmasf.dll
CALL :collect wmsgapi.dll
CALL :collect wmvcore.dll
CALL :collect wow64.dll
CALL :collect wow64base.dll
CALL :collect wow64con.dll
CALL :collect wow64cpu.dll
CALL :collect wow64win.dll
CALL :collect ws2_32.dll
CALL :collect wscript.exe
CALL :collect wshbth.dll
CALL :collect wsock32.dll
CALL :collect wtdccm.dll
CALL :collect wtsapi32.dll
CALL :collect wuceffects.dll
CALL :collect WUDFPlatform.dll
CALL :collect x3daudio1_7.dll
CALL :collect xapofx1_5.dll
CALL :collect xinput1_3.dll
CALL :collect xinput1_4.dll
CALL :collect xinput9_1_0.dll
CALL :collect xmllite.dll

CALL :collect locale.nls
CALL :collect c_1252.nls
CALL :collect c_437.nls
CALL :collect c_850.nls
CALL :collect schannel.dll
CALL :collect wdigest.dll
CALL :collect pku2u.dll
CALL :collect TSpkg.dll
CALL :collect negoexts.dll
CALL :collect cloudAP.dll
CALL :collect cryptngc.dll
CALL :collect dsreg.dll
CALL :collect kdcpw.dll
CALL :collect netlogon.dll
CALL :collect gmsaclient.dll
CALL :collect scecli.dll
CALL :collect credssp.dll
CALL :collect ncryptprov.dll
CALL :collect ncryptsslp.dll

CALL :collect_file "%WINDIR%\Cursors", aero_arrow.cur, %EMU_CURSORDIR%
CALL :collect_dir "%WINDIR%\System32\wbem", %EMU_SYSDIR%\wbem
CALL :collect_dir "%WINDIR%\SysWOW64\wbem", %EMU_SYSDIR_WOW64%\wbem
CALL :collect_dir "%WINDIR%\System32\en-US", %EMU_SYSDIR%\en-US
CALL :collect_dir "%WINDIR%\SysWOW64\en-US", %EMU_SYSDIR_WOW64%\en-US
CALL :collect_dir "%WINDIR%\Microsoft.NET", %EMU_WINDIR%\Microsoft.NET
powershell -NoProfile -ExecutionPolicy Bypass -Command "foreach ($d in 'WinSxS','assembly','Microsoft.NET','SysWOW64','System32') { $s = (Get-ChildItem -LiteralPath (Join-Path $env:WINDIR $d) -Recurse -Force -File -ErrorAction SilentlyContinue | Measure-Object Length -Sum).Sum; Write-Host ('SIZE ' + $d + ' ' + [math]::Round($s / 1MB) + ' MB') }"

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

:collect_dir
IF EXIST "%~1" (
	ECHO %~1 -^> %~2
	robocopy "%~1" "%~2" /E /NFL /NDL /NJH /NJS /NP >NUL
)
EXIT /B 0
