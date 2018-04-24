#include "mbed.h"

#define __STDC_FORMAT_MACROS
#include <string>
#include <sstream>
#include <vector>
#include "security.h"
#include "simpleclient.h"
#include "mbedtls/entropy_poll.h"
#include "setting_proc.h"
#include "DisplayEfect.h"
#include "FileAnalysis.h"

extern setting_t * get_setting_pointer(void);

// easy-connect compliancy, it has 2 sets of wifi pins we have only one
#define MBED_CONF_APP_ESP8266_TX MBED_CONF_APP_WIFI_TX
#define MBED_CONF_APP_ESP8266_RX MBED_CONF_APP_WIFI_RX
#include "easy-connect.h"

// These are example resource values for the Device Object
struct MbedClientDevice device = {
    "Manufacturer_String",      // Manufacturer
    "Type_String",              // Type
    "ModelNumber_String",       // ModelNumber
    "SerialNumber_String"       // SerialNumber
};

// Instantiate the class which implements LWM2M Client API (from simpleclient.h)
MbedClient mbed_client(device);

/*
 * The HVC contains a function (send string).
 * When `handle_string_send` is executed, the string after decoding is sent.
 */
class HvcResource {
public:
    HvcResource() : index_file_update(false), playlist_update(false) {
        // create ObjectID with metadata tag of '3203', which is 'send string'
        hvc_object = M2MInterfaceFactory::create_object("3203");
        M2MObjectInstance* hvc_inst = hvc_object->create_object_instance();

        // start_up
        start_up_res = hvc_inst->create_dynamic_resource("1", "start_up", M2MResourceInstance::INTEGER, true);
        start_up_res->set_operation(M2MBase::GET_ALLOWED);
        start_up_res->set_value(0);

        // hvc
        hvc_res = hvc_inst->create_dynamic_resource("2", "hvc", M2MResourceInstance::STRING, true);
        hvc_res->set_operation(M2MBase::GET_ALLOWED);
        hvc_res->set_value((uint8_t*)"", 1);

        // index_file
        index_file_res = hvc_inst->create_dynamic_resource("3", "index_file", M2MResourceInstance::INTEGER, true);
        index_file_res->set_operation(M2MBase::GET_PUT_ALLOWED);
        index_file_res->set_value((const uint8_t*)"index.txt", sizeof("index.txt"));
        index_file_res->set_value_updated_function(value_updated_callback(this, &HvcResource::set_index_file));

        // image_list
        image_list_res = hvc_inst->create_dynamic_resource("7", "image_list", M2MResourceInstance::STRING, true);
        image_list_res->set_operation(M2MBase::GET_PUT_ALLOWED);
        image_list_res->set_value((uint8_t*)"0", 1);
        image_list_res->set_value_updated_function(value_updated_callback(this, &HvcResource::set_image_list));

        // setting
        setting_res = hvc_inst->create_dynamic_resource("8", "setting", M2MResourceInstance::STRING, true);
        setting_res->set_operation(M2MBase::POST_ALLOWED);
        setting_res->set_execute_function(execute_callback(this, &HvcResource::set_seting));
    }

    ~HvcResource() {
    }

    M2MObject* get_object() {
        return hvc_object;
    }

    void handle_string_send(char * addr, int size) {
        // tell the string to connector
        hvc_res->set_value((uint8_t *)addr, size);
    }

    bool check_new_playlist(uint32_t * p_total_file_num) {
        uint8_t* buffIn = NULL;
        uint32_t sizeIn;

        if (index_file_update) {
            index_file_update = false;
            index_file_res->get_value(buffIn, sizeIn);
            *p_total_file_num = FileAnalysis((char *)buffIn);
            return true;
        }
        if (playlist_update) {
            playlist_update = false;
            image_list_res->get_value(buffIn, sizeIn);
            if (buffIn[0] != '\0') {
                *p_total_file_num = MemoryAnalysis((char *)buffIn, ",");
                return true;
            }
        }
        return false;
    }

private:
    M2MObject* hvc_object;
    M2MResource* start_up_res;
    M2MResource* hvc_res;
    M2MResource* index_file_res;
    M2MResource* image_list_res;
    M2MResource* setting_res;
    bool index_file_update;
    bool playlist_update;

    void set_index_file(const char *argument) {
        uint8_t* buffIn = NULL;
        uint32_t sizeIn;
        index_file_res->get_value(buffIn, sizeIn);
        if (strcmp((char *)buffIn, "IMAGE_LIST") == 0) {
            set_image_list(argument);
        } else {
            index_file_update = true;
        }
        start_up_res->set_value(1);
    }

    void set_image_list(const char *argument) {
        index_file_res->set_value((const uint8_t*)"IMAGE_LIST", sizeof("IMAGE_LIST"));
        playlist_update = true;
        start_up_res->set_value(1);
    }

    void set_seting(void *argument) {
        M2MResource::M2MExecuteParameter *pram = (M2MResource::M2MExecuteParameter *)argument;
        setting_change(pram->get_argument_value(), get_setting_pointer(), ",");
        start_up_res->set_value(1);
    }

};

HvcResource hvc_resource;

void set_hvc_result(char * str) {
    hvc_resource.handle_string_send(str, strlen(str) + 1);
}

bool check_new_playlist(uint32_t * p_total_file_num) {
    return hvc_resource.check_new_playlist(p_total_file_num);
}

static volatile bool registered;

void unregister() {
    registered = false;
}

void mbed_client_task(void) {

    unsigned int seed;
    size_t len;

#ifdef MBEDTLS_ENTROPY_HARDWARE_ALT
    // Used to randomize source port
    mbedtls_hardware_poll(NULL, (unsigned char *) &seed, sizeof seed, &len);

#elif defined MBEDTLS_TEST_NULL_ENTROPY

#warning "mbedTLS security feature is disabled. Connection will not be secure !! Implement proper hardware entropy for your selected hardware."
    // Used to randomize source port
    mbedtls_null_entropy_poll( NULL,(unsigned char *) &seed, sizeof seed, &len);

#else

#error "This hardware does not have entropy, endpoint will not register to Connector.\
You need to enable NULL ENTROPY for your application, but if this configuration change is made then no security is offered by mbed TLS.\
Add MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES and MBEDTLS_TEST_NULL_ENTROPY in mbed_app.json macros to register your endpoint."

#endif

    srand(seed);
    // Keep track of the main thread
    printf("\nStarting mbed Client example in ");

#if defined (MESH) || (MBED_CONF_LWIP_IPV6_ENABLED==true)
    printf("IPv6 mode\n");
#else
    printf("IPv4 mode\n");
#endif

#if defined(TARGET_RZ_A1H) && (MBED_CONF_APP_NETWORK_INTERFACE == WIFI_BP3595)
    DigitalOut usb1en(P3_8);
    usb1en = 1;
    Thread::wait(5);
    usb1en = 0;
    Thread::wait(5);
#endif

    NetworkInterface* network = easy_connect(true);
    if (network == NULL) {
        printf("\nConnection to Network Failed - exiting application...\n");
        return;
    }

    // Create endpoint interface to manage register and unregister
    mbed_client.create_interface(MBED_SERVER_ADDRESS, network);

    // Create Objects of varying types, see simpleclient.h for more details on implementation.
    M2MSecurity* register_object = mbed_client.create_register_object(); // server object specifying connector info
    M2MDevice*   device_object   = mbed_client.create_device_object();   // device resources object

    // Create list of Objects to register
    M2MObjectList object_list;

    // Add objects to list
    object_list.push_back(device_object);
    object_list.push_back(hvc_resource.get_object());

    // Set endpoint registration object
    mbed_client.set_register_object(register_object);

    // Register with mbed Device Connector
    mbed_client.test_register(register_object, object_list);

    Timer update_timer;
    update_timer.reset();
    update_timer.start();

    registered = true;
    InterruptIn unreg_button(USER_BUTTON0);
    unreg_button.fall(&unregister);

    while (registered) {
        if (update_timer.read() >= 25) {
            mbed_client.test_update_register();
            update_timer.reset();
        } else {
            // do nothing
        }
        Thread::wait(5);
    }

    mbed_client.test_unregister();
}

// set mac address
void mbed_mac_address(char *mac) {
    mac[0] = 0x00;
    mac[1] = 0x1D;
    mac[2] = 0x12;
    mac[3] = 0xBC;
    mac[4] = 0x84;
    mac[5] = 0xDE;
}

