/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheStorageDBConnection_h
#define mozilla_dom_cache_CacheStorageDBConnection_h

#include "mozilla/dom/CacheTypes.h"
#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"

class mozIStorageConnection;
class nsID;

namespace mozilla {
namespace dom {

class CacheStorageDBListener;

class CacheStorageDBConnection MOZ_FINAL
{
public:
  static already_AddRefed<CacheStorageDBConnection>
  Get(CacheStorageDBListener& aListener, cache::Namespace aNamespace,
      const nsACString& aOrigin, const nsACString& aBaseDomain);

  static already_AddRefed<CacheStorageDBConnection>
  GetOrCreate(CacheStorageDBListener& aListener, cache::Namespace aNamespace,
              const nsACString& aOrigin, const nsACString& aBaseDomain);

  nsresult Get(cache::RequestId aRequestId, const nsAString& aKey);
  nsresult Has(cache::RequestId aRequestId, const nsAString& aKey);
  nsresult Put(cache::RequestId aRequestId, const nsAString& aKey,
               const nsID& aCacheId);
  nsresult Delete(cache::RequestId aRequestId, const nsAString& aKey);
  nsresult Keys(cache::RequestId aRequestId);

private:
  CacheStorageDBConnection(CacheStorageDBListener& aListener,
                           cache::Namespace aNamespace,
                           already_AddRefed<mozIStorageConnection> aConnection);
  ~CacheStorageDBConnection();

  static already_AddRefed<CacheStorageDBConnection>
  GetOrCreateInternal(CacheStorageDBListener& aListener,
                      cache::Namespace aNamespace, const nsACString& aOrigin,
                      const nsACString& aBaseDomain, bool allowCreate);

  static const int32_t kLatestSchemaVersion = 1;

  CacheStorageDBListener& mListener;
  const cache::Namespace mNamespace;
  nsCOMPtr<mozIStorageConnection> mConnection;

public:
  NS_INLINE_DECL_REFCOUNTING(CacheStorageDBConnection)
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheStorageDBConnection_h
