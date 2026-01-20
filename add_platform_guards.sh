#!/bin/bash
# Script to add platform guards for ESP32-specific includes

FILES=(
    "src/main.cpp"
    "src/core/security.cpp"
    "src/core/config.cpp"
    "src/core/commands/system_commands.cpp"
    "src/core/commands/info_commands.cpp"
    "src/core/commands/config_commands.cpp"
    "src/core/commands/channel_commands.cpp"
    "src/core/neighbors.cpp"
    "src/core/identity.cpp"
    "src/core/messaging.cpp"
)

for file in "${FILES[@]}"; do
    if [ -f "$file" ]; then
        echo "Processing $file..."
        # Add guard before Preferences.h include
        sed -i 's/#include <Preferences\.h>/#if defined(ARCH_ESP32) || defined(ARCH_ESP32S3) || defined(ARCH_ESP32C3) || defined(ARCH_ESP32C6)\n#    include <Preferences.h>\n#endif/' "$file"

        # Add guard before mbedtls includes
        sed -i 's/#include <mbedtls\/base64\.h>/#if defined(ARCH_ESP32) || defined(ARCH_ESP32S3) || defined(ARCH_ESP32C3) || defined(ARCH_ESP32C6)\n#    include <mbedtls\/base64.h>\n#endif/' "$file"
    fi
done

echo "Done!"
