//! Emit Typst documentation blocks for functions.

use std::path::Path;

use crate::doxygen::memberdef::MemberDef;
use crate::typst_emitter::code;
use crate::typst_emitter::crossref;

/// Emit a complete function documentation block in Typst.
pub fn emit_function(
    member: &MemberDef,
    compound_name: &str,
    graph_svg: Option<&Path>,
) -> String {
    let mut out = String::new();
    let label = crossref::make_member_label(compound_name, &member.name);

    // Heading with label
    out.push_str(&format!(
        "=== {} {}\n\n",
        code::escape_typst_content(&member.name),
        crossref::CrossRefRegistry::emit_label(&label),
    ));

    // Signature
    out.push_str(&code::emit_signature(&member.definition, &member.argsstring));
    out.push('\n');

    // Brief
    if !member.brief.is_empty() {
        out.push_str(&code::escape_typst_content(&member.brief));
        out.push_str("\n\n");
    }

    // Detailed description
    if !member.description.detailed.is_empty() {
        out.push_str(&code::escape_typst_content(&member.description.detailed));
        out.push_str("\n\n");
    }

    // Parameters table
    if !member.description.params.is_empty() {
        out.push_str("*Parameters:*\n\n");
        out.push_str("#param-table(\n");
        for p in &member.description.params {
            let dir = if p.direction.is_empty() {
                "in"
            } else {
                &p.direction
            };
            out.push_str(&format!(
                "  [{}], [`{}`], [{}],\n",
                dir,
                code::escape_typst_content(&p.name),
                code::escape_typst_content(&p.description),
            ));
        }
        out.push_str(")\n\n");
    }

    // Return value
    if !member.description.return_doc.is_empty() {
        out.push_str(&format!(
            "*Returns:* {}\n\n",
            code::escape_typst_content(&member.description.return_doc)
        ));
    }

    // Return values table
    if !member.description.retvals.is_empty() {
        out.push_str("#retval-table(\n");
        for (val, desc) in &member.description.retvals {
            out.push_str(&format!(
                "  [`{}`], [{}],\n",
                code::escape_typst_content(val),
                code::escape_typst_content(desc),
            ));
        }
        out.push_str(")\n\n");
    }

    // Preconditions
    for pre in &member.description.preconditions {
        out.push_str(&format!(
            "#doc-precondition[{}]\n",
            code::escape_typst_content(pre)
        ));
    }

    // Postconditions
    for post in &member.description.postconditions {
        out.push_str(&format!(
            "#doc-postcondition[{}]\n",
            code::escape_typst_content(post)
        ));
    }

    // Invariants
    for inv in &member.description.invariants {
        out.push_str(&format!(
            "*Invariant:* {}\n\n",
            code::escape_typst_content(inv)
        ));
    }

    // Notes
    for note in &member.description.notes {
        out.push_str(&format!(
            "#doc-note[{}]\n",
            code::escape_typst_content(note)
        ));
    }

    // Warnings
    for warning in &member.description.warnings {
        out.push_str(&format!(
            "#doc-warning[{}]\n",
            code::escape_typst_content(warning)
        ));
    }

    // Named sections (e.g., @par Thread Safety, @par Performance)
    for (title, content) in &member.description.named_sections {
        if !title.is_empty() {
            out.push_str(&format!(
                "*{}:* {}\n\n",
                code::escape_typst_content(title),
                code::escape_typst_content(content),
            ));
        }
    }

    // Code examples
    for example in &member.description.code_examples {
        out.push_str("*Example:*\n\n");
        out.push_str(&code::emit_code_block(example));
        out.push('\n');
    }

    // See also (cross-references)
    if !member.description.see_also.is_empty() {
        out.push_str("*See also:* ");
        let refs: Vec<String> = member
            .description
            .see_also
            .iter()
            .map(|s| format!("`{}`", code::escape_typst_content(s)))
            .collect();
        out.push_str(&refs.join(", "));
        out.push_str("\n\n");
    }

    // Call graph image
    if let Some(svg) = graph_svg {
        out.push_str(&super::graph::emit_callgraph_image(
            svg,
            &format!("Call graph for {}", member.name),
        ));
    }

    // Source location
    if !member.location_file.is_empty() {
        out.push_str(&format!(
            "_Defined in `{}:{}`_\n\n",
            code::escape_typst_content(&member.location_file),
            member.location_line
        ));
    }

    // Since version
    if !member.description.since.is_empty() {
        out.push_str(&format!("_Since {}_\n\n", member.description.since));
    }

    out.push_str("---\n\n");
    out
}
