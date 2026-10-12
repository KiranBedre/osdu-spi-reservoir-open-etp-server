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
**global.tier** | tier defines the number of replicas for the service to ensure the service HA; values are `DEV`, `STAGE`, `PROD` | string | "" | no |
**global.autoscalingMode** | enables horizontal pod autoscaling on cluster spot nodes; values are `none`, `cpu`, `requests` | string | `cpu` | yes |

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
**data.logLevel** | logging severity level for this service only | string | - | yes, only if differs from the `global.logLevel` |

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

### Horizontal Pod Autoscaling (HPA) variables

| Name | Description | Type | Default |Required |
|------|-------------|------|---------|---------|
| **hpa.minReplicas** | minimum number of replicas | integer | 6 | only if `global.autoscaling` is true and `global.tier` is PROD |
| **hpa.maxReplicas** | maximum number of replicas | integer | 15 | only if `global.autoscaling` is true and `global.tier` is PROD |
| **hpa.targetType** | type of measurements: AverageValue or Value | string | "AverageValue" | only if `global.autoscaling` is true and `global.tier` is PROD |
| **hpa.targetValue** | threshold value to trigger the scaling up | integer | 140 | only if `global.autoscaling` is true and `global.tier` is PROD |
| **hpa.behaviorScaleUpStabilizationWindowSeconds** | time to start implementing the scale up when it is triggered | integer | 10 | only if `global.autoscaling` is true and `global.tier` is PROD |
| **hpa.behaviorScaleUpPoliciesValue** | the maximum number of new replicas to create (in percents from current state)| integer | 50 | only if `global.autoscaling` is true and `global.tier` is PROD |
| **hpa.behaviorScaleUpPoliciesPeriodSeconds** | pause for every new scale up decision | integer | 15 | only if `global.autoscaling` is true and `global.tier` is PROD |
| **hpa.behaviorScaleDownStabilizationWindowSeconds** | time to start implementing the scale down when it is triggered | integer | 60 | only if `global.autoscaling` is true and `global.tier` is PROD |
| **hpa.behaviorScaleDownPoliciesValue** | the maximum number of replicas to destroy (in percents from current state) | integer | 25 | only if `global.autoscaling` is true and `global.tier` is PROD |
| **hpa.behaviorScaleDownPoliciesPeriodSeconds** | pause for every new scale down decision | integer | 60 | only if `global.autoscaling` is true and `global.tier` is PROD |

### Limits variables

| Name                     | Description                                     | Type    | Default | Required                                       |
|--------------------------|-------------------------------------------------|---------|---------|------------------------------------------------|
| **limits.maxTokens**     | maximum number of requests per fillInterval     | integer | `12`    | only if `global.autoscalingMode` is `requests` |
| **limits.tokensPerFill** | number of new tokens allowed every fillInterval | integer | `12`    | only if `global.autoscalingMode` is `requests` |
| **limits.fillInterval**  | time interval                                   | string  | `"1s"`  | only if `global.autoscalingMode` is `requests` |

### Autoscaling

By default, autoscaling is configured for deployments targeting spot nodes. Pods will attempt to schedule on nodes with specific labels indicating they are spot instances. To adjust how pods are scheduled, you can update the `data.affinityLabelsSpot` in your values.yaml file.

Example:

```yml
data:
  affinityLabelsSpot:
    mylabel:
      - value1
      - test
    newLabel:
      - newValue
```

Each label, along with its values, will be translated into a separate `- matchExpressions` block within the `nodeAffinity` section of your deployment. This configuration operates with OR logic, meaning pods will be scheduled on any node that possesses at least one of the specified labels with one of its defined values.

The chart uses the `global.autoscalingMode` parameter in your `values.yaml` to control how autoscaling behaves. This parameter accepts three possible string values:

* **cpu** (default): Autoscaling is enabled and is based on CPU utilization. This is the default setting.
* **requests**: Autoscaling is enabled and is based on resource requests (custom metrics). **NOTE**: Prometheus should be installed in your cluster, custom metrics are used for this type of autoscaling.
* **none**: Autoscaling is entirely disabled for the application. Setting `global.autoscalingMode` to **none** also prevents the creation of the spot deployment.

The `global.tier` parameter controls the number of replicas based on the environment:

* **DEV**: 1-5 replicas
* **STAGE**: 2-7 replicas  
* **PROD**: 3-10 replicas
* **"" (empty)**: Uses `hpa.minReplicas` and `hpa.maxReplicas` values

### Methodology for Parameter Calculation variables: **hpa.requests.targetValue**, **limits.maxTokens** and **limits.tokensPerFill**

The parameters **hpa.requests.targetValue**, **limits.maxTokens** and **limits.tokensPerFill** were determined through empirical testing during load testing. These tests were conducted using the N2D machine series, which can run on either AMD EPYC Milan or AMD EPYC Rome processors. The values were fine-tuned to ensure optimal performance under typical workloads.

### Recommendations for New Instance Types

When changing the instance type to a newer generation, such as the C3D series, it is essential to conduct new load testing. This ensures the parameters are recalibrated to match the performance characteristics of the new processor architecture, optimizing resource utilization and maintaining application stability.

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
