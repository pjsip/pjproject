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
using VoipTasks.BackgroundOperations;
using VoipTasks.Helpers;
using Windows.ApplicationModel.AppService;
using Windows.ApplicationModel.Background;
using Windows.ApplicationModel.Calls;
using Windows.Foundation.Collections;
using VoipBackEnd;

namespace VoipTasks
{
    public sealed class AppService : IBackgroundTask
    {
        AppServiceConnection _connection;
        BackgroundTaskDeferral _deferral;

        public void Run(IBackgroundTaskInstance taskInstance)
        {
            AppServiceTriggerDetails triggerDetail = taskInstance.TriggerDetails as AppServiceTriggerDetails;
            _deferral = taskInstance.GetDeferral();

            // Register for Task Cancel callback
            taskInstance.Canceled += TaskInstance_Canceled;

            AppServiceConnection connection = triggerDetail.AppServiceConnection;
            _connection = connection;

            connection.RequestReceived += Connection_RequestReceived;
            Current.AppConnection = connection;
        }

        private async void Connection_RequestReceived(AppServiceConnection sender, AppServiceRequestReceivedEventArgs args)
        {
            var deferral = args.GetDeferral();
            var response = new ValueSet();
            bool stop = false;
            try
            {
                var request = args.Request;
                var message = request.Message;
                if (message.ContainsKey(BackgroundOperation.NewBackgroundRequest))
                {
                    switch ((BackgroundRequest)message[BackgroundOperation.NewBackgroundRequest])
                    {
                        case BackgroundRequest.NewOutgoingCall:
                            Current.AppRequest = args.Request;
                            Current.Request = BackgroundRequest.NewOutgoingCall;
                            Current.AppRequestDeferal = deferral;
                            await Current.RequestNewCallAsync(message[NewCallArguments.DstURI.ToString()] as String);
                            break;

                        case BackgroundRequest.EndCall:
                            Current.AppRequest = args.Request;
                            Current.Request = BackgroundRequest.EndCall;
                            Current.AppRequestDeferal = deferral;
                            Current.EndCallAsync();
                            break;

                        case BackgroundRequest.StartService:
                            Current.AppRequest = args.Request;
                            Current.Request = BackgroundRequest.StartService;
                            Current.AppRequestDeferal = deferral;
                            Current.StartService();                            

                            break;
                        case BackgroundRequest.StopService:
                            Current.AppRequest = args.Request;
                            Current.Request = BackgroundRequest.StopService;
                            Current.AppRequestDeferal = deferral;
                            Current.StopService();
                            break;

                        case BackgroundRequest.GetAccountInfo:
                            Current.AppRequest = args.Request;
                            Current.Request = BackgroundRequest.GetAccountInfo;
                            Current.AppRequestDeferal = deferral;

                            Current.GetAccountInfo();
                            break;

                        case BackgroundRequest.ModifyAccount:
                            Current.AppRequest = args.Request;
                            Current.Request = BackgroundRequest.ModifyAccount;
                            Current.AppRequestDeferal = deferral;
                            Current.ModifyAccount(message[ModifyAccountArguments.id.ToString()] as String,
                                    message[ModifyAccountArguments.registrar.ToString()] as String,
                                    message[ModifyAccountArguments.proxy.ToString()] as String,
                                    message[ModifyAccountArguments.username.ToString()] as String,
                                    message[ModifyAccountArguments.password.ToString()] as String);

                            break;

                        default:
                            stop = true;
                            break;
                    }
                }
            }
            finally
            {
               
                if (stop)
                {
                    _deferral.Complete();
                }
            }
        }

        private void TaskInstance_Canceled(IBackgroundTaskInstance sender, BackgroundTaskCancellationReason reason)
        {
            if (_deferral != null)
            {
                _deferral.Complete();
            }
        }

        private static void Call_ResumeRequested(VoipPhoneCall sender, CallStateChangeEventArgs args)
        {
            sender.NotifyCallActive();
        }

        private static void Call_RejectRequested(VoipPhoneCall sender, CallRejectEventArgs args)
        {
            sender.NotifyCallEnded();
        }

        private static void Call_HoldRequested(VoipPhoneCall sender, CallStateChangeEventArgs args)
        {
            sender.NotifyCallHeld();
        }

        private static void Call_EndRequested(VoipPhoneCall sender, CallStateChangeEventArgs args)
        {
            sender.NotifyCallEnded();
        }

        private static void Call_AnswerRequested(VoipPhoneCall sender, CallAnswerEventArgs args)
        {
            sender.NotifyCallActive();
        }
    }
}
