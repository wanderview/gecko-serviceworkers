/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Cache.h"

#include "mozilla/dom/CacheBinding.h"
#include "mozilla/dom/CacheChild.h"
#include "mozilla/dom/Headers.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Preferences.h"
#include "mozilla/unused.h"
#include "nsIGlobalObject.h"

namespace mozilla {
namespace dom {

using mozilla::ErrorResult;
using mozilla::unused;
using mozilla::dom::cache::INVALID_REQUEST_ID;
using mozilla::dom::cache::RequestId;

static void
ToPCacheRequest(PCacheRequest& aOut, const Request& aIn)
{
  aIn.GetMethod(aOut.method());
  aIn.GetUrl(aOut.url());
  nsRefPtr<Headers> headers = aIn.HeadersValue();
  MOZ_ASSERT(headers);
  aOut.headers() = headers->AsPHeaders();
  aOut.mode() = aIn.Mode();
  aOut.credentials() = aIn.Credentials();
  aOut.null() = false;
}

static void
ToPCacheRequest(PCacheRequest& aOut, const RequestOrScalarValueString& aIn)
{
  nsRefPtr<Request> request;
  if (aIn.IsRequest()) {
    request = &aIn.GetAsRequest();
  } else {
    MOZ_ASSERT(aIn.IsScalarValueString());
    // TODO: see nsIStandardURL.init() if Request does not provide something...
    MOZ_CRASH("implement me");
  }
  ToPCacheRequest(aOut, *request);
}

static void
ToPCacheRequest(PCacheRequest& aOut,
                const Optional<RequestOrScalarValueString>& aIn)
{
  if (!aIn.WasPassed()) {
    aOut.null() = true;
    return;
  }
  ToPCacheRequest(aOut, aIn.Value());
}

static void
ToPCacheRequest(PCacheRequest& aOut,
                const OwningRequestOrScalarValueString& aIn)
{
  nsRefPtr<Request> request;
  if (aIn.IsRequest()) {
    request = &static_cast<Request&>(aIn.GetAsRequest());
  } else {
    MOZ_ASSERT(aIn.IsScalarValueString());
    MOZ_CRASH("implement me");
  }
  ToPCacheRequest(aOut, *request);
}

static void
ToPCacheResponse(PCacheResponse& aOut, const Response& aIn)
{
  aOut.type() = aIn.Type();
  aOut.status() = aIn.Status();
  aIn.GetStatusText(aOut.statusText());
  nsRefPtr<Headers> headers = aIn.Headers_();
  MOZ_ASSERT(headers);
  aOut.headers() = headers->AsPHeaders();
}

static void
ToPCacheQueryParams(PCacheQueryParams& aOut, const QueryParams& aIn)
{
  aOut.ignoreSearch() = aIn.mIgnoreSearch.WasPassed() &&
                        aIn.mIgnoreSearch.Value();
  aOut.ignoreMethod() = aIn.mIgnoreMethod.WasPassed() &&
                        aIn.mIgnoreMethod.Value();
  aOut.ignoreVary() = aIn.mIgnoreVary.WasPassed() &&
                      aIn.mIgnoreVary.Value();
  aOut.prefixMatch() = aIn.mPrefixMatch.WasPassed() &&
                       aIn.mPrefixMatch.Value();
  aOut.cacheNameSet() = aIn.mCacheName.WasPassed();
  if (aOut.cacheNameSet()) {
    aOut.cacheName() = aIn.mCacheName.Value();
  }
}

static void
ToResponse(Response& aOut, const PCacheResponse& aIn)
{
  // TODO: implement once real Request/Response are available
  NS_WARNING("Not filling in contents of Response returned from Cache.");
}

static void
ToRequest(Request& aOut, const PCacheRequest& aIn)
{
  // TODO: implement once real Request/Response are available
  NS_WARNING("Not filling in contents of Request returned from Cache.");
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(mozilla::dom::Cache);
NS_IMPL_CYCLE_COLLECTING_RELEASE(mozilla::dom::Cache);
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Cache, mOwner, mGlobal)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Cache)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Cache::Cache(nsISupports* aOwner, nsIGlobalObject* aGlobal, PCacheChild* aActor)
  : mOwner(aOwner)
  , mGlobal(aGlobal)
  , mActor(static_cast<CacheChild*>(aActor))
{
  MOZ_ASSERT(mActor);
  SetIsDOMBinding();
  mActor->SetListener(*this);
}

already_AddRefed<Promise>
Cache::Match(const RequestOrScalarValueString& aRequest,
             const QueryParams& aParams, ErrorResult& aRv)
{
  MOZ_ASSERT(mActor);

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  RequestId requestId = AddRequestPromise(promise, aRv);
  if (requestId == INVALID_REQUEST_ID) {
    return nullptr;
  }

  PCacheRequest request;
  ToPCacheRequest(request, aRequest);

  PCacheQueryParams params;
  ToPCacheQueryParams(params, aParams);

  unused << mActor->SendMatch(requestId, request, params);

  return promise.forget();
}

already_AddRefed<Promise>
Cache::MatchAll(const RequestOrScalarValueString& aRequest,
                const QueryParams& aParams, ErrorResult& aRv)
{
  MOZ_ASSERT(mActor);

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  RequestId requestId = AddRequestPromise(promise, aRv);
  if (requestId == INVALID_REQUEST_ID) {
    return nullptr;
  }

  PCacheRequest request;
  ToPCacheRequest(request, aRequest);

  PCacheQueryParams params;
  ToPCacheQueryParams(params, aParams);

  unused << mActor->SendMatchAll(requestId, request, params);

  return promise.forget();
}

already_AddRefed<Promise>
Cache::Add(const RequestOrScalarValueString& aRequest, ErrorResult& aRv)
{
  MOZ_ASSERT(mActor);

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  RequestId requestId = AddRequestPromise(promise, aRv);
  if (requestId == INVALID_REQUEST_ID) {
    return nullptr;
  }

  PCacheRequest request;
  ToPCacheRequest(request, aRequest);

  unused << mActor->SendAdd(requestId, request);

  return promise.forget();
}

already_AddRefed<Promise>
Cache::AddAll(const Sequence<OwningRequestOrScalarValueString>& aRequests,
              ErrorResult& aRv)
{
  MOZ_ASSERT(mActor);

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  RequestId requestId = AddRequestPromise(promise, aRv);
  if (requestId == INVALID_REQUEST_ID) {
    return nullptr;
  }

  nsTArray<PCacheRequest> requests;
  for(uint32_t i = 0; i < aRequests.Length(); ++i) {
    PCacheRequest* request = requests.AppendElement();
    if (!request) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
    ToPCacheRequest(*request, aRequests[i]);
  }

  unused << mActor->SendAddAll(requestId, requests);

  return promise.forget();
}

already_AddRefed<Promise>
Cache::Put(const RequestOrScalarValueString& aRequest,
           const Response& aResponse, ErrorResult& aRv)
{
  MOZ_ASSERT(mActor);

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  RequestId requestId = AddRequestPromise(promise, aRv);
  if (requestId == INVALID_REQUEST_ID) {
    return nullptr;
  }

  PCacheRequest request;
  ToPCacheRequest(request, aRequest);

  PCacheResponse response;
  ToPCacheResponse(response, aResponse);

  unused << mActor->SendPut(requestId, request, response);

  return promise.forget();
}

already_AddRefed<Promise>
Cache::Delete(const RequestOrScalarValueString& aRequest,
              const QueryParams& aParams, ErrorResult& aRv)
{
  MOZ_ASSERT(mActor);

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  RequestId requestId = AddRequestPromise(promise, aRv);
  if (requestId == INVALID_REQUEST_ID) {
    return nullptr;
  }

  PCacheRequest request;
  ToPCacheRequest(request, aRequest);

  PCacheQueryParams params;
  ToPCacheQueryParams(params, aParams);

  unused << mActor->SendDelete(requestId, request, params);

  return promise.forget();
}

already_AddRefed<Promise>
Cache::Keys(const Optional<RequestOrScalarValueString>& aRequest,
            const QueryParams& aParams, ErrorResult& aRv)
{
  MOZ_ASSERT(mActor);

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  RequestId requestId = AddRequestPromise(promise, aRv);
  if (requestId == INVALID_REQUEST_ID) {
    return nullptr;
  }

  PCacheRequest request;
  ToPCacheRequest(request, aRequest);

  PCacheQueryParams params;
  ToPCacheQueryParams(params, aParams);

  unused << mActor->SendKeys(requestId, request, params);

  return promise.forget();
}

// static
bool
Cache::PrefEnabled(JSContext* aCx, JSObject* aObj)
{
  using mozilla::dom::workers::WorkerPrivate;
  using mozilla::dom::workers::GetWorkerPrivateFromContext;

  // In the long term we want to support Cache on main-thread, so
  // allow it to be exposed there via a pref.
  if (NS_IsMainThread()) {
    static bool sPrefCacheInit = false;
    static bool sPrefEnabled = false;
    if (sPrefCacheInit) {
      return sPrefEnabled;
    }
    Preferences::AddBoolVarCache(&sPrefEnabled, "dom.window-caches.enabled");
    sPrefCacheInit = true;
    return sPrefEnabled;
  }

  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  if (!workerPrivate) {
    return false;
  }

  // Otherwise expose on ServiceWorkers.  Also expose on others workers if
  // pref enabled.
  return workerPrivate->IsServiceWorker() || workerPrivate->DOMCachesEnabled();
}

nsISupports*
Cache::GetParentObject() const
{
  return mOwner;
}

JSObject*
Cache::WrapObject(JSContext* aContext)
{
  return CacheBinding::Wrap(aContext, this);
}

void
Cache::ActorDestroy(mozilla::ipc::IProtocol& aActor)
{
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mActor == &aActor);
  mActor->ClearListener();
  mActor = nullptr;
}

void
Cache::RecvMatchResponse(RequestId aRequestId, const PCacheResponse& aResponse)
{
  nsRefPtr<Promise> promise = RemoveRequestPromise(aRequestId);
  if (NS_WARN_IF(!promise)) {
    return;
  }
  nsRefPtr<Response> response = new Response(mOwner);
  if (NS_WARN_IF(!response)) {
    return;
  }
  ToResponse(*response, aResponse);
  promise->MaybeResolve(response);
}

void
Cache::RecvMatchAllResponse(RequestId aRequestId,
                            const nsTArray<PCacheResponse>& aResponses)
{
  nsRefPtr<Promise> promise = RemoveRequestPromise(aRequestId);
  if (NS_WARN_IF(!promise)) {
    return;
  }
  nsTArray<nsRefPtr<Response>> responses;
  for (uint32_t i = 0; i < aResponses.Length(); ++i) {
    nsRefPtr<Response> response = new Response(mOwner);
    if (NS_WARN_IF(!response)) {
      return;
    }
    ToResponse(*response, aResponses[i]);
    responses.AppendElement(response);
  }
  promise->MaybeResolve(responses);
}

void
Cache::RecvAddResponse(RequestId aRequestId, const PCacheResponse& aResponse)
{
  nsRefPtr<Promise> promise = RemoveRequestPromise(aRequestId);
  if (NS_WARN_IF(!promise)) {
    return;
  }
  nsRefPtr<Response> response = new Response(mOwner);
  if (NS_WARN_IF(!response)) {
    return;
  }
  ToResponse(*response, aResponse);
  promise->MaybeResolve(response);
}

void
Cache::RecvAddAllResponse(RequestId aRequestId,
                          const nsTArray<PCacheResponse>& aResponses)
{
  nsRefPtr<Promise> promise = RemoveRequestPromise(aRequestId);
  if (NS_WARN_IF(!promise)) {
    return;
  }
  nsTArray<nsRefPtr<Response>> responses;
  for (uint32_t i = 0; i < aResponses.Length(); ++i) {
    nsRefPtr<Response> response = new Response(mOwner);
    if (NS_WARN_IF(!response)) {
      return;
    }
    ToResponse(*response, aResponses[i]);
    responses.AppendElement(response);
  }
  promise->MaybeResolve(responses);
}

void
Cache::RecvPutResponse(RequestId aRequestId, const PCacheResponse& aResponse)
{
  nsRefPtr<Promise> promise = RemoveRequestPromise(aRequestId);
  if (NS_WARN_IF(!promise)) {
    return;
  }
  nsRefPtr<Response> response = new Response(mOwner);
  if (NS_WARN_IF(!response)) {
    return;
  }
  ToResponse(*response, aResponse);
  promise->MaybeResolve(response);
}

void
Cache::RecvDeleteResponse(RequestId aRequestId, bool aResult)
{
  nsRefPtr<Promise> promise = RemoveRequestPromise(aRequestId);
  if (NS_WARN_IF(!promise)) {
    return;
  }
  promise->MaybeResolve(aResult);
}

void
Cache::RecvKeysResponse(RequestId aRequestId,
                        const nsTArray<PCacheRequest>& aRequests)
{
  nsRefPtr<Promise> promise = RemoveRequestPromise(aRequestId);
  if (NS_WARN_IF(!promise)) {
    return;
  }
  nsTArray<nsRefPtr<Request>> requests;
  for (uint32_t i = 0; i < aRequests.Length(); ++i) {
    MOZ_CRASH("not implemented - can't construct new Request()");
    //nsRefPtr<Request> request = new Request(mOwner);
    nsRefPtr<Request> request = nullptr;
    if (NS_WARN_IF(!request)) {
      return;
    }
    ToRequest(*request, aRequests[i]);
    requests.AppendElement(request);
  }
  promise->MaybeResolve(requests);
}


Cache::~Cache()
{
  if (mActor) {
    mActor->ClearListener();
    PCacheChild::Send__delete__(mActor);
    // The actor will be deleted by the IPC manager
    mActor = nullptr;
  }
}

RequestId
Cache::AddRequestPromise(Promise* aPromise, ErrorResult& aRv)
{
  MOZ_ASSERT(aPromise);

  nsRefPtr<Promise>* ref = mRequestPromises.AppendElement();
  if (!ref) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return INVALID_REQUEST_ID;
  }

  *ref = aPromise;

  // (Ab)use the promise pointer as our request ID.  This is a fast, thread-safe
  // way to get a unique ID for the promise to be resolved later.
  return reinterpret_cast<RequestId>(aPromise);
}

already_AddRefed<Promise>
Cache::RemoveRequestPromise(RequestId aRequestId)
{
  MOZ_ASSERT(aRequestId != INVALID_REQUEST_ID);

  for (uint32_t i = 0; i < mRequestPromises.Length(); ++i) {
    nsRefPtr<Promise>& promise = mRequestPromises.ElementAt(i);
    // To be safe, only cast promise pointers to our integer RequestId
    // type and never cast an integer to a pointer.
    if (aRequestId == reinterpret_cast<RequestId>(promise.get())) {
      nsRefPtr<Promise> ref;
      ref.swap(promise);
      mRequestPromises.RemoveElementAt(i);
      return ref.forget();
    }
  }
  return nullptr;
}

} // namespace dom
} // namespace mozilla
