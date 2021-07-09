module SIP
    class Account < Pjsua2::Account
        def initialize(owner, address, number, secret)
            super()

            @owner = owner

            @config = Pjsua2::AccountConfig.new
            @config.idUri = "sip:#{number}@#{owner.registrar}"
            @config.regConfig.registrarUri = "sip:#{owner.registrar}"
            @credential = Pjsua2::AuthCredInfo.new("digest", "*", "test", 0, secret)    # not sure of the lifetime of this - better to be safe
            @config.sipConfig.authCreds << @credential
        end

        def onRegState(regState)
            @owner.onRegistrationState(self, regState)
        end

        def onIncomingCall(incomingCallParam)
            @owner.onIncomingCall(self, incomingCallParam)
        end

        def create()    # override the base class
            super(@config)
        end
    end
end
