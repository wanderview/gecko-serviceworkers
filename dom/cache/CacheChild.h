/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheChild_h
#define mozilla_dom_cache_CacheChild_h

#include "mozilla/dom/PCacheChild.h"

namespace mozilla {
namespace dom {

class CacheChildListener;

class CacheChild MOZ_FINAL : public PCacheChild
{
  CacheChildListener* mListener;

public:
  CacheChild();
  virtual ~CacheChild();

  virtual void ActorDestroy(ActorDestroyReason aReason) MOZ_OVERRIDE;

  void SetListener(CacheChildListener& aListener);
  void ClearListener();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheChild_h
