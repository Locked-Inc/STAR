// STAR BMS Tool - Rust/Tauri Implementation
// Texas A&M University

use tauri::menu::{Menu, MenuBuilder, MenuItemBuilder, PredefinedMenuItem, SubmenuBuilder};
use tauri::{AppHandle, Emitter, Runtime};

fn encode_port_id(port: &str) -> String {
    port
        .replace('%', "%25")
        .replace(' ', "%20")
        .replace('/', "%2F")
        .replace('\\', "%5C")
        .replace(':', "%3A")
}

fn decode_port_id(encoded: &str) -> String {
    encoded
        .replace("%3A", ":")
        .replace("%5C", "\\")
        .replace("%2F", "/")
        .replace("%20", " ")
        .replace("%25", "%")
}

fn build_menu<R: Runtime>(app: &AppHandle<R>) -> tauri::Result<Menu<R>> {
    let mut menu = MenuBuilder::new(app);

    #[cfg(target_os = "macos")]
    {
        let app_menu = SubmenuBuilder::new(app, "STAR BMS Tool")
            .item(&PredefinedMenuItem::about(app, None, None)?)
            .separator()
            .item(&PredefinedMenuItem::hide(app, None)?)
            .item(&PredefinedMenuItem::hide_others(app, None)?)
            .item(&PredefinedMenuItem::show_all(app, None)?)
            .separator()
            .item(&PredefinedMenuItem::quit(app, None)?)
            .build()?;
        menu = menu.item(&app_menu);
    }

    let ports = list_serial_ports().unwrap_or_default();
    let ports_menu = if ports.is_empty() {
        SubmenuBuilder::new(app, "Ports")
            .item(
                &MenuItemBuilder::with_id("connection.port:none", "No ports found")
                    .enabled(false)
                    .build(app)?,
            )
            .build()?
    } else {
        let mut ports_submenu = SubmenuBuilder::new(app, "Ports");
        for port in ports {
            let id = format!("connection.port:{}", encode_port_id(&port));
            ports_submenu = ports_submenu.item(&MenuItemBuilder::with_id(id, port).build(app)?);
        }
        ports_submenu.build()?
    };

    let connection_menu = SubmenuBuilder::new(app, "Connection")
        .item(&MenuItemBuilder::with_id("connection.refresh-ports", "Refresh Ports").build(app)?)
        .separator()
        .item(&ports_menu)
        .separator()
        .item(&MenuItemBuilder::with_id("connection.connect", "Connect").build(app)?)
        .item(&MenuItemBuilder::with_id("connection.disconnect", "Disconnect").build(app)?)
        .item(&MenuItemBuilder::with_id("connection.cancel", "Cancel Connection").build(app)?)
        .build()?;

    let view_menu = SubmenuBuilder::new(app, "View")
        .item(&MenuItemBuilder::with_id("view.packet-viewer", "Packet Viewer").build(app)?)
        .item(&MenuItemBuilder::with_id("view.terminal", "Terminal").build(app)?)
        .item(&MenuItemBuilder::with_id("view.device-info", "Device Info").build(app)?)
        .item(&MenuItemBuilder::with_id("view.properties", "Properties").build(app)?)
        .separator()
        .item(&MenuItemBuilder::with_id("view.reset-layout", "Reset Layout").build(app)?)
        .build()?;

    let window_menu = SubmenuBuilder::new(app, "Window")
        .item(&PredefinedMenuItem::minimize(app, None)?)
        .separator()
        .item(&PredefinedMenuItem::fullscreen(app, None)?)
        .build()?;

    let help_menu = SubmenuBuilder::new(app, "Help")
        .item(&MenuItemBuilder::with_id("help.docs", "Documentation").build(app)?)
        .build()?;

    menu
        .item(&connection_menu)
        .item(&view_menu)
        .item(&window_menu)
        .item(&help_menu)
        .build()
}

mod bms;
pub mod cli;
mod floating;
mod frame;
pub mod packet_capture;

pub use bms::*;
pub use floating::*;
pub use frame::*;
pub use packet_capture::*;

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .manage(BmsApp::new())
        .menu(|app| build_menu(app))
        .on_menu_event(|app, event| {
            let id = event.id().0.as_str();
            let payload = if id == "view.packet-viewer" {
                Some(serde_json::json!({ "action": "toggle-panel", "panelId": "packet-viewer" }))
            } else if id == "view.terminal" {
                Some(serde_json::json!({ "action": "toggle-panel", "panelId": "terminal" }))
            } else if id == "view.device-info" {
                Some(serde_json::json!({ "action": "toggle-panel", "panelId": "device-info" }))
            } else if id == "view.properties" {
                Some(serde_json::json!({ "action": "toggle-panel", "panelId": "properties" }))
            } else if id == "view.reset-layout" {
                Some(serde_json::json!({ "action": "reset-layout" }))
            } else if id == "connection.refresh-ports" {
                if let Ok(menu) = build_menu(app) {
                    let _ = app.set_menu(menu);
                }
                Some(serde_json::json!({ "action": "refresh-ports" }))
            } else if id == "connection.connect" {
                Some(serde_json::json!({ "action": "connect" }))
            } else if id == "connection.disconnect" {
                Some(serde_json::json!({ "action": "disconnect" }))
            } else if id == "connection.cancel" {
                Some(serde_json::json!({ "action": "cancel-connection" }))
            } else if id.starts_with("connection.port:") {
                let encoded = id.trim_start_matches("connection.port:");
                Some(serde_json::json!({
                    "action": "select-port",
                    "port": decode_port_id(encoded)
                }))
            } else {
                None
            };

            if let Some(payload) = payload {
                let _ = app.emit("menu:action", payload);
            }
        })
        .invoke_handler(tauri::generate_handler![
            list_serial_ports,
            connect_to_device,
            disconnect_from_device,
            abort_connection,
            is_connected,
            get_device_state,
            read_telemetry,
            read_cell_voltages,
            read_device_info,
            read_register,
            write_register,
            read_block,
            write_block,
            manufacturer_access,
            read_protection_status,
            is_experimental_enabled,
            get_raw_packets,
            get_parsed_packets,
            clear_packet_capture,
            get_packet_count,
            set_packet_capture_enabled,
            create_floating_panel,
            close_floating_panel,
            close_floating_panel_with_action,
            list_floating_panels,
        ])
        .setup(|app| {
            if cfg!(debug_assertions) {
                app.handle().plugin(
                    tauri_plugin_log::Builder::default()
                        .level(log::LevelFilter::Info)
                        .build(),
                )?;
            }
            app.handle().plugin(tauri_plugin_dialog::init())?;
            app.handle().plugin(tauri_plugin_fs::init())?;
            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
