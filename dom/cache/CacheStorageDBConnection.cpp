/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CacheStorageDBConnection.h"

#include "mozilla/dom/CacheQuotaRunnable.h"
#include "mozilla/dom/CacheStorageDBListener.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/UniquePtr.h"
#include "mozIStorageConnection.h"
#include "mozIStorageService.h"
#include "mozIStorageStatement.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"
#include "nsIFile.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace dom {

using mozilla::UniquePtr;
using mozilla::dom::cache::Namespace;
using mozilla::dom::cache::RequestId;
using mozilla::dom::quota::PERSISTENCE_TYPE_PERSISTENT;
using mozilla::dom::quota::PersistenceType;
using mozilla::dom::quota::QuotaManager;

class CacheStorageDBConnection::OpenRunnable : public CacheQuotaRunnable
{
public:
  OpenRunnable(Namespace aNamespace, const nsACString& aOrigin,
               const nsACString& aBaseDomain, bool aAllowCreate,
               CacheStorageDBConnection* aCacheStorageDBConnection)
    : CacheQuotaRunnable(aOrigin, aBaseDomain, NS_LITERAL_CSTRING("CacheStorage"))
    , mNamespace(aNamespace)
    , mAllowCreate(aAllowCreate)
    , mCacheStorageDBConnection(aCacheStorageDBConnection)
    , mResult(NS_OK)
  {
    MOZ_ASSERT(mCacheStorageDBConnection);
  }

protected:
  virtual void RunOnQuotaIOThread(const nsACString& aOrigin,
                                  const nsACString& aBaseDomain,
                                  nsIFile* aQuotaDir) MOZ_OVERRIDE
  {
    mResult = aQuotaDir->Append(NS_LITERAL_STRING("cachestorage"));
    if (NS_FAILED(mResult)) { return; }

    bool exists;
    mResult = aQuotaDir->Exists(&exists);
    if (NS_FAILED(mResult) || mAllowCreate) { return; }

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
    if (NS_FAILED(mResult) || mAllowCreate) { return; }

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
    if (!dbFileUrl) {
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
    if (!ss) {
      mResult = NS_ERROR_FAILURE;
      return;
    }

    nsCOMPtr<mozIStorageConnection> conn;
    mResult = ss->OpenDatabaseWithFileURL(dbFileUrl, getter_AddRefs(conn));
    if (mResult == NS_ERROR_FILE_CORRUPTED) {
      mResult = dbFile->Remove(false);
      if (NS_FAILED(mResult)) { return; }

      mResult = dbTmpDir->Exists(&exists);
      if (NS_FAILED(mResult)) { return; }

      if (exists) {
        bool isDir;
        mResult = dbTmpDir->IsDirectory(&isDir);
        if (NS_FAILED(mResult)) { return; }
        if (!isDir) {
          mResult = NS_ERROR_FAILURE;
          return;
        }
        mResult = dbTmpDir->Remove(true);
        if (NS_FAILED(mResult)) { return; }
      }
    }
    if (NS_FAILED(mResult)) { return; }

  #if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_GONK)
    mResult = conn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      // Switch the journaling mode to TRUNCATE to avoid changing the directory
      // structure at the conclusion of every transaction for devices with slower
      // file systems.
      "PRAGMA journal_mode = TRUNCATE; "
    ));
    if (NS_FAILED(mResult)) { return; }
  #endif

    int32_t schemaVersion;
    mResult = conn->GetSchemaVersion(&schemaVersion);
    if (NS_FAILED(mResult)) { return; }

    mozStorageTransaction trans(conn, false,
                                mozIStorageConnection::TRANSACTION_IMMEDIATE);

    if (!schemaVersion) {
      mResult = conn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
        "CREATE TABLE caches ("
          "namespace INTEGER NOT NULL, "
          "key TEXT NOT NULL, "
          "cache_uuid TEXT NOT NULL, "
          "PRIMARY KEY(namespace, key)"
        ");"
      ));
      if (NS_FAILED(mResult)) { return; }

      mResult = conn->SetSchemaVersion(kLatestSchemaVersion);
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

    mConnection = conn.forget();
  }

  virtual void CompleteOnInitiatingThread(nsresult aRv) MOZ_OVERRIDE
  {
    nsresult rv = NS_FAILED(aRv) ? aRv : mResult;
    mCacheStorageDBConnection->OnOpenComplete(rv, mConnection.forget());
  }

protected:
  virtual ~OpenRunnable() { }

  const Namespace mNamespace;
  const bool mAllowCreate;
  nsRefPtr<CacheStorageDBConnection> mCacheStorageDBConnection;
  nsCOMPtr<mozIStorageConnection> mConnection;
  nsresult mResult;
};

class CacheStorageDBConnection::GetRunnable MOZ_FINAL :
  public CacheStorageDBConnection::OpenRunnable
{
public:
  GetRunnable(RequestId aRequestId, const nsAString& aKey,
              Namespace aNamespace, const nsACString& aOrigin,
              const nsACString& aBaseDomain,
              CacheStorageDBConnection* aCacheStorageDBConnection)
    : OpenRunnable(aNamespace, aOrigin, aBaseDomain, false, aCacheStorageDBConnection)
    , mRequestId(aRequestId)
    , mKey(aKey)
  {
    MOZ_ASSERT(mCacheStorageDBConnection);
  }

protected:
  virtual void RunOnQuotaIOThread(const nsACString& aOrigin,
                                  const nsACString& aBaseDomain,
                                  nsIFile* aQuotaDir) MOZ_OVERRIDE
  {
    OpenRunnable::RunOnQuotaIOThread(aOrigin, aBaseDomain, aQuotaDir);
    if (NS_FAILED(mResult)) {
      return;
    }

    nsCOMPtr<mozIStorageStatement> statement;

    mResult = mConnection->CreateStatement(NS_LITERAL_CSTRING(
      "SELECT cache_uuid FROM caches WHERE namespace=?1 AND key=?2"
    ), getter_AddRefs(statement));
    if (NS_FAILED(mResult)) { return; }

    mResult = statement->BindInt32Parameter(0, mNamespace);
    if (NS_FAILED(mResult)) { return; }

    mResult = statement->BindStringParameter(1, mKey);
    if (NS_FAILED(mResult)) { return; }

    bool hasMoreData;
    mResult = statement->ExecuteStep(&hasMoreData);
    if (NS_FAILED(mResult)) { return; }

    if (!hasMoreData) {
      return;
    }

    nsAutoCString uuidString;
    mResult = statement->GetUTF8String(0, uuidString);
    if (NS_FAILED(mResult)) { return; }

    mCacheId.reset(new nsID());
    if (!mCacheId) {
      mResult = NS_ERROR_OUT_OF_MEMORY;
      return;
    }

    if (!mCacheId->Parse(uuidString.get())) {
      mResult = NS_ERROR_FAILURE;
      mCacheId = nullptr;
      return;
    }

    statement = nullptr;
    mConnection = nullptr;
  }

  virtual void CompleteOnInitiatingThread(nsresult aRv) MOZ_OVERRIDE
  {
    nsresult rv = NS_FAILED(aRv) ? aRv : mResult;
    mCacheStorageDBConnection->OnGetComplete(mRequestId, rv, mCacheId.get());
  }

private:
  virtual ~GetRunnable() { }

  const RequestId mRequestId;
  const nsString mKey;
  UniquePtr<nsID> mCacheId;
};

class CacheStorageDBConnection::PutRunnable MOZ_FINAL :
  public CacheStorageDBConnection::OpenRunnable
{
public:
  PutRunnable(RequestId aRequestId, const nsAString& aKey,
              const nsID& aCacheId,
              Namespace aNamespace, const nsACString& aOrigin,
              const nsACString& aBaseDomain,
              CacheStorageDBConnection* aCacheStorageDBConnection)
    : OpenRunnable(aNamespace, aOrigin, aBaseDomain, false, aCacheStorageDBConnection)
    , mRequestId(aRequestId)
    , mKey(aKey)
    , mCacheId(aCacheId)
    , mSuccess(false)
  {
    MOZ_ASSERT(mCacheStorageDBConnection);
  }

protected:
  virtual void RunOnQuotaIOThread(const nsACString& aOrigin,
                                  const nsACString& aBaseDomain,
                                  nsIFile* aQuotaDir) MOZ_OVERRIDE
  {
    OpenRunnable::RunOnQuotaIOThread(aOrigin, aBaseDomain, aQuotaDir);
    if (NS_FAILED(mResult)) {
      return;
    }

    nsCOMPtr<mozIStorageStatement> statement;

    mResult = mConnection->CreateStatement(NS_LITERAL_CSTRING(
      "INSERT INTO caches (namespace, key, cache_uuid)VALUES(?1, ?2, ?3)"
    ), getter_AddRefs(statement));
    if (NS_FAILED(mResult)) { return; }

    mResult = statement->BindInt32Parameter(0, mNamespace);
    if (NS_FAILED(mResult)) { return; }

    mResult = statement->BindStringParameter(1, mKey);
    if (NS_FAILED(mResult)) { return; }

    char uuidBuf[NSID_LENGTH];
    mCacheId.ToProvidedString(uuidBuf);

    mResult = statement->BindUTF8StringParameter(2, nsAutoCString(uuidBuf));
    if (NS_FAILED(mResult)) { return; }

    // TODO: do this async
    // TODO: use a transaction

    nsresult rv = statement->Execute();
    mSuccess = NS_SUCCEEDED(rv);

    statement = nullptr;
    mConnection = nullptr;
  }

  virtual void CompleteOnInitiatingThread(nsresult aRv) MOZ_OVERRIDE
  {
    nsresult rv = NS_FAILED(aRv) ? aRv : mResult;
    mCacheStorageDBConnection->OnPutComplete(mRequestId, rv, mSuccess);
  }

private:
  virtual ~PutRunnable() { }

  const RequestId mRequestId;
  const nsString mKey;
  const nsID mCacheId;
  bool mSuccess;
};

CacheStorageDBConnection::
CacheStorageDBConnection(CacheStorageDBListener& aListener,
                         Namespace aNamespace,
                         const nsACString& aOrigin,
                         const nsACString& aBaseDomain,
                         bool aAllowCreate)
  : mListener(aListener)
  , mNamespace(aNamespace)
  , mOrigin(aOrigin)
  , mBaseDomain(aBaseDomain)
  , mOwningThread(NS_GetCurrentThread())
  , mFailed(false)
{
  MOZ_ASSERT(mOwningThread);

  nsRefPtr<OpenRunnable> open = new OpenRunnable(aNamespace,
                                                 aOrigin,
                                                 aBaseDomain,
                                                 aAllowCreate,
                                                 this);
  if (!open) {
    ReportError(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  open->Dispatch();
}

void
CacheStorageDBConnection::Get(RequestId aRequestId, const nsAString& aKey)
{
  nsRefPtr<GetRunnable> get = new GetRunnable(aRequestId, aKey,
                                              mNamespace, mOrigin,
                                              mBaseDomain, this);
  if (!get) {
    ReportError(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  get->Dispatch();
}

nsresult
CacheStorageDBConnection::Has(RequestId aRequestId, const nsAString& aKey)
{
  nsCOMPtr<mozIStorageStatement> statement;

  nsresult rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT count(*) FROM caches WHERE namespace=?1 AND key=?2"
  ), getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt32Parameter(0, mNamespace);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindStringParameter(1, aKey);
  NS_ENSURE_SUCCESS(rv, rv);

  // TODO: do this async
  // TODO: use a transaction

  bool hasMoreData;
  rv = statement->ExecuteStep(&hasMoreData);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t count;
  rv = statement->GetInt32(0, &count);
  NS_ENSURE_SUCCESS(rv, rv);

  mListener.OnHas(aRequestId, count > 0);
  return NS_OK;
}

void
CacheStorageDBConnection::Put(RequestId aRequestId, const nsAString& aKey,
                              const nsID& aCacheId)
{
  nsRefPtr<PutRunnable> put = new PutRunnable(aRequestId, aKey, aCacheId,
                                              mNamespace, mOrigin,
                                              mBaseDomain, this);
  if (!put) {
    ReportError(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  put->Dispatch();
}

nsresult
CacheStorageDBConnection::Delete(RequestId aRequestId, const nsAString& aKey)
{
  nsCOMPtr<mozIStorageStatement> statement;

  nsresult rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM caches WHERE namespace=?1 AND key=?2"
  ), getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt32Parameter(0, mNamespace);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindStringParameter(1, aKey);
  NS_ENSURE_SUCCESS(rv, rv);

  // TODO: do this async
  // TODO: use a transaction

  rv = statement->Execute();
  bool result = NS_SUCCEEDED(rv);

  mListener.OnDelete(aRequestId, result);
  return NS_OK;
}

nsresult
CacheStorageDBConnection::Keys(RequestId aRequestId)
{
  nsCOMPtr<mozIStorageStatement> statement;

  nsresult rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT key FROM caches WHERE namespace=?1 ORDER BY rowid"
  ), getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt32Parameter(0, mNamespace);
  NS_ENSURE_SUCCESS(rv, rv);

  // TODO: do this async
  // TODO: use a transaction

  nsTArray<nsString> keys;

  bool hasMoreData;
  while(NS_SUCCEEDED(statement->ExecuteStep(&hasMoreData)) && hasMoreData) {
    nsString* key = keys.AppendElement();
    NS_ENSURE_TRUE(key, NS_ERROR_OUT_OF_MEMORY);
    rv = statement->GetString(0, *key);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  mListener.OnKeys(aRequestId, keys);
  return NS_OK;
}

CacheStorageDBConnection::~CacheStorageDBConnection()
{
}

void
CacheStorageDBConnection::OnOpenComplete(nsresult aRv,
                          already_AddRefed<mozIStorageConnection> aConnection)
{
  if (NS_FAILED(aRv)) {
    ReportError(aRv);
  }
  mConnection = aConnection;
}

void
CacheStorageDBConnection::OnGetComplete(RequestId aRequestId, nsresult aRv,
                                        nsID* aCacheId)
{
  mListener.OnGet(aRequestId, aRv, aCacheId);
}

void
CacheStorageDBConnection::OnPutComplete(RequestId aRequestId, nsresult aRv,
                                        bool aSuccess)
{
  mListener.OnPut(aRequestId, aRv, aSuccess);
}

void
CacheStorageDBConnection::ReportError(nsresult aRv)
{
  mFailed = true;
  mListener.OnError(aRv);
}

} // namespace dom
} // namespace mozilla
