#pragma once
// Copyright (c) .NET Foundation and contributors. All rights reserved. Licensed under the Microsoft Reciprocal License. See LICENSE.TXT file in the project root for full license information.

HRESULT GetDomainServerName(LPCWSTR pwzDomain, LPWSTR* ppwzServerName, ULONG flags = 0);
