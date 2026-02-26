//! Shell out to `dot` (Graphviz) to render call graphs.

use std::path::Path;
use std::process::Command;

/// Check if the `dot` command is available.
pub fn dot_available() -> bool {
    Command::new("dot")
        .arg("-V")
        .output()
        .is_ok_and(|o| o.status.success())
}

/// Render a DOT file to SVG.
pub fn render_dot_file(dot_path: &Path, svg_path: &Path) -> anyhow::Result<()> {
    let output = Command::new("dot")
        .args(["-Tsvg", "-o"])
        .arg(svg_path)
        .arg(dot_path)
        .output()
        .map_err(|e| anyhow::anyhow!("Failed to run dot: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        anyhow::bail!("dot failed for {}: {}", dot_path.display(), stderr);
    }

    Ok(())
}
