/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CacheStorageDBConnection.h"

#include "mozilla/dom/CacheStorageDBListener.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozIStorageConnection.h"
#include "mozIStorageService.h"
#include "mozIStorageStatement.h"
#include "mozStorageCID.h"
#include "nsIFile.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace dom {

using mozilla::dom::cache::Namespace;
using mozilla::dom::cache::RequestId;
using mozilla::dom::quota::PERSISTENCE_TYPE_PERSISTENT;
using mozilla::dom::quota::PersistenceType;
using mozilla::dom::quota::QuotaManager;

// static
already_AddRefed<CacheStorageDBConnection>
CacheStorageDBConnection::Get(CacheStorageDBListener& aListener,
                              Namespace aNamespace,
                              const nsACString& aOrigin,
                              const nsACString& aBaseDomain)
{
  return GetOrCreateInternal(aListener, aNamespace, aOrigin, aBaseDomain, false);
}

// static
already_AddRefed<CacheStorageDBConnection>
CacheStorageDBConnection::GetOrCreate(CacheStorageDBListener& aListener,
                                      Namespace aNamespace,
                                      const nsACString& aOrigin,
                                      const nsACString& aBaseDomain)
{
  return GetOrCreateInternal(aListener, aNamespace, aOrigin, aBaseDomain, true);
}

nsresult
CacheStorageDBConnection::Get(RequestId aRequestId, const nsAString& aKey)
{
  nsCOMPtr<mozIStorageStatement> statement;

  nsresult rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT cache_uuid FROM caches WHERE namespace=?1 AND key=?2"
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

  nsAutoCString uuidString;
  rv = statement->GetUTF8String(0, uuidString);
  NS_ENSURE_SUCCESS(rv, rv);

  nsID uuid;
  NS_ENSURE_TRUE(uuid.Parse(uuidString.get()), NS_ERROR_FAILURE);

  mListener.OnGet(aRequestId, uuid);
  return NS_OK;
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

nsresult
CacheStorageDBConnection::Put(RequestId aRequestId, const nsAString& aKey,
                              const nsID& aCacheId)
{
  nsCOMPtr<mozIStorageStatement> statement;

  nsresult rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO caches (namespace, key, cache_uuid)VALUES(?1, ?2, ?3)"
  ), getter_AddRefs(statement));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindInt32Parameter(0, mNamespace);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = statement->BindStringParameter(1, aKey);
  NS_ENSURE_SUCCESS(rv, rv);

  char uuidBuf[NSID_LENGTH];
  aCacheId.ToProvidedString(uuidBuf);

  rv = statement->BindUTF8StringParameter(2, nsAutoCString(uuidBuf));
  NS_ENSURE_SUCCESS(rv, rv);

  // TODO: do this async
  // TODO: use a transaction

  rv = statement->Execute();
  bool result = NS_SUCCEEDED(rv);

  mListener.OnPut(aRequestId, result);
  return NS_OK;
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
    "SELECT key FROM caches WHERE namespace=?1"
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

CacheStorageDBConnection::
CacheStorageDBConnection(CacheStorageDBListener& aListener,
                         Namespace aNamespace,
                         already_AddRefed<mozIStorageConnection> aConnection)
  : mListener(aListener)
  , mNamespace(aNamespace)
  , mConnection(aConnection)
{
}

CacheStorageDBConnection::~CacheStorageDBConnection()
{
}

//static
already_AddRefed<CacheStorageDBConnection>
CacheStorageDBConnection::GetOrCreateInternal(CacheStorageDBListener& aListener,
                                              Namespace aNamespace,
                                              const nsACString& aOrigin,
                                              const nsACString& aBaseDomain,
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

  rv = dbDir->Append(NS_LITERAL_STRING("cachestorage"));
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

  // TODO: do we need any pragmas?

  int32_t schemaVersion;
  rv = conn->GetSchemaVersion(&schemaVersion);
  NS_ENSURE_SUCCESS(rv, nullptr);

  if (!schemaVersion) {
    rv = conn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE caches ("
        "namespace INTEGER NOT NULL, "
        "key TEXT NOT NULL, "
        "cache_uuid TEXT NOT NULL, "
        "PRIMARY KEY(namespace, key)"
      ");"
    ));
    NS_ENSURE_SUCCESS(rv, nullptr);

    conn->SetSchemaVersion(kLatestSchemaVersion);
    NS_ENSURE_SUCCESS(rv, nullptr);

    rv = conn->GetSchemaVersion(&schemaVersion);
    NS_ENSURE_SUCCESS(rv, nullptr);
  }

  NS_ENSURE_TRUE(schemaVersion == kLatestSchemaVersion, nullptr);

  nsRefPtr<CacheStorageDBConnection> ref =
    new CacheStorageDBConnection(aListener, aNamespace, conn.forget());
  return ref.forget();
}

} // namespace dom
} // namespace mozilla
