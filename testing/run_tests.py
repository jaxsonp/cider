import argparse
import sys
import os
import asyncio
from pathlib import Path, PurePath
import subprocess
import dataclasses
import re

TESTS_DIR = Path(__file__).parent / "tests"

OUTPUT_DIR = Path(__file__).parent / "output"

COLOR_RESET = "\x1b[0m"
COLOR_GREEN = "\x1b[32m"
COLOR_RED = "\x1b[31m"

# pattern for parsing test definition kv pairs
TEST_DEFINITION_PAT = r"^//!\s*([^=]+)=(.*)"

# All supported platforms to test. Map of target names to lambdas that create a cmd from a file
PLATFORM_EMULATORS = {
    "linux-riscv32g": lambda file: ["qemu-riscv32-static", file],
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


class TestRunner:
    def __init__(self, n_workers: int, compiler_path: Path):
        if not compiler_path.is_file():
            print(f'Could not find file: "{compiler_path}"')
            sys.exit(1)
        elif n_workers <= 0:
            print("Worker count must >0")
            sys.exit(1)
        self.compiler_path = compiler_path
        self.n_workers = n_workers
        self.tests_ran = 0
        self.successes = 0

        self.test_queue = asyncio.Queue(100)

        # for pretty printing
        self.print_lock = asyncio.Lock()
        self.max_test_num = 0

    async def run_tests(self) -> bool:
        """
        Runs tests, returns `True` if all tests ran successfully
        """
        try:
            # prepare tests
            tests: list[Test] = []
            os.makedirs(TESTS_DIR, exist_ok=True)
            os.makedirs(OUTPUT_DIR, exist_ok=True)
            for f in TESTS_DIR.rglob("*.cdr"):
                rel_path = f.relative_to(TESTS_DIR)
                tests.extend(create_tests_from_file(rel_path))
            print(f"Found {len(tests)} tests in {TESTS_DIR}")
            if len(tests) == 0:
                return True

            # create workers
            worker_tasks = [
                asyncio.create_task(
                    self.worker(i),
                    name=f"test_runner_worker{i}",
                )
                for i in range(self.n_workers)
            ]
            print("")  # room for pretty printin

            # feed tests to workers
            for i, test in enumerate(tests):
                await self.test_queue.put((i, test))

            # wait for them to finish
            await self.test_queue.join()

            # stop workers
            for w in worker_tasks:
                w.cancel()

            # done
            pass_rate = 100.0 * float(self.successes) / float(self.tests_ran)
            print(
                f"Testing complete\n{self.successes:d}/{self.tests_ran:d} tests passing ({pass_rate:.2f}%)"
            )
            if self.successes == self.tests_ran:
                print(f"{COLOR_GREEN}All tests passing{COLOR_RESET}")
                return True
            return False
        except asyncio.CancelledError:
            print("\nTesting cancelled")
        except Exception as e:
            print(f"\nException thrown during testing: {e}")

    async def pretty_print_test_status(
        self,
        test_num: int,
        test_name: str,
        status: str,
        color: str | None = None,
        note: str | None = None,
    ):
        if color is not None:
            status = color + status + COLOR_RESET
        if note is not None:
            status += f" ({note})"
        async with self.print_lock:
            if test_num > self.max_test_num:
                # print(f"\x1b[{abs(target_height)}S", end="") # scroll
                print("\n" * (test_num - self.max_test_num), end="", flush=False)
                self.max_test_num = test_num

            target_height = 1 + self.max_test_num - test_num

            # move up, clear line, print line, move down
            print(
                f"\x1b[{target_height}F\x1b[2K {test_name} -> {status}\x1b[{target_height}E",
                end="",
                flush=True,
            )

    async def worker(self, index: int):
        while True:
            try:
                test: Test
                test_num: int
                test_num, test = await self.test_queue.get()
                self.tests_ran += 1

                # setup files/paths
                output_dir = OUTPUT_DIR / test.platform / test.rel_path.with_suffix("")
                os.makedirs(output_dir, exist_ok=True)

                bin_path = output_dir / test.rel_path.with_suffix("").name
                if bin_path.exists():
                    os.remove(bin_path)

                # running build command
                await self.pretty_print_test_status(test_num, test.name, "building...")
                with (
                    open(output_dir / "build_stdout.txt", "w") as build_stdout_f,
                    open(output_dir / "build_stderr.txt", "w") as build_stderr_f,
                ):
                    build_res = subprocess.run(
                        [
                            self.compiler_path,
                            "--target",
                            test.platform,
                            "--out",
                            bin_path,
                            test.full_path,
                        ],
                        stdout=build_stdout_f,
                        stderr=build_stderr_f,
                    )

                # checking build output
                if build_res.returncode != test.expected_build_exit_code:
                    # unexpected build exit code
                    await self.pretty_print_test_status(
                        test_num,
                        test.name,
                        "failed",
                        color=COLOR_RED,
                        note=f"build exited with {build_res.returncode}, expected {test.expected_build_exit_code}",
                    )
                    self.test_queue.task_done()
                    continue
                elif build_res.returncode != 0:
                    # successul, expected build failure
                    await self.pretty_print_test_status(test_num, test.name, "passed")
                    self.test_queue.task_done()
                    self.successes += 1
                    continue

                # changing perms (TODO fix?)
                os.chmod(bin_path, 0o700)

                await self.pretty_print_test_status(test_num, test.name, "running...")
                with (
                    open(output_dir / "stdout.txt", "w") as stdout_f,
                    open(output_dir / "stderr.txt", "w") as stderr_f,
                ):
                    run_res = subprocess.run(
                        PLATFORM_EMULATORS[test.platform](bin_path),
                        stdout=stdout_f,
                        stderr=stderr_f,
                    )

                # checking program output
                if run_res.returncode != test.expected_exit_code:
                    # unexpected program exit code
                    await self.pretty_print_test_status(
                        test_num,
                        test.name,
                        "failed",
                        color=COLOR_RED,
                        note=f"program exited with {run_res.returncode}, expected {test.expected_exit_code}",
                    )
                    self.test_queue.task_done()
                    continue
                elif test.expected_stdout != "" or test.expected_stderr != "":
                    # TODO
                    await self.pretty_print_test_status(
                        test_num,
                        test.name,
                        "???",
                        note="STDOUT/STDERR comparison is not implemented",
                    )
                    self.test_queue.task_done()
                    self.successes += 1
                    continue

                await self.pretty_print_test_status(test_num, test.name, "passed")
                self.test_queue.task_done()
                self.successes += 1
                continue

            except asyncio.CancelledError:
                break
            except Exception as e:
                print(f"\nException in worker {index}: {e}")
                break


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
        "-n",
        "--workers",
        help="Number of concurrent workers to use",
        default=4,
        metavar="N",
    )
    args = arg_parser.parse_args()

    test_runner = TestRunner(
        n_workers=int(args.workers), compiler_path=Path(args.compiler_path)
    )

    async_runner = asyncio.Runner()
    try:
        success = async_runner.run(test_runner.run_tests())
        sys.exit(0 if success else 1)
    except KeyboardInterrupt:
        print("Keyboard interrupt")
    except Exception as e:
        print(f"Exception caught during testing: {e}")
    finally:
        async_runner.close()

    sys.exit(1)
