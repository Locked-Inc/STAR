// Prevents additional console window on Windows in release for GUI mode
// Note: Always show console since we support both CLI and GUI modes
use clap::Parser;

fn main() {
    // Check if we have command-line arguments (excluding the program name)
    let args: Vec<String> = std::env::args().collect();

    if args.len() > 1 {
        // CLI mode - we have arguments
        match app_lib::cli::Cli::try_parse() {
            Ok(cli) => {
                if let Err(e) = app_lib::cli::run_cli(cli) {
                    eprintln!("Error: {:#}", e);
                    std::process::exit(1);
                }
            }
            Err(e) => {
                // Print clap's error (includes help/version)
                eprintln!("{}", e);
                std::process::exit(if e.use_stderr() { 1 } else { 0 });
            }
        }
    } else {
        // GUI mode - no arguments, run Tauri
        app_lib::run();
    }
}
