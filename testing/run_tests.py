import argparse
import sys
import os
from pathlib import Path, PurePath
from dataclasses import dataclass
import subprocess

TESTS_DIR = Path(__file__).parent / "tests"
os.makedirs(TESTS_DIR, exist_ok=True)

OUTPUT_DIR = Path(__file__).parent / "output"
os.makedirs(OUTPUT_DIR, exist_ok=True)

RESET_COLOR = "\x1b[0m"
GREEN = "\x1b[32m"
RED = "\x1b[31m"


@dataclass
class TestResult:
    # return code of the compilation
    build_status: int = 0
    # whether the test produced correct output
    correct: None | bool = None

    def describe(self) -> str:
        if self.build_status != 0:
            return f"{RED}build failed with code {self.build_status}{RESET_COLOR}"
        return (
            f"{GREEN}success{RESET_COLOR}"
            if self.correct
            else f"{RED}failed{RESET_COLOR}"
        )

    def is_success(self) -> bool:
        return self.build_status == 0 and self.correct


class Test:
    name: str
    rel_path: PurePath
    src_path: Path
    expected_path: Path
    expected_ret: int
    expected_stdout: str | None

    def __init__(self, rel_path: PurePath):
        """
        Read a test from a .sasc file given its path relative to TESTS_DIR
        """
        self.rel_path = rel_path
        self.name = rel_path.as_posix()

        self.src_path = TESTS_DIR / rel_path
        if not self.src_path.is_file():
            print(f'Invalid test "{self.name}": Unable to find file "{self.src_path}"')
            sys.exit(1)

        self.expected_path = TESTS_DIR / rel_path.with_suffix(".expected")
        if not self.expected_path.is_file():
            print(
                f'Invalid test "{self.name}": Unable to find file "{self.expected_path}"'
            )
            sys.exit(1)

        try:
            with open(self.expected_path, "r") as expected_file:
                self.expected_ret = int(expected_file.readline())
                self.expected_stdout = expected_file.read()
        except FileNotFoundError:
            print(
                f'Invalid test "{self.name}": Unable to find *.expected file at {self.expected_path}'
            )
            sys.exit(1)
        except TypeError:
            print(f'Invalid test "{self.name}": Invalid .expected file')
            sys.exit(1)

    def output_path(self) -> Path:
        out = OUTPUT_DIR / self.rel_path.with_suffix("")
        os.makedirs(out, exist_ok=True)
        return out

    def run(self, compiler_path: Path) -> TestResult:
        output_dir = self.output_path()
        build_output_dir = output_dir / "build"
        os.makedirs(build_output_dir, exist_ok=True)

        with (
            open(build_output_dir / "stdout.txt", "w") as stdout_f,
            open(build_output_dir / "stderr.txt", "w") as stderr_f,
        ):
            build_res = subprocess.run(
                [compiler_path, self.src_path], stdout=stdout_f, stderr=stderr_f
            )

        if build_res.returncode != 0:
            # build failed
            return TestResult(build_status=build_res.returncode)

        # success
        return TestResult()


if __name__ == "__main__":
    arg_parser = argparse.ArgumentParser(description="Test runner")
    arg_parser.add_argument("compiler_path", help="Path of compiler binary to test")
    args = arg_parser.parse_args()

    # validating args
    compiler_path = Path(args.compiler_path)
    if not compiler_path.is_file():
        print(f'Could not find file: "{compiler_path}"')
        sys.exit(1)

    tests = [Test(f.relative_to(TESTS_DIR)) for f in TESTS_DIR.rglob("*.sasc")]

    print(f"Running {len(tests)} test{'s' if len(tests) > 1 else ''}")

    successes = 0
    for test in tests:
        print(test.name, end=" -> ", flush=True)
        res = test.run(compiler_path)
        print(res.describe())
        if res.is_success():
            successes += 1
    print(
        f"Testing complete\n{successes}/{len(tests)} tests passing ({100.0 * float(successes) / float(len(tests)):.2f}%)"
    )
