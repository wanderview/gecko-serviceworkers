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

// XXX: This is not safe when requests are created on both main thread and
//      worker thread.
static uint32_t sNextRequestId = 0;

CacheStorage::Request::Request()
  : mId(sNextRequestId++)
{
}

bool
CacheStorage::Request::operator==(const CacheStorage::Request& right) const
{
  return mId == right.mId;
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(CacheStorage);
NS_IMPL_CYCLE_COLLECTING_RELEASE(CacheStorage);
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CacheStorage, mOwner, mGlobal)
// TODO: traverse and unlink mRequests promises

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CacheStorage)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIIPCBackgroundChildCreateCallback)
NS_INTERFACE_MAP_END

CacheStorage::CacheStorage(nsISupports* aOwner, nsIGlobalObject* aGlobal,
                           const nsACString& aOrigin)
  : mOwner(aOwner)
  , mGlobal(aGlobal)
  , mOrigin(aOrigin)
  , mActor(nullptr)
{
  MOZ_ASSERT(mGlobal);

  // TODO: Add thread assertions
  SetIsDOMBinding();

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
                    const QueryParams& aParams)
{
  MOZ_CRASH("not implemented");
}

already_AddRefed<Promise>
CacheStorage::Get(const nsAString& aKey)
{
  MOZ_CRASH("not implemented");
}

already_AddRefed<Promise>
CacheStorage::Has(const nsAString& aKey)
{
  MOZ_CRASH("not implemented");
}

already_AddRefed<Promise>
CacheStorage::Create(const nsAString& aKey, ErrorResult& aRv)
{
  MOZ_ASSERT(mActor);

  nsRefPtr<Promise> promise = Promise::Create(mGlobal, aRv);
  if (!promise) {
    return nullptr;
  }

  Request* request = mRequests.AppendElement();
  if (!request) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  request->mPromise = promise;

  unused << mActor->SendCreate(request->mId, nsString(aKey));

  return promise.forget();
}

already_AddRefed<Promise>
CacheStorage::Delete(const nsAString& aKey)
{
  MOZ_CRASH("not implemented");
}

already_AddRefed<Promise>
CacheStorage::Keys()
{
  MOZ_CRASH("not implemented");
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
  // TODO: Add thread assertions
  MOZ_ASSERT(aActor);

  CacheStorageChild* newActor = new CacheStorageChild(*this);
  if (NS_WARN_IF(!newActor)) {
    ActorFailed();
    return;
  }

  PCacheStorageChild* constructedActor =
    aActor->SendPCacheStorageConstructor(newActor, mOrigin);

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
  // TODO: Add thread assertions
  MOZ_CRASH("not implemented");
}

void
CacheStorage::ActorDestroy(IProtocol& aActor)
{
  // TODO: Add thread assertions
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(mActor == &aActor);
  mActor->ClearListener();
  mActor = nullptr;
}

void
CacheStorage::RecvCreateResponse(uint32_t aRequestId, PCacheChild* aActor)
{
  MOZ_ASSERT(aActor);
  Request* request = FindRequestById(aRequestId);
  if (!request) {
    PCacheChild::Send__delete__(aActor);
    return;
  }
  nsRefPtr<Cache> cache = new Cache(mOwner, aActor);
  request->mPromise->MaybeResolve(cache);
  mRequests.RemoveElement(*request);
}

CacheStorage::~CacheStorage()
{
  // TODO: Add thread assertions
  if (mActor) {
    mActor->ClearListener();
    PCacheStorageChild::Send__delete__(mActor);
    // The actor will be deleted by the IPC manager
    mActor = nullptr;
  }
}

CacheStorage::Request*
CacheStorage::FindRequestById(uint32_t aRequestId)
{
  for (uint32_t i = 0; i < mRequests.Length(); ++i) {
    Request& request = mRequests.ElementAt(i);
    if (request.mId == aRequestId) {
      return &request;
    }
  }
  return nullptr;
}

} // namespace dom
} // namespace mozilla
