{{/*
Common Annotations
*/}}
{{- define "reservoir-ddms.metricAnnotations" -}}
prometheus.io/path: /metrics
prometheus.io/port: "9002"
prometheus.io/scrape: "true"
{{- end }}
{{/*
Common Labels
*/}}
{{- define "reservoir-ddms.commonLabels" -}}
osduapp: reservoir-ddms
{{/* 
include "reservoir-ddms.deploymentTypeLabels" . 
*/}}
{{- end }}

{{/*
 Renders the namespace. 
*/}}
{{- define "reservoir-ddms.namespace" -}}
namespace: {{.Release.Namespace}}
{{- end }}

{{/*
 Renders the pathPrefix and suffix if there is any 
*/}}
{{- define "reservoir-ddms.prefix" -}}
{{ .Values.ingress.hosts.pathPrefix }}{{ include "reservoir-ddms.name-suffix" . }}
{{- end }}


{{/*
Define the postgres secret connection mode
*/}}

{{- define "postgres.connectionString" -}}
{{- if eq .Values.postgres.partitionMode "single" -}}
- name: POSTGRESQL_CONN_STRING
  valueFrom:
    secretKeyRef:
    {{- if .Values.postgres.kubernetesSecret }}
      name: {{ .Values.postgres.kubernetesSecret.name }}
      key: {{ .Values.postgres.kubernetesSecret.key }}
    {{- else if .Values.postgres.kvSecretName }}
      name: {{ .Release.Name }}-postgres-connection
      key: postgres-conn       
    {{- end }}
{{- end -}}
{{- end -}}