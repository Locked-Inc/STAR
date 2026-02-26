//! Protocol Buffers documentation support.
//!
//! Parses .proto files directly (they're structured enough to handle
//! without protoc-gen-doc) or consumes protoc-gen-doc JSON output.

use std::path::Path;

use serde::Deserialize;

/// protoc-gen-doc JSON output structure.
#[derive(Debug, Deserialize)]
pub struct ProtoDocOutput {
    #[serde(default)]
    pub files: Vec<ProtoFile>,
}

/// A single .proto file's documentation.
#[derive(Debug, Deserialize)]
pub struct ProtoFile {
    pub name: String,
    pub description: String,
    #[serde(rename = "package")]
    pub package_name: String,
    #[serde(default)]
    pub messages: Vec<ProtoMessage>,
    #[serde(default)]
    pub enums: Vec<ProtoEnum>,
    #[serde(default)]
    pub services: Vec<ProtoService>,
}

/// A protobuf message.
#[derive(Debug, Deserialize)]
pub struct ProtoMessage {
    pub name: String,
    #[serde(rename = "longName", default)]
    pub long_name: String,
    #[serde(rename = "fullName", default)]
    pub full_name: String,
    pub description: String,
    #[serde(default)]
    pub fields: Vec<ProtoField>,
}

/// A protobuf field.
#[derive(Debug, Deserialize)]
pub struct ProtoField {
    pub name: String,
    pub description: String,
    #[serde(rename = "type")]
    pub field_type: String,
    #[serde(rename = "longType", default)]
    pub long_type: String,
    #[serde(rename = "fullType", default)]
    pub full_type: String,
    #[serde(default)]
    pub label: String,
}

/// A protobuf enum.
#[derive(Debug, Deserialize)]
pub struct ProtoEnum {
    pub name: String,
    pub description: String,
    #[serde(default)]
    pub values: Vec<ProtoEnumValue>,
}

/// A protobuf enum value.
#[derive(Debug, Deserialize)]
pub struct ProtoEnumValue {
    pub name: String,
    pub number: String,
    pub description: String,
}

/// A protobuf service.
#[derive(Debug, Deserialize)]
pub struct ProtoService {
    pub name: String,
    pub description: String,
    #[serde(default)]
    pub methods: Vec<ProtoMethod>,
}

/// A protobuf service method.
#[derive(Debug, Deserialize)]
pub struct ProtoMethod {
    pub name: String,
    pub description: String,
    #[serde(rename = "requestType")]
    pub request_type: String,
    #[serde(rename = "responseType")]
    pub response_type: String,
}

/// Parse protoc-gen-doc JSON output.
pub fn parse_protodoc_json(path: &Path) -> anyhow::Result<ProtoDocOutput> {
    let content = std::fs::read_to_string(path)?;
    let output: ProtoDocOutput = serde_json::from_str(&content)?;
    tracing::info!("Parsed {} proto files", output.files.len());
    Ok(output)
}

/// Emit Typst documentation for protobuf schemas.
pub fn emit_protobuf_chapter(doc: &ProtoDocOutput) -> String {
    let mut out = String::new();

    for file in &doc.files {
        out.push_str(&format!(
            "== {} (`{}`)\n\n",
            file.package_name, file.name
        ));

        if !file.description.is_empty() {
            out.push_str(&file.description);
            out.push_str("\n\n");
        }

        // Messages
        for msg in &file.messages {
            out.push_str(&format!("=== {}\n\n", msg.name));
            if !msg.description.is_empty() {
                out.push_str(&msg.description);
                out.push_str("\n\n");
            }

            if !msg.fields.is_empty() {
                out.push_str("#table(\n");
                out.push_str("  columns: (auto, auto, auto, 1fr),\n");
                out.push_str("  stroke: 0.5pt + rgb(\"#e0e0e0\"),\n");
                out.push_str("  inset: 6pt,\n");
                out.push_str(
                    "  fill: (_, y) => if y == 0 { rgb(\"#e3f2fd\") } else { none },\n",
                );
                out.push_str("  [*Field*], [*Type*], [*Label*], [*Description*],\n");
                for field in &msg.fields {
                    out.push_str(&format!(
                        "  [`{}`], [`{}`], [{}], [{}],\n",
                        field.name, field.field_type, field.label, field.description,
                    ));
                }
                out.push_str(")\n\n");
            }
        }

        // Enums
        for en in &file.enums {
            out.push_str(&format!("=== {}\n\n", en.name));
            if !en.description.is_empty() {
                out.push_str(&en.description);
                out.push_str("\n\n");
            }
            if !en.values.is_empty() {
                out.push_str("#table(\n");
                out.push_str("  columns: (auto, auto, 1fr),\n");
                out.push_str("  stroke: 0.5pt + rgb(\"#e0e0e0\"),\n");
                out.push_str("  inset: 6pt,\n");
                out.push_str(
                    "  fill: (_, y) => if y == 0 { rgb(\"#e3f2fd\") } else { none },\n",
                );
                out.push_str("  [*Name*], [*Number*], [*Description*],\n");
                for val in &en.values {
                    out.push_str(&format!(
                        "  [`{}`], [{}], [{}],\n",
                        val.name, val.number, val.description,
                    ));
                }
                out.push_str(")\n\n");
            }
        }

        // Services
        for svc in &file.services {
            out.push_str(&format!("=== Service: {}\n\n", svc.name));
            if !svc.description.is_empty() {
                out.push_str(&svc.description);
                out.push_str("\n\n");
            }
            for method in &svc.methods {
                out.push_str(&format!("==== {}\n\n", method.name));
                out.push_str(&format!(
                    "`{}` -> `{}`\n\n",
                    method.request_type, method.response_type
                ));
                if !method.description.is_empty() {
                    out.push_str(&method.description);
                    out.push_str("\n\n");
                }
            }
        }
    }

    out
}
