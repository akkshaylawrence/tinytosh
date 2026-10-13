//! The lines a device writes to its serial port.

const SYS_UPDATE_PREFIX: &str = "SYS_UPDATE:";
const PRESENCE_PREFIX: &str = "PRESENCE:";
const MAX_BUFFER_SIZE: usize = 16384;

#[derive(Debug, PartialEq)]
pub(crate) enum Line {
    /// The device's settings and status, as JSON. Its answer to `GET_UPDATE`.
    SysUpdate(String),
    /// The Bluetooth mouse's status, as JSON.
    Presence(String),
    /// Anything else the firmware prints.
    Log(String),
}

/// Takes the complete lines out of `buffer` and leaves a trailing partial line for next time.
/// A buffer that grows past its limit with no line end in it is noise, and is dropped.
pub(crate) fn drain_lines(buffer: &mut String) -> Vec<Line> {
    let Some(last_newline) = buffer.rfind('\n') else {
        if buffer.len() > MAX_BUFFER_SIZE {
            buffer.clear();
        }
        return Vec::new();
    };
    let complete: String = buffer.drain(..=last_newline).collect();
    complete
        .lines()
        .map(str::trim)
        .filter(|line| !line.is_empty())
        .map(|line| {
            if let Some(json) = line.strip_prefix(SYS_UPDATE_PREFIX) {
                Line::SysUpdate(json.to_string())
            } else if let Some(json) = line.strip_prefix(PRESENCE_PREFIX) {
                Line::Presence(json.to_string())
            } else {
                Line::Log(line.to_string())
            }
        })
        .collect()
}

/// Whether a serial port could be a Tinytosh: a USB serial adapter, and not a Bluetooth or
/// modem port that would hang or answer nonsense.
pub(crate) fn is_candidate_port(port_name: &str, usb_product: &str) -> bool {
    let name = port_name.to_lowercase();
    let product = usb_product.to_lowercase();
    if ["bluetooth", "bt-", "incoming", "wlan"].iter().any(|word| name.contains(word)) {
        return false;
    }
    ["usb", "acm", "serial", "jtag"].iter().any(|word| name.contains(word))
        || name.starts_with("com")
        || ["cp210", "ch340", "ch910", "esp"].iter().any(|word| product.contains(word))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn drain_lines_sorts_lines_and_keeps_the_partial_one() {
        let mut buffer = "boot ok\r\nSYS_UPDATE:{\"a\":1}\n\nPRESENCE:{\"link\":\"off\"}\nSYS_UP".to_string();
        assert_eq!(
            drain_lines(&mut buffer),
            [
                Line::Log("boot ok".to_string()),
                Line::SysUpdate("{\"a\":1}".to_string()),
                Line::Presence("{\"link\":\"off\"}".to_string()),
            ]
        );
        assert_eq!(buffer, "SYS_UP");

        buffer.push_str("DATE:{}\n");
        assert_eq!(drain_lines(&mut buffer), [Line::SysUpdate("{}".to_string())]);
        assert_eq!(buffer, "");
    }

    #[test]
    fn drain_lines_drops_a_runaway_buffer() {
        let mut short = "no newline yet".to_string();
        assert_eq!(drain_lines(&mut short), []);
        assert_eq!(short, "no newline yet");

        let mut runaway = "x".repeat(MAX_BUFFER_SIZE + 1);
        assert_eq!(drain_lines(&mut runaway), []);
        assert_eq!(runaway, "");
    }

    #[test]
    fn candidate_ports() {
        let table = [
            ("/dev/cu.usbmodem123401", "", true),
            ("/dev/ttyACM0", "", true),
            ("COM7", "", true),
            ("/dev/cu.Bluetooth-Incoming-Port", "", false),
            ("/dev/cu.Earmuffs", "", false),
            ("/dev/cu.Earmuffs", "CP2102 USB to UART", true),
            ("/dev/cu.debug-console", "", false),
            ("/dev/cu.wlan-debug", "USB JTAG", false),
        ];
        for (name, product, expected) in table {
            assert_eq!(is_candidate_port(name, product), expected, "{name} {product}");
        }
    }
}
