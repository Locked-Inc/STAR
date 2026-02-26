//! Pipeline orchestration: coordinate XML generation, parsing, Typst
//! emission, and PDF compilation.

pub mod doxygen_runner;
pub mod dot_runner;
pub mod typst_runner;

use std::path::Path;

use tracing::info;

use crate::config::Config;
use crate::typst_emitter::DocumentEmitter;

/// Run the full build pipeline: XML -> parse -> Typst -> PDF.
pub fn run_full_pipeline(config: &Config) -> anyhow::Result<()> {
    info!("Starting full documentation pipeline");

    // Step 1: Generate Doxygen XML for configured subsystems
    run_generate_xml(config)?;

    // Step 2: Parse XML and emit Typst files
    run_generate_typst(config)?;

    // Step 3: Compile Typst to PDF
    run_compile_pdf(config)?;

    info!("Pipeline complete. Output: {}", config.output.path);
    Ok(())
}

/// Generate Doxygen XML for all configured subsystems.
pub fn run_generate_xml(config: &Config) -> anyhow::Result<()> {
    info!("Generating Doxygen XML");

    for subsystem in &config.subsystem {
        if let Some(doxyfile) = &subsystem.doxyfile {
            let doxyfile_path = Path::new(doxyfile);
            if doxyfile_path.exists() {
                info!("Running Doxygen for: {}", subsystem.name);
                doxygen_runner::run_doxygen(doxyfile_path)?;
            } else {
                tracing::warn!(
                    "Doxyfile not found for {}: {}",
                    subsystem.name,
                    doxyfile
                );
            }
        }
    }

    Ok(())
}

/// Parse all documentation sources and emit Typst files.
pub fn run_generate_typst(config: &Config) -> anyhow::Result<()> {
    info!("Generating Typst source files");

    let output_dir = Path::new(&config.output.typst_dir);
    let emitter = DocumentEmitter::new(config, output_dir);
    emitter.generate()?;

    Ok(())
}

/// Compile Typst source to PDF.
pub fn run_compile_pdf(config: &Config) -> anyhow::Result<()> {
    info!("Compiling PDF");

    let main_typ = Path::new(&config.output.typst_dir).join("main.typ");
    let output_pdf = Path::new(&config.output.path);

    if main_typ.exists() {
        typst_runner::compile_typst(&main_typ, output_pdf)?;
        info!("PDF generated: {}", output_pdf.display());
    } else {
        anyhow::bail!(
            "main.typ not found at {}. Run 'generate-typst' first.",
            main_typ.display()
        );
    }

    Ok(())
}

/// Convert existing LaTeX files to Typst using pandoc.
pub fn run_convert_latex(config: &Config) -> anyhow::Result<()> {
    info!("Converting LaTeX to Typst");

    let prose = config
        .prose
        .as_ref()
        .ok_or_else(|| anyhow::anyhow!("No [prose] section in config"))?;

    let output_dir = Path::new(&prose.sections_dir);
    std::fs::create_dir_all(output_dir)?;

    // Look for LaTeX source files in docs/sections/
    let latex_dir = Path::new("docs/sections");
    if !latex_dir.is_dir() {
        anyhow::bail!("LaTeX source directory not found: {}", latex_dir.display());
    }

    let mut entries: Vec<_> = std::fs::read_dir(latex_dir)?
        .filter_map(|e| e.ok())
        .filter(|e| {
            e.path()
                .extension()
                .is_some_and(|ext| ext == "tex")
        })
        .collect();
    entries.sort_by_key(|e| e.file_name());

    for entry in entries {
        let tex_path = entry.path();
        let stem = tex_path.file_stem().unwrap().to_string_lossy();
        let typ_path = output_dir.join(format!("{}.typ", stem));

        info!("Converting {} -> {}", tex_path.display(), typ_path.display());

        let status = std::process::Command::new("pandoc")
            .args([
                "-f", "latex",
                "-t", "typst",
                "--wrap=none",
            ])
            .arg(&tex_path)
            .arg("-o")
            .arg(&typ_path)
            .status()?;

        if !status.success() {
            tracing::warn!(
                "pandoc conversion failed for {}",
                tex_path.display()
            );
        }
    }

    info!(
        "Conversion complete. Output in {}",
        output_dir.display()
    );
    Ok(())
}

/// Check that all required tools are installed and config is valid.
pub fn run_check(config: &Config) -> anyhow::Result<()> {
    info!("Checking tools and configuration");

    let tools = [
        ("doxygen", "doxygen --version"),
        ("dot", "dot -V"),
        ("typst", "typst --version"),
        ("pandoc", "pandoc --version"),
    ];

    let mut all_ok = true;
    for (name, cmd) in &tools {
        let parts: Vec<&str> = cmd.split_whitespace().collect();
        match std::process::Command::new(parts[0])
            .args(&parts[1..])
            .output()
        {
            Ok(output) if output.status.success() => {
                let version = String::from_utf8_lossy(&output.stdout);
                let first_line = version.lines().next().unwrap_or("unknown");
                info!("[OK] {}: {}", name, first_line.trim());
            }
            Ok(_) => {
                tracing::error!("[FAIL] {} found but returned error", name);
                all_ok = false;
            }
            Err(_) => {
                tracing::error!("[FAIL] {} not found in PATH", name);
                all_ok = false;
            }
        }
    }

    // Check config
    info!("Project: {}", config.project.name);
    info!("Subsystems: {}", config.subsystem.len());
    for sub in &config.subsystem {
        info!("  - {} ({})", sub.name, sub.language);
    }

    if all_ok {
        info!("All checks passed");
        Ok(())
    } else {
        anyhow::bail!("Some tools are missing")
    }
}
