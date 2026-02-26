//! Shell out to `doxygen` to generate XML output.

use std::path::Path;
use std::process::Command;

/// Run Doxygen with the given Doxyfile to generate XML output.
pub fn run_doxygen(doxyfile: &Path) -> anyhow::Result<()> {
    let working_dir = doxyfile
        .parent()
        .ok_or_else(|| anyhow::anyhow!("Invalid Doxyfile path"))?;

    let output = Command::new("doxygen")
        .arg(doxyfile.file_name().unwrap())
        .current_dir(working_dir)
        .output()
        .map_err(|e| anyhow::anyhow!("Failed to run doxygen: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        // Doxygen warnings are not fatal
        if stderr.contains("error") {
            tracing::warn!("Doxygen reported errors:\n{}", stderr);
        }
    }

    let stdout = String::from_utf8_lossy(&output.stdout);
    let stderr_str = String::from_utf8_lossy(&output.stderr);
    let warnings: Vec<&str> = stdout
        .lines()
        .chain(stderr_str.lines())
        .filter(|l| l.contains("warning"))
        .collect();

    if !warnings.is_empty() {
        tracing::warn!("Doxygen produced {} warnings", warnings.len());
    }

    tracing::info!("Doxygen completed for {}", doxyfile.display());
    Ok(())
}
