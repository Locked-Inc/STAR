//! Emit Typst documentation for file-level compounds.
//!
//! Each source file becomes a section with its brief, includes list,
//! and all documented members grouped by kind.

use std::collections::HashMap;

use crate::doxygen::compound::CompoundDef;
use crate::doxygen::memberdef::MemberKind;
use crate::typst_emitter::code;
use crate::typst_emitter::crossref;
use crate::typst_emitter::function;

/// Emit a complete file documentation section.
pub fn emit_file_section(
    compound: &CompoundDef,
    graph_paths: &HashMap<String, std::path::PathBuf>,
) -> String {
    let mut out = String::new();
    let label = crossref::make_compound_label(&compound.name);

    out.push_str(&format!(
        "== {} {}\n\n",
        code::escape_typst_content(&compound.name),
        crossref::CrossRefRegistry::emit_label(&label),
    ));

    // File brief
    if !compound.brief.is_empty() {
        out.push_str(&code::escape_typst_content(&compound.brief));
        out.push_str("\n\n");
    }

    // File detailed description
    if !compound.description.detailed.is_empty() {
        out.push_str(&code::escape_typst_content(&compound.description.detailed));
        out.push_str("\n\n");
    }

    // Includes list
    if !compound.includes.is_empty() {
        out.push_str("*Includes:*\n\n");
        for inc in &compound.includes {
            let bracket = if inc.local { "\"" } else { "<" };
            let close = if inc.local { "\"" } else { ">" };
            out.push_str(&format!(
                "- `{}{}{}`\n",
                bracket, inc.name, close
            ));
        }
        out.push('\n');
    }

    // Group members by kind and emit
    for section in &compound.sections {
        let _section_title = match section.kind.as_str() {
            "func" => "Functions",
            "typedef" => "Type Definitions",
            "enum" => "Enumerations",
            "var" => "Variables",
            "define" => "Macros",
            other => other,
        };

        if section.members.is_empty() {
            continue;
        }

        // Only emit section header for non-function sections (functions get
        // their own subsections)
        let has_non_enumvalue = section
            .members
            .iter()
            .any(|m| m.kind != MemberKind::EnumValue);
        if !has_non_enumvalue {
            continue;
        }

        // Functions get the full treatment
        if section.kind == "func" {
            for member in &section.members {
                let svg = graph_paths.get(&member.id).map(|p| p.as_path());
                out.push_str(&function::emit_function(member, &compound.name, svg));
            }
        } else {
            // Non-function members
            for member in &section.members {
                if member.kind == MemberKind::EnumValue {
                    continue; // Handled within their parent enum
                }
                out.push_str(&super::compound::emit_member(member, &compound.name));
            }
        }
    }

    out
}
