/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CacheManager.h"

using mozilla::dom::CacheManager;

static CacheManager* sInstance = nullptr;

// static
already_AddRefed<CacheManager>
CacheManager::GetInstance()
{
  nsRefPtr<CacheManager> ref;
  if (!sInstance) {
    ref = new CacheManager();
    sInstance = ref.get();
  } else {
    nsRefPtr<CacheManager> ref = sInstance;
  }
  return ref.forget();
}

NS_IMPL_ISUPPORTS(CacheManager, nsISupports)

CacheManager::CacheManager()
{
  MOZ_ASSERT(!sInstance);
  printf_stderr("### ### CacheManager()\n");
}

CacheManager::~CacheManager()
{
  MOZ_ASSERT(sInstance == this);
  sInstance = nullptr;
  printf_stderr("### ### ~CacheManager()\n");
}
