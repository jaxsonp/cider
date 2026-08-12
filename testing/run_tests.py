import argparse
import sys
import os
import asyncio
from pathlib import Path, PurePath
import dataclasses
import re
import shutil
import os
import time
import traceback

TESTS_DIR = Path(__file__).parent / "tests"

OUTPUT_DIR = Path(__file__).parent / "output"

PROGRESS_REFRESH_HZ = 30.0
PROGRESS_BAR_WIDTH = 80

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
    def __init__(
        self,
        n_workers: int,
        compiler_path: Path,
        show_progress: bool = True,
        use_color: bool = True,
        show_passed: bool = False,
        show_failed: bool = True,
    ):
        if not compiler_path.is_file():
            print(f'Could not find file: "{compiler_path}"')
            sys.exit(1)
        elif n_workers <= 0:
            print("Worker count must >0")
            sys.exit(1)
        self.compiler_path = compiler_path
        self.n_workers = n_workers
        self.total_test_count = 0
        self.tests_ran = 0
        self.tests_passed = 0
        self.completed = False

        # display settings
        self.show_progress = show_progress
        self.show_passed = show_passed
        self.show_failed = show_failed

        # color settings
        self.style_fg_green = "\x1b[32m" if use_color else ""
        self.style_fg_red = "\x1b[31m" if use_color else ""
        self.style_reset = "\x1b[0m" if use_color else ""

        self._test_queue = asyncio.Queue()

        self._print_lock = asyncio.Lock()

    async def run_tests(self) -> bool:
        """
        Runs tests, returns `True` if all tests ran successfully
        """
        try:
            # prepare tests
            tests: list[Test] = []
            TESTS_DIR.mkdir(exist_ok=True, parents=True)
            OUTPUT_DIR.mkdir(exist_ok=True, parents=True)
            for f in TESTS_DIR.rglob("*.cdr"):
                rel_path = f.relative_to(TESTS_DIR)
                tests.extend(create_tests_from_file(rel_path))
            print(f"Found {len(tests)} tests in {TESTS_DIR}")
            if len(tests) == 0:
                return True
            self.total_test_count = len(tests)

            async with asyncio.TaskGroup() as task_group:
                # print testing progress
                progress_printing_task = None
                if self.show_progress:
                    progress_printing_task = task_group.create_task(
                        self.progress_bar_task(),
                        name="test_runner_progress",
                    )

                # create workers
                worker_tasks = [
                    task_group.create_task(
                        self.worker(),
                        name=f"test_runner_worker{i}",
                    )
                    for i in range(self.n_workers)
                ]

                print("Starting testing")
                start_t = time.perf_counter()
                # feed tests to workers
                for test in tests:
                    await self._test_queue.put(test)

                # wait for all tests to be ran
                await self._test_queue.join()
                end_t = time.perf_counter()
                self.completed = True

                # stop aux tasks
                if progress_printing_task is not None:
                    progress_printing_task.cancel()
                for worker_task in worker_tasks:
                    if not worker_task.done():
                        worker_task.cancel()
            # done, task group cleans up aux tasks

            print(f"{self.tests_ran} tests ran in {end_t - start_t:.2f} seconds")

            tests_failed = self.tests_ran - self.tests_passed
            fail_percent = 100.0 * float(tests_failed) / float(self.tests_ran)

            if self.tests_passed == self.tests_ran:
                print(f"{self.style_fg_green}All tests passing{self.style_reset}")
                return True
            else:
                print(
                    f"{self.style_fg_red}{tests_failed} tests failed ({fail_percent:.2f}%){self.style_reset}"
                )
                return False
        except asyncio.CancelledError:
            print("\nTesting cancelled")
            raise
        except Exception as e:
            print(f"\nException thrown during testing: {e}")
            traceback.format_exc()


    async def progress_bar_task(self):
        """
        Periodically prints testing progress info/bar
        """
        if not self.show_progress:
            return

        try:
            # hide cursor and send newlines to make room
            sys.stdout.write("\x1b[?25l\n\n")

            while True:
                await self.print_progress()
                await asyncio.sleep(1.0 / float(PROGRESS_REFRESH_HZ))
        finally:
            # print final state if testing completed fully
            if self.completed:
                await self.print_progress()
            # show cursor
            sys.stdout.write("\x1b[?25h")



    async def print_progress(self):
        progress = float(self.tests_ran) / float(self.total_test_count)
        progress_percent = 100.0 * progress
        success_rate = (
            (100.0 * float(self.tests_passed) / float(self.tests_ran))
            if self.tests_ran != 0
            else 100.0
        )

        async with self._print_lock:
            # move up, clear line, write progress stats
            sys.stdout.write(
                f"\x1b[2A\x1b[2K | {progress_percent:.1f}% complete, {success_rate:.1f}% passed\n | "
            )

            # then write progress bar
            for column in range(PROGRESS_BAR_WIDTH):
                # difference (in columns) of the progress to the left edge of this column
                delta = (progress * float(PROGRESS_BAR_WIDTH)) - float(column)
                if delta >= 1.0:
                    sys.stdout.write("█")
                elif delta <= 0.0:
                    sys.stdout.write("─")
                else:
                    for i, block in enumerate(("▏", "▎", "▍", "▌", "▋", "▊", "▉")):
                        if delta < (float(i + 1) / 8.0):
                            sys.stdout.write(block)
                            break
                    else:
                        sys.stdout.write("█")
            sys.stdout.write("\n")
            sys.stdout.flush()

    async def record_test_result(
        self,
        test: Test,
        success: bool,
        message: str = "",
    ):
        self.tests_ran += 1
        if success:
            self.tests_passed += 1

        if (success and not self.show_passed) or (not success and not self.show_failed):
            # nothing to do print
            return

        async with self._print_lock:
            if self.show_progress:
                # if there's a progress bar, we need to move up to clear it
                sys.stdout.write("\x1b[2A")
            sys.stdout.write("- ")
            sys.stdout.write(test.name)
            sys.stdout.write(" => ")
            sys.stdout.write(
                f"{self.style_fg_green}passed"
                if success
                else f"{self.style_fg_red}failed"
            )
            if message:
                sys.stdout.write(f" ({message})")
            sys.stdout.write(self.style_reset)
            if self.show_progress:
                # if there's a progress bar, we need to move back down below it
                sys.stdout.write("\r\x1b[2B")
            sys.stdout.write("\n")
            sys.stdout.flush()

        # update progress bar
        if self.show_progress:
            await self.print_progress()

    async def worker(self):
        try:
            while True:
                test: Test = await self._test_queue.get()

                # setup files/paths
                output_dir = OUTPUT_DIR / test.platform / test.rel_path.with_suffix("")
                if output_dir.exists():
                    shutil.rmtree(output_dir)
                output_dir.mkdir(parents=True, exist_ok=True)

                bin_path = output_dir / test.rel_path.with_suffix("").name
                build_stdout_path = output_dir / "build_stdout.txt"
                build_stderr_path = output_dir / "build_stderr.txt"
                run_stdout_path = output_dir / "stdout.txt"
                run_stderr_path = output_dir / "stderr.txt"

                # running build command
                build_cmd = [
                    self.compiler_path,
                    "--target",
                    test.platform,
                    "--emit",
                    "exe",
                    "--out",
                    bin_path,
                    "-vv",
                    test.full_path,
                ]
                with (
                    open(build_stdout_path, "w") as build_stdout_f,
                    open(build_stderr_path, "w") as build_stderr_f,
                ):
                    build_subprocess: asyncio.subprocess.Process = (
                        await asyncio.create_subprocess_exec(
                            *build_cmd,
                            stdout=build_stdout_f,
                            stderr=build_stderr_f,
                        )
                    )
                    build_return_code = await build_subprocess.wait()

                # checking build output
                if build_return_code != test.expected_build_exit_code:
                    # unexpected build exit code
                    await self.record_test_result(
                        test,
                        False,
                        f"build exited with {build_return_code}, expected {test.expected_build_exit_code}",
                    )
                    self._test_queue.task_done()
                    continue
                elif build_subprocess.returncode != 0:
                    # successul, expected build failure
                    await self.record_test_result(test, True)
                    self._test_queue.task_done()
                    continue

                # changing perms (TODO fix?)
                os.chmod(bin_path, 0o700)

                # running compiled program
                run_cmd = PLATFORM_EMULATORS[test.platform](bin_path)
                with (
                    open(run_stdout_path, "w") as run_stdout_f,
                    open(run_stderr_path, "w") as run_stderr_f,
                ):
                    run_subprocess = await asyncio.create_subprocess_exec(
                        *run_cmd,
                        stdout=run_stdout_f,
                        stderr=run_stderr_f,
                    )
                    run_return_code = await run_subprocess.wait()

                # checking program output
                if run_return_code != test.expected_exit_code:
                    # unexpected program exit code
                    await self.record_test_result(
                        test,
                        False,
                        f"program exited with {run_return_code}, expected {test.expected_exit_code}",
                    )
                    self._test_queue.task_done()
                    continue
                elif test.expected_stdout != "" or test.expected_stderr != "":
                    # TODO
                    await self.record_test_result(
                        test, False, f"stdout/stderr checking is unimplemented"
                    )
                    self._test_queue.task_done()
                    continue

                await self.record_test_result(test, True)
                self._test_queue.task_done()

        except asyncio.CancelledError:
            return
        except Exception as e:
            print(f"Exception in worker: {e}")


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
    progress_opts = arg_parser.add_mutually_exclusive_group(required=False)
    progress_opts.add_argument(
        "--progress",
        dest="progress",
        action="store_true",
        help="Force enable pretty progress bar/info (use --no-progress to disable)",
    )
    progress_opts.add_argument(
        "--no-progress",
        dest="progress",
        action="store_false",
    )
    color_opts = arg_parser.add_mutually_exclusive_group(required=False)
    color_opts.add_argument(
        "--color",
        dest="color",
        action="store_true",
        help="Force enable colored output (use --no-color to disable)",
    )
    color_opts.add_argument(
        "--no-color",
        dest="color",
        action="store_false",
    )
    arg_parser.add_argument(
        "--show-results",
        type=str,
        choices=["none", "failed", "all"],
        default="failed",
        help="Print test results as they are completed (default: %(default)s)",
    )
    arg_parser.set_defaults(color=None, progress=None)
    args = arg_parser.parse_args()

    is_tty = sys.stdout.isatty()

    use_color = (
        args.color is None
        and (
            is_tty if "FORCE_COLOR" not in os.environ
            else (os.environ["FORCE_COLOR"] != "0")
        )
    ) or args.color
    show_progress = (args.progress is None and is_tty) or args.progress
    show_passed = args.show_results == "all"
    show_failed = args.show_results != "none"

    test_runner = TestRunner(
        n_workers=int(args.workers),
        compiler_path=Path(args.compiler_path),
        show_progress=show_progress,
        use_color=use_color,
        show_passed=show_passed,
        show_failed=show_failed,
    )

    async_runner = asyncio.Runner()
    try:
        success = async_runner.run(test_runner.run_tests())
        sys.exit(int(not success))
    except KeyboardInterrupt:
        print("Stopped")
    except Exception as e:
        print(f"Uncaught exception: {e}")
        traceback.format_exc()
    finally:
        async_runner.close()

    sys.exit(1)
