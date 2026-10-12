# Install Reservoir DMS on Azure

## Overview

The Reservoir DDMS runs on Azure using Azure Kubernetes Service (AKS) and Azure Database for PostgreSQL - Flexible Server. This document covers how to set up these resources and deploy the Reservoir DDMS to them.

RDDMS supports the use of a managed identity for authenticating requests to other services. Using a managed identity is strongly recommended to enhance security. To enable this feature, make sure you follow the additional steps in each section below.

This managed identity can also be used to connect to the Azure PostgreSQL database. This is also strongly recommended to further enhance security. If you are operating an existing RDDMS instance and wish to enable this feature, make sure you follow the [PostgreSQL Managed Identity Migration Guide](#postgresql-managed-identity-migration-guide) to avoid loss of database access!

## Contents

1. [Prerequisites](#prerequisites)
1. [Bootstrap Partition Infrastructure for OETP](#bootstrap-partition-infrastructure-for-oetp)
1. [AKS Setup](#aks-setup)
1. [PostgreSQL Setup](#postgresql-setup)
1. [Partition Service Setup](#partition-service-setup)
1. [AKS Deployment](#aks-deployment)
    - [Docker Build](#docker-build)
    - [Configuration](#configuration)
    - [Deploy](#deploy)
1. [PostgreSQL Managed Identity Migration Guide](#postgresql-managed-identity-migration-guide)
1. [Archived Docs](#archived-docs)

## Prerequisites

### For RDDMS
- **An existing OSDU instance**: RDDMS supports connections with services such as the Entitlements and Parition Services. RDDMS on Azure should leverage this integration.

### For Running Commands Seen in This Guide
- [AZ CLI](https://learn.microsoft.com/en-us/cli/azure/install-azure-cli)
- [Docker](https://www.docker.com/)

## Bootstrap Partition Infrastructure for OETP

Reuse the original [data_partition](https://community.opengroup.org/osdu/platform/deployment-and-operations/infra-azure-provisioning/-/tree/v0.14.0/infra/templates/osdu-r3-mvp/data_partition) infrastructure and add feature flag in `terraform.tfvars`:

```terraform
# Enable Reservoir DDMS resources
reservoir_ddms = {
  enabled=true
  sku="GP_Gen5_4"
}
```

## AKS Setup

RDDMS should be deployed on an [AKS Cluster](https://azure.microsoft.com/en-us/products/kubernetes-service). Deployment to the cluster is covered in a later section, but an existing cluster is required for some of the setup steps below (in particular when using managed identity)

### Additional Setup for Managed Identity

Using a managed identity requires that the AKS cluster is configured with either [AKS Workload Identity](https://learn.microsoft.com/en-us/azure/aks/workload-identity-deploy-cluster) or [AKS Pod-Managed Identity (deprecated)](https://learn.microsoft.com/en-us/azure/aks/use-azure-ad-pod-identity). Once this is done, take note of the managed identity's name and client ID for later steps.

## PostgreSQL Setup

Each data partition should have an instance of an [Azure PostgreSQL Flexible Server](https://learn.microsoft.com/en-us/azure/postgresql/). The server should be set up with PostgreSQL authentication, and the connection string should use the credentials for the admin user.

### Additional Setup for Managed Identity

The easiest way to authorize a managed identity to access Azure PostgreSQL is to add the managed identity as a Microsoft Entra Administrator. This can be done while deploying Azure PostgreSQL, or you can configure it through the Azure portal by navigating to Security -> Authentication. Alternatively, you can also create the user manually by executing commands against the database as documented in [Manually Create a Managed Identity User for Azure PostgreSQL](#manually-create-a-managed-identity-user-for-azure-postgresql).

After configuring the user, connection strings used by RDDMS should use the managed identity name as the user. The password for this user is an access token which will be fetched by the service at runtime, so connection strings given to the service *should not* contain a password. In addition, it is strongly recommended to disable PostgreSQL authentication and only allow Microsoft Entra authentication to enchance database security. This can also be configured from the Security -> Administration page.

## Partition Service Setup

To register the secret metadata of the database in the Partition Service, the Terraform code will create the needed secret for Azure PostgreSQL with this request:

`PUT /api/partition/v1/partitions/<partitionname>`

```json
{
    "properties": {
        "compliance-ruleset": {
            "value": "shared"
        },
        <...>
        "reservoir-connection": {
            "sensitive": true,
            "value": "reservoir-conn"
        }
    }
}
```

### Additional Setup for Managed Identity

As mentioned previously: if you are using a managed identity, the connection strings in the Partition Service should omit the password value. A password will be obtained at runtime from the managed identity credentials.

## AKS Deployment

### Docker Build

To build a container image for use in AKS, use the multi-step [Azure Dockerfile](./Dockerfile). This image is based on Mariner, which is preferred for AKS. You can build the docker file with this command from the repo root:

```bash
IMAGE_TAG="${ACR_NAME}.azurecr.io/${OETP_SERVER_REPO}:${VERSION}"
docker build . -t $IMAGE_TAG --file devops/azure/Dockerfile
```

This is a multi-stage dockerfile with two stages: `build-env` and `runtime`. The build happens in the `build-env` stage, and the build artifacts are copied to the more lightweight `runtime` image. Note that this build does not run any tests and only performs a release build.

If you are having issues building this dockerfile, it may be useful to add the `--progress=plain` argument and redirect output to a file (e.g. by appending `&> build/docker.azure.out.txt`) to enhance debuggability.

Once the dockerfile is built, you can login to your ACR and push the image with the following commands:

```bash
az login
az acr login -n $ACR_NAME
docker push $IMAGE_TAG
```

### Configuration

#### Command-Line Configuration

Authentication and authorization in Azure are typically managed externally and not by RDDMS. In general, you should use the `--authN none` and `--authZ delegate=<url>` arguments when starting the service.

#### Environment Configuration

When deploying to Azure, it is strongly recommended to use the `multiple` parition mode and the `osdu` connectivity mode. Other combinations of these settings may be supported, but are considered less secure and their usage is not documented here.

To configure these settings, define the following variables before starting the server. These are typically managed in an AKS config object. Make sure to replace the placeholders above with values appropriate for your deployment:

```yaml
# Settings required for all Azure deployments
CLOUD_PROVIDER: az
KEYVAULT_URL: <keyvault_url>

# Settings to enable multi-partition mode
RDMS_DATA_PARTITION_MODE: multiple

# Settings to enable osdu connectivity mode
RDMS_DATA_CONNECTIVITY_MODE: osdu
PARTITION_URL: <partition_url>
ENTITLEMENTS_URL: <entitlements_url>
STORAGE_URL: <storage_url>
```

##### Additional Setup for Managed Identity

Enabling managed identity usage is also recommended when deploying to Azure. There are two config settings which control managed identity usage.

The first setting controls whether a managed identity is used to connect to other OSDU services and Key Vault. To enable this feature, use the following setting:

```yaml
AZURE_ENABLE_MSI: true
```

The second setting controls whether a managed identity is used to connect to Azure PostgreSQL. To use this feature, the `multiple` partition mode and the `osdu` connectivity mode are required. To enable this feature, use the following setting:

```yaml
AZURE_ENABLE_POSTGRES_MSI: true
```

If necessary, you can also control the scopes used for access tokens.

The scope of the access token used for OSDU core services is controlled by `AZURE_OSDU_ACCESS_TOKEN_SCOPE_OVERRIDE`. If this is not set, the service will use the value of the `aad-client-id` secret from Key Vault as the scope when interacting with these services.

You can also configure the scopes used when communicating with Key Vault and PostgreSQL. The default values for these variables are shown below, and you generally don't need to change these:

```yaml
AZURE_KEYVAULT_ACCESS_TOKEN_SCOPE: https://vault.azure.com/.default
AZURE_POSTGRES_ACCESS_TOKEN_SCOPE: https://ossrdbms-aad.database.windows.net/.default
```

### Deploy

Use [standard-ddms](https://community.opengroup.org/osdu/platform/deployment-and-operations/helm-charts-azure/-/tree/master/osdu-ddms/standard-ddms) to deploy in your environment.

## PostgreSQL Managed Identity Migration Guide

Newer versions of RDDMS allow for database connections to use a managed identity. Enabling this feature on an RDDMS instance which has existing data requires additional migration steps to avoid loss of data access.

If you do not want to enable this feature, no migration steps are necessary. The feature is fully backwards-compatible and must be explicitly enabled.

If you would like to enable managed identity authentication for PostgreSQL, follow these steps to ensure that migration is successful:

- Prerequisites:
    - Your service must be connected to an OSDU instance and must be configured with `RDMS_DATA_PARTITION_MODE == multiple` and `RDMS_DATA_CONNECTIVITY_MODE == osdu`
    - Your AKS cluster must have a managed identity set up as described in [AKS Setup](#aks-setup), and your service must be configured with `AZURE_ENABLE_MSI == true`
- Complete the [PostgreSQL Setup](#postgresql-setup) to create a managed identity user with the following changes:
    - If you intend to disable PostgreSQL authentication (which is recommended), wait to do so until completing *all* of the other migration steps.
- **If you have an existing database populated with data**, you will need to give the managed identity user access to the existing data. Using the credentials from the connection string currently used by RDDMS (typically the PostgreSQL admin account), connect to the database and execute the following command, replacing `<identity-name>` with the name of the managed identity:
  ```sql
  DO $$
  DECLARE
      r record;
      msi varchar := '<identity-name>';
  BEGIN
      FOR r IN SELECT * FROM information_schema.schemata WHERE schemata.schema_owner = (SELECT USER)
      LOOP
          EXECUTE 'GRANT ALL ON SCHEMA ' || quote_ident(r.schema_name) || ' TO ' || quote_ident(msi);
          EXECUTE 'GRANT ALL ON ALL TABLES IN SCHEMA ' || quote_ident(r.schema_name) || ' TO ' || quote_ident(msi);
      END LOOP;
  END$$;
  ```
- Change the connection string(s) in your partition service:
    - Update the user name to use the managed identity name
    - Remove the password
- Update the AKS environment config:
    - Add the following values:
    ```yaml
    AZURE_POSTGRES_ENABLE_MSI: true
    ```
    - (Optional) Remove the `IDENTITY_ENDPOINT` and `IDENTITY_HEADER` environment variables, if present. This will ensure that you're using the most up-to-date method for retreiving identity credentials.

Once these migration steps are completed, you can deploy the new image to the cluster to use the managed identity. You can also disable PostgreSQL authention to your database now.

## Archived docs

### Manually Create a Managed Identity User for Azure PostgreSQL

To use a managed identity to connect to your PostgreSQL database, you will need to ensure that both PostgreSQL authentication and Microsoft Entra authentication are enabled. This can be configured in the Azure Portal by navigating to Security -> Authentication. Then, to authorize your managed identity with the PostgreSQL database you will need to run two different commands as two different users. These commands are each covered in their own section below.

#### 1. Create the Managed Identity User as a Microsoft Entra Administrator

*This section is adapted from [the official documentation from Microsoft on connecting to an Azure PostgreSQL database with a managed identity](https://learn.microsoft.com/en-us/azure/postgresql/flexible-server/how-to-connect-with-managed-identity). Please refer to that document for more information.*

1. Add a different managed identity user as a Microsoft Entra Administrator to your database.
    - This identity will be used for managing roles on the server, not connecting to the database. For clarity, this section will refer to this second managed identity as the "entra admin identity".
    - You can add this identity during deployment or through the Azure Portal by navigating to Security -> Authentication.
1. Obtain an access token for the entra admin identity with the `https://ossrdbms-aad.database.windows.net/.default` scope:
    ```bash
    az login --scope https://ossrdbms-aad.database.windows.net/.default
    AZ_ACCESS_TOKEN=$(az account get-access-token --scope https://ossrdbms-aad.database.windows.net/.default | jq --raw-output '.accessToken')
    ```
1. Connect to the PostgreSQL database using the entra admin identity name as the user and the `AZ_ACCESS_TOKEN` as the password, then execute the command to create the user:
    ```bash
    AZ_POSTGRES_NAME=<az-postgres-resource-name>
    ADMIN_IDENTITY_NAME=<entra-admin-identity-name>
    RDDMS_IDENTITY_NAME=<rddms-identity-name>
    PSQL_STMT="SELECT * FROM pgaadauth_create_principal('$RDDMS_IDENTITY_NAME', false, false);"
    az postgres flexible-server execute -n $AZ_POSTGRES_NAME -u $ADMIN_IDENTITY_NAME -p $AZ_ACCESS_TOKEN -q $PSQL_STMT
    ```
    If the statment succeeds, you see output that looks like the following:
    ```sql
    pgaadauth_create_principal
    -----------------------------------
    Created role for "<identity_name>"
    (1 row)
    ```

#### 2. Grant Permissions to the New User as the PostgreSQL Admin User

Connect to the PostgreSQL database as the PostgreSQL admin user and grant the RDDMS identity the `azure_pg_admin` role:
```bash
PGUSER=<az-posgtres-admin-user>
PGPASSWORD=***
PSQL_STMT="GRANT azure_pg_admin TO \"$RDDMS_IDENTITY_NAME\" WITH INHERIT TRUE;"
az postgres flexible-server execute -n $AZ_POSTGRES_NAME -u $PGUSER -p $PGPASSWORD -q $PSQL_STMT
```

If you need connection information, the `PGUSER` can be copied from the Azure Portal by navigating to Settings -> Connect. The password is not accessible through the portal, so if it was not stored elsewhere during deployment you can reset it by navigating to Security -> Authentication. Note that this will cause loss of database access for anyone using this account!

### Manual installation (DEPRECATED)

Install the Reservoir DMS with flux:

1. Edit your values in `values.yaml` files.
2. Follow the next instructions to generate your `values.yaml` file accordingly [osdu-ddms](https://community.opengroup.org/osdu/platform/deployment-and-operations/helm-charts-azure/-/blob/master/osdu-ddms/README.md).
2. Install using Helm:

```shell
NAMESPACE=reservoir-ddms
kubectl create ns ${NAMESPACE}
helm install reservoir . -f values.yaml -n ${NAMESPACE}
```

### Azure DevOps pipeline installation (DEPRECATED)

```bash
export ADO_PROJECT=<ado-project>
export ADO_ORGANIZATION=<ado-organization>
export SERVICE=open-etp-server
az repos create --name $SERVICE --organization https://dev.azure.com/${ADO_ORGANIZATION} --project $ADO_PROJECT -ojson

# clone and sync your repo
az pipelines create \
  --name 'reservoir-open-etp-server'  \
  --repository open-etp-server  \
  --branch master  \
  --repository-type tfsgit  \
  --yaml-path /devops/azure/pipeline.yml  \
  --skip-first-run true \
  -ojson
```

### Single Partition Mode (DEPRECATED)

**/!\ WARNING**: The `single` partition mode is not recommended. Using a managed identity to connect to the database is not supported, so it is considered less secure than the `multi` parition mode.

You also can use an external PostgreSQL database with the `single` parition mode. You can either register new secret in service Key Vault (in the service resources) or you can create your secret and specify the name and key.

Modify the [values.yaml](./chart/values.yaml):

```yaml
postgres:
  dbName: myowndb
  partitionMode: single # osdu|single
  # If partition mode is single, specify postgres secret name:
  # SecretName should be libpq formatted: "host=<host> port=<> dbname=<dbName> user=postgres@<db> password=*** sslmode=require"
  # It can be either kvSecretName or secretName but not both in single mode
  kubernetesSecret:
    name: mysecret
    key: postgres-connection
  #kvSecretName: dp-reservoir-conn
```

To set the single-partition mode on Azure, define the following variables before starting the server (connectivity mode does not have any impact here):

```bash
export RDMS_DATA_PARTITION_MODE=single
export RDMS_DATA_CONNECTIVITY_MODE=osdu
export POSTGRESQL_CONN_STRING="host=open-etp-postgresql port=5432 dbname=postkv user=tester password=***"
```

### Istio and Application Gateway (DEPRECATED)

Helm charts also contain configuration for the envoy filter on the Reservoir DDMS installation namespace. This will not allow requests without authentication or with appropriate audiences [envoyfilter.yaml](./chart/templates/istio/envoyfilter.yaml).
