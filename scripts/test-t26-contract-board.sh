#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORKDIR="$(mktemp -d)"
trap 'rm -rf "${WORKDIR}"' EXIT

cat >"${WORKDIR}/Define.h" <<'EOF'
#pragma once
#include <cstdint>
using uint8 = std::uint8_t;
using uint16 = std::uint16_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
EOF

cat >"${WORKDIR}/ObjectGuid.h" <<'EOF'
#pragma once
class ObjectGuid
{
public:
    static ObjectGuid const Empty;
    bool IsEmpty() const { return true; }
    unsigned long long GetRawValue() const { return 0; }
};
inline ObjectGuid const ObjectGuid::Empty{};
EOF

CXX="${CXX:-g++}"

"${CXX}" \
  -std=c++17 \
  -Wall -Wextra -Werror \
  -I"${WORKDIR}" \
  -I"${ROOT}/modules/mod-fury/src" \
  "${ROOT}/tests/golden/contract_board_policy_golden.cpp" \
  -o "${WORKDIR}/contract_board_policy_golden"

"${WORKDIR}/contract_board_policy_golden"

SCRIPT="${ROOT}/modules/mod-fury/src/scripts/FuryDefiasBoardScript.cpp"
OVERLAY="${ROOT}/modules/mod-fury/data/sql/db-world/updates/2026_09_23_01_fury_westfall_board.sql"

grep -Fq 'GameObjectScript' "${SCRIPT}"
grep -Fq 'OnGossipHello' "${SCRIPT}"
grep -Fq 'OnGossipSelect' "${SCRIPT}"
grep -Fq 'AddGossipItemFor' "${SCRIPT}"
grep -Fq 'fury_westfall_contract_board' "${SCRIPT}"
grep -Fq 'classic.westfall.contracts' "${SCRIPT}"

if grep -Eiq 'SendAddonMessage|addon channel|custom client patch' "${SCRIPT}"; then
  echo "[FURY][FAIL] T26 board script introduced a client dependency" >&2
  exit 1
fi

grep -Fq '9000100' "${OVERLAY}"
grep -Fq "'fury_westfall_contract_board'" "${OVERLAY}"
grep -Eq '9000100,[[:space:]]*10,[[:space:]]*17' "${OVERLAY}"

if command -v mysql >/dev/null 2>&1 && [[ -n "${MYSQL_HOST:-}" ]]; then
  MYSQL_PORT="${MYSQL_PORT:-3306}"
  MYSQL_USER="${MYSQL_USER:-root}"
  MYSQL_PASSWORD="${MYSQL_PASSWORD:-}"
  TEST_DB="${MYSQL_DATABASE:-fury_t26_world}"

  mysql_args=(-h "${MYSQL_HOST}" -P "${MYSQL_PORT}" -u "${MYSQL_USER}" --protocol=TCP)
  if [[ -n "${MYSQL_PASSWORD}" ]]; then
    mysql_args+=("-p${MYSQL_PASSWORD}")
  fi

  mysql "${mysql_args[@]}" -e "DROP DATABASE IF EXISTS \`${TEST_DB}\`; CREATE DATABASE \`${TEST_DB}\`;"
  mysql "${mysql_args[@]}" "${TEST_DB}" <<'SQL'
CREATE TABLE gameobject_template (
  entry MEDIUMINT UNSIGNED NOT NULL PRIMARY KEY,
  type TINYINT UNSIGNED NOT NULL DEFAULT 0,
  displayId MEDIUMINT UNSIGNED NOT NULL DEFAULT 0,
  name VARCHAR(100) NOT NULL DEFAULT '',
  IconName VARCHAR(100) NOT NULL DEFAULT '',
  castBarCaption VARCHAR(100) NOT NULL DEFAULT '',
  unk1 VARCHAR(100) NOT NULL DEFAULT '',
  size FLOAT NOT NULL DEFAULT 1,
  data0 INT UNSIGNED NOT NULL DEFAULT 0,
  data1 INT UNSIGNED NOT NULL DEFAULT 0,
  data2 INT UNSIGNED NOT NULL DEFAULT 0,
  data3 INT UNSIGNED NOT NULL DEFAULT 0,
  data4 INT UNSIGNED NOT NULL DEFAULT 0,
  data5 INT UNSIGNED NOT NULL DEFAULT 0,
  data6 INT UNSIGNED NOT NULL DEFAULT 0,
  data7 INT UNSIGNED NOT NULL DEFAULT 0,
  data8 INT UNSIGNED NOT NULL DEFAULT 0,
  data9 INT UNSIGNED NOT NULL DEFAULT 0,
  data10 INT UNSIGNED NOT NULL DEFAULT 0,
  data11 INT UNSIGNED NOT NULL DEFAULT 0,
  data12 INT UNSIGNED NOT NULL DEFAULT 0,
  data13 INT UNSIGNED NOT NULL DEFAULT 0,
  data14 INT UNSIGNED NOT NULL DEFAULT 0,
  data15 INT UNSIGNED NOT NULL DEFAULT 0,
  data16 INT UNSIGNED NOT NULL DEFAULT 0,
  data17 INT UNSIGNED NOT NULL DEFAULT 0,
  data18 INT UNSIGNED NOT NULL DEFAULT 0,
  data19 INT UNSIGNED NOT NULL DEFAULT 0,
  data20 INT UNSIGNED NOT NULL DEFAULT 0,
  data21 INT UNSIGNED NOT NULL DEFAULT 0,
  data22 INT UNSIGNED NOT NULL DEFAULT 0,
  data23 INT UNSIGNED NOT NULL DEFAULT 0,
  AIName CHAR(64) NOT NULL DEFAULT '',
  ScriptName VARCHAR(64) NOT NULL DEFAULT '',
  VerifiedBuild INT NULL
);

CREATE TABLE gameobject (
  guid INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
  id INT UNSIGNED NOT NULL DEFAULT 0,
  map SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  zoneId SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  areaId SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  spawnMask TINYINT UNSIGNED NOT NULL DEFAULT 1,
  phaseMask SMALLINT UNSIGNED NOT NULL DEFAULT 1,
  position_x FLOAT NOT NULL DEFAULT 0,
  position_y FLOAT NOT NULL DEFAULT 0,
  position_z FLOAT NOT NULL DEFAULT 0,
  orientation FLOAT NOT NULL DEFAULT 0,
  rotation0 FLOAT NOT NULL DEFAULT 0,
  rotation1 FLOAT NOT NULL DEFAULT 0,
  rotation2 FLOAT NOT NULL DEFAULT 0,
  rotation3 FLOAT NOT NULL DEFAULT 0,
  spawntimesecs INT NOT NULL DEFAULT 0,
  animprogress TINYINT UNSIGNED NOT NULL DEFAULT 0,
  state TINYINT UNSIGNED NOT NULL DEFAULT 1,
  ScriptName CHAR(64) NULL DEFAULT '',
  VerifiedBuild INT NULL,
  Comment TEXT NULL
);

INSERT INTO gameobject_template
  (entry, type, displayId, name, ScriptName)
VALUES (9000101, 5, 1, 'Unrelated template', 'other_script');

INSERT INTO gameobject
  (guid, id, map, zoneId, areaId, position_x, position_y, position_z, ScriptName, Comment)
VALUES (9000101, 9000101, 1, 1, 1, 1, 2, 3, 'other_script', 'unrelated');
SQL

  mysql "${mysql_args[@]}" "${TEST_DB}" < "${OVERLAY}"
  mysql "${mysql_args[@]}" "${TEST_DB}" < "${OVERLAY}"

  template_count="$(mysql "${mysql_args[@]}" -N -s "${TEST_DB}" -e "SELECT COUNT(*) FROM gameobject_template WHERE entry=9000100;")"
  spawn_count="$(mysql "${mysql_args[@]}" -N -s "${TEST_DB}" -e "SELECT COUNT(*) FROM gameobject WHERE guid=9000100;")"
  script_name="$(mysql "${mysql_args[@]}" -N -s "${TEST_DB}" -e "SELECT ScriptName FROM gameobject_template WHERE entry=9000100;")"
  template_shape="$(mysql "${mysql_args[@]}" -N -s "${TEST_DB}" -e "SELECT CONCAT(type,':',displayId) FROM gameobject_template WHERE entry=9000100;")"
  spawn_shape="$(mysql "${mysql_args[@]}" -N -s "${TEST_DB}" -e "SELECT CONCAT(map,':',zoneId,':',areaId) FROM gameobject WHERE guid=9000100;")"
  unrelated="$(mysql "${mysql_args[@]}" -N -s "${TEST_DB}" -e "SELECT CONCAT(name,':',ScriptName) FROM gameobject_template WHERE entry=9000101;")"

  [[ "${template_count}" == "1" ]]
  [[ "${spawn_count}" == "1" ]]
  [[ "${script_name}" == "fury_westfall_contract_board" ]]
  [[ "${template_shape}" == "10:17" ]]
  [[ "${spawn_shape}" == "0:40:108" ]]
  [[ "${unrelated}" == "Unrelated template:other_script" ]]

  mysql "${mysql_args[@]}" -e "DROP DATABASE \`${TEST_DB}\`;"
  echo "[FURY][PASS] T26 world overlay is idempotent and leaves unrelated rows untouched"
fi

echo "[FURY][PASS] T26 Westfall contract board gate passed"
