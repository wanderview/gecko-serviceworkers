/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CacheStorageParent.h"

#include "mozilla/dom/CacheStorageManager.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/unused.h"
#include "nsCOMPtr.h"

using mozilla::dom::CacheStorageParent;

CacheStorageParent::CacheStorageParent(const nsACString& aOrigin,
                                       const nsACString& aBaseDomain)
  : mOrigin(aOrigin)
  , mBaseDomain(aBaseDomain)
  , mCacheStorageManager(CacheStorageManager::GetInstance())
{
  MOZ_ASSERT(mCacheStorageManager);
}

CacheStorageParent::~CacheStorageParent()
{
}

void
CacheStorageParent::ActorDestroy(ActorDestroyReason aReason)
{
}

bool
CacheStorageParent::RecvGet(const uintptr_t& aRequestId, const nsString& aKey)
{
  CacheParent* actor = mCacheStorageManager->Get(aKey);
  if (actor) {
    actor = static_cast<CacheParent*>(Manager()->SendPCacheConstructor(actor));
  }
  unused << SendGetResponse(aRequestId, actor);
  return true;
}

bool
CacheStorageParent::RecvHas(const uintptr_t& aRequestId, const nsString& aKey)
{
  unused << SendHasResponse(aRequestId, mCacheStorageManager->Has(aKey));
  return true;
}

bool
CacheStorageParent::RecvCreate(const uintptr_t& aRequestId, const nsString& aKey)
{
  CacheParent* actor = nullptr;
  if (!mCacheStorageManager->Has(aKey)) {
    actor = new CacheParent();
    actor = static_cast<CacheParent*>(Manager()->SendPCacheConstructor(actor));
    if (actor) {
      if (!mCacheStorageManager->Put(aKey, actor)) {
        unused << PCacheParent::Send__delete__(actor);
        actor = nullptr;
      }
    }
  }
  unused << SendCreateResponse(aRequestId, actor);
  return true;
}

bool
CacheStorageParent::RecvDelete(const uintptr_t& aRequestId, const nsString& aKey)
{
  unused << SendDeleteResponse(aRequestId, mCacheStorageManager->Delete(aKey));
  return true;
}

bool
CacheStorageParent::RecvKeys(const uintptr_t& aRequestId)
{
  nsTArray<nsString> keys;
  mCacheStorageManager->Keys(keys);
  unused << SendKeysResponse(aRequestId, keys);
  return true;
}
