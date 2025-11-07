/*******************************************************************************
 * Copyright (c) 2025 NVIDIA Corporation & Affiliates.                         *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

#include "common/Logger.h"
#include "common/RestClient.h"
#include "common/ServerHelper.h"
#include "cudaq/Support/Version.h"
#include "cudaq/utils/cudaq_utils.h"
#include <bitset>
#include <fstream>
#include <map>
#include <thread>

using json = nlohmann::json;

namespace cudaq {

/// @brief The AegiqServerHelper class extends the ServerHelper class
/// to handle interactions with the Provider Name server for submitting and
/// retrieving quantum computation jobs.
class AegiqServerHelper : public ServerHelper {
  static constexpr const char *DEFAULT_URL = "https://api.aegiq.com";
  static constexpr const char *DEFAULT_VERSION = "v1.0";

public:
  const std::string name() const override { return "aegiq"; }

  /// @brief Creates backend with provided configuration.
  void initialize(BackendConfig config) override;

  /// @brief Define required headers.
  RestHeaders getHeaders() override;

  /// @brief Generates required jobs from a vector of jobs
  ServerJobPayload createJob(std::vector<KernelExecution> &circuitCodes) override;

  /// @brief Example implementation of job ID tracking.
  std::string extractJobId(ServerMessage &postResponse) override;

  /// @brief Example implementation of job ID tracking.
  std::string constructGetJobPath(ServerMessage &postResponse) override;

  /// @brief Track job based on the ID.
  std::string constructGetJobPath(std::string &jobId) override;

  /// @brief Example implementation of job status checking.
  bool jobIsDone(ServerMessage &getJobResponse) override;

  /// @brief Proccess returned results and gets them in to the required format
  cudaq::sample_result processResults(ServerMessage &getJobResponse,
                                      std::string &jobId) override;

  /// @brief Sets polling interval for system.
  std::chrono::microseconds
  nextResultPollingInterval(ServerMessage &postResponse) override {
    return std::chrono::seconds(1);
  }

private:
  /// @brief Helper method to retrieve the value of an environment variable.
  std::string getEnvVar(const std::string &key, const std::string &defaultVal,
                        const bool isRequired) const;

  /// @brief Helper function to get value from config or return a default value.
  std::string getValueOrDefault(const BackendConfig &config,
                                const std::string &key,
                                const std::string &defaultValue) const;
};
                    


/// @brief Example implementation of backend initialization.
void AegiqServerHelper::initialize(BackendConfig config) {
  CUDAQ_INFO("Initializing Provider Name Backend");

  backendConfig = config;

  backendConfig["url"] = getValueOrDefault(config, "url", DEFAULT_URL);
  backendConfig["version"] = DEFAULT_VERSION;
  
  /// @todo Decide if/how default qpu should be set
  backendConfig["qpu"] = getValueOrDefault(config, "qpu", "Artemis");

  backendConfig["api_key"] = getEnvVar("AEGIQ_API_KEY", 0, true);

  // Set shots if provided
  if (config.find("shots") != config.end())
    this->setShots(std::stoul(config["shots"]));
}

/// @brief Define required headers.
RestHeaders AegiqServerHelper::getHeaders() {
  RestHeaders headers;
  headers["Content-Type"] = "application/json";

  // Add authentication key
  if (backendConfig.count("api_key"))
    headers["x-access-token"] = backendConfig["api_key"];

  return headers;
}

/// Generate required jobs from a list of jobs
ServerJobPayload AegiqServerHelper::createJob(std::vector<KernelExecution> &circuitCodes) {
  std::vector<ServerMessage> jobs;
  for (auto &circuitCode : circuitCodes) {
    ServerMessage job;
    job["qpu"] = backendConfig["qpu"];
    job["name"] = circuitCode.name;
    job["lightworks_version"] = "0.0.0"; // This is not important here so just get to be generic
    job["n_samples"] = shots;
    // Below this line need to figure out how these would be defined.
    job["input"] = NULL;
    job["min_direction"] = NULL;
    job["direction_implementation"] = NULL;
    job["unitary"] = NULL;
    job["circuit_spec"] = circuitCode.code; // Does this make sense? Will need to be converted to correct format
    job["job_data"] = NULL;

    jobs.push_back(job);
  }

  RestHeaders headers = getHeaders();
  std::string path = "/jobs";

  return std::make_tuple(backendConfig["url"] + path, headers, jobs);
}

/// @brief Example implementation of job ID tracking.
std::string AegiqServerHelper::extractJobId(ServerMessage &postResponse) {
  if (!postResponse.contains("X-Job-Id"))
    return "";

  return postResponse.at("X-Job-Id");
}

/// @brief Example implementation of job ID tracking.
std::string AegiqServerHelper::constructGetJobPath(ServerMessage &postResponse) {
  return extractJobId(postResponse);
}

/// @brief Track job based on the ID.
std::string AegiqServerHelper::constructGetJobPath(std::string &jobId) {
  return backendConfig["url"] + "/jobs/" + jobId;
}

/// @brief Example implementation of job status checking.
bool AegiqServerHelper::jobIsDone(ServerMessage &getJobResponse) {
  if (!getJobResponse.contains("status"))
    return false;

  std::string status = getJobResponse["status"];
  return status == "COMPLETED" || status == "FAILED";
}

/// @brief Example implementation of result processing.
///
/// The raw results from quantum hardware often need post-processing (bit
/// reordering, normalization, etc.) to match CUDA-Q's expectations.
/// This is the place to do that.
cudaq::sample_result AegiqServerHelper::processResults(ServerMessage &getJobResponse,
                                                       std::string &jobId) {
  CUDAQ_INFO("Processing results: {}", getJobResponse.dump());

  // Extract measurement results from the response
  auto samplesJson = getJobResponse["results"]["counts"];
  cudaq::CountsDictionary counts;

  for (auto &item : samplesJson.items()) {
    std::string bitstring = item.key();
    std::size_t count = item.value();
    counts[bitstring] = count;
  }

  // Create an ExecutionResult
  cudaq::ExecutionResult execResult{counts};

  // Return the sample_result
  return cudaq::sample_result{execResult};
}

// Helper method to retrieve an environment variable
std::string AegiqServerHelper::getEnvVar(const std::string &key,
                                              const std::string &defaultVal,
                                              const bool isRequired) const {
  const char *env_var = std::getenv(key.c_str());
  if (env_var == nullptr) {
    if (isRequired)
      throw std::runtime_error("Environment variable " + key +
                               " is required but not set.");
    else
      return defaultVal;
  }
  return std::string(env_var);
}

// Helper function to get a value from config or return a default
std::string AegiqServerHelper::getValueOrDefault(
    const BackendConfig &config, const std::string &key,
    const std::string &defaultValue) const {
  auto it = config.find(key);
  return (it != config.end()) ? it->second : defaultValue;
}

} // namespace cudaq

// Register the server helper in the CUDA-Q server helper factory
CUDAQ_REGISTER_TYPE(cudaq::ServerHelper, cudaq::AegiqServerHelper, aegiq)