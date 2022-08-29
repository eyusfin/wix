// Copyright (c) .NET Foundation and contributors. All rights reserved. Licensed under the Microsoft Reciprocal License. See LICENSE.TXT file in the project root for full license information.

#include "precomp.h"

// Exit macros
#define ExitOnNetworkError(x) { er = x; if (ERROR_SUCCESS != er) { goto LNExit; }}

/********************************************************************
 * CreateSmb - CUSTOM ACTION ENTRY POINT for creating fileshares
 *
 * Input:  deferred CustomActionData -
 *    wzFsKey\twzShareDesc\twzFullPath\tfIntegratedAuth\twzUserName\tnPermissions\twzUserName\tnPermissions...
 *
 * ****************************************************************/
extern "C" UINT __stdcall CreateSmb(MSIHANDLE hInstall)
{
//AssertSz(0, "debug CreateSmb");
    UINT er = ERROR_SUCCESS;
    HRESULT hr = S_OK;

    LPWSTR pwzData = NULL;
    LPWSTR pwz = NULL;
    LPWSTR pwzFsKey = NULL;
    LPWSTR pwzShareDesc = NULL;
    LPWSTR pwzDirectory = NULL;
    int iAccessMode = 0;
    DWORD nExPermissions = 0;
    BOOL fIntegratedAuth;
    LPWSTR pwzExUser = NULL;
    SCA_SMBP ssp = {0};
    DWORD dwExUserPerms = 0;
    DWORD dwCounter = 0;
    SCA_SMBP_USER_PERMS* pUserPermsList = NULL;

    hr = WcaInitialize(hInstall, "CreateSmb");
    ExitOnFailure(hr, "failed to initialize");

    hr = WcaGetProperty( L"CustomActionData", &pwzData);
    ExitOnFailure(hr, "failed to get CustomActionData");

    WcaLog(LOGMSG_TRACEONLY, "CustomActionData: %ls", pwzData);

    pwz = pwzData;
    hr = WcaReadStringFromCaData(&pwz, &pwzFsKey); // share name
    ExitOnFailure(hr, "failed to read share name");
    hr = WcaReadStringFromCaData(&pwz, &pwzShareDesc); // share description
    ExitOnFailure(hr, "failed to read share name");
    hr = WcaReadStringFromCaData(&pwz, &pwzDirectory); // full path to share
    ExitOnFailure(hr, "failed to read share name");
    hr = WcaReadIntegerFromCaData(&pwz, reinterpret_cast<int *>(&fIntegratedAuth));
    ExitOnFailure(hr, "failed to read integrated authentication");

    hr = WcaReadIntegerFromCaData(&pwz, reinterpret_cast<int *>(&dwExUserPerms));
    ExitOnFailure(hr, "failed to read count of permissions to set");
    if(dwExUserPerms > 0)
    {
        pUserPermsList = static_cast<SCA_SMBP_USER_PERMS*>(MemAlloc(sizeof(SCA_SMBP_USER_PERMS)*dwExUserPerms, TRUE));
        ExitOnNull(pUserPermsList, hr, E_OUTOFMEMORY, "failed to allocate memory for permissions structure");

        //Pull out all of the ExUserPerm strings
        for (dwCounter = 0; dwCounter < dwExUserPerms; ++dwCounter)
        {
            hr = WcaReadStringFromCaData(&pwz, &pwzExUser); // user account
            ExitOnFailure(hr, "failed to read user account");
            pUserPermsList[dwCounter].wzUser = pwzExUser;
            pwzExUser = NULL;

            hr = WcaReadIntegerFromCaData(&pwz, &iAccessMode);
            ExitOnFailure(hr, "failed to read access mode");
            pUserPermsList[dwCounter].accessMode = (ACCESS_MODE)iAccessMode;
            iAccessMode = 0;

            hr = WcaReadIntegerFromCaData(&pwz, reinterpret_cast<int *>(&nExPermissions));
            ExitOnFailure(hr, "failed to read count of permissions");
            pUserPermsList[dwCounter].nPermissions = nExPermissions;
            nExPermissions = 0;
        }
    }

    ssp.wzKey = pwzFsKey;
    ssp.wzDescription = pwzShareDesc;
    ssp.wzDirectory = pwzDirectory;
    ssp.fUseIntegratedAuth = fIntegratedAuth;
    ssp.dwUserPermissionCount = dwExUserPerms;
    ssp.pUserPerms = pUserPermsList;

    hr = ScaEnsureSmbExists(&ssp);
    MessageExitOnFailure(hr, msierrSMBFailedCreate, "failed to create share: '%ls'", pwzFsKey);

    hr = WcaProgressMessage(COST_SMB_CREATESMB, FALSE);

LExit:
    ReleaseStr(pwzFsKey);
    ReleaseStr(pwzShareDesc);
    ReleaseStr(pwzDirectory);
    ReleaseStr(pwzData);

    if (pUserPermsList)
    {
        MemFree(pUserPermsList);
    }

    if (FAILED(hr))
    {
        er = ERROR_INSTALL_FAILURE;
    }
    return WcaFinalize(er);
}



/********************************************************************
 DropSmb - CUSTOM ACTION ENTRY POINT for creating fileshares

 Input:  deferred CustomActionData - wzFsKey\twzShareDesc\twzFullPath\tnPermissions\tfIntegratedAuth\twzUserName\twzPassword

 * ****************************************************************/
extern "C" UINT __stdcall DropSmb(MSIHANDLE hInstall)
{
    //AssertSz(0, "debug DropSmb");
    UINT er = ERROR_SUCCESS;
    HRESULT hr = S_OK;

    LPWSTR pwzData = NULL;
    LPWSTR pwz = NULL;
    LPWSTR pwzFsKey = NULL;
    SCA_SMBP ssp = {0};

    hr = WcaInitialize(hInstall, "DropSmb");
    ExitOnFailure(hr, "failed to initialize");

    hr = WcaGetProperty( L"CustomActionData", &pwzData);
    ExitOnFailure(hr, "failed to get CustomActionData");

    WcaLog(LOGMSG_TRACEONLY, "CustomActionData: %ls", pwzData);

    pwz = pwzData;
    hr = WcaReadStringFromCaData(&pwz, &pwzFsKey); // share name
    ExitOnFailure(hr, "failed to read share name");

    ssp.wzKey = pwzFsKey;

    hr = ScaDropSmb(&ssp);
    MessageExitOnFailure(hr, msierrSMBFailedDrop, "failed to delete share: '%ls'", pwzFsKey);

    hr = WcaProgressMessage(COST_SMB_DROPSMB, FALSE);

LExit:
    ReleaseStr(pwzFsKey);
    ReleaseStr(pwzData);

    if (FAILED(hr))
    {
        er = ERROR_INSTALL_FAILURE;
    }
    return WcaFinalize(er);
}


static HRESULT AddUserToGroup(
    __in LPWSTR wzUser,
    __in LPCWSTR wzUserDomain,
    __in LPCWSTR wzGroup,
    __in LPCWSTR wzGroupDomain
    )
{
    Assert(wzUser && *wzUser && wzUserDomain && wzGroup && *wzGroup && wzGroupDomain);

    HRESULT hr = S_OK;
    IADsGroup *pGroup = NULL;
    BSTR bstrUser = NULL;
    BSTR bstrGroup = NULL;
    LPCWSTR wz = NULL;
    LPWSTR pwzUser = NULL;
    LOCALGROUP_MEMBERS_INFO_3 lgmi;

    if (*wzGroupDomain)
    {
        wz = wzGroupDomain;
    }

    // Try adding it to the global group first
    UINT ui = ::NetGroupAddUser(wz, wzGroup, wzUser);
    if (NERR_GroupNotFound == ui)
    {
        // Try adding it to the local group
        if (wzUserDomain)
        {
            hr = StrAllocFormatted(&pwzUser, L"%s\\%s", wzUserDomain, wzUser);
            ExitOnFailure(hr, "failed to allocate user domain string");
        }

        lgmi.lgrmi3_domainandname = (NULL == pwzUser ? wzUser : pwzUser);
        ui = ::NetLocalGroupAddMembers(wz, wzGroup, 3 , reinterpret_cast<LPBYTE>(&lgmi), 1);
    }
    hr = HRESULT_FROM_WIN32(ui);
    if (HRESULT_FROM_WIN32(ERROR_MEMBER_IN_ALIAS) == hr) // if they're already a member of the group don't report an error
        hr = S_OK;

    //
    // If we failed, try active directory
    //
    if (FAILED(hr))
    {
        WcaLog(LOGMSG_VERBOSE, "Failed to add user: %ls, domain %ls to group: %ls, domain: %ls with error 0x%x.  Attempting to use Active Directory", wzUser, wzUserDomain, wzGroup, wzGroupDomain, hr);

        hr = UserCreateADsPath(wzUserDomain, wzUser, &bstrUser);
        ExitOnFailure(hr, "failed to create user ADsPath for user: %ls domain: %ls", wzUser, wzUserDomain);

        hr = UserCreateADsPath(wzGroupDomain, wzGroup, &bstrGroup);
        ExitOnFailure(hr, "failed to create group ADsPath for group: %ls domain: %ls", wzGroup, wzGroupDomain);

        hr = ::ADsGetObject(bstrGroup,IID_IADsGroup, reinterpret_cast<void**>(&pGroup));
        ExitOnFailure(hr, "Failed to get group '%ls'.", reinterpret_cast<WCHAR*>(bstrGroup) );

        hr = pGroup->Add(bstrUser);
        if ((HRESULT_FROM_WIN32(ERROR_OBJECT_ALREADY_EXISTS) == hr) || (HRESULT_FROM_WIN32(ERROR_MEMBER_IN_ALIAS) == hr))
            hr = S_OK;

        ExitOnFailure(hr, "Failed to add user %ls to group '%ls'.", reinterpret_cast<WCHAR*>(bstrUser), reinterpret_cast<WCHAR*>(bstrGroup) );
    }

LExit:
    ReleaseObject(pGroup);
    ReleaseBSTR(bstrUser);
    ReleaseBSTR(bstrGroup);

    return hr;
}

static HRESULT RemoveUserFromGroup(
    __in LPWSTR wzUser,
    __in LPCWSTR wzUserDomain,
    __in LPCWSTR wzGroup,
    __in LPCWSTR wzGroupDomain
    )
{
    Assert(wzUser && *wzUser && wzUserDomain && wzGroup && *wzGroup && wzGroupDomain);

    HRESULT hr = S_OK;
    IADsGroup *pGroup = NULL;
    BSTR bstrUser = NULL;
    BSTR bstrGroup = NULL;
    LPCWSTR wz = NULL;
    LPWSTR pwzUser = NULL;
    LOCALGROUP_MEMBERS_INFO_3 lgmi;

    if (*wzGroupDomain)
    {
        wz = wzGroupDomain;
    }

    // Try removing it from the global group first
    UINT ui = ::NetGroupDelUser(wz, wzGroup, wzUser);
    if (NERR_GroupNotFound == ui)
    {
        // Try removing it from the local group
        if (wzUserDomain)
        {
            hr = StrAllocFormatted(&pwzUser, L"%s\\%s", wzUserDomain, wzUser);
            ExitOnFailure(hr, "failed to allocate user domain string");
        }

        lgmi.lgrmi3_domainandname = (NULL == pwzUser ? wzUser : pwzUser);
        ui = ::NetLocalGroupDelMembers(wz, wzGroup, 3 , reinterpret_cast<LPBYTE>(&lgmi), 1);
    }
    hr = HRESULT_FROM_WIN32(ui);

    //
    // If we failed, try active directory
    //
    if (FAILED(hr))
    {
        WcaLog(LOGMSG_VERBOSE, "Failed to remove user: %ls, domain %ls from group: %ls, domain: %ls with error 0x%x.  Attempting to use Active Directory", wzUser, wzUserDomain, wzGroup, wzGroupDomain, hr);

        hr = UserCreateADsPath(wzUserDomain, wzUser, &bstrUser);
        ExitOnFailure(hr, "failed to create user ADsPath in order to remove user: %ls domain: %ls from a group", wzUser, wzUserDomain);

        hr = UserCreateADsPath(wzGroupDomain, wzGroup, &bstrGroup);
        ExitOnFailure(hr, "failed to create group ADsPath in order to remove user from group: %ls domain: %ls", wzGroup, wzGroupDomain);

        hr = ::ADsGetObject(bstrGroup,IID_IADsGroup, reinterpret_cast<void**>(&pGroup));
        ExitOnFailure(hr, "Failed to get group '%ls'.", reinterpret_cast<WCHAR*>(bstrGroup) );

        hr = pGroup->Remove(bstrUser);
        ExitOnFailure(hr, "Failed to remove user %ls from group '%ls'.", reinterpret_cast<WCHAR*>(bstrUser), reinterpret_cast<WCHAR*>(bstrGroup) );
    }

LExit:
    ReleaseObject(pGroup);
    ReleaseBSTR(bstrUser);
    ReleaseBSTR(bstrGroup);

    return hr;
}


static HRESULT GetUserHasRight(
    __in LSA_HANDLE hPolicy,
    __in PSID pUserSid,
    __in LPWSTR wzRight,
    __out BOOL* fHasRight
)
{
    HRESULT hr = S_OK;
    NTSTATUS nt = 0;
    LSA_UNICODE_STRING lucPrivilege = { 0 };
    PLSA_ENUMERATION_INFORMATION rgSids = NULL;
    ULONG cSids = 0;
    *fHasRight = FALSE;

    lucPrivilege.Buffer = wzRight;
    lucPrivilege.Length = static_cast<USHORT>(lstrlenW(lucPrivilege.Buffer) * sizeof(WCHAR));
    lucPrivilege.MaximumLength = (lucPrivilege.Length + 1) * sizeof(WCHAR);

    nt = ::LsaEnumerateAccountsWithUserRight(hPolicy, &lucPrivilege, reinterpret_cast<PVOID*>(&rgSids), &cSids);
    hr = HRESULT_FROM_WIN32(::LsaNtStatusToWinError(nt));
    ExitOnFailure(hr, "Failed to enumerate users for right: %ls", lucPrivilege.Buffer);

    for (DWORD i = 0; i < cSids; ++i)
    {
        PLSA_ENUMERATION_INFORMATION pInfo = rgSids + i;
        if (::EqualSid(pUserSid, pInfo->Sid))
        {
            *fHasRight = TRUE;
            break;
        }
    }

LExit:
    if (rgSids)
    {
        ::LsaFreeMemory(rgSids);
    }

    return hr;
}


static HRESULT GetExistingUserRightsAssignments(
    __in_opt LPCWSTR wzDomain,
    __in LPCWSTR wzName,
    __inout int* iAttributes
)
{
    HRESULT hr = S_OK;
    NTSTATUS nt = 0;
    BOOL fHasRight = FALSE;

    LSA_HANDLE hPolicy = NULL;
    LSA_OBJECT_ATTRIBUTES objectAttributes = { 0 };

    LPWSTR pwzUser = NULL;
    PSID psid = NULL;

    if (wzDomain && *wzDomain)
    {
        hr = StrAllocFormatted(&pwzUser, L"%s\\%s", wzDomain, wzName);
        ExitOnFailure(hr, "Failed to allocate user with domain string");
    }
    else
    {
        hr = StrAllocString(&pwzUser, wzName, 0);
        ExitOnFailure(hr, "Failed to allocate string from user name.");
    }

    hr = AclGetAccountSid(NULL, pwzUser, &psid);
    ExitOnFailure(hr, "Failed to get SID for user: %ls", pwzUser);

    nt = ::LsaOpenPolicy(NULL, &objectAttributes, POLICY_LOOKUP_NAMES | POLICY_VIEW_LOCAL_INFORMATION, &hPolicy);
    hr = HRESULT_FROM_WIN32(::LsaNtStatusToWinError(nt));
    ExitOnFailure(hr, "Failed to open LSA policy store");

    hr = GetUserHasRight(hPolicy, psid, L"SeServiceLogonRight", &fHasRight);
    ExitOnFailure(hr, "Failed to check LogonAsService right");

    if (fHasRight)
    {
        *iAttributes |= SCAU_ALLOW_LOGON_AS_SERVICE;
    }

    hr = GetUserHasRight(hPolicy, psid, L"SeBatchLogonRight", &fHasRight);
    ExitOnFailure(hr, "Failed to check LogonAsBatchJob right");

    if (fHasRight)
    {
        *iAttributes |= SCAU_ALLOW_LOGON_AS_BATCH;
    }

LExit:
    if (hPolicy)
    {
        ::LsaClose(hPolicy);
    }

    ReleaseSid(psid);
    ReleaseStr(pwzUser);
    return hr;
}


static HRESULT ModifyUserLocalServiceRight(
    __in_opt LPCWSTR wzDomain,
    __in LPCWSTR wzName,
    __in BOOL fAdd
    )
{
    HRESULT hr = S_OK;
    NTSTATUS nt = 0;

    LPWSTR pwzUser = NULL;
    PSID psid = NULL;
    LSA_HANDLE hPolicy = NULL;
    LSA_OBJECT_ATTRIBUTES ObjectAttributes = { 0 };
    LSA_UNICODE_STRING lucPrivilege = { 0 };

    if (wzDomain && *wzDomain)
    {
        hr = StrAllocFormatted(&pwzUser, L"%s\\%s", wzDomain, wzName);
        ExitOnFailure(hr, "Failed to allocate user with domain string");
    }
    else
    {
        hr = StrAllocString(&pwzUser, wzName, 0);
        ExitOnFailure(hr, "Failed to allocate string from user name.");
    }

    hr = AclGetAccountSid(NULL, pwzUser, &psid);
    ExitOnFailure(hr, "Failed to get SID for user: %ls", pwzUser);

    nt = ::LsaOpenPolicy(NULL, &ObjectAttributes, POLICY_ALL_ACCESS, &hPolicy);
    hr = HRESULT_FROM_WIN32(::LsaNtStatusToWinError(nt));
    ExitOnFailure(hr, "Failed to open LSA policy store.");

    lucPrivilege.Buffer = L"SeServiceLogonRight";
    lucPrivilege.Length = static_cast<USHORT>(lstrlenW(lucPrivilege.Buffer) * sizeof(WCHAR));
    lucPrivilege.MaximumLength = (lucPrivilege.Length + 1) * sizeof(WCHAR);

    if (fAdd)
    {
        nt = ::LsaAddAccountRights(hPolicy, psid, &lucPrivilege, 1);
        hr = HRESULT_FROM_WIN32(::LsaNtStatusToWinError(nt));
        ExitOnFailure(hr, "Failed to add 'logon as service' bit to user: %ls", pwzUser);
    }
    else
    {
        nt = ::LsaRemoveAccountRights(hPolicy, psid, FALSE, &lucPrivilege, 1);
        hr = HRESULT_FROM_WIN32(::LsaNtStatusToWinError(nt));
        ExitOnFailure(hr, "Failed to remove 'logon as service' bit from user: %ls", pwzUser);
    }

LExit:
    if (hPolicy)
    {
        ::LsaClose(hPolicy);
    }

    ReleaseSid(psid);
    ReleaseStr(pwzUser);
    return hr;
}


static HRESULT ModifyUserLocalBatchRight(
  __in_opt LPCWSTR wzDomain,
  __in LPCWSTR wzName,
  __in BOOL fAdd
  )
{
    HRESULT hr = S_OK;
    NTSTATUS nt = 0;

    LPWSTR pwzUser = NULL;
    PSID psid = NULL;
    LSA_HANDLE hPolicy = NULL;
    LSA_OBJECT_ATTRIBUTES ObjectAttributes = { 0 };
    LSA_UNICODE_STRING lucPrivilege = { 0 };

    if (wzDomain && *wzDomain)
    {
        hr = StrAllocFormatted(&pwzUser, L"%s\\%s", wzDomain, wzName);
        ExitOnFailure(hr, "Failed to allocate user with domain string");
    }
    else
    {
        hr = StrAllocString(&pwzUser, wzName, 0);
        ExitOnFailure(hr, "Failed to allocate string from user name.");
    }

    hr = AclGetAccountSid(NULL, pwzUser, &psid);
    ExitOnFailure(hr, "Failed to get SID for user: %ls", pwzUser);

    nt = ::LsaOpenPolicy(NULL, &ObjectAttributes, POLICY_ALL_ACCESS, &hPolicy);
    hr = HRESULT_FROM_WIN32(::LsaNtStatusToWinError(nt));
    ExitOnFailure(hr, "Failed to open LSA policy store.");

    lucPrivilege.Buffer = L"SeBatchLogonRight";
    lucPrivilege.Length = static_cast<USHORT>(lstrlenW(lucPrivilege.Buffer) * sizeof(WCHAR));
    lucPrivilege.MaximumLength = (lucPrivilege.Length + 1) * sizeof(WCHAR);

    if (fAdd)
    {
        nt = ::LsaAddAccountRights(hPolicy, psid, &lucPrivilege, 1);
        hr = HRESULT_FROM_WIN32(::LsaNtStatusToWinError(nt));
        ExitOnFailure(hr, "Failed to add 'logon as batch job' bit to user: %ls", pwzUser);
    }
    else
    {
        nt = ::LsaRemoveAccountRights(hPolicy, psid, FALSE, &lucPrivilege, 1);
        hr = HRESULT_FROM_WIN32(::LsaNtStatusToWinError(nt));
        ExitOnFailure(hr, "Failed to remove 'logon as batch job' bit from user: %ls", pwzUser);
    }

  LExit:
    if (hPolicy)
    {
        ::LsaClose(hPolicy);
    }

    ReleaseSid(psid);
    ReleaseStr(pwzUser);
    return hr;
}

static HRESULT ApplyAttributes(int iAttributes, DWORD* pFlags)
{
    HRESULT hr = S_OK;

    if (SCAU_DONT_EXPIRE_PASSWRD & iAttributes)
    {
        *pFlags |= UF_DONT_EXPIRE_PASSWD;
    }
    else
    {
        *pFlags &= ~UF_DONT_EXPIRE_PASSWD;
    }

    if (SCAU_PASSWD_CANT_CHANGE & iAttributes)
    {
        *pFlags |= UF_PASSWD_CANT_CHANGE;
    }
    else
    {
        *pFlags &= ~UF_PASSWD_CANT_CHANGE;
    }

    if (SCAU_DISABLE_ACCOUNT & iAttributes)
    {
        *pFlags |= UF_ACCOUNTDISABLE;
    }
    else
    {
        *pFlags &= ~UF_ACCOUNTDISABLE;
    }

    if (SCAU_PASSWD_CHANGE_REQD_ON_LOGIN & iAttributes) // TODO: for some reason this doesn't work
    {
        *pFlags |= UF_PASSWORD_EXPIRED;
    }
    else
    {
        *pFlags &= ~UF_PASSWORD_EXPIRED;
    }

    return hr;
}

static HRESULT ApplyComment(int iAttributes, LPWSTR pwzComment, LPWSTR* ppComment)
{
    HRESULT hr = S_OK;

    if (SCAU_REMOVE_COMMENT & iAttributes)
    {
        *ppComment = L"";
    }
    else if (pwzComment && *pwzComment)
    {
        *ppComment = pwzComment;
    }

    return hr;
}

static NET_API_STATUS SetUserPassword(__in LPWSTR pwzServerName, __in LPWSTR pwzName, __in LPWSTR pwzPassword)
{
    _USER_INFO_1003 userInfo1003;

    userInfo1003.usri1003_password = pwzPassword;
    return ::NetUserSetInfo(pwzServerName, pwzName, 1003, reinterpret_cast<LPBYTE>(&userInfo1003), NULL);
}

static NET_API_STATUS SetUserComment(__in LPWSTR pwzServerName, __in LPWSTR pwzName, __in LPWSTR pwzComment)
{
    _USER_INFO_1007 userInfo1007;

    userInfo1007.usri1007_comment = pwzComment;
    return ::NetUserSetInfo(pwzServerName, pwzName, 1007, reinterpret_cast<LPBYTE>(&userInfo1007), NULL);
}

static NET_API_STATUS SetUserFlags(__in LPWSTR pwzServerName, __in LPWSTR pwzName, __in DWORD flags)
{
    _USER_INFO_1008 userInfo1008;

    userInfo1008.usri1008_flags = flags;
    return ::NetUserSetInfo(pwzServerName, pwzName, 1008, reinterpret_cast<LPBYTE>(&userInfo1008), NULL);
}

static HRESULT RemoveUserInternal(
    LPWSTR wzGroupCaData,
    LPWSTR wzDomain,
    LPWSTR wzName,
    int iAttributes
)
{
    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    LPWSTR pwz = NULL;
    LPWSTR pwzGroup = NULL;
    LPWSTR pwzGroupDomain = NULL;
    LPCWSTR wz = NULL;
    PDOMAIN_CONTROLLER_INFOW pDomainControllerInfo = NULL;

    //
    // Remove the logon as service privilege.
    //
    if (SCAU_ALLOW_LOGON_AS_SERVICE & iAttributes)
    {
        hr = ModifyUserLocalServiceRight(wzDomain, wzName, FALSE);
        if (FAILED(hr))
        {
            WcaLogError(hr, "Failed to remove logon as service right from user, continuing...");
            hr = S_OK;
        }
    }

    if (SCAU_ALLOW_LOGON_AS_BATCH & iAttributes)
    {
        hr = ModifyUserLocalBatchRight(wzDomain, wzName, FALSE);
        if (FAILED(hr))
        {
            WcaLogError(hr, "Failed to remove logon as batch job right from user, continuing...");
            hr = S_OK;
        }
    }

    //
    // Remove the User Account if the user was created by us.
    //
    if (!(SCAU_DONT_CREATE_USER & iAttributes))
    {
        if (wzDomain && *wzDomain)
        {
            er = ::DsGetDcNameW(NULL, (LPCWSTR)wzDomain, NULL, NULL, NULL, &pDomainControllerInfo);
            if (RPC_S_SERVER_UNAVAILABLE == er)
            {
                // MSDN says, if we get the above error code, try again with the "DS_FORCE_REDISCOVERY" flag
                er = ::DsGetDcNameW(NULL, (LPCWSTR)wzDomain, NULL, NULL, DS_FORCE_REDISCOVERY, &pDomainControllerInfo);
            }
            if (ERROR_SUCCESS == er)
            {
                wz = pDomainControllerInfo->DomainControllerName + 2;  //Add 2 so that we don't get the \\ prefix
            }
            else
            {
                wz = wzDomain;
            }
        }

        er = ::NetUserDel(wz, wzName);
        if (NERR_UserNotFound == er)
        {
            er = NERR_Success;
        }
        ExitOnFailure(hr = HRESULT_FROM_WIN32(er), "failed to delete user account: %ls", wzName);
    }
    else
    {
        //
        // Remove the user from the groups
        //
        pwz = wzGroupCaData;
        while (S_OK == (hr = WcaReadStringFromCaData(&pwz, &pwzGroup)))
        {
            hr = WcaReadStringFromCaData(&pwz, &pwzGroupDomain);

            if (FAILED(hr))
            {
                WcaLogError(hr, "failed to get domain for group: %ls, continuing anyway.", pwzGroup);
            }
            else
            {
                hr = RemoveUserFromGroup(wzName, wzDomain, pwzGroup, pwzGroupDomain);
                if (FAILED(hr))
                {
                    WcaLogError(hr, "failed to remove user: %ls from group %ls, continuing anyway.", wzName, pwzGroup);
                }
            }
        }

        if (E_NOMOREITEMS == hr) // if there are no more items, all is well
        {
            hr = S_OK;
        }

        ExitOnFailure(hr, "failed to get next group from which to remove user:%ls", wzName);
    }

LExit:
    if (pDomainControllerInfo)
    {
        ::NetApiBufferFree(static_cast<LPVOID>(pDomainControllerInfo));
    }

    return hr;
}

static HRESULT GetServerName(LPWSTR pwzDomain, LPWSTR* ppwzServerName)
{
    HRESULT hr = S_OK;

    PDOMAIN_CONTROLLER_INFOW pDomainControllerInfo = NULL;
    UINT er;

    if (pwzDomain && *pwzDomain)
    {
        er = ::DsGetDcNameW(NULL, (LPCWSTR)pwzDomain, NULL, NULL, NULL, &pDomainControllerInfo);
        if (RPC_S_SERVER_UNAVAILABLE == er)
        {
            // MSDN says, if we get the above error code, try again with the "DS_FORCE_REDISCOVERY" flag
            er = ::DsGetDcNameW(NULL, (LPCWSTR)pwzDomain, NULL, NULL, DS_FORCE_REDISCOVERY, &pDomainControllerInfo);
        }
        if (ERROR_SUCCESS == er
         && 2 <= wcslen(pDomainControllerInfo->DomainControllerName)
         && '\\' == *pDomainControllerInfo->DomainControllerName
         && '\\' == *pDomainControllerInfo->DomainControllerName + 1)
        {
            *ppwzServerName = pDomainControllerInfo->DomainControllerName + 2;  // Skip the \\ prefix
        }
        else
        {
            *ppwzServerName = pwzDomain;
        }
    }

    if (pDomainControllerInfo)
    {
        ::NetApiBufferFree((LPVOID)pDomainControllerInfo);
    }

    return hr;
}

/********************************************************************
 CreateUser - CUSTOM ACTION ENTRY POINT for creating users

  Input:  deferred CustomActionData - UserName\tDomain\tPassword\tAttributes\tGroupName\tDomain\tGroupName\tDomain...
 * *****************************************************************/
extern "C" UINT __stdcall CreateUser(
    __in MSIHANDLE hInstall
)
{
    //AssertSz(0, "Debug CreateUser");

    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    LPWSTR pwzData = NULL;
    LPWSTR pwz = NULL;
    LPWSTR pwzName = NULL;
    LPWSTR pwzDomain = NULL;
    LPWSTR pwzComment = NULL;
    LPWSTR pwzScriptKey = NULL;
    LPWSTR pwzPassword = NULL;
    LPWSTR pwzGroup = NULL;
    LPWSTR pwzGroupDomain = NULL;
    int iAttributes = 0;
    BOOL fInitializedCom = FALSE;

    WCA_CASCRIPT_HANDLE hRollbackScript = NULL;
    int iOriginalAttributes = 0;
    int iRollbackAttributes = 0;

    USER_INFO_1 userInfo1;
    USER_INFO_1* pUserInfo1 = NULL;
    DWORD dw;
    LPWSTR pwzServerName = NULL;

    hr = WcaInitialize(hInstall, "CreateUser");
    ExitOnFailure(hr, "failed to initialize");

    hr = ::CoInitialize(NULL);
    ExitOnFailure(hr, "failed to initialize COM");
    fInitializedCom = TRUE;

    hr = WcaGetProperty(L"CustomActionData", &pwzData);
    ExitOnFailure(hr, "failed to get CustomActionData");

    WcaLog(LOGMSG_TRACEONLY, "CustomActionData: %ls", pwzData);

    //
    // Read in the CustomActionData
    //
    pwz = pwzData;
    hr = WcaReadStringFromCaData(&pwz, &pwzName);
    ExitOnFailure(hr, "failed to read user name from custom action data");

    hr = WcaReadStringFromCaData(&pwz, &pwzDomain);
    ExitOnFailure(hr, "failed to read domain from custom action data");

    hr = WcaReadStringFromCaData(&pwz, &pwzComment);
    ExitOnFailure(hr, "failed to read user comment from custom action data");

    hr = WcaReadIntegerFromCaData(&pwz, &iAttributes);
    ExitOnFailure(hr, "failed to read attributes from custom action data");

    hr = WcaReadStringFromCaData(&pwz, &pwzScriptKey);
    ExitOnFailure(hr, "failed to read encoding key from custom action data");

    hr = WcaReadStringFromCaData(&pwz, &pwzPassword);
    ExitOnFailure(hr, "failed to read password from custom action data");

    if (!(SCAU_DONT_CREATE_USER & iAttributes))
    {
        pUserInfo1 = &userInfo1;
        ::ZeroMemory(pUserInfo1, sizeof(USER_INFO_1));
        pUserInfo1->usri1_name = pwzName;
        pUserInfo1->usri1_priv = USER_PRIV_USER;
        pUserInfo1->usri1_flags = UF_SCRIPT;
        pUserInfo1->usri1_home_dir = NULL;
        pUserInfo1->usri1_comment = NULL;
        pUserInfo1->usri1_script_path = NULL;

        // Set the user's password
        pUserInfo1->usri1_password = pwzPassword;

        // Set the user's comment
        hr = ApplyComment(iAttributes, pwzComment, &pUserInfo1->usri1_comment);
        ExitOnFailure(hr, "failed to apply comment");

        // Set the user's flags
        hr = ApplyAttributes(iAttributes, &pUserInfo1->usri1_flags);
        ExitOnFailure(hr, "failed to apply attributes");

        //
        // Create the User
        //
        hr = GetServerName(pwzDomain, &pwzServerName);
        ExitOnFailure(hr, "failed to get server name");

        er = ::NetUserAdd(pwzServerName, 1, reinterpret_cast<LPBYTE>(pUserInfo1), &dw);
        if (NERR_UserExists == er)
        {
            if (SCAU_UPDATE_IF_EXISTS & iAttributes)
            {
                pUserInfo1 = NULL;
                ExitOnNetworkError(::NetUserGetInfo(pwzServerName, pwzName, 1, reinterpret_cast<LPBYTE*>(&pUserInfo1)));
                // There is no rollback scheduled if the key is empty.
                // Best effort to get original configuration and save it in the script so rollback can restore it.

                if (*pwzScriptKey)
                {
                    // Try to open the rollback script
                    hr = WcaCaScriptOpen(WCA_ACTION_INSTALL, WCA_CASCRIPT_ROLLBACK, FALSE, pwzScriptKey, &hRollbackScript);

                    if (INVALID_HANDLE_VALUE != hRollbackScript)
                    {
                        WcaCaScriptClose(hRollbackScript, WCA_CASCRIPT_CLOSE_PRESERVE);
                    }
                    else
                    {
                        hRollbackScript = NULL;
                        hr = WcaCaScriptCreate(WCA_ACTION_INSTALL, WCA_CASCRIPT_ROLLBACK, FALSE, pwzScriptKey, FALSE, &hRollbackScript);
                        ExitOnFailure(hr, "Failed to open rollback CustomAction script.");

                        iRollbackAttributes = 0;
                        hr = GetExistingUserRightsAssignments(pwzDomain, pwzName, &iOriginalAttributes);
                        if (FAILED(hr))
                        {
                            WcaLogError(hr, "failed to get existing user rights: %ls, continuing anyway.", pwzName);
                        }
                        else
                        {
                            if (!(SCAU_ALLOW_LOGON_AS_SERVICE & iOriginalAttributes) && (SCAU_ALLOW_LOGON_AS_SERVICE & iAttributes))
                            {
                                iRollbackAttributes |= SCAU_ALLOW_LOGON_AS_SERVICE;
                            }

                            if (!(SCAU_ALLOW_LOGON_AS_BATCH & iOriginalAttributes) && (SCAU_ALLOW_LOGON_AS_BATCH & iAttributes))
                            {
                                iRollbackAttributes |= SCAU_ALLOW_LOGON_AS_BATCH;
                            }
                        }

                        hr = WcaCaScriptWriteString(hRollbackScript, pUserInfo1->usri1_comment);
                        ExitOnFailure(hr, "Failed to add rollback comment to rollback script.");

                        if (!pUserInfo1->usri1_comment || !*pUserInfo1->usri1_comment)
                        {
                            iRollbackAttributes |= SCAU_REMOVE_COMMENT;
                        }

                        hr = WcaCaScriptWriteNumber(hRollbackScript, iRollbackAttributes);
                        ExitOnFailure(hr, "Failed to add rollback attributes to rollback script.");

                        // Nudge the system to get all our rollback data written to disk.
                        WcaCaScriptFlush(hRollbackScript);
                    }
                }

                ExitOnNetworkError(::SetUserPassword(pwzServerName, pwzName, pwzPassword));

                if (SCAU_REMOVE_COMMENT & iAttributes)
                {
                    ExitOnNetworkError(SetUserComment(pwzServerName, pwzName, L""));
                }
                else if (pwzComment && *pwzComment)
                {
                    ExitOnNetworkError(SetUserComment(pwzServerName, pwzName, pwzComment));
                }

                DWORD flags = pUserInfo1->usri1_flags;

                hr = ApplyAttributes(iAttributes, &flags);
                ExitOnFailure(hr, "failed to apply attributes");

                ExitOnNetworkError(SetUserFlags(pwzServerName, pwzName, flags));
            }
            else
            {
                // The user exists, but was not updated
                if (SCAU_REMOVE_COMMENT & iAttributes)
                {
                    ExitOnNetworkError(SetUserComment(pwzServerName, pwzName, L""));
                }
                else if (pwzComment && *pwzComment)
                {
                    ExitOnNetworkError(SetUserComment(pwzServerName, pwzName, pwzComment));
                }

                if (SCAU_FAIL_IF_EXISTS & iAttributes)
                {
                    ExitOnNetworkError(NERR_UserExists);
                }
            }
        }
        else if (NERR_PasswordTooShort == er || NERR_PasswordTooLong == er)
        {
            MessageExitOnFailure(hr = HRESULT_FROM_WIN32(er), msierrUSRFailedUserCreatePswd, "failed to create user: %ls due to invalid password.", pwzName);
        }

    LNExit:
        MessageExitOnFailure(hr = HRESULT_FROM_WIN32(er), msierrUSRFailedUserCreate, "failed to create user: %ls", pwzName);
    }

    if (SCAU_ALLOW_LOGON_AS_SERVICE & iAttributes)
    {
        hr = ModifyUserLocalServiceRight(pwzDomain, pwzName, TRUE);
        MessageExitOnFailure(hr, msierrUSRFailedGrantLogonAsService, "Failed to grant logon as service rights to user: %ls", pwzName);
    }

    if (SCAU_ALLOW_LOGON_AS_BATCH & iAttributes)
    {
        hr = ModifyUserLocalBatchRight(pwzDomain, pwzName, TRUE);
        MessageExitOnFailure(hr, msierrUSRFailedGrantLogonAsService, "Failed to grant logon as batch job rights to user: %ls", pwzName);
    }

//
// Add the users to groups
//
while (S_OK == (hr = WcaReadStringFromCaData(&pwz, &pwzGroup)))
{
    hr = WcaReadStringFromCaData(&pwz, &pwzGroupDomain);
    ExitOnFailure(hr, "failed to get domain for group: %ls", pwzGroup);

        WcaLog(LOGMSG_STANDARD, "Adding user %ls\\%ls to group %ls\\%ls", pwzDomain, pwzName, pwzGroupDomain, pwzGroup);
        hr = AddUserToGroup(pwzName, pwzDomain, pwzGroup, pwzGroupDomain);
        MessageExitOnFailure(hr, msierrUSRFailedUserGroupAdd, "failed to add user: %ls to group %ls", pwzName, pwzGroup);
    }
    if (E_NOMOREITEMS == hr) // if there are no more items, all is well
    {
        hr = S_OK;
    }

    ExitOnFailure(hr, "failed to get next group in which to include user:%ls", pwzName);

ExitOnFailure(hr, "failed to get next group in which to include user:%ls", pwzName);

LExit:
    WcaCaScriptClose(hRollbackScript, WCA_CASCRIPT_CLOSE_PRESERVE);

    if (pUserInfo1 && pUserInfo1 != &userInfo1)
    {
        ::NetApiBufferFree((LPVOID)pUserInfo1);
    }

    ReleaseStr(pwzData);
    ReleaseStr(pwzName);
    ReleaseStr(pwzDomain);
    ReleaseStr(pwzComment);
    ReleaseStr(pwzScriptKey);
    ReleaseStr(pwzPassword);
    ReleaseStr(pwzGroup);
    ReleaseStr(pwzGroupDomain);

    if (fInitializedCom)
    {
        ::CoUninitialize();
    }

    if (SCAU_NON_VITAL & iAttributes)
    {
        er = ERROR_SUCCESS;
    }
    else if (FAILED(hr))
    {
        er = ERROR_INSTALL_FAILURE;
    }

    return WcaFinalize(er);
}


/********************************************************************
 CreateUserRollback - CUSTOM ACTION ENTRY POINT for CreateUser rollback

 * *****************************************************************/
extern "C" UINT __stdcall CreateUserRollback(
    MSIHANDLE hInstall
)
{
    //AssertSz(0, "Debug CreateUserRollback");

    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    LPWSTR pwzData = NULL;
    LPWSTR pwz = NULL;
    LPWSTR pwzScriptKey = NULL;
    LPWSTR pwzName = NULL;
    LPWSTR pwzDomain = NULL;
    LPWSTR pwzComment = NULL;
    int iAttributes = 0;
    BOOL fInitializedCom = FALSE;

    WCA_CASCRIPT_HANDLE hRollbackScript = NULL;
    LPWSTR pwzRollbackData = NULL;
    int iOriginalAttributes = 0;
    LPWSTR pwzOriginalComment = NULL;

    hr = WcaInitialize(hInstall, "CreateUserRollback");
    ExitOnFailure(hr, "failed to initialize");

    hr = ::CoInitialize(NULL);
    ExitOnFailure(hr, "failed to initialize COM");
    fInitializedCom = TRUE;

    hr = WcaGetProperty(L"CustomActionData", &pwzData);
    ExitOnFailure(hr, "failed to get CustomActionData");

    WcaLog(LOGMSG_TRACEONLY, "CustomActionData: %ls", pwzData);

    //
    // Read in the CustomActionData
    //
    pwz = pwzData;
    hr = WcaReadStringFromCaData(&pwz, &pwzScriptKey);
    ExitOnFailure(hr, "failed to read encoding key from custom action data");

    hr = WcaReadStringFromCaData(&pwz, &pwzName);
    ExitOnFailure(hr, "failed to read name from custom action data");

    hr = WcaReadStringFromCaData(&pwz, &pwzDomain);
    ExitOnFailure(hr, "failed to read domain from custom action data");

    hr = WcaReadStringFromCaData(&pwz, &pwzComment);
    ExitOnFailure(hr, "failed to read comment from custom action data");

    hr = WcaReadIntegerFromCaData(&pwz, &iAttributes);
    ExitOnFailure(hr, "failed to read attributes from custom action data");

    // Best effort to read original configuration from CreateUser.
    hr = WcaCaScriptOpen(WCA_ACTION_INSTALL, WCA_CASCRIPT_ROLLBACK, FALSE, pwzScriptKey, &hRollbackScript);
    if (FAILED(hr))
    {
        WcaLogError(hr, "Failed to open rollback CustomAction script, continuing anyway.");
    }
    else
    {
        hr = WcaCaScriptReadAsCustomActionData(hRollbackScript, &pwzRollbackData);
        if (FAILED(hr))
        {
            WcaLogError(hr, "Failed to read rollback script into CustomAction data, continuing anyway.");
        }
        else
        {
            WcaLog(LOGMSG_TRACEONLY, "Rollback Data: %ls", pwzRollbackData);

            pwz = pwzRollbackData;
            hr = WcaReadStringFromCaData(&pwz, &pwzOriginalComment);
            if (FAILED(hr))
            {
                WcaLogError(hr, "failed to read comment from rollback data, continuing anyway");
            }
            else
            {
                pwzComment = pwzOriginalComment;
            }
            hr = WcaReadIntegerFromCaData(&pwz, &iOriginalAttributes);
            if (FAILED(hr))
            {
                WcaLogError(hr, "failed to read attributes from rollback data, continuing anyway");
            }
            else
            {
                iAttributes |= iOriginalAttributes;
            }
        }
    }

    hr = RemoveUserInternal(pwz, pwzDomain, pwzName, iAttributes);

LExit:
    WcaCaScriptClose(hRollbackScript, WCA_CASCRIPT_CLOSE_DELETE);

    ReleaseStr(pwzData);
    ReleaseStr(pwzName);
    ReleaseStr(pwzDomain);
    ReleaseStr(pwzComment);
    ReleaseStr(pwzScriptKey);
    ReleaseStr(pwzRollbackData);
    ReleaseStr(pwzOriginalComment);

    if (fInitializedCom)
    {
        ::CoUninitialize();
    }

    if (FAILED(hr))
    {
        er = ERROR_INSTALL_FAILURE;
    }

    return WcaFinalize(er);
}


/********************************************************************
 RemoveUser - CUSTOM ACTION ENTRY POINT for removing users

  Input:  deferred CustomActionData - Name\tDomain
 * *****************************************************************/
extern "C" UINT __stdcall RemoveUser(
    MSIHANDLE hInstall
)
{
    //AssertSz(0, "Debug RemoveUser");

    HRESULT hr = S_OK;
    UINT er = ERROR_SUCCESS;

    LPWSTR pwzData = NULL;
    LPWSTR pwz = NULL;
    LPWSTR pwzName = NULL;
    LPWSTR pwzDomain = NULL;
    LPWSTR pwzComment = NULL;
    int iAttributes = 0;
    BOOL fInitializedCom = FALSE;

    hr = WcaInitialize(hInstall, "RemoveUser");
    ExitOnFailure(hr, "failed to initialize");

    hr = ::CoInitialize(NULL);
    ExitOnFailure(hr, "failed to initialize COM");
    fInitializedCom = TRUE;

    hr = WcaGetProperty(L"CustomActionData", &pwzData);
    ExitOnFailure(hr, "failed to get CustomActionData");

    WcaLog(LOGMSG_TRACEONLY, "CustomActionData: %ls", pwzData);

    //
    // Read in the CustomActionData
    //
    pwz = pwzData;
    hr = WcaReadStringFromCaData(&pwz, &pwzName);
    ExitOnFailure(hr, "failed to read name from custom action data");

    hr = WcaReadStringFromCaData(&pwz, &pwzDomain);
    ExitOnFailure(hr, "failed to read domain from custom action data");

    hr = WcaReadStringFromCaData(&pwz, &pwzComment);
    ExitOnFailure(hr, "failed to read comment from custom action data");

    hr = WcaReadIntegerFromCaData(&pwz, &iAttributes);
    ExitOnFailure(hr, "failed to read attributes from custom action data");

    hr = RemoveUserInternal(pwz, pwzDomain, pwzName, iAttributes);

LExit:
    ReleaseStr(pwzData);
    ReleaseStr(pwzName);
    ReleaseStr(pwzDomain);
    ReleaseStr(pwzComment);

    if (fInitializedCom)
    {
        ::CoUninitialize();
    }

    if (FAILED(hr))
    {
        er = ERROR_INSTALL_FAILURE;
    }

    return WcaFinalize(er);
}
