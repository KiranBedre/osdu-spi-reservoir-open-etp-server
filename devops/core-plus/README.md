## Partition Service Values

You must add the following value to Partition service:

```json
"reservoir-connection": {
        "sensitive": true,
        "value": "<THE_ENV_VARIABLE_IN_DEPLOY>"
    }
```

Since the `reservoir-connection` is a sensitive value we can't store its value in Partition service directly. So, we just save the **environment variable's name** that is defined in ETP Server deploy.

<a name="conn-string-example"></a>
### Example

Lets say, Partition service value for the `osdu-main` data partition id could look like the following:
```json

"reservoir-connection": {
        "sensitive": true,
        "value": "OSDU_MAIN_PSQL_CONN_STRING" # the value is arbitrary
    }
```

Then, we need to specify the following environment variable (secret):
```bash
OSDU_MAIN_PSQL_CONN_STRING="host=<psqlhost> port=<port> dbname=<dbname> user=<user> password=<password>"
```

## Environment variables

### Secrets
We should specify a new PSQL connection string variable for each data partition. The name can be arbitrary, the only requirement is that the variable name should be specified in Partition service for the corresponding data-partition-id.  

See how it should look like [here](#conn-string-example).


### Environmental Variables

```bash
CLOUD_PROVIDER: gc 
DOMAIN_NAME: group
ENTITLEMENTS_URL: <entitlements_host>/api/entitlements/v2
OSDU_HOST: <osdu_host> # seems that this variable is needed only for Swagger Generated Docs
PARTITION_URL: <partition_host>/api/partition/v1
RDMS_DATA_CONNECTIVITY_MODE: osdu
RDMS_DATA_PARTITION_MODE: multiple
STORAGE_URL: <storage_host>/api/storage/v2
```
