/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ActorDestroyListener_h
#define mozilla_dom_ActorDestroyListener_h

namespace mozilla {

namespace ipc {
  class IProtocol;
}

namespace dom {

class ActorDestroyListener
{
public:
  virtual ~ActorDestroyListener() { }
  virtual void ActorDestroy(mozilla::ipc::IProtocol& aActor)=0;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ActorDestroyListener_h
