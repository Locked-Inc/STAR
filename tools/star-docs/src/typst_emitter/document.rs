//! Top-level document assembly.
//!
//! Orchestrates emission of the complete Typst document: preamble,
//! title page, table of contents, prose chapters, API reference chapters
//! (one per subsystem), and the alphabetical index.

use std::collections::HashMap;
use std::path::{Path, PathBuf};

use rayon::prelude::*;
use tracing::info;

use crate::config::{Config, SubsystemConfig};
use crate::doxygen;
use crate::doxygen::compound::CompoundDef;
use crate::typst_emitter::{file, index_page, template};

/// Main document emitter that coordinates all output.
pub struct DocumentEmitter<'a> {
    config: &'a Config,
    output_dir: PathBuf,
}

impl<'a> DocumentEmitter<'a> {
    pub fn new(config: &'a Config, output_dir: &Path) -> Self {
        Self {
            config,
            output_dir: output_dir.to_path_buf(),
        }
    }

    /// Generate all Typst source files for the complete document.
    pub fn generate(&self) -> anyhow::Result<PathBuf> {
        std::fs::create_dir_all(&self.output_dir)?;
        let graphs_dir = self.output_dir.join("graphs");
        std::fs::create_dir_all(&graphs_dir)?;

        // Generate template
        let preamble = template::generate_preamble(
            &self.config.project.name,
            &self.config.project.version,
        );
        let preamble_path = self.output_dir.join("_template.typ");
        std::fs::write(&preamble_path, &preamble)?;
        info!("Wrote template: {}", preamble_path.display());

        // Collect all subsystem chapter filenames
        let mut chapter_files: Vec<String> = Vec::new();

        // Process each subsystem with Doxygen XML
        for subsystem in &self.config.subsystem {
            match subsystem.language.as_str() {
                "c" | "cpp" => {
                    if let Some(xml_dir) = &subsystem.xml_dir {
                        let xml_path = Path::new(xml_dir);
                        if xml_path.is_dir() {
                            let filename =
                                self.process_doxygen_subsystem(subsystem, xml_path, &graphs_dir)?;
                            chapter_files.push(filename);
                        } else {
                            tracing::warn!(
                                "XML directory not found for {}: {}",
                                subsystem.name,
                                xml_dir
                            );
                            chapter_files.push(self.write_placeholder_chapter(subsystem)?);
                        }
                    } else {
                        chapter_files.push(self.write_placeholder_chapter(subsystem)?);
                    }
                }
                "go" | "typescript" | "protobuf" => {
                    chapter_files.push(self.write_placeholder_chapter(subsystem)?);
                }
                _ => {
                    tracing::warn!("Unknown language: {}", subsystem.language);
                }
            }
        }

        // Generate the main.typ file
        let main_path = self.generate_main_typ(&chapter_files)?;
        info!("Wrote main document: {}", main_path.display());

        Ok(main_path)
    }

    /// Process a Doxygen XML subsystem and emit its chapter .typ file.
    fn process_doxygen_subsystem(
        &self,
        subsystem: &SubsystemConfig,
        xml_dir: &Path,
        graphs_dir: &Path,
    ) -> anyhow::Result<String> {
        info!("Processing Doxygen XML for: {}", subsystem.name);

        // Parse index
        let entries = doxygen::parse_index(xml_dir)?;

        // Parse compounds in parallel
        let compounds: Vec<CompoundDef> = entries
            .par_iter()
            .filter(|e| e.kind == "file")
            .filter_map(|entry| {
                match doxygen::compound::parse_compound_file(xml_dir, &entry.refid) {
                    Ok(c) => Some(c),
                    Err(e) => {
                        tracing::warn!("Failed to parse {}: {}", entry.refid, e);
                        None
                    }
                }
            })
            .collect();

        info!(
            "Parsed {} file compounds for {}",
            compounds.len(),
            subsystem.name
        );

        // Render call graphs
        let all_members: Vec<_> = compounds
            .iter()
            .flat_map(|c| c.sections.iter().flat_map(|s| s.members.iter()))
            .cloned()
            .collect();

        let graph_paths = doxygen::graph::render_member_graphs(&all_members, graphs_dir);
        info!(
            "Rendered {} call graphs for {}",
            graph_paths.len(),
            subsystem.name
        );

        // Emit chapter
        let chapter_content = self.emit_subsystem_chapter(subsystem, &compounds, &graph_paths);
        let filename = format!(
            "api_{}.typ",
            subsystem.name.to_lowercase().replace(' ', "_")
        );
        let chapter_path = self.output_dir.join(&filename);
        std::fs::write(&chapter_path, &chapter_content)?;
        info!("Wrote chapter: {}", chapter_path.display());

        Ok(filename)
    }

    /// Emit the content for a subsystem API reference chapter.
    fn emit_subsystem_chapter(
        &self,
        subsystem: &SubsystemConfig,
        compounds: &[CompoundDef],
        graph_paths: &HashMap<String, PathBuf>,
    ) -> String {
        let mut out = String::new();

        out.push_str(&format!(
            "= {} API Reference\n\n",
            subsystem.name
        ));

        for compound in compounds {
            out.push_str(&file::emit_file_section(compound, graph_paths));
        }

        // Append index for this subsystem
        out.push_str(&index_page::emit_index(compounds));

        out
    }

    /// Write a placeholder chapter for subsystems whose docs aren't yet processed.
    fn write_placeholder_chapter(
        &self,
        subsystem: &SubsystemConfig,
    ) -> anyhow::Result<String> {
        let filename = format!(
            "api_{}.typ",
            subsystem.name.to_lowercase().replace(' ', "_")
        );
        let path = self.output_dir.join(&filename);
        let content = format!(
            "= {} API Reference\n\n\
             _Documentation generation for {} ({}) is pending. \
             Configure the subsystem in `star-docs.toml` to enable._\n",
            subsystem.name, subsystem.name, subsystem.language
        );
        std::fs::write(&path, &content)?;
        info!(
            "Wrote placeholder chapter for {}: {}",
            subsystem.name,
            path.display()
        );
        Ok(filename)
    }

    /// Generate the `main.typ` file that includes all chapters.
    fn generate_main_typ(&self, chapter_files: &[String]) -> anyhow::Result<PathBuf> {
        let mut content = String::new();

        // Import template
        content.push_str("#import \"_template.typ\": *\n\n");

        // Title page
        content.push_str("// Title page\n");
        content.push_str("#align(center)[\n");
        content.push_str("  #v(3cm)\n");
        content.push_str(&format!(
            "  #text(28pt, weight: \"bold\", fill: rgb(\"#1565c0\"))[{}]\n",
            self.config.project.name
        ));
        content.push_str("  #v(1cm)\n");
        content.push_str("  #text(14pt, fill: rgb(\"#666666\"))[Complete System Documentation]\n");
        content.push_str("  #v(0.5cm)\n");
        content.push_str(&format!(
            "  #text(12pt)[Version {}]\n",
            self.config.project.version
        ));
        content.push_str("  #v(0.5cm)\n");
        content.push_str("  #text(10pt, fill: rgb(\"#999999\"))[Generated by star-docs]\n");
        content.push_str("  #v(2cm)\n");
        content.push_str("]\n");
        content.push_str("#pagebreak()\n\n");

        // Table of contents
        content.push_str("// Table of Contents\n");
        content.push_str("#outline(depth: 3)\n");
        content.push_str("#pagebreak()\n\n");

        // Include prose sections if configured
        if let Some(prose) = &self.config.prose {
            let prose_dir = Path::new(&prose.sections_dir);
            if prose_dir.is_dir() {
                let mut typ_files: Vec<_> = std::fs::read_dir(prose_dir)?
                    .filter_map(|e| e.ok())
                    .filter(|e| {
                        e.path()
                            .extension()
                            .is_some_and(|ext| ext == "typ")
                    })
                    .filter(|e| {
                        // Skip template and main
                        let name = e.file_name().to_string_lossy().to_string();
                        !name.starts_with('_') && name != "main.typ"
                    })
                    .map(|e| e.file_name().to_string_lossy().to_string())
                    .collect();
                typ_files.sort();

                for f in &typ_files {
                    let rel_path = format!("{}/{}", prose.sections_dir, f);
                    content.push_str(&format!(
                        "#include \"{}\"\n",
                        rel_path
                    ));
                }
                content.push('\n');
            }
        }

        // Include API reference chapters
        for filename in chapter_files {
            content.push_str(&format!("#include \"{}\"\n", filename));
        }

        let main_path = self.output_dir.join("main.typ");
        std::fs::write(&main_path, &content)?;
        Ok(main_path)
    }
}
