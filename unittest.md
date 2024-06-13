(Work in progress, do not review etc)

This PR is part one of two to speed up testing for PJSIP. This first part attempts to speed up the C/C++ based tests and part two will address Python based testing framework.

The plan is to implement unit testing framework in PJLIB, that that is what the bulk of this PR presents. More specifically, this PR contains several work:

1. the (new) unit testing framework, is in `<pj/unittest.h>` and `pj/unittest.c`.
2. largish modifications to `pjlib-test`, `pjlib-util-test`, `pjnath-test`, `pjmedia-test`, and `pjsip-test`.
3. minor developments:

   - modifications to `pj/fifobuf.[hc]`, an old feature (part of pjsip initial commit!) that has never been used until now 
   - new auxiliary feature: `<pj/argparse.h>`, header only utilities to parse command line arguments.

Let's discuss the work in more detail.

### 1. Unit testing framework

#### Overview

The unit testing framework `<pj/unittest.h>` provides two parts.

First part is test macros that can be used to replace manual `if`s in the test codes. This can be used anywhere without having to use the unit test framework (apart from including the header file of course). Example:

```
   PJ_TEST_NON_ZERO(pool, "pool allocation failed", {rc=-1; goto on_error;});
   PJ_TEST_EQ(count, expected, "invalid read count", return -100);
   PJ_TEST_SUCCESS(pj_ioqueue_read(...), NULL, {rc=-80; goto on_return; });
```

On failure, the macros above will display the expressions being checked, the value of the expressions, and the status message (for PJ_TEST_SUCCESS), saving the developer from having to write all these things.

The second part is the unit test framework. The framework uses common architecture as described in https://en.wikipedia.org/wiki/XUnit. The global workflow to create/run unit test is something like this:

```
   pj_test_case tc0, tc1;
   pj_test_suite suite;
   pj_test_runner *runner;
   pj_test_stat stat;

   pj_test_suite_init(&suite);

   pj_test_case_init(&tc0, ..., &func_to_test0, arg0, ... );
   pj_test_suite_add_case(&suite, &tc0);

   pj_test_case_init(&tc1, ..., &func_to_test1, arg1, ... );
   pj_test_suite_add_case(&suite, &tc1);

   pj_test_create_text_runner(pool, .., &runner);
   pj_test_run(runner, &suite);
   pj_test_runner_destroy(runner);

   pj_test_get_stat(&suite, &stat);
   pj_test_display_stat(&stat, ..);
   pj_test_display_log_messages(&suite, PJ_TEST_FAILED_TESTS);
```


### 2. Modifications to test apps

#### Speedup

In PJLIB-TEST:

- 0 worker thread: 9m22.228s (original)
- 1 worker thread: 4m51.940s
- 2 worker threads: 4m9.322s
- 3 worker threads: 2m52.350s
- 4 worker threads: 2m31.399s
- 6 worker threads: 2m28.450s
- 8 worker threads: 2m19.779s

So it looks like the sweet spot is with 3 worker threads. This is because some tests takes quite a long time t o finish:

```
[14/20] udp_ioqueue_unreg_test           [OK] [92.759s]
[15/20] ioqueue_stress_test              [OK] [111.061s]
[17/20] ioqueue_perf_test1               [OK] [110.149s]
[18/20] activesock_test                  [OK] [101.890s]
```

I've tried to split up those tests into smaller tests, but mostly that's not feasible because the tests are benchmarking tests (that need to gather all results to determine the overall winner).


### 3. Other (Minor) developments



The unit test framework has the following features.

#### Parallel execution

The main objective of this project is to speed up testing. Parallelism should be configurable on test by test case basis. The test designer will be able to control which tests can run in parallel and which can't.

#### Familiarity

Use common architecture as described in https://en.wikipedia.org/wiki/XUnit

#### Easy to use

Enable porting of existing tests with minimal effort, e.g. just need changing the `test.c` and not each of the test file.

#### Nice output

Nice console output, e.g.:

```
[1/24] errno_test.. [OK]
[2/24] exception_test.. [OK]
[3/24] os_test. [OK]
```
Must capture logging of each test so output is not cluttered when the test is run in parallel. Most likely will show the log only after all tests are done (to maintain the niceness of console output in the above point), and will only show logs for failed tests.

#### Selective test

Run specific tests from cmdline, e.g. `./pjlib-test fifobuf_test os_test`

#### Work in restricted targets

Able to run without pool etc, because crucial/basic tests such as list, pool, thread tests  can only be run after the test suite creation is finished!

#### Useful test macros

Make it a complete unit-testing framework by implementing various assertion macros. While changing existing tests to use this new assertion macros is tedious and without significant benefit, providing the macros may be nice for writing future tests.

Also added argparse.h

Some considerations:

1. When a unit-test is run (in any thread), it will change (globally) the log writer function to an internal unittest log writer function (in order to capture the log). This has some implications:
   a. If apps change the log writer before running the unittest, this should be okay. The unittest will restore the log writer to any writer prior to it being run, and will use that writer to display the logs after it is run.
   b. If apps change the log writer somewhere inside a test function, I think this should be okay as long as it restores back the writer. 
   c. if another thread (that is not aware about testing) is writing to the log, then the unittest log writer will pass that log message to pj_log_write() (i.e. the "official" log writer). I think this is okay and it is the desired behavior, but it will clutter the test output.
2. Use {} instead of do {} while (0) in case user pus "break" as action.

Test times:

- 0 thread: 9m22.228s
- 1 thread: 4m51.940s
- 2 threads: 4m9.322s
- 3 threads: 2m52.350s
- 4 threads: 2m31.399s
- 6 threads: 2m28.450s
- 8 threads: 2m19.779s

Auxiliary features:

- pj/argparse.h
