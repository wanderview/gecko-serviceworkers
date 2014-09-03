/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Request.h"
#include "nsDOMString.h"
#include "nsPIDOMWindow.h"
#include "nsIURI.h"
#include "nsISupportsImpl.h"

#include "nsDOMString.h"
#include "nsPIDOMWindow.h"

#include "mozilla/dom/FetchBodyStream.h"
#include "mozilla/dom/Headers.h"
#include "mozilla/dom/URL.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/WorkerPrivate.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(Request)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Request)
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(Request)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Request)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END


// static
bool
Request::PrefEnabled(JSContext* aCx, JSObject* aObj)
{
  using mozilla::dom::workers::WorkerPrivate;
  using mozilla::dom::workers::GetWorkerPrivateFromContext;

  if (NS_IsMainThread()) {
    return Preferences::GetBool("dom.fetch.enabled");
  }

  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  if (!workerPrivate) {
    return false;
  }

  return workerPrivate->DOMFetchEnabled();
}

Request::Request(nsISupports* aOwner, InternalRequest* aRequest)
  : mOwner(aOwner)
  , mRequest(aRequest)
  , mHeaders(new Headers(aOwner))
{
  SetIsDOMBinding();
}

Request::~Request()
{
}

void
Request::GetHeader(const nsAString& header, DOMString& value) const
{
  MOZ_CRASH("NOT IMPLEMENTED!");
}

already_AddRefed<Headers>
Request::HeadersValue() const
{
  nsRefPtr<Headers> ref = mHeaders;
  return ref.forget();
}

already_AddRefed<FetchBodyStream>
Request::Body() const
{
  MOZ_CRASH("NOT IMPLEMENTED!");
}

already_AddRefed<InternalRequest>
Request::GetInternalRequest()
{
  nsRefPtr<InternalRequest> r = mRequest;
  return r.forget();
}

/*static*/ already_AddRefed<Request>
Request::Constructor(const GlobalObject& global, const RequestOrScalarValueString& aInput,
                     const RequestInit& aInit, ErrorResult& aRv)
{
  nsRefPtr<InternalRequest> request;

  if (aInput.IsRequest()) {
    request = aInput.GetAsRequest().GetInternalRequest();
  } else {
    nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(global.GetAsSupports());
    MOZ_ASSERT(window);
    MOZ_ASSERT(window->GetExtantDoc());
    request = new InternalRequest(window->GetExtantDoc());
  }

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(global.GetAsSupports());
  MOZ_ASSERT(window);
  MOZ_ASSERT(window->GetExtantDoc());
  request = request->GetRestrictedCopy(window->GetExtantDoc());

  if (aInput.IsScalarValueString()) {
    nsString input;
    input.Assign(aInput.GetAsScalarValueString());
    // FIXME(nsm): Add worker support.
    workers::AssertIsOnMainThread();
    nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(global.GetAsSupports());
    MOZ_ASSERT(window);

    nsCOMPtr<nsIURI> docURI = window->GetDocumentURI();
    nsCString spec;
    docURI->GetSpec(spec);
    nsRefPtr<URL> url = URL::Constructor(global, input, NS_ConvertUTF8toUTF16(spec), aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    nsString sURL;
    url->Stringify(sURL, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }
    request->SetURL(NS_ConvertUTF16toUTF8(sURL));
  }

  nsCString method = aInit.mMethod.WasPassed() ? aInit.mMethod.Value() : NS_LITERAL_CSTRING("GET");

  if (method.LowerCaseEqualsLiteral("connect") ||
      method.LowerCaseEqualsLiteral("trace") ||
      method.LowerCaseEqualsLiteral("track")) {
    aRv.ThrowTypeError(MSG_INVALID_REQUEST_METHOD, method.get());
    return nullptr;
  }

  request->SetMethod(method);

  nsRefPtr<Request> domRequest =
    new Request(global.GetAsSupports(), request);

  // FIXME(nsm): Plenty of headers and body property setting here.

  // FIXME(nsm): Headers
  // FIXME(nsm): Body setup from FetchBodyStreamInit.
  request->SetMode(aInit.mMode.WasPassed() ? aInit.mMode.Value() : RequestMode::Same_origin);
  request->SetCredentialsMode(aInit.mCredentials.WasPassed() ? aInit.mCredentials.Value() : RequestCredentials::Omit);
  return domRequest.forget();
}

} // namespace dom
} // namespace mozilla
