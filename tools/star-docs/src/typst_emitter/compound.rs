//! Emit Typst documentation blocks for structs, enums, typedefs, and classes.

use crate::doxygen::memberdef::{MemberDef, MemberKind};
use crate::typst_emitter::code;
use crate::typst_emitter::crossref;

/// Emit documentation for an enum member definition.
pub fn emit_enum(member: &MemberDef, compound_name: &str) -> String {
    let mut out = String::new();
    let label = crossref::make_member_label(compound_name, &member.name);

    out.push_str(&format!(
        "=== {} {}\n\n",
        code::escape_typst_content(&member.name),
        crossref::CrossRefRegistry::emit_label(&label),
    ));

    if !member.brief.is_empty() {
        out.push_str(&code::escape_typst_content(&member.brief));
        out.push_str("\n\n");
    }

    if !member.description.detailed.is_empty() {
        out.push_str(&code::escape_typst_content(&member.description.detailed));
        out.push_str("\n\n");
    }

    // Enum values table
    if !member.enum_values.is_empty() {
        out.push_str("#table(\n");
        out.push_str("  columns: (auto, auto, 1fr),\n");
        out.push_str("  stroke: 0.5pt + rgb(\"#e0e0e0\"),\n");
        out.push_str("  inset: 6pt,\n");
        out.push_str("  fill: (_, y) => if y == 0 { rgb(\"#e3f2fd\") } else { none },\n");
        out.push_str("  [*Value*], [*Name*], [*Description*],\n");
        for ev in &member.enum_values {
            let desc = if !ev.brief.is_empty() {
                &ev.brief
            } else {
                &ev.detailed
            };
            out.push_str(&format!(
                "  [`{}`], [`{}`], [{}],\n",
                code::escape_typst_content(&ev.initializer),
                code::escape_typst_content(&ev.name),
                code::escape_typst_content(desc),
            ));
        }
        out.push_str(")\n\n");
    }

    emit_common_doc_tags(&member.description, &mut out);
    out.push_str("---\n\n");
    out
}

/// Emit documentation for a typedef member.
pub fn emit_typedef(member: &MemberDef, compound_name: &str) -> String {
    let mut out = String::new();
    let label = crossref::make_member_label(compound_name, &member.name);

    out.push_str(&format!(
        "=== {} {}\n\n",
        code::escape_typst_content(&member.name),
        crossref::CrossRefRegistry::emit_label(&label),
    ));

    // Show the full definition
    if !member.definition.is_empty() {
        out.push_str(&format!(
            "```c\n{}\n```\n\n",
            member.definition
        ));
    }

    if !member.brief.is_empty() {
        out.push_str(&code::escape_typst_content(&member.brief));
        out.push_str("\n\n");
    }

    if !member.description.detailed.is_empty() {
        out.push_str(&code::escape_typst_content(&member.description.detailed));
        out.push_str("\n\n");
    }

    emit_common_doc_tags(&member.description, &mut out);
    out.push_str("---\n\n");
    out
}

/// Emit documentation for a variable member.
pub fn emit_variable(member: &MemberDef, compound_name: &str) -> String {
    let mut out = String::new();
    let label = crossref::make_member_label(compound_name, &member.name);

    out.push_str(&format!(
        "=== {} {}\n\n",
        code::escape_typst_content(&member.name),
        crossref::CrossRefRegistry::emit_label(&label),
    ));

    if !member.definition.is_empty() {
        out.push_str(&format!(
            "```c\n{}\n```\n\n",
            member.definition
        ));
    }

    if !member.brief.is_empty() {
        out.push_str(&code::escape_typst_content(&member.brief));
        out.push_str("\n\n");
    }

    emit_common_doc_tags(&member.description, &mut out);
    out.push_str("---\n\n");
    out
}

/// Emit documentation for a macro definition.
pub fn emit_define(member: &MemberDef, compound_name: &str) -> String {
    let mut out = String::new();
    let label = crossref::make_member_label(compound_name, &member.name);

    out.push_str(&format!(
        "=== {} {}\n\n",
        code::escape_typst_content(&member.name),
        crossref::CrossRefRegistry::emit_label(&label),
    ));

    let sig = if member.argsstring.is_empty() {
        format!("#define {}", member.name)
    } else {
        format!("#define {}{}", member.name, member.argsstring)
    };
    // Append initializer if present
    let sig = if member.initializer.is_empty() {
        sig
    } else {
        format!("{} {}", sig, member.initializer)
    };
    out.push_str(&format!("```c\n{}\n```\n\n", sig));

    if !member.brief.is_empty() {
        out.push_str(&code::escape_typst_content(&member.brief));
        out.push_str("\n\n");
    }

    emit_common_doc_tags(&member.description, &mut out);
    out.push_str("---\n\n");
    out
}

/// Emit a member based on its kind, delegating to the appropriate emitter.
pub fn emit_member(member: &MemberDef, compound_name: &str) -> String {
    match member.kind {
        MemberKind::Enum => emit_enum(member, compound_name),
        MemberKind::Typedef => emit_typedef(member, compound_name),
        MemberKind::Variable => emit_variable(member, compound_name),
        MemberKind::Define => emit_define(member, compound_name),
        MemberKind::EnumValue => String::new(), // Handled within enum
        _ => String::new(),
    }
}

/// Emit common documentation tags shared across member types.
fn emit_common_doc_tags(
    desc: &crate::doxygen::description::Description,
    out: &mut String,
) {
    for note in &desc.notes {
        out.push_str(&format!(
            "#doc-note[{}]\n",
            code::escape_typst_content(note)
        ));
    }
    for warning in &desc.warnings {
        out.push_str(&format!(
            "#doc-warning[{}]\n",
            code::escape_typst_content(warning)
        ));
    }
    if !desc.see_also.is_empty() {
        out.push_str("*See also:* ");
        let refs: Vec<String> = desc
            .see_also
            .iter()
            .map(|s| format!("`{}`", code::escape_typst_content(s)))
            .collect();
        out.push_str(&refs.join(", "));
        out.push_str("\n\n");
    }
    for example in &desc.code_examples {
        out.push_str("*Example:*\n\n");
        out.push_str(&code::emit_code_block(example));
        out.push('\n');
    }
    if !desc.since.is_empty() {
        out.push_str(&format!("_Since {}_\n\n", desc.since));
    }
}
