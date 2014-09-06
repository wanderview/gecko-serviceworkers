/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CacheStorageParent.h"

#include "mozilla/dom/CacheStorageDBConnection.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/unused.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {

using mozilla::dom::cache::RequestId;

CacheStorageParent::CacheStorageParent(cache::Namespace aNamespace,
                                       const nsACString& aOrigin,
                                       const nsACString& aBaseDomain)
  : mNamespace(aNamespace)
  , mOrigin(aOrigin)
  , mBaseDomain(aBaseDomain)
{
}

CacheStorageParent::~CacheStorageParent()
{
  // TODO: Make sure any pending CacheStorageDBConnection calls are
  //       canceled or we clear ourself as a listener
}

void
CacheStorageParent::ActorDestroy(ActorDestroyReason aReason)
{
}

bool
CacheStorageParent::RecvGet(const RequestId& aRequestId, const nsString& aKey)
{
  CacheStorageDBConnection *conn = GetDBConnection();
  if (!conn) {
    unused << SendGetResponse(aRequestId, NS_ERROR_OUT_OF_MEMORY, nullptr);
    return true;
  }

  conn->Get(aRequestId, aKey);

  return true;
}

bool
CacheStorageParent::RecvHas(const RequestId& aRequestId, const nsString& aKey)
{
  CacheStorageDBConnection *conn = GetDBConnection();
  if (!conn) {
    unused << SendHasResponse(aRequestId, false);
    return true;
  }

  if(NS_FAILED(conn->Has(aRequestId, aKey))) {
    unused << SendHasResponse(aRequestId, false);
  }

  return true;
}

bool
CacheStorageParent::RecvCreate(const RequestId& aRequestId,
                               const nsString& aKey)
{
  CacheStorageDBConnection *conn = GetOrCreateDBConnection();
  if (NS_WARN_IF(!conn)) {
    unused << SendCreateResponse(aRequestId, NS_ERROR_OUT_OF_MEMORY, nullptr);
    return true;
  }

  // TODO: perform a Has() check first
  // TODO: create real DB-backed cache object
  // TODO: get uuid from cache object
  nsID uuid;

  conn->Put(aRequestId, aKey, uuid);

  return true;
}

bool
CacheStorageParent::RecvDelete(const RequestId& aRequestId,
                               const nsString& aKey)
{
  CacheStorageDBConnection *conn = GetDBConnection();
  if (!conn) {
    unused << SendDeleteResponse(aRequestId, false);
    return true;
  }

  if (NS_FAILED(conn->Delete(aRequestId, aKey))) {
    unused << SendDeleteResponse(aRequestId, false);
  }

  return true;
}

bool
CacheStorageParent::RecvKeys(const RequestId& aRequestId)
{
  CacheStorageDBConnection *conn = GetDBConnection();
  if (!conn) {
    unused << SendKeysResponse(aRequestId, nsTArray<nsString>());
    return true;
  }

  if (NS_FAILED(conn->Keys(aRequestId))) {
    unused << SendKeysResponse(aRequestId, nsTArray<nsString>());
  }

  return true;
}

void
CacheStorageParent::OnError(nsresult aRv)
{
  MOZ_CRASH("implement me");
}

void
CacheStorageParent::OnGet(cache::RequestId aRequestId, nsresult aRv,
                          nsID* aCacheId)
{
  if (NS_FAILED(aRv) || !aCacheId) {
    unused << SendGetResponse(aRequestId, aRv, nullptr);
  }

  // TODO: create cache parent for given uuid
  CacheParent* actor = new CacheParent(mOrigin, mBaseDomain);
  if (actor) {
    PCacheParent* base = Manager()->SendPCacheConstructor(actor, mOrigin,
                                                          mBaseDomain);
    actor = static_cast<CacheParent*>(base);
  }
  unused << SendGetResponse(aRequestId, aRv, actor);
}

void
CacheStorageParent::OnHas(RequestId aRequestId, bool aResult)
{
  unused << SendHasResponse(aRequestId, aResult);
}

void
CacheStorageParent::OnPut(RequestId aRequestId, nsresult aRv, bool aSuccess)
{
  if (NS_FAILED(aRv)) {
    unused << SendCreateResponse(aRequestId, aRv, nullptr);
  }

  CacheParent* actor = nullptr;
  if (aSuccess) {
    // TODO: retrieve DB-backed actor for uuid generated in RecvCreate()
    actor = new CacheParent(mOrigin, mBaseDomain);
    if (actor) {
      PCacheParent* base = Manager()->SendPCacheConstructor(actor, mOrigin,
                                                            mBaseDomain);
      actor = static_cast<CacheParent*>(base);
    }
  }
  unused << SendCreateResponse(aRequestId, aRv, actor);
}

void
CacheStorageParent::OnDelete(RequestId aRequestId, bool aResult)
{
  unused << SendDeleteResponse(aRequestId, aResult);
}

void
CacheStorageParent::OnKeys(RequestId aRequestId,
                           const nsTArray<nsString>& aKeys)
{
  unused << SendKeysResponse(aRequestId, aKeys);
}


CacheStorageDBConnection*
CacheStorageParent::GetDBConnection()
{
  if (!mDBConnection) {
    mDBConnection = new CacheStorageDBConnection(*this, mNamespace, mOrigin,
                                                 mBaseDomain, false);
  }

  return mDBConnection;
}

CacheStorageDBConnection*
CacheStorageParent::GetOrCreateDBConnection()
{
  if (!mDBConnection) {
    mDBConnection = new CacheStorageDBConnection(*this, mNamespace, mOrigin,
                                                 mBaseDomain, true);
  }

  return mDBConnection;
}

} // namespace dom
} // namespace mozilla
