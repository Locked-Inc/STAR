//! Parse Doxygen `descriptionType` XML elements.
//!
//! Doxygen wraps documentation in `<briefdescription>` and
//! `<detaileddescription>` elements containing `<para>` with nested:
//! - `<simplesect kind="return|pre|post|see|note|warning">`
//! - `<parameterlist kind="param">` -> `<parameteritem>`
//! - `<programlisting>` -> `<codeline>` -> `<highlight>`
//! - `<ref>` inline cross-references

use roxmltree::Node;

/// Fully parsed documentation for a code element.
#[derive(Debug, Clone, Default)]
pub struct Description {
    /// Brief one-line summary.
    pub brief: String,
    /// Detailed multi-paragraph description.
    pub detailed: String,
    /// @param entries: (direction, name, description).
    pub params: Vec<ParamDoc>,
    /// @return description.
    pub return_doc: String,
    /// @retval entries: (value, description).
    pub retvals: Vec<(String, String)>,
    /// @pre preconditions.
    pub preconditions: Vec<String>,
    /// @post postconditions.
    pub postconditions: Vec<String>,
    /// @note entries.
    pub notes: Vec<String>,
    /// @warning entries.
    pub warnings: Vec<String>,
    /// @see cross-references.
    pub see_also: Vec<String>,
    /// @since version.
    pub since: String,
    /// @invariant entries.
    pub invariants: Vec<String>,
    /// @todo entries.
    pub todos: Vec<String>,
    /// @code blocks.
    pub code_examples: Vec<CodeBlock>,
    /// @par named sections: (title, content).
    pub named_sections: Vec<(String, String)>,
}

/// Documentation for a single parameter.
#[derive(Debug, Clone)]
pub struct ParamDoc {
    pub direction: String, // "in", "out", "in,out"
    pub name: String,
    pub description: String,
}

/// A code example block.
#[derive(Debug, Clone)]
pub struct CodeBlock {
    pub language: String,
    pub content: String,
}

/// Parse a `<briefdescription>` element.
pub fn parse_brief(node: Node) -> String {
    collect_text_recursive(node).trim().to_string()
}

/// Parse a `<detaileddescription>` element into a full `Description`.
pub fn parse_detailed(node: Node) -> Description {
    let mut desc = Description::default();

    for para in node.children().filter(|n| n.has_tag_name("para")) {
        parse_para(&para, &mut desc);
    }

    desc
}

/// Parse a `<para>` element, extracting nested simplesects, parameterlists, etc.
fn parse_para(para: &Node, desc: &mut Description) {
    let mut plain_text = String::new();

    for child in para.children() {
        if child.is_text() {
            plain_text.push_str(child.text().unwrap_or_default());
        } else if child.has_tag_name("simplesect") {
            let kind = child.attribute("kind").unwrap_or_default();
            let content = collect_text_recursive(child).trim().to_string();
            match kind {
                "return" => desc.return_doc = content,
                "pre" => desc.preconditions.push(content),
                "post" => desc.postconditions.push(content),
                "note" => desc.notes.push(content),
                "warning" => desc.warnings.push(content),
                "see" => desc.see_also.push(content),
                "since" => desc.since = content,
                "par" => {
                    let title = child
                        .children()
                        .find(|n| n.has_tag_name("title"))
                        .map(|n| collect_text_recursive(n).trim().to_string())
                        .unwrap_or_default();
                    let body = child
                        .children()
                        .filter(|n| n.has_tag_name("para"))
                        .map(|n| collect_text_recursive(n).trim().to_string())
                        .collect::<Vec<_>>()
                        .join("\n");
                    desc.named_sections.push((title, body));
                }
                "remark" => desc.notes.push(content),
                _ => {
                    // Unknown simplesect kind; append to detailed
                    if !content.is_empty() {
                        if !desc.detailed.is_empty() {
                            desc.detailed.push_str("\n\n");
                        }
                        desc.detailed.push_str(&content);
                    }
                }
            }
        } else if child.has_tag_name("parameterlist") {
            let kind = child.attribute("kind").unwrap_or_default();
            if kind == "param" {
                for item in child
                    .children()
                    .filter(|n| n.has_tag_name("parameteritem"))
                {
                    let param = parse_param_item(item);
                    desc.params.push(param);
                }
            } else if kind == "retval" {
                for item in child
                    .children()
                    .filter(|n| n.has_tag_name("parameteritem"))
                {
                    let name_node = item
                        .descendants()
                        .find(|n| n.has_tag_name("parametername"));
                    let desc_node = item
                        .descendants()
                        .find(|n| n.has_tag_name("parameterdescription"));
                    let name = name_node
                        .map(|n| collect_text_recursive(n).trim().to_string())
                        .unwrap_or_default();
                    let description = desc_node
                        .map(|n| collect_text_recursive(n).trim().to_string())
                        .unwrap_or_default();
                    desc.retvals.push((name, description));
                }
            }
        } else if child.has_tag_name("programlisting") {
            let code = parse_program_listing(child);
            desc.code_examples.push(code);
        } else if child.has_tag_name("ref") {
            plain_text.push_str(&collect_text_recursive(child));
        } else if child.has_tag_name("emphasis") || child.has_tag_name("bold") {
            plain_text.push_str(&collect_text_recursive(child));
        } else if child.has_tag_name("computeroutput") {
            plain_text.push('`');
            plain_text.push_str(&collect_text_recursive(child));
            plain_text.push('`');
        } else if child.has_tag_name("itemizedlist") {
            for li in child.children().filter(|n| n.has_tag_name("listitem")) {
                plain_text.push_str("\n- ");
                plain_text.push_str(collect_text_recursive(li).trim());
            }
        } else if child.has_tag_name("orderedlist") {
            for (i, li) in child
                .children()
                .filter(|n| n.has_tag_name("listitem"))
                .enumerate()
            {
                plain_text.push_str(&format!("\n{}. ", i + 1));
                plain_text.push_str(collect_text_recursive(li).trim());
            }
        }
    }

    let trimmed = plain_text.trim();
    if !trimmed.is_empty() {
        if !desc.detailed.is_empty() {
            desc.detailed.push_str("\n\n");
        }
        desc.detailed.push_str(trimmed);
    }
}

/// Parse a `<parameteritem>` into a `ParamDoc`.
fn parse_param_item(item: Node) -> ParamDoc {
    let name_list = item
        .children()
        .find(|n| n.has_tag_name("parameternamelist"));
    let desc_node = item
        .children()
        .find(|n| n.has_tag_name("parameterdescription"));

    let mut direction = String::new();
    let mut name = String::new();

    if let Some(nl) = name_list {
        for pn in nl.children().filter(|n| n.has_tag_name("parametername")) {
            direction = pn
                .attribute("direction")
                .unwrap_or_default()
                .to_string();
            name = collect_text_recursive(pn).trim().to_string();
        }
    }

    let description = desc_node
        .map(|n| collect_text_recursive(n).trim().to_string())
        .unwrap_or_default();

    ParamDoc {
        direction,
        name,
        description,
    }
}

/// Parse a `<programlisting>` element into a `CodeBlock`.
fn parse_program_listing(node: Node) -> CodeBlock {
    let mut content = String::new();
    let mut language = String::new();

    // Try to detect language from filename attribute
    if let Some(filename) = node.attribute("filename") {
        if filename.ends_with(".c") || filename.ends_with(".h") {
            language = "c".to_string();
        } else if filename.ends_with(".cpp") || filename.ends_with(".hpp") {
            language = "cpp".to_string();
        } else if filename.ends_with(".go") {
            language = "go".to_string();
        } else if filename.ends_with(".ts") {
            language = "typescript".to_string();
        }
    }

    for codeline in node.children().filter(|n| n.has_tag_name("codeline")) {
        if !content.is_empty() {
            content.push('\n');
        }
        content.push_str(&collect_text_recursive(codeline));
    }

    CodeBlock { language, content }
}

/// Recursively collect all text content from a node and its descendants.
pub fn collect_text_recursive(node: Node) -> String {
    let mut text = String::new();
    for child in node.children() {
        if child.is_text() {
            text.push_str(child.text().unwrap_or_default());
        } else {
            text.push_str(&collect_text_recursive(child));
        }
    }
    text
}
