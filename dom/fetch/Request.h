/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Request_h
#define mozilla_dom_Request_h

#include "mozilla/dom/RequestBinding.h"
#include "mozilla/dom/UnionTypes.h"
#include "ipc/IPCMessageUtils.h"

#include "nsWrapperCache.h"
#include "nsISupportsImpl.h"

#include "InternalRequest.h"

class nsPIDOMWindow;

namespace IPC {
  template<>
  struct ParamTraits<mozilla::dom::RequestMode> :
    public ContiguousTypedEnumSerializer<mozilla::dom::RequestMode,
                                         mozilla::dom::RequestMode::Same_origin,
                                         mozilla::dom::RequestMode::EndGuard_> {};
  template<>
  struct ParamTraits<mozilla::dom::RequestCredentials> :
    public ContiguousTypedEnumSerializer<mozilla::dom::RequestCredentials,
                                         mozilla::dom::RequestCredentials::Omit,
                                         mozilla::dom::RequestCredentials::EndGuard_> {};
}

namespace mozilla {
namespace dom {

class FetchBodyStream;
class Headers;

class Request MOZ_FINAL : public nsISupports
                        , public nsWrapperCache
{
NS_DECL_CYCLE_COLLECTING_ISUPPORTS
NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Request)

public:
  static bool PrefEnabled(JSContext* cx, JSObject* obj);

  Request(nsISupports* aOwner, InternalRequest* aRequest);

  void
  GetUrl(nsAString& aUrl) const
  {
    aUrl = NS_ConvertUTF8toUTF16(mRequest->mURL);
  }

  void
  GetMethod(nsCString& aMethod) const
  {
    aMethod = mRequest->mMethod;
  }

  RequestMode
  Mode() const
  {
    return mRequest->mMode;
  }

  RequestCredentials
  Credentials() const
  {
    return mRequest->mCredentialsMode;
  }

  void
  GetReferrer(DOMString& aReferrer) const
  {
    // FIXME(nsm): deal with client referrers.
    aReferrer.AsAString() = NS_ConvertUTF8toUTF16(mRequest->mReferrerURL);
  }

  void GetHeader(const nsAString& header, DOMString& value) const;
  already_AddRefed<Headers> HeadersValue() const;
  already_AddRefed<FetchBodyStream> Body() const;

  static already_AddRefed<Request>
  Constructor(const GlobalObject& global, const RequestOrScalarValueString& aInput,
              const RequestInit& aInit, ErrorResult& rv);

  virtual JSObject*
  WrapObject(JSContext* aCx)
  {
    return mozilla::dom::RequestBinding::Wrap(aCx, this);
  }

  nsISupports* GetParentObject() const
  {
    return mOwner;
  }

  already_AddRefed<InternalRequest>
  GetInternalRequest();
private:
  ~Request();

  nsISupports* mOwner;

  // Headers?
  nsRefPtr<FetchBodyStream> mBody;
  nsRefPtr<InternalRequest> mRequest;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Request_h
