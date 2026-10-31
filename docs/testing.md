# End-to-end Testing

## CIMPL Acceptance Smoke Test

Run the deployed-service smoke test from the repository checkout:

```bash
export BASE_URL=https://your-osdu-instance.example
export DATA_PARTITION_ID=osdu
# Supply AUTHORIZATION securely as a Bearer authorization value.
bash devops/scripts/run_acceptance.sh
```

Only `BASE_URL`, `DATA_PARTITION_ID`, and `AUTHORIZATION` are runtime test
inputs. The base URL is the HTTPS instance origin, optionally with a port,
not the ETP endpoint path. The script locates the checked-out Volve Horizons
EPC/HDF5 fixture relative to itself. Bash, curl, jq and the `openETPServer`
executable must be available on PATH. Use only an approved test instance.

### Fixed Test Metadata Exception

For open-etp-server hardening under issue #277, the service owner requested
fixed test metadata instead of metadata discovery on 2026-10-15:

- Legal tag: `osdu-demo-legaltag`.
- Data groups: `data.default.owners@osdu.group` and
    `data.default.viewers@osdu.group`.
- Relevant country: `US`.

This is a service-specific portability exception, not proof that these values
work on every OSDU instance. Record the exception in subsequent DoD assessments;
the generic configuration criterion still requires an owner-recorded disposition.
Other partitions/domains require equivalent approved fixture setup or a reviewed
change to the fixed values, not extra undocumented runtime inputs.

The script checks the legal tag through the Legal API and creates it only on
HTTP 404, using the existing CIMPL legal bootstrap definition: Public Domain
Data, OSDU originator, US origin, Public security classification, EAR99,
No Personal Data, No Contract Related, and expiration `2099-01-01`.
An existing tag is not modified. A concurrent create conflict is followed by
another read; other errors fail setup. The tag is a shared test prerequisite
and is retained, including when first provisioned by this script. The no-residual
data claim applies to run-owned dataspaces, not this explicitly retained fixture.

The identity must have the deployed service's reservoir create/read/delete
permissions, membership of the fixed data owner group, and Legal tag read access
(plus Legal tag creation access if the fixture is absent). Relevant service roles
are `service.reservoir-dms.owners`/`service.reservoir-dms.viewers`, with the
partition/domain suffix configured by the instance. Provision the fixture in
advance for identities without creation rights; the test does not grant roles
or modify ACL membership.

### Smoke Parity And Cleanup

| Scenario | Azure CLI job | GC CLI job | CIMPL acceptance |
|---|---|---|---|
| ETP ping | Yes | Yes | `ETP-PING` |
| Initial dataspace listing | Yes | Yes | `SPACE-LIST-BEFORE` |
| Dataspace creation | Yes | Yes | `SPACE-CREATE`, unique test name |
| Volve Horizons EPC import | Yes | Yes | `SPACE-IMPORT` |
| Final dataspace listing | Yes | Yes | `SPACE-LIST-AFTER`, checks exact owned name |
| Final dataspace deletion | Yes | No | `SPACE-DELETE`, verifies absence |

This table compares the CLI smoke flows in `devops/azure/azure.gitlab-ci.yml`
and `devops/gc/pipeline/override-stages.yml`. It does not claim parity with
uninspected inherited suites or every C++ test. The existing CTest and local
server E2E tests remain in `compile-and-unit-test` unchanged.

Each attempt creates a unique `test/acceptance-*` dataspace; it never deletes
shared `demo/Volve` or another run's data. An EXIT trap deletes its dataspace
on success, command failure, or handled INT/TERM. Cleanup failure returns a
nonzero exit status, and the job prints the exact owned name for recovery.
SIGKILL, runner loss, expired credentials, or an unavailable server may prevent
cleanup; verify the reported name on the approved instance before deleting it.

### CI Results And DoD Evidence

`cimpl-dev-acceptance-test`, `cimpl-dev-backup-acceptance-test`, and
`cimpl-prod-acceptance-test` use the shared ci-cd-pipelines acceptance-stage
rules and wait for their deployment and the SHA-tagged runtime client image.
CI acquires the token outside the portable script. Duplicate integration and
Python test jobs are disabled. Job retry is zero; failed attempts are not hidden.

The script writes `tests/acceptance/results/junit.xml` and
`tests/acceptance/results/test-results.log`. Reports include setup and cleanup;
unexecuted steps are skipped, not passed. The job log and results log include
failed-step CLI output, setup validation errors, Legal API HTTP status codes
and the final exit status. Authorization values, the bearer token and the CI
client secret are redacted before captured output is printed or persisted.
Raw output remains temporary and is deleted; JUnit uses fixed failure messages.
Shell tracing stays disabled. These diagnostics do not replace server-side logs.

Issue #277 targets 100% successful completed acceptance attempts; epic 52's
minimum is 80%. Measure the latest 15 completed pipelines containing the new
acceptance job, recording pipeline/job IDs and commit SHA. Report attempt,
first-attempt and final-pipeline rates separately; exclude skipped/canceled
attempts from completed-run pass rate and report them separately. Do not mix
historical integration-job results into this new acceptance baseline. A green
lint or offline shell check does not establish live pass rate or zero flakes.

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
