use crate::ble::{self, AgentClient};
use crate::cli::{Cli, Command};
use crate::protocol::{self, AgentState, AGENT_COUNT, NO_AGENT};
use anyhow::{ensure, Context, Result};
use std::time::Duration;
use tokio::time::sleep;

pub async fn run(cli: &Cli) -> Result<()> {
    let adapter = ble::default_adapter().await?;
    if matches!(cli.command, Command::Scan) {
        return ble::scan(
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

async fn run_command(client: &AgentClient, command: &Command) -> Result<()> {
    match command {
        Command::Scan | Command::Serve | Command::Hook => unreachable!(),
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
