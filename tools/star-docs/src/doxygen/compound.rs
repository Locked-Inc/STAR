//! Parse Doxygen per-compound XML files.
//!
//! Each compound (file, class, struct, namespace) gets its own XML file
//! containing a `<compounddef>` with `<sectiondef>` groups of `<memberdef>`s.

use std::path::Path;

use roxmltree::Node;

use super::description::{self, Description};
use super::memberdef::{self, MemberDef};

/// The kind of compound.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum CompoundKind {
    File,
    Class,
    Struct,
    Union,
    Namespace,
    Group,
    Page,
    Dir,
    Unknown(String),
}

impl CompoundKind {
    pub fn from_str(s: &str) -> Self {
        match s {
            "file" => Self::File,
            "class" => Self::Class,
            "struct" => Self::Struct,
            "union" => Self::Union,
            "namespace" => Self::Namespace,
            "group" => Self::Group,
            "page" => Self::Page,
            "dir" => Self::Dir,
            other => Self::Unknown(other.to_string()),
        }
    }
}

/// A section within a compound (groups members by visibility/kind).
#[derive(Debug, Clone)]
pub struct SectionDef {
    /// Section kind (e.g., "func", "typedef", "enum", "var", "define").
    pub kind: String,
    /// Members in this section.
    pub members: Vec<MemberDef>,
}

/// An include directive listed in the compound.
#[derive(Debug, Clone)]
pub struct IncludeEntry {
    pub refid: String,
    pub local: bool,
    pub name: String,
}

/// A fully parsed compound definition.
#[derive(Debug, Clone)]
pub struct CompoundDef {
    /// Unique identifier.
    pub id: String,
    /// Compound kind.
    pub kind: CompoundKind,
    /// Compound name (e.g., "rx_pid.c" for files, "rx_pid_config_t" for structs).
    pub name: String,
    /// Brief description.
    pub brief: String,
    /// Full documentation.
    pub description: Description,
    /// Source file location.
    pub location_file: String,
    /// Include directives.
    pub includes: Vec<IncludeEntry>,
    /// Member sections.
    pub sections: Vec<SectionDef>,
    /// Inner classes/structs (for file compounds).
    pub inner_classes: Vec<InnerRef>,
    /// Inner namespaces.
    pub inner_namespaces: Vec<InnerRef>,
}

/// A reference to an inner compound (class, struct, namespace).
#[derive(Debug, Clone)]
pub struct InnerRef {
    pub refid: String,
    pub name: String,
}

/// Parse a compound XML file from the Doxygen XML output directory.
pub fn parse_compound_file(xml_dir: &Path, refid: &str) -> anyhow::Result<CompoundDef> {
    let file_path = xml_dir.join(format!("{}.xml", refid));
    let content = std::fs::read_to_string(&file_path)
        .map_err(|e| anyhow::anyhow!("Failed to read {}: {}", file_path.display(), e))?;

    let doc = roxmltree::Document::parse(&content)?;
    let root = doc.root_element();

    let compounddef = root
        .children()
        .find(|n| n.has_tag_name("compounddef"))
        .ok_or_else(|| anyhow::anyhow!("No <compounddef> found in {}", file_path.display()))?;

    parse_compounddef(compounddef)
}

/// Parse a `<compounddef>` element.
fn parse_compounddef(node: Node) -> anyhow::Result<CompoundDef> {
    let id = node.attribute("id").unwrap_or_default().to_string();
    let kind = CompoundKind::from_str(node.attribute("kind").unwrap_or_default());

    let name = node
        .children()
        .find(|n| n.has_tag_name("compoundname"))
        .and_then(|n| n.text())
        .unwrap_or_default()
        .to_string();

    let brief = node
        .children()
        .find(|n| n.has_tag_name("briefdescription"))
        .map(description::parse_brief)
        .unwrap_or_default();

    let mut desc = node
        .children()
        .find(|n| n.has_tag_name("detaileddescription"))
        .map(description::parse_detailed)
        .unwrap_or_default();
    desc.brief = brief.clone();

    let location = node.children().find(|n| n.has_tag_name("location"));
    let location_file = location
        .and_then(|n| n.attribute("file"))
        .unwrap_or_default()
        .to_string();

    // Parse includes
    let includes = node
        .children()
        .filter(|n| n.has_tag_name("includes"))
        .map(|n| IncludeEntry {
            refid: n.attribute("refid").unwrap_or_default().to_string(),
            local: n.attribute("local").unwrap_or("no") == "yes",
            name: n.text().unwrap_or_default().to_string(),
        })
        .collect();

    // Parse sections
    let sections = node
        .children()
        .filter(|n| n.has_tag_name("sectiondef"))
        .map(|section| {
            let section_kind = section.attribute("kind").unwrap_or_default().to_string();
            let members = section
                .children()
                .filter(|n| n.has_tag_name("memberdef"))
                .map(memberdef::parse_memberdef)
                .collect();
            SectionDef {
                kind: section_kind,
                members,
            }
        })
        .collect();

    // Parse inner classes/structs
    let inner_classes = node
        .children()
        .filter(|n| n.has_tag_name("innerclass"))
        .map(|n| InnerRef {
            refid: n.attribute("refid").unwrap_or_default().to_string(),
            name: n.text().unwrap_or_default().to_string(),
        })
        .collect();

    let inner_namespaces = node
        .children()
        .filter(|n| n.has_tag_name("innernamespace"))
        .map(|n| InnerRef {
            refid: n.attribute("refid").unwrap_or_default().to_string(),
            name: n.text().unwrap_or_default().to_string(),
        })
        .collect();

    Ok(CompoundDef {
        id,
        kind,
        name,
        brief,
        description: desc,
        location_file,
        includes,
        sections,
        inner_classes,
        inner_namespaces,
    })
}
