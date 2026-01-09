// STAR BMS Tool - Floating Window Management
// Texas A&M University

use tauri::{AppHandle, Emitter, Manager, WebviewUrl, WebviewWindow, WebviewWindowBuilder};

/// Create a floating panel window
///
/// Creates a new Tauri window for a detached panel. The window is labeled
/// with "floating-{panel_id}" and navigates to a route that displays only
/// the panel content.
#[tauri::command]
pub async fn create_floating_panel(
    app: AppHandle,
    panel_id: String,
    title: String,
    x: Option<i32>,
    y: Option<i32>,
) -> Result<String, String> {
    let window_label = format!("floating-{}", panel_id);

    // Check if window already exists
    if app.get_webview_window(&window_label).is_some() {
        return Err(format!("Window {} already exists", window_label));
    }

    // Create window URL with panel_id as query parameter
    let url = format!("/#/floating/{}", panel_id);

    // Create new window
    let mut builder = WebviewWindowBuilder::new(&app, &window_label, WebviewUrl::App(url.into()))
        .title(&title)
        .inner_size(800.0, 600.0)
        .min_inner_size(400.0, 300.0)
        .resizable(true)
        .decorations(true);

    if let (Some(x), Some(y)) = (x, y) {
        builder = builder.position(x as f64, y as f64);
    } else {
        builder = builder.center();
    }

    let window: WebviewWindow = builder
        .build()
        .map_err(|e| format!("Failed to create window: {}", e))?;

    // Focus the new window
    window
        .set_focus()
        .map_err(|e| format!("Failed to focus window: {}", e))?;

    Ok(window_label)
}

/// Close a floating panel window
///
/// Closes the floating window with the given label. If the window doesn't
/// exist, this operation succeeds silently.
#[tauri::command]
pub async fn close_floating_panel(app: AppHandle, window_label: String) -> Result<(), String> {
    if let Some(window) = app.get_webview_window(&window_label) {
        window
            .close()
            .map_err(|e| format!("Failed to close window: {}", e))?;
    }
    Ok(())
}

#[tauri::command]
pub async fn close_floating_panel_with_action(
    app: AppHandle,
    window_label: String,
    panel_id: Option<String>,
    action: Option<String>,
) -> Result<(), String> {
    if let (Some(panel_id), Some(action)) = (panel_id, action) {
        if action == "hide" {
            let _ = app.emit("docking:hide-panel", serde_json::json!({ "panelId": panel_id }));
        }
    }
    close_floating_panel(app, window_label).await
}

/// Get list of all floating panel windows
///
/// Returns the labels of all floating panel windows currently open.
#[tauri::command]
pub async fn list_floating_panels(app: AppHandle) -> Result<Vec<String>, String> {
    let windows: Vec<String> = app
        .webview_windows()
        .keys()
        .filter(|label| label.starts_with("floating-"))
        .cloned()
        .collect();

    Ok(windows)
}
