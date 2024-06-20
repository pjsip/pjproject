(Work in progress, do not review etc)

This PR is part one of two to speed up testing in PJSIP. This part attempts to speed up the C/C++ based tests and part two will address Python based testing framework.

The plan is to implement unit testing framework in PJLIB, that that is what the bulk of this PR does. More specifically, this PR contains several work:

1. A (new) unit testing framework, in `<pj/unittest.h>` and `pj/unittest.c`.
2. largish modifications to `pjlib-test`, `pjlib-util-test`, `pjnath-test`, `pjmedia-test`, and `pjsip-test`.
3. minor developments:

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

- Supports parallel execution on per test case basis, resulting in up to four times speed up in PJLIB-TEST.
- Familiarity with common architecture as described in https://en.wikipedia.org/wiki/XUnit
- Easy to port existing test functions. In fact, no modification is needed to the test functions
- PJ_TEST_XXX() macros can be used in place of manual testing
- All (C based) PJ unit testing apps have nicer output:

```
07:34:32.878 Performing 20 features tests with 4 worker threads
[ 1/20] hash_test                        [OK] [0.000s]
[ 2/20] atomic_test                      [OK] [0.000s]
[ 3/20] rand_test                        [OK] [0.002s]
...
```

- Even nicer output, logging emited during test is captured and by default only displayed if the test fails (or can be displayed all the time, configurable by cmdline option)
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

The main test body (`test.c`) was modified to use the unit-test framework, resulting in nicer output:

```
07:34:32.878 Performing 20 features tests with 4 worker threads
[ 1/20] hash_test                        [OK] [0.000s]
[ 2/20] atomic_test                      [OK] [0.000s]
[ 3/20] rand_test                        [OK] [0.002s]
[ 4/20] pool_perf_test                   [OK] [0.003s]
[ 5/20] sock_test                        [OK] [0.003s]
[ 6/20] rbtree_test                      [Err: -40] [0.007s]
[ 7/20] select_test                      [OK] [0.010s]
...
...
...
[19/20] ioqueue_perf_test1               [OK] [110.046s]
[20/20] ioqueue_stress_test              [OK] [132.287s]
07:37:02.192 Unit test statistics for features tests:
07:37:02.192     Total number of tests: 20
07:37:02.192     Number of test run:    20
07:37:02.192     Number of failed test: 1
07:37:02.192     Total duration:        2m29.313s
07:37:02.192 ------------ Displaying failed test logs: ------------
07:37:02.192 Logs for rbtree_test [rc:-40]:
07:34:32.885 Error: .....
07:37:02.192 --------------------------------------------------------
07:37:02.192  
07:37:02.192 Stack max usage: 0, deepest: :0
07:37:02.192 **Test completed with error(s)**
```

#### Speed result: PJLIB-TEST

In PJLIB-TEST, test time is speed up by up to four times:

- 0 worker thread: 9m22.228s (resembles the original file)
- 1 worker thread: 4m51.940s
- 2 worker threads: 4m9.322s
- 3 worker threads: 2m52.350s
- 4 worker threads: 2m31.399s
- 6 worker threads: 2m28.450s
- 8 worker threads: 2m19.779s

It looks like the sweet spot is with 3 worker threads. This is because some tests takes quite a long time t o finish, so using many threads won't help these tests:

```
[14/20] udp_ioqueue_unreg_test           [OK] [92.759s]
[15/20] ioqueue_stress_test              [OK] [111.061s]
[17/20] ioqueue_perf_test1               [OK] [110.149s]
[18/20] activesock_test                  [OK] [101.890s]
```

I've tried to split up those tests into smaller tests, but mostly that's not feasible because the tests are benchmarking tests (that need to gather all results to determine the overall winner).

#### Speed result: PJLIB-UTIL-TEST

In PJLIB-UTIL-TEST, there is about 2x speed up from 5m52.500s to 3m3.615s. We couldn't achieve more speed up due to long running tests such as resolver_test() and http_client_test() which couldn't be broken up due to the use of global states.


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

