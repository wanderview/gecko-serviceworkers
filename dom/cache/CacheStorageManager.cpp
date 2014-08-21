/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CacheStorageManager.h"

#include "mozilla/dom/CacheParent.h"

namespace mozilla {
namespace dom {

static CacheStorageManager* sInstance = nullptr;

// static
already_AddRefed<CacheStorageManager>
CacheStorageManager::GetInstance()
{
  nsRefPtr<CacheStorageManager> ref;
  if (!sInstance) {
    ref = new CacheStorageManager();
    sInstance = ref.get();
  } else {
    ref = sInstance;
  }
  return ref.forget();
}

NS_IMPL_ISUPPORTS(CacheStorageManager, nsISupports)

CacheParent*
CacheStorageManager::Get(const nsString& aKey) const
{
  if (!mKeys.Contains(aKey)) {
    return nullptr;
  }

  return new CacheParent();
}

bool
CacheStorageManager::Has(const nsString& aKey) const
{
  return mKeys.Contains(aKey);
}

bool
CacheStorageManager::Put(const nsString& aKey, CacheParent* aCache)
{
  if (mKeys.Contains(aKey)) {
    return false;
  }

  mKeys.AppendElement(aKey);

  return true;
}

bool
CacheStorageManager::Delete(const nsString& aKey)
{
  return mKeys.RemoveElement(aKey);
}

void
CacheStorageManager::Keys(nsTArray<nsString>& aKeysOut) const
{
  aKeysOut = mKeys;
}

CacheStorageManager::CacheStorageManager()
{
  MOZ_ASSERT(!sInstance);
}

CacheStorageManager::~CacheStorageManager()
{
  MOZ_ASSERT(sInstance == this);
  sInstance = nullptr;
}

} // namespace dom
} // namesapce mozilla
