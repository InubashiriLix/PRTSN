use crate::session::SessionAgent;
use std::collections::HashMap;
use std::fs;
use std::io::{BufRead as _, BufReader, Seek as _, SeekFrom};

pub fn detect_interrupted_turns(sessions: &mut HashMap<String, SessionAgent>) {
    for agent in sessions.values_mut() {
        if transcript_has_new_abort(agent) {
            agent.mark_interrupted();
        }
    }
}

fn transcript_has_new_abort(agent: &mut SessionAgent) -> bool {
    let Some(path) = agent.transcript_path.as_ref() else {
        return false;
    };
    let Ok(mut file) = fs::File::open(path) else {
        return false;
    };
    let length = file.metadata().map(|metadata| metadata.len()).unwrap_or(0);
    if length < agent.transcript_offset {
        agent.transcript_offset = 0;
    }
    if file.seek(SeekFrom::Start(agent.transcript_offset)).is_err() {
        return false;
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
    interrupted
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

#[cfg(test)]
mod tests {
    use super::*;
    use crate::protocol::AgentState;
    use std::fs::OpenOptions;
    use std::io::Write as _;

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
    fn ignores_history_and_handles_new_ctrl_c() {
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
