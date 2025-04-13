// Copyright © 2020, Verdigris Technologies, Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this
// list of conditions and the following disclaimer.
//
// Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS”
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include "vt-fitness/aws_device_shadow.h"

#include <aws/crt/UUID.h>
#include <aws/iotshadow/ErrorResponse.h>
#include <aws/iotshadow/UpdateShadowRequest.h>
#include <aws/iotshadow/UpdateNamedShadowRequest.h>

#include <iostream>

#define DEFAULT_MQTT_PORT 443

namespace fitness {

using Aws::Crt::ErrorDebugString;
using Aws::Crt::JsonObject;
using Aws::Crt::Io::ClientBootstrap;
using Aws::Crt::Io::DefaultHostResolver;
using Aws::Crt::Io::EventLoopGroup;
using Aws::Crt::Mqtt::MqttConnection;
using Aws::Crt::Mqtt::ReturnCode;
using Aws::Iotshadow::ShadowState;
using Aws::Iotshadow::UpdateShadowRequest;
using Aws::Iotshadow::UpdateNamedShadowRequest;

// Cannot use the statement below due to a bug in GCC:
//
// using std::literals::chrono_literals::operator""ms
//
// It has been fixed on GCC 8.
using namespace std::chrono_literals;  // NOLINT

static const auto disconnectionTimeoutMS = 1000ms;
static const auto updateTimeoutMS = 5000ms;
static const auto connectionTimeoutMS = 10000;

AwsConnectionConfig MakeAwsConnectionConfig(const std::string &filename) {
  std::ifstream ifs(filename);
  if (!ifs.is_open()) {
    fprintf(stderr, "Failed to open %s (%s).\n", filename.c_str(),
            strerror(errno));
    exit(-1);
  }
  std::string jsonString((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());

  auto sysInfo = Aws::Crt::JsonObject(jsonString.c_str());
  auto sysInfoView = sysInfo.View();

  // Load AWS IoT device shadow credentials
  AwsConnectionConfig config;
  auto aws = sysInfoView.GetJsonObject("aws");

  if (aws.KeyExists("url")) {
    config.endpoint = std::string(aws.GetString("url").c_str());
  } else {
    config.endpoint = kDefaultMqttEndpoint;
  }

  config.root_ca_path = std::string(aws.GetString("ROOT_CA_CERT").c_str());
  config.priv_key = std::string(aws.GetString("PRIVATE_KEY").c_str());
  config.cert_path = std::string(aws.GetString("DEVICE_CERT").c_str());
  config.thing_name =
      std::string(sysInfoView.GetJsonObject("bud").GetString("serial").c_str());
  config.connection_timeout_ms = connectionTimeoutMS;

  if (aws.KeyExists("port")) {
    config.port = aws.GetInteger("port");
  } else {
    config.port = DEFAULT_MQTT_PORT;
  }

  return config;
}

AwsDeviceShadow::AwsDeviceShadow(const std::string &cert_path,
                                 const std::string &priv_key,
                                 const std::string &root_ca_path,
                                 const std::string &endpoint,
                                 const std::string &thing_name,
                                 const uint16_t port,
                                 const uint32_t connection_timeout_ms)
    : cert_path_(cert_path),
      priv_key_(priv_key),
      root_ca_path_(root_ca_path),
      endpoint_(endpoint),
      port_(port),
      thing_name_(thing_name),
      connection_timeout_ms_(connection_timeout_ms) {}

AwsDeviceShadow::AwsDeviceShadow(const AwsConnectionConfig &config)
    : AwsDeviceShadow(config.cert_path, config.priv_key, config.root_ca_path,
                      config.endpoint, config.thing_name, config.port,
                      config.connection_timeout_ms) {}

bool AwsDeviceShadow::Publish(const std::string &payload,
                              const std::string &shadow_name,
                              const std::string &jsonObject) {
  /*
   * You need an event loop group to process IO events.
   * If you only have a few connections, 1 thread is ideal
   */
  EventLoopGroup eventLoopGroup(1);
  if (!eventLoopGroup) {
    fprintf(stderr, "Event Loop Group Creation failed with error %s\n",
            ErrorDebugString(eventLoopGroup.LastError()));
    return false;
  }

  DefaultHostResolver hostResolver(eventLoopGroup, 1, 5);
  ClientBootstrap bootstrap(eventLoopGroup, hostResolver);
  if (!bootstrap) {
    fprintf(stderr, "ClientBootstrap failed with error %s\n",
            ErrorDebugString(bootstrap.LastError()));
    return false;
  }

  /*
   * Now Create a client. This can not throw.
   * An instance of a client must outlive its connections.
   * It is the users responsibility to make sure of this.
   */
  Aws::Iot::MqttClient mqtt_client(bootstrap);
  if (!mqtt_client) {
    fprintf(stderr, "MQTT Client Creation failed with error %s\n",
            ErrorDebugString(mqtt_client.LastError()));
    return false;
  }

  auto config_builder = Aws::Iot::MqttClientConnectionConfigBuilder(
      cert_path_.c_str(), priv_key_.c_str());
  config_builder.WithEndpoint(endpoint_.c_str())
      .WithCertificateAuthority(root_ca_path_.c_str())
      .WithPortOverride(port_)
      .WithTcpConnectTimeout(connection_timeout_ms_);

  auto config = config_builder.Build();
  if (!config) {
    fprintf(stderr,
            "Client Configuration initialization failed with error %s\n",
            ErrorDebugString(config.LastError()));
    return false;
  }

  /*
   * This type is move only and its underlying memory is managed by the client.
   */
  auto connection = mqtt_client.NewConnection(config);
  if (!*connection) {
    fprintf(stderr, "MQTT Connection Creation failed with error %s\n",
            ErrorDebugString(connection->LastError()));
    return false;
  }

  std::atomic<bool> connectionClosed(false);
  std::atomic<bool> connectionSucceeded(false);
  std::atomic<bool> connectionCompleted(false);

  /*
   * This will execute when an mqtt connection has completed or failed.
   */
  auto onConnectionCompleted = [&connectionSucceeded, &connectionCompleted,
                                &conditionVariable = conditionVariable_](
                                   MqttConnection &, int errorCode,
                                   ReturnCode returnCode, bool) {
    if (errorCode) {
      fprintf(stdout, "Connection failed with error %s\n",
              ErrorDebugString(errorCode));
      connectionSucceeded = false;
    } else {
      fprintf(stdout, "Connection completed with return code %d\n", returnCode);
      connectionSucceeded = true;
    }

    connectionCompleted = true;
    conditionVariable.notify_one();
  };

  /*
   * Invoked when a disconnect message has completed.
   */
  auto onDisconnect = [&connectionClosed,
                       &conditionVariable =
                           conditionVariable_](MqttConnection & /*conn*/) {
    {
      fprintf(stdout, "Disconnect completed\n");
      connectionClosed = true;
    }
    conditionVariable.notify_one();
  };

  connection->OnConnectionCompleted = std::move(onConnectionCompleted);
  connection->OnDisconnect = std::move(onDisconnect);

  // Gene: Add a unique client ID for the connection
  auto clientId = thing_name_ + "_firmware_pack";
  if (!connection->Connect(clientId.c_str(), true, 0)) {
    fprintf(stderr, "MQTT Connection failed with error %s\n",
            ErrorDebugString(connection->LastError()));
    return false;
  }

  std::mutex mutex;
  std::unique_lock<std::mutex> uniqueLock(mutex);

  conditionVariable_.wait(uniqueLock, [&]() {
    return connectionCompleted || connectionClosed;
  });

  if (connectionSucceeded) {
    Aws::Iotshadow::IotShadowClient shadowClient(connection);

    UpdateDeviceState(&shadowClient, JsonObject(payload.c_str()),
                      shadow_name, jsonObject);

    if (!conditionVariable_.wait_for(uniqueLock, updateTimeoutMS, [this]() {
          return publish_completed_.load();
        })) {
      fprintf(stderr, "Update timed out.\n");
    }
  }

  if (!connectionClosed) {
    connection->Disconnect();
    if (!conditionVariable_.wait_for(uniqueLock, disconnectionTimeoutMS, [&]() {
          return connectionClosed.load();
        })) {
      fprintf(stderr, "Disconnection timed out.\n");
      return false;
    }
  }

  return true;
}

void AwsDeviceShadow::EnableLogging() const {
  fitness::apiHandle.InitializeLogging(Aws::Crt::LogLevel::Debug, stderr);
}

void AwsDeviceShadow::UpdateDeviceState(Aws::Iotshadow::IotShadowClient *client,
                                        const Aws::Crt::JsonObject &&info,
                                        const std::string &shadow_name,
                                        const std::string &jsonObject) {
  auto onPublishCompleted = [&publish_completed = publish_completed_,
                             &conditionVariable = conditionVariable_,
                             thing_name = thing_name_](int ioErr) {
    if (ioErr != AWS_OP_SUCCESS) {
      fprintf(stderr, "Failed to update %s shadow state: error %s\n",
              thing_name.c_str(), ErrorDebugString(ioErr));
    } else {
      fprintf(stdout, "Successfully updated shadow state for %s\n",
              thing_name.c_str());
    }

    publish_completed = true;
    conditionVariable.notify_one();
  };

  ShadowState state;

  if (shadow_name.empty() || shadow_name == "Classic Shadow") {  // Fixed here
    if (jsonObject == "desired") {
      JsonObject desired(std::move(info));
      state.Desired = desired;
    } else if (jsonObject == "reported") {
      JsonObject reported;
      reported.WithObject("info", std::move(info));
      state.Reported = reported;
    } else {
      fprintf(stderr, "Invalid JSON object type. Use 'desired' or 'reported'.");
      return;
    }

    UpdateShadowRequest request;
    Aws::Crt::UUID uuid;
    request.ClientToken = uuid.ToString();
    request.ThingName = Aws::Crt::String(thing_name_.c_str());
    request.State = state;

    client->PublishUpdateShadow(request, AWS_MQTT_QOS_AT_LEAST_ONCE,
                                std::move(onPublishCompleted));
  } else {
    JsonObject tmpJson(std::move(info));
    if (jsonObject == "desired") {
      state.Desired = tmpJson;
    } else if (jsonObject == "reported") {
      state.Reported = tmpJson;
    } else {
      fprintf(stderr, "Invalid JSON object type. Use 'desired' or 'reported'.");
      return;
    }

    UpdateNamedShadowRequest request;
    Aws::Crt::UUID uuid;
    request.ClientToken = uuid.ToString();
    request.ThingName = Aws::Crt::String(thing_name_.c_str());
    request.ShadowName = Aws::Crt::String(shadow_name.c_str());
    request.State = state;

    client->PublishUpdateNamedShadow(request, AWS_MQTT_QOS_AT_LEAST_ONCE,
                                     std::move(onPublishCompleted));
  }
}

}  // namespace fitness
