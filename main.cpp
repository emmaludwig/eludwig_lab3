#include "mbed.h"
#include <cstdint>
#include <events/mbed_events.h>
#include "ble/BLE.h"
#include "ble/Gap.h"
#include <AdvertisingDataSimpleBuilder.h>
#include <ble/gatt/GattCharacteristic.h>
#include <ble/gatt/GattService.h>
#include "att_uuid.h"
#include <USBSerial.h>

USBSerial ser;

Thread tempthread;

static events::EventQueue event_queue(16 * EVENTS_EVENT_SIZE);

BLE &bleinit= BLE::Instance();
Gap& gap = bleinit.gap();
GattServer& gattServe = bleinit.gattServer();
GattClient& gattClient = bleinit.gattClient();

int16_t TOUT =0;

using namespace ble;

/**
 * Event handler struct
 */
struct GapEventHandler : Gap::EventHandler{
    /* 
	 * Implement the functions here that you think you'll need. These are defined in the GAP EventHandler:
     * https://os.mbed.com/docs/mbed-os/v6.6/mbed-os-api-doxy/structble_1_1_gap_1_1_event_handler.html
     */
     onAdvertisingStart(const AdvertisingStartEvent &event){
        ser.printf("Advertising starting.\r\n");

     }
     onAdvertisingEnd(const AdvertisingEndEvent &event){
        ser.printf("Advertising ending.\r\n");
        
     }
     onAdvertisingReport (const AdvertisingReportEvent &event){  

     }
     onConnectionComplete (const ConnectionCompleteEvent &event){
        ser.printf("Connection complete.\r\n");
        ser.printf(event.getStatus());
     }

};

GapEventHandler THE_gap_EvtHandler;

void measure_temp(){
    I2C sensor_bus(I2C_SDA1, I2C_SCL1);

    const int readaddr = 0xBF;
    const int writeaddr = 0xBE;
    uint8_t whoamiaddr[] = {0x0F};
    int resp=4;

    char readData[] ={0, 0};
    resp = sensor_bus.write(writeaddr, (const char *) whoamiaddr, 1, true);
    
    if(  resp != 0 ){
        ser.printf("I failed to talk at the temp sensor. (Returned: %d)\n\r", resp);            
    }
              
    if( sensor_bus.read(readaddr, readData, 1)  != 0 ){
        ser.printf("I failed to listen to the temp sensor.\n\r");        
    }
    
    ser.printf("Who Am I? %d\n", readData[0] );
    if( readData[0] != 0xBC ){
        ser.printf("Who are are you?\n\r");
    }

    readData[0] = 0x20; // Control Reg 1
    readData[1] = 0x84; // Turn on our temp sensor, and ensure that we read low to high on our values.
    resp = sensor_bus.write(readaddr, readData, 2);    


    uint8_t databuf[2];
    uint8_t subaddr[2];
    while(1){        
        readData[0] = 0x21; // Control Reg 2
        readData[1] = 0x1; // Signal a one shot temp reading.
        resp = sensor_bus.write(readaddr, readData, 2);

        
        subaddr[0] = 0x2A; // LSB Temperature
        sensor_bus.write(writeaddr, (const char *) subaddr, 1, true);
        sensor_bus.read(readaddr, readData, 1);
        databuf[0] = ((uint8_t)readData[0]);

        subaddr[0] = 0x2B; // MSB Temperature
        sensor_bus.write(writeaddr, (const char *) subaddr, 1, true);
        sensor_bus.read(readaddr, readData, 1);
        databuf[1] = readData[0];

        TOUT = databuf[0] | (databuf[1]<<8);
        ser.printf("Uncalibrated temperature: %d\n\r",TOUT);

        // Sleep for a while.
        thread_sleep_for(5000);
    }
}

void on_init_complete(BLE::InitializationCompleteCallbackContext *params){
    BLE& bleinit = params -> ble;
    ble_error_t initialization_error = params -> error;

    ser.printf("Initialization complete.\r\n");

    if(params -> error != initialization_error){
        ser.printf("Initialization failed.\r\n");
    }

    gap.setAdvertisingPayload(LEGACY_ADVERTISING_HANDLE,
            AdvertisingDataSimpleBuilder<LEGACY_ADVERTISING_MAX_SIZE>()
            .setFlags()
            .setName("Emma's Chip", true)
            .setAppearance(adv_data_appearance_t::GENERIC_THERMOMETER)
            .setLocalService(ATT_UUID_INTERMEDIATE_TEMP)
            .getAdvertisingData() 
    );

    gap.startAdvertising(LEGACY_ADVERTISING_HANDLE);
}

/* Schedule processing of events from the BLE middleware in the event queue. */
void schedule_ble_events(BLE::OnEventsToProcessCallbackContext *context){
    event_queue.call(mbed::Callback<void()>(&context->ble, &BLE::processEvents));
}


int main(){
    DigitalOut i2cbuspull(P1_0); // Pull up i2C. resistor.
    i2cbuspull.write(1);
    DigitalOut sensor_pwr(P0_22); // Supply power to all of the sensors (VCC)
    sensor_pwr.write(1);

    BLE &bleinit = BLE::Instance();

    bleinit.onEventsToProcess(schedule_ble_events);
    
    bleinit.init(on_init_complete); //initialize bluetooth instance
    

    // This will never return...
    event_queue.dispatch_forever();
}
