/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CacheParent.h"

#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {

using mozilla::dom::cache::RequestId;

CacheParent::CacheParent()
{
}

CacheParent::~CacheParent()
{
}

void
CacheParent::ActorDestroy(ActorDestroyReason aReason)
{
}

bool
CacheParent::RecvMatch(const RequestId& requestId, const PCacheRequest& request,
                       const PCacheQueryParams& params)
{
  return false;
}

bool
CacheParent::RecvMatchAll(const RequestId& requestId,
                          const PCacheRequest& request,
                          const PCacheQueryParams& params)
{
  return false;
}

bool
CacheParent::RecvAdd(const RequestId& requestId, const PCacheRequest& request)
{
  return false;
}

bool
CacheParent::RecvAddAll(const RequestId& requestId,
                        const nsTArray<PCacheRequest>& requests)
{
  return false;
}

bool
CacheParent::RecvPut(const RequestId& requestId, const PCacheRequest& request,
                     const PCacheResponse& response)
{
  return false;
}

bool
CacheParent::RecvDelete(const RequestId& requestId,
                        const PCacheRequest& request,
                        const PCacheQueryParams& params)
{
  return false;
}

bool
CacheParent::RecvKeys(const RequestId& requestId, const PCacheRequest& request,
                      const PCacheQueryParams& params)
{
  return false;
}

} // namespace dom
} // namesapce mozilla
