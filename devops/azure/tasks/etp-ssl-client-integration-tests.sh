#!/usr/bin/env bash

set -uo pipefail

for var in RDDMS_URL DATA_PARTITION_ID ACCESS_TOKEN LEGAL_TAG DOMAIN; do
  if [[ -z "${!var:-}" ]]; then
    echo "ERROR: Required environment variable $var is not set" >&2
    exit 2
  fi
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
DATA_DIR="${DATA_DIR:-${REPO_ROOT}/data/IntegrationTestsData}"
RESULTS_DIR="${RESULTS_DIR:-${SCRIPT_DIR}/results}"
REPORT_PATH="${RESULTS_DIR}/open-etp.xml"
TIMEOUT="${TIMEOUT:-180s}"

case "$RDDMS_URL" in
  https://*) WSS_URL="wss://${RDDMS_URL#https://}" ;;
  http://*) WSS_URL="ws://${RDDMS_URL#http://}" ;;
  wss://*|ws://*) WSS_URL="$RDDMS_URL" ;;
  *) WSS_URL="wss://$RDDMS_URL" ;;
esac

suffix="$(date +%s)-$$"
SPACE_MAIN="test/sslclient-${suffix}"
SPACE_COPY="test/copy-${suffix}"
SPACE_TXSIZE="test/txsize-${suffix}"
SPACE_TXTIME="test/txtime-${suffix}"
SPACE_IMPSPC="test/impspc-${suffix}"
EXPORT_DIR="${RESULTS_DIR}/export-${suffix}"
EPC_FILE="${DATA_DIR}/Volve_Demo_Horizons_Depth.epc"

TEST_NAMES=()
TEST_RESULTS=()
FAILURES=0

space() {
  openETPServer space \
    -S "$WSS_URL" \
    --data-partition-id "$DATA_PARTITION_ID" \
    --auth bearer \
    --jwt-token "$ACCESS_TOKEN" \
    --timeout "$TIMEOUT" \
    "$@"
}

probe() {
  openETPServer probe \
    -S "$WSS_URL" \
    --data-partition-id "$DATA_PARTITION_ID" \
    --auth bearer \
    --jwt-token "$ACCESS_TOKEN" \
    --timeout "$TIMEOUT" \
    "$@"
}

cleanup() {
  echo "Cleaning acceptance dataspaces"
  space --delete -s "$SPACE_IMPSPC" >/dev/null 2>&1 || true
  space --delete -s "$SPACE_COPY" >/dev/null 2>&1 || true
  space --unlock -s "$SPACE_MAIN" >/dev/null 2>&1 || true
  space --delete -s "$SPACE_MAIN" >/dev/null 2>&1 || true
  space --delete -s "$SPACE_TXSIZE" >/dev/null 2>&1 || true
  space --delete -s "$SPACE_TXTIME" >/dev/null 2>&1 || true
  rm -rf "$EXPORT_DIR"
}

write_junit() {
  mkdir -p "$RESULTS_DIR"
  {
    printf '<?xml version="1.0" encoding="UTF-8"?>\n'
    printf '<testsuite name="open-etp-acceptance" tests="%d" failures="%d">\n' \
      "${#TEST_NAMES[@]}" "$FAILURES"
    for index in "${!TEST_NAMES[@]}"; do
      printf '  <testcase classname="open-etp.ssl-client" name="%s">' \
        "${TEST_NAMES[$index]}"
      if [[ "${TEST_RESULTS[$index]}" == "failed" ]]; then
        printf '<failure message="scenario failed">See workflow output for command diagnostics.</failure>'
      fi
      printf '</testcase>\n'
    done
    printf '</testsuite>\n'
  } > "$REPORT_PATH"
}

finish() {
  code=$?
  trap - EXIT INT TERM
  cleanup
  write_junit
  exit "$code"
}
trap finish EXIT INT TERM

run_test() {
  local name="$1"
  shift
  TEST_NAMES+=("$name")
  echo "=== ${#TEST_NAMES[@]}: $name ==="
  if (set -e; "$@"); then
    TEST_RESULTS+=("passed")
    echo "PASS: $name"
  else
    TEST_RESULTS+=("failed")
    FAILURES=$((FAILURES + 1))
    echo "FAIL: $name" >&2
    return 1
  fi
}

xdata="{\"viewers\":[\"data.default.viewers@${DATA_PARTITION_ID}.${DOMAIN}\"],\"owners\":[\"data.default.owners@${DATA_PARTITION_ID}.${DOMAIN}\"],\"legaltags\":\"${LEGAL_TAG}\",\"otherRelevantDataCountries\":[\"US\"]}"

test_probe() {
  local output
  output="$(probe --ping 2>&1)"
  echo "$output"
  grep -q "Pong Timestamp" <<< "$output"
}

test_create() {
  space --new -s "$SPACE_MAIN" --xdata "$xdata"
}

test_list() {
  local output
  output="$(space -l 2>&1)"
  echo "$output"
  grep -q "$SPACE_MAIN" <<< "$output"
}

test_info() {
  space --info -s "$SPACE_MAIN"
}

test_import() {
  space -s "$SPACE_MAIN" --import-epc "$EPC_FILE"
}

assert_total() {
  local output
  output="$(space --stats -s "$1" 2>&1)"
  echo "$output"
  grep -q "TOTAL" <<< "$output"
}

test_stats() {
  assert_total "$SPACE_MAIN"
}

test_toc() {
  local output
  output="$(space --toc -s "$SPACE_MAIN" 2>&1)"
  echo "$output"
  grep -q "resource" <<< "$output"
}

test_array_stats() {
  local output
  output="$(space --array-stats -s "$SPACE_MAIN" 2>&1)"
  echo "$output"
  grep -q "TOTAL" <<< "$output"
}

test_export() {
  mkdir -p "$EXPORT_DIR"
  space -s "$SPACE_MAIN" --export-epc "$EXPORT_DIR/exported.epc" --overwrite
  [[ -s "$EXPORT_DIR/exported.epc" ]]
}

test_lock() {
  space --lock -s "$SPACE_MAIN"
}

test_lock_status() {
  local output
  output="$(space -l 2>&1)"
  echo "$output"
  grep "$SPACE_MAIN" <<< "$output" | grep -q "r-"
}

test_copy_by_ref() {
  space --new -s "$SPACE_COPY" --xdata "$xdata"
  space --copy --by-ref -s "$SPACE_COPY" --source-space "$SPACE_MAIN"
  assert_total "$SPACE_COPY"
}

test_unlock() {
  space --delete -s "$SPACE_COPY"
  space --unlock -s "$SPACE_MAIN"
}

test_transaction_size() {
  space --new -s "$SPACE_TXSIZE" --xdata "$xdata"
  space -s "$SPACE_TXSIZE" --import-epc "$EPC_FILE" \
    --transaction-limit-size 1048576 --max-msg-size 50000
  assert_total "$SPACE_TXSIZE"
}

test_transaction_time() {
  space --new -s "$SPACE_TXTIME" --xdata "$xdata"
  space -s "$SPACE_TXTIME" --import-epc "$EPC_FILE" \
    --transaction-limit-time 3s --max-msg-size 50000
  assert_total "$SPACE_TXTIME"
}

test_reject_duplicates() {
  space -s "$SPACE_MAIN" --import-epc "$EPC_FILE" --reject-duplicates
  assert_total "$SPACE_MAIN"
}

test_gzip() {
  space -s "$SPACE_MAIN" --import-epc "$EPC_FILE" -Z gzip
  assert_total "$SPACE_MAIN"
}

test_import_space() {
  space --lock -s "$SPACE_MAIN"
  space --new -s "$SPACE_IMPSPC" --xdata "$xdata"
  space --import-space "$SPACE_MAIN" -s "$SPACE_IMPSPC"
  assert_total "$SPACE_IMPSPC"
}

[[ -f "$EPC_FILE" ]] || {
  echo "ERROR: acceptance fixture not found: $EPC_FILE" >&2
  exit 2
}

run_test "probe ping" test_probe || exit 1
run_test "create isolated dataspace" test_create || exit 1
run_test "list dataspaces" test_list || exit 1
run_test "get dataspace info" test_info || exit 1
run_test "import EPC" test_import || exit 1
run_test "verify import stats" test_stats || exit 1
run_test "table of contents" test_toc || exit 1
run_test "array stats" test_array_stats || exit 1
run_test "export EPC" test_export || exit 1
run_test "lock dataspace" test_lock || exit 1
run_test "verify lock status" test_lock_status || exit 1
run_test "copy by reference" test_copy_by_ref || exit 1
run_test "unlock dataspace" test_unlock || exit 1
run_test "transaction size limit" test_transaction_size || exit 1
run_test "transaction time limit" test_transaction_time || exit 1
run_test "reject duplicate import" test_reject_duplicates || exit 1
run_test "gzip import" test_gzip || exit 1
run_test "server-side import space" test_import_space || exit 1

echo "ALL 18 TESTS PASSED"
