/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * http://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html
 *
 */

[Exposed=(Window,Worker),
 Func="mozilla::dom::Cache::PrefEnabled"]
interface Cache {
  Promise<Response> match((Request or ScalarValueString) request,
                          optional QueryParams params);
  Promise<sequence<Response>> matchAll((Request or ScalarValueString) request,
                                       optional QueryParams params);
  Promise<Response> add((Request or ScalarValueString) request);
  Promise<sequence<Response>> addAll(sequence<(Request or ScalarValueString)> requests);
  Promise<Response> put((Request or ScalarValueString) request,
                        Response response);
  Promise<boolean> delete((Request or ScalarValueString) request,
                          optional QueryParams params);
  Promise<sequence<Request>> keys(optional (Request or ScalarValueString) request,
                                  optional QueryParams params);
};

dictionary QueryParams {
  boolean ignoreSearch;
  boolean ignoreMethod;
  boolean ignoreVary;
  boolean prefixMatch;
  DOMString cacheName;
};

dictionary CacheBatchOperation {
  DOMString type;
  Request request;
  Response response;
  QueryParams matchParams;
};
