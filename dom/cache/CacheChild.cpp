/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CacheChild.h"

#include "mozilla/dom/CacheChildListener.h"

namespace mozilla {
namespace dom {

CacheChild::CacheChild()
  : mListener(nullptr)
{
}

CacheChild::~CacheChild()
{
  MOZ_ASSERT(!mListener);
}

void
CacheChild::ActorDestroy(ActorDestroyReason aReason)
{
  // If the listener is destroyed before we are, then they will clear
  // their registration.
  if (mListener) {
    mListener->ActorDestroy(*this);
  }
}

void
CacheChild::SetListener(CacheChildListener& aListener)
{
  MOZ_ASSERT(!mListener);
  mListener = &aListener;
}

void
CacheChild::ClearListener()
{
  MOZ_ASSERT(mListener);
  mListener = nullptr;
}

} // namespace dom
} // namesapce mozilla
