/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CacheStorage_h
#define mozilla_dom_CacheStorage_h

#include "nsISupportsImpl.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class Promise;
class QueryParams;
class RequestOrScalarValueString;

class CacheStorage MOZ_FINAL : public nsISupports
                             , public nsWrapperCache
{
public:
  explicit CacheStorage(nsISupports* aOwner, const nsACString& aOrigin);

  // webidl interface methods
  already_AddRefed<Promise> Match(const RequestOrScalarValueString& aRequest,
                                  const QueryParams& aParams);
  already_AddRefed<Promise> Get(const nsAString& aKey);
  already_AddRefed<Promise> Has(const nsAString& aKey);
  already_AddRefed<Promise> Create(const nsAString& aKey);
  already_AddRefed<Promise> Delete(const nsAString& aKey);
  already_AddRefed<Promise> Keys();

  // binding methods
  static bool PrefEnabled(JSContext* aCx, JSObject* aObj);

  virtual nsISupports* GetParentObject() const MOZ_OVERRIDE;
  virtual JSObject* WrapObject(JSContext* aContext) MOZ_OVERRIDE;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(CacheStorage)

private:
  virtual ~CacheStorage();

  nsCOMPtr<nsISupports> mOwner;
  const nsCString mOrigin;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CacheStorage_h
