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
template<class T> class nsTArray;

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

  nsresult MatchAll(cache::RequestId aRequestId, const PCacheRequest& aRequest,
                    const PCacheQueryParams& aParams);

  nsresult Put(cache::RequestId aRequestId, const PCacheRequest& aRequest,
               const PCacheResponse& aResponse);

private:
  CacheDBConnection(CacheDBListener& aListener,
                    const nsID& aCacheId,
                    already_AddRefed<mozIStorageConnection> aConnection);
  ~CacheDBConnection();

  static already_AddRefed<CacheDBConnection>
  GetOrCreateInternal(CacheDBListener& aListener, const nsACString& aOrigin,
                      const nsACString& aBaseDomain, const nsID& aCacheId,
                      bool allowCreate);

  struct QueryResult
  {
    PCacheRequest request;
    PCacheResponse response;
  };

  nsresult QueryCache(cache::RequestId aRequestId,
                      const PCacheRequest& aRequest,
                      const PCacheQueryParams& aParams,
                      nsTArray<QueryResult>& aResponsesOut);

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
