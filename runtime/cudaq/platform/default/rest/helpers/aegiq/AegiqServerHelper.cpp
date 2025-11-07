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
#include <iostream>
#include <map>
#include <regex>
#include <sstream>
#include <thread>
#include <unordered_set>

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

  /// @brief Define required headers.
  RestHeaders getHeaders() override {
    RestHeaders headers;
    headers["Content-Type"] = "application/json";

    // Add authentication key
    if (backendConfig.count("api_key"))
      headers["x-access-token"] = backendConfig["api_key"];

    return headers;
  }

  /// @brief Example implementation of backend initialization.
  void initialize(BackendConfig config) override {
    CUDAQ_INFO("Initializing Provider Name Backend");
    backendConfig = config;

    if (!backendConfig.count("url"))
      backendConfig["url"] = DEFAULT_URL;
    if (!backendConfig.count("version"))
      backendConfig["version"] = DEFAULT_VERSION;

    // Set shots if provided
    if (config.find("shots") != config.end())
      this->setShots(std::stoul(config["shots"]));
  }

  /// Generate required jobs from a list of jobs
  ServerJobPayload createJob(std::vector<KernelExecution> &circuitCodes) override {
    std::vector<ServerMessage> jobs;
    for (auto &circuitCode : circuitCodes) {
      ServerMessage job;
      job["qpu"] = "Artemis"; // This should probably be set somewhere!
      job["name"] = circuitCode.name;
      job["lightworks_version"] = "0.0.0"; // This is not important here
      job["shots"] = shots;
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
  std::string extractJobId(ServerMessage &postResponse) override {
    if (!postResponse.contains("X-Job-Id"))
      return "";

    return postResponse.at("X-Job-Id");
  }

  /// @brief Example implementation of job ID tracking.
  std::string constructGetJobPath(ServerMessage &postResponse) override {
    return extractJobId(postResponse);
  }

  /// @brief Track job based on the ID.
  std::string constructGetJobPath(std::string &jobId) override {
    return backendConfig["url"] + "/jobs/" + jobId;
  }

  /// @brief Example implementation of job status checking.
  bool jobIsDone(ServerMessage &getJobResponse) override {
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
  cudaq::sample_result processResults(ServerMessage &getJobResponse,
                                     std::string &jobId) override {
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

  /// @brief Example implementation of polling configuration.
  std::chrono::microseconds
  nextResultPollingInterval(ServerMessage &postResponse) override {
    return std::chrono::seconds(5);
  }
};

} // namespace cudaq

// Register the server helper in the CUDA-Q server helper factory
CUDAQ_REGISTER_TYPE(cudaq::ServerHelper, cudaq::AegiqServerHelper, aegiq)