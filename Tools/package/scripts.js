function installCheckScript() {
    try {
        var hvController = system.ioregistry.matchingClass('HyperVController');
        if (hvController.length > 0) {
            return true;
        }
    } catch(e) {
        system.log('installCheckScript threw exception ' + e);
    }

    // If HyperVController is not present, fail as this is not on a Hyper-V platform.
    my.result.message = system.localizedStringWithFormat('ERR_NOT_HYPERV');
    my.result.type = 'Fatal';
    return false;
}

function checkIfTiger() {
    try {
        if(system.compareVersions(my.target.systemVersion.ProductVersion, '10.4') != -1) {
            if(system.compareVersions(my.target.systemVersion.ProductVersion, '10.5') == -1) {
                return true;
            }
        }
    } catch(err) { }

    return false;
}
