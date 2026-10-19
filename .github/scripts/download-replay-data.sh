#!/usr/bin/env bash
set -euo pipefail
: "${AWS_ACCESS_KEY_ID:?Missing R2_ACCESS_KEY_ID}" \
  "${AWS_SECRET_ACCESS_KEY:?Missing R2_SECRET_ACCESS_KEY}" \
  "${AWS_ENDPOINT_URL:?Missing R2_ENDPOINT_URL}" "${GAME_PATH:?}"

# These are the same trimmed replay-only archives used by the Windows job.
download_data() {
    local archive=$1 game=$2 checksum=$3
    local destination="$RUNNER_TEMP/$archive"
    aws s3 cp "s3://github-ci/$archive" "$destination" --endpoint-url "$AWS_ENDPOINT_URL"
    echo "$checksum  $destination" | sha256sum --check --strict
    mkdir -p "$GAME_PATH/$game"
    7z x -y "$destination" "-o$GAME_PATH/$game"
    rm "$destination"
}
download_data generals108_gamedata_trimmed.7z Generals \
    37a351aa430199d1f05deb9e404857dce7b461a6ac272c5d4a0b5652cdb06372
download_data zerohour104_gamedata_trimmed.7z GeneralsMD \
    6837fe1e3009a4c239406c39b1598216c0943ee8ed46bb10626767029ac05e21
