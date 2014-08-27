/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://fetch.spec.whatwg.org/
 */

typedef (Request or ScalarValueString) RequestInfo;

[Constructor(RequestInfo input, optional RequestInit init),
 Exposed=(Window,Worker),
 Func="mozilla::dom::Request::PrefEnabled"]
interface Request {
  readonly attribute ByteString method;
  readonly attribute ScalarValueString url;
  readonly attribute Headers headers;

  readonly attribute FetchBodyStream body;

  readonly attribute DOMString referrer;
  readonly attribute RequestMode mode;
  readonly attribute RequestCredentials credentials;
};

dictionary RequestInit {
  ByteString method;
  HeadersInit headers;
  FetchBodyInit body;
  RequestMode mode;
  RequestCredentials credentials;
};

enum RequestMode { "same-origin", "no-cors", "cors" };
enum RequestCredentials { "omit", "same-origin", "include" };
