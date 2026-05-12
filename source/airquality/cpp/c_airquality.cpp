#include "airquality/c_airquality.h"

#include "rcore/c_app.h"
#include "rcore/c_gpio.h"
#include "rcore/c_timer.h"
#include "rcore/c_log.h"
#include "rcore/c_packet.h"
#include "rcore/c_str.h"
#include "rcore/c_system.h"
#include "rcore/c_task.h"
#include "rcore/c_wire.h"

#include "rwifi/c_wifi_mgr.h"
#include "rwifi/c_tcp_client.h"
#include "rwifi/c_network_mgr.h"

#include "lib_bh1750/c_bh1750.h"
#include "lib_bme280/c_bme280.h"
#include "lib_scd41/c_scd41.h"
#include "lib_rd03d/c_rd03d.h"

#define ENABLE_BH1750
#define ENABLE_BME280
#define ENABLE_SCD41
#define ENABLE_RD03D

namespace ncore
{
    struct bme280_data_t
    {
        s8  temperature;
        u16 pressure;
        u8  humidity;

        void reset()
        {
            temperature = 0;
            pressure    = 0;
            humidity    = 0;
        }
    };

    struct bh1750_data_t
    {
        u16 lux;

        void reset() { lux = 0; }
    };

    struct scd41_data_t
    {
        u16 co2;
        s8  temperature;
        u8  humidity;

        void reset()
        {
            co2         = 0;
            temperature = 0;
            humidity    = 0;
        }
    };

    struct rd03d_data_t
    {
        u32 DetectionBits[3];
        u8  Detected[3];
        u8  LastSendDetected[3];

        void reset()
        {
            for (s8 i = 0; i < 3; ++i)
            {
                DetectionBits[i]    = 0;
                Detected[i]         = 4;  // Unknown state
                LastSendDetected[i] = 3;
            }
        }
    };

    struct state_app_t
    {
        ntcp::config_t     gTcpConfig;
        ntcp::tcp_client_t gTcpClient;

        nwifi::wifi_config_t  gWifiConfig;
        nwifi::wifi_manager_t gWifiManager;

        npacket::packet_t gSensorPacket;  // Sensor packet for sending data

        bme280_data_t           gCurrentBme;
        bme280_data_t           gLastSendBme;
        ntimer::periodic_task_t gBmeSampleTask;
        ntimer::periodic_task_t gBmeSendTask;

        bh1750_data_t           gCurrentBh;
        bh1750_data_t           gLastSendBh;
        ntimer::periodic_task_t gBhSampleTask;
        ntimer::periodic_task_t gBhSendTask;

        scd41_data_t            gCurrentScd;
        scd41_data_t            gLastSendScd;
        ntimer::periodic_task_t gScdSampleTask;
        ntimer::periodic_task_t gScdSendTask;

        nsensors::nrd03d::sensor_t gCurrentRd03dSensor;
        rd03d_data_t               gCurrentRd03d;
        ntimer::periodic_task_t    gRd03dSampleTask;
        ntimer::periodic_task_t    gRd03dSendTask;
    };
    state_app_t  gAppState;

    void read_bh1750(void)
    {
#ifdef ENABLE_BH1750
        // TODO whenever a sensor cannot be read (faulty?) we need to know so that we can
        //      send a 'state' packet that indicates the sensor is not working.

        // Read the BH1750 sensor data
        u16        lux          = 0;
        const bool valid_bh1750 = nsensors::updateBH1750(lux);
        if (valid_bh1750)
        {
            gAppState.gCurrentBh.lux = lux;
        }
#endif
    }

    void send_bh1750(void)
    {
#ifdef ENABLE_BH1750
        const u16 lux = gAppState.gCurrentBh.lux;
        if (gAppState.gLastSendBh.lux != lux)
        {
            gAppState.gLastSendBh.lux = lux;

            nlog::printf("Light: %d lx\n", va_t((u32)lux));

            // Write a custom (binary-format) network message
            npacket::packet_init(gAppState.gSensorPacket);
            npacket::packet_write(gAppState.gSensorPacket, npacket::ID_LIGHT, nwifi::get_mac_address(gAppState.gWifiManager), (u16)lux);

            // Send the sensor data to the server
            ntcp::send(gAppState.gTcpClient, gAppState.gSensorPacket.Data, gAppState.gSensorPacket.Size);
        }
#endif
    }

    void read_bme280(void)
    {
#ifdef ENABLE_BME280
        // Read the BME280 sensor data
        f32        bme_temp     = 0.0f;
        f32        bme_pres     = 0.0f;
        f32        bme_humi     = 0.0f;
        const bool valid_bme280 = nsensors::updateBME280(bme_pres, bme_temp, bme_humi);
        if (valid_bme280)
        {
            gAppState.gCurrentBme.temperature = static_cast<s8>(bme_temp);   // Temperature to one signed byte (°C)
            gAppState.gCurrentBme.pressure    = static_cast<u16>(bme_pres);  // Pressure to unsigned short (hPa)
            gAppState.gCurrentBme.humidity    = static_cast<u8>(bme_humi);   // Humidity to one unsigned byte (%)
        }
#endif
    }

    void send_bme280()
    {
#ifdef ENABLE_BME280
        // Write a custom (binary-format) network message

        npacket::packet_init(gAppState.gSensorPacket);

        const s8  temperature = gAppState.gCurrentBme.temperature;
        const u16 pressure    = gAppState.gCurrentBme.pressure;
        const u8  humidity    = gAppState.gCurrentBme.humidity;
        if (gAppState.gLastSendBme.temperature != temperature)
        {
            gAppState.gLastSendBme.temperature = temperature;
            nlog::printf("Temperature: %d °C\n", va_t((s32)temperature));
            npacket::packet_write(gAppState.gSensorPacket, npacket::ID_TEMPERATURE, nwifi::get_mac_address(gAppState.gWifiManager), (u16)temperature);
        }
        if (gAppState.gLastSendBme.pressure != pressure)
        {
            gAppState.gLastSendBme.pressure = pressure;
            nlog::printf("Pressure: %d hPa\n", va_t((u32)pressure));
            npacket::packet_write(gAppState.gSensorPacket, npacket::ID_PRESSURE, nwifi::get_mac_address(gAppState.gWifiManager), (u16)pressure);
        }
        if (gAppState.gLastSendBme.humidity != humidity)
        {
            gAppState.gLastSendBme.humidity = humidity;
            nlog::printf("Humidity: %d %%\n", va_t((u32)humidity));
            npacket::packet_write(gAppState.gSensorPacket, npacket::ID_HUMIDITY, nwifi::get_mac_address(gAppState.gWifiManager), (u16)humidity);
        }

        if (gAppState.gSensorPacket.Size > 0)
        {
            ntcp::send(gAppState.gTcpClient, gAppState.gSensorPacket.Data, gAppState.gSensorPacket.Size);
        }
#endif
    }

    void read_scd41()
    {
#ifdef ENABLE_SCD41
        // Read the SCD41 sensor data
        f32        scd_humi    = 0.0f;  // Initialize humidity value for SCD41
        f32        scd_temp    = 0.0f;  // Initialize temperature value for SCD41
        u16        scd_co2     = 0;     // Initialize CO2 value
        const bool valid_scd41 = nsensors::updateSCD41(scd_humi, scd_temp, scd_co2);

        if (valid_scd41)
        {
            gAppState.gCurrentScd.co2         = scd_co2;
            gAppState.gCurrentScd.temperature = static_cast<s8>(scd_temp);
            gAppState.gCurrentScd.humidity    = static_cast<u8>(scd_humi);
        }
#endif
    }

    void send_scd41()
    {
#ifdef ENABLE_SCD41
        // Write a custom (binary-format) network message

        npacket::packet_init(gAppState.gSensorPacket);

        const u16 co2         = gAppState.gCurrentScd.co2;
        const s8  temperature = gAppState.gCurrentScd.temperature;
        const u8  humidity    = gAppState.gCurrentScd.humidity;

        if (gAppState.gLastSendScd.co2 != co2)
        {
            gAppState.gLastSendScd.co2 = co2;
            nlog::printf("SCD CO2: %d ppm\n", va_t((u32)co2));
            npacket::packet_write(gAppState.gSensorPacket, npacket::ID_CO2, nwifi::get_mac_address(gAppState.gWifiManager), (u16)co2);
        }
        if (gAppState.gLastSendScd.temperature != temperature)
        {
            gAppState.gLastSendScd.temperature = temperature;
            nlog::printf("SCD Temperature: %d °C\n", va_t((s32)temperature));
            npacket::packet_write(gAppState.gSensorPacket, npacket::ID_TEMPERATURE, nwifi::get_mac_address(gAppState.gWifiManager), (u16)temperature);
        }
        if (gAppState.gLastSendScd.humidity != humidity)
        {
            gAppState.gLastSendScd.humidity = humidity;
            nlog::printf("SCD Humidity: %d %%\n", va_t((u32)humidity));
            npacket::packet_write(gAppState.gSensorPacket, npacket::ID_HUMIDITY, nwifi::get_mac_address(gAppState.gWifiManager), (u16)humidity);
        }

        if (gAppState.gSensorPacket.Size > 0)
        {
            ntcp::send(gAppState.gTcpClient, gAppState.gSensorPacket.Data, gAppState.gSensorPacket.Size);
        }
#endif
    }

    void read_rd03d()
    {
#ifdef ENABLE_RD03D
        if (nsensors::nrd03d::update(gAppState.gCurrentRd03dSensor))
        {
            for (s8 i = 0; i < 3; ++i)
            {
                nsensors::nrd03d::target_t tgt;
                if (nsensors::nrd03d::getTarget(gAppState.gCurrentRd03dSensor, i, tgt))
                {
                    gAppState.gCurrentRd03d.DetectionBits[i] = (gAppState.gCurrentRd03d.DetectionBits[i] << 1) | 1;
                    // nlog::printf("T%d: %d, %d\n", va_t(i), va_t(tgt[i].x), va_t(tgt[i].y));
                }
                else
                {
                    gAppState.gCurrentRd03d.DetectionBits[i] = (gAppState.gCurrentRd03d.DetectionBits[i] << 1) | 0;
                }

                u8 detected = gAppState.gCurrentRd03d.Detected[i] & 3;  // Current detection state

                const bool dseen = (gAppState.gCurrentRd03d.DetectionBits[i] & 0x3F) == 0x3F;
                if (dseen)
                {
                    // Too transition from no-presence to presence we must have seen 3 detections in a row (300 ms)
                    detected = ((detected << 1) | 1);
                }
                else
                {
                    const bool dnone = (gAppState.gCurrentRd03d.DetectionBits[i] & 0x3FFFFFFF) == 0;
                    if (dnone)
                    {
                        // To transition from presence to no-presence we must have seen 30 no-detections in a row (3 seconds)
                        detected = ((detected << 1) | 0);
                    }
                }
                gAppState.gCurrentRd03d.Detected[i] = detected;

                nlog::printf("T%d detection: %s\n", va_t(i), va_t((detected != 0) ? "PRESENCE" : "ABSENCE"));
            }
        }
#endif
    }

    void send_rd03d(void)
    {
#ifdef ENABLE_RD03D
        // Write a custom (binary-format) network message

        npacket::packet_init(gAppState.gSensorPacket);

        for (s8 i = 0; i < 3; ++i)
        {
            const u8 detected = gAppState.gCurrentRd03d.Detected[i];
            if (gAppState.gCurrentRd03d.LastSendDetected[i] != detected)
            {
                gAppState.gCurrentRd03d.LastSendDetected[i] = detected;
                npacket::packet_write(gAppState.gSensorPacket, npacket::ID_PRESENCE1 + i, nwifi::get_mac_address(gAppState.gWifiManager), (u16)detected);
            }
        }

        if (gAppState.gSensorPacket.Size > 0)
        {
            ntcp::send(gAppState.gTcpClient, gAppState.gSensorPacket.Data, gAppState.gSensorPacket.Size);
        }
#endif
    }

}  // namespace ncore

namespace ncore
{
    namespace napp
    {
#define SDA_PIN 21
#define SCL_PIN 22

        void wakeup(state_t* state, nwakeup::reason_t reason)
        {
            // No special handling needed for wakeup in this application, but we could add 
            // logic here if we wanted to do something specific on wakeup.
        }

        void presetup(state_t* state)
        {
            // Initialize I2C bus
            nwire::begin(SDA_PIN, SCL_PIN);
        }

        void setup(state_t* state)
        {
#ifdef ENABLE_BH1750
            gAppState.gCurrentBh.reset();
            gAppState.gLastSendBh.reset();
            nsensors::initBH1750();                // Initialize the BH1750 sensor
            gAppState.gBhSampleTask.interval_ms = 1 * 1000;  // Sample BH1750 every 1 second
            gAppState.gBhSampleTask.task_fn     = read_bh1750;
            gAppState.gBhSendTask.interval_ms   = 1 * 1000;  // Send BH1750 data every 1 second
            gAppState.gBhSendTask.task_fn       = send_bh1750;
#endif
#ifdef ENABLE_BME280
            gAppState.gCurrentBme.reset();
            gAppState.gLastSendBme.reset();
            nsensors::initBME280();                 // Initialize the BME280 sensor
            gAppState.gBmeSampleTask.interval_ms = 5 * 1000;  // Sample BME280 every 5 seconds
            gAppState.gBmeSampleTask.task_fn     = read_bme280;
            gAppState.gBmeSendTask.interval_ms   = 10 * 1000;  // Send BME280 data every 10 seconds
            gAppState.gBmeSendTask.task_fn       = send_bme280;
#endif
#ifdef ENABLE_SCD41
            gAppState.gCurrentScd.reset();
            gAppState.gLastSendScd.reset();
            nsensors::initSCD41();                  // Initialize the SCD4X sensor
            gAppState.gScdSampleTask.interval_ms = 5 * 1000;  // Sample SCD41 every 5 seconds
            gAppState.gScdSampleTask.task_fn     = read_scd41;
            gAppState.gScdSendTask.interval_ms   = 10 * 1000;  // Send SCD41 data every 10 seconds
            gAppState.gScdSendTask.task_fn       = send_scd41;
#endif
#ifdef ENABLE_RD03D
            gAppState.gCurrentRd03d.reset();
            nsensors::nrd03d::begin(gAppState.gCurrentRd03dSensor, 16, 17);  // Initialize RD03D sensor UART rx and tx pin
            gAppState.gRd03dSampleTask.interval_ms = 10;                               // Sample RD03D every 10 ms
            gAppState.gRd03dSampleTask.task_fn     = read_rd03d;
            gAppState.gRd03dSendTask.interval_ms   = 250;  // Send RD03D data every 250 ms
            gAppState.gRd03dSendTask.task_fn       = send_rd03d;
#endif
            nwifi::init_wifi_config(gAppState.gWifiConfig, WIFI_SSID(), WIFI_PASSWORD());
            nwifi::setup(gAppState.gWifiManager, &gAppState.gWifiConfig);

            void* socket = ntcp::setup_default(&gAppState.gTcpConfig);
            ntcp::setup(gAppState.gTcpClient, &gAppState.gTcpConfig, socket, SERVER_IP(), SERVER_TCPPORT());
        }

        void tick(state_t* state)
        {
            nnetwork::tick(gAppState.gWifiManager, gAppState.gTcpClient);

            ntimer::tick(&gAppState.gBhSampleTask);
            ntimer::tick(&gAppState.gBhSendTask);

            ntimer::tick(&gAppState.gBmeSampleTask);
            ntimer::tick(&gAppState.gBmeSendTask);

            ntimer::tick(&gAppState.gScdSampleTask);
            ntimer::tick(&gAppState.gScdSendTask);

            ntimer::tick(&gAppState.gRd03dSampleTask);
            ntimer::tick(&gAppState.gRd03dSendTask);
        }

    }  // namespace napp
}  // namespace ncore