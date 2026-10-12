#!/usr/bin/env bash

set -euo pipefail

docker build \
  --file devops/azure/Dockerfile \
  --target build-env \
  --tag "open-etp-build:${GITHUB_SHA:-local}" \
  .
