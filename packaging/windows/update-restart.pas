// The check function of the [Run] entry of update-restart.iss (#117): true
// when the app started this setup for an update, with /RESTARTAPP=1.
// https://jrsoftware.org/ishelp/topic_consts.htm ({param:...})
// CMakeLists.txt adds this file with CPACK_INNOSETUP_CODE_FILES: CPack puts
// it in the [Code] section.
function FdRestartAfterUpdate: Boolean;
begin
  Result := ExpandConstant('{param:RESTARTAPP|0}') = '1';
end;
