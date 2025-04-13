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

#include <iostream>
#include "vt-fitness/aws_device_shadow.h"

#define AWS_IOT_MQTT_CONFIGURATION_PATH "/etc/conf.d/vt-systemd.json"

using fitness::AwsDeviceShadow;
using fitness::MakeAwsConnectionConfig;

static void print_usage(char *program) {
  std::cout << "Usage: " << program
            << " STATE_FILE [SHADOW_NAME] [desired|reported]" << std::endl;
}

int main(int argc, char *argv[]) {
  std::string shadow_name = "";
  std::string json_object = "reported";  // Default to "reported"

  if (argc < 2) {
    print_usage(basename(argv[0]));
    exit(EXIT_FAILURE);
  } else if (argc >= 3) {
    shadow_name = argv[2];
  }
  if (argc == 4) {
    json_object = argv[3];
    if (json_object != "desired" && json_object != "reported") {
      std::cerr << "Invalid argument for JSON object type. Use 'desired' or "
                   "'reported'."
                << std::endl;
      print_usage(basename(argv[0]));
      exit(EXIT_FAILURE);
    }
  }

  auto config = MakeAwsConnectionConfig(AWS_IOT_MQTT_CONFIGURATION_PATH);
  AwsDeviceShadow shadow{config};

  std::ifstream ifs(argv[1]);
  std::string payload((std::istreambuf_iterator<char>(ifs)),
                      std::istreambuf_iterator<char>());

  return shadow.Publish(payload, shadow_name, json_object) ? 0 : 1;
}
