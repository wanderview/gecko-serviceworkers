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
  CacheStorageDBConnection(CacheStorageDBListener& aListener,
                           cache::Namespace aNamespace,
                           const nsACString& aOrigin,
                           const nsACString& aBaseDomain,
                           bool aAllowCreate);

  void Get(cache::RequestId aRequestId, const nsAString& aKey);
  nsresult Has(cache::RequestId aRequestId, const nsAString& aKey);
  void Put(cache::RequestId aRequestId, const nsAString& aKey,
           const nsID& aCacheId);
  nsresult Delete(cache::RequestId aRequestId, const nsAString& aKey);
  nsresult Keys(cache::RequestId aRequestId);

private:
  class OpenRunnable;
  class GetRunnable;
  class PutRunnable;

  ~CacheStorageDBConnection();
  void OnOpenComplete(nsresult aRv,
                      already_AddRefed<mozIStorageConnection> aConnection);
  void OnGetComplete(cache::RequestId aRequestId, nsresult aRv, nsID* aCacheId);
  void OnPutComplete(cache::RequestId aRequestId, nsresult aRv, bool aSuccess);

  void ReportError(nsresult aRv);

  static const int32_t kLatestSchemaVersion = 1;

  CacheStorageDBListener& mListener;
  const cache::Namespace mNamespace;
  const nsCString mOrigin;
  const nsCString mBaseDomain;
  nsCOMPtr<nsIThread> mOwningThread;
  nsCOMPtr<mozIStorageConnection> mConnection;
  bool mFailed;

public:
  NS_INLINE_DECL_REFCOUNTING(CacheStorageDBConnection)
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheStorageDBConnection_h
