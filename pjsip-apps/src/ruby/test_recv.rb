require_relative 'SIP'

sip = SIP::SIP.new("127.0.0.1", 15062)

account = SIP::Account.new(sip, "127.0.0.1", 902, "secret")
sip.register(account)
sip.idle 100    # wait for incoming calls

# shut down
account.shutdown
sip.idle 1
sip.close
