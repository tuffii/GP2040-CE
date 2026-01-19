#include "peripheralmanager.h"
#include "storagemanager.h"

void PeripheralManager::initUSB(){
    const PeripheralOptions& peripheralOptions = Storage::getInstance().getPeripheralOptions();
    if (peripheralOptions.blockUSB0.enabled) blockUSB0.setConfig(0, peripheralOptions.blockUSB0.dp, peripheralOptions.blockUSB0.enable5v, peripheralOptions.blockUSB0.order);
}

void PeripheralManager::initI2C(){
    const PeripheralOptions& peripheralOptions = Storage::getInstance().getPeripheralOptions();
    if (peripheralOptions.blockI2C0.enabled) blockI2C0.setConfig(0, peripheralOptions.blockI2C0.sda, peripheralOptions.blockI2C0.scl, peripheralOptions.blockI2C0.speed);
    if (peripheralOptions.blockI2C1.enabled) blockI2C1.setConfig(1, peripheralOptions.blockI2C1.sda, peripheralOptions.blockI2C1.scl, peripheralOptions.blockI2C1.speed); 
}

void PeripheralManager::initSPI(){
    const PeripheralOptions& peripheralOptions = Storage::getInstance().getPeripheralOptions();
    if (peripheralOptions.blockSPI0.enabled) blockSPI0.setConfig(0, peripheralOptions.blockSPI0.tx, peripheralOptions.blockSPI0.rx, peripheralOptions.blockSPI0.sck, peripheralOptions.blockSPI0.cs);
    if (peripheralOptions.blockSPI1.enabled) blockSPI1.setConfig(1, peripheralOptions.blockSPI1.tx, peripheralOptions.blockSPI1.rx, peripheralOptions.blockSPI1.sck, peripheralOptions.blockSPI1.cs);
}

void PeripheralManager::initUART() {
    const PeripheralOptions& options = Storage::getInstance().getPeripheralOptions();

    if (options.blockUART0.enabled) {
        blockUART0.setConfig(
            0,
            options.blockUART0.txPin, 
            options.blockUART0.rxPin, 
            options.blockUART0.ctsPin, 
            options.blockUART0.rtsPin, 
            4000000
        );
    }

    if (options.blockUART1.enabled) {
        blockUART1.setConfig(
            1,
            options.blockUART1.txPin, 
            options.blockUART1.rxPin, 
            options.blockUART1.ctsPin, 
            options.blockUART1.rtsPin, 
            4000000
        );
    }
}

PeripheralI2C* PeripheralManager::getI2C(uint8_t block) {
    if (block < NUM_I2CS) {
        return ((block == 0) ? &blockI2C0 : &blockI2C1);
    }
    return nullptr;
}

PeripheralSPI* PeripheralManager::getSPI(uint8_t block) {
    if (block < NUM_SPIS) {
        return ((block == 0) ? &blockSPI0 : &blockSPI1);
    }
    return nullptr;
}

PeripheralUSB* PeripheralManager::getUSB(uint8_t block) {
    if (block < NUM_USBS) {
        return ((block == 0) ? &blockUSB0 : &blockUSB0);
    }
    return nullptr;
}

PeripheralUART* PeripheralManager::getUART(uint8_t block) {
    if (block < 2) { 
        return ((block == 0) ? &blockUART0 : &blockUART1);
    }
    return nullptr;
}

bool PeripheralManager::isI2CEnabled(uint8_t block) {
    if (block < NUM_I2CS) {
        return (((block == 0) ? blockI2C0.configured : blockI2C1.configured));
    }
    return false;
}

bool PeripheralManager::isSPIEnabled(uint8_t block) {
    if (block < NUM_SPIS) {
        return (((block == 0) ? blockSPI0.configured : blockSPI1.configured));
    }
    return false;
}

bool PeripheralManager::isUSBEnabled(uint8_t block) {
    if (block < NUM_USBS) {
        return (((block == 0) ? blockUSB0.configured : false));
    }
    return false;
}

bool PeripheralManager::isUARTEnabled(uint8_t block) {
    const PeripheralOptions& peripheralOptions = Storage::getInstance().getPeripheralOptions();
    if (block < 2) {
        return (block == 0) ? peripheralOptions.blockUART0.enabled : peripheralOptions.blockUART1.enabled;
    }
    return false;
}

PeripheralI2CScanResult PeripheralManager::scanForI2CDevice(std::vector<uint8_t> addressList) {
    PeripheralI2CScanResult scanResult = {
        .address = -1,
        .block = 0
    };
    
    for (uint8_t block = 0; block < NUM_I2CS; block++) {
        if (isI2CEnabled(block)) {
            PeripheralI2C* i2c = getI2C(block);

            for (uint8_t i = 0; i < addressList.size(); i++) {
                if (!((addressList[i] & 0x78) == 0 || (addressList[i] & 0x78) == 0x78)) {
                    uint8_t result = i2c->test(addressList[i]);

                    if (result) {
                        scanResult.address = addressList[i];
                        scanResult.block = block;
                        return scanResult;
                    }
                }
            }
        }
    }
    return scanResult;
}