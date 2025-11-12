use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Config {
    pub server_url: String,
    pub api_key: String,
    pub poll_interval_secs: u64,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            server_url: "http://localhost:8081".to_string(),
            api_key: String::new(),
            poll_interval_secs: 5,
        }
    }
}
