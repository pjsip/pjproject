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
using VoipTasks.BackgroundOperations;
using Windows.ApplicationModel.AppService;
using Windows.Foundation.Collections;
using Windows.ApplicationModel.Core;
using Windows.UI.Core;

namespace VoipUI.Helpers
{
    class AppServiceHelper
    {
        ~AppServiceHelper()
        {
            if (_appConnection != null)
            {
                _appConnection.Dispose();
                _appConnection = null;
            }
        }

        public async Task<ValueSet> SendMessageAsync(ValueSet message)
        {
            ValueSet returnValue = null;
            AppServiceConnection appConnection = await GetAppConnectionAsync();

            if (appConnection != null)
            {
                AppServiceResponse response = await appConnection.SendMessageAsync(message);

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

        public async void SendMessage(ValueSet message)
        {
            AppServiceConnection appConnection = await GetAppConnectionAsync();

            if (appConnection != null)
            {
                await appConnection.SendMessageAsync(message);
            }
        }

        private async Task<AppServiceConnection> GetAppConnectionAsync()
        {
            AppServiceConnection appConnection = _appConnection;

            if (appConnection == null)
            {
                appConnection = new AppServiceConnection();

                appConnection.ServiceClosed += AppConnection_ServiceClosed;

                appConnection.AppServiceName = BackgroundOperation.AppServiceName;

                appConnection.PackageFamilyName = Windows.ApplicationModel.Package.Current.Id.FamilyName;

                AppServiceConnectionStatus status = await appConnection.OpenAsync();

                if (status == AppServiceConnectionStatus.Success)
                {
                    _appConnection = appConnection;
                    _appConnection.RequestReceived += Connection_RequestReceived;
                }
            }

            return appConnection;
        }

        private void AppConnection_ServiceClosed(AppServiceConnection sender, AppServiceClosedEventArgs args)
        {
            _appConnection = null;
        }

        private async void Connection_RequestReceived(AppServiceConnection sender, AppServiceRequestReceivedEventArgs args)
        {
            var deferral = args.GetDeferral();
            var response = new ValueSet();
            //bool stop = false;
            try
            {
                var request = args.Request;
                var message = request.Message;
                if (message.ContainsKey(ForegroundOperation.NewForegroundRequest))
                {
                    switch ((ForegroundReguest)message[ForegroundOperation.NewForegroundRequest])
                    {
                        case ForegroundReguest.UpdateCallState:
                            AppRequest = args.Request;
                            Request = ForegroundReguest.UpdateCallState;
                            AppRequestDeferal = deferral;

                            await CoreApplication.MainView.CoreWindow.Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () => {

                                MainPage.Current.UpdateCallState(message[UpdateCallStateArguments.CallState.ToString()] as String);
                            });
                            
                            break;

                        case ForegroundReguest.UpdateRegState:
                            AppRequest = args.Request;
                            Request = ForegroundReguest.UpdateRegState;
                            AppRequestDeferal = deferral;

                            await CoreApplication.MainView.CoreWindow.Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () => {

                                MainPage.Current.UpdateRegState(message[UpdateRegStateArguments.RegState.ToString()] as String);
                            });

                            break;

                        case ForegroundReguest.UpdateAcccountInfo:
                            AppRequest = args.Request;
                            Request = ForegroundReguest.UpdateAcccountInfo;
                            AppRequestDeferal = deferral;

                            await CoreApplication.MainView.CoreWindow.Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () => {

                                MainPage.Current.UpdateAccountInfo(message[UpdateAccountInfoArguments.id.ToString()] as String,
                                                                   message[UpdateAccountInfoArguments.registrar.ToString()] as String,
                                                                   message[UpdateAccountInfoArguments.proxy.ToString()] as String,
                                                                   message[UpdateAccountInfoArguments.username.ToString()] as String,
                                                                   message[UpdateAccountInfoArguments.password.ToString()] as String);
                            });

                            break;

                        default:
                            break;
                    }
                }
            }
            finally
            {
            }
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

        public static ForegroundReguest Request
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

        private AppServiceConnection _appConnection = null;

        private static AppServiceRequest _appRequest = null;
        private static AppServiceDeferral _appDeferral = null;
        private static ForegroundReguest _request = ForegroundReguest.InValid;
        private static Object _lock = new Object();
    }
}
