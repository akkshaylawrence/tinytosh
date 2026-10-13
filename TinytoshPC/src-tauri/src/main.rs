#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

use std::sync::mpsc::{self, Sender};
use std::sync::Mutex;
use std::thread;
use std::time::Duration;

use mac_address::get_mac_address;
use tauri::menu::{Menu, MenuItem};
use tauri::tray::{MouseButton, MouseButtonState, TrayIconBuilder, TrayIconEvent};
use tauri::{LogicalSize, Manager, Size, WindowEvent};
use tauri_plugin_autostart::ManagerExt;

use link::{Link, Request, Transport};
use media::MediaStats;

mod link;
mod media;
mod presence;
mod serial;
mod sonos;
mod telemetry;

const FETCH_CONFIG_TIMEOUT: Duration = Duration::from_secs(2);
const SAVE_CONFIG_TIMEOUT: Duration = Duration::from_secs(15);
const SERIAL_CONFIG_POLLS: u32 = 30;
const SERIAL_CONFIG_POLL_DELAY: Duration = Duration::from_millis(100);

const WINDOW_MIN_WIDTH: f64 = 450.0;
const WINDOW_MIN_HEIGHT: f64 = 450.0;
const MINIMIZED_FLAG: &str = "--minimized";

struct AppState {
    link: Mutex<Link>,
    requests: Mutex<Sender<Request>>,
    media_info: Mutex<MediaStats>,
    sonos_media: Mutex<MediaStats>,
    presence: Mutex<presence::Presence>,
}

/// One entry of the window's device list.
#[derive(serde::Serialize)]
struct PortRow {
    label: String,
    transport: Transport,
}

impl From<Transport> for PortRow {
    fn from(transport: Transport) -> Self {
        PortRow { label: transport.label(), transport }
    }
}

#[derive(serde::Serialize)]
struct PortStatus {
    ports: Vec<PortRow>,
    connected: Option<PortRow>,
    status_text: String,
    target_ip: String,
}

#[tauri::command]
fn set_autostart(app: tauri::AppHandle, enable: bool) -> Result<(), String> {
    let autostart_manager = app.autolaunch();
    if enable {
        autostart_manager.enable().map_err(|e| e.to_string())
    } else {
        autostart_manager.disable().map_err(|e| e.to_string())
    }
}

#[tauri::command]
fn check_autostart(app: tauri::AppHandle) -> bool {
    app.autolaunch().is_enabled().unwrap_or(false)
}

#[tauri::command]
fn get_stats(state: tauri::State<AppState>) -> String {
    state.link.lock().unwrap().stats_json.clone()
}

#[tauri::command]
fn get_ports(state: tauri::State<AppState>) -> PortStatus {
    let mut transports: Vec<Transport> = serialport::available_ports()
        .unwrap_or_default()
        .into_iter()
        .map(|port| Transport::Serial { port: port.port_name })
        .collect();

    let link = state.link.lock().unwrap();
    // A port can drop out of the system's list while it is still open here.
    if let Some(serial @ Transport::Serial { .. }) = &link.connected {
        if !transports.contains(serial) {
            transports.insert(0, serial.clone());
        }
    }
    transports.extend(link.discovered.iter().map(|(ip, name)| Transport::Wifi { ip: ip.clone(), name: name.clone() }));

    let target_ip = match &link.connected {
        Some(Transport::Wifi { ip, .. }) => ip.clone(),
        _ => link.discovered.keys().next().cloned().unwrap_or_default(),
    };
    PortStatus {
        ports: transports.into_iter().map(PortRow::from).collect(),
        connected: link.connected.clone().map(PortRow::from),
        status_text: link.status.clone(),
        target_ip,
    }
}

#[tauri::command]
fn get_logs(state: tauri::State<AppState>) -> Vec<String> {
    state.link.lock().unwrap().logs.drain(..).collect()
}

#[tauri::command]
fn toggle_logging(state: tauri::State<AppState>, enable: bool) {
    let mut link = state.link.lock().unwrap();
    link.logging = enable;
    if !enable {
        link.logs.clear();
    }
}

/// Connects to `target`, or disconnects and stays disconnected when there is none.
#[tauri::command]
fn toggle_connection(state: tauri::State<AppState>, target: Option<Transport>) -> Result<(), String> {
    let request = target.map_or(Request::Disconnect, Request::Connect);
    state.requests.lock().unwrap().send(request).map_err(|e| e.to_string())
}

fn fetch_device_data_blocking(app: &tauri::AppHandle) -> Result<String, String> {
    let state = app.state::<AppState>();
    let connected = state.link.lock().unwrap().connected.clone();
    match connected {
        Some(Transport::Wifi { ip, .. }) => ureq::builder()
            .timeout(FETCH_CONFIG_TIMEOUT)
            .build()
            .get(&format!("http://{}/update", ip))
            .call()
            .map_err(|e| e.to_string())?
            .into_string()
            .map_err(|e| e.to_string()),
        Some(Transport::Serial { .. }) => {
            {
                let mut link = state.link.lock().unwrap();
                link.latest_config.clear();
                link.outbox.push("GET_UPDATE\n".to_string());
            }
            for _ in 0..SERIAL_CONFIG_POLLS {
                let config = state.link.lock().unwrap().latest_config.clone();
                if !config.is_empty() {
                    return Ok(config);
                }
                thread::sleep(SERIAL_CONFIG_POLL_DELAY);
            }
            Err("Serial timeout".into())
        }
        None => Err("Not connected".into()),
    }
}

#[tauri::command]
async fn fetch_device_data(app: tauri::AppHandle) -> Result<String, String> {
    tauri::async_runtime::spawn_blocking(move || fetch_device_data_blocking(&app)).await.map_err(|e| e.to_string())?
}

fn save_device_settings_blocking(app: &tauri::AppHandle, json_payload: &str) -> Result<(), String> {
    let state = app.state::<AppState>();
    let connected = state.link.lock().unwrap().connected.clone();
    match connected {
        Some(Transport::Wifi { ip, .. }) => ureq::builder()
            .timeout(SAVE_CONFIG_TIMEOUT)
            .build()
            .post(&format!("http://{}/save", ip))
            .set("Content-Type", "application/json")
            .send_string(json_payload)
            .map(|_| ())
            .map_err(|e| e.to_string()),
        Some(Transport::Serial { .. }) => {
            state.link.lock().unwrap().outbox.push(format!("SAVE_CFG:{}\n", json_payload));
            Ok(())
        }
        None => Err("Not connected".into()),
    }
}

#[tauri::command]
async fn save_device_settings(app: tauri::AppHandle, json_payload: String) -> Result<(), String> {
    tauri::async_runtime::spawn_blocking(move || save_device_settings_blocking(&app, &json_payload)).await.map_err(|e| e.to_string())?
}

struct PresenceMenuItem(MenuItem<tauri::Wry>);

fn refresh_presence_menu(app: &tauri::AppHandle) {
    let enabled = app.state::<AppState>().presence.lock().unwrap().enabled();
    let label = if enabled { "Disable Presence" } else { "Enable Presence" };
    let _ = app.state::<PresenceMenuItem>().0.set_text(label);
}

fn show_window_safely(app_handle: &tauri::AppHandle, window: tauri::WebviewWindow) {
    #[cfg(target_os = "macos")]
    let _ = app_handle.set_activation_policy(tauri::ActivationPolicy::Regular);

    let _ = window.set_min_size(Some(Size::Logical(LogicalSize { width: WINDOW_MIN_WIDTH, height: WINDOW_MIN_HEIGHT })));
    let _ = window.unminimize();
    let _ = window.show();
    let _ = window.set_focus();
}

fn hide_window(app_handle: &tauri::AppHandle, window: &tauri::WebviewWindow) {
    let _ = window.hide();
    #[cfg(target_os = "macos")]
    let _ = app_handle.set_activation_policy(tauri::ActivationPolicy::Accessory);
}

/// Names this run of the app to the device, which answers one PC at a time.
fn pc_id() -> String {
    let pid = std::process::id();
    match get_mac_address() {
        Ok(Some(mac)) => {
            let mac = mac.to_string().replace(':', "").to_lowercase();
            match mac.len().checked_sub(4) {
                Some(start) => format!("pc-{}:{}", &mac[start..], pid),
                None => format!("pc-fallback-{}", pid),
            }
        }
        _ => format!("pc-fallback-{}", pid),
    }
}

fn build_tray(app: &tauri::App) -> tauri::Result<()> {
    let quit_i = MenuItem::with_id(app, "quit", "Quit", true, None::<&str>)?;
    let show_i = MenuItem::with_id(app, "show", "Show", true, None::<&str>)?;
    let presence_i = MenuItem::with_id(app, "presence", "", true, None::<&str>)?;
    let menu = Menu::with_items(app, &[&show_i, &presence_i, &quit_i])?;
    app.manage(PresenceMenuItem(presence_i));
    refresh_presence_menu(app.handle());
    let icon = app.default_window_icon().unwrap().clone();

    TrayIconBuilder::new()
        .icon(icon)
        .menu(&menu)
        .on_menu_event(|app: &tauri::AppHandle, event| match event.id().as_ref() {
            "quit" => {
                presence::will_quit(app);
                app.exit(0)
            }
            "presence" => {
                let enabled = !app.state::<AppState>().presence.lock().unwrap().enabled();
                presence::presence_set_enabled(app.clone(), app.state::<AppState>(), enabled);
            }
            "show" => {
                if let Some(window) = app.get_webview_window("main") {
                    show_window_safely(app, window);
                }
            }
            _ => {}
        })
        .on_tray_icon_event(|tray: &tauri::tray::TrayIcon, event| {
            if let TrayIconEvent::Click { button: MouseButton::Left, button_state: MouseButtonState::Up, .. } = event {
                let app = tray.app_handle();
                let Some(window) = app.get_webview_window("main") else { return };
                let in_front = window.is_visible().unwrap_or(false)
                    && !window.is_minimized().unwrap_or(false)
                    && window.is_focused().unwrap_or(false);
                if in_front {
                    hide_window(app, &window);
                } else {
                    show_window_safely(app, window);
                }
            }
        })
        .build(app)?;
    Ok(())
}

fn main() {
    let (requests, incoming_requests) = mpsc::channel();
    let app_state = AppState {
        link: Mutex::new(Link::default()),
        requests: Mutex::new(requests),
        media_info: Mutex::new(MediaStats::default()),
        sonos_media: Mutex::new(MediaStats::default()),
        presence: Mutex::new(presence::Presence::default()),
    };

    tauri::Builder::default()
        .plugin(tauri_plugin_autostart::init(tauri_plugin_autostart::MacosLauncher::LaunchAgent, Some(vec![MINIMIZED_FLAG])))
        .manage(app_state)
        .invoke_handler(tauri::generate_handler![
            get_stats, get_ports, toggle_connection, set_autostart, check_autostart,
            fetch_device_data, save_device_settings, get_logs, toggle_logging,
            presence::presence_get, presence::presence_set_enabled, presence::presence_set_profile,
            presence::presence_trigger, presence::presence_reset, presence::presence_set_watch
        ])
        .setup(move |app| {
            let is_minimized = std::env::args().any(|arg| arg == MINIMIZED_FLAG);

            #[cfg(target_os = "macos")]
            app.set_activation_policy(if is_minimized {
                tauri::ActivationPolicy::Accessory
            } else {
                tauri::ActivationPolicy::Regular
            });

            presence::start(app.handle());
            build_tray(app)?;

            media::spawn(app.handle().clone());
            sonos::spawn(app.handle().clone());
            link::spawn_discovery(app.handle().clone());
            link::spawn(app.handle().clone(), pc_id(), incoming_requests);

            if !is_minimized {
                if let Some(window) = app.get_webview_window("main") {
                    show_window_safely(app.handle(), window);
                }
            }
            Ok(())
        })
        .on_window_event(|window, event| {
            if let WindowEvent::CloseRequested { api, .. } = event {
                api.prevent_close();
                let _ = window.hide();
                #[cfg(target_os = "macos")]
                let _ = window.app_handle().set_activation_policy(tauri::ActivationPolicy::Accessory);
            }
        })
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
