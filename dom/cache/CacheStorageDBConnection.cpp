/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CacheStorageDBConnection.h"

#include "mozilla/dom/quota/QuotaManager.h"
#include "mozIStorageConnection.h"
#include "mozIStorageService.h"
#include "mozStorageCID.h"
#include "nsIFile.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace dom {

using mozilla::dom::quota::PERSISTENCE_TYPE_PERSISTENT;
using mozilla::dom::quota::PersistenceType;
using mozilla::dom::quota::QuotaManager;

// static
already_AddRefed<CacheStorageDBConnection>
CacheStorageDBConnection::Get(const nsACString& aOrigin,
                              const nsACString& aBaseDomain)
{
  return GetOrCreateInternal(aOrigin, aBaseDomain, false);
}

// static
already_AddRefed<CacheStorageDBConnection>
CacheStorageDBConnection::GetOrCreate(const nsACString& aOrigin,
                                      const nsACString& aBaseDomain)
{
  return GetOrCreateInternal(aOrigin, aBaseDomain, true);
}

CacheStorageDBConnection::
CacheStorageDBConnection(already_AddRefed<mozIStorageConnection> aConnection)
  : mConnection(aConnection)
{
}

CacheStorageDBConnection::~CacheStorageDBConnection()
{
}

already_AddRefed<nsID>
CacheStorageDBConnection::Get(const nsAString& aKey)
{
  return nullptr;
}

bool
CacheStorageDBConnection::Has(const nsAString& aKey)
{
  return false;
}

bool
CacheStorageDBConnection::Put(const nsAString& aKey, nsID* aCacheId)
{
  return false;
}

bool
CacheStorageDBConnection::Delete(const nsAString& aKey)
{
  return false;
}

void
CacheStorageDBConnection::Keys(nsTArray<nsString> aKeysOut)
{
}

//static
already_AddRefed<CacheStorageDBConnection>
CacheStorageDBConnection::GetOrCreateInternal(const nsACString& aOrigin,
                                              const nsACString& aBaseDomain,
                                              bool allowCreate)
{
  QuotaManager* quota = QuotaManager::Get();
  MOZ_ASSERT(quota);

  const PersistenceType persistenceType = PERSISTENCE_TYPE_PERSISTENT;

  nsCOMPtr<nsIFile> dbDir;
  nsresult rv = quota->EnsureOriginIsInitialized(persistenceType,
                                                 aBaseDomain, aOrigin,
                                                 true, // aTrackQuota
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
        "key TEXT NOT NULL PRIMARY KEY, "
        "cache_uuid TEXT NOT NULL"
      ");"
    ));
    NS_ENSURE_SUCCESS(rv, nullptr);

    conn->SetSchemaVersion(kLatestSchemaVersion);
    NS_ENSURE_SUCCESS(rv, nullptr);
  }

  NS_ENSURE_TRUE(schemaVersion == kLatestSchemaVersion, nullptr);

  nsRefPtr<CacheStorageDBConnection> ref =
    new CacheStorageDBConnection(conn.forget());
  return ref.forget();
}

} // namespace dom
} // namespace mozilla
