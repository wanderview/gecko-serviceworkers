/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CacheStorageManager_h
#define mozilla_dom_CacheStorageManager_h

#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {

class CacheParent;

class CacheStorageManager MOZ_FINAL : public nsISupports
{
public:
  static already_AddRefed<CacheStorageManager> GetInstance();

  CacheParent* Get(const nsString& aKey) const;
  bool Has(const nsString& aKey) const;
  bool Put(const nsString& aKey, CacheParent* aCache);
  bool Delete(const nsString& aKey);
  void Keys(nsTArray<nsString>& aKeysOut) const;

private:
  CacheStorageManager();
  virtual ~CacheStorageManager();

  CacheStorageManager(const CacheStorageManager&) MOZ_DELETE;
  CacheStorageManager operator=(const CacheStorageManager&) MOZ_DELETE;

  nsTArray<nsString> mKeys;

public:
  NS_DECL_ISUPPORTS
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CacheStorageManager_h
