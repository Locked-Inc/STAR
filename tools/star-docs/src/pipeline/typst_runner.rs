//! Shell out to `typst` to compile PDF from Typst source.

use std::path::Path;
use std::process::Command;

/// Compile a Typst source file to PDF.
pub fn compile_typst(main_typ: &Path, output_pdf: &Path) -> anyhow::Result<()> {
    if let Some(parent) = output_pdf.parent() {
        std::fs::create_dir_all(parent)?;
    }

    let output = Command::new("typst")
        .args(["compile"])
        .arg(main_typ)
        .arg(output_pdf)
        .output()
        .map_err(|e| anyhow::anyhow!("Failed to run typst: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        anyhow::bail!("typst compile failed:\n{}", stderr);
    }

    tracing::info!("PDF compiled: {}", output_pdf.display());
    Ok(())
}
