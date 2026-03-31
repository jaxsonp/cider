import argparse
import sys
import os
from pathlib import Path, PurePath
import subprocess
import dataclasses
import re

TESTS_DIR = Path(__file__).parent / "tests"
os.makedirs(TESTS_DIR, exist_ok=True)

OUTPUT_DIR = Path(__file__).parent / "output"
os.makedirs(OUTPUT_DIR, exist_ok=True)

RESET_COLOR = "\x1b[0m"
GREEN = "\x1b[32m"
RED = "\x1b[31m"


# pattern for parsing test definition kv pairs
TEST_DEFINITION_PAT = r"^//!\s*([^=]+)=(.*)"

# All supported platforms to test. Map of target names to lambdas that create a cmd from a file
PLATFORM_EMULATORS = {
    "linux-riscv32g": lambda file: ["qemu-riscv32", file],
}


@dataclasses.dataclass()
class Test:
    name: str
    platform: str
    rel_path: PurePath
    full_path: Path
    expected_build_exit_code: int = 0
    expected_exit_code: int = 0
    expected_stdout: str = ""
    expected_stderr: str = ""

    def run(self, compiler_path: Path, compiler_verbosity: int) -> tuple[bool, str]:
        output_dir = OUTPUT_DIR / self.platform / self.rel_path.with_suffix("")
        os.makedirs(output_dir, exist_ok=True)

        bin_path = output_dir / self.rel_path.with_suffix("").name
        if bin_path.exists():
            os.remove(bin_path)

        # creating build command
        cmd = [compiler_path]
        if compiler_verbosity > 0:
            cmd.append("-" + ("v" * compiler_verbosity))
        cmd.extend(["--target", self.platform])
        cmd.extend(["--out", bin_path])
        cmd.append(self.full_path)

        # running build command
        with (
            open(output_dir / "build_stdout.txt", "w") as build_stdout_f,
            open(output_dir / "build_stderr.txt", "w") as build_stderr_f,
        ):
            build_res = subprocess.run(
                cmd,
                stdout=build_stdout_f,
                stderr=build_stderr_f,
            )

        # checking build output
        if build_res.returncode != self.expected_build_exit_code:
            # unexpected build exit code
            return (
                False,
                f"{RED}failed{RESET_COLOR} (build exited with {build_res.returncode}, expected {self.expected_build_exit_code})",
            )
        elif build_res.returncode != 0:
            # expected build failure
            return (
                True,
                f"{GREEN}success{RESET_COLOR}",
            )

        # changing perms
        os.chmod(bin_path, 0o700)

        # running program
        cmd = PLATFORM_EMULATORS[self.platform](bin_path)
        with (
            open(output_dir / "stdout.txt", "w") as stdout_f,
            open(output_dir / "stderr.txt", "w") as stderr_f,
        ):
            run_res = subprocess.run(
                cmd,
                stdout=stdout_f,
                stderr=stderr_f,
            )

        # checking program output
        if run_res.returncode != self.expected_exit_code:
            # unexpected program exit code
            return (
                False,
                f"{RED}failed{RESET_COLOR} (program exited with {run_res.returncode}, expected {self.expected_exit_code})",
            )
        elif self.expected_stdout != "" or self.expected_stderr != "":
            # TODO
            return (False, "checking stdout/stderr is not implemented yet")

        return (True, f"{GREEN}pass{RESET_COLOR}")


def create_tests_from_file(rel_path: Path) -> list[Test]:
    """
    Creates tests given a filepath relative to TESTS_DIR
    """
    full_path = TESTS_DIR / rel_path
    new_tests = [
        Test(
            name=f"{platform} {rel_path.as_posix()}",
            platform=platform,
            rel_path=rel_path,
            full_path=full_path,
        )
        for platform in PLATFORM_EMULATORS.keys()
    ]
    with open(full_path, "r") as f:
        while True:
            line = f.readline()
            if len(line) == 0:
                break
            match = re.match(TEST_DEFINITION_PAT, line.rstrip("\n"))
            if not match:
                break
            key = match.group(1)
            val = match.group(2)
            match key:
                case "BUILD_EXIT_CODE":
                    for t in new_tests:
                        t.expected_build_exit_code = int(val)
                case "EXIT_CODE":
                    for t in new_tests:
                        t.expected_exit_code = int(val)
                case "STDOUT":
                    for t in new_tests:
                        t.expected_stdout = val
                case "STDERR":
                    for t in new_tests:
                        t.expected_stderr = val
    return new_tests


if __name__ == "__main__":
    arg_parser = argparse.ArgumentParser(description="Test runner")
    arg_parser.add_argument("compiler_path", help="Path of compiler binary to test")
    arg_parser.add_argument(
        "--compiler-verbosity",
        type=int,
        default=1,
        metavar="N",
        help="Verbosity setting when setting compiler (default: 1)",
    )
    args = arg_parser.parse_args()

    # validating cli args
    compiler_path = Path(args.compiler_path)
    if not compiler_path.is_file():
        print(f'Could not find file: "{compiler_path}"')
        sys.exit(1)
    compiler_verbosity = args.compiler_verbosity

    # reading tests
    print(f"Reading tests from {TESTS_DIR}")
    tests: list[Test] = []
    for f in TESTS_DIR.rglob("*.sasc"):
        rel_path = f.relative_to(TESTS_DIR)
        tests.extend(create_tests_from_file(rel_path))

    print(f"Running {len(tests)} test{'s' if len(tests) > 1 else ''}")

    successes = 0
    for test in tests:
        print(f" {test.name} -> ", end="", flush=True)
        success, msg = test.run(compiler_path, compiler_verbosity)
        print(msg)
        if success:
            successes += 1
    print(
        f"Testing complete\n{successes}/{len(tests)} tests passing ({100.0 * float(successes) / float(len(tests)):.2f}%)"
    )

    sys.exit(0 if successes == len(tests) else 1)
