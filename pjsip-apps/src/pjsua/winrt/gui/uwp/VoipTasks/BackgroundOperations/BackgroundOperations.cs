﻿//*********************************************************
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

namespace VoipTasks.BackgroundOperations
{
    public enum BackgroundRequest
    {
        NewOutgoingCall,
        EndCall,
        GetCallDuration,
        StartVideo,
        EndVideo,

        //Account request
        GetAccountInfo,
        ModifyAccount,

        //Endpoint request
        StartService,
        StopService,

        // Always keep this as the last option
        InValid
    }

    public enum ForegroundReguest
    {
        UpdateCallState,
        UpdateAcccountInfo,
        UpdateRegState,
        InValid
    }

    public enum NewCallArguments
    {
        DstURI
    }

    public enum ModifyAccountArguments
    {
        id,
        registrar,
        proxy,
        username,
        password
    }

    public enum UpdateCallStateArguments
    {
        CallState
    }

    public enum UpdateRegStateArguments
    {
        RegState
    }

    public enum UpdateAccountInfoArguments
    {
        id,
        registrar,
        proxy,
        username,
        password
    }

    public enum OperationResult
    {
        Succeeded,
        Failed
    }

    public static class BackgroundOperation
    {
        public static String AppServiceName
        {
            get { return _appServiceName; }
        }

        public static String NewBackgroundRequest
        {
            get { return _newBackgroundRequest; }
        }

        public static String Result
        {
            get { return _result; }
        }

        const String _appServiceName = "VoipTasks.AppService";
        const String _newBackgroundRequest = "NewBackgroundRequest";
        const String _result = "Result";
    }

    public static class ForegroundOperation
    {
        public static String NewForegroundRequest
        {
            get { return _newForegroundRequest; }
        }
        const String _newForegroundRequest = "NewForegroundRequest";
    }
}
