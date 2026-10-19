#!/usr/bin/env python3

import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RESULTS = ROOT / ".spi" / "results"
REPORT = RESULTS / "descriptor-validation.json"
MAX_GITHUB_BLOB_SIZE = 100 * 1024 * 1024


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def require_publishable_git_snapshot() -> None:
    index = subprocess.run(
        ["git", "ls-files", "--stage", "-z"],
        cwd=ROOT,
        check=True,
        capture_output=True,
    ).stdout.split(b"\0")
    index_paths = {}
    index_ids = []
    for entry in index:
        if not entry:
            continue
        metadata, raw_path = entry.split(b"\t", 1)
        object_id = metadata.split()[1].decode("ascii")
        relative_path = raw_path.decode("utf-8", errors="surrogateescape")
        index_ids.append(object_id)
        index_paths.setdefault(object_id, relative_path)
    index_info = subprocess.run(
        ["git", "cat-file", "--batch-check=%(objecttype) %(objectname) %(objectsize)"],
        cwd=ROOT,
        check=True,
        capture_output=True,
        input="\n".join(index_ids) + "\n",
        text=True,
    ).stdout
    oversized_tracked = []
    for line in index_info.splitlines():
        object_type, object_id, size = line.split()
        if object_type == "blob" and int(size) > MAX_GITHUB_BLOB_SIZE:
            oversized_tracked.append(f"{index_paths[object_id]} ({size} bytes)")
    require(
        not oversized_tracked,
        "tracked files exceed GitHub's 100 MiB limit: " + ", ".join(oversized_tracked),
    )

    head = subprocess.run(
        ["git", "rev-parse", "--verify", "HEAD"],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )
    if head.returncode != 0:
        return

    objects_output = subprocess.run(
        ["git", "rev-list", "--objects", "HEAD"],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    object_paths = {}
    object_ids = []
    for line in objects_output.splitlines():
        object_id, _, path = line.partition(" ")
        object_ids.append(object_id)
        if path:
            object_paths.setdefault(object_id, path)
    object_info = subprocess.run(
        ["git", "cat-file", "--batch-check=%(objecttype) %(objectname) %(objectsize)"],
        cwd=ROOT,
        check=True,
        capture_output=True,
        input="\n".join(object_ids) + "\n",
        text=True,
    ).stdout
    oversized_history = []
    for line in object_info.splitlines():
        object_type, object_id, size = line.split()
        if object_type == "blob" and int(size) > MAX_GITHUB_BLOB_SIZE:
            path = object_paths.get(object_id, "<historical path unavailable>")
            oversized_history.append(f"{path} ({size} bytes, {object_id})")
    require(
        not oversized_history,
        "reachable Git history exceeds GitHub's 100 MiB limit: " + ", ".join(oversized_history),
    )


def main() -> int:
    require_publishable_git_snapshot()

    RESULTS.mkdir(parents=True, exist_ok=True)
    try:
        subprocess.run(
            [
                sys.executable,
                str(ROOT / ".github/actions/acceptance-resolver/resolve.py"),
                "--contract-only",
                "--descriptor",
                str(ROOT / ".spi/service.yaml"),
                "--report",
                str(REPORT),
            ],
            cwd=ROOT,
            check=True,
        )
        report = json.loads(REPORT.read_text(encoding="utf-8"))
    finally:
        REPORT.unlink(missing_ok=True)

    service = report["service_config"]
    contract = report["contract"]
    require(service["name"] == "open-etp", "descriptor service name changed")
    require(
        service["archetype"] == "native-container-azure",
        "native container archetype did not resolve",
    )
    require(
        service["image"]["dockerfile"] == "devops/azure/Dockerfile",
        "service image Dockerfile did not resolve",
    )
    require(
        set(contract["dependencies"]) == {"partition", "entitlements", "legal", "storage"},
        "acceptance dependencies changed",
    )
    require(
        contract["requires"]["groups"] == ["service.reservoir-dms.owners"],
        "required Reservoir DDMS owner group changed",
    )
    require(
        contract["report_paths"] == ["results/open-etp.xml"],
        "JUnit report path changed",
    )

    script_path = ROOT / "devops/azure/tasks/etp-ssl-client-integration-tests.sh"
    subprocess.run(
        ["bash", "-n", "devops/azure/tasks/etp-ssl-client-integration-tests.sh"],
        cwd=ROOT,
        check=True,
    )
    subprocess.run(
        ["bash", "-n", "devops/scripts/fetch-test-fixtures.sh"],
        cwd=ROOT,
        check=True,
    )
    script = script_path.read_text(encoding="utf-8")
    require("docker run" not in script, "acceptance suite must not use Docker-in-Docker")
    require('"demo/Volve"' not in script, "acceptance suite must not persist demo/Volve")
    require(script.count('run_test "') == 18, "acceptance suite must declare 18 scenarios")
    require("trap finish EXIT INT TERM" in script, "acceptance suite must clean up on failure")
    require("open-etp.xml" in script, "acceptance suite must emit JUnit XML")

    dockerfile = (ROOT / "devops/azure/Dockerfile").read_text(encoding="utf-8")
    stages = re.findall(r"^FROM\s+\S+\s+AS\s+(\S+)", dockerfile, re.MULTILINE)
    require(
        stages == ["build-env", "acceptance-fixtures", "runtime-base", "acceptance", "runtime"],
        "Docker stages changed",
    )
    require("USER 1000:1000" in dockerfile, "runtime must use Kubernetes UID 1000")
    require(
        re.search(r'ARG VCPKG_VERSION="[0-9a-f]{40}"', dockerfile) is not None,
        "vcpkg must be pinned to a full commit",
    )
    require(
        "https://github.com/microsoft/vcpkg.git" in dockerfile,
        "vcpkg must come from the public Microsoft repository",
    )
    dockerignore = (ROOT / ".dockerignore").read_text(encoding="utf-8")
    require(
        "!/devops/azure/tasks/etp-ssl-client-integration-tests.sh" in dockerignore
        and "!/devops/scripts/**" in dockerignore,
        "acceptance script or fixture fetcher is excluded from the Docker context",
    )
    require(
        "COPY --from=acceptance-fixtures" in dockerfile,
        "acceptance image must use integrity-verified downloaded fixtures",
    )
    require(
        "WORKDIR /suite/devops/azure/tasks" in dockerfile,
        "acceptance image must place the suite under the SPI paired-image root",
    )
    require(
        "/suite/data/IntegrationTestsData/" in dockerfile,
        "acceptance fixtures must remain relative to the SPI suite root",
    )

    fixture_fetcher = ROOT / "devops/scripts/fetch-test-fixtures.sh"
    fixture_script = fixture_fetcher.read_text(encoding="utf-8")
    require(
        "b639f8d549d505ec92d681c53c55e3769071bb5d" in fixture_script,
        "fixture source is not pinned to the public OSDU provenance commit",
    )
    require(
        "06ab5eb57d772770260be9d8e1cdddc391108b60c512c43347499cdd4eefd9e1"
        in fixture_script
        and "e0be9b03c272bf9b5b789581a30aec4c8caad0b045ba70633da93f0bab3ea725"
        in fixture_script,
        "oversized fixture SHA-256 pins are missing",
    )
    require(
        "sha256sum --check --status" in fixture_script,
        "downloaded fixture integrity is not enforced",
    )
    require(
        not (ROOT / "data/Demo_Drogon.h5").exists()
        and not (ROOT / "data/Volve_Demo_Reservoir_Grid_Depth.h5").exists(),
        "oversized HDF5 fixtures remain in the published snapshot",
    )

    build_paths = [
        ROOT / "devops/azure/Dockerfile",
        fixture_fetcher,
        ROOT / ".spi",
        ROOT / ".github/actions/cpp-build",
        ROOT / ".github/actions/acceptance-image",
        ROOT / ".github/workflows/build.yml",
        ROOT / ".github/workflows/validate.yml",
    ]
    private_pattern = re.compile(
        r"dev\." r"azure\.com|ADO_" r"ACCESS_TOKEN",
        re.IGNORECASE,
    )
    for path in build_paths:
        files = [path] if path.is_file() else [item for item in path.rglob("*") if item.is_file()]
        for item in files:
            require(
                private_pattern.search(item.read_text(encoding="utf-8", errors="ignore")) is None,
                f"private ADO reference remains in build path: {item.relative_to(ROOT)}",
            )

    tags = (ROOT / ".github/actions/docker-build/compute-tags.sh").read_text(encoding="utf-8")
    require(":sha-${SHORT_SHA}" in tags, "immutable SHA image tag convention missing")
    require("main|fork_integration|fork_upstream" in tags, "snapshot tag convention missing")
    acceptance = (ROOT / ".github/actions/acceptance-image/action.yml").read_text(encoding="utf-8")
    require("IMAGE_NAME: ${{ inputs.image_name }}-acceptance" in acceptance, "paired image name changed")
    require("'acceptance'" in acceptance, "C++ acceptance Docker target is not selected")

    schema = json.loads(
        (ROOT / ".github/actions/acceptance-resolver/service-descriptor.schema.json").read_text(
            encoding="utf-8"
        )
    )
    archetypes = schema["properties"]["service"]["properties"]["archetype"]["enum"]
    require(
        "native-container-azure" in archetypes,
        "published schema omits native container archetype",
    )

    provenance = json.loads((ROOT / "commits.json").read_text(encoding="utf-8"))
    require(
        provenance["last_osdu_commit"] == "b639f8d549d505ec92d681c53c55e3769071bb5d",
        "public OSDU base provenance changed",
    )
    require(
        provenance["last_adme_commit"] == "de43beec11df92982db17f62b2c25fadd6b09b2e",
        "ADO overlay provenance changed",
    )

    print("SPI C++ fork validation passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
