use clap::Parser;
use pe_save_editor::cli::{run_cli_command, Cli, Command};
use pe_save_editor::gui::run_gui;

fn main() -> anyhow::Result<()> {
    let cli = Cli::parse();
    let default_pack = std::env::current_exe()?.with_file_name("editor-definitions.json");
    if let Some(path) = cli
        .definitions
        .as_deref()
        .or_else(|| default_pack.is_file().then_some(default_pack.as_path()))
    {
        pe_save_editor::definitions::load(path)?;
    }
    match cli.command {
        None => run_gui(cli.path),
        Some(Command::Gui { path }) => run_gui(path.or(cli.path)),
        Some(other) => run_cli_command(other),
    }
}
