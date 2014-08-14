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
 Func="mozilla::dom::CacheStorage::PrefEnabled"]
interface CacheStorage {
   Promise<Response> match((Request or ScalarValueString) request,
                           optional QueryParams params);
   Promise<Cache> get(DOMString cacheName);
   Promise<boolean> has(DOMString cacheName);
   Promise<Cache> create(DOMString cacheName);
   Promise<boolean> delete(DOMString cacheName);
   Promise<sequence<DOMString>> keys();
};
