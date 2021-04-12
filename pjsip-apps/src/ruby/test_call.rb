require_relative 'SIP'

sip = SIP::SIP.new("127.0.0.1", 15061)

account = SIP::Account.new(sip, "127.0.0.1", 901, "secret")
sip.register(account)
sip.idle 2

call = sip.call(account, "sip:902@127.0.0.1") # establish call
sip.idle 10  # let it sit idle for a while
sip.hangup(call)    # and hang it up

# shut down
account.shutdown
sip.idle 1
sip.close
