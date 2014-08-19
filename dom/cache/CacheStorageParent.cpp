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
CacheStorageParent::RecvCreate(const uint32_t& aRequestId, const nsString& aKey)
{
  PCacheParent* actor = Manager()->SendPCacheConstructor();
  printf_stderr("### ### CacheStorageParent::RecvCreate(%lu)\n", aRequestId);
  unused << SendCreateResponse(aRequestId, actor);
  return true;
}
