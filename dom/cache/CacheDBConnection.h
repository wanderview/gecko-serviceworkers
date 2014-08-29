/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheDBConnection_h
#define mozilla_dom_cache_CacheDBConnection_h

#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"

class mozIStorageConnection;
struct nsID;

namespace mozilla {
namespace dom {

class CacheDBListener;

class CacheDBConnection MOZ_FINAL
{
public:
  static already_AddRefed<CacheDBConnection>
  Get(CacheDBListener& aListener, const nsACString& aOrigin,
      const nsACString& aBaseDomain, const nsID& aCacheId);

  static already_AddRefed<CacheDBConnection>
  Create(CacheDBListener& aListener, const nsACString& aOrigin,
         const nsACString& aBaseDomain);

private:
  CacheDBConnection(CacheDBListener& aListener,
                    const nsID& aCacheId,
                    already_AddRefed<mozIStorageConnection> aConnection);
  ~CacheDBConnection();

  static already_AddRefed<CacheDBConnection>
  GetOrCreateInternal(CacheDBListener& aListener, const nsACString& aOrigin,
                      const nsACString& aBaseDomain, const nsID& aCacheId,
                      bool allowCreate);

  static const int32_t kLatestSchemaVersion = 1;

  CacheDBListener& mListener;
  const nsID mCacheId;
  nsCOMPtr<mozIStorageConnection> mDBConnection;

public:
  NS_INLINE_DECL_REFCOUNTING(CacheDBConnection)
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheDBConnection_h
