#!/usr/bin/env bash
# Builds the compiler and runs the unit tests, then the e2e tests for every
# supported target, like .github/workflows/test.yml. Non-native targets run
# through qemu-user-static. Extra arguments are passed to every runner call.

set -u

cd "$(dirname "$0")/.."

# target and the command its programs are run through
targets=(
	"linux-x86_64|"
	"linux-x86|qemu-i386-static"
	"linux-riscv32g|qemu-riscv32-static -cpu rv32,c=false"
	"linux-riscv32gc|qemu-riscv32-static"
	"linux-riscv64g|qemu-riscv64-static -cpu rv64,c=false"
	"linux-riscv64gc|qemu-riscv64-static"
)

echo "=== building ========================================================"
cmake -S . -B build || exit 1
cmake --build build --parallel || exit 1

runner=build/testing/ciderc_test_runner
failed=()

echo "=== unit tests ======================================================"
"$runner" --unit "$@" || failed+=("unit")

for entry in "${targets[@]}"; do
	target="${entry%%|*}"
	prefix="${entry#*|}"
	echo "=== e2e tests ($target) ============================================="
	if [[ -n "$prefix" ]] && ! command -v "${prefix%% *}" > /dev/null; then
		echo "${prefix%% *} not found, install qemu-user-static"
		failed+=("$target")
		continue
	fi
	"$runner" --e2e -C build/ciderc -t "$target" --e2e-prefix "$prefix" "$@" \
		|| failed+=("$target")
done

echo "=== done ============================================================"
if [[ ${#failed[@]} -eq 0 ]]; then
	echo "All tests passed"
else
	echo "Failed: ${failed[*]}"
	exit 1
fi
