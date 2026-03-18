#include "airquality/c_airquality.h"

#include "rcore/c_malloc.h"
#include "rcore/c_gpio.h"
#include "rcore/c_timer.h"
#include "rcore/c_log.h"
#include "rcore/c_packet.h"
#include "rcore/c_str.h"
#include "rcore/c_system.h"
#include "rcore/c_task.h"
#include "rcore/c_wire.h"

#include "rwifi/c_tcp.h"
#include "rwifi/c_node.h"

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
        npacket::packet_t gSensorPacket;  // Sensor packet for sending data

        bme280_data_t gCurrentBme;
        bme280_data_t gLastSendBme;

        bh1750_data_t gCurrentBh;
        bh1750_data_t gLastSendBh;

        scd41_data_t gCurrentScd;
        scd41_data_t gLastSendScd;

        nsensors::nrd03d::sensor_t gCurrentRd03dSensor;
        rd03d_data_t               gCurrentRd03d;
    };
    state_app_t  gAppState;
    state_task_t gAppTask;

    ntask::result_t read_bh1750(state_t* state)
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
        return ntask::RESULT_OK;
    }

    ntask::result_t send_bh1750(state_t* state)
    {
#ifdef ENABLE_BH1750
        const u16 lux = gAppState.gCurrentBh.lux;
        if (gAppState.gLastSendBh.lux != lux)
        {
            gAppState.gLastSendBh.lux = lux;

            // Write a custom (binary-format) network message
            npacket::packet_set_mac(gAppState.gSensorPacket, state->MACAddress);

            nlog::printf("Light: %d lx\n", va_t((u32)lux));
            npacket::sensor_block_t sensor_block;
            sensor_block.begin(&gAppState.gSensorPacket);
            {
                npacket::sensor_value_t light_sensor(npacket::sensor_block_t::ID_LIGHT, (u16)lux);
                sensor_block.write(&gAppState.gSensorPacket, light_sensor);
            }
            sensor_block.finalize(&gAppState.gSensorPacket);

            nnode::send_sensor_data(state, gAppState.gSensorPacket.Data, gAppState.gSensorPacket.Size);
        }
#endif
        return ntask::RESULT_OK;
    }

    ntask::result_t read_bme280(state_t* state)
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
        return ntask::RESULT_OK;
    }

    ntask::result_t send_bme280(state_t* state)
    {
#ifdef ENABLE_BME280
        // Write a custom (binary-format) network message
        npacket::packet_set_mac(gAppState.gSensorPacket, state->MACAddress);

        npacket::sensor_block_t sensor_block;
        sensor_block.begin(&gAppState.gSensorPacket);

        const s8  temperature = gAppState.gCurrentBme.temperature;
        const u16 pressure    = gAppState.gCurrentBme.pressure;
        const u8  humidity    = gAppState.gCurrentBme.humidity;
        if (gAppState.gLastSendBme.temperature != temperature)
        {
            gAppState.gLastSendBme.temperature = temperature;
            nlog::printf("Temperature: %d °C\n", va_t((s32)temperature));
            npacket::sensor_value_t temp_sensor(npacket::sensor_block_t::ID_TEMPERATURE, (u16)temperature);
            sensor_block.write(&gAppState.gSensorPacket, temp_sensor);
        }
        if (gAppState.gLastSendBme.pressure != pressure)
        {
            gAppState.gLastSendBme.pressure = pressure;
            nlog::printf("Pressure: %d hPa\n", va_t((u32)pressure));
            npacket::sensor_value_t pressure_sensor(npacket::sensor_block_t::ID_PRESSURE, (u16)pressure);
            sensor_block.write(&gAppState.gSensorPacket, pressure_sensor);
        }
        if (gAppState.gLastSendBme.humidity != humidity)
        {
            gAppState.gLastSendBme.humidity = humidity;
            nlog::printf("Humidity: %d %%\n", va_t((u32)humidity));
            npacket::sensor_value_t humidity_sensor(npacket::sensor_block_t::ID_HUMIDITY, (u16)humidity);
            sensor_block.write(&gAppState.gSensorPacket, humidity_sensor);
        }

        if (sensor_block.finalize(&gAppState.gSensorPacket) > 0)
        {
            nnode::send_sensor_data(state, gAppState.gSensorPacket.Data, gAppState.gSensorPacket.Size);
        }
#endif
        return ntask::RESULT_OK;
    }

    ntask::result_t read_scd41(state_t* state)
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

        return ntask::RESULT_OK;
    }

    ntask::result_t send_scd41(state_t* state)
    {
#ifdef ENABLE_SCD41
        // Write a custom (binary-format) network message
        npacket::packet_set_mac(gAppState.gSensorPacket, state->MACAddress);

        npacket::sensor_block_t sensor_block;
        sensor_block.begin(&gAppState.gSensorPacket);

        const u16 co2         = gAppState.gCurrentScd.co2;
        const s8  temperature = gAppState.gCurrentScd.temperature;
        const u8  humidity    = gAppState.gCurrentScd.humidity;

        if (gAppState.gLastSendScd.co2 != co2)
        {
            gAppState.gLastSendScd.co2 = co2;
            nlog::printf("SCD CO2: %d ppm\n", va_t((u32)co2));
            npacket::sensor_value_t co2_sensor(npacket::sensor_block_t::ID_CO2, (u16)co2);
            sensor_block.write(&gAppState.gSensorPacket, co2_sensor);
        }
        if (gAppState.gLastSendScd.temperature != temperature)
        {
            gAppState.gLastSendScd.temperature = temperature;
            nlog::printf("SCD Temperature: %d °C\n", va_t((s32)temperature));
            npacket::sensor_value_t temp_sensor(npacket::sensor_block_t::ID_TEMPERATURE, (u16)temperature);
            sensor_block.write(&gAppState.gSensorPacket, temp_sensor);
        }
        if (gAppState.gLastSendScd.humidity != humidity)
        {
            gAppState.gLastSendScd.humidity = humidity;
            nlog::printf("SCD Humidity: %d %%\n", va_t((u32)humidity));
            npacket::sensor_value_t humidity_sensor(npacket::sensor_block_t::ID_HUMIDITY, (u16)humidity);
            sensor_block.write(&gAppState.gSensorPacket, humidity_sensor);
        }

        if (sensor_block.finalize(&gAppState.gSensorPacket) > 0)
        {
            nnode::send_sensor_data(state, gAppState.gSensorPacket.Data, gAppState.gSensorPacket.Size);
        }
#endif

        return ntask::RESULT_OK;
    }

    ntask::result_t read_rd03d(state_t* state)
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
        return ntask::RESULT_OK;
    }

    ntask::result_t send_rd03d(state_t* state)
    {
#ifdef ENABLE_RD03D
        // Write a custom (binary-format) network message
        npacket::packet_set_mac(gAppState.gSensorPacket, state->MACAddress);

        npacket::sensor_block_t sensor_block;
        sensor_block.begin(&gAppState.gSensorPacket);

        for (s8 i = 0; i < 3; ++i)
        {
            const u8 detected = gAppState.gCurrentRd03d.Detected[i];
            if (gAppState.gCurrentRd03d.LastSendDetected[i] != detected)
            {
                gAppState.gCurrentRd03d.LastSendDetected[i] = detected;
                npacket::sensor_value_t presence_sensor(npacket::sensor_block_t::ID_PRESENCE1 + i, (u16)detected);
                sensor_block.write(&gAppState.gSensorPacket, presence_sensor);
            }
        }

        if (sensor_block.finalize(&gAppState.gSensorPacket) > 0)
        {
            nnode::send_sensor_data(state, gAppState.gSensorPacket.Data, gAppState.gSensorPacket.Size);
        }
#endif
        return ntask::RESULT_OK;
    }

}  // namespace ncore

namespace ncore
{
    namespace napp
    {
        ntask::periodic_t periodic_read_bh1750(5 * 1000);
        ntask::periodic_t periodic_read_bme280(10 * 1000);
        ntask::periodic_t periodic_read_scd41(30 * 1000);
        ntask::periodic_t periodic_read_rd03d(100);

        ntask::periodic_t periodic_send_bh1750((30 * 1000) + 1);
        ntask::periodic_t periodic_send_bme280((60 * 1000) + 2);
        ntask::periodic_t periodic_send_scd41((2 * 60 * 1000) - 1);
        ntask::periodic_t periodic_send_rd03d(200 + 3);

        void main_program(ntask::scheduler_t* exec, state_t* state)
        {
            if (ntask::is_first_call(exec))
            {
                ntask::init_periodic(exec, periodic_read_bh1750);
                ntask::init_periodic(exec, periodic_read_bme280);
                ntask::init_periodic(exec, periodic_read_scd41);
                ntask::init_periodic(exec, periodic_read_rd03d);

                ntask::init_periodic(exec, periodic_send_bh1750);
                ntask::init_periodic(exec, periodic_send_bme280);
                ntask::init_periodic(exec, periodic_send_scd41);
                ntask::init_periodic(exec, periodic_send_rd03d);
            }

            // Reading sensor data

#ifdef ENABLE_BH1750
            if (ntask::periodic(exec, periodic_read_bh1750))
            {
                ntask::call(exec, read_bh1750);
            }
#endif
#ifdef ENABLE_BME280
            if (ntask::periodic(exec, periodic_read_bme280))
            {
                ntask::call(exec, read_bme280);
            }
#endif
#ifdef ENABLE_SCD41
            if (ntask::periodic(exec, periodic_read_scd41))
            {
                ntask::call(exec, read_scd41);
            }
#endif
#ifdef ENABLE_RD03D
            if (ntask::periodic(exec, periodic_read_rd03d))
            {
                ntask::call(exec, read_rd03d);
            }
#endif

            // Sending sensor data

#ifdef ENABLE_BH1750
            if (ntask::periodic(exec, periodic_send_bh1750))
            {
                ntask::call(exec, send_bh1750);
            }
#endif
#ifdef ENABLE_BME280
            if (ntask::periodic(exec, periodic_send_bme280))
            {
                ntask::call(exec, send_bme280);
            }
#endif
#ifdef ENABLE_SCD41
            if (ntask::periodic(exec, periodic_send_scd41))
            {
                ntask::call(exec, send_scd41);
            }
#endif
#ifdef ENABLE_RD03D
            if (ntask::periodic(exec, periodic_send_rd03d))
            {
                ntask::call(exec, send_rd03d);
            }
#endif
        }
        ntask::program_t gMainProgram(main_program);

#define SDA_PIN 21
#define SCL_PIN 22

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
            nsensors::initBH1750();  // Initialize the BH1750 sensor
#endif
#ifdef ENABLE_BME280
            gAppState.gCurrentBme.reset();
            gAppState.gLastSendBme.reset();
            nsensors::initBME280();  // Initialize the BME280 sensor
#endif
#ifdef ENABLE_SCD41
            gAppState.gCurrentScd.reset();
            gAppState.gLastSendScd.reset();
            nsensors::initSCD41();  // Initialize the SCD4X sensor
#endif
#ifdef ENABLE_RD03D
            gAppState.gCurrentRd03d.reset();
            nsensors::nrd03d::begin(gAppState.gCurrentRd03dSensor, 16, 17);  // Initialize RD03D sensor UART rx and tx pin
#endif
            ntask::set_main(state, &gAppTask, &gMainProgram);

            // A node is responsible for connecting to WiFi, it initializes WiFi, TCP and UDP
            // state and then starts the main program that will read the sensors and send
            // sensor data to the server.
            // When WiFi connection is lost the node program will jump back to the WiFi connection
            // program and try to reconnect. If it cannot reconnect within a certain timeout it will
            // reset the WiFi configuration (so that next time it starts connecting from scratch) and
            // then jump back to the WiFi connection program again.
            nnode::initialize(state, &gAppTask);
        }

        void tick(state_t* state) { ntask::tick(state, &gAppTask); }

    }  // namespace napp
}  // namespace ncore