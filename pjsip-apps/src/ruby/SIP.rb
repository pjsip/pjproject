require 'pjsua2'
require_relative 'Account'
require_relative 'Call'

module SIP
    class SIP
        attr_reader :registrar

        def initialize(registrar, port, callProcessor = nil)
            @registrar, @callProcessor = registrar, callProcessor

            # create and initialise library
            @endpointConfig = Pjsua2::EpConfig.new
            @endpointConfig.logConfig.level = 4
            @endpointConfig.logConfig.msgLogging = 0
            #@endpointConfig.uaConfig.mainThreadOnly = true
            @endpoint = Pjsua2::Endpoint.new
            @endpoint.libCreate
            @endpoint.libInit(@endpointConfig)
            @endpoint.audDevManager().setNullDev()  # don't use an audio device

            # create SIP transport
            @sipTransportConfig = Pjsua2::TransportConfig.new
            @sipTransportConfig.port = port
            @endpoint.transportCreate(Pjsua2::PJSIP_TRANSPORT_UDP, @sipTransportConfig)

            # start library
            @endpoint.libStart

            @calls = []
        end

        def registerThread
            @endpoint.libRegisterThread("thread") unless @endpoint.libIsThreadRegistered
        end

        def close
            @calls.each { |call| hangup(call) }    # hang up all calls

            # shut down library
            @endpoint.libDestroy
        end

        # register a SIP account
        def register(account)
            account.create
        end

        # unregister a SIP account
        def unregister(account)
            account.shutdown
        end

        # make an outgoing call
        def call(account, uri)
            call = Call.new(self, account)
            callOpParam = Pjsua2::CallOpParam.new(true)
            #callOpParam.opt.audioCount = 0 # no audio in call
            callOpParam.opt.videoCount = 0  # or video
            call.makeCall(uri, callOpParam)
            return call
        end

        # hang up a call
        def hangup(call)
            callOpParam = Pjsua2::CallOpParam.new(true)
            call.hangup(callOpParam)
        end

        # answer an incoming call
        def answer(call)
            answerParam = Pjsua2::CallOpParam.new
            answerParam.statusCode = Pjsua2::PJSIP_SC_OK
            #answerParam.opt.audioCount = 0 # no audio in call
            answerParam.opt.videoCount = 0  # or video
            call.answer(answerParam)
        end

        def onRegistrationState(account, registrationState)
            # do nothing (for now)
        end

        def onIncomingCall(account, incomingCallParam)
            call = Call.new(self, account, incomingCallParam.callId)
            @calls << call
            if @callProcessor.nil?
                answer(call)    # nobody to tell about it - just answer it ourselves
            else
                @callProcessor.incomingCallReceived(call)   # let the call processor answer it
            end
        end

        def onCallState(call, callState)
            if call.getId() == Pjsua2::PJSUA_INVALID_ID
                # this shouldn't happen, but it does because the call id is cleared in PJSUA before this callback occurs
                @callProcessor.disconnected(call) unless @callProcessor.nil? # pretend that we got a disconnect
                @calls.delete(call) # it's no longer a valid call
                return
            end

            @calls.delete(call) if call.getInfo().state == Pjsua2::PJSIP_INV_STATE_DISCONNECTED # call no longer exists

            return if @callProcessor.nil?   # do nothing if nobody is interested
            case call.getInfo().state
              # implement any of these as required
              #when Pjsua2::PJSIP_INV_STATE_NULL then
              #when Pjsua2::PJSIP_INV_STATE_CALLING then
              when Pjsua2::PJSIP_INV_STATE_INCOMING then @callProcessor.incoming(call, call.getinfo().remoteUri)
              #when Pjsua2::PJSIP_INV_STATE_EARLY then
              #when Pjsua2::PJSIP_INV_STATE_CONNECTING then
              #when Pjsua2::PJSIP_INV_STATE_CONFIRMED then
              when Pjsua2::PJSIP_INV_STATE_DISCONNECTED then @callProcessor.disconnected(call)
            end
        end

        def idle(secs)
            #(secs * 1000 / 10).times { @endpoint.libHandleEvents(10) }
            sleep secs
        end
    end
end
