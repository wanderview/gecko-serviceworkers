/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheStorageDBConnection_h
#define mozilla_dom_cache_CacheStorageDBConnection_h

#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"

class mozIStorageConnection;
class nsID;

namespace mozilla {
namespace dom {

class CacheStorageDBConnection MOZ_FINAL
{
public:
  static already_AddRefed<CacheStorageDBConnection>
  Get(const nsACString& aOrigin, const nsACString& aBaseDomain);

  static already_AddRefed<CacheStorageDBConnection>
  GetOrCreate(const nsACString& aOrigin, const nsACString& aBaseDomain);

  already_AddRefed<nsID> Get(const nsAString& aKey);
  bool Has(const nsAString& aKey);
  bool Put(const nsAString& aKey, nsID* aCacheId);
  bool Delete(const nsAString& aKey);
  void Keys(nsTArray<nsString> aKeysOut);

private:
  CacheStorageDBConnection(already_AddRefed<mozIStorageConnection> aConnection);
  ~CacheStorageDBConnection();

  static already_AddRefed<CacheStorageDBConnection>
  GetOrCreateInternal(const nsACString& aOrigin, const nsACString& aBaseDomain,
                      bool allowCreate);

  static const int32_t kLatestSchemaVersion = 1;

  nsCOMPtr<mozIStorageConnection> mConnection;

public:
  NS_INLINE_DECL_REFCOUNTING(CacheStorageDBConnection)
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheStorageDBConnection_h
