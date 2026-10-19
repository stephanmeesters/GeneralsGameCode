#!/usr/bin/env bash
set -euo pipefail

: "${RUNNER_TEMP:?}" "${GAME_PATH:?}" "${GAME:?}" "${USERDATA:?}"
[[ "$GAME" == GeneralsMD ]] || { echo 'Only GeneralsMD replays are supported' >&2; exit 1; }

# A pinned standalone Proton installation and a minimal Steam library let
# Protontricks select this exact prefix without a Steam login or game download.
export PROTON_VERSION=GE-Proton9-27
export STEAM_DIR="$RUNNER_TEMP/replay-steam"
export STEAM_COMPAT_CLIENT_INSTALL_PATH="$STEAM_DIR"
export STEAM_COMPAT_DATA_PATH="$STEAM_DIR/steamapps/compatdata/2732960"
export SteamAppId=2732960 SteamGameId=2732960
export PROTON_USE_WINED3D=1 WINEDEBUG=-all
proton_dir="$STEAM_DIR/compatibilitytools.d/$PROTON_VERSION"
mkdir -p "$STEAM_DIR/ubuntu12_32" "$STEAM_DIR/config" \
    "$STEAM_DIR/steamapps/common/GeneralsMD" "$STEAM_COMPAT_DATA_PATH" \
    "$STEAM_DIR/compatibilitytools.d"
if [[ ! -f "$proton_dir/proton" ]]; then
    archive="$RUNNER_TEMP/$PROTON_VERSION.tar.gz"
    curl --fail --location --retry 3 \
        "https://github.com/GloriousEggroll/proton-ge-custom/releases/download/$PROTON_VERSION/$PROTON_VERSION.tar.gz" \
        -o "$archive"
    # Release checksum, pinned alongside the release version.
    echo '86a2b2962a2509104201b3532bc829d058d666bc8220417f71bd4af660b6d05781e9f684b3982339d695a5fd4babe19e97ec42a82a78311faf99fc1257280623'"  $archive" | sha512sum --check --strict
    tar -xzf "$archive" -C "$STEAM_DIR/compatibilitytools.d"
    rm "$archive"
fi
cat > "$STEAM_DIR/steamapps/appmanifest_2732960.acf" <<'EOF'
"AppState"
{
    "appid" "2732960"
    "name" "Generals Zero Hour CI"
    "installdir" "GeneralsMD"
    "StateFlags" "4"
}
EOF
printf '"libraryfolders" {}\n' > "$STEAM_DIR/steamapps/libraryfolders.vdf"
printf '"InstallConfigStore" {}\n' > "$STEAM_DIR/config/config.vdf"

proton="$proton_dir/proton"
"$proton" run wineboot.exe --init
# Ubuntu 24.04's Winetricks rejects Protontricks' Wine wrapper scripts during
# architecture detection. Use the pinned upstream release without that patch.
export WINETRICKS="$RUNNER_TEMP/winetricks"
curl --fail --location --retry 3 \
    https://raw.githubusercontent.com/Winetricks/winetricks/e20b2f6f80d175f96208f51800130db7459dd28c/src/winetricks \
    -o "$WINETRICKS"
chmod +x "$WINETRICKS"
# Install into the SAME Proton prefix used for replay checking. Plain Wine's
# built-in msvcrt is not suitable for VC6 replay compatibility.
protontricks --no-runtime --no-bwrap 2732960 -q vcrun6
export WINEDLLOVERRIDES=msvcrt=n,b

cp -a "$GAME_PATH/$GAME/." build/
generals_win="Z:${GAME_PATH//\//\\}\\Generals\\"
"$proton" run reg.exe add 'HKCU\Software\Electronic Arts\EA Games\Generals' \
    /v InstallPath /t REG_SZ /d "$generals_win" /f

# Proton creates steamuser and routes the Windows Documents folder here.
documents="$STEAM_COMPAT_DATA_PATH/pfx/drive_c/users/steamuser/Documents"
user_dir="$documents/Command and Conquer Generals Zero Hour Data"
mkdir -p "$user_dir/Replays" "$user_dir/Maps"
cp -a "$USERDATA/Replays/." "$user_dir/Replays/"
cp -a "$USERDATA/Maps/." "$user_dir/Maps/"
test -n "$(find "$user_dir/Replays" -iname '*.rep' -print -quit)"

cd build
test -f generalszh.exe
status=0
timeout --kill-after=30s 10m "$proton" run ./generalszh.exe \
    -jobs 4 -headless -replay '*.rep' > stdout.log 2> stderr.log || status=$?
cat stdout.log
cat stderr.log >&2
exit "$status"
