/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CacheDBConnection.h"

#include "mozilla/dom/CacheDBListener.h"
#include "mozilla/dom/CacheQuotaRunnable.h"
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

static const int32_t MAX_ENTRIES_PER_STATEMENT = 255;

static void
AppendListParamsToQuery(nsACString& aQuery,
                        const nsTArray<CacheDBConnection::EntryId>& aEntryIdList,
                        uint32_t aPos, int32_t aLen)
{
  MOZ_ASSERT((aPos + aLen) <= aEntryIdList.Length());
  for (int32_t i = aPos; i < aLen; ++i) {
    if (i == 0) {
      aQuery.Append(NS_LITERAL_CSTRING("?"));
    } else {
      aQuery.Append(NS_LITERAL_CSTRING(",?"));
    }
  }
}

static nsresult
BindListParamsToQuery(mozIStorageStatement* aStatement,
                      const nsTArray<CacheDBConnection::EntryId>& aEntryIdList,
                      uint32_t aPos, int32_t aLen)
{
  MOZ_ASSERT((aPos + aLen) <= aEntryIdList.Length());
  for (int32_t i = aPos; i < aLen; ++i) {
    nsresult rv = aStatement->BindInt32Parameter(i, aEntryIdList[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}

class CacheDBConnection::OpenRunnable : public CacheQuotaRunnable
{
public:
  OpenRunnable(const nsACString& aOrigin, const nsACString& aBaseDomain,
               const nsACString& aQuotaId, bool aAllowCreate,
               const nsID& aCacheId, RequestId aRequestId,
               CacheDBConnection* aCacheDBConnection)
    : CacheQuotaRunnable(aOrigin, aBaseDomain, aQuotaId)
    , mAllowCreate(aAllowCreate)
    , mCacheId(aCacheId)
    , mRequestId(aRequestId)
    , mCacheDBConnection(aCacheDBConnection)
    , mResult(NS_OK)
  {
    MOZ_ASSERT(mCacheDBConnection);
  }

protected:
  virtual void AfterOpenOnQuotaIOThread(mozIStorageConnection* aConnection)=0;

  virtual ~OpenRunnable() { }
  virtual void RunOnQuotaIOThread(const nsACString& aOrigin,
                                  const nsACString& aBaseDomain,
                                  nsIFile* aQuotaDir) MOZ_OVERRIDE
  {
    MOZ_ASSERT(aQuotaDir);

    mResult = aQuotaDir->Append(NS_LITERAL_STRING("cache"));
    if (NS_FAILED(mResult)) { return; }

    char cacheIdBuf[NSID_LENGTH];
    mCacheId.ToProvidedString(cacheIdBuf);
    mResult = aQuotaDir->Append(NS_ConvertUTF8toUTF16(cacheIdBuf));
    if (NS_FAILED(mResult)) { return; }

    bool exists;
    mResult = aQuotaDir->Exists(&exists);
    if (NS_FAILED(mResult) || (!exists && !mAllowCreate)) { return; }

    if (!exists) {
      mResult = aQuotaDir->Create(nsIFile::DIRECTORY_TYPE, 0755);
      if (NS_FAILED(mResult)) { return; }
    }

    nsCOMPtr<nsIFile> dbFile;
    mResult = aQuotaDir->Clone(getter_AddRefs(dbFile));
    if (NS_FAILED(mResult)) { return; }

    mResult = dbFile->Append(NS_LITERAL_STRING("db.sqlite"));
    if (NS_FAILED(mResult)) { return; }

    mResult = dbFile->Exists(&exists);
    if (NS_FAILED(mResult) || (!exists && !mAllowCreate)) { return; }

    // XXX: Jonas tells me nsIFileURL usage off-main-thread is dangerous,
    //      but this is what IDB does to access mozIStorageConnection so
    //      it seems at least this corner case mostly works.

    nsCOMPtr<nsIFile> dbTmpDir;
    mResult = aQuotaDir->Clone(getter_AddRefs(dbTmpDir));
    if (NS_FAILED(mResult)) { return; }

    mResult = dbTmpDir->Append(NS_LITERAL_STRING("db"));
    if (NS_FAILED(mResult)) { return; }

    nsCOMPtr<nsIURI> uri;
    mResult = NS_NewFileURI(getter_AddRefs(uri), dbFile);
    if (NS_FAILED(mResult)) { return; }

    nsCOMPtr<nsIFileURL> dbFileUrl = do_QueryInterface(uri);
    if (NS_WARN_IF(!dbFileUrl)) {
      mResult = NS_ERROR_FAILURE;
      return;
    }

    nsAutoCString type;
    PersistenceTypeToText(PERSISTENCE_TYPE_PERSISTENT, type);

    mResult = dbFileUrl->SetQuery(NS_LITERAL_CSTRING("persistenceType=") + type +
                             NS_LITERAL_CSTRING("&group=") + aBaseDomain +
                             NS_LITERAL_CSTRING("&origin=") + aOrigin);
    if (NS_FAILED(mResult)) { return; }

    nsCOMPtr<mozIStorageService> ss =
      do_GetService(MOZ_STORAGE_SERVICE_CONTRACTID);
    if (NS_WARN_IF(!ss)) {
      mResult = NS_ERROR_FAILURE;
      return;
    }

    nsCOMPtr<mozIStorageConnection> conn;
    mResult = ss->OpenDatabaseWithFileURL(dbFileUrl, getter_AddRefs(conn));
    if (mResult == NS_ERROR_FILE_CORRUPTED) {
      dbFile->Remove(false);
      if (NS_FAILED(mResult)) { return; }

      mResult = dbTmpDir->Exists(&exists);
      if (NS_FAILED(mResult)) { return; }

      if (exists) {
        bool isDir;
        mResult = dbTmpDir->IsDirectory(&isDir);
        if (NS_FAILED(mResult) || !isDir) { return; }
        mResult = dbTmpDir->Remove(true);
        if (NS_FAILED(mResult)) { return; }
      }

      mResult = ss->OpenDatabaseWithFileURL(dbFileUrl, getter_AddRefs(conn));
    }
    if (NS_FAILED(mResult)) { return; }
    MOZ_ASSERT(conn);

    mResult = conn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
  #if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
      // Switch the journaling mode to TRUNCATE to avoid changing the directory
      // structure at the conclusion of every transaction for devices with slower
      // file systems.
      "PRAGMA journal_mode = TRUNCATE; "
  #endif
      "PRAGMA foreign_keys = ON; "
    ));
    if (NS_FAILED(mResult)) { return; }

    int32_t schemaVersion;
    mResult = conn->GetSchemaVersion(&schemaVersion);
    if (NS_FAILED(mResult)) { return; }

    mozStorageTransaction trans(conn, false,
                                mozIStorageConnection::TRANSACTION_IMMEDIATE);

    if (!schemaVersion) {
      mResult = conn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
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
      if (NS_FAILED(mResult)) { return; }

      mResult = conn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE TABLE request_headers ("
          "name TEXT NOT NULL, "
          "value TEXT NOT NULL, "
          "map_id INTEGER NOT NULL REFERENCES map(id) "
        ");"
      ));
      if (NS_FAILED(mResult)) { return; }

      mResult = conn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE TABLE response_headers ("
          "name TEXT NOT NULL, "
          "value TEXT NOT NULL, "
          "map_id INTEGER NOT NULL REFERENCES map(id) "
        ");"
      ));
      if (NS_FAILED(mResult)) { return; }

      conn->SetSchemaVersion(kLatestSchemaVersion);
      if (NS_FAILED(mResult)) { return; }

      mResult = conn->GetSchemaVersion(&schemaVersion);
      if (NS_FAILED(mResult)) { return; }
    }

    if (schemaVersion != kLatestSchemaVersion) {
      mResult = NS_ERROR_FAILURE;
      return;
    }

    mResult = trans.Commit();
    if (NS_FAILED(mResult)) { return; }

    AfterOpenOnQuotaIOThread(conn);
  }

  const bool mAllowCreate;
  const nsID mCacheId;
  const RequestId mRequestId;
  nsRefPtr<CacheDBConnection> mCacheDBConnection;
  nsresult mResult;
};

class CacheDBConnection::MatchRunnable MOZ_FINAL :
  public CacheDBConnection::OpenRunnable
{
public:
  MatchRunnable(const nsACString& aOrigin, const nsACString& aBaseDomain,
               const nsACString& aQuotaId, const nsID& aCacheId,
               RequestId aRequestId, const PCacheRequest& aRequest,
               const PCacheQueryParams& aParams,
               CacheDBConnection* aCacheDBConnection)
    : OpenRunnable(aOrigin, aBaseDomain, aQuotaId, false, aCacheId,
                   aRequestId, aCacheDBConnection)
    , mRequest(aRequest)
    , mParams(aParams)
    , mResponseOrVoid(void_t())
  { }

protected:
  virtual ~MatchRunnable() { }

  virtual void
  AfterOpenOnQuotaIOThread(mozIStorageConnection* aConnection) MOZ_OVERRIDE
  {
    MOZ_ASSERT(aConnection);

    nsTArray<EntryId> matches;
    mResult = mCacheDBConnection->QueryCache(aConnection, mRequest, mParams,
                                             matches);
    if (NS_FAILED(mResult)) { return; }

    if (matches.Length() < 1) {
      mResponseOrVoid = void_t();
      return;
    }

    PCacheResponse response;
    mResult = mCacheDBConnection->ReadResponse(aConnection, matches[0],
                                               response);
    if (NS_FAILED(mResult)) { return; }

    mResponseOrVoid = response;
  }

  virtual void
  CompleteOnInitiatingThread(nsresult aRv) MOZ_OVERRIDE
  {
    nsresult rv = NS_FAILED(aRv) ? aRv : mResult;
    mCacheDBConnection->OnMatchComplete(mRequestId, rv, mResponseOrVoid);
  }

  const PCacheRequest mRequest;
  const PCacheQueryParams mParams;
  PCacheResponseOrVoid mResponseOrVoid;
};

class CacheDBConnection::MatchAllRunnable MOZ_FINAL :
  public CacheDBConnection::OpenRunnable
{
public:
  MatchAllRunnable(const nsACString& aOrigin, const nsACString& aBaseDomain,
                   const nsACString& aQuotaId, const nsID& aCacheId,
                   RequestId aRequestId, const PCacheRequestOrVoid& aRequestOrVoid,
                   const PCacheQueryParams& aParams,
                   CacheDBConnection* aCacheDBConnection)
    : OpenRunnable(aOrigin, aBaseDomain, aQuotaId, false, aCacheId,
                   aRequestId, aCacheDBConnection)
    , mRequestOrVoid(aRequestOrVoid)
    , mParams(aParams)
  { }

protected:
  virtual ~MatchAllRunnable() { }

  virtual void
  AfterOpenOnQuotaIOThread(mozIStorageConnection* aConnection) MOZ_OVERRIDE
  {
    MOZ_ASSERT(aConnection);

    nsTArray<EntryId> matches;
    if (mRequestOrVoid.type() == PCacheRequestOrVoid::Tvoid_t) {
      mResult = mCacheDBConnection->QueryAll(aConnection, matches);
      if (NS_FAILED(mResult)) { return; }
    } else {
      mResult = mCacheDBConnection->QueryCache(aConnection, mRequestOrVoid,
                                               mParams, matches);
      if (NS_FAILED(mResult)) { return; }
    }

    // TODO: replace this with a bulk load using SQL IN clause
    for (uint32_t i = 0; i < matches.Length(); ++i) {
      PCacheResponse *response = mResponses.AppendElement();
      if (!response) {
        mResult = NS_ERROR_OUT_OF_MEMORY;
        return;
      }

      mResult = mCacheDBConnection->ReadResponse(aConnection, matches[i],
                                                 *response);
      if (NS_FAILED(mResult)) { return; }
    }
  }

  virtual void
  CompleteOnInitiatingThread(nsresult aRv) MOZ_OVERRIDE
  {
    nsresult rv = NS_FAILED(aRv) ? aRv : mResult;
    mCacheDBConnection->OnMatchAllComplete(mRequestId, rv, mResponses);
  }

  const PCacheRequestOrVoid mRequestOrVoid;
  const PCacheQueryParams mParams;
  nsTArray<PCacheResponse> mResponses;
};

class CacheDBConnection::PutRunnable MOZ_FINAL :
  public CacheDBConnection::OpenRunnable
{
public:
  PutRunnable(const nsACString& aOrigin, const nsACString& aBaseDomain,
              const nsACString& aQuotaId, const nsID& aCacheId,
              RequestId aRequestId, const PCacheRequest& aRequest,
              const PCacheResponse& aResponse,
              CacheDBConnection* aCacheDBConnection)
    : OpenRunnable(aOrigin, aBaseDomain, aQuotaId, true, aCacheId,
                   aRequestId, aCacheDBConnection)
    , mRequest(aRequest)
    , mResponse(aResponse)
  { }

protected:
  virtual ~PutRunnable() { }

  virtual void
  AfterOpenOnQuotaIOThread(mozIStorageConnection* aConnection) MOZ_OVERRIDE
  {
    MOZ_ASSERT(aConnection);

    PCacheQueryParams params(false, false, false, false, false,
                             NS_LITERAL_STRING(""));

    mozStorageTransaction trans(aConnection, false,
                                mozIStorageConnection::TRANSACTION_IMMEDIATE);

    nsTArray<EntryId> matches;
    mResult = mCacheDBConnection->QueryCache(aConnection, mRequest, params,
                                             matches);
    if (NS_FAILED(mResult)) { return; }

    mResult = mCacheDBConnection->DeleteEntries(aConnection, matches);
    if (NS_FAILED(mResult)) { return; }

    mResult = mCacheDBConnection->InsertEntry(aConnection, mRequest, mResponse);
    if (NS_FAILED(mResult)) { return; }

    mResult = trans.Commit();
    if (NS_FAILED(mResult)) { return; }
  }

  virtual void
  CompleteOnInitiatingThread(nsresult aRv) MOZ_OVERRIDE
  {
    nsresult rv = NS_FAILED(aRv) ? aRv : mResult;
    PCacheResponseOrVoid responseOrVoid;
    if (NS_FAILED(rv)) {
      responseOrVoid = void_t();
    } else {
      responseOrVoid = mResponse;
    }
    mCacheDBConnection->OnPutComplete(mRequestId, rv, responseOrVoid);
  }

  const PCacheRequest mRequest;
  const PCacheResponse mResponse;
};

class CacheDBConnection::DeleteRunnable MOZ_FINAL :
  public CacheDBConnection::OpenRunnable
{
public:
  DeleteRunnable(const nsACString& aOrigin, const nsACString& aBaseDomain,
                 const nsACString& aQuotaId, const nsID& aCacheId,
                 RequestId aRequestId, const PCacheRequest& aRequest,
                 const PCacheQueryParams& aParams,
                 CacheDBConnection* aCacheDBConnection)
    : OpenRunnable(aOrigin, aBaseDomain, aQuotaId, false, aCacheId,
                   aRequestId, aCacheDBConnection)
    , mRequest(aRequest)
    , mParams(aParams)
    , mSuccess(false)
  { }

protected:
  virtual ~DeleteRunnable() { }

  virtual void
  AfterOpenOnQuotaIOThread(mozIStorageConnection* aConnection) MOZ_OVERRIDE
  {
    MOZ_ASSERT(aConnection);

    mozStorageTransaction trans(aConnection, false,
                                mozIStorageConnection::TRANSACTION_IMMEDIATE);

    nsTArray<EntryId> matches;
    mResult = mCacheDBConnection->QueryCache(aConnection, mRequest, mParams,
                                             matches);
    if (NS_FAILED(mResult)) { return; }

    if (matches.Length() > 0) {
      mResult = mCacheDBConnection->DeleteEntries(aConnection, matches);
      if (NS_FAILED(mResult)) { return; }

      mResult = trans.Commit();
      if (NS_FAILED(mResult)) { return; }

      mSuccess = true;
    }
  }

  virtual void
  CompleteOnInitiatingThread(nsresult aRv) MOZ_OVERRIDE
  {
    nsresult rv = NS_FAILED(aRv) ? aRv : mResult;
    mCacheDBConnection->OnDeleteComplete(mRequestId, rv, mSuccess);
  }

  const PCacheRequest mRequest;
  const PCacheQueryParams mParams;
  bool mSuccess;
};

//static
already_AddRefed<CacheDBConnection>
CacheDBConnection::Create(CacheDBListener* aListener, const nsACString& aOrigin,
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

  nsRefPtr<CacheDBConnection> ref = new CacheDBConnection(aListener, aOrigin,
                                                          aBaseDomain, cacheId);
  return ref.forget();
}

CacheDBConnection::CacheDBConnection(CacheDBListener* aListener,
                                     const nsACString& aOrigin,
                                     const nsACString& aBaseDomain,
                                     const nsID& aCacheId)
  : mListener(aListener)
  , mOrigin(aOrigin)
  , mBaseDomain(aBaseDomain)
  , mCacheId(aCacheId)
  , mQuotaId("Cache:")
{
  MOZ_ASSERT(mListener);

  char cacheIdBuf[NSID_LENGTH];
  mCacheId.ToProvidedString(cacheIdBuf);
  mQuotaId.Append(cacheIdBuf);
}

void
CacheDBConnection::ClearListener()
{
  MOZ_ASSERT(mListener);
  mListener = nullptr;
}

void
CacheDBConnection::Match(RequestId aRequestId, const PCacheRequest& aRequest,
                         const PCacheQueryParams& aParams)
{
  nsRefPtr<MatchRunnable> match = new MatchRunnable(mOrigin, mBaseDomain, mQuotaId,
                                                    mCacheId, aRequestId, aRequest,
                                                    aParams, this);
  if (!match) {
    OnMatchComplete(aRequestId, NS_ERROR_OUT_OF_MEMORY,
                    PCacheResponseOrVoid(void_t()));
    return;
  }
  match->Dispatch();
}

void
CacheDBConnection::MatchAll(RequestId aRequestId,
                            const PCacheRequestOrVoid& aRequest,
                            const PCacheQueryParams& aParams)
{
  nsRefPtr<MatchAllRunnable> match = new MatchAllRunnable(mOrigin, mBaseDomain,
                                                          mQuotaId, mCacheId,
                                                          aRequestId, aRequest,
                                                          aParams, this);
  if (!match) {
    OnMatchAllComplete(aRequestId, NS_ERROR_OUT_OF_MEMORY,
                       nsTArray<PCacheResponse>());
    return;
  }
  match->Dispatch();
}

// TODO: Make sure AddAll() doesn't delete entries it just created.

void
CacheDBConnection::Put(RequestId aRequestId, const PCacheRequest& aRequest,
                       const PCacheResponse& aResponse)
{
  nsRefPtr<PutRunnable> put = new PutRunnable(mOrigin, mBaseDomain, mQuotaId,
                                              mCacheId, aRequestId, aRequest,
                                              aResponse, this);
  if (!put) {
    OnPutComplete(aRequestId, NS_ERROR_OUT_OF_MEMORY,
                  PCacheResponseOrVoid(void_t()));
    return;
  }
  put->Dispatch();
}

void
CacheDBConnection::Delete(cache::RequestId aRequestId,
                          const PCacheRequest& aRequest,
                          const PCacheQueryParams& aParams)
{
  nsRefPtr<DeleteRunnable> del = new DeleteRunnable(mOrigin, mBaseDomain,
                                                    mQuotaId, mCacheId,
                                                    aRequestId, aRequest,
                                                    aParams, this);
  if (!del) {
    OnDeleteComplete(aRequestId, NS_ERROR_OUT_OF_MEMORY, false);
    return;
  }
  del->Dispatch();
}

CacheDBConnection::~CacheDBConnection()
{
  MOZ_ASSERT(!mListener);
}

nsresult
CacheDBConnection::QueryAll(mozIStorageConnection* aConnection,
                            nsTArray<EntryId>& aEntryIdListOut)
{
  MOZ_ASSERT(aConnection);

  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id FROM map"
  ), getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMoreData;
  while(NS_SUCCEEDED(statement->ExecuteStep(&hasMoreData)) && hasMoreData) {
    EntryId* entryId = aEntryIdListOut.AppendElement();
    NS_ENSURE_TRUE(entryId, NS_ERROR_OUT_OF_MEMORY);
    rv = statement->GetInt32(0, entryId);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
CacheDBConnection::QueryCache(mozIStorageConnection* aConnection,
                              const PCacheRequest& aRequest,
                              const PCacheQueryParams& aParams,
                              nsTArray<EntryId>& aEntryIdListOut)
{
  MOZ_ASSERT(aConnection);

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
    query.Append(NS_LITERAL_CSTRING(" LIKE ?1 ESCAPE '\\'"));
  } else {
    query.Append(NS_LITERAL_CSTRING("=?1"));
  }

  query.Append(NS_LITERAL_CSTRING(" GROUP BY map.id ORDER BY map.id"));

  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = aConnection->CreateStatement(query, getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  if (aParams.prefixMatch()) {
    nsAutoString escapedUrlToMatch;
    rv = statement->EscapeStringForLIKE(urlToMatch, '\\', escapedUrlToMatch);
    NS_ENSURE_SUCCESS(rv, rv);
    urlToMatch = escapedUrlToMatch;
  }

  rv = statement->BindStringParameter(0, urlToMatch);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMoreData;
  while(NS_SUCCEEDED(statement->ExecuteStep(&hasMoreData)) && hasMoreData) {
    EntryId entryId;
    rv = statement->GetInt32(0, &entryId);
    NS_ENSURE_SUCCESS(rv, rv);

    int32_t varyCount;
    rv = statement->GetInt32(1, &varyCount);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!aParams.ignoreVary() && varyCount > 0 &&
        !MatchByVaryHeader(aConnection, aRequest, entryId)) {
      continue;
    }

    aEntryIdListOut.AppendElement(entryId);
  }

  return NS_OK;
}

bool
CacheDBConnection::MatchByVaryHeader(mozIStorageConnection* aConnection,
                                     const PCacheRequest& aRequest,
                                     EntryId entryId)
{
  MOZ_ASSERT(aConnection);

  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT value FROM response_headers "
    "WHERE name='vary' AND map_id=?1"
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
  rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT name, value FROM request_headers "
    "WHERE AND map_id=?1"
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

nsresult
CacheDBConnection::DeleteEntries(mozIStorageConnection* aConnection,
                                 const nsTArray<EntryId>& aEntryIdList,
                                 uint32_t aPos, int32_t aLen)
{
  MOZ_ASSERT(aConnection);

  if (aEntryIdList.Length() < 1) {
    return NS_OK;
  }

  MOZ_ASSERT(aPos < aEntryIdList.Length());

  if (aLen < 0) {
    aLen = aEntryIdList.Length() - aPos;
  }

  // Sqlite limits the number of entries allowed for an IN clause,
  // so split up larger operations.
  if (aLen > MAX_ENTRIES_PER_STATEMENT) {
    uint32_t curPos = aPos;
    int32_t remaining = aLen;
    while (remaining > 0) {
      int32_t curLen = std::min(MAX_ENTRIES_PER_STATEMENT, remaining);
      nsresult rv = DeleteEntries(aConnection, aEntryIdList, curPos, curLen);
      NS_ENSURE_SUCCESS(rv, rv);

      curPos += curLen;
      remaining -= curLen;
    }
    return NS_OK;
  }

  nsCOMPtr<mozIStorageStatement> statement;
  nsAutoCString query(
    "DELETE FROM response_headers WHERE map_id IN ("
  );
  AppendListParamsToQuery(query, aEntryIdList, aPos, aLen);
  query.Append(NS_LITERAL_CSTRING(")"));

  nsresult rv = aConnection->CreateStatement(query, getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = BindListParamsToQuery(statement, aEntryIdList, aPos, aLen);
  NS_ENSURE_SUCCESS(rv, rv);

  // Don't stop if this fails.  There may not be any entries in this table,
  // but we must continue to process the other tables.
  statement->Execute();

  query = NS_LITERAL_CSTRING(
    "DELETE FROM request_headers WHERE map_id IN ("
  );
  AppendListParamsToQuery(query, aEntryIdList, aPos, aLen);
  query.Append(NS_LITERAL_CSTRING(")"));

  rv = aConnection->CreateStatement(query, getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = BindListParamsToQuery(statement, aEntryIdList, aPos, aLen);
  NS_ENSURE_SUCCESS(rv, rv);

  // Don't stop if this fails.  There may not be any entries in this table,
  // but we must continue to process the other tables.
  statement->Execute();

  query = NS_LITERAL_CSTRING(
    "DELETE FROM map WHERE id IN ("
  );
  AppendListParamsToQuery(query, aEntryIdList, aPos, aLen);
  query.Append(NS_LITERAL_CSTRING(")"));

  rv = aConnection->CreateStatement(query, getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = BindListParamsToQuery(statement, aEntryIdList, aPos, aLen);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
CacheDBConnection::InsertEntry(mozIStorageConnection* aConnection,
                               const PCacheRequest& aRequest,
                               const PCacheResponse& aResponse)
{
  MOZ_ASSERT(aConnection);

  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO map ("
      "request_method, "
      "request_url, "
      "request_url_no_query, "
      "request_mode, "
      "request_credentials, "
      "response_type, "
      "response_status, "
      "response_status_text "
    ") VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)"
  ), getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindUTF8StringParameter(0, aRequest.method());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindStringParameter(1, aRequest.url());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindStringParameter(2, aRequest.urlWithoutQuery());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt32Parameter(3, static_cast<int32_t>(aRequest.mode()));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt32Parameter(4,
    static_cast<int32_t>(aRequest.credentials()));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt32Parameter(5, static_cast<int32_t>(aResponse.type()));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt32Parameter(6, aResponse.status());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindUTF8StringParameter(7, aResponse.statusText());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT last_insert_rowid()"
  ), getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMoreData;
  rv = statement->ExecuteStep(&hasMoreData);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t mapId;
  rv = statement->GetInt32(0, &mapId);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO request_headers ("
      "name, "
      "value, "
      "map_id "
    ") VALUES (?1, ?2, ?3)"
  ), getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  const nsTArray<PHeadersEntry>& requestHeaders = aRequest.headers().list();
  for (uint32_t i = 0; i < requestHeaders.Length(); ++i) {
    rv = statement->BindUTF8StringParameter(0, requestHeaders[i].name());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->BindUTF8StringParameter(1, requestHeaders[i].value());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->BindInt32Parameter(2, mapId);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO response_headers ("
      "name, "
      "value, "
      "map_id "
    ") VALUES (?1, ?2, ?3)"
  ), getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  const nsTArray<PHeadersEntry>& responseHeaders = aResponse.headers().list();
  for (uint32_t i = 0; i < responseHeaders.Length(); ++i) {
    rv = statement->BindUTF8StringParameter(0, responseHeaders[i].name());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->BindUTF8StringParameter(1, responseHeaders[i].value());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->BindInt32Parameter(2, mapId);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->Execute();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
CacheDBConnection::ReadResponse(mozIStorageConnection* aConnection,
                                EntryId aEntryId, PCacheResponse& aResponseOut)
{
  MOZ_ASSERT(aConnection);
  nsCOMPtr<mozIStorageStatement> statement;
  nsresult rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT "
      "response_type, "
      "response_status, "
      "response_status_text "
    "FROM map "
    "WHERE id=?1 "
  ), getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt32Parameter(0, aEntryId);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasMoreData;
  rv = statement->ExecuteStep(&hasMoreData);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t type;
  rv = statement->GetInt32(0, &type);
  NS_ENSURE_SUCCESS(rv, rv);
  aResponseOut.type() = static_cast<ResponseType>(type);

  int32_t status;
  rv = statement->GetInt32(1, &status);
  NS_ENSURE_SUCCESS(rv, rv);
  aResponseOut.status() = status;

  rv = statement->GetUTF8String(2, aResponseOut.statusText());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT "
      "name, "
      "value "
    "FROM response_headers "
    "WHERE map_id=?1 "
  ), getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt32Parameter(0, aEntryId);
  NS_ENSURE_SUCCESS(rv, rv);

  while(NS_SUCCEEDED(statement->ExecuteStep(&hasMoreData)) && hasMoreData) {
    PHeadersEntry* header = aResponseOut.headers().list().AppendElement();
    NS_ENSURE_TRUE(header, NS_ERROR_OUT_OF_MEMORY);

    rv = statement->GetUTF8String(0, header->name());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = statement->GetUTF8String(1, header->value());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

void
CacheDBConnection::OnMatchComplete(RequestId aRequestId, nsresult aRv,
                                   const PCacheResponseOrVoid& aResponse)
{
  if (mListener) {
    mListener->OnMatch(aRequestId, aRv, aResponse);
  }
}

void
CacheDBConnection::OnMatchAllComplete(RequestId aRequestId, nsresult aRv,
                                      const nsTArray<PCacheResponse>& aResponses)
{
  if (mListener) {
    mListener->OnMatchAll(aRequestId, aRv, aResponses);
  }
}

void
CacheDBConnection::OnPutComplete(RequestId aRequestId, nsresult aRv,
                                 const PCacheResponseOrVoid& aResponse)
{
  if (mListener) {
    mListener->OnPut(aRequestId, aRv, aResponse);
  }
}

void
CacheDBConnection::OnDeleteComplete(RequestId aRequestId, nsresult aRv,
                                    bool aSuccess)
{
  if (mListener) {
    mListener->OnDelete(aRequestId, aRv, aSuccess);
  }
}


} // namespace dom
} // namespace mozilla
