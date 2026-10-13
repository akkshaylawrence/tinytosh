//! The connection to a device: finding one over USB or Wi-Fi, and pushing stats to it.
//!
//! One thread owns the connection. It runs once a second, and everything else talks to it
//! through `Link`, which it never holds across a sleep or a request, and through a channel of
//! `Request`s from the user.

use std::collections::{BTreeMap, VecDeque};
use std::io::{Read, Write};
use std::sync::mpsc::Receiver;
use std::thread;
use std::time::Duration;

use mdns_sd::{ServiceDaemon, ServiceEvent};
use serialport::{SerialPort, SerialPortType};
use tauri::Manager;

use crate::media::{pick_media, MediaStats};
use crate::presence;
use crate::serial::{self, Line};
use crate::telemetry::{Sample, Telemetry};
use crate::AppState;

const TICK: Duration = Duration::from_millis(1000);

// The device's web server handles one request at a time, so Wi-Fi stats go every third tick.
const WIFI_PUSH_EVERY_TICKS: u32 = 3;
const MAX_WIFI_FAILURES: u32 = 5;
const HTTP_TIMEOUT: Duration = Duration::from_millis(500);

const SERIAL_BAUD_RATE: u32 = 115_200;
const SERIAL_TIMEOUT: Duration = Duration::from_millis(100);
const SERIAL_HANDSHAKE_TICKS: u32 = 5;
const SERIAL_CHUNK_SIZE: usize = 64;
const SERIAL_CHUNK_DELAY: Duration = Duration::from_millis(5);
const SERIAL_COMMAND_DELAY: Duration = Duration::from_millis(150);
const SERIAL_READ_BUFFER_SIZE: usize = 1024;

const MAX_LOG_LINES: usize = 500;
const MDNS_SERVICE_TYPE: &str = "_http._tcp.local.";

#[derive(serde::Serialize, serde::Deserialize, Clone, PartialEq, Debug)]
#[serde(tag = "kind", rename_all = "lowercase")]
pub(crate) enum Transport {
    Serial { port: String },
    Wifi { ip: String, name: String },
}

impl Transport {
    pub(crate) fn label(&self) -> String {
        match self {
            Transport::Serial { port } => format!("Serial: {}", port),
            Transport::Wifi { ip, name } => format!("WiFi: {} ({})", name, ip),
        }
    }
}

pub(crate) enum Request {
    Connect(Transport),
    Disconnect,
}

/// What the connection thread shares with the commands.
pub(crate) struct Link {
    pub(crate) connected: Option<Transport>,
    pub(crate) status: String,
    /// IP address to mDNS name of the devices seen on the network.
    pub(crate) discovered: BTreeMap<String, String>,
    /// The last stats message, for the window's own readout.
    pub(crate) stats_json: String,
    /// The device's last answer to `GET_UPDATE` over serial. Empty until one arrives.
    pub(crate) latest_config: String,
    /// Lines waiting to be written to the serial port.
    pub(crate) outbox: Vec<String>,
    pub(crate) logging: bool,
    pub(crate) logs: VecDeque<String>,
}

impl Default for Link {
    fn default() -> Self {
        Link {
            connected: None,
            status: "Waiting for connection...".to_string(),
            discovered: BTreeMap::new(),
            stats_json: "{}".to_string(),
            latest_config: String::new(),
            outbox: Vec::new(),
            logging: false,
            logs: VecDeque::new(),
        }
    }
}

#[derive(serde::Serialize)]
struct StatsMessage<'a> {
    pc_id: &'a str,
    #[serde(flatten)]
    sample: Sample,
    #[serde(flatten)]
    media: MediaStats,
    presence: &'a presence::Command,
}

enum PostError {
    /// The device is answering another PC.
    PairedElsewhere,
    Unreachable,
}

enum Phase {
    /// The user disconnected. Stays here until they connect again.
    Paused,
    Scan,
    TestSerial { port: String, ticks: u32 },
    TestWifi { ip: String },
    Serial,
    Wifi { ip: String, ticks: u32, failures: u32 },
}

struct Worker {
    app: tauri::AppHandle,
    agent: ureq::Agent,
    phase: Phase,
    serial_candidates: Vec<String>,
    wifi_candidates: Vec<String>,
    candidate: usize,
    port: Option<Box<dyn SerialPort>>,
    serial_buffer: String,
}

/// Follows mDNS for devices that announce themselves as tinytosh.
pub(crate) fn spawn_discovery(app: tauri::AppHandle) {
    thread::spawn(move || {
        let Ok(mdns) = ServiceDaemon::new() else { return };
        let Ok(receiver) = mdns.browse(MDNS_SERVICE_TYPE) else { return };
        let state = app.state::<AppState>();
        let mut addresses: BTreeMap<String, String> = BTreeMap::new();

        while let Ok(event) = receiver.recv() {
            match event {
                ServiceEvent::ServiceResolved(info) => {
                    let fullname = info.get_fullname().to_lowercase();
                    if !fullname.contains("tinytosh") {
                        continue;
                    }
                    let Some(ip) = info.get_addresses().iter().next().map(|ip| ip.to_string()) else { continue };
                    let name = fullname.replace(&format!(".{}", MDNS_SERVICE_TYPE), "");
                    addresses.insert(fullname, ip.clone());
                    state.link.lock().unwrap().discovered.insert(ip, name);
                }
                ServiceEvent::ServiceRemoved(_, fullname) => {
                    if let Some(ip) = addresses.remove(&fullname.to_lowercase()) {
                        state.link.lock().unwrap().discovered.remove(&ip);
                    }
                }
                _ => {}
            }
        }
    });
}

pub(crate) fn spawn(app: tauri::AppHandle, pc_id: String, requests: Receiver<Request>) {
    thread::spawn(move || {
        let mut telemetry = Telemetry::new();
        let mut worker = Worker {
            agent: ureq::builder()
                .timeout_connect(HTTP_TIMEOUT)
                .timeout_read(HTTP_TIMEOUT)
                .timeout_write(HTTP_TIMEOUT)
                .build(),
            app,
            phase: Phase::Scan,
            serial_candidates: Vec::new(),
            wifi_candidates: Vec::new(),
            candidate: 0,
            port: None,
            serial_buffer: String::new(),
        };

        loop {
            let state = worker.app.state::<AppState>();
            let media = pick_media(&state.media_info.lock().unwrap(), &state.sonos_media.lock().unwrap());
            let over_wifi = matches!(worker.phase, Phase::TestWifi { .. } | Phase::Wifi { .. });
            let command = state.presence.lock().unwrap().command(over_wifi);
            let message = StatsMessage { pc_id: &pc_id, sample: telemetry.sample(), media, presence: &command };
            let payload = serde_json::to_string(&message).unwrap_or_else(|_| "{}".to_string());
            state.link.lock().unwrap().stats_json = payload.clone();

            // A request acts now and its first step runs on this same tick.
            for request in requests.try_iter() {
                worker.handle(request);
            }
            worker.step(&payload, &command);
            thread::sleep(TICK);
        }
    });
}

impl Worker {
    fn link<T>(&self, change: impl FnOnce(&mut Link) -> T) -> T {
        change(&mut self.app.state::<AppState>().link.lock().unwrap())
    }

    fn set_status(&self, status: impl Into<String>) {
        let status = status.into();
        self.link(|link| link.status = status);
    }

    fn drop_connection(&mut self) {
        self.port = None;
        self.serial_buffer.clear();
        self.link(|link| link.connected = None);
        self.app.state::<AppState>().presence.lock().unwrap().link_lost();
    }

    fn handle(&mut self, request: Request) {
        self.drop_connection();
        self.serial_candidates.clear();
        self.wifi_candidates.clear();
        self.candidate = 0;
        self.phase = match request {
            Request::Disconnect => {
                self.set_status("⏸️ Disconnected");
                Phase::Paused
            }
            Request::Connect(Transport::Serial { port }) => Phase::TestSerial { port, ticks: 0 },
            Request::Connect(Transport::Wifi { ip, .. }) => Phase::TestWifi { ip },
        };
    }

    fn step(&mut self, payload: &str, command: &presence::Command) {
        // Each arm returns the phase to be in next, and may have taken a turn at it already.
        self.phase = match std::mem::replace(&mut self.phase, Phase::Scan) {
            Phase::Paused => Phase::Paused,
            Phase::Scan => self.scan(),
            Phase::TestSerial { port, ticks } => self.test_serial(port, ticks),
            Phase::TestWifi { ip } => self.test_wifi(ip, payload, command),
            Phase::Serial => self.push_serial(payload, command),
            Phase::Wifi { ip, ticks, failures } => self.push_wifi(ip, ticks, failures, payload, command),
        };
    }

    fn scan(&mut self) -> Phase {
        self.set_status("🔍 Scanning for devices...");
        self.drop_connection();

        self.serial_candidates = serialport::available_ports()
            .unwrap_or_default()
            .into_iter()
            .filter(|port| {
                let product = match &port.port_type {
                    SerialPortType::UsbPort(info) => info.product.clone().unwrap_or_default(),
                    _ => String::new(),
                };
                serial::is_candidate_port(&port.port_name, &product)
            })
            .map(|port| port.port_name)
            .collect();
        self.wifi_candidates = self.link(|link| link.discovered.keys().cloned().collect());
        self.candidate = 0;

        if let Some(port) = self.serial_candidates.first() {
            Phase::TestSerial { port: port.clone(), ticks: 0 }
        } else if let Some(ip) = self.wifi_candidates.first() {
            Phase::TestWifi { ip: ip.clone() }
        } else {
            Phase::Scan
        }
    }

    /// The next serial candidate, then the Wi-Fi ones, then a fresh scan.
    fn after_serial_candidate(&mut self) -> Phase {
        self.port = None;
        self.candidate += 1;
        if let Some(port) = self.serial_candidates.get(self.candidate) {
            return Phase::TestSerial { port: port.clone(), ticks: 0 };
        }
        self.candidate = 0;
        self.wifi_candidates = self.link(|link| link.discovered.keys().cloned().collect());
        match self.wifi_candidates.first() {
            Some(ip) => Phase::TestWifi { ip: ip.clone() },
            None => Phase::Scan,
        }
    }

    fn test_serial(&mut self, port_name: String, ticks: u32) -> Phase {
        if ticks == 0 {
            self.set_status(format!("⚪ Connecting to USB ({})...", port_name));
            let Ok(mut port) = serialport::new(port_name.clone(), SERIAL_BAUD_RATE).timeout(SERIAL_TIMEOUT).open() else {
                return self.after_serial_candidate();
            };
            let _ = port.write(b"GET_UPDATE\n");
            self.port = Some(port);
            self.serial_buffer.clear();
            self.link(|link| link.latest_config.clear());
        }

        if self.read_serial() {
            self.set_status("🔌 Connected via USB");
            self.link(|link| link.connected = Some(Transport::Serial { port: port_name }));
            Phase::Serial
        } else if ticks + 1 >= SERIAL_HANDSHAKE_TICKS {
            self.after_serial_candidate()
        } else {
            Phase::TestSerial { port: port_name, ticks: ticks + 1 }
        }
    }

    /// Reads what the device has written and files each line. Says whether its settings came.
    fn read_serial(&mut self) -> bool {
        let Some(port) = self.port.as_mut() else { return false };
        let mut chunk = [0u8; SERIAL_READ_BUFFER_SIZE];
        while let Ok(count) = port.read(&mut chunk) {
            if count == 0 {
                break;
            }
            self.serial_buffer.push_str(&String::from_utf8_lossy(&chunk[..count]));
        }

        let mut got_settings = false;
        for line in serial::drain_lines(&mut self.serial_buffer) {
            match line {
                Line::SysUpdate(json) => {
                    self.link(|link| link.latest_config = json);
                    got_settings = true;
                }
                Line::Presence(json) => self.app.state::<AppState>().presence.lock().unwrap().receive(&json),
                Line::Log(text) => self.link(|link| {
                    if link.logging {
                        link.logs.push_back(text);
                        while link.logs.len() > MAX_LOG_LINES {
                            link.logs.pop_front();
                        }
                    }
                }),
            }
        }
        got_settings
    }

    fn push_serial(&mut self, payload: &str, command: &presence::Command) -> Phase {
        let outbox = self.link(|link| std::mem::take(&mut link.outbox));
        let Some(port) = self.port.as_mut() else { return Phase::Scan };

        for line in &outbox {
            for chunk in line.as_bytes().chunks(SERIAL_CHUNK_SIZE) {
                let _ = port.write(chunk);
                thread::sleep(SERIAL_CHUNK_DELAY);
            }
        }
        if !outbox.is_empty() {
            thread::sleep(SERIAL_COMMAND_DELAY);
        }

        if port.write(format!("{}\n", payload).as_bytes()).is_err() {
            self.port = None;
            self.link(|link| link.connected = None);
            return Phase::Scan;
        }
        self.app.state::<AppState>().presence.lock().unwrap().sent(command);
        self.read_serial();
        Phase::Serial
    }

    fn post_stats(&self, ip: &str, payload: &str, command: &presence::Command) -> Result<(), PostError> {
        let response = self
            .agent
            .post(&format!("http://{}/pc-stats", ip))
            .set("Content-Type", "application/json")
            .set("Connection", "close")
            .send_string(payload)
            .map_err(|error| match error {
                ureq::Error::Status(403, _) => PostError::PairedElsewhere,
                _ => PostError::Unreachable,
            })?;

        let state = self.app.state::<AppState>();
        let mut presence = state.presence.lock().unwrap();
        presence.sent(command);
        let reply: serde_json::Value = serde_json::from_str(&response.into_string().unwrap_or_default()).unwrap_or_default();
        if let Some(status) = reply.get("presence") {
            presence.receive(&status.to_string());
        }
        Ok(())
    }

    fn test_wifi(&mut self, ip: String, payload: &str, command: &presence::Command) -> Phase {
        self.set_status(format!("⚪ Connecting to Wi-Fi ({})...", ip));
        match self.post_stats(&ip, payload, command) {
            Ok(()) => {
                self.link(|link| {
                    let name = link.discovered.get(&ip).unwrap_or(&ip).clone();
                    link.connected = Some(Transport::Wifi { ip: ip.clone(), name });
                    link.status = "📶 Connected via WiFi".to_string();
                });
                return Phase::Wifi { ip, ticks: 0, failures: 0 };
            }
            Err(PostError::PairedElsewhere) => {
                self.link(|link| {
                    link.status = "❌ Device already paired".to_string();
                    link.discovered.remove(&ip);
                });
            }
            Err(PostError::Unreachable) => {}
        }

        self.candidate += 1;
        match self.wifi_candidates.get(self.candidate) {
            Some(next) => Phase::TestWifi { ip: next.clone() },
            None => Phase::Scan,
        }
    }

    fn push_wifi(&mut self, ip: String, ticks: u32, failures: u32, payload: &str, command: &presence::Command) -> Phase {
        if ticks < WIFI_PUSH_EVERY_TICKS {
            return Phase::Wifi { ip, ticks: ticks + 1, failures };
        }
        match self.post_stats(&ip, payload, command) {
            Ok(()) => {
                self.set_status("📶 Connected via WiFi");
                Phase::Wifi { ip, ticks: 0, failures: 0 }
            }
            Err(_) if failures + 1 >= MAX_WIFI_FAILURES => {
                self.set_status("⏳ Connection lost, scanning...");
                self.link(|link| link.connected = None);
                Phase::Scan
            }
            Err(_) => {
                self.set_status(format!("⏳ Reconnecting ({} / {})...", failures + 1, MAX_WIFI_FAILURES));
                Phase::Wifi { ip, ticks: 0, failures: failures + 1 }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn transport_crosses_to_the_window_tagged_by_kind() {
        let wifi = Transport::Wifi { ip: "10.0.0.5".to_string(), name: "tinytosh-c754".to_string() };
        assert_eq!(serde_json::to_string(&wifi).unwrap(), r#"{"kind":"wifi","ip":"10.0.0.5","name":"tinytosh-c754"}"#);
        assert_eq!(wifi.label(), "WiFi: tinytosh-c754 (10.0.0.5)");

        let serial: Transport = serde_json::from_str(r#"{"kind":"serial","port":"/dev/cu.usbmodem1"}"#).unwrap();
        assert_eq!(serial, Transport::Serial { port: "/dev/cu.usbmodem1".to_string() });
        assert_eq!(serial.label(), "Serial: /dev/cu.usbmodem1");
    }
}
