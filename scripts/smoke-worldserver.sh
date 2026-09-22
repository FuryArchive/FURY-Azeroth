#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

WORLDSERVER="${FURY_WORLDSERVER:-${ROOT}/build/dist/bin/worldserver}"
WORLDSERVER_CONF="${FURY_WORLDSERVER_CONF:-${ROOT}/build/dist/etc/worldserver.conf}"
if [[ ! -f "${WORLDSERVER_CONF}" && -f "${WORLDSERVER_CONF}.dist" ]]; then
  WORLDSERVER_CONF="${WORLDSERVER_CONF}.dist"
fi
STARTUP_TIMEOUT="${FURY_STARTUP_TIMEOUT:-180}"
SMOKE_RUNS="${FURY_SMOKE_RUNS:-2}"

if [[ ! -x "${WORLDSERVER}" ]]; then
  echo "[FURY][FAIL] worldserver not found or not executable: ${WORLDSERVER}" >&2
  exit 2
fi

if [[ ! -f "${WORLDSERVER_CONF}" ]]; then
  echo "[FURY][FAIL] worldserver config missing: ${WORLDSERVER_CONF}" >&2
  exit 2
fi

if ! [[ "${SMOKE_RUNS}" =~ ^[1-9][0-9]*$ ]]; then
  echo "[FURY][FAIL] FURY_SMOKE_RUNS must be a positive integer, got: ${SMOKE_RUNS}" >&2
  exit 2
fi

run_smoke_once() (
  local run_number="$1"
  local workdir fifo log server_pid=""
  workdir="$(mktemp -d)"
  fifo="${workdir}/stdin"
  log="${workdir}/worldserver.log"
  mkfifo "${fifo}"

  cleanup_run() {
    if [[ -n "${server_pid}" ]] && kill -0 "${server_pid}" 2>/dev/null; then
      kill -TERM "${server_pid}" 2>/dev/null || true
      wait "${server_pid}" 2>/dev/null || true
    fi
    rm -rf "${workdir}"
  }

  wait_for_log() {
    local pattern="$1"
    local label="$2"
    local deadline=$((SECONDS + STARTUP_TIMEOUT))

    while (( SECONDS < deadline )); do
      if grep -Fq "${pattern}" "${log}"; then
        echo "[FURY][PASS] run ${run_number}: ${label}"
        return 0
      fi

      if ! kill -0 "${server_pid}" 2>/dev/null; then
        echo "[FURY][FAIL] run ${run_number}: worldserver exited before ${label}" >&2
        tail -n 200 "${log}" >&2 || true
        return 1
      fi

      sleep 1
    done

    echo "[FURY][FAIL] run ${run_number}: timeout waiting for ${label}" >&2
    tail -n 200 "${log}" >&2 || true
    return 1
  }

  trap cleanup_run EXIT

  echo "[FURY] starting worldserver smoke run ${run_number}/${SMOKE_RUNS}"
  "${WORLDSERVER}" -c "${WORLDSERVER_CONF}" <"${fifo}" >"${log}" 2>&1 &
  server_pid=$!

  # Keep the FIFO writer open for the lifetime of this smoke run.
  exec 3>"${fifo}"

  wait_for_log "[FURY] FURY database ready." "FURY database startup"
  wait_for_log "[FURY] mod-fury initialized" "FuryApp startup"
  wait_for_log "worldserver-daemon) ready..." "worldserver ready"

  printf 'fury validate\n' >&3
  wait_for_log "FURY validation: HEALTHY" ".fury validate"

  printf 'server shutdown 1\n' >&3
  exec 3>&-

  local deadline=$((SECONDS + 30))
  while kill -0 "${server_pid}" 2>/dev/null && (( SECONDS < deadline )); do
    sleep 1
  done

  if kill -0 "${server_pid}" 2>/dev/null; then
    echo "[FURY][FAIL] run ${run_number}: worldserver did not shut down cleanly" >&2
    tail -n 200 "${log}" >&2 || true
    return 1
  fi

  local rc=0
  set +e
  wait "${server_pid}"
  rc=$?
  set -e
  server_pid=""

  if [[ "${rc}" -ne 0 ]]; then
    echo "[FURY][FAIL] run ${run_number}: worldserver exited with code ${rc}" >&2
    tail -n 200 "${log}" >&2 || true
    return "${rc}"
  fi

  echo "[FURY][PASS] run ${run_number}: startup/validation/shutdown"
)

for ((run_number = 1; run_number <= SMOKE_RUNS; run_number++)); do
  run_smoke_once "${run_number}"
done

echo "[FURY][PASS] worldserver smoke passed ${SMOKE_RUNS} consecutive run(s)"
