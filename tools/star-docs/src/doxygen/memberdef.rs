//! Parse Doxygen `memberdef` XML elements.
//!
//! A `memberdef` represents a documented function, variable, typedef, enum,
//! enum value, or macro definition within a compound (file, class, struct).

use roxmltree::Node;

use super::description::{self, Description};
use super::linkedtext::{self, LinkedSegment};

/// The kind of a member definition.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum MemberKind {
    Function,
    Variable,
    Typedef,
    Enum,
    EnumValue,
    Define,
    Unknown(String),
}

impl MemberKind {
    pub fn from_str(s: &str) -> Self {
        match s {
            "function" => Self::Function,
            "variable" => Self::Variable,
            "typedef" => Self::Typedef,
            "enum" => Self::Enum,
            "enumvalue" => Self::EnumValue,
            "define" => Self::Define,
            other => Self::Unknown(other.to_string()),
        }
    }
}

/// A function parameter (from `<param>` inside `memberdef`).
#[derive(Debug, Clone)]
pub struct Param {
    /// The type of the parameter as linked text segments.
    pub type_segments: Vec<LinkedSegment>,
    /// The parameter name.
    pub declname: String,
    /// Default value, if any.
    pub defval: String,
}

/// A reference to another member (callgraph edge).
#[derive(Debug, Clone)]
pub struct MemberRef {
    pub refid: String,
    pub compound_ref: String,
    pub name: String,
}

/// An enum value entry.
#[derive(Debug, Clone)]
pub struct EnumValue {
    pub name: String,
    pub id: String,
    pub initializer: String,
    pub brief: String,
    pub detailed: String,
}

/// A fully parsed member definition.
#[derive(Debug, Clone)]
pub struct MemberDef {
    /// Unique identifier within Doxygen output.
    pub id: String,
    /// Member kind.
    pub kind: MemberKind,
    /// Member name (function name, variable name, etc.).
    pub name: String,
    /// Qualified name (e.g., "rx_pid.c::rx_pid_compute").
    pub qualifiedname: String,
    /// Return type as linked text segments.
    pub return_type: Vec<LinkedSegment>,
    /// Function parameters.
    pub params: Vec<Param>,
    /// Full function signature string (definition).
    pub definition: String,
    /// Argument string (e.g., "(int x, float y)").
    pub argsstring: String,
    /// Brief description.
    pub brief: String,
    /// Full parsed documentation.
    pub description: Description,
    /// Source file location.
    pub location_file: String,
    /// Source line number.
    pub location_line: u32,
    /// Whether this is a static member.
    pub is_static: bool,
    /// Whether this is inline.
    pub is_inline: bool,
    /// Functions called by this function.
    pub references: Vec<MemberRef>,
    /// Functions that call this function.
    pub referenced_by: Vec<MemberRef>,
    /// Enum values (only for kind == Enum).
    pub enum_values: Vec<EnumValue>,
    /// Initializer (for variables, defines, enum values).
    pub initializer: String,
}

/// Parse a `<memberdef>` XML node into a `MemberDef`.
pub fn parse_memberdef(node: Node) -> MemberDef {
    let id = node.attribute("id").unwrap_or_default().to_string();
    let kind = MemberKind::from_str(node.attribute("kind").unwrap_or_default());
    let is_static = node.attribute("static").unwrap_or("no") == "yes";
    let is_inline = node.attribute("inline").unwrap_or("no") == "yes";

    let name = get_child_text(&node, "name");
    let qualifiedname = get_child_text(&node, "qualifiedname");
    let definition = get_child_text(&node, "definition");
    let argsstring = get_child_text(&node, "argsstring");
    let initializer = get_child_text(&node, "initializer");

    // Parse return type
    let return_type = node
        .children()
        .find(|n| n.has_tag_name("type"))
        .map(|n| linkedtext::parse_linked_text(n))
        .unwrap_or_default();

    // Parse function parameters
    let params = node
        .children()
        .filter(|n| n.has_tag_name("param"))
        .map(parse_param)
        .collect();

    // Parse brief description
    let brief = node
        .children()
        .find(|n| n.has_tag_name("briefdescription"))
        .map(description::parse_brief)
        .unwrap_or_default();

    // Parse detailed description
    let mut desc = node
        .children()
        .find(|n| n.has_tag_name("detaileddescription"))
        .map(description::parse_detailed)
        .unwrap_or_default();
    desc.brief = brief.clone();

    // Parse location
    let location = node.children().find(|n| n.has_tag_name("location"));
    let location_file = location
        .and_then(|n| n.attribute("file"))
        .unwrap_or_default()
        .to_string();
    let location_line = location
        .and_then(|n| n.attribute("line"))
        .and_then(|s| s.parse().ok())
        .unwrap_or(0);

    // Parse references (outgoing calls)
    let references = node
        .children()
        .filter(|n| n.has_tag_name("references"))
        .map(|n| MemberRef {
            refid: n.attribute("refid").unwrap_or_default().to_string(),
            compound_ref: n.attribute("compoundref").unwrap_or_default().to_string(),
            name: description::collect_text_recursive(n).trim().to_string(),
        })
        .collect();

    // Parse referenced-by (incoming calls)
    let referenced_by = node
        .children()
        .filter(|n| n.has_tag_name("referencedby"))
        .map(|n| MemberRef {
            refid: n.attribute("refid").unwrap_or_default().to_string(),
            compound_ref: n.attribute("compoundref").unwrap_or_default().to_string(),
            name: description::collect_text_recursive(n).trim().to_string(),
        })
        .collect();

    // Parse enum values
    let enum_values = node
        .children()
        .filter(|n| n.has_tag_name("enumvalue"))
        .map(|n| {
            let ev_id = n.attribute("id").unwrap_or_default().to_string();
            let ev_name = get_child_text(&n, "name");
            let ev_init = get_child_text(&n, "initializer");
            let ev_brief = n
                .children()
                .find(|c| c.has_tag_name("briefdescription"))
                .map(description::parse_brief)
                .unwrap_or_default();
            let ev_detailed = n
                .children()
                .find(|c| c.has_tag_name("detaileddescription"))
                .map(|c| description::collect_text_recursive(c).trim().to_string())
                .unwrap_or_default();
            EnumValue {
                name: ev_name,
                id: ev_id,
                initializer: ev_init,
                brief: ev_brief,
                detailed: ev_detailed,
            }
        })
        .collect();

    MemberDef {
        id,
        kind,
        name,
        qualifiedname,
        return_type,
        params,
        definition,
        argsstring,
        brief,
        description: desc,
        location_file,
        location_line,
        is_static,
        is_inline,
        references,
        referenced_by,
        enum_values,
        initializer,
    }
}

/// Parse a `<param>` element.
fn parse_param(node: Node) -> Param {
    let type_segments = node
        .children()
        .find(|n| n.has_tag_name("type"))
        .map(|n| linkedtext::parse_linked_text(n))
        .unwrap_or_default();
    let declname = get_child_text(&node, "declname");
    let defval = get_child_text(&node, "defval");

    Param {
        type_segments,
        declname,
        defval,
    }
}

/// Get the text content of a direct child element by tag name.
fn get_child_text(node: &Node, tag: &str) -> String {
    node.children()
        .find(|n| n.has_tag_name(tag))
        .map(|n| description::collect_text_recursive(n).trim().to_string())
        .unwrap_or_default()
}
