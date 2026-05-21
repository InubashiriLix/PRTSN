use std::collections::VecDeque;
use std::env;
use std::fs::{read_dir, File, OpenOptions};
use std::io::{self, Read, Write};
use std::path::Path;
use std::process::{Command, Stdio};
use std::sync::mpsc::{self, Sender};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

const BAUDS: &[u32] = &[
    9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600, 1_000_000, 1_152_000, 2_000_000,
];

const MAX_LINES: usize = 3000;
const MAX_LINE_LEN: usize = 240;

#[derive(Clone, Copy, Eq, PartialEq)]
enum Ending {
    None,
    Lf,
    Cr,
    Crlf,
}

impl Ending {
    fn next(self) -> Self {
        match self {
            Self::None => Self::Lf,
            Self::Lf => Self::Cr,
            Self::Cr => Self::Crlf,
            Self::Crlf => Self::None,
        }
    }

    fn bytes(self) -> &'static [u8] {
        match self {
            Self::None => b"",
            Self::Lf => b"\n",
            Self::Cr => b"\r",
            Self::Crlf => b"\r\n",
        }
    }

    fn label(self) -> &'static str {
        match self {
            Self::None => "NONE",
            Self::Lf => "LF",
            Self::Cr => "CR",
            Self::Crlf => "CRLF",
        }
    }
}

#[derive(Clone)]
struct Config {
    port: Option<String>,
    baud: u32,
}

enum Event {
    Input(u8),
    Serial(Vec<u8>),
    SerialClosed(String),
}

struct App {
    ports: Vec<String>,
    selected_port: usize,
    baud_index: usize,
    connected_port: Option<String>,
    serial: Option<Arc<Mutex<File>>>,
    lines: VecDeque<String>,
    current_line: String,
    input: Vec<u8>,
    ending: Ending,
    hex_view: bool,
    hex_send: bool,
    timestamps: bool,
    autoscroll: bool,
    status: String,
    rx_bytes: usize,
    tx_bytes: usize,
    last_rx_bytes: usize,
    last_tx_bytes: usize,
    last_rate_at: Instant,
    rx_rate: usize,
    tx_rate: usize,
}

struct TerminalGuard {
    saved_stty: String,
}

impl Drop for TerminalGuard {
    fn drop(&mut self) {
        let _ = Command::new("stty")
            .arg(&self.saved_stty)
            .stdin(Stdio::inherit())
            .stdout(Stdio::null())
            .stderr(Stdio::null())
            .status();
        let _ = write!(io::stdout(), "\x1b[?1049l\x1b[?25h\x1b[0m");
        let _ = io::stdout().flush();
    }
}

fn main() -> io::Result<()> {
    let config = parse_args();
    let _terminal = setup_terminal()?;

    let (tx, rx) = mpsc::channel::<Event>();
    spawn_input_thread(tx.clone());

    let mut app = App::new(config);
    app.draw()?;

    loop {
        while let Ok(event) = rx.try_recv() {
            match event {
                Event::Input(byte) => {
                    if !handle_input(&mut app, &tx, byte)? {
                        return Ok(());
                    }
                }
                Event::Serial(data) => app.push_serial_data(&data),
                Event::SerialClosed(reason) => {
                    app.serial = None;
                    app.connected_port = None;
                    app.status = reason;
                }
            }
        }

        app.update_rates();
        app.draw()?;
        thread::sleep(Duration::from_millis(16));
    }
}

impl App {
    fn new(config: Config) -> Self {
        let ports = scan_ports();
        let selected_port = config
            .port
            .as_ref()
            .and_then(|port| ports.iter().position(|item| item == port))
            .unwrap_or(0);
        let baud_index = BAUDS
            .iter()
            .position(|baud| *baud == config.baud)
            .unwrap_or_else(|| BAUDS.iter().position(|baud| *baud == 115200).unwrap_or(0));

        Self {
            ports,
            selected_port,
            baud_index,
            connected_port: None,
            serial: None,
            lines: VecDeque::new(),
            current_line: String::new(),
            input: Vec::new(),
            ending: Ending::Lf,
            hex_view: false,
            hex_send: false,
            timestamps: false,
            autoscroll: true,
            status: String::from("Disconnected"),
            rx_bytes: 0,
            tx_bytes: 0,
            last_rx_bytes: 0,
            last_tx_bytes: 0,
            last_rate_at: Instant::now(),
            rx_rate: 0,
            tx_rate: 0,
        }
    }

    fn selected_port(&self) -> Option<&str> {
        self.ports.get(self.selected_port).map(String::as_str)
    }

    fn baud(&self) -> u32 {
        BAUDS[self.baud_index]
    }

    fn refresh_ports(&mut self) {
        let old = self.selected_port().map(str::to_owned);
        self.ports = scan_ports();
        self.selected_port = old
            .as_ref()
            .and_then(|port| self.ports.iter().position(|item| item == port))
            .unwrap_or(0);
        self.status = format!("Found {} serial port(s)", self.ports.len());
    }

    fn connect(&mut self, events: &Sender<Event>) {
        if self.serial.is_some() {
            self.serial = None;
            self.connected_port = None;
            self.status = String::from("Disconnected");
            return;
        }

        let Some(port) = self.selected_port().map(str::to_owned) else {
            self.status = String::from("No serial port selected");
            return;
        };

        if let Err(err) = configure_serial_port(&port, self.baud()) {
            self.status = format!("stty failed: {err}");
            return;
        }

        let file = match OpenOptions::new().read(true).write(true).open(&port) {
            Ok(file) => file,
            Err(err) => {
                self.status = format!("Open failed: {err}");
                return;
            }
        };

        let serial = Arc::new(Mutex::new(file));
        self.serial = Some(serial.clone());
        self.connected_port = Some(port.clone());
        self.status = format!("Connected {port} @ {}", self.baud());
        spawn_serial_thread(port, serial, events.clone());
    }

    fn apply_baud(&mut self) {
        let Some(port) = self.connected_port.as_ref() else {
            self.status = format!("Baud selected: {}", self.baud());
            return;
        };

        match configure_serial_port(port, self.baud()) {
            Ok(()) => self.status = format!("Baud applied: {}", self.baud()),
            Err(err) => self.status = format!("Baud apply failed: {err}"),
        }
    }

    fn send_input(&mut self) {
        let mut payload = if self.hex_send {
            match parse_hex(&self.input) {
                Ok(data) => data,
                Err(err) => {
                    self.status = err;
                    return;
                }
            }
        } else {
            let mut data = self.input.clone();
            data.extend_from_slice(self.ending.bytes());
            data
        };

        if payload.is_empty() {
            return;
        }

        let Some(serial) = self.serial.as_ref() else {
            self.status = String::from("Not connected");
            return;
        };

        let write_result = serial
            .lock()
            .map_err(|_| io::Error::new(io::ErrorKind::Other, "serial lock poisoned"))
            .and_then(|mut file| {
                file.write_all(&payload)?;
                file.flush()
            });

        match write_result {
            Ok(()) => {
                self.tx_bytes += payload.len();
                self.push_log(
                    format!(
                        "TX {}",
                        display_bytes(&payload, self.hex_view || self.hex_send)
                    ),
                    true,
                );
                self.input.clear();
                payload.clear();
            }
            Err(err) => self.status = format!("Write failed: {err}"),
        }
    }

    fn push_serial_data(&mut self, data: &[u8]) {
        self.rx_bytes += data.len();
        if self.hex_view {
            self.push_log(display_bytes(data, true), false);
            return;
        }

        for &byte in data {
            match byte {
                b'\r' => {}
                b'\n' => self.finish_current_line(),
                0x20..=0x7e | b'\t' => {
                    if self.current_line.len() < MAX_LINE_LEN {
                        self.current_line.push(byte as char);
                    }
                }
                _ => {
                    if self.current_line.len() + 4 < MAX_LINE_LEN {
                        self.current_line.push_str(&format!("\\x{byte:02X}"));
                    }
                }
            }
        }
    }

    fn push_log(&mut self, text: String, meta: bool) {
        let line = if self.timestamps {
            format!("{} {text}", time_label())
        } else if meta {
            format!("[{}] {text}", time_label())
        } else {
            text
        };
        self.lines.push_back(line);
        while self.lines.len() > MAX_LINES {
            self.lines.pop_front();
        }
    }

    fn finish_current_line(&mut self) {
        let line = std::mem::take(&mut self.current_line);
        self.push_log(line, false);
    }

    fn update_rates(&mut self) {
        let now = Instant::now();
        let dt = now.duration_since(self.last_rate_at);
        if dt < Duration::from_millis(900) {
            return;
        }

        let secs = dt.as_secs_f64().max(0.001);
        self.rx_rate = ((self.rx_bytes - self.last_rx_bytes) as f64 / secs) as usize;
        self.tx_rate = ((self.tx_bytes - self.last_tx_bytes) as f64 / secs) as usize;
        self.last_rx_bytes = self.rx_bytes;
        self.last_tx_bytes = self.tx_bytes;
        self.last_rate_at = now;
    }

    fn draw(&self) -> io::Result<()> {
        let mut out = io::stdout();
        let (cols, rows) = terminal_size();
        let width = cols.max(80) as usize;
        let height = rows.max(24) as usize;
        let log_height = height.saturating_sub(8);

        write!(out, "\x1b[H\x1b[2J")?;
        write!(
            out,
            "\x1b[38;5;81mSerial TUI\x1b[0m {}\r\n",
            horizontal(width.saturating_sub(11))
        )?;

        let port = self.selected_port().unwrap_or("<none>");
        let state = self.connected_port.as_deref().unwrap_or("disconnected");
        write!(
            out,
            "\x1b[1mPort\x1b[0m {}   \x1b[1mBaud\x1b[0m {}   \x1b[1mState\x1b[0m {}\r\n",
            trim(port, 32),
            self.baud(),
            trim(state, 32)
        )?;
        write!(
            out,
            "\x1b[1mMode\x1b[0m ending={} hex-view={} hex-send={} timestamp={}   \x1b[1mRate\x1b[0m RX {} B/s TX {} B/s\r\n",
            self.ending.label(),
            on_off(self.hex_view),
            on_off(self.hex_send),
            on_off(self.timestamps),
            self.rx_rate,
            self.tx_rate,
        )?;
        write!(
            out,
            "\x1b[38;5;244m{}\x1b[0m\r\n",
            trim(&self.status, width)
        )?;
        write!(out, "{}\r\n", horizontal(width))?;

        let visible_lines = self.visible_lines(log_height);
        for line in &visible_lines {
            write!(out, "{}\r\n", trim(line, width))?;
        }
        for _ in 0..log_height.saturating_sub(visible_lines.len()) {
            write!(out, "\r\n")?;
        }

        write!(out, "{}\r\n", horizontal(width))?;
        write!(
            out,
            "\x1b[38;5;244mCtrl-C/Ctrl-Q quit | Ctrl-O connect | Ctrl-R refresh | Ctrl-B baud | Ctrl-E ending | Ctrl-X hex view | Ctrl-S hex send | Ctrl-T timestamp | Ctrl-L clear\x1b[0m\r\n"
        )?;
        write!(
            out,
            "> {}",
            trim(
                &String::from_utf8_lossy(&self.input),
                width.saturating_sub(2)
            )
        )?;
        write!(out, "\x1b[?25h")?;
        out.flush()
    }

    fn visible_lines(&self, log_height: usize) -> Vec<&String> {
        let mut lines: Vec<&String> = self.lines.iter().collect();
        if !self.current_line.is_empty() {
            lines.push(&self.current_line);
        }
        if self.autoscroll && lines.len() > log_height {
            lines[lines.len() - log_height..].to_vec()
        } else {
            lines.into_iter().take(log_height).collect()
        }
    }
}

fn handle_input(app: &mut App, events: &Sender<Event>, byte: u8) -> io::Result<bool> {
    match byte {
        0x03 | 0x11 => return Ok(false), // Ctrl-C / Ctrl-Q
        0x0f => app.connect(events),     // Ctrl-O
        0x12 => app.refresh_ports(),     // Ctrl-R
        0x02 => {
            app.baud_index = (app.baud_index + 1) % BAUDS.len();
            app.apply_baud();
        }
        0x05 => {
            app.ending = app.ending.next();
            app.status = format!("Line ending: {}", app.ending.label());
        }
        0x18 => {
            app.hex_view = !app.hex_view;
            app.status = format!("Hex view {}", on_off(app.hex_view));
        }
        0x13 => {
            app.hex_send = !app.hex_send;
            app.status = format!("Hex send {}", on_off(app.hex_send));
        }
        0x14 => {
            app.timestamps = !app.timestamps;
            app.status = format!("Timestamp {}", on_off(app.timestamps));
        }
        0x0c => {
            app.lines.clear();
            app.current_line.clear();
            app.status = String::from("Cleared");
        }
        b'\r' | b'\n' => app.send_input(),
        0x7f | 0x08 => {
            app.input.pop();
        }
        0x20..=0x7e | b'\t' => {
            if app.input.len() < 512 {
                app.input.push(byte);
            }
        }
        _ => {}
    }
    Ok(true)
}

fn setup_terminal() -> io::Result<TerminalGuard> {
    let saved_stty = String::from_utf8(
        Command::new("stty")
            .arg("-g")
            .stdin(Stdio::inherit())
            .output()?
            .stdout,
    )
    .unwrap_or_default()
    .trim()
    .to_owned();

    let status = Command::new("stty")
        .args(["raw", "-echo", "min", "0", "time", "0"])
        .stdin(Stdio::inherit())
        .status()?;
    if !status.success() {
        return Err(io::Error::new(
            io::ErrorKind::Other,
            "failed to set terminal raw mode",
        ));
    }

    write!(io::stdout(), "\x1b[?1049h\x1b[?25l\x1b[2J\x1b[H")?;
    io::stdout().flush()?;
    Ok(TerminalGuard { saved_stty })
}

fn configure_serial_port(port: &str, baud: u32) -> io::Result<()> {
    let status = Command::new("stty")
        .arg("-F")
        .arg(port)
        .arg(baud.to_string())
        .args([
            "raw", "-echo", "-echoe", "-echok", "-echoctl", "-echoke", "cs8", "-parenb", "-cstopb",
            "-ixon", "-ixoff", "min", "0", "time", "1",
        ])
        .status()?;

    if status.success() {
        Ok(())
    } else {
        Err(io::Error::new(
            io::ErrorKind::Other,
            "stty returned non-zero status",
        ))
    }
}

fn spawn_input_thread(events: Sender<Event>) {
    thread::spawn(move || {
        let mut input = io::stdin();
        let mut buf = [0u8; 64];
        loop {
            match input.read(&mut buf) {
                Ok(0) => thread::sleep(Duration::from_millis(5)),
                Ok(n) => {
                    for &byte in &buf[..n] {
                        if events.send(Event::Input(byte)).is_err() {
                            return;
                        }
                    }
                }
                Err(_) => return,
            }
        }
    });
}

fn spawn_serial_thread(port: String, serial: Arc<Mutex<File>>, events: Sender<Event>) {
    thread::spawn(move || {
        let mut buf = [0u8; 1024];
        loop {
            let read_result = serial
                .lock()
                .map_err(|_| io::Error::new(io::ErrorKind::Other, "serial lock poisoned"))
                .and_then(|mut file| file.read(&mut buf));

            match read_result {
                Ok(0) => thread::sleep(Duration::from_millis(5)),
                Ok(n) => {
                    if events.send(Event::Serial(buf[..n].to_vec())).is_err() {
                        return;
                    }
                }
                Err(err) if err.kind() == io::ErrorKind::WouldBlock => {
                    thread::sleep(Duration::from_millis(5))
                }
                Err(err) => {
                    let _ = events.send(Event::SerialClosed(format!("{port} closed: {err}")));
                    return;
                }
            }
        }
    });
}

fn scan_ports() -> Vec<String> {
    let mut ports = Vec::new();
    if let Ok(entries) = read_dir("/dev") {
        for entry in entries.flatten() {
            let name = entry.file_name().to_string_lossy().into_owned();
            if name.starts_with("ttyACM")
                || name.starts_with("ttyUSB")
                || name.starts_with("ttyS")
                || name.starts_with("ttyAMA")
            {
                ports.push(format!("/dev/{name}"));
            }
        }
    }
    ports.sort();
    ports
}

fn parse_args() -> Config {
    let mut port = None;
    let mut baud = 115200;
    let mut args = env::args().skip(1);

    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--port" | "-p" => port = args.next(),
            "--baud" | "-b" => {
                if let Some(value) = args.next() {
                    baud = value.parse().unwrap_or(baud);
                }
            }
            "--help" | "-h" => {
                println!("serial-tui --port /dev/ttyACM0 --baud 115200");
                std::process::exit(0);
            }
            value if Path::new(value).exists() => port = Some(value.to_owned()),
            _ => {}
        }
    }

    Config { port, baud }
}

fn parse_hex(input: &[u8]) -> Result<Vec<u8>, String> {
    let text = String::from_utf8_lossy(input).replace(',', " ");
    let compact: String = text.split_whitespace().collect();
    let tokens: Vec<String> = if text.split_whitespace().count() <= 1 && compact.len() > 2 {
        if compact.len() % 2 != 0 {
            return Err(String::from("Hex needs even digits"));
        }
        compact
            .as_bytes()
            .chunks(2)
            .map(|chunk| String::from_utf8_lossy(chunk).to_string())
            .collect()
    } else {
        text.split_whitespace().map(str::to_owned).collect()
    };

    let mut out = Vec::with_capacity(tokens.len());
    for token in tokens {
        if token.len() > 2 {
            return Err(String::from("Hex byte must be 00..FF"));
        }
        let byte = u8::from_str_radix(&token, 16).map_err(|_| String::from("Invalid hex input"))?;
        out.push(byte);
    }
    Ok(out)
}

fn display_bytes(data: &[u8], hex: bool) -> String {
    if hex {
        data.iter()
            .map(|byte| format!("{byte:02X}"))
            .collect::<Vec<_>>()
            .join(" ")
    } else {
        String::from_utf8_lossy(data).replace('\r', "")
    }
}

fn time_label() -> String {
    let secs = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs()
        % 86_400;
    format!(
        "{:02}:{:02}:{:02}",
        secs / 3600,
        (secs / 60) % 60,
        secs % 60
    )
}

fn terminal_size() -> (u16, u16) {
    let output = Command::new("stty")
        .arg("size")
        .stdin(Stdio::inherit())
        .output();
    if let Ok(output) = output {
        if let Ok(text) = String::from_utf8(output.stdout) {
            let mut parts = text.split_whitespace();
            if let (Some(rows), Some(cols)) = (parts.next(), parts.next()) {
                if let (Ok(rows), Ok(cols)) = (rows.parse::<u16>(), cols.parse::<u16>()) {
                    return (cols, rows);
                }
            }
        }
    }
    (100, 30)
}

fn horizontal(width: usize) -> String {
    "-".repeat(width)
}

fn trim(text: &str, width: usize) -> String {
    if text.chars().count() <= width {
        return text.to_owned();
    }
    let mut out = text
        .chars()
        .take(width.saturating_sub(1))
        .collect::<String>();
    out.push('…');
    out
}

fn on_off(value: bool) -> &'static str {
    if value {
        "on"
    } else {
        "off"
    }
}
