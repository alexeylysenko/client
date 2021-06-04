import names
import squish


class AccountStatus:
    ACCOUNT_BUTTON = {
        "container": names.settings_stack_QStackedWidget,
        "name": "_accountToolbox",
        "type": "QToolButton",
        "visible": 1,
    }
    ACCOUNT_MENU = {
        "type": "QMenu",
        "unnamed": 1,
        "visible": 1,
        "window": names.settings_OCC_SettingsDialog,
    }
    SIGNED_OUT_TEXT_BAR = {
        "container": names.settings_stack_QStackedWidget,
        "name": "connectLabel",
        "type": "QLabel",
        "visible": 1,
    }

    REMOVE_CONNECTION_BUTTON = {
        "container": names.settings_stack_QStackedWidget,
        "text": "Remove connection",
        "type": "QPushButton",
        "unnamed": 1,
        "visible": 1,
    }

    def userLogout(self):
        squish.sendEvent(
            "QMouseEvent",
            squish.waitForObject(self.ACCOUNT_BUTTON),
            squish.QEvent.MouseButtonPress,
            0,
            0,
            squish.Qt.LeftButton,
            0,
            0,
        )
        squish.activateItem(squish.waitForObjectItem(self.ACCOUNT_MENU, "Log out"))

    def userLogsIn(self):
        squish.sendEvent(
            "QMouseEvent",
            squish.waitForObject(self.ACCOUNT_BUTTON),
            squish.QEvent.MouseButtonPress,
            0,
            0,
            squish.Qt.LeftButton,
            0,
        )
        squish.activateItem(squish.waitForObjectItem(self.ACCOUNT_MENU, "Log in"))

    def removeUser(self):
        squish.sendEvent(
            "QMouseEvent",
            squish.waitForObject(self.ACCOUNT_BUTTON),
            squish.QEvent.MouseButtonPress,
            0,
            0,
            squish.Qt.LeftButton,
            0,
            0,
        )
        squish.activateItem(squish.waitForObjectItem(names.settings_QMenu, "Remove"))
        squish.clickButton(squish.waitForObject(self.REMOVE_CONNECTION_BUTTON))
