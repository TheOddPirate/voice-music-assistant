#!/bin/sh
set -e

TARGET_DIR="$1"

# Store build timestamp so the device boots with a sane default time
# instead of 1970-01-01 (no RTC battery). This accelerates NTP sync
# and avoids TLS/DNS failures caused by expired certificates.
date -u +%Y%m%d%H%M > "$TARGET_DIR/etc/build_timestamp"

if [ -n "$BR2_CONFIG" ] && grep -q "^BR2_PACKAGE_TR_BUTTOND=y" "$BR2_CONFIG"; then
    sed -i 's/^NAME=input_eventd$/NAME=tr-buttond/' \
        "$TARGET_DIR/etc/init.d/S60input"
    rm -f "$TARGET_DIR/usr/bin/input_eventd"
fi

exit 0
