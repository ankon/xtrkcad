;
; This file is included from the CMake generated NSIS file during install.
;

CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\XTrackCAD Help.lnk" "$INSTDIR\share\xtrkcad\xtrkcad.chm" "" "" 0
CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\XTrackCAD ReadMe.lnk" "notepad.exe" "$INSTDIR\share\xtrkcad\ReadMe.txt"
CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\XTrackCAD CHANGELOG.lnk" "notepad.exe" "$INSTDIR\share\xtrkcad\CHANGELOG.txt"


;
;  create file association
;
; back up old value of .xtc
  !define Index "Line${__LINE__}"
  ReadRegStr $1 HKCR ".xtc" ""
  StrCmp $1 "" "${Index}-NoBackup"
    StrCmp $1 "XTrackCAD.Design" "${Index}-NoBackup"
      WriteRegStr HKCR ".xtc" "backup_val" $1
  "${Index}-NoBackup:"

  ; back up old value of .xtce
    !define /redef Index "Line${__LINE__}"
    ReadRegStr $1 HKCR ".xtce" ""
    StrCmp $1 "" "${Index}-NoBackup"
      StrCmp $1 "XTrackCAD.Design" "${Index}-NoBackup"
        WriteRegStr HKCR ".xtce" "backup_xtce" $1
    "${Index}-NoBackup:"

; create the new association
    WriteRegStr HKCR ".xtc" "" "XTrackCAD.Design"
    WriteRegStr HKCR ".xtce" "" "XTrackCAD.Design"
    WriteRegStr HKCR "XTrackCAD.Design" "" "XTrackCAD Layout Design"
    WriteRegStr HKCR "XTrackCAD.Design\shell" "" "open"
    WriteRegStr HKCR "XTrackCAD.Design\DefaultIcon" "" "xtrkcad.exe,0"
    WriteRegStr HKCR "XTrackCAD.Design\shell\open\command" "" '$INSTDIR\bin\xtrkcad.exe "%1"'

  System::Call 'Shell32::SHChangeNotify(i 0x8000000, i 0, i 0, i 0)'

  !undef Index
