//! Cross-reference label and link management for Typst output.
//!
//! Every documented function, type, and file gets a unique Typst label.
//! `@see` cross-references and call graph links resolve to these labels.

use std::collections::HashMap;

/// Registry of all cross-reference labels in the document.
#[derive(Debug, Default)]
pub struct CrossRefRegistry {
    /// Maps Doxygen refid -> Typst label string.
    labels: HashMap<String, String>,
}

impl CrossRefRegistry {
    pub fn new() -> Self {
        Self::default()
    }

    /// Register a label for a Doxygen refid.
    pub fn register(&mut self, refid: &str, label: &str) {
        self.labels.insert(refid.to_string(), label.to_string());
    }

    /// Look up the Typst label for a Doxygen refid.
    pub fn get_label(&self, refid: &str) -> Option<&str> {
        self.labels.get(refid).map(|s| s.as_str())
    }

    /// Generate a Typst label declaration (placed after a heading or block).
    pub fn emit_label(label: &str) -> String {
        format!("<{}>", sanitize_label(label))
    }

    /// Generate a Typst reference to a label.
    pub fn emit_ref(label: &str) -> String {
        format!("@{}", sanitize_label(label))
    }

    /// Generate a Typst reference with custom display text.
    pub fn emit_ref_with_text(label: &str, text: &str) -> String {
        format!("#link(label(\"{}\"))[{}]", sanitize_label(label), text)
    }

    /// Return all registered labels sorted alphabetically by label name.
    pub fn all_labels_sorted(&self) -> Vec<(&str, &str)> {
        let mut pairs: Vec<_> = self
            .labels
            .iter()
            .map(|(k, v)| (k.as_str(), v.as_str()))
            .collect();
        pairs.sort_by(|a, b| a.1.cmp(b.1));
        pairs
    }
}

/// Sanitize a string for use as a Typst label (alphanumeric + hyphens only).
pub fn sanitize_label(s: &str) -> String {
    s.chars()
        .map(|c| {
            if c.is_alphanumeric() || c == '-' {
                c
            } else {
                '-'
            }
        })
        .collect()
}

/// Create a stable label from a member name and its compound.
pub fn make_member_label(compound_name: &str, member_name: &str) -> String {
    format!(
        "{}--{}",
        sanitize_label(compound_name),
        sanitize_label(member_name)
    )
}

/// Create a stable label for a compound (file, struct, class).
pub fn make_compound_label(compound_name: &str) -> String {
    format!("compound-{}", sanitize_label(compound_name))
}
