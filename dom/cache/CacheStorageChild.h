/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheStorageChild_h
#define mozilla_dom_cache_CacheStorageChild_h

#include "mozilla/dom/PCacheStorageChild.h"

namespace mozilla {
namespace dom {

class CacheStorageChildListener;
class PCacheChild;

class CacheStorageChild MOZ_FINAL : public PCacheStorageChild
{
public:
  CacheStorageChild(CacheStorageChildListener& aListener);
  virtual ~CacheStorageChild();
  virtual void ActorDestroy(ActorDestroyReason aReason) MOZ_OVERRIDE;
  virtual bool RecvCreateResponse(const uint32_t& aRequestId,
                                  PCacheChild* aActor) MOZ_OVERRIDE;
  void ClearListener();
private:
  CacheStorageChildListener* mListener;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheStorageChild_h
