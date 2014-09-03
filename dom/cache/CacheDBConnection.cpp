/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CacheDBConnection.h"

#include "mozilla/dom/CacheDBListener.h"
#include "mozilla/dom/CacheTypes.h"
#include "mozilla/dom/Headers.h"
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
  , mConnection(aConnection)
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
      "CREATE TABLE map ("
        "id INTEGER NOT NULL PRIMARY KEY, "
        "request_method TEXT NOT NULL, "
        "request_url TEXT NOT NULL, "
        "request_url_no_query TEXT NOT NULL, "
        "request_mode INTEGER NOT NULL, "
        "request_credentials INTEGER NOT NULL, "
        //"request_body_file TEXT NOT NULL, "
        "response_type INTEGER NOT NULL, "
        "response_status INTEGER NOT NULL, "
        "response_status_text TEXT NOT NULL "
        //"response_body_file TEXT NOT NULL "
      ");"
    ));
    NS_ENSURE_SUCCESS(rv, nullptr);

    rv = conn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE request_headers ("
        "name TEXT NOT NULL, "
        "value TEXT NOT NULL, "
        "map_id INTEGER NOT NULL REFERENCES map(id) "
      ");"
    ));
    NS_ENSURE_SUCCESS(rv, nullptr);

    rv = conn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE response_headers ("
        "name TEXT NOT NULL, "
        "value TEXT NOT NULL, "
        "map_id INTEGER NOT NULL REFERENCES map(id) "
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
                              nsTArray<int32_t>& aEntryIdListOut)
{
  nsTArray<PCacheRequest> requestArray;
  nsTArray<PCacheResponse> responseArray;

  // TODO: throw if new Request() would have failed:
  // TODO:    - throw if aRequest is no CORS and method is not simple method

  if (!aParams.ignoreMethod() && !aRequest.method().LowerCaseEqualsLiteral("get")
                              && !aRequest.method().LowerCaseEqualsLiteral("head"))
  {
    return NS_OK;
  }

  nsAutoCString query(
    "SELECT id, COUNT(response_headers.name) AS vary_count "
    "FROM map "
    "LEFT OUTER JOIN response_headers ON map.id=response_headers.map_id "
                                    "AND response_headers.name='vary' "
    "WHERE map."
  );

  nsAutoString urlToMatch;
  if (aParams.ignoreSearch()) {
    urlToMatch = aRequest.urlWithoutQuery();
    query.Append(NS_LITERAL_CSTRING("request_url_no_query"));
  } else {
    urlToMatch = aRequest.url();
    query.Append(NS_LITERAL_CSTRING("request_url"));
  }

  nsAutoCString urlComparison;
  if (aParams.prefixMatch()) {
    urlToMatch.AppendLiteral("%");
    query.Append(NS_LITERAL_CSTRING(" LIKE ?0"));
  } else {
    query.Append(NS_LITERAL_CSTRING("=?0"));
  }

  query.Append(NS_LITERAL_CSTRING("GROUP BY map.id"));

  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = mConnection->CreateStatement(query, getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindStringParameter(0, urlToMatch);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMoreData;
  while(NS_SUCCEEDED(statement->ExecuteStep(&hasMoreData)) && hasMoreData) {
    int32_t entryId;
    rv = statement->GetInt32(0, &entryId);
    if (NS_FAILED(rv)) {
      continue;
    }
    int32_t varyCount;
    rv = statement->GetInt32(0, &varyCount);
    if (NS_FAILED(rv)) {
      continue;
    }

    if (!aParams.ignoreVary() && varyCount > 0 &&
        !MatchByVaryHeader(aRequest, entryId)) {
      continue;
    }

    aEntryIdListOut.AppendElement(entryId);
  }

  return NS_OK;
}

bool
CacheDBConnection::MatchByVaryHeader(const PCacheRequest& aRequest,
                                     int32_t entryId)
{
  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT value FROM response_headers "
    "WHERE name='vary' AND map_id=?0"
  ), getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, false);

  rv = statement->BindInt32Parameter(0, entryId);
  NS_ENSURE_SUCCESS(rv, false);

  // TODO: do this async
  // TODO: use a transaction

  nsTArray<nsCString> varyValues;

  bool hasMoreData;
  while(NS_SUCCEEDED(statement->ExecuteStep(&hasMoreData)) && hasMoreData) {
    nsCString* value = varyValues.AppendElement();
    NS_ENSURE_TRUE(value, false);
    rv = statement->GetUTF8String(0, *value);
    NS_ENSURE_SUCCESS(rv, false);
  }
  NS_ENSURE_SUCCESS(rv, false);

  // Should not have called this function if this was not the case
  MOZ_ASSERT(varyValues.Length() > 0);

  statement->Reset();
  rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT name, value FROM request_headers "
    "WHERE AND map_id=?0"
  ), getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, false);

  rv = statement->BindInt32Parameter(0, entryId);
  NS_ENSURE_SUCCESS(rv, false);

  nsRefPtr<Headers> cachedHeaders = new Headers(nullptr);
  NS_ENSURE_TRUE(cachedHeaders, false);

  ErrorResult errorResult;

  while(NS_SUCCEEDED(statement->ExecuteStep(&hasMoreData)) && hasMoreData) {
    nsAutoCString name;
    nsAutoCString value;
    rv = statement->GetUTF8String(0, name);
    NS_ENSURE_SUCCESS(rv, false);
    rv = statement->GetUTF8String(1, value);
    NS_ENSURE_SUCCESS(rv, false);

    cachedHeaders->Append(name, value, errorResult);
    ENSURE_SUCCESS(errorResult, false);
  }
  NS_ENSURE_SUCCESS(rv, false);

  nsRefPtr<Headers> queryHeaders = new Headers(nullptr, aRequest.headers());
  NS_ENSURE_TRUE(queryHeaders, false);

  for (uint32_t i = 0; i < varyValues.Length(); ++i) {
    if (varyValues[i].EqualsLiteral("*")) {
      continue;
    }

    nsAutoCString queryValue;
    queryHeaders->Get(varyValues[i], queryValue, errorResult);
    ENSURE_SUCCESS(errorResult, false);

    nsAutoCString cachedValue;
    cachedHeaders->Get(varyValues[i], cachedValue, errorResult);
    ENSURE_SUCCESS(errorResult, false);

    if (queryValue != cachedValue) {
      return false;
    }
  }

  return true;
}

} // namespace dom
} // namespace mozilla
