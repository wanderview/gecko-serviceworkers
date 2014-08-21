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

CacheStorageParent::CacheStorageParent(const nsACString& aOrigin)
  : mCacheStorageManager(CacheStorageManager::GetInstance())
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
  printf_stderr("### ### CacheStorageParent::RecvCreate() sending get response %p\n", actor);
  unused << SendGetResponse(aRequestId, actor);
  return true;
}

bool
CacheStorageParent::RecvHas(const uintptr_t& aRequestId, const nsString& aKey)
{
  printf_stderr("### ### CacheStorageParent::RecvCreate() sending has response\n");
  unused << SendHasResponse(aRequestId, mCacheStorageManager->Has(aKey));
  return true;
}

bool
CacheStorageParent::RecvCreate(const uintptr_t& aRequestId, const nsString& aKey)
{
  CacheParent* actor = nullptr;
  if (!mCacheStorageManager->Has(aKey)) {
    actor = new CacheParent();
    printf_stderr("### ### CacheStorageParent::RecvCreate() allocated actor %p\n", actor);
    actor = static_cast<CacheParent*>(Manager()->SendPCacheConstructor(actor));
    printf_stderr("### ### CacheStorageParent::RecvCreate() constructed actor %p\n", actor);
    if (actor) {
      if (!mCacheStorageManager->Put(aKey, actor)) {
        printf_stderr("### ### CacheStorageParent::RecvCreate() create failed to store actor\n");
      }
    }
  }
  printf_stderr("### ### CacheStorageParent::RecvCreate() sending create response %p\n", actor);
  unused << SendCreateResponse(aRequestId, actor);
  return true;
}

bool
CacheStorageParent::RecvDelete(const uintptr_t& aRequestId, const nsString& aKey)
{
  printf_stderr("### ### CacheStorageParent::RecvCreate() sending delete response\n");
  unused << SendDeleteResponse(aRequestId, mCacheStorageManager->Delete(aKey));
  return true;
}

bool
CacheStorageParent::RecvKeys(const uintptr_t& aRequestId)
{
  nsTArray<nsString> keys;
  mCacheStorageManager->Keys(keys);
  printf_stderr("### ### CacheStorageParent::RecvCreate() sending keys response\n");
  unused << SendKeysResponse(aRequestId, keys);
  return true;
}
