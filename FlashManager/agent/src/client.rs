use anyhow::{Context, Result};
use log::{debug, info};
use reqwest::blocking::Client;
use serde::{Deserialize, Serialize};

use crate::config::Config;

#[derive(Debug, Clone)]
pub struct FlashManagerClient {
    config: Config,
    client: Client,
    access_token: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FlashJob {
    pub id: String,
    pub component: String,
    pub target_device: String,
    pub binary_path: String,
}

#[derive(Debug, Serialize)]
struct GraphQLRequest<V> {
    query: String,
    variables: V,
}

#[derive(Debug, Deserialize)]
struct GraphQLResponse<T> {
    data: Option<T>,
    errors: Option<Vec<GraphQLError>>,
}

#[derive(Debug, Deserialize)]
struct GraphQLError {
    message: String,
}

#[derive(Debug, Deserialize)]
struct LoginData {
    #[serde(rename = "deviceLogin")]
    device_login: AuthPayload,
}

#[derive(Debug, Deserialize)]
struct AuthPayload {
    #[serde(rename = "accessToken")]
    access_token: String,
}

#[derive(Debug, Deserialize)]
struct PollData {
    #[serde(rename = "pollFlashJobs")]
    poll_flash_jobs: Vec<FlashJobResponse>,
}

#[derive(Debug, Deserialize)]
struct FlashJobResponse {
    id: String,
    #[serde(rename = "targetDevice")]
    target_device: String,
    #[serde(rename = "buildJob")]
    build_job: BuildJobResponse,
}

#[derive(Debug, Deserialize)]
struct BuildJobResponse {
    component: String,
    #[serde(rename = "binaryPath")]
    binary_path: Option<String>,
}

#[derive(Debug, Deserialize)]
struct ClaimData {
    #[serde(rename = "claimFlashJob")]
    _claim_flash_job: serde_json::Value,
}

#[derive(Debug, Deserialize)]
struct UpdateStatusData {
    #[serde(rename = "updateFlashStatus")]
    _update_flash_status: serde_json::Value,
}

#[derive(Debug, Deserialize)]
struct CompleteData {
    #[serde(rename = "completeFlashJob")]
    _complete_flash_job: serde_json::Value,
}

#[derive(Debug, Deserialize)]
struct HeartbeatData {
    #[serde(rename = "updateDeviceHeartbeat")]
    _update_device_heartbeat: serde_json::Value,
}

impl FlashManagerClient {
    pub fn new(config: Config) -> Result<Self> {
        let client = Client::builder()
            .timeout(std::time::Duration::from_secs(30))
            .build()?;

        Ok(Self {
            config,
            client,
            access_token: None,
        })
    }

    pub async fn login(&mut self) -> Result<()> {
        let query = r#"
            mutation DeviceLogin($apiKey: String!) {
                deviceLogin(apiKey: $apiKey) {
                    accessToken
                }
            }
        "#;

        #[derive(Serialize)]
        struct Variables {
            #[serde(rename = "apiKey")]
            api_key: String,
        }

        let variables = Variables {
            api_key: self.config.api_key.clone(),
        };

        let response: GraphQLResponse<LoginData> = self
            .send_request(query, variables, false)
            .context("Failed to login")?;

        if let Some(data) = response.data {
            self.access_token = Some(data.device_login.access_token);
            info!("Login successful");
            Ok(())
        } else if let Some(errors) = response.errors {
            anyhow::bail!("Login failed: {}", errors[0].message)
        } else {
            anyhow::bail!("Login failed: No data or errors")
        }
    }

    pub async fn poll_flash_jobs(&self) -> Result<Vec<FlashJob>> {
        let query = r#"
            query PollFlashJobs {
                pollFlashJobs {
                    id
                    targetDevice
                    buildJob {
                        component
                        binaryPath
                    }
                }
            }
        "#;

        #[derive(Serialize)]
        struct Variables {}

        let response: GraphQLResponse<PollData> = self
            .send_request(query, Variables {}, true)
            .context("Failed to poll flash jobs")?;

        if let Some(data) = response.data {
            let jobs: Vec<FlashJob> = data
                .poll_flash_jobs
                .into_iter()
                .filter_map(|job| {
                    job.build_job.binary_path.map(|path| FlashJob {
                        id: job.id,
                        component: job.build_job.component,
                        target_device: job.target_device,
                        binary_path: path,
                    })
                })
                .collect();

            debug!("Polled {} flash jobs", jobs.len());
            Ok(jobs)
        } else {
            Ok(Vec::new())
        }
    }

    pub async fn claim_flash_job(&self, job_id: &str) -> Result<()> {
        let query = r#"
            mutation ClaimFlashJob($flashJobId: ID!) {
                claimFlashJob(flashJobId: $flashJobId) {
                    id
                }
            }
        "#;

        #[derive(Serialize)]
        struct Variables {
            #[serde(rename = "flashJobId")]
            flash_job_id: String,
        }

        let variables = Variables {
            flash_job_id: job_id.to_string(),
        };

        let response: GraphQLResponse<ClaimData> = self
            .send_request(query, variables, true)
            .context("Failed to claim flash job")?;

        if response.data.is_some() {
            Ok(())
        } else if let Some(errors) = response.errors {
            anyhow::bail!("Claim failed: {}", errors[0].message)
        } else {
            anyhow::bail!("Claim failed: No data or errors")
        }
    }

    pub async fn update_flash_status(&self, job_id: &str, status: &str) -> Result<()> {
        let query = r#"
            mutation UpdateFlashStatus($flashJobId: ID!, $status: JobStatus!) {
                updateFlashStatus(flashJobId: $flashJobId, status: $status) {
                    id
                }
            }
        "#;

        #[derive(Serialize)]
        struct Variables {
            #[serde(rename = "flashJobId")]
            flash_job_id: String,
            status: String,
        }

        let variables = Variables {
            flash_job_id: job_id.to_string(),
            status: status.to_string(),
        };

        let response: GraphQLResponse<UpdateStatusData> = self
            .send_request(query, variables, true)
            .context("Failed to update flash status")?;

        if response.data.is_some() {
            Ok(())
        } else if let Some(errors) = response.errors {
            anyhow::bail!("Update status failed: {}", errors[0].message)
        } else {
            anyhow::bail!("Update status failed: No data or errors")
        }
    }

    pub async fn complete_flash_job(
        &self,
        job_id: &str,
        success: bool,
        duration: i64,
        error_message: Option<&str>,
    ) -> Result<()> {
        let query = r#"
            mutation CompleteFlashJob(
                $flashJobId: ID!
                $success: Boolean!
                $duration: Long!
                $errorMessage: String
            ) {
                completeFlashJob(
                    flashJobId: $flashJobId
                    success: $success
                    duration: $duration
                    errorMessage: $errorMessage
                ) {
                    id
                }
            }
        "#;

        #[derive(Serialize)]
        struct Variables {
            #[serde(rename = "flashJobId")]
            flash_job_id: String,
            success: bool,
            duration: i64,
            #[serde(rename = "errorMessage")]
            error_message: Option<String>,
        }

        let variables = Variables {
            flash_job_id: job_id.to_string(),
            success,
            duration,
            error_message: error_message.map(String::from),
        };

        let response: GraphQLResponse<CompleteData> = self
            .send_request(query, variables, true)
            .context("Failed to complete flash job")?;

        if response.data.is_some() {
            Ok(())
        } else if let Some(errors) = response.errors {
            anyhow::bail!("Complete failed: {}", errors[0].message)
        } else {
            anyhow::bail!("Complete failed: No data or errors")
        }
    }

    pub async fn heartbeat(&self) -> Result<()> {
        let query = r#"
            mutation UpdateDeviceHeartbeat {
                updateDeviceHeartbeat {
                    id
                }
            }
        "#;

        #[derive(Serialize)]
        struct Variables {}

        let response: GraphQLResponse<HeartbeatData> = self
            .send_request(query, Variables {}, true)
            .context("Failed to send heartbeat")?;

        if response.data.is_some() {
            debug!("Heartbeat sent");
            Ok(())
        } else {
            Ok(())
        }
    }

    fn send_request<V: Serialize, T: for<'de> Deserialize<'de>>(
        &self,
        query: &str,
        variables: V,
        requires_auth: bool,
    ) -> Result<GraphQLResponse<T>> {
        let url = format!("{}/api/v1/graphql", self.config.server_url);

        let request = GraphQLRequest {
            query: query.to_string(),
            variables,
        };

        let mut req = self.client.post(&url).json(&request);

        if requires_auth {
            if let Some(token) = &self.access_token {
                req = req.header("Authorization", format!("Bearer {}", token));
            } else {
                anyhow::bail!("Not authenticated");
            }
        }

        let response = req.send().context("Failed to send request")?;

        if !response.status().is_success() {
            anyhow::bail!("Request failed with status: {}", response.status());
        }

        let graphql_response: GraphQLResponse<T> = response
            .json()
            .context("Failed to parse response")?;

        Ok(graphql_response)
    }
}
