/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CacheParent.h"

#include "mozilla/dom/CacheDBConnection.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {

using mozilla::dom::cache::RequestId;

CacheParent::CacheParent(const nsACString& aOrigin,
                         const nsACString& aBaseDomain)
  : mOrigin(aOrigin)
  , mBaseDomain(aBaseDomain)
  , mDBConnection(CacheDBConnection::Create(*this, aOrigin, aBaseDomain))
{
}

CacheParent::CacheParent(const nsACString& aOrigin,
                         const nsACString& aBaseDomain,
                         const nsID& aExistingCacheId)
  : mOrigin(aOrigin)
  , mBaseDomain(aBaseDomain)
  , mDBConnection(CacheDBConnection::Get(*this, aOrigin, aBaseDomain,
                                         aExistingCacheId))
{
}

CacheParent::~CacheParent()
{
}

void
CacheParent::ActorDestroy(ActorDestroyReason aReason)
{
}

bool
CacheParent::RecvMatch(const RequestId& aRequestId, const PCacheRequest& aRequest,
                       const PCacheQueryParams& aParams)
{
  return false;
}

bool
CacheParent::RecvMatchAll(const RequestId& aRequestId,
                          const PCacheRequest& aRequest,
                          const PCacheQueryParams& aParams)
{
  return false;
}

bool
CacheParent::RecvAdd(const RequestId& aRequestId, const PCacheRequest& aRequest)
{
  return false;
}

bool
CacheParent::RecvAddAll(const RequestId& aRequestId,
                        const nsTArray<PCacheRequest>& aRequests)
{
  return false;
}

bool
CacheParent::RecvPut(const RequestId& aRequestId, const PCacheRequest& aRequest,
                     const PCacheResponse& aResponse)
{
  MOZ_ASSERT(mDBConnection);
  nsresult rv = mDBConnection->Put(aRequestId, aRequest, aResponse);
  if (NS_FAILED(rv)) {
    PCacheResponse response;
    response.null() = true;
    unused << SendPutResponse(aRequestId, response);
  }

  return true;
}

bool
CacheParent::RecvDelete(const RequestId& aRequestId,
                        const PCacheRequest& aRequest,
                        const PCacheQueryParams& aParams)
{
  return false;
}

bool
CacheParent::RecvKeys(const RequestId& aRequestId, const PCacheRequest& aRequest,
                      const PCacheQueryParams& aParams)
{
  return false;
}

void
CacheParent::OnMatchAll(cache::RequestId aRequestId,
                        const nsTArray<PCacheResponse>& aResponses)
{
  unused << SendMatchAllResponse(aRequestId, aResponses);
}

void
CacheParent::OnPut(RequestId aRequestId, const PCacheResponse& aResponse)
{
  unused << SendPutResponse(aRequestId, aResponse);
}

} // namespace dom
} // namesapce mozilla
