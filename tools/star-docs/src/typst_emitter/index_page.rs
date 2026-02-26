//! Generate an alphabetical function/type index page.

use crate::doxygen::compound::CompoundDef;
use crate::doxygen::memberdef::MemberKind;
use crate::typst_emitter::crossref;

/// Entry in the generated index.
struct IndexItem {
    name: String,
    kind: String,
    compound: String,
    label: String,
}

/// Generate an alphabetical index of all documented functions and types.
pub fn emit_index(compounds: &[CompoundDef]) -> String {
    let mut items: Vec<IndexItem> = Vec::new();

    for compound in compounds {
        for section in &compound.sections {
            for member in &section.members {
                if member.kind == MemberKind::EnumValue {
                    continue;
                }
                let kind_str = match member.kind {
                    MemberKind::Function => "function",
                    MemberKind::Typedef => "typedef",
                    MemberKind::Enum => "enum",
                    MemberKind::Variable => "variable",
                    MemberKind::Define => "macro",
                    _ => continue,
                };
                let label = crossref::make_member_label(&compound.name, &member.name);
                items.push(IndexItem {
                    name: member.name.clone(),
                    kind: kind_str.to_string(),
                    compound: compound.name.clone(),
                    label,
                });
            }
        }
    }

    items.sort_by(|a, b| a.name.to_lowercase().cmp(&b.name.to_lowercase()));

    let mut out = String::new();
    out.push_str("= Index\n\n");

    let mut current_letter = '\0';
    for item in &items {
        let first = item
            .name
            .chars()
            .next()
            .unwrap_or('_')
            .to_ascii_uppercase();
        if first != current_letter {
            current_letter = first;
            out.push_str(&format!("\n== {}\n\n", current_letter));
        }
        out.push_str(&format!(
            "- #link(label(\"{}\"))[`{}`] ({}, {})\n",
            crossref::sanitize_label(&item.label),
            item.name,
            item.kind,
            item.compound,
        ));
    }

    out
}
