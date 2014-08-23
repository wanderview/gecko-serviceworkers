/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheStorageParent_h
#define mozilla_dom_cache_CacheStorageParent_h

#include "mozilla/dom/CacheStorageDBListener.h"
#include "mozilla/dom/PCacheStorageParent.h"

template <class T> class nsRefPtr;

namespace mozilla {
namespace dom {

class CacheStorageDBConnection;
class CacheStorageManager;

class CacheStorageParent MOZ_FINAL : public PCacheStorageParent
                                   , public CacheStorageDBListener
{
public:
  CacheStorageParent(const nsACString& aOrigin, const nsACString& mBaseDomain);
  virtual ~CacheStorageParent();

  // PCacheStorageParent methods
  virtual void ActorDestroy(ActorDestroyReason aReason) MOZ_OVERRIDE;
  virtual bool RecvGet(const uintptr_t& aRequestId,
                       const nsString& aKey) MOZ_OVERRIDE;
  virtual bool RecvHas(const uintptr_t& aRequestId,
                       const nsString& aKey) MOZ_OVERRIDE;
  virtual bool RecvCreate(const uintptr_t& aRequestId,
                          const nsString& aKey) MOZ_OVERRIDE;
  virtual bool RecvDelete(const uintptr_t& aRequestId,
                          const nsString& aKey) MOZ_OVERRIDE;
  virtual bool RecvKeys(const uintptr_t& aRequestId) MOZ_OVERRIDE;

  // CacheStorageDBListener
  virtual void OnGet(uintptr_t aRequestId, const nsID& aCacheId) MOZ_OVERRIDE;
  virtual void OnHas(uintptr_t aRequestId, bool aResult) MOZ_OVERRIDE;
  virtual void OnPut(uintptr_t aRequestId, bool aResult) MOZ_OVERRIDE;
  virtual void OnDelete(uintptr_t aRequestId, bool aResult) MOZ_OVERRIDE;
  virtual void OnKeys(uintptr_t aRequestId,
                      const nsTArray<nsString>& aKeys) MOZ_OVERRIDE;

private:
  CacheStorageDBConnection* GetDBConnection();
  CacheStorageDBConnection* GetOrCreateDBConnection();

  const nsCString mOrigin;
  const nsCString mBaseDomain;
  nsRefPtr<CacheStorageDBConnection> mDBConnection;
  nsRefPtr<CacheStorageManager> mCacheStorageManager;
  nsTArray<nsString> mKeys;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheStorageParent_h
