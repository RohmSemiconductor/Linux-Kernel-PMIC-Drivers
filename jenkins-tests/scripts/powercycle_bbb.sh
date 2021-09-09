#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

echo "Bang Bang - I shoot myself"
echo "Oh dear - I don't have a gun :("
echo "Should have some relay-thingee to kill power from PMIC"

$DIR/reboot.expect $TARGET_IP
echo "Rebooting... Wait for wake-up"
