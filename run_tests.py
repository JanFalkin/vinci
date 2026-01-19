#!/usr/bin/env python3
"""
Vinci Tree Generator Test Runner

Builds and runs all tests with enhanced output formatting and reporting.
Demonstrates Python subprocess management, output parsing, and reporting.
"""

import argparse
import subprocess
import sys
import re
import time
from pathlib import Path
from dataclasses import dataclass
from typing import List, Optional
from enum import Enum


class TestStatus(Enum):
    """Test execution status"""
    PASSED = "PASSED"
    FAILED = "FAILED"
    SKIPPED = "SKIPPED"


class Color:
    """ANSI color codes for terminal output"""
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    BOLD = '\033[1m'
    RESET = '\033[0m'

    @staticmethod
    def disable():
        """Disable colors for non-TTY output"""
        Color.GREEN = ''
        Color.RED = ''
        Color.YELLOW = ''
        Color.BLUE = ''
        Color.CYAN = ''
        Color.BOLD = ''
        Color.RESET = ''


@dataclass
class TestResult:
    """Individual test result"""
    suite: str
    name: str
    status: TestStatus
    duration_ms: int

    @property
    def full_name(self) -> str:
        return f"{self.suite}.{self.name}"


@dataclass
class TestSummary:
    """Overall test run summary"""
    total_tests: int
    passed: int
    failed: int
    skipped: int
    total_duration_ms: int
    test_results: List[TestResult]


class TestRunner:
    """Manages building and running C++ tests"""

    def __init__(self, build_dir: Path, verbose: bool = False, no_color: bool = False):
        self.build_dir = build_dir
        self.verbose = verbose
        self.root_dir = build_dir.parent

        if no_color or not sys.stdout.isatty():
            Color.disable()

    def build_tests(self) -> bool:
        """Build the test executable using CMake"""
        print(f"{Color.BLUE}Building tests...{Color.RESET}")
        start_time = time.time()

        try:
            result = subprocess.run(
                ['cmake', '--build', str(self.build_dir)],
                cwd=self.root_dir,
                capture_output=True,
                text=True,
                check=False
            )

            build_time = time.time() - start_time

            if result.returncode != 0:
                print(f"{Color.RED}✗ Build failed{Color.RESET} ({build_time:.1f}s)")
                if self.verbose or result.returncode != 0:
                    print(result.stderr)
                return False

            # Check for warnings
            warnings = [line for line in result.stderr.split('\n') if 'warning:' in line.lower()]

            if warnings:
                print(f"{Color.YELLOW}✓ Build succeeded with {len(warnings)} warning(s){Color.RESET} ({build_time:.1f}s)")
                if self.verbose:
                    for warning in warnings:
                        print(f"  {warning}")
            else:
                print(f"{Color.GREEN}✓ Build succeeded{Color.RESET} ({build_time:.1f}s)")

            return True

        except FileNotFoundError:
            print(f"{Color.RED}Error: cmake not found{Color.RESET}")
            return False
        except Exception as e:
            print(f"{Color.RED}Build error: {e}{Color.RESET}")
            return False

    def run_tests(self, filter_pattern: Optional[str] = None) -> Optional[TestSummary]:
        """Run the test executable and parse results"""
        # Use absolute path resolution
        test_executable = (self.root_dir / self.build_dir / 'tree_tests').resolve()

        if not test_executable.exists():
            print(f"{Color.RED}Error: Test executable not found at {test_executable}{Color.RESET}")
            return None

        print(f"\n{Color.BLUE}Running tests...{Color.RESET}")

        # Build command
        cmd = [str(test_executable)]
        if filter_pattern:
            cmd.append(f'--gtest_filter={filter_pattern}')
            print(f"{Color.CYAN}Filter: {filter_pattern}{Color.RESET}")

        try:
            result = subprocess.run(
                cmd,
                cwd=self.build_dir,
                capture_output=True,
                text=True,
                timeout=120  # 2 minute timeout
            )

            # Show full output in verbose mode
            if self.verbose:
                print(f"\n{Color.CYAN}{'='*70}{Color.RESET}")
                print(result.stdout)
                if result.stderr:
                    print(result.stderr)
                print(f"{Color.CYAN}{'='*70}{Color.RESET}\n")

            return self._parse_test_output(result.stdout, result.stderr)

        except subprocess.TimeoutExpired:
            print(f"{Color.RED}Error: Tests timed out after 120 seconds{Color.RESET}")
            return None
        except Exception as e:
            print(f"{Color.RED}Error running tests: {e}{Color.RESET}")
            return None

    def _parse_test_output(self, stdout: str, stderr: str) -> TestSummary:
        """Parse Google Test output format"""
        test_results = []

        # Pattern: [       OK ] TestSuite.TestName (123 ms)
        # Pattern: [  FAILED  ] TestSuite.TestName (123 ms)
        test_pattern = re.compile(r'\[\s+(OK|FAILED)\s+\]\s+(\w+)\.(\w+)\s+\((\d+)\s+ms\)')

        for line in stdout.split('\n'):
            match = test_pattern.search(line)
            if match:
                status_str, suite, name, duration = match.groups()
                status = TestStatus.PASSED if status_str == 'OK' else TestStatus.FAILED
                test_results.append(TestResult(
                    suite=suite,
                    name=name,
                    status=status,
                    duration_ms=int(duration)
                ))

        # Parse summary line: [  PASSED  ] 22 tests.
        summary_pattern = re.compile(r'\[\s+PASSED\s+\]\s+(\d+)\s+tests?')
        failed_pattern = re.compile(r'\[\s+FAILED\s+\]\s+(\d+)\s+tests?')

        passed = 0
        failed = 0

        for line in stdout.split('\n'):
            if match := summary_pattern.search(line):
                passed = int(match.group(1))
            if match := failed_pattern.search(line):
                failed = int(match.group(1))

        total_duration = sum(test.duration_ms for test in test_results)

        return TestSummary(
            total_tests=len(test_results),
            passed=passed,
            failed=failed,
            skipped=0,
            total_duration_ms=total_duration,
            test_results=test_results
        )

    def print_summary(self, summary: TestSummary):
        """Print formatted test summary"""
        print(f"\n{Color.BOLD}{'='*70}{Color.RESET}")
        print(f"{Color.BOLD}Test Summary{Color.RESET}")
        print(f"{Color.BOLD}{'='*70}{Color.RESET}")

        # Overall status
        if summary.failed == 0:
            status_color = Color.GREEN
            status_icon = "✓"
        else:
            status_color = Color.RED
            status_icon = "✗"

        print(f"\n{status_color}{status_icon} Total: {summary.total_tests} tests{Color.RESET}")
        print(f"  {Color.GREEN}Passed: {summary.passed}{Color.RESET}")

        if summary.failed > 0:
            print(f"  {Color.RED}Failed: {summary.failed}{Color.RESET}")

        # Duration breakdown
        print(f"\n{Color.CYAN}Duration: {summary.total_duration_ms/1000:.1f}s total{Color.RESET}")

        # Show slowest tests
        if summary.test_results:
            slowest = sorted(summary.test_results, key=lambda t: t.duration_ms, reverse=True)[:5]
            print(f"\n{Color.BOLD}Slowest tests:{Color.RESET}")
            for test in slowest:
                duration_sec = test.duration_ms / 1000
                print(f"  {test.full_name:.<50} {duration_sec:>6.1f}s")

        # Show failed tests
        failed_tests = [t for t in summary.test_results if t.status == TestStatus.FAILED]
        if failed_tests:
            print(f"\n{Color.RED}{Color.BOLD}Failed tests:{Color.RESET}")
            for test in failed_tests:
                print(f"  {Color.RED}✗ {test.full_name}{Color.RESET}")

        print(f"\n{Color.BOLD}{'='*70}{Color.RESET}\n")


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description='Build and run Vinci tree generator tests',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                          # Build and run all tests
  %(prog)s --filter "*OEIS*"        # Run only OEIS tests
  %(prog)s --no-build               # Skip build, just run tests
  %(prog)s --verbose                # Show detailed output
        """
    )

    parser.add_argument(
        '--build-dir',
        type=Path,
        default=Path('build'),
        help='Build directory (default: build)'
    )

    parser.add_argument(
        '--filter',
        type=str,
        help='Google Test filter pattern (e.g., "*OEIS*")'
    )

    parser.add_argument(
        '--no-build',
        action='store_true',
        help='Skip building, just run tests'
    )

    parser.add_argument(
        '--verbose',
        action='store_true',
        help='Show detailed build output and warnings'
    )

    parser.add_argument(
        '--no-color',
        action='store_true',
        help='Disable colored output'
    )

    args = parser.parse_args()

    # Validate build directory
    if not args.build_dir.exists():
        print(f"{Color.RED}Error: Build directory '{args.build_dir}' does not exist{Color.RESET}")
        print(f"Run: mkdir {args.build_dir} && cd {args.build_dir} && cmake ..")
        return 1

    runner = TestRunner(args.build_dir, verbose=args.verbose, no_color=args.no_color)

    # Build phase
    if not args.no_build:
        if not runner.build_tests():
            return 1

    # Test phase
    summary = runner.run_tests(filter_pattern=args.filter)

    if summary is None:
        return 1

    # Report results
    runner.print_summary(summary)

    # Exit with appropriate code
    return 0 if summary.failed == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
