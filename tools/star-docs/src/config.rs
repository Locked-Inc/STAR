//! Configuration file parsing for `star-docs.toml`.

use serde::Deserialize;
use std::path::Path;

/// Top-level configuration.
#[derive(Debug, Deserialize)]
pub struct Config {
    pub project: ProjectConfig,
    pub output: OutputConfig,
    #[serde(default)]
    pub subsystem: Vec<SubsystemConfig>,
    pub prose: Option<ProseConfig>,
}

#[derive(Debug, Deserialize)]
pub struct ProjectConfig {
    pub name: String,
    #[serde(default = "default_version")]
    pub version: String,
}

fn default_version() -> String {
    "1.0.0".to_string()
}

#[derive(Debug, Deserialize)]
pub struct OutputConfig {
    pub path: String,
    pub typst_dir: String,
}

#[derive(Debug, Deserialize)]
pub struct SubsystemConfig {
    pub name: String,
    pub language: String,
    pub doxyfile: Option<String>,
    pub xml_dir: Option<String>,
    pub go_doc_json: Option<String>,
    pub source_dir: Option<String>,
}

#[derive(Debug, Deserialize)]
pub struct ProseConfig {
    pub sections_dir: String,
}

impl Config {
    /// Load and parse configuration from a TOML file.
    pub fn load(path: &str) -> anyhow::Result<Self> {
        let content = std::fs::read_to_string(path)
            .map_err(|e| anyhow::anyhow!("Failed to read config file '{}': {}", path, e))?;
        let config: Config = toml::from_str(&content)?;
        config.validate()?;
        Ok(config)
    }

    fn validate(&self) -> anyhow::Result<()> {
        for sub in &self.subsystem {
            match sub.language.as_str() {
                "c" | "cpp" => {
                    if sub.xml_dir.is_none() && sub.doxyfile.is_none() {
                        anyhow::bail!(
                            "Subsystem '{}' (language={}) requires xml_dir or doxyfile",
                            sub.name,
                            sub.language
                        );
                    }
                }
                "go" => {
                    if sub.go_doc_json.is_none() && sub.source_dir.is_none() {
                        anyhow::bail!(
                            "Subsystem '{}' (go) requires go_doc_json or source_dir",
                            sub.name
                        );
                    }
                }
                "typescript" | "protobuf" => {
                    if sub.source_dir.is_none() {
                        anyhow::bail!(
                            "Subsystem '{}' (language={}) requires source_dir",
                            sub.name,
                            sub.language
                        );
                    }
                }
                other => {
                    anyhow::bail!("Unknown language '{}' for subsystem '{}'", other, sub.name);
                }
            }
        }

        if let Some(prose) = &self.prose {
            if !Path::new(&prose.sections_dir).is_dir() {
                tracing::warn!(
                    "Prose sections directory '{}' does not exist yet",
                    prose.sections_dir
                );
            }
        }

        Ok(())
    }
}
