//! STAR documentation pipeline CLI.
//!
//! Converts Doxygen XML output to Typst source files and compiles them into a
//! unified PDF covering all STAR subsystems.

#![allow(dead_code)]

use clap::{Parser, Subcommand};
use tracing::info;

mod config;
mod doxygen;
mod languages;
mod pipeline;
mod typst_emitter;

#[derive(Parser)]
#[command(name = "star-docs")]
#[command(about = "STAR documentation pipeline: Doxygen XML -> Typst -> PDF")]
#[command(version)]
struct Cli {
    /// Path to configuration file
    #[arg(short, long, default_value = "star-docs.toml")]
    config: String,

    /// Enable verbose logging
    #[arg(short, long)]
    verbose: bool,

    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Full pipeline: doxygen -> parse -> typst -> PDF
    Build,
    /// Only run doxygen XML generation
    GenerateXml,
    /// Only parse XML and emit .typ files
    GenerateTypst,
    /// Only run typst compile
    CompilePdf,
    /// Convert existing .tex files to .typ (one-time helper)
    ConvertLatex,
    /// Validate config and check tools installed
    Check,
}

fn main() -> anyhow::Result<()> {
    let cli = Cli::parse();

    let log_level = if cli.verbose { "debug" } else { "info" };
    tracing_subscriber::fmt()
        .with_env_filter(log_level)
        .init();

    let cfg = config::Config::load(&cli.config)?;
    info!("Loaded config for project: {}", cfg.project.name);

    match cli.command {
        Commands::Build => pipeline::run_full_pipeline(&cfg)?,
        Commands::GenerateXml => pipeline::run_generate_xml(&cfg)?,
        Commands::GenerateTypst => pipeline::run_generate_typst(&cfg)?,
        Commands::CompilePdf => pipeline::run_compile_pdf(&cfg)?,
        Commands::ConvertLatex => pipeline::run_convert_latex(&cfg)?,
        Commands::Check => pipeline::run_check(&cfg)?,
    }

    Ok(())
}
