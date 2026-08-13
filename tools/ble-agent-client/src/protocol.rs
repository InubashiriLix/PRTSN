use anyhow::{bail, ensure, Context, Result};
use std::fmt;
use std::str::FromStr;

pub const MAX_NAME: usize = 15;
pub const MAX_TEXT: usize = 18;
pub const MAX_FRAME: usize = 20;
pub const AGENT_COUNT: u8 = 7;
pub const NO_AGENT: u8 = 0xff;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(u8)]
pub enum Opcode {
    Register = 0x01,
    SetState = 0x02,
    SetText = 0x03,
    Unregister = 0x04,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(u8)]
pub enum AgentState {
    Off = 0,
    Idle,
    Working,
    WaitPermission,
    WaitOption,
    Done,
    Error,
}

impl AgentState {
    pub const ALL: [Self; 7] = [
        Self::Off,
        Self::Idle,
        Self::Working,
        Self::WaitPermission,
        Self::WaitOption,
        Self::Done,
        Self::Error,
    ];
}

impl FromStr for AgentState {
    type Err = anyhow::Error;

    fn from_str(value: &str) -> Result<Self> {
        match value.to_ascii_lowercase().replace('_', "-").as_str() {
            "off" => Ok(Self::Off),
            "idle" => Ok(Self::Idle),
            "working" => Ok(Self::Working),
            "wait-permission" | "permission" => Ok(Self::WaitPermission),
            "wait-option" | "option" => Ok(Self::WaitOption),
            "done" => Ok(Self::Done),
            "error" => Ok(Self::Error),
            _ => bail!("unknown state '{value}'; expected off, idle, working, wait-permission, wait-option, done, or error"),
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(u8)]
pub enum Status {
    Ok = 0x00,
    InvalidOpcode = 0x01,
    InvalidLength = 0x02,
    InvalidUtf8 = 0x03,
    NoFreeSlot = 0x04,
    UnknownAgent = 0x05,
    InvalidState = 0x06,
    Busy = 0x07,
    InternalError = 0x08,
}

impl TryFrom<u8> for Status {
    type Error = anyhow::Error;

    fn try_from(value: u8) -> Result<Self> {
        match value {
            0x00 => Ok(Self::Ok),
            0x01 => Ok(Self::InvalidOpcode),
            0x02 => Ok(Self::InvalidLength),
            0x03 => Ok(Self::InvalidUtf8),
            0x04 => Ok(Self::NoFreeSlot),
            0x05 => Ok(Self::UnknownAgent),
            0x06 => Ok(Self::InvalidState),
            0x07 => Ok(Self::Busy),
            0x08 => Ok(Self::InternalError),
            _ => bail!("unknown protocol status 0x{value:02x}"),
        }
    }
}

impl fmt::Display for Status {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(formatter, "{self:?}")
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct Response {
    pub opcode: u8,
    pub status: Status,
    pub agent_id: u8,
}

impl Response {
    pub fn decode(frame: &[u8]) -> Result<Self> {
        ensure!(
            frame.len() == 4,
            "response must contain exactly 4 bytes, got {}",
            frame.len()
        );
        ensure!(
            frame[0] == 0x80,
            "invalid response marker 0x{:02x}",
            frame[0]
        );
        Ok(Self {
            opcode: frame[1],
            status: Status::try_from(frame[2])?,
            agent_id: frame[3],
        })
    }

    pub fn require_ok(self, expected: Opcode) -> Result<Self> {
        ensure!(
            self.opcode == expected as u8,
            "response opcode mismatch: expected 0x{:02x}, got 0x{:02x}",
            expected as u8,
            self.opcode
        );
        ensure!(
            self.status == Status::Ok,
            "device rejected {:?}: {} (0x{:02x}), agent id {}",
            expected,
            self.status,
            self.status as u8,
            self.agent_id
        );
        Ok(self)
    }
}

pub fn register(key: u32, name: &str) -> Result<Vec<u8>> {
    ensure!(key != 0, "Agent key must not be zero");
    let name = name.as_bytes();
    ensure!(!name.is_empty(), "Agent name must not be empty");
    ensure!(
        name.len() <= MAX_NAME,
        "Agent name exceeds {MAX_NAME} UTF-8 bytes"
    );

    let mut frame = Vec::with_capacity(5 + name.len());
    frame.push(Opcode::Register as u8);
    frame.extend_from_slice(&key.to_le_bytes());
    frame.extend_from_slice(name);
    ensure!(
        frame.len() <= MAX_FRAME,
        "register frame exceeds {MAX_FRAME} bytes"
    );
    Ok(frame)
}

pub fn set_state(id: u8, state: AgentState) -> Result<Vec<u8>> {
    validate_id(id)?;
    Ok(vec![Opcode::SetState as u8, id, state as u8])
}

pub fn set_text(id: u8, text: &str) -> Result<Vec<u8>> {
    validate_id(id)?;
    let text = text.as_bytes();
    ensure!(
        text.len() <= MAX_TEXT,
        "Agent text exceeds {MAX_TEXT} UTF-8 bytes"
    );
    let mut frame = Vec::with_capacity(2 + text.len());
    frame.extend_from_slice(&[Opcode::SetText as u8, id]);
    frame.extend_from_slice(text);
    ensure!(
        frame.len() <= MAX_FRAME,
        "text frame exceeds {MAX_FRAME} bytes"
    );
    Ok(frame)
}

pub fn unregister(id: u8) -> Result<Vec<u8>> {
    validate_id(id)?;
    Ok(vec![Opcode::Unregister as u8, id])
}

fn validate_id(id: u8) -> Result<()> {
    ensure!(
        id < AGENT_COUNT,
        "Agent id must be in 0..{}, got {id}",
        AGENT_COUNT - 1
    );
    Ok(())
}

pub fn opcode(frame: &[u8]) -> Result<Opcode> {
    let value = *frame.first().context("command frame is empty")?;
    match value {
        0x01 => Ok(Opcode::Register),
        0x02 => Ok(Opcode::SetState),
        0x03 => Ok(Opcode::SetText),
        0x04 => Ok(Opcode::Unregister),
        _ => bail!("unsupported command opcode 0x{value:02x}"),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn encodes_register_with_little_endian_key() {
        assert_eq!(
            register(0x1234_5678, "A").unwrap(),
            [1, 0x78, 0x56, 0x34, 0x12, b'A']
        );
    }

    #[test]
    fn validates_utf8_byte_limits() {
        assert!(register(1, "123456789012345").is_ok());
        assert!(register(1, "1234567890123456").is_err());
        assert!(set_text(0, "123456789012345678").is_ok());
        assert!(set_text(0, "1234567890123456789").is_err());
        assert!(register(1, "六六六六六六").is_err());
    }

    #[test]
    fn validates_agent_ids() {
        assert!(set_state(6, AgentState::Working).is_ok());
        assert!(set_state(7, AgentState::Working).is_err());
        assert!(unregister(0xff).is_err());
    }

    #[test]
    fn parses_state_aliases() {
        assert_eq!(
            "wait-permission".parse::<AgentState>().unwrap(),
            AgentState::WaitPermission
        );
        assert_eq!(
            "wait_option".parse::<AgentState>().unwrap(),
            AgentState::WaitOption
        );
        assert!("wat".parse::<AgentState>().is_err());
        assert_eq!(AgentState::ALL.len(), 7);
        for (numeric, state) in AgentState::ALL.iter().enumerate() {
            assert_eq!(*state as usize, numeric);
        }
    }

    #[test]
    fn decodes_response() {
        assert_eq!(
            Response::decode(&[0x80, 1, 0, 4]).unwrap(),
            Response {
                opcode: 1,
                status: Status::Ok,
                agent_id: 4
            }
        );
        assert!(Response::decode(&[0x80, 1, 0]).is_err());
        assert!(Response::decode(&[0x81, 1, 0, 0]).is_err());
        assert!(Response::decode(&[0x80, 1, 0xff, 0]).is_err());
    }
}
