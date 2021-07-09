This is an attempt at a Ruby wrapper for PJSUA2. It mostly works, but I can't get the last few things fixed. Perhaps somebody else can continue the work.

Problems:
- The Makefile could do with some cleaning up. And it should build as a Ruby gem rather than just copy to the lib directory.
- Scripts crash when they call Ruby's sleep() with "stack level too deep (SystemStackError)". With Ruby 2.0, the commented out line near the bottom of
  SIP.rb in the sample fixes it. But that workaround doesn't help on newer Ruby versions (2.5, 2.6, 2.7). If I comment out the contents of stack_check()
  in VM_eval.c in the Ruby source code, then it appears to work. But I'm not sure whether that is just hiding the problem.
- The scripts crash on terminations with "pjsua_call_set_user_data: Assertion `call_id>=0 && call_id<(int)pjsua_var.ua_cfg.max_calls' failed". This is
  because the destructor of Pjsua2::Call is called after the library has been terminated with Pjsua2::Endpoint::libDestroy(). Commenting out that assertion
  in the PJSUA code causes it to work correctly. I've tried various things to get the Call destuctors to be called earlier but haven't been successful.
  I've also tried not calling libDestroy() at all, but the destructor of Enpoint calls it anyway so that doesn't help. At this point I'm giving up.

Good luck to anybody trying this. Please contact me if you get it working.

Graham Menhennitt
graham+pjsua@menhennitt.com.au
2021-04-12
