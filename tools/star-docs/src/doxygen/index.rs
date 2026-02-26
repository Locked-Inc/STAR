//! Parse Doxygen `index.xml` to enumerate all documented compounds.
//!
//! The index file lists every compound (file, class, struct, namespace, etc.)
//! with a `refid` that maps to the per-compound XML filename.

use std::path::Path;

/// One entry from `index.xml`.
#[derive(Debug, Clone)]
#[allow(dead_code)]
pub struct IndexEntry {
    /// The refid attribute, used as the XML filename stem (e.g. "rx__pid_8c").
    pub refid: String,
    /// Compound kind (file, class, struct, namespace, dir, page, group).
    pub kind: String,
    /// Human-readable compound name.
    pub name: String,
    /// Member refids within this compound.
    pub members: Vec<IndexMember>,
}

/// A member listed inside an index compound entry.
#[derive(Debug, Clone)]
#[allow(dead_code)]
pub struct IndexMember {
    pub refid: String,
    pub kind: String,
    pub name: String,
}

/// Parse `index.xml` from the given Doxygen XML output directory.
pub fn parse_index(xml_dir: &Path) -> anyhow::Result<Vec<IndexEntry>> {
    let index_path = xml_dir.join("index.xml");
    let content = std::fs::read_to_string(&index_path)
        .map_err(|e| anyhow::anyhow!("Failed to read {}: {}", index_path.display(), e))?;

    let doc = roxmltree::Document::parse(&content)?;
    let root = doc.root_element();

    let mut entries = Vec::new();

    for compound in root.children().filter(|n| n.has_tag_name("compound")) {
        let refid = compound
            .attribute("refid")
            .unwrap_or_default()
            .to_string();
        let kind = compound
            .attribute("kind")
            .unwrap_or_default()
            .to_string();
        let name = compound
            .children()
            .find(|n| n.has_tag_name("name"))
            .and_then(|n| n.text())
            .unwrap_or_default()
            .to_string();

        let mut members = Vec::new();
        for member in compound.children().filter(|n| n.has_tag_name("member")) {
            let m_refid = member
                .attribute("refid")
                .unwrap_or_default()
                .to_string();
            let m_kind = member
                .attribute("kind")
                .unwrap_or_default()
                .to_string();
            let m_name = member
                .children()
                .find(|n| n.has_tag_name("name"))
                .and_then(|n| n.text())
                .unwrap_or_default()
                .to_string();
            members.push(IndexMember {
                refid: m_refid,
                kind: m_kind,
                name: m_name,
            });
        }

        entries.push(IndexEntry {
            refid,
            kind,
            name,
            members,
        });
    }

    tracing::info!("Parsed index.xml: {} compounds", entries.len());
    Ok(entries)
}
