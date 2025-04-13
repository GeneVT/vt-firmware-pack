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

#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <streambuf>
#include <string>

#include "gtest/gtest.h"

namespace fitness {

static std::string CreateTempFile(const std::string &content) {
  auto tmp_name = strdup("/tmp/tmpfileXXXXXX");
  mkstemp(tmp_name);
  std::ofstream config_file(tmp_name);

  config_file << content;
  config_file.close();

  return tmp_name;
}

static std::string ReadFile(const std::string &filename) {
  std::ifstream ifs(filename);
  std::string content((std::istreambuf_iterator<char>(ifs)),
                      std::istreambuf_iterator<char>());

  return content;
}

static void AddHostname(const std::string &ip_addr, const std::string &name) {
  // Back up the file
  system("cp /etc/hosts hosts");

  std::stringstream command;
  command << "echo '" << ip_addr << ' ' << name << "' >> /etc/hosts";
  system(command.str().c_str());
  system("sync");
}

static void RestoreHostnameFile() {
  std::ifstream ifs("hosts");

  if (ifs.good()) {
    system("cp hosts /etc/hosts");
    system("rm hosts");
    system("sync");
  }
}

static void EnableNetwork() {
  auto state = ReadFile("/sys/class/net/eth0/operstate");
  if (state == "up\n")
    // The network interface card is already up.
    return;

  system("ifconfig eth0 up");

  auto route = ReadFile("default_route.txt");
  route.insert(0, "route add default gw ");
  system(route.c_str());
}

static void DisableNetwork() {
  system(
      "ip route show default | awk '/default/ {print $3}' "
      ">default_route.txt");
  system("ifconfig eth0 down");
}

class AwsDeviceShadowTest : public ::testing::Test {
 protected:
  AwsDeviceShadowTest() {
    auto device_state_ = ReadFile("/tmp/vt-fitness.json");

    config_.connection_timeout_ms = 1000;

    shadow_ = std::make_unique<AwsDeviceShadow>(config_);
  }

  ~AwsDeviceShadowTest() {
    EnableNetwork();

    RestoreHostnameFile();
  }

  bool PublishWithRetry(int retry = 3,
                        const std::string &shadow_name = std::string()) {
    while (retry > 0) {
      if (shadow_->Publish(device_state_, shadow_name)) {
        return true;
      }
      --retry;
    }
    return false;
  }

  bool Publish() { return shadow_->Publish(device_state_); }

  std::string device_state_;
  std::unique_ptr<AwsDeviceShadow> shadow_{};
  AwsConnectionConfig config_ =
      MakeAwsConnectionConfig("/etc/conf.d/vt-systemd.json");
};

TEST_F(AwsDeviceShadowTest, Update) {
  auto success = PublishWithRetry();

  ASSERT_TRUE(success);
}

TEST_F(AwsDeviceShadowTest, NoInternetConnection) {
  DisableNetwork();

  auto success = Publish();

  EXPECT_FALSE(success);
}

TEST_F(AwsDeviceShadowTest, TcpPort443) {
  config_.port = 443;
  shadow_ = std::make_unique<AwsDeviceShadow>(config_);

  auto success = PublishWithRetry();

  EXPECT_TRUE(success);
}

TEST_F(AwsDeviceShadowTest, WrongDeviceCertificate) {
  config_.cert_path = "/etc/conf.d/.aws/wrong-device-cert.pem";
  config_.priv_key = "/etc/conf.d/.aws/wrong-device-private.key";
  shadow_ = std::make_unique<AwsDeviceShadow>(config_);

  auto success = Publish();

  EXPECT_FALSE(success);
}

TEST_F(AwsDeviceShadowTest, WrongEndpoint) {
  config_.endpoint = "https://test.mosquitto.org/";
  config_.port = 443;
  shadow_ = std::make_unique<AwsDeviceShadow>(config_);

  auto success = Publish();

  EXPECT_FALSE(success);
}

TEST_F(AwsDeviceShadowTest, WrongThingName) {
  config_.thing_name = "THIS-SHOULD-NEVER-EXIST";
  shadow_ = std::make_unique<AwsDeviceShadow>(config_);

  auto success = Publish();

  EXPECT_FALSE(success);
}

TEST_F(AwsDeviceShadowTest, UnsupportedPort) {
  config_.port = 999;
  shadow_ = std::make_unique<AwsDeviceShadow>(config_);

  auto success = Publish();

  EXPECT_FALSE(success);
}

TEST(AwsConnectionConfigTest, NoPortFound) {
  auto content = R"(
    {
      "aws": {
        "url": "a1snrwtdyvr04x-ats.iot.us-west-2.amazonaws.com",
        "ROOT_CA_CERT": "/etc/conf.d/.aws/AmazonRootCA1.pem",
        "PRIVATE_KEY": "/etc/conf.d/.aws/test-device-private.key",
        "DEVICE_CERT": "/etc/conf.d/.aws/test-device-cert.pem"
      },
      "bud": {
        "serial": "vt-fitness-test"
      }
    }
  )";
  auto tmp_name = CreateTempFile(content);

  auto config = MakeAwsConnectionConfig(tmp_name);

  EXPECT_EQ(443, config.port);
}

TEST(AwsConnectionConfigTest, PortOverridden) {
  auto content = R"(
    {
      "aws": {
        "url": "a1snrwtdyvr04x-ats.iot.us-west-2.amazonaws.com",
        "ROOT_CA_CERT": "/etc/conf.d/.aws/AmazonRootCA1.pem",
        "PRIVATE_KEY": "/etc/conf.d/.aws/test-device-private.key",
        "DEVICE_CERT": "/etc/conf.d/.aws/test-device-cert.pem",
        "port": 443
      },
      "bud": {
        "serial": "vt-fitness-test"
      }
    }
  )";
  auto tmp_name = CreateTempFile(content);

  auto config = MakeAwsConnectionConfig(tmp_name);

  EXPECT_EQ(443, config.port);
}

TEST(AwsConnectionConfigTest, DefaultEndpoint) {
  auto content = R"(
    {
      "aws": {
        "ROOT_CA_CERT": "/etc/conf.d/.aws/AmazonRootCA1.pem",
        "PRIVATE_KEY": "/etc/conf.d/.aws/test-device-private.key",
        "DEVICE_CERT": "/etc/conf.d/.aws/test-device-cert.pem"
      },
      "bud": {
        "serial": "vt-fitness-test"
      }
    }
  )";
  auto tmp_name = CreateTempFile(content);

  auto config = MakeAwsConnectionConfig(tmp_name);

  EXPECT_EQ(kDefaultMqttEndpoint, config.endpoint);
}

TEST_F(AwsDeviceShadowTest, ViaAmazonProxyServer) {
  // Send data via Amazon proxy server
  auto amazon_proxy_server_ip = "34.216.81.7";
  AddHostname(amazon_proxy_server_ip, config_.endpoint);

  auto success = PublishWithRetry();

  ASSERT_TRUE(success);
}

TEST_F(AwsDeviceShadowTest, UpdatePhaseMappings) {
  auto success = PublishWithRetry(3, "PhaseMappings");

  ASSERT_TRUE(success);
}

}  // namespace fitness
