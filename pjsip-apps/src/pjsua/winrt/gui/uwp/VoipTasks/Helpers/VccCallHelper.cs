//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
using System;
using System.Diagnostics;
using System.Threading.Tasks;
using VoipTasks.BackgroundOperations;
using Windows.ApplicationModel.Calls;
using Windows.Foundation.Collections;
using Windows.Foundation.Metadata;
using VoipBackEnd;

namespace VoipTasks.Helpers
{
    class VccCallHelper
    {
        private int RTcTaskAlreadyRuningErrorCode = -2147024713;
        public async Task<VoipPhoneCallResourceReservationStatus> RequestNewCallAsync(string dstURI)
        {
            VoipPhoneCallResourceReservationStatus status = VoipPhoneCallResourceReservationStatus.ResourcesNotAvailable;
            
            status = await LaunchRTCTaskAsync();
            if (status == VoipPhoneCallResourceReservationStatus.Success)
            {
                NewOutgoingCall(dstURI);
            }

            Current.Request = BackgroundRequest.InValid;

            return status;
        }

        private async Task<VoipPhoneCallResourceReservationStatus> LaunchRTCTaskAsync()
        {
            // End current call before starting another call, there should be only one RTC task active at a time.
            // Duplicate calls to launch RTC task will result in HR ERROR_ALREADY_EXSIST
            // <TODO> For multiple calls against single rtc task add logic to verify that the rtc is not completed, 
            // and then Skip launching new rtc task
            Current.EndCall();

            VoipCallCoordinator vCC = VoipCallCoordinator.GetDefault();
            VoipPhoneCallResourceReservationStatus status = VoipPhoneCallResourceReservationStatus.ResourcesNotAvailable;

            try
            {
                status = await vCC.ReserveCallResourcesAsync(Current.RtcCallTaskName);
            }
            catch (Exception ex)
            {
                if (ex.HResult == RTcTaskAlreadyRuningErrorCode )
                {
                    Debug.WriteLine("RTC Task Already running");
                }
            }

            return status;
        }

        internal void NewOutgoingCall(string dstURI)
        {
            bool status = false;
            try
            {
                VoipCallCoordinator vCC = VoipCallCoordinator.GetDefault();
                VoipPhoneCall call = vCC.RequestNewOutgoingCall( "Pjsua Test Call ", dstURI, "", VoipPhoneCallMedia.Audio);
                if (call != null)
                {
                    call.EndRequested += Call_EndRequested;
                    call.RejectRequested += Call_RejectRequested;

                    call.NotifyCallActive();

                    Current.VoipCall = call;

                    MyAppRT.Instance.makeCall(dstURI);

                    status = true;
                }
            }
            catch (Exception e)
            {
                Debug.WriteLine(e.ToString());
            }

            ValueSet response = new ValueSet();
            response[BackgroundOperation.Result] = status ? (int)OperationResult.Succeeded : (int)OperationResult.Failed;

            Current.SendResponse(response);
        }

        internal void NewIncomingCall(string context, string contactName, string serviceName)
        {
            try
            {
                VoipCallCoordinator vCC = VoipCallCoordinator.GetDefault();
                VoipPhoneCall call = vCC.RequestNewIncomingCall(
                                                                "Hello",
                                                                contactName,
                                                                context,
                                                                new Uri("file://c:/data/test/bin/FakeVoipAppLight.png"),
                                                                serviceName,
                                                                new Uri("file://c:/data/test/bin/FakeVoipAppLight.png"),
                                                                "",
                                                               new Uri("file://c:/data/test/bin/FakeVoipAppRingtone.wma"),
                                                                VoipPhoneCallMedia.Audio,
                                                                new TimeSpan(0, 1, 20));
                if (call != null)
                {
                    call.AnswerRequested += Call_AnswerRequested;
                    call.EndRequested += Call_EndRequested;
                    call.RejectRequested += Call_RejectRequested;

                    Current.VoipCall = call;
                }
            }
            catch (Exception e)
            {
                Debug.WriteLine(e.ToString());
            }
        }

        private void Call_RejectRequested(VoipPhoneCall sender, CallRejectEventArgs args)
        {
            Current.EndCall();
        }

        private void Call_EndRequested(VoipPhoneCall sender, CallStateChangeEventArgs args)
        {
            Current.EndCall();
        }

        private void Call_AnswerRequested(VoipPhoneCall sender, CallAnswerEventArgs args)
        {            
            CallOpParamRT param = new CallOpParamRT();
            param.statusCode = 200;
            param.reason = "OK";

            try
            {
                Current.MyApp.answerCall(param);
            }
            catch (Exception ex)
            {                
                Current.MyApp.writeLog(2, ex.Message);
            }
            Current.VoipCall = sender;
            try
            {
                Current.VoipCall.NotifyCallActive();
            }
            catch (Exception ex)
            {                
                Current.MyApp.writeLog(2, ex.Message);
            }            
        }
    }
}
