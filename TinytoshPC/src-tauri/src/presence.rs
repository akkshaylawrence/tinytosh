//! Host side of the Bluetooth mouse: what the user wants, and when the device may move.
//!
//! The device moves by itself only while it holds a lease, and every stats push carries the
//! whole command, lease included. So a host that sleeps, quits or crashes simply stops
//! renewing, and the device goes still without being told.

use std::collections::BTreeMap;
use std::fs;
use std::path::{Path, PathBuf};
use std::thread;
use std::time::{Duration, Instant};

use serde::{Deserialize, Serialize};
use tauri::Manager;

use crate::AppState;

const SERIAL_LEASE_MS: u16 = 5000;
const WIFI_LEASE_MS: u16 = 10000;
const STATUS_STALE_AFTER: Duration = Duration::from_secs(8);
// A missed wake notification must not hold the device still for good. Instant does not
// advance while the machine sleeps, so this counts awake time only.
const SLEEP_LATCH: Duration = Duration::from_secs(20);
const REVOKE_WAIT: Duration = Duration::from_millis(1500);

const PROFILES: [(u8, &str); 3] = [(1, "Stealth"), (2, "Balanced"), (3, "Aggressive")];

#[derive(Serialize, Deserialize, Clone, PartialEq, Debug)]
struct Saved {
    enabled: bool,
    profile: u8,
    /// Bundle id to display name. Empty means the device may move whenever it is enabled.
    #[serde(default)]
    watch: BTreeMap<String, String>,
}

impl Default for Saved {
    fn default() -> Self {
        Saved { enabled: false, profile: 2, watch: BTreeMap::new() }
    }
}

/// The `presence` object of a stats message.
#[derive(Serialize, Clone, Default, PartialEq, Debug)]
pub(crate) struct Command {
    session: u16,
    lease: bool,
    lease_ms: u16,
    paused: bool,
    profile: u8,
    trigger_seq: u16,
    reset_seq: u16,
    /// Shown on the device, and kept there while this app is too asleep to say anything else.
    host_asleep: bool,
}

/// What the device reports back.
#[derive(Serialize, Deserialize, Clone, PartialEq, Debug)]
pub(crate) struct DeviceStatus {
    link: String,
    paused: bool,
    leased: bool,
    moving: bool,
    profile: u8,
    last_gesture: u8,
    next_sec: u8,
    pos_x: i8,
    pos_y: i8,
    count: u16,
}

#[derive(Clone, Copy, PartialEq, Debug)]
struct LeaseTerms {
    enabled: bool,
    awake: bool,
    display: bool,
    watch_open: bool,
}

impl LeaseTerms {
    fn granted(&self) -> bool {
        self.enabled && self.awake && self.display && self.watch_open
    }

    fn host_asleep(&self) -> bool {
        !self.awake || !self.display
    }
}

#[derive(Serialize)]
struct WatchRow {
    id: String,
    name: String,
    checked: bool,
}

#[derive(Serialize)]
pub(crate) struct View {
    enabled: bool,
    profile: u8,
    status: String,
    reachable: bool,
    watch_supported: bool,
    watch: Vec<WatchRow>,
}

pub(crate) struct Presence {
    path: Option<PathBuf>,
    saved: Saved,
    session: u16,
    trigger_seq: u16,
    reset_seq: u16,
    sleep_started: Option<Instant>,
    quitting: bool,
    last_sent: Option<Command>,
    device: Option<(DeviceStatus, Instant)>,
}

impl Default for Presence {
    fn default() -> Self {
        Presence {
            path: None,
            saved: Saved::default(),
            // Tells the device that counters from an earlier run of this app mean nothing now.
            session: std::process::id() as u16,
            trigger_seq: 0,
            reset_seq: 0,
            sleep_started: None,
            quitting: false,
            last_sent: None,
            device: None,
        }
    }
}

/// An unreadable file reads as disabled: unknown intent must hold the device still.
fn load(path: &Path) -> Saved {
    let Ok(text) = fs::read_to_string(path) else { return Saved::default() };
    match serde_json::from_str::<Saved>(&text) {
        Ok(saved) if PROFILES.iter().any(|(id, _)| *id == saved.profile) => saved,
        _ => Saved::default(),
    }
}

/// Replaces the file, so a crash leaves the old or the new one and never half of either.
fn save(path: &Path, saved: &Saved) -> std::io::Result<()> {
    if let Some(dir) = path.parent() {
        fs::create_dir_all(dir)?;
    }
    let tmp = path.with_extension("json.tmp");
    fs::write(&tmp, serde_json::to_string_pretty(saved)?)?;
    fs::rename(&tmp, path)
}

fn watch_allows(watch: &BTreeMap<String, String>, running: &BTreeMap<String, String>) -> bool {
    watch.is_empty() || watch.keys().any(|id| running.contains_key(id))
}

/// Watched apps first, then the other running apps, each by name.
fn watch_rows(watch: &BTreeMap<String, String>, running: &BTreeMap<String, String>) -> Vec<WatchRow> {
    let by_name = |apps: Vec<(&String, &String)>, checked: bool| {
        let mut rows: Vec<WatchRow> = apps
            .into_iter()
            .map(|(id, name)| WatchRow { id: id.clone(), name: name.clone(), checked })
            .collect();
        rows.sort_by_key(|row| row.name.to_lowercase());
        rows
    };
    let mut rows = by_name(watch.iter().collect(), true);
    rows.extend(by_name(running.iter().filter(|(id, _)| !watch.contains_key(*id)).collect(), false));
    rows
}

fn waiting_label(watch: &BTreeMap<String, String>) -> String {
    let mut names: Vec<&str> = watch.values().map(String::as_str).collect();
    names.sort_by_key(|name| name.to_lowercase());
    if names.len() <= 2 {
        format!("Waiting for {}", names.join(" or "))
    } else {
        format!("Waiting for {} apps", names.len())
    }
}

fn status_line(saved: &Saved, terms: LeaseTerms, device: Option<&DeviceStatus>) -> String {
    let Some(device) = device else { return "No device".to_string() };
    match device.link.as_str() {
        "off" => return "Bluetooth mouse is off in the device settings".to_string(),
        "connected" => {}
        _ => return "Not paired: connect \"Tinytosh Mouse\" in Bluetooth settings".to_string(),
    }
    let profile = PROFILES.iter().find(|(id, _)| *id == saved.profile).map_or("", |(_, name)| name);
    let state = if !saved.enabled {
        "Paused".to_string()
    } else if !terms.watch_open {
        waiting_label(&saved.watch)
    } else if device.leased {
        return format!("Active \u{2014} {} \u{00b7} {} gestures", profile, device.count);
    } else {
        "Standby".to_string()
    };
    format!("{} \u{2014} {}", state, profile)
}

impl Presence {
    pub(crate) fn open(&mut self, path: PathBuf) {
        self.saved = load(&path);
        self.path = Some(path);
    }

    fn store(&self) {
        if let Some(path) = &self.path {
            if let Err(error) = save(path, &self.saved) {
                eprintln!("presence: could not save {}: {}", path.display(), error);
            }
        }
    }

    fn terms(&self, running: &BTreeMap<String, String>) -> LeaseTerms {
        LeaseTerms {
            enabled: self.saved.enabled && !self.quitting,
            awake: self.sleep_started.is_none_or(|at| at.elapsed() > SLEEP_LATCH),
            display: platform::display_awake(),
            watch_open: watch_allows(&self.saved.watch, running),
        }
    }

    /// The command for this tick's stats push.
    pub(crate) fn command(&self, over_wifi: bool) -> Command {
        let running = if self.saved.watch.is_empty() { BTreeMap::new() } else { platform::running_apps() };
        let terms = self.terms(&running);
        Command {
            session: self.session,
            lease: terms.granted(),
            lease_ms: if over_wifi { WIFI_LEASE_MS } else { SERIAL_LEASE_MS },
            paused: !self.saved.enabled,
            profile: self.saved.profile,
            trigger_seq: self.trigger_seq,
            reset_seq: self.reset_seq,
            host_asleep: terms.host_asleep(),
        }
    }

    pub(crate) fn sent(&mut self, command: &Command) {
        self.last_sent = Some(command.clone());
    }

    pub(crate) fn link_lost(&mut self) {
        self.last_sent = None;
        self.device = None;
    }

    /// Takes a `PRESENCE:` line's JSON, or the `presence` member of a `/pc-stats` reply.
    pub(crate) fn receive(&mut self, json: &str) {
        if let Ok(status) = serde_json::from_str::<DeviceStatus>(json) {
            self.device = Some((status, Instant::now()));
        }
    }

    fn fresh_device(&self) -> Option<&DeviceStatus> {
        self.device.as_ref().filter(|(_, at)| at.elapsed() < STATUS_STALE_AFTER).map(|(status, _)| status)
    }

    pub(crate) fn enabled(&self) -> bool {
        self.saved.enabled
    }

    fn view(&self) -> View {
        let running = platform::running_apps();
        let device = self.fresh_device();
        View {
            enabled: self.saved.enabled,
            profile: self.saved.profile,
            status: status_line(&self.saved, self.terms(&running), device),
            reachable: device.is_some_and(|d| d.link == "connected"),
            watch_supported: platform::WATCH_SUPPORTED,
            watch: watch_rows(&self.saved.watch, &running),
        }
    }
}

/// Changes what the next command says and waits for the main loop to say it to the device.
/// Used where the process is about to stop running: system sleep and quit. Nothing to wait
/// for when no device is connected.
fn tell_and_wait(app: &tauri::AppHandle, mark: impl FnOnce(&mut Presence), told: fn(&Command) -> bool) {
    let state = app.state::<AppState>();
    mark(&mut state.presence.lock().unwrap());
    let deadline = Instant::now() + REVOKE_WAIT;
    while Instant::now() < deadline {
        match &state.presence.lock().unwrap().last_sent {
            Some(sent) if !told(sent) => {}
            _ => return,
        }
        thread::sleep(Duration::from_millis(20));
    }
}

pub(crate) fn will_sleep(app: &tauri::AppHandle) {
    tell_and_wait(app, |presence| presence.sleep_started = Some(Instant::now()), |sent| sent.host_asleep);
}

pub(crate) fn did_wake(app: &tauri::AppHandle) {
    app.state::<AppState>().presence.lock().unwrap().sleep_started = None;
}

pub(crate) fn will_quit(app: &tauri::AppHandle) {
    tell_and_wait(app, |presence| presence.quitting = true, |sent| !sent.lease);
}

pub(crate) fn start(app: &tauri::AppHandle) {
    if let Ok(dir) = app.path().app_config_dir() {
        app.state::<AppState>().presence.lock().unwrap().open(dir.join("presence.json"));
    }
    platform::watch_sleep(app.clone());
}

#[tauri::command]
pub(crate) fn presence_get(state: tauri::State<AppState>) -> View {
    state.presence.lock().unwrap().view()
}

#[tauri::command]
pub(crate) fn presence_set_enabled(app: tauri::AppHandle, state: tauri::State<AppState>, enabled: bool) {
    {
        let mut presence = state.presence.lock().unwrap();
        presence.saved.enabled = enabled;
        presence.store();
    }
    crate::refresh_presence_menu(&app);
}

#[tauri::command]
pub(crate) fn presence_set_profile(state: tauri::State<AppState>, profile: u8) -> Result<(), String> {
    if !PROFILES.iter().any(|(id, _)| *id == profile) {
        return Err("Unknown profile".into());
    }
    let mut presence = state.presence.lock().unwrap();
    presence.saved.profile = profile;
    presence.store();
    Ok(())
}

#[tauri::command]
pub(crate) fn presence_trigger(state: tauri::State<AppState>) {
    let mut presence = state.presence.lock().unwrap();
    presence.trigger_seq = presence.trigger_seq.wrapping_add(1);
}

#[tauri::command]
pub(crate) fn presence_reset(state: tauri::State<AppState>) {
    let mut presence = state.presence.lock().unwrap();
    presence.reset_seq = presence.reset_seq.wrapping_add(1);
}

#[tauri::command]
pub(crate) fn presence_set_watch(state: tauri::State<AppState>, id: String, name: String, checked: bool) {
    let mut presence = state.presence.lock().unwrap();
    if checked {
        presence.saved.watch.insert(id, name);
    } else {
        presence.saved.watch.remove(&id);
    }
    presence.store();
}

#[cfg(target_os = "macos")]
mod platform {
    use std::collections::BTreeMap;
    use std::ptr::NonNull;

    use block2::RcBlock;
    use objc2_app_kit::{
        NSApplicationActivationPolicy, NSWorkspace, NSWorkspaceDidWakeNotification, NSWorkspaceWillSleepNotification,
    };
    use objc2_foundation::NSNotification;

    pub(super) const WATCH_SUPPORTED: bool = true;

    #[link(name = "CoreGraphics", kind = "framework")]
    extern "C" {
        fn CGMainDisplayID() -> u32;
        fn CGDisplayIsAsleep(display: u32) -> i32;
    }

    /// False during Power Nap and other dark wakes, which keep the display off.
    pub(super) fn display_awake() -> bool {
        unsafe { CGDisplayIsAsleep(CGMainDisplayID()) == 0 }
    }

    /// Bundle id to name of the running apps that appear in the Dock.
    pub(super) fn running_apps() -> BTreeMap<String, String> {
        let mut apps = BTreeMap::new();
        let workspace = NSWorkspace::sharedWorkspace();
        for app in workspace.runningApplications().iter() {
            if app.activationPolicy() != NSApplicationActivationPolicy::Regular || app.isTerminated() {
                continue;
            }
            let Some(id) = app.bundleIdentifier() else { continue };
            let name = app.localizedName().map_or_else(|| id.to_string(), |name| name.to_string());
            apps.insert(id.to_string(), name);
        }
        apps
    }

    /// macOS holds sleep while the will-sleep handler runs, which is what gives the revoke
    /// time to reach the device. Call from the main thread.
    pub(super) fn watch_sleep(app: tauri::AppHandle) {
        let center = NSWorkspace::sharedWorkspace().notificationCenter();
        let sleeping = app.clone();
        let will_sleep = RcBlock::new(move |_: NonNull<NSNotification>| super::will_sleep(&sleeping));
        let did_wake = RcBlock::new(move |_: NonNull<NSNotification>| super::did_wake(&app));
        unsafe {
            // The observers live as long as the app does.
            std::mem::forget(center.addObserverForName_object_queue_usingBlock(
                Some(NSWorkspaceWillSleepNotification),
                None,
                None,
                &will_sleep,
            ));
            std::mem::forget(center.addObserverForName_object_queue_usingBlock(
                Some(NSWorkspaceDidWakeNotification),
                None,
                None,
                &did_wake,
            ));
        }
    }
}

#[cfg(not(target_os = "macos"))]
mod platform {
    use std::collections::BTreeMap;

    pub(super) const WATCH_SUPPORTED: bool = false;

    pub(super) fn display_awake() -> bool {
        true
    }

    pub(super) fn running_apps() -> BTreeMap<String, String> {
        BTreeMap::new()
    }

    pub(super) fn watch_sleep(_app: tauri::AppHandle) {}
}

#[cfg(test)]
mod tests {
    use super::*;

    fn apps(pairs: &[(&str, &str)]) -> BTreeMap<String, String> {
        pairs.iter().map(|(id, name)| (id.to_string(), name.to_string())).collect()
    }

    fn device(link: &str, leased: bool) -> DeviceStatus {
        DeviceStatus {
            link: link.to_string(),
            paused: false,
            leased,
            moving: false,
            profile: 2,
            last_gesture: 0,
            next_sec: 0,
            pos_x: 0,
            pos_y: 0,
            count: 1204,
        }
    }

    const OPEN: LeaseTerms = LeaseTerms { enabled: true, awake: true, display: true, watch_open: true };

    #[test]
    fn lease_needs_every_term() {
        assert!(OPEN.granted());
        assert!(!LeaseTerms { enabled: false, ..OPEN }.granted());
        assert!(!LeaseTerms { awake: false, ..OPEN }.granted());
        assert!(!LeaseTerms { display: false, ..OPEN }.granted());
        assert!(!LeaseTerms { watch_open: false, ..OPEN }.granted());
    }

    #[test]
    fn host_is_asleep_when_the_system_or_the_display_is() {
        assert!(!OPEN.host_asleep());
        assert!(LeaseTerms { awake: false, ..OPEN }.host_asleep());
        assert!(LeaseTerms { display: false, ..OPEN }.host_asleep());
        assert!(!LeaseTerms { enabled: false, watch_open: false, ..OPEN }.host_asleep());
    }

    #[test]
    fn empty_watch_list_allows_motion_any_time() {
        assert!(watch_allows(&apps(&[]), &apps(&[])));
        assert!(watch_allows(&apps(&[]), &apps(&[("a", "A")])));
        assert!(!watch_allows(&apps(&[("teams", "Teams")]), &apps(&[("a", "A")])));
        assert!(watch_allows(&apps(&[("teams", "Teams"), ("zoom", "Zoom")]), &apps(&[("zoom", "Zoom")])));
    }

    #[test]
    fn watch_rows_list_watched_apps_first() {
        let rows = watch_rows(&apps(&[("z", "zed"), ("t", "Teams")]), &apps(&[("t", "Teams"), ("s", "Safari"), ("a", "arc")]));
        let seen: Vec<(&str, bool)> = rows.iter().map(|row| (row.name.as_str(), row.checked)).collect();
        assert_eq!(seen, [("Teams", true), ("zed", true), ("arc", false), ("Safari", false)]);
    }

    #[test]
    fn status_line_over_the_states() {
        let on = Saved { enabled: true, ..Saved::default() };
        let off = Saved::default();
        let watching = Saved { watch: apps(&[("t", "Microsoft Teams")]), ..on.clone() };
        let closed = LeaseTerms { watch_open: false, ..OPEN };

        assert_eq!(status_line(&on, OPEN, None), "No device");
        assert!(status_line(&on, OPEN, Some(&device("off", false))).starts_with("Bluetooth mouse is off"));
        assert!(status_line(&on, OPEN, Some(&device("advertising", false))).starts_with("Not paired"));
        assert_eq!(status_line(&on, OPEN, Some(&device("connected", true))), "Active — Balanced · 1204 gestures");
        assert_eq!(status_line(&on, OPEN, Some(&device("connected", false))), "Standby — Balanced");
        assert_eq!(status_line(&off, OPEN, Some(&device("connected", false))), "Paused — Balanced");
        assert_eq!(status_line(&watching, closed, Some(&device("connected", false))), "Waiting for Microsoft Teams — Balanced");
    }

    #[test]
    fn saved_state_round_trips_and_bad_files_read_as_disabled() {
        let path = std::env::temp_dir().join(format!("tinytosh-presence-test-{}.json", std::process::id()));
        let saved = Saved { enabled: true, profile: 3, watch: apps(&[("t", "Teams")]) };
        save(&path, &saved).unwrap();
        assert_eq!(load(&path), saved);

        fs::write(&path, "{\"enabled\": true, \"profile\": 9}").unwrap();
        assert_eq!(load(&path), Saved::default());
        fs::write(&path, "not json").unwrap();
        assert_eq!(load(&path), Saved::default());
        fs::remove_file(&path).unwrap();
        assert_eq!(load(&path), Saved::default());
    }

    #[test]
    fn command_carries_intent_and_a_longer_lease_over_wifi() {
        let mut presence = Presence {
            saved: Saved { enabled: false, profile: 1, watch: BTreeMap::new() },
            trigger_seq: 4,
            ..Presence::default()
        };
        let serial = presence.command(false);
        assert!(serial.paused && !serial.lease);
        assert_eq!((serial.profile, serial.trigger_seq, serial.lease_ms), (1, 4, SERIAL_LEASE_MS));
        assert_eq!(presence.command(true).lease_ms, WIFI_LEASE_MS);

        presence.saved.enabled = true;
        presence.quitting = true;
        assert!(!presence.command(false).lease);
    }

    #[test]
    fn device_status_parses_the_firmware_json() {
        let mut presence = Presence::default();
        presence.receive(r#"{"link":"connected","paused":false,"leased":true,"moving":false,"profile":2,"last_gesture":1,"next_sec":14,"pos_x":-12,"pos_y":6,"count":3}"#);
        assert_eq!(presence.fresh_device().map(|d| (d.pos_x, d.count)), Some((-12, 3)));
        presence.link_lost();
        assert!(presence.fresh_device().is_none());
    }
}
