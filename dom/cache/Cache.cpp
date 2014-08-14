/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Cache.h"
#include "mozilla/dom/CacheBinding.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/Preferences.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(Cache);
NS_IMPL_CYCLE_COLLECTING_RELEASE(Cache);
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Cache, mOwner)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Cache)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Cache::Cache(nsISupports* aOwner)
: mOwner(aOwner)
{
  SetIsDOMBinding();
}

already_AddRefed<Promise>
Cache::Match(const RequestOrScalarValueString& aRequest,
             const QueryParams& aParams)
{
  MOZ_CRASH("not implemented");
}

already_AddRefed<Promise>
Cache::MatchAll(const RequestOrScalarValueString& aRequest,
                const QueryParams& aParams)
{
  MOZ_CRASH("not implemented");
}

already_AddRefed<Promise>
Cache::Add(const RequestOrScalarValueString& aRequests)
{
  MOZ_CRASH("not implemented");
}

already_AddRefed<Promise>
Cache::AddAll(const Sequence<OwningRequestOrScalarValueString>& aRequests)
{
  MOZ_CRASH("not implemented");
}

already_AddRefed<Promise>
Cache::Put(const RequestOrScalarValueString& aRequest,
           const Response& aResponse)
{
  MOZ_CRASH("not implemented");
}

already_AddRefed<Promise>
Cache::Delete(const RequestOrScalarValueString& aRequest,
              const QueryParams& aParams)
{
  MOZ_CRASH("not implemented");
}

already_AddRefed<Promise>
Cache::Keys(const Optional<RequestOrScalarValueString>& aRequest,
            const QueryParams& aParams)
{
  MOZ_CRASH("not implemented");
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

Cache::~Cache()
{
}

} // namespace dom
} // namespace mozilla
