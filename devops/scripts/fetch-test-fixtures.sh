#!/usr/bin/env bash

set -euo pipefail

readonly SOURCE_COMMIT="b639f8d549d505ec92d681c53c55e3769071bb5d"
readonly SOURCE_ROOT="https://community.opengroup.org/osdu/platform/domain-data-mgmt-services/reservoir/open-etp-server/-/raw/${SOURCE_COMMIT}"

usage() {
  echo "Usage: $0 <acceptance|e2e|drogon|all> [destination]" >&2
  exit 2
}

download() {
  local source_path="$1"
  local expected_sha256="$2"
  local output_path="$3"
  local partial_path="${output_path}.part.$$"

  mkdir -p "$(dirname "$output_path")"
  if [[ -f "$output_path" ]] &&
    printf '%s  %s\n' "$expected_sha256" "$output_path" | sha256sum --check --status; then
    echo "Fixture already verified: $output_path"
    return
  fi

  rm -f "$partial_path"
  trap 'rm -f -- "${partial_path:-}"' RETURN
  curl \
    --fail \
    --location \
    --proto '=https' \
    --retry 3 \
    --show-error \
    --silent \
    --tlsv1.2 \
    --output "$partial_path" \
    "${SOURCE_ROOT}/${source_path}"
  printf '%s  %s\n' "$expected_sha256" "$partial_path" | sha256sum --check --status || {
    echo "ERROR: SHA-256 mismatch for ${source_path}" >&2
    return 1
  }
  mv -f "$partial_path" "$output_path"
  trap - RETURN
  echo "Downloaded and verified: $output_path"
}

fetch_acceptance() {
  local destination="$1"
  download \
    "data/Volve_Demo_Horizons_Depth.epc" \
    "b4dfb20cf1724828d137046a268cf5d78ce1217c94e903d8f1f7499eec4ddd7c" \
    "${destination}/IntegrationTestsData/Volve_Demo_Horizons_Depth.epc"
  download \
    "data/Volve_Demo_Horizons_Depth.h5" \
    "0d0a3c4c7d4b08b3522a237cc22537905d4adc134eea59d41f9c15211f5184e4" \
    "${destination}/IntegrationTestsData/Volve_Demo_Horizons_Depth.h5"
}

fetch_e2e() {
  local destination="$1"
  download \
    "data/Volve_Demo_Reservoir_Grid_Depth.epc" \
    "df47f8214163a10b127414099d04abf1d35cbecebcba277e94948f4472ae9aaf" \
    "${destination}/Volve_Demo_Reservoir_Grid_Depth.epc"
  download \
    "data/Volve_Demo_Reservoir_Grid_Depth.h5" \
    "06ab5eb57d772770260be9d8e1cdddc391108b60c512c43347499cdd4eefd9e1" \
    "${destination}/Volve_Demo_Reservoir_Grid_Depth.h5"
}

fetch_drogon() {
  local destination="$1"
  download \
    "data/Demo_Drogon.epc" \
    "4f660756c575287d02cef9ff2d839ac775e2608217fe6c618d16e57d67a1527f" \
    "${destination}/Demo_Drogon.epc"
  download \
    "data/Demo_Drogon.h5" \
    "e0be9b03c272bf9b5b789581a30aec4c8caad0b045ba70633da93f0bab3ea725" \
    "${destination}/Demo_Drogon.h5"
}

fixture_set="${1:-}"
destination="${2:-data}"

case "$fixture_set" in
  acceptance)
    fetch_acceptance "$destination"
    ;;
  e2e)
    fetch_e2e "$destination"
    ;;
  drogon)
    fetch_drogon "$destination"
    ;;
  all)
    fetch_acceptance "$destination"
    fetch_e2e "$destination"
    fetch_drogon "$destination"
    ;;
  *)
    usage
    ;;
esac
