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
using VoipUI.Helpers;
using VoipTasks.BackgroundOperations;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using System.Diagnostics;
using VoipBackEnd;
using System.Threading;
using Windows.ApplicationModel.AppService;
using Windows.UI.Xaml.Navigation;

namespace VoipUI
{
    public sealed partial class MainPage : Page
    {
        //App developer should include the logic to prevent the user from being
        //able to call the Start Call async method multiple times. This is out
        //of the scope of this sample.
        private int MethodCallUnexpectedTime = -2147483634;

        public static string callerName, callerNumber;
        public static MainPage Current;        

        public MainPage()
        {
            this.InitializeComponent();
            Current = this;
        }

        private async void GetAccountInfoAsync()
        {
            try
            {
                OperationResult result = await EndpointHelper.GetAccountInfo();
            }
            catch (Exception ex)
            {
                Debug.WriteLine(ex.Message);
            }
        }

        private async void ModifyAccountAsync()
        {
            try
            {
                OperationResult result = await EndpointHelper.ModifyAccount(
                                           txt_UserId.Text, 
                                           txt_Registrar.Text, 
                                           txt_Proxy.Text, 
                                           txt_RegUserName.Text, 
                                           txt_Password.Text);
            }
            catch (Exception ex)
            {
                Debug.WriteLine(ex.Message);
            }
        }

        private async void NewOutgoingCallAsync()
        {
            try
            {
                OperationResult result = await VoipCallHelper.NewOutgoingCallAsync(txt_CallerNumber.Text);
            }
            catch (Exception ex)
            {
                Debug.WriteLine(ex.Message);
            }
        }

        private void EndCall()
        {
            try
            {
                OperationResult result = VoipCallHelper.EndCallAsync();
            }
            catch (Exception ex)
            {
                Debug.WriteLine(ex.Message);
            }
            txt_CallStatus.Text = "Disconnected";
        }

        public void UpdateAccountInfo(String id, String registrar, String proxy,
                                      String username, String password)
        {
            txt_UserId.Text = id;
            txt_Registrar.Text = registrar;
            txt_Proxy.Text = proxy;
            txt_RegUserName.Text = username;
            txt_Password.Text = password;
        }

        private void btn_NewOutgoingCall_Click(object sender, RoutedEventArgs e)
        {
            NewOutgoingCallAsync();
        }


        private void btn_EndCall_Click(object sender, RoutedEventArgs e)
        {
            EndCall();
        }

        public void UpdateCallState(String state)
        {
            txt_CallStatus.Text = state;            
        }

        public void UpdateRegState(String state)
        {
            txt_RegState.Text = "Reg:" + state;
        }

        private void btn_GetAccountInfo_Click(object sender, RoutedEventArgs e)
        {
            GetAccountInfoAsync();
        }

        private void btn_ModifyAccount_Click(object sender, RoutedEventArgs e)
        {
            ModifyAccountAsync();
        }
    }
}