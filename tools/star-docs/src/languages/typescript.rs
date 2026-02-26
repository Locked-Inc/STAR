//! TypeScript documentation support via TypeDoc JSON output.

use serde::Deserialize;
use std::path::Path;

/// Simplified TypeDoc JSON structure.
#[derive(Debug, Deserialize)]
pub struct TypeDocOutput {
    pub name: String,
    #[serde(default)]
    pub children: Vec<TypeDocNode>,
}

/// A node in the TypeDoc JSON tree.
#[derive(Debug, Deserialize)]
pub struct TypeDocNode {
    pub name: String,
    #[serde(rename = "kindString", default)]
    pub kind_string: String,
    #[serde(default)]
    pub comment: Option<TypeDocComment>,
    #[serde(default)]
    pub children: Vec<TypeDocNode>,
    #[serde(default)]
    pub signatures: Vec<TypeDocSignature>,
}

/// TypeDoc comment structure.
#[derive(Debug, Deserialize)]
pub struct TypeDocComment {
    #[serde(rename = "shortText", default)]
    pub short_text: String,
    #[serde(default)]
    pub text: String,
}

/// TypeDoc function signature.
#[derive(Debug, Deserialize)]
pub struct TypeDocSignature {
    pub name: String,
    #[serde(default)]
    pub comment: Option<TypeDocComment>,
    #[serde(default)]
    pub parameters: Vec<TypeDocParam>,
    #[serde(rename = "type", default)]
    pub return_type: Option<TypeDocType>,
}

/// TypeDoc parameter.
#[derive(Debug, Deserialize)]
pub struct TypeDocParam {
    pub name: String,
    #[serde(default)]
    pub comment: Option<TypeDocComment>,
    #[serde(rename = "type", default)]
    pub param_type: Option<TypeDocType>,
}

/// TypeDoc type reference.
#[derive(Debug, Deserialize)]
pub struct TypeDocType {
    #[serde(default)]
    pub name: String,
    #[serde(rename = "type", default)]
    pub kind: String,
}

/// Parse TypeDoc JSON output.
pub fn parse_typedoc_json(path: &Path) -> anyhow::Result<TypeDocOutput> {
    let content = std::fs::read_to_string(path)
        .map_err(|e| anyhow::anyhow!("Failed to read TypeDoc JSON {}: {}", path.display(), e))?;
    let output: TypeDocOutput = serde_json::from_str(&content)?;
    tracing::info!("Parsed TypeDoc output: {}", output.name);
    Ok(output)
}

/// Emit Typst documentation for TypeScript modules.
pub fn emit_typescript_chapter(doc: &TypeDocOutput) -> String {
    let mut out = String::new();

    out.push_str(&format!("= {} API Reference\n\n", doc.name));

    for child in &doc.children {
        emit_typedoc_node(child, 2, &mut out);
    }

    out
}

fn emit_typedoc_node(node: &TypeDocNode, level: usize, out: &mut String) {
    let heading = "=".repeat(level);
    out.push_str(&format!("{} {}\n\n", heading, node.name));

    if let Some(comment) = &node.comment {
        if !comment.short_text.is_empty() {
            out.push_str(&comment.short_text);
            out.push_str("\n\n");
        }
        if !comment.text.is_empty() {
            out.push_str(&comment.text);
            out.push_str("\n\n");
        }
    }

    // Function signatures
    for sig in &node.signatures {
        out.push_str(&format!("```typescript\n{}(", sig.name));
        let params: Vec<String> = sig
            .parameters
            .iter()
            .map(|p| {
                let type_name = p
                    .param_type
                    .as_ref()
                    .map(|t| t.name.as_str())
                    .unwrap_or("any");
                format!("{}: {}", p.name, type_name)
            })
            .collect();
        out.push_str(&params.join(", "));
        out.push(')');
        if let Some(rt) = &sig.return_type {
            out.push_str(&format!(": {}", rt.name));
        }
        out.push_str("\n```\n\n");
    }

    // Recurse into children
    for child in &node.children {
        let next_level = (level + 1).min(6);
        emit_typedoc_node(child, next_level, out);
    }
}
