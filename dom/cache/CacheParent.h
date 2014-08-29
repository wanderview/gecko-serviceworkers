/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheParent_h
#define mozilla_dom_cache_CacheParent_h

#include "mozilla/dom/CacheDBListener.h"
#include "mozilla/dom/PCacheParent.h"

struct nsID;
template <class T> class nsRefPtr;

namespace mozilla {
namespace dom {

class CacheDBConnection;

class CacheParent MOZ_FINAL : public PCacheParent
                            , public CacheDBListener
{
public:
  CacheParent(const nsACString& aOrigin, const nsACString& aBaseDomain);
  CacheParent(const nsACString& aOrigin, const nsACString& aBaseDomain,
              const nsID& aExistingCacheId);
  virtual ~CacheParent();
  virtual void ActorDestroy(ActorDestroyReason aReason) MOZ_OVERRIDE;

  // PCacheParent method
  virtual bool
  RecvMatch(const RequestId& requestId, const PCacheRequest& request,
            const PCacheQueryParams& params) MOZ_OVERRIDE;
  virtual bool
  RecvMatchAll(const RequestId& requestId, const PCacheRequest& request,
               const PCacheQueryParams& params) MOZ_OVERRIDE;
  virtual bool
  RecvAdd(const RequestId& requestId,
          const PCacheRequest& request) MOZ_OVERRIDE;
  virtual bool
  RecvAddAll(const RequestId& requestId,
             const nsTArray<PCacheRequest>& requests) MOZ_OVERRIDE;
  virtual bool
  RecvPut(const RequestId& requestId, const PCacheRequest& request,
          const PCacheResponse& response) MOZ_OVERRIDE;
  virtual bool
  RecvDelete(const RequestId& requestId, const PCacheRequest& request,
             const PCacheQueryParams& params) MOZ_OVERRIDE;
  virtual bool
  RecvKeys(const RequestId& requestId, const PCacheRequest& request,
           const PCacheQueryParams& params) MOZ_OVERRIDE;

private:
  const nsCString mOrigin;
  const nsCString mBaseDomain;
  nsRefPtr<CacheDBConnection> mDBConnection;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheParent_h
