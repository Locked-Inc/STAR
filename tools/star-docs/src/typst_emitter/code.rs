//! Syntax-highlighted code block emission for Typst.

use crate::doxygen::description::CodeBlock;

/// Emit a Typst raw code block with optional language tag.
pub fn emit_code_block(block: &CodeBlock) -> String {
    let lang = if block.language.is_empty() {
        "c" // Default to C for STAR project
    } else {
        &block.language
    };

    format!("```{}\n{}\n```\n", lang, block.content)
}

/// Emit an inline code snippet.
pub fn emit_inline_code(code: &str) -> String {
    format!("`{}`", code)
}

/// Emit a function signature as a styled code block.
pub fn emit_signature(definition: &str, argsstring: &str) -> String {
    let sig = if definition.is_empty() {
        argsstring.to_string()
    } else if argsstring.is_empty() {
        definition.to_string()
    } else {
        format!("{}{}", definition, argsstring)
    };

    format!("#func-sig(\"{}\")\n", escape_typst_string(&sig))
}

/// Escape special characters for Typst string literals.
pub fn escape_typst_string(s: &str) -> String {
    s.replace('\\', "\\\\")
        .replace('"', "\\\"")
        .replace('#', "\\#")
}

/// Escape special characters for Typst content (not inside strings).
pub fn escape_typst_content(s: &str) -> String {
    s.replace('#', "\\#")
        .replace('<', "\\<")
        .replace('>', "\\>")
        .replace('@', "\\@")
        .replace('$', "\\$")
}
