/*!
\copyright  Copyright (c) 2008 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for Battery monitoring
*/

#ifndef BATTERY_MONITOR_H_
#define BATTERY_MONITOR_H_

#include "domain_message.h"

#include <marshal.h>

/*! Battery level updates messages. The message a client receives depends upon
    the batteryRegistrationForm::representation set when registering by calling
    #appBatteryRegister.  */
enum battery_messages
{
    /*! Message signalling the battery module initialisation is complete */
    MESSAGE_BATTERY_INIT_CFM = BATTERY_APP_MESSAGE_BASE,
    /*! Message signalling the battery voltage has changed. */
    MESSAGE_BATTERY_LEVEL_UPDATE_VOLTAGE,
    /*! This must be the final message */
    BATTERY_APP_MESSAGE_END
};

/*! Options for representing the battery voltage */
enum battery_level_representation
{
    /*! As a voltage */
    battery_level_repres_voltage
};

/*! Message #MESSAGE_BATTERY_LEVEL_UPDATE_VOLTAGE content. */
typedef struct
{
    /*! The updated battery voltage in milli-volts. */
    uint16 voltage_mv;
} MESSAGE_BATTERY_LEVEL_UPDATE_VOLTAGE_T;
extern const marshal_type_descriptor_t marshal_type_descriptor_MESSAGE_BATTERY_LEVEL_UPDATE_VOLTAGE_T;

/*! Battery client registration form */
typedef struct
{
    /*! The task that will receive battery status messages */
    Task task;
    /*! The representation method requested by the client */
    enum battery_level_representation representation;

    /*! The reporting hysteresis
    */
    uint16 hysteresis;

} batteryRegistrationForm;

/*! Structure used internally to the battery module to store per-client state */
typedef struct battery_registered_client_item
{
    /*! The next client in the list */
    struct battery_registered_client_item *next;
    /*! The client's registration information */
    batteryRegistrationForm form;
    /*! The last battery voltage sent to the client */
    
    /*! As a voltage */
    uint16 voltage;
    
} batteryRegisteredClient;

/*! Battery task structure */
typedef struct
{
    /*! Battery task */
    TaskData task;
    /*! The measurement period. Value between 500 and 10000 ms. */
    uint16 period;  
    /*! Store the vref measurement, which is required to calculate vbat */
    uint16 vref_raw;
    /* track cfm sending */
    bool cfm_sent;
    /*! A sub-struct to allow reset */
    struct
    {
        /*! Configurable window used for median filter. Value 3 or 5. */
        uint16 median_filter_window;
        /* latest value */
        uint16 instantaneous;
    } filter;
    struct
    {
        /* smoothing factor with value between 0 and 1 */
        uint8 weight;
        /* last exponential moving average. */
        uint32 last_ema;
        uint32 current_ema;
    } average;
    /*! A linked-list of clients */
    batteryRegisteredClient *client_list;
} batteryTaskData;

/*! \brief Battery component task data. */
extern batteryTaskData app_battery;
/*! \brief Access the battery task data. */
#define GetBattery()    (&app_battery)

/*! Start monitoring the battery voltage */
bool appBatteryInit(Task init_task);

/*! @brief Register to receive battery change notifications.

    @note The first notification after registering will only be
    sent when sufficient battery readings have been taken after
    power on to ensure that the notification represents a stable
    value.

    @param form The client's registration form.

    @return TRUE on successful registration, otherwise FALSE.
*/
bool appBatteryRegister(batteryRegistrationForm *form);

/*! @brief Unregister a task from receiving battery change notifications.
    @param task The client task to unregister.
    Silently ignores unregister requests for a task not previously registered
*/
void appBatteryUnregister(Task task);

/*! @brief Read the averaged battery voltage in mV.
    @return The battery voltage. */
uint16 appBatteryGetVoltageAverage(void);

/*! @brief Read the filtered battery voltage in mV.
    @return The battery voltage. */
uint16 appBatteryGetVoltageInstantaneous(void);

/*! \brief Override the battery level for test purposes.

    After calling this function actual battery measurements will be ignored,
    and voltage value will be used instead.

    \param voltage Voltage level to be used.
*/
void appBatteryTestSetFakeVoltage(uint16 voltage);

/*! @brief Unset test battery value to 0 and start periodic monitoring.
 */
void appBatteryTestUnsetFakeVoltage(void);

/*! \brief Inject new battery level for test purposes.

    After calling this function actual battery measurements will be ignored,
    and voltage value will be used instead.

    \param voltage Voltage level to be used.
*/
void appBatteryTestInjectFakeLevel(uint16 voltage);

/*! @brief Unset test battery value to 0 and start periodic monitoring.
 */
void appBatteryTestResumeAdcMeasurements(void);

#endif /* BATTERY_MONITOR_H_ */
