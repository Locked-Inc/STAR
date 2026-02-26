//! Go documentation support via JSON from the `go-doc-json` helper tool.
//!
//! The `tools/go-doc-json` Go program uses stdlib `go/parser`, `go/ast`,
//! and `go/doc` to parse Go packages and emit a JSON representation that
//! this module consumes.

use serde::Deserialize;
use std::path::Path;

/// Top-level JSON output from go-doc-json.
#[derive(Debug, Deserialize)]
pub struct GoDocOutput {
    pub packages: Vec<GoPackage>,
}

/// A documented Go package.
#[derive(Debug, Deserialize)]
pub struct GoPackage {
    pub name: String,
    pub import_path: String,
    pub doc: String,
    #[serde(default)]
    pub types: Vec<GoType>,
    #[serde(default)]
    pub funcs: Vec<GoFunc>,
    #[serde(default)]
    pub consts: Vec<GoConst>,
    #[serde(default)]
    pub vars: Vec<GoVar>,
}

/// A documented Go type (struct, interface, etc.).
#[derive(Debug, Deserialize)]
pub struct GoType {
    pub name: String,
    pub doc: String,
    pub decl: String,
    #[serde(default)]
    pub methods: Vec<GoFunc>,
    #[serde(default)]
    pub fields: Vec<GoField>,
}

/// A struct field with documentation.
#[derive(Debug, Deserialize)]
pub struct GoField {
    pub name: String,
    pub type_name: String,
    pub doc: String,
    pub tag: String,
}

/// A documented Go function or method.
#[derive(Debug, Deserialize)]
pub struct GoFunc {
    pub name: String,
    pub doc: String,
    pub signature: String,
    pub recv: String,
}

/// A documented Go constant group.
#[derive(Debug, Deserialize)]
pub struct GoConst {
    pub names: Vec<String>,
    pub doc: String,
    pub decl: String,
}

/// A documented Go variable.
#[derive(Debug, Deserialize)]
pub struct GoVar {
    pub names: Vec<String>,
    pub doc: String,
    pub decl: String,
}

/// Parse Go documentation JSON file.
pub fn parse_go_doc_json(path: &Path) -> anyhow::Result<GoDocOutput> {
    let content = std::fs::read_to_string(path)
        .map_err(|e| anyhow::anyhow!("Failed to read Go doc JSON {}: {}", path.display(), e))?;
    let output: GoDocOutput = serde_json::from_str(&content)?;
    tracing::info!("Parsed {} Go packages", output.packages.len());
    Ok(output)
}

/// Emit Typst documentation for all Go packages.
pub fn emit_go_chapter(doc: &GoDocOutput) -> String {
    let mut out = String::new();

    for pkg in &doc.packages {
        out.push_str(&format!("== Package `{}`\n\n", pkg.name));

        if !pkg.doc.is_empty() {
            out.push_str(&pkg.doc);
            out.push_str("\n\n");
        }

        // Types
        for typ in &pkg.types {
            out.push_str(&format!("=== {}\n\n", typ.name));
            if !typ.doc.is_empty() {
                out.push_str(&typ.doc);
                out.push_str("\n\n");
            }
            if !typ.decl.is_empty() {
                out.push_str(&format!("```go\n{}\n```\n\n", typ.decl));
            }
            // Fields
            if !typ.fields.is_empty() {
                out.push_str("*Fields:*\n\n");
                for field in &typ.fields {
                    out.push_str(&format!(
                        "- `{}` (`{}`): {}\n",
                        field.name, field.type_name, field.doc
                    ));
                }
                out.push('\n');
            }
            // Methods
            for method in &typ.methods {
                out.push_str(&format!("==== {}\n\n", method.name));
                out.push_str(&format!("```go\n{}\n```\n\n", method.signature));
                if !method.doc.is_empty() {
                    out.push_str(&method.doc);
                    out.push_str("\n\n");
                }
            }
        }

        // Package-level functions
        for func in &pkg.funcs {
            out.push_str(&format!("=== {}\n\n", func.name));
            out.push_str(&format!("```go\n{}\n```\n\n", func.signature));
            if !func.doc.is_empty() {
                out.push_str(&func.doc);
                out.push_str("\n\n");
            }
        }

        // Constants
        if !pkg.consts.is_empty() {
            out.push_str("=== Constants\n\n");
            for c in &pkg.consts {
                out.push_str(&format!("```go\n{}\n```\n\n", c.decl));
                if !c.doc.is_empty() {
                    out.push_str(&c.doc);
                    out.push_str("\n\n");
                }
            }
        }
    }

    out
}
