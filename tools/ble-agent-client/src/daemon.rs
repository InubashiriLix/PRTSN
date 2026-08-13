use crate::ble::{self, AgentClient};
use crate::cli::{Cli, DaemonConfig};
use crate::codex_hook::DaemonEvent;
use crate::protocol::{self, NO_AGENT};
use crate::session::SessionAgent;
use crate::transcript::detect_interrupted_turns;
use anyhow::{bail, ensure, Context, Result};
use std::collections::HashMap;
use std::fs;
use std::os::unix::fs::PermissionsExt as _;
use std::path::PathBuf;
use std::time::Duration;
use tokio::io::AsyncReadExt as _;
use tokio::net::{UnixListener, UnixStream};
use tokio::sync::mpsc;

pub async fn run(cli: &Cli) -> Result<()> {
    let socket = cli.socket_path();
    prepare_socket(&socket).await?;
    let listener = UnixListener::bind(&socket).with_context(|| {
        format!(
            "binding BLE Agent daemon socket {} failed",
            socket.display()
        )
    })?;
    fs::set_permissions(&socket, fs::Permissions::from_mode(0o600)).with_context(|| {
        format!(
            "securing BLE Agent daemon socket {} failed",
            socket.display()
        )
    })?;
    println!("BLE Agent daemon listening at {}", socket.display());

    let (sender, receiver) = mpsc::channel(64);
    tokio::spawn(ble_worker(receiver, cli.daemon_config()));
    loop {
        let event = receive_event(&listener).await?;
        if sender.send(event).await.is_err() {
            bail!("BLE Agent worker stopped unexpectedly");
        }
    }
}

async fn prepare_socket(socket: &PathBuf) -> Result<()> {
    if let Some(parent) = socket.parent() {
        fs::create_dir_all(parent)
            .with_context(|| format!("creating socket directory {} failed", parent.display()))?;
    }
    if socket.exists() {
        if UnixStream::connect(socket.as_path()).await.is_ok() {
            bail!(
                "BLE Agent daemon is already running at {}",
                socket.display()
            );
        }
        fs::remove_file(socket)
            .with_context(|| format!("removing stale socket {} failed", socket.display()))?;
    }
    Ok(())
}

async fn receive_event(listener: &UnixListener) -> Result<DaemonEvent> {
    loop {
        let (mut stream, _) = listener
            .accept()
            .await
            .context("accepting a hook event failed")?;
        let mut payload = Vec::new();
        stream
            .read_to_end(&mut payload)
            .await
            .context("reading a hook event failed")?;
        match serde_json::from_slice(&payload) {
            Ok(event) => return Ok(event),
            Err(error) => eprintln!("ignored malformed hook event: {error}"),
        }
    }
}

async fn ble_worker(mut receiver: mpsc::Receiver<DaemonEvent>, config: DaemonConfig) {
    let mut sessions = HashMap::<String, SessionAgent>::new();
    let mut client = None;
    let mut retry = tokio::time::interval(Duration::from_secs(2));
    retry.set_missed_tick_behavior(tokio::time::MissedTickBehavior::Skip);

    loop {
        tokio::select! {
            event = receiver.recv() => {
                let Some(event) = event else { return; };
                let agent = sessions
                    .entry(event.session_id.clone())
                    .or_insert_with(|| SessionAgent::new(&event.session_id));
                agent.set_transcript(event.transcript_path);
                agent.apply(event.activity);
            }
            _ = retry.tick() => {}
        }

        if sessions.is_empty() {
            continue;
        }
        detect_interrupted_turns(&mut sessions);
        if let Err(error) = synchronize(&mut sessions, &mut client, &config).await {
            eprintln!("BLE synchronization failed: {error:#}");
            if let Some(existing) = client.take() {
                existing.disconnect().await;
            }
            sessions
                .values_mut()
                .for_each(SessionAgent::reset_connection);
        }
    }
}

async fn synchronize(
    sessions: &mut HashMap<String, SessionAgent>,
    client: &mut Option<AgentClient>,
    config: &DaemonConfig,
) -> Result<()> {
    if !client_is_connected(client).await {
        let adapter = ble::default_adapter().await?;
        *client = Some(
            AgentClient::connect(
                &adapter,
                &config.device_name,
                config.scan_timeout,
                config.response_timeout,
            )
            .await?,
        );
        sessions
            .values_mut()
            .for_each(SessionAgent::reset_connection);
    }
    let client = client.as_ref().expect("connected client was installed");
    let mut removed = Vec::new();

    for (session_id, agent) in sessions.iter_mut().filter(|(_, agent)| agent.dirty) {
        if agent.slot.is_none() {
            let response = client
                .execute(&protocol::register(agent.key, &agent.name)?)
                .await?;
            ensure!(
                response.agent_id != NO_AGENT,
                "register succeeded without an Agent id"
            );
            agent.slot = Some(response.agent_id);
        }
        let slot = agent.slot.expect("registered Agent has a slot");
        if agent.remove {
            client.execute(&protocol::unregister(slot)?).await?;
            removed.push(session_id.clone());
            continue;
        }
        client
            .execute(&protocol::set_text(slot, agent.text)?)
            .await?;
        client
            .execute(&protocol::set_state(slot, agent.state)?)
            .await?;
        agent.dirty = false;
    }
    for session_id in removed {
        sessions.remove(&session_id);
    }
    Ok(())
}

async fn client_is_connected(client: &Option<AgentClient>) -> bool {
    match client {
        Some(client) => client.is_connected().await,
        None => false,
    }
}
