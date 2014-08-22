/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CacheStorage.h"

#include "mozilla/unused.h"
#include "mozilla/dom/Cache.h"
#include "mozilla/dom/CacheStorageBinding.h"
#include "mozilla/dom/CacheStorageChild.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"

namespace mozilla {
namespace dom {

using mozilla::unused;
using mozilla::ErrorResult;
using mozilla::ipc::BackgroundChild;
using mozilla::ipc::PBackgroundChild;
using mozilla::ipc::IProtocol;

NS_IMPL_CYCLE_COLLECTING_ADDREF(CacheStorage);
NS_IMPL_CYCLE_COLLECTING_RELEASE(CacheStorage);
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CacheStorage, mOwner,
                                                    mGlobal,
                                                    mRequestPromises)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CacheStorage)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIIPCBackgroundChildCreateCallback)
NS_INTERFACE_MAP_END

CacheStorage::CacheStorage(nsISupports* aOwner, nsIGlobalObject* aGlobal,
                           const nsACString& aOrigin,
                           const nsACString& aBaseDomain)
  : mOwner(aOwner)
  , mGlobal(aGlobal)
  , mOrigin(aOrigin)
  , mBaseDomain(aBaseDomain)
  , mActor(nullptr)
{
  MOZ_ASSERT(mGlobal);

  SetIsDOMBinding();

  if (mOrigin.EqualsLiteral("null") || mBaseDomain.EqualsLiteral("")) {
    ActorFailed();
    return;
  }

  PBackgroundChild* actor = BackgroundChild::GetForCurrentThread();
  if (actor) {
    ActorCreated(actor);
  } else {
    bool ok = BackgroundChild::GetOrCreateForCurrentThread(this);
    if (!ok) {
      ActorFailed();
    }
  }
}

already_AddRefed<Promise>
CacheStorage::Match(const RequestOrScalarValueString& aRequest,
                    const QueryParams& aParams, ErrorResult& aRv)
{
  MOZ_CRASH("not implemented");
}

already_AddRefed<Promise>
CacheStorage::Get(const nsAString& aKey, ErrorResult& aRv)
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

  unused << mActor->SendGet(requestId, nsString(aKey));

  return promise.forget();
}

already_AddRefed<Promise>
CacheStorage::Has(const nsAString& aKey, ErrorResult& aRv)
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

  unused << mActor->SendHas(requestId, nsString(aKey));

  return promise.forget();
}

already_AddRefed<Promise>
CacheStorage::Create(const nsAString& aKey, ErrorResult& aRv)
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

  unused << mActor->SendCreate(requestId, nsString(aKey));

  return promise.forget();
}

already_AddRefed<Promise>
CacheStorage::Delete(const nsAString& aKey, ErrorResult& aRv)
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

  unused << mActor->SendDelete(requestId, nsString(aKey));

  return promise.forget();
}

already_AddRefed<Promise>
CacheStorage::Keys(ErrorResult& aRv)
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

  unused << mActor->SendKeys(requestId);

  return promise.forget();
}

// static
bool
CacheStorage::PrefEnabled(JSContext* aCx, JSObject* aObj)
{
  return Cache::PrefEnabled(aCx, aObj);
}

nsISupports*
CacheStorage::GetParentObject() const
{
  return mOwner;
}

JSObject*
CacheStorage::WrapObject(JSContext* aContext)
{
  return mozilla::dom::CacheStorageBinding::Wrap(aContext, this);
}

void
CacheStorage::ActorCreated(PBackgroundChild* aActor)
{
  MOZ_ASSERT(aActor);

  CacheStorageChild* newActor = new CacheStorageChild(*this);
  if (NS_WARN_IF(!newActor)) {
    ActorFailed();
    return;
  }

  PCacheStorageChild* constructedActor =
    aActor->SendPCacheStorageConstructor(newActor, mOrigin, mBaseDomain);

  if (NS_WARN_IF(!constructedActor)) {
    ActorFailed();
    return;
  }

  MOZ_ASSERT(constructedActor == newActor);
  mActor = newActor;
}

void
CacheStorage::ActorFailed()
{
  // TODO: This should reject any pending Promises and cause all future
  //       Promises created by this object to immediately reject.
  MOZ_CRASH("not implemented");
}

void
CacheStorage::ActorDestroy(IProtocol& aActor)
{
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mActor == &aActor);
  mActor->ClearListener();
  mActor = nullptr;
}

void
CacheStorage::RecvGetResponse(uint64_t aRequestId, PCacheChild* aActor)
{
  nsRefPtr<Promise> promise = RemoveRequestPromise(aRequestId);
  if (NS_WARN_IF(!promise)) {
    if (aActor) {
      PCacheChild::Send__delete__(aActor);
    }
    return;
  }

  if (!aActor) {
    promise->MaybeResolve(JS::UndefinedHandleValue);
    return;
  }

  nsRefPtr<Cache> cache = new Cache(mOwner, aActor);
  promise->MaybeResolve(cache);
}

void
CacheStorage::RecvHasResponse(uintptr_t aRequestId, bool aResult)
{
  nsRefPtr<Promise> promise = RemoveRequestPromise(aRequestId);
  if (NS_WARN_IF(!promise)) {
    return;
  }
  promise->MaybeResolve(aResult);
}

void
CacheStorage::RecvCreateResponse(uint64_t aRequestId, PCacheChild* aActor)
{
  nsRefPtr<Promise> promise = RemoveRequestPromise(aRequestId);
  if (NS_WARN_IF(!promise)) {
    if (aActor) {
      PCacheChild::Send__delete__(aActor);
    }
    return;
  }

  if (!aActor) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }

  nsRefPtr<Cache> cache = new Cache(mOwner, aActor);
  promise->MaybeResolve(cache);
}

void
CacheStorage::RecvDeleteResponse(uintptr_t aRequestId, bool aResult)
{
  nsRefPtr<Promise> promise = RemoveRequestPromise(aRequestId);
  if (NS_WARN_IF(!promise)) {
    return;
  }
  promise->MaybeResolve(aResult);
}

void
CacheStorage::RecvKeysResponse(const uintptr_t& aRequestId,
                               const nsTArray<nsString>& aKeys)
{
  nsRefPtr<Promise> promise = RemoveRequestPromise(aRequestId);
  if (NS_WARN_IF(!promise)) {
    return;
  }
  promise->MaybeResolve(aKeys);
}

CacheStorage::~CacheStorage()
{
  if (mActor) {
    mActor->ClearListener();
    PCacheStorageChild::Send__delete__(mActor);
    // The actor will be deleted by the IPC manager
    mActor = nullptr;
  }
}

CacheStorage::RequestId
CacheStorage::AddRequestPromise(Promise* aPromise, ErrorResult& aRv)
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
CacheStorage::RemoveRequestPromise(RequestId aRequestId)
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
