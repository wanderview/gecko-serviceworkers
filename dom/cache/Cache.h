/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Cache_h
#define mozilla_dom_Cache_h

#include "mozilla/dom/CacheChildListener.h"
#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace dom {

class CacheChild;
class OwningRequestOrScalarValueString;
class Promise;
class PCacheChild;
class RequestOrScalarValueString;
struct QueryParams;
class Response;
template<typename T> class Optional;
template<typename T> class Sequence;

class Cache MOZ_FINAL : public nsISupports
                      , public nsWrapperCache
                      , public CacheChildListener
{
public:
  Cache(nsISupports* aOwner, nsIGlobalObject* aGlobal, const nsACString& aOrigin,
        const nsACString& aBaseDomain, PCacheChild* aActor);

  // webidl interface methods
  already_AddRefed<Promise>
  Match(const RequestOrScalarValueString& aRequest, const QueryParams& aParams,
        ErrorResult& aRv);
  already_AddRefed<Promise>
  MatchAll(const Optional<RequestOrScalarValueString>& aRequest,
           const QueryParams& aParams, ErrorResult& aRv);
  already_AddRefed<Promise>
  Add(const RequestOrScalarValueString& aRequest, ErrorResult& aRv);
  already_AddRefed<Promise>
  AddAll(const Sequence<OwningRequestOrScalarValueString>& aRequests,
         ErrorResult& aRv);
  already_AddRefed<Promise>
  Put(const RequestOrScalarValueString& aRequest, const Response& aResponse,
      ErrorResult& aRv);
  already_AddRefed<Promise>
  Delete(const RequestOrScalarValueString& aRequest, const QueryParams& aParams,
         ErrorResult& aRv);
  already_AddRefed<Promise>
  Keys(const Optional<RequestOrScalarValueString>& aRequest,
       const QueryParams& aParams, ErrorResult& aRv);

  // binding methods
  static bool PrefEnabled(JSContext* aCx, JSObject* aObj);

  virtual nsISupports* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aContext) MOZ_OVERRIDE;

  // CacheChildListener methods
  virtual void ActorDestroy(mozilla::ipc::IProtocol& aActor) MOZ_OVERRIDE;
  virtual void
  RecvMatchResponse(cache::RequestId aRequestId, nsresult aRv,
                    const PCacheResponseOrVoid& aResponse) MOZ_OVERRIDE;
  virtual void
  RecvMatchAllResponse(cache::RequestId aRequestId, nsresult aRv,
                       const nsTArray<PCacheResponse>& aResponses) MOZ_OVERRIDE;
  virtual void
  RecvAddResponse(cache::RequestId aRequestId, nsresult aRv,
                  const PCacheResponse& aResponse) MOZ_OVERRIDE;
  virtual void
  RecvAddAllResponse(cache::RequestId aRequestId, nsresult aRv,
                     const nsTArray<PCacheResponse>& aResponses) MOZ_OVERRIDE;
  virtual void
  RecvPutResponse(cache::RequestId aRequestId, nsresult aRv,
                  const PCacheResponseOrVoid& aResponse) MOZ_OVERRIDE;
  virtual void
  RecvDeleteResponse(cache::RequestId aRequestId, nsresult aRv,
                     bool aSuccess) MOZ_OVERRIDE;
  virtual void
  RecvKeysResponse(cache::RequestId aRequestId, nsresult aRv,
                   const nsTArray<PCacheRequest>& aRequests) MOZ_OVERRIDE;

private:
  virtual ~Cache();

  cache::RequestId AddRequestPromise(Promise* aPromise, ErrorResult& aRv);
  already_AddRefed<Promise> RemoveRequestPromise(cache::RequestId aRequestId);

private:
  nsCOMPtr<nsISupports> mOwner;
  nsCOMPtr<nsIGlobalObject> mGlobal;
  const nsCString mOrigin;
  const nsCString mBaseDomain;
  CacheChild* mActor;
  nsTArray<nsRefPtr<Promise>> mRequestPromises;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Cache)
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Cache_h
