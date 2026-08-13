use crate::protocol::{self, Response};
use anyhow::{bail, Context, Result};
use bluez_async::BluetoothSession;
use btleplug::api::{
    Central, Characteristic, Manager as _, Peripheral as _, ScanFilter, WriteType,
};
use btleplug::platform::{Adapter, Manager, Peripheral};
use futures_util::StreamExt;
use std::time::Duration;
use tokio::time::{sleep, timeout, Instant};
use uuid::{uuid, Uuid};

const SERVICE_UUID: Uuid = uuid!("6e291bc2-80c8-484d-a505-49509c3c868e");
const COMMAND_UUID: Uuid = uuid!("b6c66dd2-4047-473c-93f3-d97d9405330c");
const EVENT_UUID: Uuid = uuid!("1c69273f-d0fb-447a-90c1-0c472a0b7b53");

pub struct AgentClient {
    peripheral: Peripheral,
    command: Characteristic,
    event: Characteristic,
    response_timeout: Duration,
}

impl AgentClient {
    pub async fn connect(
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

    pub async fn execute(&self, frame: &[u8]) -> Result<Response> {
        let expected = protocol::opcode(frame)?;
        let mut notifications = self
            .peripheral
            .notifications()
            .await
            .context("opening notification stream failed")?;
        self.peripheral
            .write(&self.command, frame, WriteType::WithResponse)
            .await
            .with_context(|| format!("writing {expected:?} failed"))?;

        let deadline = Instant::now() + self.response_timeout;
        loop {
            let remaining = deadline.saturating_duration_since(Instant::now());
            let notification = timeout(remaining, notifications.next())
                .await
                .with_context(|| format!("timed out waiting for {expected:?} response"))?
                .context("BLE notification stream closed")?;
            if notification.uuid != self.event.uuid {
                eprintln!("warning: ignored notification from {}", notification.uuid);
                continue;
            }
            match Response::decode(&notification.value) {
                Ok(response) if response.opcode == expected as u8 => {
                    return response.require_ok(expected)
                }
                Ok(response) => eprintln!(
                    "warning: ignored response for opcode 0x{:02x} while waiting for 0x{:02x}",
                    response.opcode, expected as u8
                ),
                Err(error) => eprintln!("warning: ignored malformed Event TX frame: {error}"),
            }
        }
    }

    pub async fn is_connected(&self) -> bool {
        self.peripheral.is_connected().await.unwrap_or(false)
    }

    pub async fn disconnect(&self) {
        let _ = self.peripheral.unsubscribe(&self.event).await;
        if self.is_connected().await {
            let _ = self.peripheral.disconnect().await;
        }
    }
}

pub async fn default_adapter() -> Result<Adapter> {
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

pub async fn scan(adapter: &Adapter, target_name: &str, duration: Duration) -> Result<()> {
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
