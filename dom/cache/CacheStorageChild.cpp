/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CacheStorageChild.h"

#include "mozilla/dom/CacheStorageChildListener.h"

using mozilla::dom::CacheStorageChild;

CacheStorageChild::CacheStorageChild(CacheStorageChildListener& aListener)
  : mListener(&aListener)
{
}

CacheStorageChild::~CacheStorageChild()
{
  MOZ_ASSERT(!mListener);
}

void
CacheStorageChild::ActorDestroy(ActorDestroyReason aReason)
{
  // If the listener is destroyed before we are, then they will clear
  // their registration.
  if (mListener) {
    mListener->ActorDestroy(*this);
  }
}

bool
CacheStorageChild::RecvCreateResponse(const uint64_t& aRequestId,
                                      PCacheChild* aActor)
{
  MOZ_ASSERT(mListener);
  mListener->RecvCreateResponse(aRequestId, aActor);
  return true;
}

void
CacheStorageChild::ClearListener()
{
  MOZ_ASSERT(mListener);
  mListener = nullptr;
}
