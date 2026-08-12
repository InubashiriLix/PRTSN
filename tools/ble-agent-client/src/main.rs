mod protocol;

use anyhow::{bail, ensure, Context, Result};
use bluez_async::BluetoothSession;
use btleplug::api::{
    Central, Characteristic, Manager as _, Peripheral as _, ScanFilter, WriteType,
};
use btleplug::platform::{Adapter, Manager, Peripheral};
use clap::{Parser, Subcommand};
use futures_util::StreamExt;
use protocol::{AgentState, Response, AGENT_COUNT, NO_AGENT};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs::{self, OpenOptions};
use std::io::{BufRead as _, BufReader, Read as _, Seek as _, SeekFrom};
use std::os::unix::fs::PermissionsExt as _;
use std::path::{Path, PathBuf};
use std::process::{Command as ProcessCommand, Stdio};
use std::time::Duration;
use tokio::io::{AsyncReadExt as _, AsyncWriteExt as _};
use tokio::net::{UnixListener, UnixStream};
use tokio::sync::mpsc;
use tokio::time::{sleep, timeout, Instant};
use uuid::{uuid, Uuid};

const DEVICE_NAME: &str = "PRTN-AgentPanel";
const SERVICE_UUID: Uuid = uuid!("6e291bc2-80c8-484d-a505-49509c3c868e");
const COMMAND_UUID: Uuid = uuid!("b6c66dd2-4047-473c-93f3-d97d9405330c");
const EVENT_UUID: Uuid = uuid!("1c69273f-d0fb-447a-90c1-0c472a0b7b53");

#[derive(Parser, Debug)]
#[command(
    version,
    about = "PRTN Agent Panel BLE protocol and hardware test client"
)]
struct Cli {
    /// BLE local name to find.
    #[arg(long = "device-name", global = true, default_value = DEVICE_NAME)]
    device_name: String,

    /// Scan timeout in seconds.
    #[arg(long, global = true, default_value_t = 10)]
    scan_timeout: u64,

    /// Command response timeout in seconds.
    #[arg(long, global = true, default_value_t = 5)]
    response_timeout: u64,

    /// Override the local socket used by Codex hooks and the BLE daemon.
    #[arg(long, global = true)]
    socket: Option<PathBuf>,

    #[command(subcommand)]
    command: Command,
}

#[derive(Subcommand, Debug)]
enum Command {
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
        state: AgentState,
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

struct AgentClient {
    peripheral: Peripheral,
    command: Characteristic,
    event: Characteristic,
    response_timeout: Duration,
}

impl AgentClient {
    async fn connect(
        adapter: &Adapter,
        device_name: &str,
        scan_timeout: Duration,
        response_timeout: Duration,
    ) -> Result<Self> {
        let peripheral = find_panel(adapter, device_name, scan_timeout).await?;
        let address = peripheral.address();
        println!("connecting to {device_name} ({address})...");
        peripheral
            .connect()
            .await
            .context("BLE connection failed")?;

        let initialization = async {
            pair_with_bluez(&peripheral, response_timeout).await?;
            peripheral
                .discover_services()
                .await
                .context("GATT service discovery failed")?;

            let characteristics = peripheral.characteristics();
            let command = characteristics
                .iter()
                .find(|item| item.uuid == COMMAND_UUID)
                .cloned()
                .context("Command RX characteristic was not found")?;
            let event = characteristics
                .iter()
                .find(|item| item.uuid == EVENT_UUID)
                .cloned()
                .context("Event TX characteristic was not found")?;
            peripheral
                .subscribe(&event)
                .await
                .context("subscribing to Event TX indications failed")?;
            Ok::<_, anyhow::Error>((command, event))
        }
        .await;
        let (command, event) = match initialization {
            Ok(characteristics) => characteristics,
            Err(error) => {
                let _ = peripheral.disconnect().await;
                return Err(error);
            }
        };
        println!("connected, paired, and subscribed");

        Ok(Self {
            peripheral,
            command,
            event,
            response_timeout,
        })
    }

    async fn execute(&self, frame: &[u8]) -> Result<Response> {
        let expected = protocol::opcode(frame)?;
        let mut notifications = self
            .peripheral
            .notifications()
            .await
            .context("opening notification stream failed")?;
        self.peripheral
            .write(&self.command, frame, WriteType::WithResponse)
            .await
            .with_context(|| format!("writing {:?} failed", expected))?;

        let deadline = Instant::now() + self.response_timeout;
        loop {
            let remaining = deadline.saturating_duration_since(Instant::now());
            let notification = timeout(remaining, notifications.next())
                .await
                .with_context(|| format!("timed out waiting for {:?} response", expected))?
                .context("BLE notification stream closed")?;
            if notification.uuid != self.event.uuid {
                eprintln!("warning: ignored notification from {}", notification.uuid);
                continue;
            }
            match Response::decode(&notification.value) {
                Ok(response) if response.opcode == expected as u8 => {
                    return response.require_ok(expected)
                }
                Ok(response) => {
                    eprintln!(
                        "warning: ignored response for opcode 0x{:02x} while waiting for 0x{:02x}",
                        response.opcode, expected as u8
                    );
                }
                Err(error) => eprintln!("warning: ignored malformed Event TX frame: {error}"),
            }
        }
    }

    async fn disconnect(&self) {
        let _ = self.peripheral.unsubscribe(&self.event).await;
        if self.peripheral.is_connected().await.unwrap_or(false) {
            let _ = self.peripheral.disconnect().await;
        }
    }
}

#[tokio::main]
async fn main() -> Result<()> {
    let cli = Cli::parse();

    match &cli.command {
        Command::Serve => return run_daemon(&cli).await,
        Command::Hook => return forward_codex_hook(&cli).await,
        _ => {}
    }

    let adapter = default_adapter().await?;

    if matches!(cli.command, Command::Scan) {
        return scan(
            &adapter,
            &cli.device_name,
            Duration::from_secs(cli.scan_timeout),
        )
        .await;
    }

    let client = AgentClient::connect(
        &adapter,
        &cli.device_name,
        Duration::from_secs(cli.scan_timeout),
        Duration::from_secs(cli.response_timeout),
    )
    .await?;
    let result = tokio::select! {
        result = run_command(&client, &cli.command) => result,
        signal = tokio::signal::ctrl_c() => {
            signal.context("installing Ctrl-C handler failed")?;
            eprintln!("interrupted; disconnecting...");
            Ok(())
        }
    };
    client.disconnect().await;
    result
}

async fn pair_with_bluez(peripheral: &Peripheral, pairing_timeout: Duration) -> Result<()> {
    let (_dbus_task, session) = BluetoothSession::new()
        .await
        .context("opening a BlueZ D-Bus session for pairing failed")?;
    let address = peripheral.address().to_string();
    let device = session
        .get_devices()
        .await
        .context("querying BlueZ devices failed")?
        .into_iter()
        .find(|item| item.mac_address.to_string() == address)
        .with_context(|| format!("connected BLE device {address} was not present in BlueZ"))?;

    if !device.paired {
        println!("pairing with {address} using BlueZ Just Works...");
        session
            .pair_with_timeout(&device.id, pairing_timeout)
            .await
            .context("BlueZ pairing failed")?;
    }
    Ok(())
}

async fn run_command(client: &AgentClient, command: &Command) -> Result<()> {
    match command {
        Command::Scan => unreachable!(),
        Command::Register { key, name } => {
            let response = client.execute(&protocol::register(*key, name)?).await?;
            ensure!(
                response.agent_id != NO_AGENT,
                "register succeeded without an Agent id"
            );
            println!("registered '{name}' as Agent {}", response.agent_id);
        }
        Command::State { id, state } => {
            client.execute(&protocol::set_state(*id, *state)?).await?;
            println!("Agent {id} state -> {state:?}");
        }
        Command::Text { id, text } => {
            client.execute(&protocol::set_text(*id, text)?).await?;
            println!("Agent {id} text -> {text:?}");
        }
        Command::Unregister { id } => {
            client.execute(&protocol::unregister(*id)?).await?;
            println!("Agent {id} removed");
        }
        Command::Demo { step_ms } => run_demo(client, Duration::from_millis(*step_ms)).await?,
        Command::Serve | Command::Hook => unreachable!(),
    }
    Ok(())
}

async fn run_demo(client: &AgentClient, step: Duration) -> Result<()> {
    println!("running seven-Agent demo; watch slots and LEDs 0..6");
    let mut ids = Vec::with_capacity(AGENT_COUNT as usize);
    for index in 0..AGENT_COUNT {
        let name = format!("agent-{index}");
        let response = client
            .execute(&protocol::register(u32::from(index) + 1, &name)?)
            .await?;
        ensure!(
            response.agent_id == index,
            "expected slot {index}, device allocated {}",
            response.agent_id
        );
        ids.push(response.agent_id);
        println!("  registered {name} -> slot {}", response.agent_id);
        sleep(step).await;
    }

    println!("testing the complete 7 slots x 7 states matrix (49 state transitions)...");
    for &state in &AgentState::ALL {
        println!("state phase: {state:?}");
        for &id in &ids {
            // Label the display as well as changing the state bar/LED so the
            // operator can verify that the transition reached the right slot.
            client
                .execute(&protocol::set_text(id, &format!("{state:?}"))?)
                .await?;
            client.execute(&protocol::set_state(id, state)?).await?;
            println!("  slot {id}/6 -> {state:?}");
            sleep(step).await;
        }
    }

    println!("all 49 slot/state combinations passed; unregistering slots...");
    for &id in ids.iter().rev() {
        client.execute(&protocol::unregister(id)?).await?;
        println!("  removed slot {id}");
        sleep(step).await;
    }
    println!("demo passed; disconnecting (LEDs should become red)");
    Ok(())
}

#[derive(Debug, Deserialize)]
struct CodexHookInput {
    session_id: String,
    hook_event_name: String,
    #[serde(default)]
    transcript_path: Option<PathBuf>,
    #[serde(default)]
    tool_name: Option<String>,
}

#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
#[serde(rename_all = "snake_case")]
enum Activity {
    Idle,
    Working,
    WaitPermission,
    WaitOption,
    Done,
    Remove,
}

impl Activity {
    fn from_hook(input: &CodexHookInput) -> Option<Self> {
        match input.hook_event_name.as_str() {
            "SessionStart" => Some(Self::Idle),
            "UserPromptSubmit" => Some(Self::Working),
            "PermissionRequest" => Some(Self::WaitPermission),
            "PreToolUse" if input.tool_name.as_deref() == Some("request_user_input") => {
                Some(Self::WaitOption)
            }
            "PreToolUse" => Some(Self::Working),
            "PostToolUse" => Some(Self::Working),
            "Stop" => Some(Self::Done),
            "SessionEnd" => Some(Self::Remove),
            _ => None,
        }
    }

    fn panel_state(self) -> Option<AgentState> {
        match self {
            Self::Idle => Some(AgentState::Idle),
            Self::Working => Some(AgentState::Working),
            Self::WaitPermission => Some(AgentState::WaitPermission),
            Self::WaitOption => Some(AgentState::WaitOption),
            Self::Done => Some(AgentState::Done),
            Self::Remove => None,
        }
    }

    fn display_text(self) -> &'static str {
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
struct DaemonEvent {
    session_id: String,
    activity: Activity,
    transcript_path: Option<PathBuf>,
}

struct SessionAgent {
    key: u32,
    name: String,
    state: AgentState,
    text: &'static str,
    slot: Option<u8>,
    remove: bool,
    dirty: bool,
    transcript_path: Option<PathBuf>,
    transcript_offset: u64,
}

impl SessionAgent {
    fn new(session_id: &str) -> Self {
        let key = session_key(session_id);
        Self {
            key,
            name: format!("codex-{key:08x}"),
            state: AgentState::Idle,
            text: Activity::Idle.display_text(),
            slot: None,
            remove: false,
            dirty: true,
            transcript_path: None,
            transcript_offset: 0,
        }
    }

    fn apply(&mut self, activity: Activity) {
        if activity.panel_state().is_none() {
            self.remove = true;
            self.dirty = true;
            return;
        }
        self.state = activity
            .panel_state()
            .expect("non-removal activity has a state");
        self.text = activity.display_text();
        self.remove = false;
        self.dirty = true;
    }

    fn set_transcript(&mut self, path: Option<PathBuf>) {
        let Some(path) = path else { return };
        if self.transcript_path.as_ref() == Some(&path) {
            return;
        }
        // Start at EOF: old interrupted turns from a resumed transcript must
        // not overwrite the new SessionStart/Working state.
        self.transcript_offset = fs::metadata(&path)
            .map(|metadata| metadata.len())
            .unwrap_or(0);
        self.transcript_path = Some(path);
    }

    fn mark_interrupted(&mut self) {
        self.state = AgentState::Idle;
        self.text = "interrupted";
        self.remove = false;
        self.dirty = true;
    }
}

#[derive(Clone)]
struct DaemonConfig {
    device_name: String,
    scan_timeout: Duration,
    response_timeout: Duration,
}

async fn forward_codex_hook(cli: &Cli) -> Result<()> {
    let mut input = Vec::new();
    std::io::stdin()
        .read_to_end(&mut input)
        .context("reading Codex hook JSON from stdin failed")?;
    let hook: CodexHookInput =
        serde_json::from_slice(&input).context("decoding Codex hook JSON failed")?;
    let Some(activity) = Activity::from_hook(&hook) else {
        return Ok(());
    };
    let event = serde_json::to_vec(&DaemonEvent {
        session_id: hook.session_id,
        activity,
        transcript_path: hook.transcript_path,
    })?;
    let socket = socket_path(cli);

    if send_daemon_event(&socket, &event).await.is_ok() {
        return Ok(());
    }

    spawn_daemon(cli, &socket)?;
    for _ in 0..50 {
        sleep(Duration::from_millis(40)).await;
        if send_daemon_event(&socket, &event).await.is_ok() {
            return Ok(());
        }
    }
    bail!(
        "BLE Agent daemon did not create {} within two seconds; see {}",
        socket.display(),
        daemon_log_path(&socket).display()
    )
}

async fn send_daemon_event(socket: &Path, event: &[u8]) -> Result<()> {
    let mut stream = UnixStream::connect(socket).await.with_context(|| {
        format!(
            "connecting to BLE Agent daemon at {} failed",
            socket.display()
        )
    })?;
    stream
        .write_all(event)
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

    ProcessCommand::new(executable)
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

async fn run_daemon(cli: &Cli) -> Result<()> {
    let socket = socket_path(cli);
    if let Some(parent) = socket.parent() {
        fs::create_dir_all(parent)
            .with_context(|| format!("creating socket directory {} failed", parent.display()))?;
    }
    if socket.exists() {
        if UnixStream::connect(&socket).await.is_ok() {
            bail!(
                "BLE Agent daemon is already running at {}",
                socket.display()
            );
        }
        fs::remove_file(&socket)
            .with_context(|| format!("removing stale socket {} failed", socket.display()))?;
    }

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

    let config = DaemonConfig {
        device_name: cli.device_name.clone(),
        scan_timeout: Duration::from_secs(cli.scan_timeout),
        response_timeout: Duration::from_secs(cli.response_timeout),
    };
    let (sender, receiver) = mpsc::channel(64);
    tokio::spawn(ble_worker(receiver, config));

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
        let event: DaemonEvent = match serde_json::from_slice(&payload) {
            Ok(event) => event,
            Err(error) => {
                eprintln!("ignored malformed hook event: {error}");
                continue;
            }
        };
        if sender.send(event).await.is_err() {
            bail!("BLE Agent worker stopped unexpectedly");
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
        if let Err(error) = synchronize_sessions(&mut sessions, &mut client, &config).await {
            eprintln!("BLE synchronization failed: {error:#}");
            if let Some(existing) = client.take() {
                existing.disconnect().await;
            }
            for agent in sessions.values_mut() {
                agent.slot = None;
                agent.dirty = true;
            }
        }
    }
}

fn detect_interrupted_turns(sessions: &mut HashMap<String, SessionAgent>) {
    for agent in sessions.values_mut() {
        let Some(path) = agent.transcript_path.as_ref() else {
            continue;
        };
        let Ok(mut file) = fs::File::open(path) else {
            continue;
        };
        let length = file.metadata().map(|metadata| metadata.len()).unwrap_or(0);
        if length < agent.transcript_offset {
            agent.transcript_offset = 0;
        }
        if file.seek(SeekFrom::Start(agent.transcript_offset)).is_err() {
            continue;
        }

        let mut interrupted = false;
        let mut reader = BufReader::new(file);
        let mut line = String::new();
        loop {
            line.clear();
            match reader.read_line(&mut line) {
                Ok(0) => break,
                Ok(_) => interrupted |= is_turn_aborted(&line),
                Err(_) => break,
            }
        }
        if let Ok(position) = reader.stream_position() {
            agent.transcript_offset = position;
        }
        if interrupted {
            agent.mark_interrupted();
        }
    }
}

fn is_turn_aborted(line: &str) -> bool {
    let Ok(value) = serde_json::from_str::<serde_json::Value>(line) else {
        return false;
    };
    value.get("type").and_then(serde_json::Value::as_str) == Some("event_msg")
        && value
            .get("payload")
            .and_then(|payload| payload.get("type"))
            .and_then(serde_json::Value::as_str)
            == Some("turn_aborted")
}

async fn synchronize_sessions(
    sessions: &mut HashMap<String, SessionAgent>,
    client: &mut Option<AgentClient>,
    config: &DaemonConfig,
) -> Result<()> {
    let connected = match client.as_ref() {
        Some(existing) => existing.peripheral.is_connected().await.unwrap_or(false),
        None => false,
    };
    if !connected {
        let adapter = default_adapter().await?;
        *client = Some(
            AgentClient::connect(
                &adapter,
                &config.device_name,
                config.scan_timeout,
                config.response_timeout,
            )
            .await?,
        );
        for agent in sessions.values_mut() {
            agent.slot = None;
            agent.dirty = true;
        }
    }
    let client = client.as_ref().expect("connected client was installed");
    let mut removed = Vec::new();

    for (session_id, agent) in sessions.iter_mut() {
        if !agent.dirty {
            continue;
        }
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

fn socket_path(cli: &Cli) -> PathBuf {
    cli.socket.clone().unwrap_or_else(|| {
        let directory = std::env::var_os("XDG_RUNTIME_DIR")
            .map(PathBuf::from)
            .unwrap_or_else(std::env::temp_dir);
        directory.join("prtn-ble-agent.sock")
    })
}

fn daemon_log_path(socket: &Path) -> PathBuf {
    socket.with_extension("log")
}

fn session_key(session_id: &str) -> u32 {
    // Stable FNV-1a keeps the same firmware slot across daemon reconnects.
    let mut hash = 0x811c_9dc5_u32;
    for byte in session_id.as_bytes() {
        hash ^= u32::from(*byte);
        hash = hash.wrapping_mul(0x0100_0193);
    }
    if hash == 0 {
        1
    } else {
        hash
    }
}

async fn default_adapter() -> Result<Adapter> {
    let manager = Manager::new()
        .await
        .context("opening the system Bluetooth manager failed")?;
    manager
        .adapters()
        .await
        .context("listing Bluetooth adapters failed")?
        .into_iter()
        .next()
        .context("no Bluetooth adapter found; enable Bluetooth and check BlueZ")
}

async fn scan(adapter: &Adapter, target_name: &str, duration: Duration) -> Result<()> {
    adapter
        .start_scan(ScanFilter::default())
        .await
        .context("starting BLE scan failed")?;
    let scan_result = async {
        sleep(duration).await;
        adapter
            .peripherals()
            .await
            .context("reading scan results failed")
    }
    .await;
    adapter
        .stop_scan()
        .await
        .context("stopping BLE scan failed")?;
    let peripherals = scan_result?;
    if peripherals.is_empty() {
        bail!("no BLE devices discovered");
    }
    for peripheral in peripherals {
        let properties = match peripheral.properties().await {
            Ok(Some(properties)) => properties,
            Ok(None) => continue,
            Err(error) => {
                eprintln!(
                    "warning: could not read {} properties: {error}",
                    peripheral.address()
                );
                continue;
            }
        };
        let name = properties.local_name.as_deref().unwrap_or("<unnamed>");
        let marker = if name == target_name || properties.services.contains(&SERVICE_UUID) {
            "*"
        } else {
            " "
        };
        println!("{marker} {}  {name}", peripheral.address());
    }
    Ok(())
}

async fn find_panel(adapter: &Adapter, name: &str, duration: Duration) -> Result<Peripheral> {
    adapter
        .start_scan(ScanFilter {
            services: vec![SERVICE_UUID],
        })
        .await
        .context("starting Agent Panel scan failed")?;
    let scan_result = async {
        let deadline = Instant::now() + duration;
        let mut candidates = Vec::new();
        let mut first_match_at = None;
        while Instant::now() < deadline {
            candidates.clear();
            for peripheral in adapter.peripherals().await? {
                let properties = match peripheral.properties().await {
                    Ok(Some(properties)) => properties,
                    Ok(None) | Err(_) => continue,
                };
                if properties.services.contains(&SERVICE_UUID)
                    || properties.local_name.as_deref() == Some(name)
                {
                    candidates.push(peripheral);
                }
            }
            if !candidates.is_empty() {
                let found_at = *first_match_at.get_or_insert_with(Instant::now);
                // Keep scanning briefly after the first match so duplicate panels
                // are diagnosed instead of choosing whichever advertised first.
                if Instant::now().duration_since(found_at) >= Duration::from_secs(1) {
                    break;
                }
            }
            sleep(Duration::from_millis(250)).await;
        }
        Ok::<_, anyhow::Error>(candidates)
    }
    .await;
    let stop_result = adapter
        .stop_scan()
        .await
        .context("stopping Agent Panel scan failed");
    let mut candidates = scan_result?;
    stop_result?;
    match candidates.len() {
        0 => bail!(
            "Agent Panel '{name}' was not found within {} seconds",
            duration.as_secs()
        ),
        1 => Ok(candidates.remove(0)),
        _ => {
            let addresses = candidates
                .iter()
                .map(|item| item.address().to_string())
                .collect::<Vec<_>>()
                .join(", ");
            bail!("multiple Agent Panels matched ({addresses}); power off all but one and retry")
        }
    }
}

#[cfg(test)]
mod hook_tests {
    use super::*;
    use std::io::Write as _;

    fn hook(event: &str, tool: Option<&str>) -> CodexHookInput {
        CodexHookInput {
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

    #[test]
    fn session_keys_are_stable_nonzero_and_session_specific() {
        assert_eq!(session_key("thr_a"), session_key("thr_a"));
        assert_ne!(session_key("thr_a"), 0);
        assert_ne!(session_key("thr_a"), session_key("thr_b"));
    }

    #[test]
    fn recognizes_only_transcript_turn_abort_events() {
        assert!(is_turn_aborted(
            r#"{"type":"event_msg","payload":{"type":"turn_aborted","reason":"interrupted"}}"#
        ));
        assert!(!is_turn_aborted(
            r#"{"type":"event_msg","payload":{"type":"turn_completed"}}"#
        ));
        assert!(!is_turn_aborted("not JSON"));
    }

    #[test]
    fn transcript_watcher_ignores_history_and_handles_new_ctrl_c() {
        let path = std::env::temp_dir().join(format!(
            "prtn-ble-agent-transcript-test-{}.jsonl",
            std::process::id()
        ));
        fs::write(
            &path,
            "{\"type\":\"event_msg\",\"payload\":{\"type\":\"turn_aborted\"}}\n",
        )
        .unwrap();

        let mut agent = SessionAgent::new("thr_test");
        agent.set_transcript(Some(path.clone()));
        agent.state = AgentState::WaitOption;
        agent.text = "your input";
        agent.dirty = false;
        let mut sessions = HashMap::from([("thr_test".to_owned(), agent)]);

        detect_interrupted_turns(&mut sessions);
        assert_eq!(sessions["thr_test"].state, AgentState::WaitOption);
        assert_eq!(sessions["thr_test"].text, "your input");

        let mut file = OpenOptions::new().append(true).open(&path).unwrap();
        writeln!(file, "{{\"type\":\"event_msg\",\"payload\":{{\"type\":\"turn_aborted\",\"reason\":\"interrupted\"}}}}")
        .unwrap();
        detect_interrupted_turns(&mut sessions);
        assert_eq!(sessions["thr_test"].state, AgentState::Idle);
        assert_eq!(sessions["thr_test"].text, "interrupted");
        assert!(sessions["thr_test"].dirty);

        fs::remove_file(path).unwrap();
    }
}
