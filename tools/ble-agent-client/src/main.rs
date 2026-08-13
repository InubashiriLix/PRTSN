mod ble;
mod cli;
mod codex_hook;
mod commands;
mod daemon;
mod protocol;
mod session;
mod transcript;

use anyhow::Result;
use clap::Parser as _;
use cli::{Cli, Command};

#[tokio::main]
async fn main() -> Result<()> {
    let cli = Cli::parse();
    match cli.command {
        Command::Serve => daemon::run(&cli).await,
        Command::Hook => codex_hook::forward(&cli).await,
        _ => commands::run(&cli).await,
    }
}
