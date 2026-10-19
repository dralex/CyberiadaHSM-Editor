; -----------------------------------------------------------------------------
; CyberiadaPro, the Cyberiada State Machine Editor
;
; The Windows installer: the editor, Python 3 when no suitable one is found, the
; python plugins (hsm-to-python, hsm-maker) on the pure-Python cyberiadaml-py,
; and a check that the editor runs them. Everything is in the installer, nothing
; is downloaded. Built on Linux by packaging/build-windows-installer.sh:
;
;   makensis -DVERSION=1.0.0 -DSTAGE=<the unpacked editor zip> -DWHEELS=<wheels>
;            -DTOOLS=<check-plugins.py + samples/> -DPYTHON_INSTALLER=<python.org exe>
;            -DPYTHON_VERSION=3.14.8 -DICON=<cyberiada-pro.ico> -DOUTFILE=<setup.exe>
;            cyberiada-pro.nsi
;
; Copyright (C) 2026 Alexey Fedoseev <aleksey@fedoseev.net>
;
; This program is free software; you can redistribute it and/or modify it under
; the terms of the GNU Lesser General Public License, version 3 or later.
; -----------------------------------------------------------------------------

Unicode true
; a 64-bit installer: Program Files and the registry without the WOW64 redirection
Target amd64-unicode
ManifestDPIAware true
SetCompressor /SOLID lzma
RequestExecutionLevel admin

!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "nsDialogs.nsh"
!include "Sections.nsh"
!include "FileFunc.nsh"
!include "WinMessages.nsh"

!ifndef VERSION | STAGE | WHEELS | TOOLS | PYTHON_INSTALLER | PYTHON_VERSION | ICON | OUTFILE
  !error "define VERSION, STAGE, WHEELS, TOOLS, PYTHON_INSTALLER, PYTHON_VERSION, ICON, OUTFILE"
!endif

!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\CyberiadaPro"
!define PLUGINS_DIR "share\cyberiada-pro\plugins"
!define PROGID "CyberiadaPro.graphml"
; what pip installs (the dispatcher, cyberiadaml-py and pydantic come with them)
!define PIP_PACKAGES "hsm-to-python cybersphere-hsm-maker"
; what the uninstaller removes (pydantic may serve other programs: it stays)
!define OUR_PACKAGES "hsm-to-python cybersphere-hsm-maker hsm-dispatcher-py cyberiadaml-py"

Name "CyberiadaPro ${VERSION}"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\CyberiadaPro"
InstallDirRegKey HKLM "${UNINST_KEY}" "InstallLocation"
BrandingText "CyberiadaPro ${VERSION}"

Var PythonExe          ; the python.exe of the plugins, empty when none is suitable
Var PythonVersion
Var Problems           ; what went wrong during the install, for the check page
Var CheckCode          ; the exit code of check-plugins.py, empty when it did not run
Var LangCode           ; en or ru, for check-plugins.py

; --- pages --------------------------------------------------------------------

!define MUI_ICON "${ICON}"
!define MUI_UNICON "${ICON}"
!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${STAGE}/LICENSE"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
Page custom PythonPageShow
!insertmacro MUI_PAGE_INSTFILES
Page custom CheckPageShow
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Russian"
!insertmacro MUI_RESERVEFILE_LANGDLL

VIProductVersion "${VERSION}.0"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "CyberiadaPro"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "CyberiadaPro ${VERSION} setup"
VIAddVersionKey /LANG=${LANG_ENGLISH} "CompanyName" "Kruzhok Association"
VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" "Copyright (C) 2024-2026 Alexey Fedoseev"

; --- texts --------------------------------------------------------------------

LangString SecEditorName ${LANG_ENGLISH} "CyberiadaPro"
LangString SecEditorName ${LANG_RUSSIAN} "CyberiadaPro"
LangString SecEditorDesc ${LANG_ENGLISH} "The state machine editor."
LangString SecEditorDesc ${LANG_RUSSIAN} "Редактор машин состояний."
LangString SecToolsName ${LANG_ENGLISH} "Python plugins"
LangString SecToolsName ${LANG_RUSSIAN} "Модули на Python"
LangString SecToolsDesc ${LANG_ENGLISH} "hsm-to-python (the Python program of a diagram) and hsm-maker (diagrams drawn from a description); installs Python ${PYTHON_VERSION} when no Python 3.10-3.14 (64-bit) is found."
LangString SecToolsDesc ${LANG_RUSSIAN} "hsm-to-python (программа на Python по диаграмме) и hsm-maker (диаграммы по описанию); устанавливает Python ${PYTHON_VERSION}, если не найден Python 3.10-3.14 (64-бит)."
LangString SecAssocName ${LANG_ENGLISH} "Open .graphml files with CyberiadaPro"
LangString SecAssocName ${LANG_RUSSIAN} "Открывать файлы .graphml в CyberiadaPro"
LangString SecAssocDesc ${LANG_ENGLISH} "Makes CyberiadaPro the program of the .graphml files."
LangString SecAssocDesc ${LANG_RUSSIAN} "Назначает CyberiadaPro программой для файлов .graphml."

LangString PythonTitle ${LANG_ENGLISH} "Python"
LangString PythonTitle ${LANG_RUSSIAN} "Python"
LangString PythonSubtitle ${LANG_ENGLISH} "The Python the plugins run with"
LangString PythonSubtitle ${LANG_RUSSIAN} "Python, на котором работают модули"
LangString PythonFound ${LANG_ENGLISH} "The plugins will use Python $PythonVersion:$\r$\n$\r$\n$PythonExe$\r$\n$\r$\nTheir packages are installed into it."
LangString PythonFound ${LANG_RUSSIAN} "Модули будут использовать Python $PythonVersion:$\r$\n$\r$\n$PythonExe$\r$\n$\r$\nИх пакеты будут установлены в него."
LangString PythonInstall ${LANG_ENGLISH} "No Python 3.10-3.14 (64-bit) was found. Python ${PYTHON_VERSION} will be installed for all users, and the plugins will use it."
LangString PythonInstall ${LANG_RUSSIAN} "Python 3.10-3.14 (64-бит) не найден. Будет установлен Python ${PYTHON_VERSION} для всех пользователей, модули будут использовать его."

LangString InstallingPython ${LANG_ENGLISH} "Installing Python ${PYTHON_VERSION}..."
LangString InstallingPython ${LANG_RUSSIAN} "Установка Python ${PYTHON_VERSION}..."
LangString InstallingPackages ${LANG_ENGLISH} "Installing the python packages of the plugins..."
LangString InstallingPackages ${LANG_RUSSIAN} "Установка пакетов Python для модулей..."
LangString PinningPlugins ${LANG_ENGLISH} "Binding the plugins to the Python..."
LangString PinningPlugins ${LANG_RUSSIAN} "Привязка модулей к Python..."
LangString Checking ${LANG_ENGLISH} "Checking the plugins..."
LangString Checking ${LANG_RUSSIAN} "Проверка модулей..."

LangString PythonFailed ${LANG_ENGLISH} "Python ${PYTHON_VERSION} could not be installed (exit $0; see logs\python-install.log). Install Python 3.10-3.14 (64-bit) from python.org, then run this setup again."
LangString PythonFailed ${LANG_RUSSIAN} "Не удалось установить Python ${PYTHON_VERSION} (код $0; см. logs\python-install.log). Установите Python 3.10-3.14 (64-бит) с python.org и запустите эту установку ещё раз."
LangString PipMissing ${LANG_ENGLISH} "pip is missing from $PythonExe and could not be added (ensurepip, exit $0). Repair that Python installation, then run this setup again."
LangString PipMissing ${LANG_RUSSIAN} "В $PythonExe нет pip, и его не удалось добавить (ensurepip, код $0). Восстановите эту установку Python и запустите эту установку ещё раз."
LangString PipFailed ${LANG_ENGLISH} "The python packages could not be installed into $PythonExe (exit $0; see logs\pip.log)."
LangString PipFailed ${LANG_RUSSIAN} "Не удалось установить пакеты Python в $PythonExe (код $0; см. logs\pip.log)."
LangString PinFailed ${LANG_ENGLISH} "The plugin $1 could not be bound to $PythonExe (exit $0)."
LangString PinFailed ${LANG_RUSSIAN} "Не удалось привязать модуль $1 к $PythonExe (код $0)."

LangString CheckTitle ${LANG_ENGLISH} "Plugin check"
LangString CheckTitle ${LANG_RUSSIAN} "Проверка модулей"
LangString CheckPassed ${LANG_ENGLISH} "The editor runs the plugins."
LangString CheckPassed ${LANG_RUSSIAN} "Редактор запускает модули."
LangString CheckFailed ${LANG_ENGLISH} "Some plugins do not work yet: see what to check below."
LangString CheckFailed ${LANG_RUSSIAN} "Некоторые модули пока не работают: что проверить, написано ниже."
LangString CheckLog ${LANG_ENGLISH} "The full log: $INSTDIR\logs\check.log. The check runs again from the Start menu: CyberiadaPro, Check plugins."
LangString CheckLog ${LANG_RUSSIAN} "Полный журнал: $INSTDIR\logs\check.log. Проверку можно повторить из меню Пуск: CyberiadaPro, Проверка модулей."
LangString CheckShortcut ${LANG_ENGLISH} "Check plugins"
LangString CheckShortcut ${LANG_RUSSIAN} "Проверка модулей"

LangString PythonKept ${LANG_ENGLISH} "Python stays installed; remove it in Settings, Apps if no other program needs it."
LangString PythonKept ${LANG_RUSSIAN} "Python остаётся установленным; удалите его в Параметрах, Приложения, если он не нужен другим программам."

; --- sections -----------------------------------------------------------------

Section "$(SecEditorName)" SecEditor
  SectionIn RO
  SetOutPath "$INSTDIR"
  File /r "${STAGE}/*"
  File "/oname=cyberiada-pro.ico" "${ICON}"
  CreateDirectory "$INSTDIR\logs"
  CreateDirectory "$SMPROGRAMS\CyberiadaPro"
  CreateShortcut "$SMPROGRAMS\CyberiadaPro\CyberiadaPro.lnk" "$INSTDIR\CyberiadaPro.exe" "" \
      "$INSTDIR\cyberiada-pro.ico"
SectionEnd

Section "$(SecToolsName)" SecTools
  SetOutPath "$INSTDIR\python\wheels"
  File "${WHEELS}/*.whl"
  SetOutPath "$INSTDIR\tools"
  File "${TOOLS}/check-plugins.py"
  SetOutPath "$INSTDIR\tools\samples"
  File "${TOOLS}/samples/*.graphml"
  ; python -m puts the working folder first on sys.path: a folder of no modules
  SetOutPath "$PLUGINSDIR"

  ${If} $PythonExe == ""
    DetailPrint "$(InstallingPython)"
    File "/oname=python-${PYTHON_VERSION}-amd64.exe" "${PYTHON_INSTALLER}"
    nsExec::ExecToLog '"$PLUGINSDIR\python-${PYTHON_VERSION}-amd64.exe" /quiet InstallAllUsers=1 PrependPath=1 Include_launcher=1 Include_test=0 /log "$INSTDIR\logs\python-install.log"'
    Pop $0
    DetailPrint "python-${PYTHON_VERSION}-amd64.exe: $0"
    Call FindPython
    ${If} $PythonExe == ""
      StrCpy $Problems "$(PythonFailed)"
      Goto done
    ${EndIf}
  ${EndIf}
  DetailPrint "Python $PythonVersion: $PythonExe"

  nsExec::ExecToLog '"$PythonExe" -m pip --version'
  Pop $0
  ${If} $0 != 0
    nsExec::ExecToLog '"$PythonExe" -m ensurepip --default-pip'
    Pop $0
    ${If} $0 != 0
      StrCpy $Problems "$(PipMissing)"
      Goto done
    ${EndIf}
  ${EndIf}

  DetailPrint "$(InstallingPackages)"
  nsExec::ExecToLog '"$PythonExe" -m pip install --disable-pip-version-check --no-index --find-links "$INSTDIR\python\wheels" --upgrade --force-reinstall --log "$INSTDIR\logs\pip.log" ${PIP_PACKAGES}'
  Pop $0
  ${If} $0 != 0
    StrCpy $Problems "$(PipFailed)"
    Goto done
  ${EndIf}

  ; the plugins of the editor folder, bound to this Python for every user (each
  ; command imports the whole stack first: a broken install fails here)
  DetailPrint "$(PinningPlugins)"
  StrCpy $1 "hsm-to-python"
  nsExec::ExecToLog '"$PythonExe" -m hsm --install-editor-plugin "$INSTDIR\${PLUGINS_DIR}"'
  Pop $0
  ${If} $0 != 0
    StrCpy $Problems "$Problems$(PinFailed)$\r$\n"
  ${EndIf}
  StrCpy $1 "hsm-maker"
  nsExec::ExecToLog '"$PythonExe" -m hsm_maker --install-editor-plugin "$INSTDIR\${PLUGINS_DIR}"'
  Pop $0
  ${If} $0 != 0
    StrCpy $Problems "$Problems$(PinFailed)$\r$\n"
  ${EndIf}

  WriteRegStr HKLM "${UNINST_KEY}" "PythonExe" "$PythonExe"
  SetOutPath "$INSTDIR\tools"
  CreateShortcut "$SMPROGRAMS\CyberiadaPro\$(CheckShortcut).lnk" "$PythonExe" \
      '"$INSTDIR\tools\check-plugins.py" --editor "$INSTDIR\CyberiadaPro.exe" --language $LangCode --pause' \
      "$INSTDIR\cyberiada-pro.ico"

  DetailPrint "$(Checking)"
  SetOutPath "$PLUGINSDIR"
  nsExec::ExecToLog '"$PythonExe" "$INSTDIR\tools\check-plugins.py" --editor "$INSTDIR\CyberiadaPro.exe" --language $LangCode --log "$INSTDIR\logs\check.log" --summary "$PLUGINSDIR\check-summary.txt"'
  Pop $CheckCode
done:
SectionEnd

Section /o "$(SecAssocName)" SecAssoc
  WriteRegStr HKLM "Software\Classes\${PROGID}" "" "CyberiadaML diagram"
  WriteRegStr HKLM "Software\Classes\${PROGID}\DefaultIcon" "" "$INSTDIR\cyberiada-pro.ico"
  WriteRegStr HKLM "Software\Classes\${PROGID}\shell\open\command" "" '"$INSTDIR\CyberiadaPro.exe" "%1"'
  WriteRegStr HKLM "Software\Classes\.graphml" "" "${PROGID}"
  WriteRegStr HKLM "Software\Classes\.graphml\OpenWithProgids" "${PROGID}" ""
  ; SHCNE_ASSOCCHANGED: the shell picks the new association up
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
SectionEnd

Section "-Register"
  WriteUninstaller "$INSTDIR\uninstall.exe"
  CreateShortcut "$SMPROGRAMS\CyberiadaPro\Uninstall CyberiadaPro.lnk" "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayName" "CyberiadaPro ${VERSION}"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINST_KEY}" "Publisher" "Kruzhok Association"
  WriteRegStr HKLM "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\cyberiada-pro.ico"
  WriteRegStr HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINST_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKLM "${UNINST_KEY}" "EstimatedSize" "$0"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecEditor} "$(SecEditorDesc)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecTools} "$(SecToolsDesc)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecAssoc} "$(SecAssocDesc)"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; --- functions ----------------------------------------------------------------

Function .onInit
  !insertmacro MUI_LANGDLL_DISPLAY
  SetShellVarContext all
  InitPluginsDir
  ${If} $LANGUAGE == ${LANG_RUSSIAN}
    StrCpy $LangCode "ru"
  ${Else}
    StrCpy $LangCode "en"
  ${EndIf}
  Call FindPython
FunctionEnd

; CheckPython: the python.exe on the stack; $R1 = "ok" when it is a 64-bit
; CPython 3.10-3.14 (the bundled pydantic-core wheels) outside the Store
Function CheckPython
  Exch $R2
  nsExec::ExecToStack `"$R2" -c "import sys, struct; print('ok' if (3, 10) <= sys.version_info[:2] <= (3, 14) and struct.calcsize('P') == 8 and 'WindowsApps' not in sys.executable else 'no')"`
  Pop $R3
  Pop $R1
  StrCpy $R1 $R1 2
  ${If} $R3 != 0
    StrCpy $R1 ""
  ${EndIf}
  Pop $R2
FunctionEnd

; TryPython ROOT TAG: take the Python registered under that PEP 514 tag
!macro TryPython ROOT TAG
  ${If} $PythonExe == ""
    StrCpy $R0 ""
    ReadRegStr $R0 ${ROOT} "Software\Python\PythonCore\${TAG}\InstallPath" "ExecutablePath"
    ${If} $R0 == ""
      ReadRegStr $R0 ${ROOT} "Software\Python\PythonCore\${TAG}\InstallPath" ""
      ${If} $R0 != ""
        StrCpy $R0 "$R0\python.exe"
      ${EndIf}
    ${EndIf}
    ${If} $R0 != ""
    ${AndIf} ${FileExists} "$R0"
      GetFullPathName $R0 "$R0"
      Push $R0
      Call CheckPython
      ${If} $R1 == "ok"
        StrCpy $PythonExe $R0
      ${EndIf}
    ${EndIf}
  ${EndIf}
!macroend

; FindPython: $PythonExe and $PythonVersion of the newest suitable Python, for all
; users first (the plugins are bound to it for every user), else of this user
Function FindPython
  StrCpy $PythonExe ""
  StrCpy $PythonVersion ""
  !insertmacro TryPython HKLM "3.14"
  !insertmacro TryPython HKLM "3.13"
  !insertmacro TryPython HKLM "3.12"
  !insertmacro TryPython HKLM "3.11"
  !insertmacro TryPython HKLM "3.10"
  !insertmacro TryPython HKCU "3.14"
  !insertmacro TryPython HKCU "3.13"
  !insertmacro TryPython HKCU "3.12"
  !insertmacro TryPython HKCU "3.11"
  !insertmacro TryPython HKCU "3.10"
  ${If} $PythonExe != ""
    nsExec::ExecToStack '"$PythonExe" -c "import sys; print(sys.version.split()[0])"'
    Pop $R3
    Pop $PythonVersion
    ; the output ends with a line break
    StrCpy $PythonVersion $PythonVersion -2
  ${EndIf}
FunctionEnd

Function PythonPageShow
  ${IfNot} ${SectionIsSelected} ${SecTools}
    Abort
  ${EndIf}
  !insertmacro MUI_HEADER_TEXT "$(PythonTitle)" "$(PythonSubtitle)"
  nsDialogs::Create 1018
  Pop $0
  ${If} $PythonExe != ""
    ${NSD_CreateLabel} 0 0 100% 80u "$(PythonFound)"
  ${Else}
    ${NSD_CreateLabel} 0 0 100% 80u "$(PythonInstall)"
  ${EndIf}
  Pop $0
  nsDialogs::Show
FunctionEnd

Function CheckPageShow
  ${IfNot} ${SectionIsSelected} ${SecTools}
    Abort
  ${EndIf}
  ${If} $CheckCode == "0"
  ${AndIf} $Problems == ""
    !insertmacro MUI_HEADER_TEXT "$(CheckTitle)" "$(CheckPassed)"
  ${Else}
    !insertmacro MUI_HEADER_TEXT "$(CheckTitle)" "$(CheckFailed)"
  ${EndIf}
  nsDialogs::Create 1018
  Pop $0
  ${NSD_CreateMLText} 0 0 100% 110u ""
  Pop $1
  SendMessage $1 ${EM_SETREADONLY} 1 0
  ${If} $Problems != ""
    SendMessage $1 ${EM_REPLACESEL} 0 "STR:$Problems$\r$\n"
  ${EndIf}
  ; the lines of check-plugins.py (UTF-16, so the Russian texts read as they are)
  ClearErrors
  FileOpen $2 "$PLUGINSDIR\check-summary.txt" r
  ${IfNot} ${Errors}
    ${Do}
      ClearErrors
      FileReadUTF16LE $2 $3
      ${IfThen} ${Errors} ${|} ${ExitDo} ${|}
      SendMessage $1 ${EM_REPLACESEL} 0 "STR:$3"
    ${Loop}
    FileClose $2
  ${EndIf}
  ${NSD_CreateLabel} 0 114u 100% 26u "$(CheckLog)"
  Pop $0
  nsDialogs::Show
FunctionEnd

; --- uninstaller --------------------------------------------------------------

Function un.onInit
  !insertmacro MUI_UNGETLANGUAGE
  SetShellVarContext all
FunctionEnd

Section "Uninstall"
  ; our packages out of the Python of the plugins (pydantic and Python stay)
  ReadRegStr $0 HKLM "${UNINST_KEY}" "PythonExe"
  ${If} $0 != ""
  ${AndIf} ${FileExists} "$0"
    SetOutPath "$TEMP"
    nsExec::ExecToLog '"$0" -m pip uninstall --yes --disable-pip-version-check ${OUR_PACKAGES}'
    Pop $1
  ${EndIf}

  ; the association, when it is ours
  ReadRegStr $1 HKLM "Software\Classes\.graphml" ""
  ${If} $1 == "${PROGID}"
    DeleteRegValue HKLM "Software\Classes\.graphml" ""
  ${EndIf}
  DeleteRegValue HKLM "Software\Classes\.graphml\OpenWithProgids" "${PROGID}"
  DeleteRegKey HKLM "Software\Classes\${PROGID}"
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'

  Delete "$SMPROGRAMS\CyberiadaPro\*.lnk"
  RMDir "$SMPROGRAMS\CyberiadaPro"
  ; the folder goes as a whole only when it is the editor's
  ${If} ${FileExists} "$INSTDIR\CyberiadaPro.exe"
    SetOutPath "$TEMP"
    RMDir /r "$INSTDIR"
  ${EndIf}
  DeleteRegKey HKLM "${UNINST_KEY}"
  DetailPrint "$(PythonKept)"
SectionEnd
