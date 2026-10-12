<!--- Deploy --->

# Deploy helm chart

## Introduction

This chart installs a deployment on a [Kubernetes](https://kubernetes.io) cluster using [Helm](https://helm.sh) package manager.

## Prerequisites

The code was tested on **Kubernetes cluster** (v1.21.11) with **Istio** (1.12.6)
> It is possible to use other versions, but it hasn't been tested

### Operation system

The code works in Debian-based Linux (Debian 10 and Ubuntu 20.04) and Windows WSL 2. Also, it works but is not guaranteed in Google Cloud Shell. All other operating systems, including macOS, are not verified and supported.

### Packages

Packages are only needed for installation from a local computer.

* **HELM** (version: v3.7.1 or higher) [helm](https://helm.sh/docs/intro/install/)
* **Kubectl** (version: v1.21.0 or higher) [kubectl](https://kubernetes.io/docs/tasks/tools/#kubectl)

## Installation

First you need to set variables in **values.yaml** file using any code editor. Some of the values are prefilled, but you need to specify some values as well. You can find more information about them below.

### Global variables

| Name | Description | Type | Default |Required |
|------|-------------|------|---------|---------|
**global.domain** | your domain for the external endpoint, ex `example.com` | string | - | yes
**global.onPremEnabled** | whether on-prem is enabled | boolean | false | yes
**global.limitsEnabled** | whether CPU and memory limits are enabled | boolean | true | yes
**global.tier** | Only PROD must be used to enable autoscaling | string | "" | no
**global.autoscaling** | enables horizontal pod autoscaling, when tier=PROD | boolean | true | yes

### Configmap variables

| Name | Description | Type | Default |Required |
|------|-------------|------|---------|---------|
**data.entitlementsUrl** | entitlements url | string | `http://entitlements/api/entitlements/v2` | yes
**data.partitionHost** | partition url | string | `http://partition/api/partition/v1` | yes
**data.storageUrl** | storage url | string | `http://storage/api/storage/v2` | yes
**data.cloudProvider** | cloud provider | string | gc | yes
**data.osduHost** | OSDU host | string | `https://example.com` | yes
**data.domainName** | domain part for Entitlements service | string | `.example.test` | yes
**data.rdmsDataConnectivityMode** | RDMS data connectivity mode | string | osdu | yes
**data.rdmsDataPartitionMode** | RDMS data partition mode | string | multiple | yes

### Deployment variables

| Name | Description | Type | Default |Required |
|------|-------------|------|---------|---------|
**data.requestsCpu** | amount of requested CPU | string | 100m | yes
**data.requestsMemory** | amount of requested memory| string | 0.2G | yes
**data.limitsCpu** | CPU limit | string | 1 | only if `global.limitsEnabled` is true
**data.limitsMemory** | memory limit | string | 1G | only if `global.limitsEnabled` is true
**data.cloudSqlProxyVersion** | version of Cloud SQL Proxy | string | `1.32.0` | yes
**data.sqlConnectionString** | string for SQL connection | string | - | yes
**data.image** | service image | string | - | yes
**data.imagePullPolicy** | when to pull image | string | IfNotPresent | yes
**data.serviceAccountName** | name of your service account | string | `oetp-server` | yes

### Configuration variables

| Name | Description | Type | Default |Required |
|------|-------------|------|---------|---------|
**conf.appName** | name of the app | string | `oetp-server` | yes
**conf.configmap** | configmap to be used | string | `oetp-server-config` | yes
**conf.openEtpServerSecretName** | secret for open etp server | string | `oetp-server-secret` | yes

### Horizontal Pod Autoscaling (HPA) variables (works only if tier=PROD and autoscaling=true)

| Name | Description | Type | Default |Required |
|------|-------------|------|---------|---------|
**hpa.minReplicas** | minimum number of replicas | integer | 6 | only if `global.autoscaling` is true and `global.tier` is PROD
**hpa.maxReplicas** | maximum number of replicas | integer | 15 | only if `global.autoscaling` is true and `global.tier` is PROD
**hpa.targetType** | type of measurements: AverageValue or Value | string | "AverageValue" | only if `global.autoscaling` is true and `global.tier` is PROD
**hpa.targetValue** | threshold value to trigger the scaling up | integer | 80 | only if `global.autoscaling` is true and `global.tier` is PROD
**hpa.behaviorScaleUpStabilizationWindowSeconds** | time to start implementing the scale up when it is triggered | integer | 10 | only if `global.autoscaling` is true and `global.tier` is PROD
**hpa.behaviorScaleUpPoliciesValue** | the maximum number of new replicas to create (in percents from current state)| integer | 50 | only if `global.autoscaling` is true and `global.tier` is PROD
**hpa.behaviorScaleUpPoliciesPeriodSeconds** | pause for every new scale up decision | integer | 15 | only if `global.autoscaling` is true and `global.tier` is PROD
**hpa.behaviorScaleDownStabilizationWindowSeconds** | time to start implementing the scale down when it is triggered | integer | 60 | only if `global.autoscaling` is true and `global.tier` is PROD
**hpa.behaviorScaleDownPoliciesValue** | the maximum number of replicas to destroy (in percents from current state) | integer | 25 | only if `global.autoscaling` is true and `global.tier` is PROD
**hpa.behaviorScaleDownPoliciesPeriodSeconds** | pause for every new scale down decision | integer | 60 | only if `global.autoscaling` is true and `global.tier` is PROD

### Limits variables

| Name | Description | Type | Default |Required |
|------|-------------|------|---------|---------|
**limits.maxTokens** | maximum number of requests per fillInterval | integer | 50 | only if `global.autoscaling` is true and `global.tier` is PROD
**limits.tokensPerFill** | number of new tokens allowed every fillInterval | integer | 50 | only if `global.autoscaling` is true and `global.tier` is PROD
**limits.fillInterval** | time interval | string | "1s" | only if `global.autoscaling` is true and `global.tier` is PROD

### Install the helm chart

Run this command from within this directory:

```console
helm install gc-oetp-server-deploy .
```

## Uninstalling the Chart

To uninstall the helm deployment:

```console
helm uninstall gc-oetp-server-deploy
```

[Move-to-Top](#deploy-helm-chart)
