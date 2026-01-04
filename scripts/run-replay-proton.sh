REPLAY_ARG="${1-}"
DEBUG_CRC_FROM_FRAME="${2-}"
DEBUG_CRC_UNTIL_FRAME="${3-}"

flatpak run --env=REPLAY_ARG="$REPLAY_ARG" \
  --env=DEBUG_CRC_FROM_FRAME="$DEBUG_CRC_FROM_FRAME" \
  --env=DEBUG_CRC_UNTIL_FRAME="$DEBUG_CRC_UNTIL_FRAME" \
  --command=sh com.valvesoftware.Steam -c '
REPLAY_ARG=${REPLAY_ARG-}
DEBUG_CRC_FROM_FRAME=${DEBUG_CRC_FROM_FRAME-}
DEBUG_CRC_UNTIL_FRAME=${DEBUG_CRC_UNTIL_FRAME-}
export REPLAY_ARG
export DEBUG_CRC_FROM_FRAME
export DEBUG_CRC_UNTIL_FRAME
set -u
export STEAM_COMPAT_DATA_PATH=/var/home/stephan/.var/app/com.valvesoftware.Steam/data/Steam/steamapps/compatdata/2732960
export STEAM_COMPAT_CLIENT_INSTALL_PATH=/var/home/stephan/.var/app/com.valvesoftware.Steam/data/Steam
export STEAM_COMPAT_APP_ID=2732960
export SteamAppId=2732960

PROTON=/var/home/stephan/.var/app/com.valvesoftware.Steam/data/Steam/compatibilitytools.d/GE-Proton10-28/proton
CMD_EXE="Z:\var\home\stephan\.var\app\com.valvesoftware.Steam\data\Steam\steamapps\compatdata\2732960\pfx\drive_c\windows\system32\cmd.exe"
GAME_DIR="Z:\var\home\stephan\.var\app\com.valvesoftware.Steam\data\Steam\steamapps\common\COMM~NGG"
PFX="$STEAM_COMPAT_DATA_PATH/pfx/drive_c"
STDOUT_LOG="$PFX/stdout.log"
STDERR_LOG="$PFX/stderr.log"

rm -f "$STDOUT_LOG" "$STDERR_LOG"

# Run the bat, but do not abort the script if it fails
set +e
if [ -n "$REPLAY_ARG" ]; then
  WINEDLLOVERRIDES=msvcrt=n,b "$PROTON" waitforexitandrun "$CMD_EXE" /c "pushd $GAME_DIR && call run_replays.bat"
else
  "$PROTON" waitforexitandrun "$CMD_EXE" /c "pushd $GAME_DIR && Generals.exe"
fi
code=$?
set -e

echo "proton/cmd exit code: $code"

echo "=== STDOUT (C:\\stdout.log) ==="
if [ -f "$STDOUT_LOG" ]; then
  cat "$STDOUT_LOG"
else
  echo "(no C:\\stdout.log)"
fi

echo "=== STDERR (C:\\stderr.log) ==="
if [ -f "$STDERR_LOG" ]; then
  cat "$STDERR_LOG"
else
  echo "(no C:\\stderr.log)"
fi

exit $code
'
