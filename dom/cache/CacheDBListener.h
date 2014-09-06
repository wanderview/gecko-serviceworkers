/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_cache_CacheDBListener_h
#define mozilla_dom_cache_CacheDBListener_h

#include "mozilla/dom/CacheTypes.h"

template<class T> class nsTArray;

namespace mozilla {
namespace dom {

class PCacheResponse;
class PCacheResponseOrVoid;

class CacheDBListener
{
  public:
    virtual ~CacheDBListener() { }

    virtual void OnError(cache::RequestId aRequestId, nsresult aRv)=0;

    virtual void OnMatch(cache::RequestId aRequestId,
                         PCacheResponseOrVoid& aResponse)=0;
    virtual void OnMatchAll(cache::RequestId aRequestId,
                            const nsTArray<PCacheResponse>& aResponses)=0;
    virtual void OnPut(cache::RequestId aRequestId,
                       const PCacheResponseOrVoid& aResponse)=0;
    virtual void OnDelete(cache::RequestId aRequestId, bool aResult)=0;

    // TODO: OnConnected
    // TODO: OnError (or pass nsresult in each On*() method?
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_cache_CacheDBListener_h
