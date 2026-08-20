#!/usr/bin/env bash
# Build mcutrace, generate traceability evidence, and update the report
# consumed by the VS Code extension.
#
# Usage:
#   ./make.sh check             # verify required local tools
#   ./make.sh build             # configure and compile the instrumented build
#   ./make.sh test              # run mcutest binaries and write JSON evidence
#   ./make.sh coverage          # merge captures and write mcucov JSON evidence
#   ./make.sh analyze           # run mcucheck and write static-analysis evidence
#   ./make.sh report            # validate and write build/mcutrace-report.json
#   ./make.sh vscode            # test, package, and install the VS Code extension
#   ./make.sh all               # run the complete evidence workflow (default)

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${MCUTRACE_BUILD_DIR:-$ROOT_DIR/build/trace}"
ARTIFACT_DIR="$BUILD_DIR/artifacts"
TEST_ARTIFACT_DIR="$ARTIFACT_DIR/mcutest"
CAPTURE_DIR="$BUILD_DIR/captures"
HOVER_REPORT="$ROOT_DIR/build/mcutrace-report.json"
MCUTRACE_BIN="$BUILD_DIR/mcutrace"
MCUCOV_BIN="$BUILD_DIR/_deps/mcucov/mcucov"
MCUCHECK_BUILD_DIR="${MCUCHECK_BUILD_DIR:-$ROOT_DIR/../mcucheck/build/host}"
MCUCHECK_BIN="${MCUCHECK_BIN:-$MCUCHECK_BUILD_DIR/mcucheck}"
MCUCHECK_CONFIG="${MCUCHECK_CONFIG:-$ROOT_DIR/../mcucheck/standards/llm_cpp/config/default.toml}"
ANALYSIS_COMMANDS="$ARTIFACT_DIR/compile_commands.mcucheck.json"
VSCODE_DIR="$ROOT_DIR/editors/vscode"

TEST_TARGETS=(
  mcutrace_tests
  mcutrace_requirement_tests
  mcutrace_importer_tests
  mcutrace_producer_importer_tests
  mcutrace_trace_import_tests
  mcutrace_source_annotation_tests
  mcutrace_hardening_tests
  mcutrace_assembly_tests
  mcutrace_validation_tests
  mcutrace_config_tests
  mcutrace_cli_tests
  mcutrace_output_tests
  mcutrace_evidence_expectation_tests
)

log() {
  echo "==> $*"
}

run() {
  echo "    $*"
  "$@"
}

has_cmd() {
  command -v "$1" >/dev/null 2>&1
}

usage() {
  sed -n '1,12p' "$0"
}

cmd_check() {
  log "Checking required tools"
  local status=0
  local tool
  for tool in cmake ninja jq; do
    if has_cmd "$tool"; then
      echo "    [OK]   $tool"
    else
      echo "    [FAIL] $tool (required)" >&2
      status=1
    fi
  done
  if has_cmd c++ || has_cmd g++ || has_cmd clang++; then
    echo "    [OK]   C++ compiler"
  else
    echo "    [FAIL] C++ compiler (required)" >&2
    status=1
  fi
  return "$status"
}

cmd_build() {
  log "Configuring instrumented build in $BUILD_DIR"
  run cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DMCUTRACE_BUILD_TESTS=ON \
    -DMCUTRACE_DISABLE_EXCEPTIONS_RTTI=ON \
    -DMCUTRACE_ENABLE_MCUCOV=ON \
    -DMCUCOV_FETCH_DEPENDENCIES="${MCUCOV_FETCH_DEPENDENCIES:-OFF}"
  run cmake --build "$BUILD_DIR" --parallel
  run cmake --build "$BUILD_DIR" --target mcucov --parallel
}

cmd_test() {
  if [[ "${1:-}" != "--skip-build" ]]; then
    cmd_build
  fi
  mkdir -p "$TEST_ARTIFACT_DIR" "$CAPTURE_DIR"

  log "Running mcutest binaries"
  local status=0
  local target
  for target in "${TEST_TARGETS[@]}"; do
    local test_binary="$BUILD_DIR/tests/$target"
    local test_artifact="$TEST_ARTIFACT_DIR/$target.json"
    local capture="$CAPTURE_DIR/$target.mcuv"
    echo "    $target"
    if ! MCUTRACE_JSON_TEST_OUTPUT=1 MCUTRACE_MCUCOV_CAPTURE="$capture" \
      "$test_binary" > "$test_artifact"; then
      status=1
    fi
  done

  log "Merging mcutest evidence"
  jq -s '{
    format: "mcutest-results",
    version: 1,
    summary: {
      tests: ([.[].tests[]] | length),
      failures: ([.[].tests[] | select(.status == "failed")] | length)
    },
    tests: [.[].tests[]]
  }' "$TEST_ARTIFACT_DIR"/*.json > "$ARTIFACT_DIR/mcutest.json"
  return "$status"
}

cmd_coverage() {
  if [[ "${1:-}" != "--skip-build" ]]; then
    cmd_build
  fi
  mkdir -p "$ARTIFACT_DIR"

  local captures=()
  local target
  for target in "${TEST_TARGETS[@]}"; do
    local capture="$CAPTURE_DIR/$target.mcuv"
    if [[ -s "$capture" ]]; then
      captures+=("$capture")
    fi
  done
  if [[ ${#captures[@]} -eq 0 ]]; then
    echo "No mcucov captures found. Run ./make.sh test first." >&2
    return 2
  fi

  log "Generating mcucov report"
  run "$MCUCOV_BIN" merge "${captures[@]}" --output "$ARTIFACT_DIR/mcucov.mcuv"
  run "$MCUCOV_BIN" report \
    --input "$ARTIFACT_DIR/mcucov.mcuv" \
    --manifest "$BUILD_DIR/mcucov/mcutrace_core" \
    --source-root "$ROOT_DIR" \
    --format json \
    --output "$ARTIFACT_DIR/mcucov.json"
  run "$MCUCOV_BIN" report \
    --input "$ARTIFACT_DIR/mcucov.mcuv" \
    --manifest "$BUILD_DIR/mcucov/mcutrace_core" \
    --source-root "$ROOT_DIR" \
    --format lcov \
    --output "$ARTIFACT_DIR/mcucov.lcov"
}

ensure_mcucheck() {
  if [[ -x "$MCUCHECK_BIN" ]]; then
    return
  fi
  log "Building mcucheck in $MCUCHECK_BUILD_DIR"
  run cmake -S "$ROOT_DIR/../mcucheck" -B "$MCUCHECK_BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DMCUCHECK_BUILD_TESTS=OFF
  run cmake --build "$MCUCHECK_BUILD_DIR" --target mcucheck --parallel
}

cmd_analyze() {
  if [[ "${1:-}" != "--skip-build" ]]; then
    cmd_build
  fi
  ensure_mcucheck
  mkdir -p "$ARTIFACT_DIR"

  log "Preparing compilation database for mcucheck"
  jq 'map(if has("arguments")
      then .arguments |= map(select(. != "-Werror"))
      else .command |= gsub("(^| )-Werror( |$)"; " ")
      end)' "$BUILD_DIR/compile_commands.json" > "$ANALYSIS_COMMANDS"

  log "Running mcucheck"
  local status=0
  if "$MCUCHECK_BIN" analyze "$ANALYSIS_COMMANDS" \
    --project-root "$ROOT_DIR" \
    --config "$MCUCHECK_CONFIG" \
    --jobs 1 \
    --format json \
    > "$ARTIFACT_DIR/mcucheck.json" \
    2> "$ARTIFACT_DIR/mcucheck.log"; then
    :
  else
    status=$?
  fi
  if [[ ! -s "$ARTIFACT_DIR/mcucheck.json" ]]; then
    echo "mcucheck did not produce JSON evidence; see $ARTIFACT_DIR/mcucheck.log" >&2
    return 2
  fi
  return "$status"
}

cmd_report() {
  mkdir -p "$(dirname "$HOVER_REPORT")"
  log "Validating the traceability graph"
  if "$MCUTRACE_BIN" --config "$ROOT_DIR/mcutrace.toml" validate --format json > "$HOVER_REPORT"; then
    :
  else
    local status=$?
    echo "Trace report contains validation failures." >&2
    return "$status"
  fi
}

cmd_vscode() {
  log "Checking VS Code extension tooling"
  local status=0
  local tool
  for tool in node npm npx code; do
    if has_cmd "$tool"; then
      echo "    [OK]   $tool"
    else
      echo "    [FAIL] $tool (required)" >&2
      status=1
    fi
  done
  if [[ "$status" -ne 0 ]]; then
    return "$status"
  fi

  pushd "$VSCODE_DIR" >/dev/null
  local package_name
  local package_version
  local package_path
  package_name="$(node -p 'require("./package.json").name')"
  package_version="$(node -p 'require("./package.json").version')"
  package_path="$VSCODE_DIR/$package_name-$package_version.vsix"

  log "Running VS Code extension tests"
  run npm test

  log "Packaging VS Code extension"
  run npx --yes @vscode/vsce package --out "$package_path"
  popd >/dev/null

  log "Installing VS Code extension"
  run code --install-extension "$package_path" --force
  echo "VS Code extension: $package_path"
}

cmd_all() {
  local status=0
  cmd_build || return $?
  cmd_test --skip-build || status=$?
  cmd_coverage --skip-build || status=$?
  cmd_analyze --skip-build || status=$?
  cmd_report || status=$?
  echo "Trace report: $HOVER_REPORT"
  echo "Artifacts: $ARTIFACT_DIR"
  return "$status"
}

case "${1:-all}" in
  check) cmd_check ;;
  build) cmd_build ;;
  test) cmd_test ;;
  coverage) cmd_coverage ;;
  analyze) cmd_analyze ;;
  report) cmd_report ;;
  vscode) cmd_vscode ;;
  all) cmd_all ;;
  -h|--help|help) usage ;;
  *)
    echo "Unknown command: $1" >&2
    usage >&2
    exit 2
    ;;
esac
