/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Cache_h
#define mozilla_dom_Cache_h

#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class Promise;
class OwningRequestOrScalarValueString;
class RequestOrScalarValueString;
class QueryParams;
class Response;
template<typename T> class Optional;
template<typename T> class Sequence;

class Cache MOZ_FINAL : public nsISupports
                      , public nsWrapperCache
{
public:
  // webidl interface methods
  already_AddRefed<Promise> Match(const RequestOrScalarValueString& aRequest,
                                  const QueryParams& aParams);
  already_AddRefed<Promise> MatchAll(const RequestOrScalarValueString& aRequest,
                                     const QueryParams& aParams);
  already_AddRefed<Promise> Add(const RequestOrScalarValueString& aRequest);
  already_AddRefed<Promise> AddAll(const Sequence<OwningRequestOrScalarValueString>& aRequests);
  already_AddRefed<Promise> Put(const RequestOrScalarValueString& aRequest,
                                const Response& aResponse);
  already_AddRefed<Promise> Delete(const RequestOrScalarValueString& aRequest,
                                   const QueryParams& aParams);
  already_AddRefed<Promise> Keys(const Optional<RequestOrScalarValueString>& aRequest,
                                 const QueryParams& aParams);

  // binding methods
  static bool PrefEnabled(JSContext* aCx, JSObject* aObj);

  virtual nsISupports* GetParentObject() const MOZ_OVERRIDE;
  virtual JSObject* WrapObject(JSContext* aContext) MOZ_OVERRIDE;

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Cache)

private:
  Cache(nsISupports* aOwner);
  virtual ~Cache();

private:
  nsCOMPtr<nsISupports> mOwner;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Cache_h
