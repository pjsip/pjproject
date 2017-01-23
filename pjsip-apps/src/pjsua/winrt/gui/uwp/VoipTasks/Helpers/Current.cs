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
using System.Threading.Tasks;
using System.Diagnostics;
using VoipTasks.BackgroundOperations;
using Windows.ApplicationModel.AppService;
using Windows.ApplicationModel.Background;
using Windows.ApplicationModel.Calls;
using Windows.Foundation.Collections;
using VoipBackEnd;

namespace VoipTasks.Helpers
{
    static class Current
    {
        public static String RtcCallTaskName
        {
            get { return _rtcCallTaskName; }
        }

        public static AppServiceRequest AppRequest
        {
            set
            {
                lock (_lock)
                {
                    _appRequest = value;
                }
            }
            get
            {
                lock (_lock)
                {
                    return _appRequest;
                }
            }
        }

        public static AppServiceDeferral AppRequestDeferal
        {
            set
            {
                lock (_lock)
                {
                    _appDeferral = value;
                }
            }
            get
            {
                lock (_lock)
                {
                    return _appDeferral;
                }
            }
        }

        public static BackgroundTaskDeferral RTCTaskDeferral
        {
            set
            {
                lock (_lock)
                {
                    _rtcTaskDeferral = value;
                }
            }
            get
            {
                lock (_lock)
                {
                    return _rtcTaskDeferral;
                }
            }
        }

        public static VoipPhoneCall VoipCall
        {
            set
            {
                lock (_lock)
                {
                    _voipCall = value;
                }
            }
            get
            {
                lock (_lock)
                {
                    return _voipCall;
                }
            }
        }

        public static BackgroundRequest Request
        {
            set
            {
                lock (_lock)
                {
                    _request = value;
                }
            }
            get
            {
                lock (_lock)
                {
                    return _request;
                }
            }
        }
        
        public static AppServiceConnection AppConnection
        {
            set
            {
                lock (_lock)
                {
                    _connection = value;
                }
            }
            get
            {
                lock (_lock)
                {
                    return _connection;
                }
            }
        }

        public static MyAppRT MyApp
        {
            get
            {
                lock (_lock)
                {
                    return _myApp;
                }
            }
        }

        public static async void SendResponse(ValueSet response)
        {
            AppServiceRequest request = AppRequest;
            AppServiceDeferral deferal = AppRequestDeferal;

            try
            {
                if (request != null)
                {
                    await request.SendResponseAsync(response);
                }
            }
            finally
            {
                if (deferal != null)
                {
                    deferal.Complete();
                    deferal = null;
                }

                AppRequest = null;
            }
        }

        public static async Task<ValueSet> SendMessageAsync(ValueSet message)
        {
            ValueSet returnValue = null;

            if (_connection != null)
            {
                AppServiceResponse response = await _connection.SendMessageAsync(message);

                if (response.Status == AppServiceResponseStatus.Success)
                {
                    if (response.Message.Keys.Contains(BackgroundOperation.Result))
                    {
                        returnValue = response.Message;
                    }
                }
            }

            return returnValue;
        }

        public static async void EndCallAsync()
        {
            AppServiceRequest request = AppRequest;
            AppServiceDeferral deferal = AppRequestDeferal;

            try
            {                
                EndCall();
                
                if (request != null)
                {
                    ValueSet response = new ValueSet();
                    response[BackgroundOperation.Result] = (int)OperationResult.Succeeded;
                    await request.SendResponseAsync(response);
                }

                if (deferal != null)
                {
                    deferal.Complete();
                }
            }
            finally
            {
                AppRequest = null;
                AppRequestDeferal = null;
            }
        }


        public static void EndCall()
        {
            VoipPhoneCall call = VoipCall;
            BackgroundTaskDeferral deferral = RTCTaskDeferral;

            if (_voipCall == null) {
                return;
            }

            try
            {
                _myApp.hangupCall();
            }
            catch (Exception e)
            {
                _myApp.writeLog(2, e.Message);
            }            

            try
            {                
                if (call != null)
                {
                    call.NotifyCallEnded();
                }
            }
            catch
            {

            }

            try
            {
                if (deferral != null)
                {
                    deferral.Complete();
                }

            }
            catch
            {

            }
            finally
            {
                VoipCall = null;
                RTCTaskDeferral = null;
            }
        }

        public static async Task<VoipPhoneCallResourceReservationStatus> RequestNewCallAsync(string dstURI)
        {
            VccCallHelper vccCallHelper;
            VoipPhoneCallResourceReservationStatus status = VoipPhoneCallResourceReservationStatus.ResourcesNotAvailable;

            lock (_lock)
            {
                vccCallHelper = _vccCallHelper;
            }

            if (vccCallHelper != null)
            {
                status = await vccCallHelper.RequestNewCallAsync(dstURI);
            }

            return status;
        }

        public static void RequestNewIncomingCall(string context, string contactName, string serviceName)
        {
            VccCallHelper vccCallHelper;


            lock (_lock)
            {
                vccCallHelper = _vccCallHelper;
            }

            if (vccCallHelper != null)
            {
                vccCallHelper.NewIncomingCall(context, contactName, serviceName);
            }
        }

        public static void StartService()
        {
            if (_started)
                return;

            _myApp.init(_epHelper, _epHelper);

            _started = true;
        }


        public static void StopService()
        {
            if (!_started)
                return;

            _myApp.deInit();

            _started = false;
        }

        public static void ModifyAccount(String id, String registrar,
                                         String proxy, String username, 
                                         String password)
        {
            try
            {
                MyAppRT.Instance.modifyAccount(id, registrar, proxy, username, password);
            }
            catch (Exception e)
            {
                MyAppRT.Instance.writeLog(2, e.Message);
            }            
        }

        async public static void GetAccountInfo()
        {
            if (!_started)
                return;

            AccountInfo accInfo = MyAppRT.Instance.getAccountInfo();

            ValueSet message = new ValueSet();

            message[UpdateAccountInfoArguments.id.ToString()] = accInfo.id;
            message[UpdateAccountInfoArguments.registrar.ToString()] = accInfo.registrar;
            message[UpdateAccountInfoArguments.proxy.ToString()] = accInfo.proxy;
            message[UpdateAccountInfoArguments.username.ToString()] = accInfo.username;
            message[UpdateAccountInfoArguments.password.ToString()] = accInfo.password;

            message[ForegroundOperation.NewForegroundRequest] = (int)ForegroundReguest.UpdateAcccountInfo;

            ValueSet response = await Current.SendMessageAsync(message);

        }

        private const String _rtcCallTaskName = "VoipTasks.CallRtcTask";
        private static BackgroundRequest _request = BackgroundRequest.InValid;
        private static Object _lock = new Object();
        private static AppServiceRequest _appRequest = null;
        private static AppServiceDeferral _appDeferral = null;
        private static VoipPhoneCall _voipCall = null;
        private static VccCallHelper _vccCallHelper = new VccCallHelper();
        private static BackgroundTaskDeferral _rtcTaskDeferral = null;

        /* Pjsua data */
        private static MyAppRT _myApp = MyAppRT.Instance;
        private static bool _started = false;

        private static EndpointHelper _epHelper = new EndpointHelper();
        private static AppServiceConnection _connection = null;
    }
}
