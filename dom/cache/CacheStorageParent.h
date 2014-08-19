/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheStorageParent_h
#define mozilla_dom_cache_CacheStorageParent_h

#include "mozilla/dom/PCacheStorageParent.h"

template <class T> class nsRefPtr;

namespace mozilla {
namespace dom {

class CacheManager;

class CacheStorageParent MOZ_FINAL : public PCacheStorageParent
{
public:
  CacheStorageParent(const nsACString& aOrigin);
  virtual ~CacheStorageParent();
  virtual void ActorDestroy(ActorDestroyReason aReason) MOZ_OVERRIDE;
  virtual bool RecvCreate(const uint64_t& aRequestId, const nsString& aKey) MOZ_OVERRIDE;
private:
  nsRefPtr<CacheManager> mCacheManager;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheStorageParent_h
