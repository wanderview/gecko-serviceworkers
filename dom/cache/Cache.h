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

namespace mozilla {
namespace dom {

class CacheChild;
class OwningRequestOrScalarValueString;
class Promise;
class PCacheChild;
class RequestOrScalarValueString;
class QueryParams;
class Response;
template<typename T> class Optional;
template<typename T> class Sequence;

class Cache MOZ_FINAL : public nsISupports
                      , public nsWrapperCache
                      , public CacheChildListener
{
public:
  Cache(nsISupports* aOwner, PCacheChild* aActor);

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

  virtual nsISupports* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aContext) MOZ_OVERRIDE;

  // CacheChildListener methods
  virtual void ActorDestroy(mozilla::ipc::IProtocol& aActor) MOZ_OVERRIDE;

private:
  virtual ~Cache();

private:
  nsCOMPtr<nsISupports> mOwner;
  CacheChild* mActor;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Cache)
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Cache_h
