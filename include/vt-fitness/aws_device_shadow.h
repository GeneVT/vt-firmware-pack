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

#ifndef INCLUDE_VT_FITNESS_AWS_DEVICE_SHADOW_H_
#define INCLUDE_VT_FITNESS_AWS_DEVICE_SHADOW_H_

#include <aws/crt/Api.h>
#include <aws/crt/JsonObject.h>
#include <aws/iot/MqttClient.h>
#include <aws/iotshadow/IotShadowClient.h>

#include <condition_variable>  // NOLINT: Google bans this because it clashes with their in-house implementation.
#include <fstream>
#include <string>

namespace fitness {

//
// Do the global initialization for the API.
//
static Aws::Crt::ApiHandle apiHandle;

static constexpr auto kDefaultMqttEndpoint =
    "a1snrwtdyvr04x-ats.iot.us-west-2.amazonaws.com";

// Represents an AWS connection configuration
struct AwsConnectionConfig {
  std::string cert_path;
  std::string priv_key;
  std::string root_ca_path;
  std::string endpoint;
  std::string thing_name;
  uint16_t port;

  uint32_t connection_timeout_ms;
};

extern AwsConnectionConfig MakeAwsConnectionConfig(const std::string &filename);

// -----------------------------------------------------------------------------
// fitness::AwsDeviceShadow
// -----------------------------------------------------------------------------
//
// An AwsDeviceShadow is used to publish the device state to the AWS IoT
// device shadow.
class AwsDeviceShadow {
 public:
  explicit AwsDeviceShadow(const AwsConnectionConfig &config);

  AwsDeviceShadow(const std::string &cert_path, const std::string &priv_key,
                  const std::string &root_cert_authority_path,
                  const std::string &endpoint, const std::string &thing_name,
                  const uint16_t port = 8883,
                  const uint32_t connection_timeout_ms = 3000);

  // Updates the specified payload to the AWS IoT Device Shadow.
  bool Publish(const std::string &payload,
               const std::string &shadow_name = std::string(),
               const std::string &jsonObject = std::string());

  void EnableLogging() const;

 private:
  void UpdateDeviceState(Aws::Iotshadow::IotShadowClient *client,
                         const Aws::Crt::JsonObject &&info,
                         const std::string &shadow_name,
                         const std::string &jsonObject);

  const std::string cert_path_;
  const std::string priv_key_;
  const std::string root_ca_path_;
  const std::string endpoint_;
  const uint16_t port_;
  const std::string thing_name_;
  const uint32_t connection_timeout_ms_;

  std::atomic<bool> publish_completed_{false};

  std::condition_variable conditionVariable_;
};

}  // namespace fitness

#endif  // INCLUDE_VT_FITNESS_AWS_DEVICE_SHADOW_H_
