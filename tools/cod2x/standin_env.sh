# Sourced, not run: `. tools/cod2x/standin_env.sh`.
#
# A CoD2x engine asks CoD2x's own services at every start (the master's
# version check, and a server's update check). A test or a measurement must
# not: this points all three addresses at a port on this Mac that nothing
# listens on. Values already in the environment are kept, so a test that runs
# its own stand-in server sets them first.
#
# A person who really wants the services, to play on a build from the tree,
# sets COD2X_LIVE_SERVICES=1.
if [ "${COD2X_LIVE_SERVICES:-0}" != 1 ]; then
    export COD2X_TEST_SERVICE_URL="${COD2X_TEST_SERVICE_URL:-http://127.0.0.1:9}"
    export COD2X_TEST_UPDATER_ADDRESS="${COD2X_TEST_UPDATER_ADDRESS:-127.0.0.1:9}"
    export COD2X_TEST_UPDATER_URL="${COD2X_TEST_UPDATER_URL:-http://127.0.0.1:9/api/updater/check}"
fi
