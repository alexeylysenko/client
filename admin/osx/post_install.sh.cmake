#!/bin/bash

LOGGED_IN_USER_ID=$(id -u "${USER}")

# Always enable the new 10.10 finder plugin if available
if [[ -x "$(command -v pluginkit)" ]]; then
    # add it to DB. This happens automatically too but we try to push it a bit harder for issue #3463
    pluginkit -a  "/Applications/@APPLICATION_EXECUTABLE@.app/Contents/PlugIns/FinderSyncExt.appex/"
    # Since El Capitan we need to sleep #4650
    sleep 10s
    # enable it
    pluginkit -e use -i @APPLICATION_REV_DOMAIN@.FinderSyncExt
fi

if [[ -f "${INSTALLER_TEMP}/OC_RESTART_NEEDED" ]]; then
    if [[ "${COMMAND_LINE_INSTALL}" = "" ]]; then
        /bin/launchctl asuser "${LOGGED_IN_USER_ID}" /usr/bin/open -g "/Applications/@APPLICATION_EXECUTABLE@.app"
    fi
fi

exit 0
