use clap::{Parser, Subcommand};
use std::path::PathBuf;
use std::time::Duration;

const DEVICE_NAME: &str = "PRTN-AgentPanel";

#[derive(Parser, Debug)]
#[command(
    version,
    about = "PRTN Agent Panel BLE protocol and hardware test client"
)]
pub struct Cli {
    /// BLE local name to find.
    #[arg(long = "device-name", global = true, default_value = DEVICE_NAME)]
    pub device_name: String,

    /// Scan timeout in seconds.
    #[arg(long, global = true, default_value_t = 10)]
    pub scan_timeout: u64,

    /// Command response timeout in seconds.
    #[arg(long, global = true, default_value_t = 5)]
    pub response_timeout: u64,

    /// Override the local socket used by Codex hooks and the BLE daemon.
    #[arg(long, global = true)]
    pub socket: Option<PathBuf>,

    #[command(subcommand)]
    pub command: Command,
}

impl Cli {
    pub fn daemon_config(&self) -> DaemonConfig {
        DaemonConfig {
            device_name: self.device_name.clone(),
            scan_timeout: Duration::from_secs(self.scan_timeout),
            response_timeout: Duration::from_secs(self.response_timeout),
        }
    }

    pub fn socket_path(&self) -> PathBuf {
        self.socket.clone().unwrap_or_else(|| {
            let directory = std::env::var_os("XDG_RUNTIME_DIR")
                .map(PathBuf::from)
                .unwrap_or_else(std::env::temp_dir);
            directory.join("prtn-ble-agent.sock")
        })
    }
}

#[derive(Clone, Debug)]
pub struct DaemonConfig {
    pub device_name: String,
    pub scan_timeout: Duration,
    pub response_timeout: Duration,
}

#[derive(Subcommand, Debug)]
pub enum Command {
    /// List nearby BLE devices and mark matching Agent Panels.
    Scan,
    /// Register an Agent and print its allocated slot ID.
    Register {
        #[arg(long)]
        key: u32,
        #[arg(long)]
        name: String,
    },
    /// Change the state of an allocated Agent slot.
    State {
        #[arg(long)]
        id: u8,
        #[arg(long)]
        state: crate::protocol::AgentState,
    },
    /// Replace the display text of an allocated Agent slot.
    Text {
        #[arg(long)]
        id: u8,
        #[arg(long)]
        text: String,
    },
    /// Remove an Agent slot.
    Unregister {
        #[arg(long)]
        id: u8,
    },
    /// Run the complete seven-Agent visual and protocol acceptance test.
    Demo {
        /// Delay between visible demo steps.
        #[arg(long, default_value_t = 500)]
        step_ms: u64,
    },
    /// Keep one BLE connection open and accept Codex lifecycle events locally.
    Serve,
    /// Read one Codex hook JSON object from stdin and deliver it to the daemon.
    Hook,
}
