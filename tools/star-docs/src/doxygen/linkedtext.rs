//! Parse Doxygen `linkedTextType` elements.
//!
//! These appear in type names, parameter types, and return types where
//! identifiers may be cross-referenced via `<ref>` elements.

use roxmltree::Node;

/// A segment of linked text -- either plain text or a cross-reference.
#[derive(Debug, Clone)]
pub enum LinkedSegment {
    /// Plain text (not a link).
    Text(String),
    /// A cross-reference to another documented element.
    Ref {
        refid: String,
        kind_ref: String,
        text: String,
    },
}

/// Parse a node containing mixed text and `<ref>` children into segments.
pub fn parse_linked_text(node: Node) -> Vec<LinkedSegment> {
    let mut segments = Vec::new();

    for child in node.children() {
        if child.is_text() {
            let text = child.text().unwrap_or_default().to_string();
            if !text.is_empty() {
                segments.push(LinkedSegment::Text(text));
            }
        } else if child.has_tag_name("ref") {
            let refid = child.attribute("refid").unwrap_or_default().to_string();
            let kind_ref = child.attribute("kindref").unwrap_or_default().to_string();
            let text = super::description::collect_text_recursive(child);
            segments.push(LinkedSegment::Ref {
                refid,
                kind_ref,
                text,
            });
        }
    }

    segments
}

/// Flatten linked text segments into a plain string (discarding link info).
pub fn linked_text_to_string(segments: &[LinkedSegment]) -> String {
    segments
        .iter()
        .map(|s| match s {
            LinkedSegment::Text(t) => t.as_str(),
            LinkedSegment::Ref { text, .. } => text.as_str(),
        })
        .collect()
}
