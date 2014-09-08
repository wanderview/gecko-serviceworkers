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
  typedef int32_t EntryId;

  static already_AddRefed<CacheDBConnection>
  Create(CacheDBListener* aListener, const nsACString& aOrigin,
         const nsACString& aBaseDomain);

  CacheDBConnection(CacheDBListener* aListener, const nsACString& aOrigin,
                    const nsACString& aBaseDomain, const nsID& aCacheId);

  void ClearListener();

  void Match(cache::RequestId aRequestId,
             const PCacheRequest& aRequest,
             const PCacheQueryParams& aParams);

  void MatchAll(cache::RequestId aRequestId,
                const PCacheRequestOrVoid& aRequest,
                const PCacheQueryParams& aParams);

  void Put(cache::RequestId aRequestId, const PCacheRequest& aRequest,
           const PCacheResponse& aResponse);

  void Delete(cache::RequestId aRequestId, const PCacheRequest& aRequest,
              const PCacheQueryParams& aParams);

private:
  class OpenRunnable;
  class MatchRunnable;
  class MatchAllRunnable;
  class PutRunnable;
  class DeleteRunnable;

  ~CacheDBConnection();

  static already_AddRefed<CacheDBConnection>
  GetOrCreateInternal(CacheDBListener& aListener, const nsACString& aOrigin,
                      const nsACString& aBaseDomain, const nsID& aCacheId,
                      bool allowCreate);

  nsresult QueryAll(mozIStorageConnection* aConnection,
                    nsTArray<EntryId>& aEntryIdListOut);
  nsresult QueryCache(mozIStorageConnection* aConnection,
                      const PCacheRequest& aRequest,
                      const PCacheQueryParams& aParams,
                      nsTArray<EntryId>& aEntryIdListOut);
  bool MatchByVaryHeader(mozIStorageConnection* aConnection,
                         const PCacheRequest& aRequest, int32_t entryId);
  nsresult DeleteEntries(mozIStorageConnection* aConnection,
                         const nsTArray<EntryId>& aEntryIdList,
                         uint32_t aPos=0, int32_t aLen=-1);
  nsresult InsertEntry(mozIStorageConnection* aConnection,
                       const PCacheRequest& aRequest,
                       const PCacheResponse& aResponse);
  nsresult ReadResponse(mozIStorageConnection* aConnection,
                        EntryId aEntryId, PCacheResponse& aResponseOut);

  void OnMatchComplete(cache::RequestId aRequestId, nsresult aRv,
                       const PCacheResponseOrVoid& aResponse);
  void OnMatchAllComplete(cache::RequestId aRequestId, nsresult aRv,
                          const nsTArray<PCacheResponse>& aResponses);
  void OnPutComplete(cache::RequestId aRequestId, nsresult aRv,
                     const PCacheResponseOrVoid& aResponse);
  void OnDeleteComplete(cache::RequestId aRequestId, nsresult aRv,
                        bool aSuccess);

  static const int32_t kLatestSchemaVersion = 1;
  CacheDBListener* mListener;
  const nsCString mOrigin;
  const nsCString mBaseDomain;
  const nsID mCacheId;
  nsCString mQuotaId;

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CacheDBConnection)
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheDBConnection_h
