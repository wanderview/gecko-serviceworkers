/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CacheStorage.h"
#include "mozilla/dom/Cache.h"
#include "mozilla/dom/CacheStorageBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(CacheStorage);
NS_IMPL_CYCLE_COLLECTING_RELEASE(CacheStorage);
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(CacheStorage, mOwner)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CacheStorage)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

CacheStorage::CacheStorage(nsISupports* aOwner, const nsACString& aOrigin)
  : mOwner(aOwner)
  , mOrigin(aOrigin)
{
  SetIsDOMBinding();
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
CacheStorage::Create(const nsAString& aKey)
{
  MOZ_CRASH("not implemented");
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

CacheStorage::~CacheStorage()
{
}

} // namespace dom
} // namespace mozilla
