use crate::codex_hook::Activity;
use crate::protocol::AgentState;
use std::fs;
use std::path::PathBuf;

pub struct SessionAgent {
    pub key: u32,
    pub name: String,
    pub state: AgentState,
    pub text: &'static str,
    pub slot: Option<u8>,
    pub remove: bool,
    pub dirty: bool,
    pub transcript_path: Option<PathBuf>,
    pub transcript_offset: u64,
}

impl SessionAgent {
    pub fn new(session_id: &str) -> Self {
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

    pub fn apply(&mut self, activity: Activity) {
        let Some(state) = activity.panel_state() else {
            self.remove = true;
            self.dirty = true;
            return;
        };
        self.state = state;
        self.text = activity.display_text();
        self.remove = false;
        self.dirty = true;
    }

    pub fn set_transcript(&mut self, path: Option<PathBuf>) {
        let Some(path) = path else { return };
        if self.transcript_path.as_ref() == Some(&path) {
            return;
        }
        // Start at EOF so old abort events do not affect a resumed session.
        self.transcript_offset = fs::metadata(&path)
            .map(|metadata| metadata.len())
            .unwrap_or(0);
        self.transcript_path = Some(path);
    }

    pub fn mark_interrupted(&mut self) {
        self.state = AgentState::Idle;
        self.text = "interrupted";
        self.remove = false;
        self.dirty = true;
    }

    pub fn reset_connection(&mut self) {
        self.slot = None;
        self.dirty = true;
    }
}

fn session_key(session_id: &str) -> u32 {
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

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn session_keys_are_stable_nonzero_and_session_specific() {
        assert_eq!(session_key("thr_a"), session_key("thr_a"));
        assert_ne!(session_key("thr_a"), 0);
        assert_ne!(session_key("thr_a"), session_key("thr_b"));
    }
}
