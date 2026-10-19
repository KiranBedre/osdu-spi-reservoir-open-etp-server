# OSDU stub server

## Purpose of creation
Assistance with local development and testing of Open-ETP-Server in the **multi-partition mode** (when required OSDU environment). Even if you have real OSDU environment you still need to make stubs for Azure Identity service which is not accessible externally. Using real environment also will lead to fill it with a lot of dirty data, part of them could be not consistent or corrupted during testing/developing new features by developers. OSDU stub server helps to avoid such issues, it emulates all necessary APIs for Open-ETP-Server.

## Build and run
- Make sure that CMake option `BUILD_OSDU_STUB_SERVER` is `ON` (enabled by default)
- Build Open-ETP-Server as usually
- Set environment variable `POSTGRESQL_CONN_STRING` with connection string to PostgreSQL (the same as needed for Open-ETP-Server)
- Run `osduStubServer` from your build directory

Should be displayed:
```
-= OSDU stub server =-
Listening HTTP on port 9080
```

Local HTTP server is available **localhost:9080** by default, you can try to make request to one of endpoints via browser, for example: [http://localhost:9080/partitions](http://localhost:9080/partitions). If you set the environment variable or config file (/StubServer/StubPort) a new value, then you have to change the port in the url as well.

## Configuring and run Open-ETP-Server
Before you will run Open-ETP-server, need to configure OSDU endpoints that are passing via environment variables. Below the full list of variables that are required for work with Azure provider:
| Environment variable | Value |
| ------ | ------ |
| STUB_PORT | 9080 |
| STUB_SERVER_PARTITION_IDS | local,local2 |
| RDMS_DATA_PARTITION_MODE | multiple |
| RDMS_DATA_CONNECTIVITY_MODE | osdu |
| CLOUD_PROVIDER | az |
| OSDU_HOST | http://localhost:9080 |
| AZURE_ENABLE_MSI | true |
| AZURE_PRINCIPAL_ID | test-principal-id |
| AZURE_PRINCIPAL_SECRET | secret |
| AZURE_AD_TENANT_ID | test-tenant-id |
| AZURE_AD_APP_RESOURCE_ID | test-res-id |
| DOMAIN_NAME | test |
| KEYVAULT_URL | http://localhost:9080 |
| IDENTITY_ENDPOINT | http://localhost:9080/identity/oauth2/token |
| PARTITION_URL | http://localhost:9080 |
| ENTITLEMENTS_URL | http://localhost:9080 |
| STORAGE_URL | http://localhost:9080 |

You can set multiple partitions with the config file (/DataPartitions/PartitionStubIds) or environment variable STUB_SERVER_PARTITION_IDS, separated by comma "," line ```partition1,partition2,partitionN```.

Run Open-ETP-Server:
```shell
./openETPServer server --start --authN none --debug --verbose
```
If everything were configured correctly, Open-ETP-Server should connect to OSDU stub server, get all necessary requisites and go to listening mode. You can try to create dataspace in the test `local` partition via below command:
```shell
./openETPServer space -S ws://localhost:9002 --username foo --password bar --data-partition-id local --new -s demo/Volve
```

## OSDU state
To be consistent with data stored in the database, OSDU stub server saves current state as JSON files to sub-directory `./OsduStubServer/`. You can safely remove this, but better to do it together with clean-up PostgreSQL DB.
