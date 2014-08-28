/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CacheDBConnection.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "nsServiceManagerUtils.h"
#include "mozIStorageConnection.h"
#include "mozIStorageService.h"
#include "mozIStorageStatement.h"
#include "mozStorageCID.h"
#include "nsIFile.h"
#include "nsIUUIDGenerator.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace dom {

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
                                     already_AddRefed<mozIStorageConnection> aConnection)
{
}

CacheDBConnection::~CacheDBConnection()
{
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

  // TODO: do we need any pragmas?
  // TODO: enable foreign key support

  int32_t schemaVersion;
  rv = conn->GetSchemaVersion(&schemaVersion);
  NS_ENSURE_SUCCESS(rv, nullptr);

  if (!schemaVersion) {
    rv = conn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE requests ("
        "method TEXT NOT NULL, "
        "url TEXT NOT NULL, "
        "mode INTEGER NOT NULL, "
        "credentials INTEGER NOT NULL, "
        //"body_file TEXT NOT NULL, "
        "PRIMARY KEY(rowid)"
      ");"
    ));
    NS_ENSURE_SUCCESS(rv, nullptr);

    rv = conn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE request_headers ("
        "name TEXT NOT NULL, "
        "value TEXT NOT NULL, "
        "request_rowid INTEGER NOT NULL, "
        "PRIMARY KEY(rowid), "
        "FOREIGN KEY(request_rowid) REFERENCES requests(rowid)"
      ");"
    ));
    NS_ENSURE_SUCCESS(rv, nullptr);

    rv = conn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE responses ("
        "type INTEGER NOT NULL, "
        "status INTEGER NOT NULL, "
        "statusText TEXT NOT NULL, "
        //"body_file TEXT NOT NULL, "
        "PRIMARY KEY(rowid)"
      ");"
    ));
    NS_ENSURE_SUCCESS(rv, nullptr);

    rv = conn->ExecuteSimpleSQL(NS_LITERAL_CSTRING(
      "CREATE TABLE response_headers ("
        "name TEXT NOT NULL, "
        "value TEXT NOT NULL, "
        "response_rowid INTEGER NOT NULL, "
        "PRIMARY KEY(rowid), "
        "FOREIGN KEY(response_rowid) REFERENCES responses(rowid)"
      ");"
    ));
    NS_ENSURE_SUCCESS(rv, nullptr);

    conn->SetSchemaVersion(kLatestSchemaVersion);
    NS_ENSURE_SUCCESS(rv, nullptr);

    rv = conn->GetSchemaVersion(&schemaVersion);
    NS_ENSURE_SUCCESS(rv, nullptr);
  }

  NS_ENSURE_TRUE(schemaVersion == kLatestSchemaVersion, nullptr);

  nsRefPtr<CacheDBConnection> ref = new CacheDBConnection(aListener,
                                                          conn.forget());
  return ref.forget();
}

} // namespace dom
} // namespace mozilla
