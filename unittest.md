(Work in progress, do not review etc)

This PR is part one of two to speed up testing in PJSIP. This part attempts to speed up the C/C++ based tests and part two will address Python based testing framework.

The plan is to implement unit testing framework in PJLIB, that that is what the bulk of this PR does. More specifically, this PR contains several work:

1. A (new) unit testing framework, in `<pj/unittest.h>` and `pj/unittest.c`.
2. Some modifications to `pjlib-test`, `pjlib-util-test`, `pjnath-test`, `pjmedia-test`, and `pjsip-test` to use the framework.
3. Other developments:

   - modifications to `pj/fifobuf.[hc]`, an old feature (part of pjsip initial commit!) that has never been used until now 
   - new auxiliary feature: `<pj/argparse.h>`, header only utilities to parse command line arguments.

Let's discuss the work in more detail.

### 1. Unit testing framework

#### Overview

The unit testing framework `<pj/unittest.h>` provides two parts.

First part is test macros that can be used to replace manual `if`s in the test codes. This can be used anywhere (in the test code though, not in library code, due to size concern of the expansion macro) without having to use the unit test framework (apart from including the header file of course). Example:

```
   PJ_TEST_NON_ZERO(pool, "pool allocation failed", {rc=-1; goto on_error;});
   PJ_TEST_EQ(count, expected, "invalid read count", return -100);
   PJ_TEST_SUCCESS(pj_ioqueue_read(...), NULL, {rc=-80; goto on_return; });
```

On failure, the macros above will display the expressions being checked, the value of the expressions, and the status message (for PJ_TEST_SUCCESS), saving the developer from having to write all these things. For example:

```
15:57:24.365 Test pj_strcmp("abdefcghkji", "abdecfghkji")==0 fails (pj_strcmp result=3) in \
             unittest_test.c:563 (wrong test scheduling)
```

A list of the test macros currently implemented are as follows:

- `PJ_TEST_SUCCESS(expr, err_reason, err_action)`
- `PJ_TEST_NON_ZERO(expr, err_reason, err_action)`
- `PJ_TEST_TRUE(expr, err_reason, err_action)`
- `PJ_TEST_NOT_NULL(expr, err_reason, err_action)`
- `PJ_TEST_EQ(expr0, expr1, err_reason, err_action)`
- `PJ_TEST_NEQ(expr0, expr1, err_reason, err_action)`
- `PJ_TEST_LT(expr0, expr1, err_reason, err_action)`
- `PJ_TEST_LTE(expr0, expr1, err_reason, err_action)`
- `PJ_TEST_GT(expr0, expr1, err_reason, err_action)`
- `PJ_TEST_GTE(expr0, expr1, err_reason, err_action)`
- `PJ_TEST_STRCMP(ps0, ps1, res_op, exp_result, err_reason, err_action)`
- `PJ_TEST_STRICMP(ps0, ps1, res_op, exp_result, err_reason, err_action)`
- `PJ_TEST_STREQ(ps0, ps1, err_reason, err_action)`
- `PJ_TEST_STRNEQ(ps0, ps1, err_reason, err_action)`


The second part is the unit test framework. The framework uses common architecture as described in https://en.wikipedia.org/wiki/XUnit. The global workflow to create/run unit test is something like this:

```
   pj_test_case tc0, tc1;
   pj_test_suite suite;
   pj_test_runner *runner;
   pj_test_stat stat;

   /* Init test suite */
   pj_test_suite_init(&suite);

   /* Init test cases */
   pj_test_case_init(&tc0, ..., &func_to_test0, arg0, ... );
   pj_test_suite_add_case(&suite, &tc0);

   pj_test_case_init(&tc1, ..., &func_to_test1, arg1, ... );
   pj_test_suite_add_case(&suite, &tc1);

   /* Create runner and run the test suite */
   pj_test_create_text_runner(pool, .., &runner);
   pj_test_run(runner, &suite);
   pj_test_runner_destroy(runner);

   /* Get/show stats, display logs for failed tests */
   pj_test_get_stat(&suite, &stat);
   pj_test_display_stat(&stat, ..);
   pj_test_display_log_messages(&suite, PJ_TEST_FAILED_TESTS);
```

#### Features

Below are the features of the unit-test and also other enhancements done in this PR:

- Familiarity with common architecture as described in https://en.wikipedia.org/wiki/XUnit
- By default uses parallel execution unless it is disabled on per test case basis.
- Easy to port existing test functions. In fact, no modification is needed to the test functions
- Convenient `PJ_TEST_XXX()` macros can be used in place of manual testing and error reporting
- Nicer output:

```
07:34:32.878 Performing 20 features tests with 4 worker threads
[ 1/20] hash_test                        [OK] [0.000s]
[ 2/20] atomic_test                      [OK] [0.000s]
[ 3/20] rand_test                        [OK] [0.002s]
...
```

- Even nicer output, logging is captured and by default only displayed if the test fails (configurable by cmdline option)
- All (C based) PJ unit testing apps can select test(s) to invoke from cmdline

### 2. Modifications to test apps

#### Console front-end

The main front-end (`main.c`) was modified to be more nice as command line apps, now it can be invoked with arguments:

```
Usage:
  pjlib-test [OPTION] [test_to_run] [..]

where OPTIONS:

  -h, --help   Show this help screen
  -l 0,1,2     0: Don't show logging after tests
               1: Show logs of failed tests (default)
               2: Show logs of all tests
  -w N         Set N worker threads (0: disable. Default: 1)
  -L, --list   List the tests and exit
  --stop-err   Stop testing on error
  --skip-e     Skip essential tests
  --ci-mode    Running in slow CI  mode
  -i           Ask ENTER before quitting
  -n           Do not trap signals
  -p PORT      Use port PORT for echo port
  -s SERVER    Use SERVER as ech oserver
  -t ucp,tcp   Set echo socket type to UDP or TCP
```

Other test apps have been modified to produce similar look.


#### Test body

The main modification in test body (`test.c`) is to use the unit-test framework.

#### Test modifications

Some tests (mostly in pjlib-test) was modified, replacing manual checks with `PJ_TEST_XXX()` macros. This is done to test the usage of `PJ_TEST_XXX()` macros and to make the test nicer. But since it made the PR very big, I didn't continue the effort.

Some tests were also split up to make them run in parallel.

#### Speed result: PJLIB-TEST

In PJLIB-TEST, test time is speed up by up to four times:

- 0 worker thread: 9m22.228s (resembles the original file)
- 1 worker thread: 4m51.940s
- 2 worker threads: 4m9.322s
- 3 worker threads: 2m52.350s
- 4 worker threads: 2m31.399s
- 6 worker threads: 2m28.450s
- 8 worker threads: 2m19.779s

It looks like the sweet spot is with 3 worker threads. Runing with more than this did not speed up the test considerably because some tests just take a long time to finish (more than 2 minutes). I've tried to split up those tests into smaller tests, but mostly that's not feasible because the tests are benchmarking tests (that need to gather all results to determine the overall winner), or because the tests shares global states with each other.

#### Speed result: PJLIB-UTIL-TEST

In PJLIB-UTIL-TEST, there is almost 2x speed up from 5m52.500s to 3m3.615s with 1 worker thread (the default). We couldn't speed up more because tests such as `resolver_test()` and `http_client_test()` takes about three minutes to complete and they couldn't be split up due to the use of global states.

#### Speed result: PJNATH-TEST

The original version took 45m42.275s to complete, excluding `ice_conc_test()` which apparently is not called (this test alone takes 123s to complete). Parallelizing the test requires large modifications as follows:

- remove global `mem` pool factory altogether, since the tests validate the memory leak in the pool factory, therefore having a single pool factory shared by multiple threads will not work
- remove static constants (such as server port number) in `server.c` so that server can be instantiated multiple times simultaneously.
- split tests with multiple configurations (such as `ice_test`, `turn_sock_test`, `concur_test`) into individual test for each configuration, making them parallelable.

As the result, there are 70 test items in pjnath.test. The test durations are as follows:

- 3 worker threads: 11m51.526s
- 4 worker threads: 9m44.503s
- 10 worker threads: 4m31.195s
- 40 worker threads: 2m9.205s

Hence with 10 worker threads, we can save 40 minutes of test time!


### 3. Other developments

#### `<pj/argparse.h>`

This is header only feature to parse command line options. We have `pj_getopt()`, but unfortunately this is in `pjlib-util` thus can't be used by PJLIB.

#### Working `fifobuf`

The `fifobuf` feature has been there for the longest time (it was part of pjlib first ever commit) and finally has got some use.


### 4. Known issues and considerations

### Cluttered logging

When a unit-test is run (in any thread), it will change (globally) the log writer function to an internal unittest log writer function (in order to capture the log). This has some implications:

1. If apps change the log writer before running the unittest, this should be okay. The unittest will restore the log writer to any writer prior to it being run, and will use that writer to display the logs after it is run.
2. If apps change the log writer somewhere inside a test function, I think this should be okay as long as it restores back the writer. 
3. if another thread (that is not aware about testing) is writing to the log, then the unittest log writer will pass that log message to pj_log_write() (i.e. the "official" log writer). I think this is okay and it is the desired behavior, but it will clutter the test output.

### Other

1. Just rationale: in `PJ_TEST_XXX()` macros, we use `{ }` instead of `do {} while (0)` block to support user putting "`break`" as action.

