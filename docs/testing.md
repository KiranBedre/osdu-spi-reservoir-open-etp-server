# End-to-end Testing

If the [test environment](#creating-test-environment) is created on the partition,  [download the Postman collection](#testing-openetpclient) and run it.

## Creating Test Environment

After the [Reservoir DDMS deployment](../devops/azure/README.md), the test environment should be created. The test environment is a data space with imported RESQML test data.

This instruction provides the commands for openETPserver.

### Prerequisites

1. Copy the `data` [folder from the Community repository](https://community.opengroup.org/osdu/platform/domain-data-mgmt-services/reservoir/open-etp-server/-/tree/main/data) to your local machine.

2. You need an ETP client to connect to a remote server via WSS.

    * Download a prebuild client from the OSDU GitLab docker container registry.
        ```bash
        export CLIENT_IMAGE=community.opengroup.org:5555/osdu/platform/domain-data-mgmt-services/reservoir/open-etp-server/open-etp-server-main

        docker pull ${CLIENT_IMAGE}
        docker tag ${CLIENT_IMAGE} open-etp:client
        ```

4. Get a valid access token.

5. Set the following variables:

    * RDDMS_URL
    * PARTITION
    * TOKEN

> Note: At the moment, authentication is [implemented for Azure only](../README.md#authentication-azure-only).

### Steps to create the test environment

1. Create the demo data space:

```bash
docker run -it --rm open-etp:client openETPServer space -S wss://${RDDMS_URL} --new -s demo/Volve --data-partition-id ${PARTITION} --auth bearer --jwt-token ${TOKEN}
```

2. Make sure the data space is created by getting the data spaces list:

```bash
docker run -it --rm open-etp:client openETPServer space -S wss://${RDDMS_URL} -l --data-partition-id ${PARTITION} --auth bearer --jwt-token ${TOKEN}
```

3. Import the RESQML data to the data space:

```bash
docker run -it --rm -v ~/reservoir/open-etp-server/data:/data open-etp:client openETPServer space -S wss://${RDDMS_URL} -s demo/Volve --import-epc ./data/Volve_Demo_Faults_Depth.epc --data-partition-id ${PARTITION} --auth bearer --jwt-token ${TOKEN}

docker run -it --rm -v ~/reservoir/open-etp-server/data:/data open-etp:client openETPServer space -S wss://${RDDMS_URL} -s demo/Volve --import-epc ./data/Volve_Demo_Horizons_Depth.epc --data-partition-id ${PARTITION} --auth bearer --jwt-token ${TOKEN}

docker run -it --rm -v ~/reservoir/open-etp-server/data:/data open-etp:client openETPServer space -S wss://${RDDMS_URL} -s demo/Volve --import-epc ./data/Volve_Demo_Reservoir_Grid_Depth.epc --data-partition-id ${PARTITION} --auth bearer --jwt-token ${TOKEN}

docker run -it --rm -v ~/reservoir/open-etp-server/data:/data open-etp:client openETPServer space -S wss://${RDDMS_URL} -s demo/Volve --import-epc ./data/Volve_Demo_Wells_Depth.epc --data-partition-id ${PARTITION} --auth bearer --jwt-token ${TOKEN}
```

> Importing takes some time and you may need to refresh your token.

4. Access data to ensure the data is imported correctly:

```bash
docker run -it --rm open-etp:client openETPServer space -S wss://${RDDMS_URL} -s demo/Volve --stats --data-partition-id ${PARTITION} --auth bearer --jwt-token ${TOKEN}
```

Remove space if you need to recreate the test environment:

```bash
docker run -it --rm open-etp:client openETPServer space --delete -S wss://${RDDMS_URL} -s demo/Volve --data-partition-id ${PARTITION} --auth bearer --jwt-token ${TOKEN}
```

## Testing openETPclient

Download the [Postman environment](https://community.opengroup.org/osdu/platform/testing/-/tree/master/Postman%20Collection/00_CICD_Setup_Environment) and the [collection](https://community.opengroup.org/osdu/platform/testing/-/tree/master/Postman%20Collection/45_CICD_Reservoir_DDMS) to test the Reservoir DDMS via openETPclient.
