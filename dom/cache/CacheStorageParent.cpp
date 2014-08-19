/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CacheStorageParent.h"

#include "mozilla/dom/CacheManager.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/unused.h"
#include "nsCOMPtr.h"

using mozilla::dom::CacheStorageParent;

CacheStorageParent::CacheStorageParent(const nsACString& aOrigin)
  : mCacheManager(CacheManager::GetInstance())
{
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
  // TODO: implement persistent cache entry
  PCacheParent* actor = nullptr;
  if (mKeys.Contains(aKey)) {
    actor = Manager()->SendPCacheConstructor();
  }
  unused << SendGetResponse(aRequestId, actor);
  return true;
}

bool
CacheStorageParent::RecvHas(const uintptr_t& aRequestId, const nsString& aKey)
{
  // TODO: implement persistent cache entry
  unused << SendHasResponse(aRequestId, mKeys.Contains(aKey));
  return true;
}

bool
CacheStorageParent::RecvCreate(const uintptr_t& aRequestId, const nsString& aKey)
{
  // TODO: implement persistent cache entry
  PCacheParent* actor = nullptr;
  if (!mKeys.Contains(aKey)) {
    mKeys.AppendElement(aKey);
    actor = Manager()->SendPCacheConstructor();
  }
  unused << SendCreateResponse(aRequestId, actor);
  return true;
}

bool
CacheStorageParent::RecvDelete(const uintptr_t& aRequestId, const nsString& aKey)
{
  // TODO: implement persistent cache entry
  unused << SendDeleteResponse(aRequestId, mKeys.RemoveElement(aKey));
  return true;
}

bool
CacheStorageParent::RecvKeys(const uintptr_t& aRequestId)
{
  // TODO: implement persistent cache entry
  unused << SendKeysResponse(aRequestId, mKeys);
  return true;
}
