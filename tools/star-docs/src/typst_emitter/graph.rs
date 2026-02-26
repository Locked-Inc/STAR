//! Embed SVG call graph images in Typst output.

use std::path::Path;

/// Emit a Typst image reference for a call graph SVG.
pub fn emit_callgraph_image(svg_path: &Path, caption: &str) -> String {
    let rel_path = svg_path.display();
    format!(
        r#"#figure(
  image("{rel_path}", width: 90%),
  caption: [{caption}],
)
"#
    )
}

/// Emit a placeholder when no call graph is available.
pub fn emit_no_callgraph() -> String {
    String::new()
}
