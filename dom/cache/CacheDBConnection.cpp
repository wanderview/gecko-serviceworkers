/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CacheDBConnection.h"

#include "mozilla/dom/CacheDBListener.h"
#include "mozilla/dom/CacheTypes.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "nsServiceManagerUtils.h"
#include "mozIStorageConnection.h"
#include "mozIStorageService.h"
#include "mozIStorageStatement.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"
#include "nsIFile.h"
#include "nsIUUIDGenerator.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace dom {

using mozilla::dom::cache::RequestId;
using mozilla::dom::quota::PERSISTENCE_TYPE_PERSISTENT;
using mozilla::dom::quota::PersistenceType;
using mozilla::dom::quota::QuotaManager;

//static
already_AddRefed<CacheDBConnection>
CacheDBConnection::Get(CacheDBListener& aListener, const nsACString& aOrigin,
    const nsACString& aBaseDomain, const nsID& aCacheId)
{
  return GetOrCreateInternal(aListener, aOrigin, aBaseDomain, aCacheId, false);
}

//static
already_AddRefed<CacheDBConnection>
CacheDBConnection::Create(CacheDBListener& aListener, const nsACString& aOrigin,
       const nsACString& aBaseDomain)
{
  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidgen =
    do_GetService("@mozilla.org/uuid-generator;1", &rv);
  if (NS_WARN_IF(NS_FAILED(rv) || !uuidgen)) {
    return nullptr;
  }

  nsID cacheId;
  rv = uuidgen->GenerateUUIDInPlace(&cacheId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  return GetOrCreateInternal(aListener, aOrigin, aBaseDomain, cacheId, true);
}

CacheDBConnection::CacheDBConnection(CacheDBListener& aListener,
                                     const nsID& aCacheId,
                                     already_AddRefed<mozIStorageConnection> aConnection)
  : mListener(aListener)
  , mCacheId(aCacheId)
  , mDBConnection(aConnection)
{
}

CacheDBConnection::~CacheDBConnection()
{
}

nsresult
CacheDBConnection::MatchAll(RequestId aRequestId, const PCacheRequest& aRequest,
                            const PCacheQueryParams& aParams)
{
  nsTArray<PCacheResponse> responses;

  if (aRequest.null()) {
    // TODO: Get all responses
  } else {
    // TODO: Run QueryCache algorithm
  }

  mListener.OnMatchAll(aRequestId, responses);
  return NS_OK;
}

nsresult
CacheDBConnection::Put(RequestId aRequestId, const PCacheRequest& aRequest,
                       const PCacheResponse& aResponse)
{
  PCacheResponse response;
  response.null() = true;

  // TODO: run QueryCache algorithm on request
  // TODO: delete any found responses
  // TODO: insert new request/response pair

  mListener.OnPut(aRequestId, response);
  return NS_OK;
}


//static
already_AddRefed<CacheDBConnection>
CacheDBConnection::GetOrCreateInternal(CacheDBListener& aListener,
                                       const nsACString& aOrigin,
                                       const nsACString& aBaseDomain,
                                       const nsID& aCacheId,
                                       bool allowCreate)
{
  QuotaManager* quota = QuotaManager::Get();
  MOZ_ASSERT(quota);

  const PersistenceType persistenceType = PERSISTENCE_TYPE_PERSISTENT;

  nsCOMPtr<nsIFile> dbDir;
  nsresult rv = quota->GetDirectoryForOrigin(persistenceType,
                                             aOrigin,
                                             getter_AddRefs(dbDir));
  NS_ENSURE_SUCCESS(rv, nullptr);

  rv = dbDir->Append(NS_LITERAL_STRING("cache"));
  NS_ENSURE_SUCCESS(rv, nullptr);

  rv = dbDir->Append(NS_ConvertUTF8toUTF16(aCacheId.ToString()));
  NS_ENSURE_SUCCESS(rv, nullptr);

  bool exists;
  rv = dbDir->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, nullptr);
  NS_ENSURE_TRUE(exists || allowCreate, nullptr);

  if (!exists) {
    rv = dbDir->Create(nsIFile::DIRECTORY_TYPE, 0755);
    NS_ENSURE_SUCCESS(rv, nullptr);
  }

  nsCOMPtr<nsIFile> dbFile;
  rv = dbDir->Clone(getter_AddRefs(dbFile));
  NS_ENSURE_SUCCESS(rv, nullptr);

  rv = dbFile->Append(NS_LITERAL_STRING("db.sqlite"));
  NS_ENSURE_SUCCESS(rv, nullptr);

  rv = dbFile->Exists(&exists);
  NS_ENSURE_SUCCESS(rv, nullptr);
  NS_ENSURE_TRUE(exists || allowCreate, nullptr);

  // XXX: Jonas tells me nsIFileURL usage off-main-thread is dangerous,
  //      but this is what IDB does to access mozIStorageConnection so
  //      it seems at least this corner case mostly works.

  nsCOMPtr<nsIFile> dbTmpDir;
  rv = dbDir->Clone(getter_AddRefs(dbTmpDir));
  NS_ENSURE_SUCCESS(rv, nullptr);

  rv = dbTmpDir->Append(NS_LITERAL_STRING("db"));
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewFileURI(getter_AddRefs(uri), dbFile);
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsCOMPtr<nsIFileURL> dbFileUrl = do_QueryInterface(uri);
  NS_ENSURE_TRUE(dbFileUrl, nullptr);

  nsAutoCString type;
  PersistenceTypeToText(persistenceType, type);

  rv = dbFileUrl->SetQuery(NS_LITERAL_CSTRING("persistenceType=") + type +
                           NS_LITERAL_CSTRING("&group=") + aBaseDomain +
                           NS_LITERAL_CSTRING("&origin=") + aOrigin);
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsCOMPtr<mozIStorageService> ss =
    do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(ss, nullptr);

  nsCOMPtr<mozIStorageConnection> conn;
  rv = ss->OpenDatabaseWithFileURL(dbFileUrl, getter_AddRefs(conn));
  if (rv == NS_ERROR_FILE_CORRUPTED) {
    dbFile->Remove(false);
    NS_ENSURE_SUCCESS(rv, nullptr);

    rv = dbTmpDir->Exists(&exists);
    NS_ENSURE_SUCCESS(rv, nullptr);

    if (exists) {
      bool isDir;
      rv = dbTmpDir->IsDirectory(&isDir);
      NS_ENSURE_SUCCESS(rv, nullptr);
      NS_ENSURE_TRUE(isDir, nullptr);
      rv = dbTmpDir->Remove(true);
      NS_ENSURE_SUCCESS(rv, nullptr);
    }
  }
  NS_ENSURE_SUCCESS(rv, nullptr);
  NS_ENSURE_TRUE(conn, nullptr);

  rv = conn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
    // Switch the journaling mode to TRUNCATE to avoid changing the directory
    // structure at the conclusion of every transaction for devices with slower
    // file systems.
    "PRAGMA journal_mode = TRUNCATE; "
#endif
    "PRAGMA foreign_keys = ON; "
  ));
  NS_ENSURE_SUCCESS(rv, nullptr);

  int32_t schemaVersion;
  rv = conn->GetSchemaVersion(&schemaVersion);
  NS_ENSURE_SUCCESS(rv, nullptr);

  mozStorageTransaction trans(conn, false,
                              mozIStorageConnection::TRANSACTION_IMMEDIATE);

  if (!schemaVersion) {
    rv = conn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE requests ("
        "id INTEGER NOT NULL PRIMARY KEY, "
        "method TEXT NOT NULL, "
        "url TEXT NOT NULL, "
        "mode INTEGER NOT NULL, "
        "credentials INTEGER NOT NULL "
        //"body_file TEXT NOT NULL "
      ");"
    ));
    NS_ENSURE_SUCCESS(rv, nullptr);

    rv = conn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE request_headers ("
        "name TEXT NOT NULL, "
        "value TEXT NOT NULL, "
        "request_id INTEGER NOT NULL REFERENCES requests(id) "
      ");"
    ));
    NS_ENSURE_SUCCESS(rv, nullptr);

    rv = conn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE responses ("
        "id INTEGER NOT NULL PRIMARY KEY, "
        "type INTEGER NOT NULL, "
        "status INTEGER NOT NULL, "
        "statusText TEXT NOT NULL "
        //"body_file TEXT NOT NULL "
      ");"
    ));
    NS_ENSURE_SUCCESS(rv, nullptr);

    rv = conn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE response_headers ("
        "name TEXT NOT NULL, "
        "value TEXT NOT NULL, "
        "response_id INTEGER NOT NULL REFERENCES responses(id) "
      ");"
    ));
    NS_ENSURE_SUCCESS(rv, nullptr);

    conn->SetSchemaVersion(kLatestSchemaVersion);
    NS_ENSURE_SUCCESS(rv, nullptr);

    rv = conn->GetSchemaVersion(&schemaVersion);
    NS_ENSURE_SUCCESS(rv, nullptr);
  }

  NS_ENSURE_TRUE(schemaVersion == kLatestSchemaVersion, nullptr);

  rv = trans.Commit();
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsRefPtr<CacheDBConnection> ref = new CacheDBConnection(aListener,
                                                          aCacheId,
                                                          conn.forget());
  return ref.forget();
}

nsresult
CacheDBConnection::QueryCache(cache::RequestId aRequestId,
                              const PCacheRequest& aRequest,
                              const PCacheQueryParams& aParams,
                              nsTArray<QueryResult>& aResponsesOut)
{
  nsTArray<PCacheRequest> requestArray;
  nsTArray<PCacheResponse> responseArray;

  // TODO: throw if new Request() would have failed:
  // TODO:    - throw if aRequest is no CORS and method is not simple method

  if (!aParams.ignoreMethod() && aRequest.method().LowerCaseEqualsLiteral("get")
                              && aRequest.method().LowerCaseEqualsLiteral("head"))
  {
    return NS_OK;
  }

  // TODO: break out URL into separate components to be stored in database?

  // TODO: implement algorithm below
  // For each request-to-response entry
    // Let cachedURL be new URL(entry.[[key]].url)
    // Let requestURL be new URL(request.url)
    // if params.ignoreSearch
      // set cachedURL.search to ""
      // set requestURL.search to "" -> this should make query part of url not matter
    // if params.prefixMatch
      // set cachedURL.href to substring of itself with length requestURL.href
    // if cachedURL.href matches requestURL.href
      // Add entry.[[key]] to requestArray
      // Add entry.[[value]] to responseArray
  // For each cacheResponse in responseArray with index:
    // let cachedRequest be the indexth element in requestArray
    // if params.ignoreVary or cachedResponse.headers.has("Vary") is false
      // add [cachedRequest, cachedResponse] to requestArray
      // continue to next loop iteration
    // Let varyHeaders be array of values of cachedResponse.getAll("Vary") (parse out comma-separated values)
    // For each f in varyHeaders
      // if f matches "*", then:
        // continue to next loop iteration
      // if cacheRequest.headers.get(f) does not match request.headers.get(f)
        // break out of loop
      // add [cachedReqeust, cachedResponse] to requestArray

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
