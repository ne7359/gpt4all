var finishedText = null;

function cancelInstaller(message) {
    installer.setDefaultPageVisible(QInstaller.Introduction,         false);
    installer.setDefaultPageVisible(QInstaller.TargetDirectory,      false);
    installer.setDefaultPageVisible(QInstaller.ComponentSelection,   false);
    installer.setDefaultPageVisible(QInstaller.ReadyForInstallation, false);
    installer.setDefaultPageVisible(QInstaller.StartMenuSelection,   false);
    installer.setDefaultPageVisible(QInstaller.PerformInstallation,  false);
    installer.setDefaultPageVisible(QInstaller.LicenseCheck,         false);
    finishedText = message;
    installer.setCanceled();
}

function vercmp(a, b) {
    return a.localeCompare(b, undefined, { numeric: true, sensitivity: "base" });
}

function Controller() {
}

Controller.prototype.TargetDirectoryPageCallback = function() {
    if (
        (systemInfo.productType === "macos" || systemInfo.productType === "osx")
        && vercmp(systemInfo.kernelVersion, "22.4.0") < 0
    ) {
        cancelInstaller(
            "Installation cannot continue because GPT4All does not support your operating system: " +
            `Darwin ${systemInfo.kernelVersion}<br/><br/>` +
            "GPT4All requires macOS Ventura 13.3 or higher."
        );
    }
}

Controller.prototype.FinishedPageCallback = function() {
    const widget = gui.currentPageWidget();
    if (widget != null && finishedText != null) {
        widget.MessageLabel.setText(finishedText);
    }
}
