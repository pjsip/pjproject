(Work in progress, do not review etc)

### Features

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

- 0 thread: 6m57.421s
- 1 thread: 4m3.933s
- 2 threads: 2m18.979s (error in rbtree_test:102)
- 4 threads: 2m16.407s (error in rbtree_test:102)

Auxiliary features:

- pj/argparse.h
