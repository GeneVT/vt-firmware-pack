FROM verdigristech/aws-iot-sdk-cpp-v2:v1.12.8

# Set up the testing environment
WORKDIR /root
COPY tests/fixtures/vt-systemd.json /etc/conf.d/

# Copy test credentials
COPY tests/fixtures/AmazonRootCA1.pem /etc/conf.d/.aws/
COPY tests/fixtures/test-device-cert.pem /etc/conf.d/.aws/
COPY tests/fixtures/test-device-private.key /etc/conf.d/.aws/

# Generate wrong device certificate and its private key
RUN openssl genrsa -out /etc/conf.d/.aws/wrong-device-private.key 2048
RUN openssl req -x509 -new -nodes -key /etc/conf.d/.aws/wrong-device-private.key -sha256 -days 1024 \
    -out /etc/conf.d/.aws/wrong-device-cert.pem -subj "/C=US/ST=Pluto/L=Mars/O=VerdigirsTech/OU=Software"

WORKDIR /root/vt-firmware-pack

CMD [ "/bin/bash" ]
