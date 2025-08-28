#ifndef OPENTHREAD_EVENTS_H
#define OPENTHREAD_EVENTS_H

#include <zephyr/kernel.h>
#include <openthread/instance.h>
#include <openthread/thread.h>
#include <openthread/coap.h>
#include <zephyr/net/openthread.h>
#include<openthread/commissioner.h>
#include<openthread/thread_ftd.h>
#include"utils.h"
#include <openthread/ip6.h>   // otMessageInfo buradan geliyor



/**
 * @brief Maksimum mesaj kuyugu oge sayisi.
 */
#define OPENTHREAD_EVENTS_MSGQ_MAX_MSGS   10
/**
 * @brief Mesaj kuyugu hizalamasi.
 */
#define OPENTHREAD_EVENTS_MSGQ_ALIGNMENT  1

/**
 * @brief OpenThread olay is parcacigi yigin boyutu (bayt).
 */
#define OPENTHREAD_EVENTS_THREAD_STACK_SIZE   2048
/**
 * @brief OpenThread olay is parcacigi onceligi.
 */
#define OPENTHREAD_EVENTS_THREAD_PRIORITY      7

/**
 * @brief Test/ornek is parcacigi yigin boyutu (bayt).
 */
#define OPENTHREAD_EVENTS_TEST_THREAD_STACK_SIZE   2048
/**
 * @brief Test/ornek is parcacigi onceligi.
 */
#define OPENTHREAD_EVENTS_TEST_THREAD_PRIORITY      4

/**
 * @brief CoAP kaynak URI: Health.
 */
#define OPENTHREAD_COAP_URI_HEALTH "Hlth"

/**
 * @brief OpenThread mesaj sarmalayıcısı.
 */
typedef struct
{
   uint8_t message[BUFFER_SIZE];
   uint16_t len;
   otMessageInfo message_info;
} openthread_message_info_t;




/**
 * @brief Kaynak URI yolu.
 */
typedef enum {
    DEVICE_DATA = 0, /*!< Cihaz verisi */
    HEALTH = 1       /*!< Sağlık bilgisi */
} inolink_uri_path_t;





#endif