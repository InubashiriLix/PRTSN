use crate::cli::Cli;
use crate::protocol::AgentState;
use anyhow::{bail, Context, Result};
use serde::{Deserialize, Serialize};
use std::fs::OpenOptions;
use std::io::Read as _;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::time::Duration;
use tokio::io::AsyncWriteExt as _;
use tokio::net::UnixStream;
use tokio::time::sleep;

#[derive(Debug, Deserialize)]
struct HookInput {
    session_id: String,
    hook_event_name: String,
    #[serde(default)]
    transcript_path: Option<PathBuf>,
    #[serde(default)]
    tool_name: Option<String>,
}

#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
#[serde(rename_all = "snake_case")]
pub enum Activity {
    Idle,
    Working,
    WaitPermission,
    WaitOption,
    Done,
    Remove,
}

impl Activity {
    fn from_hook(input: &HookInput) -> Option<Self> {
        match input.hook_event_name.as_str() {
            "SessionStart" => Some(Self::Idle),
            "UserPromptSubmit" | "PostToolUse" => Some(Self::Working),
            "PermissionRequest" => Some(Self::WaitPermission),
            "PreToolUse" if input.tool_name.as_deref() == Some("request_user_input") => {
                Some(Self::WaitOption)
            }
            "PreToolUse" => Some(Self::Working),
            "Stop" => Some(Self::Done),
            "SessionEnd" => Some(Self::Remove),
            _ => None,
        }
    }

    pub fn panel_state(self) -> Option<AgentState> {
        match self {
            Self::Idle => Some(AgentState::Idle),
            Self::Working => Some(AgentState::Working),
            Self::WaitPermission => Some(AgentState::WaitPermission),
            Self::WaitOption => Some(AgentState::WaitOption),
            Self::Done => Some(AgentState::Done),
            Self::Remove => None,
        }
    }

    pub fn display_text(self) -> &'static str {
        match self {
            Self::Idle => "idle",
            Self::Working => "working",
            Self::WaitPermission => "approval",
            Self::WaitOption => "your input",
            Self::Done => "done",
            Self::Remove => "",
        }
    }
}

#[derive(Debug, Deserialize, Serialize)]
pub struct DaemonEvent {
    pub session_id: String,
    pub activity: Activity,
    pub transcript_path: Option<PathBuf>,
}

pub async fn forward(cli: &Cli) -> Result<()> {
    let mut input = Vec::new();
    std::io::stdin()
        .read_to_end(&mut input)
        .context("reading Codex hook JSON from stdin failed")?;
    let hook: HookInput =
        serde_json::from_slice(&input).context("decoding Codex hook JSON failed")?;
    let Some(activity) = Activity::from_hook(&hook) else {
        return Ok(());
    };
    let payload = serde_json::to_vec(&DaemonEvent {
        session_id: hook.session_id,
        activity,
        transcript_path: hook.transcript_path,
    })?;
    let socket = cli.socket_path();

    if send(&socket, &payload).await.is_ok() {
        return Ok(());
    }
    spawn_daemon(cli, &socket)?;
    for _ in 0..50 {
        sleep(Duration::from_millis(40)).await;
        if send(&socket, &payload).await.is_ok() {
            return Ok(());
        }
    }
    bail!(
        "BLE Agent daemon did not create {} within two seconds; see {}",
        socket.display(),
        daemon_log_path(&socket).display()
    )
}

pub fn daemon_log_path(socket: &Path) -> PathBuf {
    socket.with_extension("log")
}

async fn send(socket: &Path, payload: &[u8]) -> Result<()> {
    let mut stream = UnixStream::connect(socket).await.with_context(|| {
        format!(
            "connecting to BLE Agent daemon at {} failed",
            socket.display()
        )
    })?;
    stream
        .write_all(payload)
        .await
        .context("writing a Codex event to the BLE Agent daemon failed")?;
    stream
        .shutdown()
        .await
        .context("closing the BLE Agent daemon socket failed")?;
    Ok(())
}

fn spawn_daemon(cli: &Cli, socket: &Path) -> Result<()> {
    let executable =
        std::env::current_exe().context("locating the BLE Agent client executable failed")?;
    let log_path = daemon_log_path(socket);
    let log = OpenOptions::new()
        .create(true)
        .append(true)
        .open(&log_path)
        .with_context(|| format!("opening daemon log {} failed", log_path.display()))?;
    let error_log = log
        .try_clone()
        .context("cloning the BLE Agent daemon log failed")?;

    Command::new(executable)
        .arg("--device-name")
        .arg(&cli.device_name)
        .arg("--scan-timeout")
        .arg(cli.scan_timeout.to_string())
        .arg("--response-timeout")
        .arg(cli.response_timeout.to_string())
        .arg("--socket")
        .arg(socket)
        .arg("serve")
        .stdin(Stdio::null())
        .stdout(Stdio::from(log))
        .stderr(Stdio::from(error_log))
        .spawn()
        .context("starting the BLE Agent daemon failed")?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn hook(event: &str, tool: Option<&str>) -> HookInput {
        HookInput {
            session_id: "thr_test".into(),
            hook_event_name: event.into(),
            transcript_path: None,
            tool_name: tool.map(str::to_owned),
        }
    }

    #[test]
    fn maps_codex_lifecycle_to_panel_activity() {
        assert!(matches!(
            Activity::from_hook(&hook("SessionStart", None)),
            Some(Activity::Idle)
        ));
        assert!(matches!(
            Activity::from_hook(&hook("UserPromptSubmit", None)),
            Some(Activity::Working)
        ));
        assert!(matches!(
            Activity::from_hook(&hook("PermissionRequest", None)),
            Some(Activity::WaitPermission)
        ));
        assert!(matches!(
            Activity::from_hook(&hook("PreToolUse", Some("request_user_input"))),
            Some(Activity::WaitOption)
        ));
        assert!(matches!(
            Activity::from_hook(&hook("PostToolUse", Some("request_user_input"))),
            Some(Activity::Working)
        ));
        assert!(matches!(
            Activity::from_hook(&hook("PreToolUse", Some("Bash"))),
            Some(Activity::Working)
        ));
        assert!(matches!(
            Activity::from_hook(&hook("Stop", None)),
            Some(Activity::Done)
        ));
        assert!(matches!(
            Activity::from_hook(&hook("SessionEnd", None)),
            Some(Activity::Remove)
        ));
    }
}
