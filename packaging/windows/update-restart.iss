; The update from the app (#117, UpdateInstaller): the app runs this setup
; with /SILENT /CLOSEAPPLICATIONS /NORESTARTAPPLICATIONS /RESTARTAPP=1. The
; Restart Manager closes the app, the setup installs the new version, and
; this entry starts it when the installation ends. "runasoriginaluser": as the
; user who started the update, not as an administrator (an installation "for
; all users" runs elevated). Only in a silent setup with /RESTARTAPP=1
; (FdRestartAfterUpdate, update-restart.pas): a normal setup asks on its last
; page ("Launch ...").
; https://jrsoftware.org/ishelp/topic_runsection.htm
; CMakeLists.txt adds this file with CPACK_INNOSETUP_EXTRA_SCRIPTS.

[Run]
Filename: "{app}\FinanceDashboard.exe"; Flags: nowait runasoriginaluser skipifnotsilent; Check: FdRestartAfterUpdate
