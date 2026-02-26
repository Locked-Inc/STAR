//! Build call graphs from Doxygen XML `references`/`referencedby` elements.
//!
//! Doxygen encodes function call relationships as `<references>` (outgoing)
//! and `<referencedby>` (incoming) attributes on `<memberdef>` elements,
//! NOT as `graphType` nodes.  We build DOT source from these pairs and
//! shell out to `dot -Tsvg` to render them.

use std::collections::HashMap;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::process::Command;

use super::memberdef::{MemberDef, MemberKind};

/// A directed edge in the call graph.
#[derive(Debug, Clone, Hash, PartialEq, Eq)]
pub struct CallEdge {
    pub caller: String,
    pub callee: String,
}

/// Build a DOT-format call graph for a single function.
pub fn build_callgraph_dot(member: &MemberDef) -> Option<String> {
    if member.kind != MemberKind::Function {
        return None;
    }
    if member.references.is_empty() && member.referenced_by.is_empty() {
        return None;
    }

    let mut dot = String::new();
    dot.push_str("digraph callgraph {\n");
    dot.push_str("  rankdir=LR;\n");
    dot.push_str("  node [shape=box, fontname=\"monospace\", fontsize=10];\n");
    dot.push_str("  edge [fontsize=8];\n");

    // Center node (this function)
    dot.push_str(&format!(
        "  \"{}\" [style=filled, fillcolor=\"#e3f2fd\"];\n",
        escape_dot(&member.name)
    ));

    // Outgoing calls (references)
    for r in &member.references {
        dot.push_str(&format!(
            "  \"{}\" -> \"{}\";\n",
            escape_dot(&member.name),
            escape_dot(&r.name)
        ));
    }

    // Incoming calls (referenced_by)
    for r in &member.referenced_by {
        dot.push_str(&format!(
            "  \"{}\" -> \"{}\";\n",
            escape_dot(&r.name),
            escape_dot(&member.name)
        ));
    }

    dot.push_str("}\n");
    Some(dot)
}

/// Build a DOT-format caller graph (who calls this function).
pub fn build_callergraph_dot(member: &MemberDef) -> Option<String> {
    if member.kind != MemberKind::Function || member.referenced_by.is_empty() {
        return None;
    }

    let mut dot = String::new();
    dot.push_str("digraph callergraph {\n");
    dot.push_str("  rankdir=LR;\n");
    dot.push_str("  node [shape=box, fontname=\"monospace\", fontsize=10];\n");

    dot.push_str(&format!(
        "  \"{}\" [style=filled, fillcolor=\"#e3f2fd\"];\n",
        escape_dot(&member.name)
    ));

    for r in &member.referenced_by {
        dot.push_str(&format!(
            "  \"{}\" -> \"{}\";\n",
            escape_dot(&r.name),
            escape_dot(&member.name)
        ));
    }

    dot.push_str("}\n");
    Some(dot)
}

/// Render a DOT string to an SVG file using the `dot` command.
pub fn render_dot_to_svg(dot_source: &str, output_path: &Path) -> anyhow::Result<()> {
    if let Some(parent) = output_path.parent() {
        std::fs::create_dir_all(parent)?;
    }

    let mut child = Command::new("dot")
        .args(["-Tsvg"])
        .stdin(std::process::Stdio::piped())
        .stdout(std::process::Stdio::piped())
        .stderr(std::process::Stdio::piped())
        .spawn()
        .map_err(|e| anyhow::anyhow!("Failed to run dot: {}. Is graphviz installed?", e))?;

    if let Some(ref mut stdin) = child.stdin {
        stdin.write_all(dot_source.as_bytes())?;
    }

    let output = child.wait_with_output()?;
    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        anyhow::bail!("dot failed: {}", stderr);
    }

    std::fs::write(output_path, &output.stdout)?;
    Ok(())
}

/// Render all call graphs for a set of members, returning paths to SVG files.
pub fn render_member_graphs(
    members: &[MemberDef],
    output_dir: &Path,
) -> HashMap<String, PathBuf> {
    let mut graph_paths = HashMap::new();

    for member in members {
        if let Some(dot) = build_callgraph_dot(member) {
            let svg_name = format!("{}_callgraph.svg", sanitize_filename(&member.id));
            let svg_path = output_dir.join(&svg_name);
            match render_dot_to_svg(&dot, &svg_path) {
                Ok(()) => {
                    graph_paths.insert(member.id.clone(), svg_path);
                }
                Err(e) => {
                    tracing::warn!(
                        "Failed to render callgraph for {}: {}",
                        member.name,
                        e
                    );
                }
            }
        }
    }

    graph_paths
}

/// Escape special characters for DOT labels.
fn escape_dot(s: &str) -> String {
    s.replace('\\', "\\\\")
        .replace('"', "\\\"")
        .replace('<', "\\<")
        .replace('>', "\\>")
}

/// Sanitize a Doxygen refid for use as a filename.
fn sanitize_filename(s: &str) -> String {
    s.chars()
        .map(|c| if c.is_alphanumeric() || c == '_' || c == '-' { c } else { '_' })
        .collect()
}
