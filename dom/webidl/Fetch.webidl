/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://fetch.spec.whatwg.org/
 */

typedef object JSON;
// FIXME: Bug 1025183 ScalarValueString.
typedef (ArrayBuffer or ArrayBufferView or Blob or DOMString) FetchBodyInit;
// FIXME(nsm): JSON support
typedef FetchBodyInit FetchBody;

[NoInterfaceObject,
 Exposed=(Window,Worker)]
interface FetchBodyStream {
  Promise<ArrayBuffer> asArrayBuffer();
  Promise<Blob> asBlob();
  //Promise<FormData> asFormData();
  Promise<JSON> asJSON();
  //Promise<ScalarValueString>
  Promise<DOMString> asText();
};

[NoInterfaceObject,
 Exposed=(Window,Worker),
 Func="mozilla::dom::Headers::PrefEnabled"]
interface GlobalFetch {
  // Promise<Response>
  Promise<Response> fetch(RequestInfo input, optional RequestInit init);
};
