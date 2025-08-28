#include "openthread_events.h"
#include <zephyr/logging/log.h>
#include<zephyr/init.h>

LOG_MODULE_REGISTER(openthread_events, LOG_LEVEL_DBG);


otError openthread_multicast_send(const uint8_t* cmd, uint16_t size,const char* ipaddr);
static otError openthread_coap_send_multicast(const inolink_uri_path_t uri_path,const uint8_t* msg , uint16_t *buffer_index, const otCoapType a_type, const otCoapCode a_code ,const char* ipaddr);
static void store_data_request_callback(void *p_context, otMessage *p_message, const otMessageInfo *p_message_info);
static void state_changed_callback(uint32_t flags, void *context);
static void neighbor_callback(otNeighborTableEvent event_type, const otNeighborTableEntryInfo *entry_info, void *context);
static void ip6_address_callback(const otIp6AddressInfo *address_info, bool is_added, void *context);

otExtAddress device_mac_addr = {0};
K_MSGQ_DEFINE(thread_msgq, sizeof(openthread_message_info_t), OPENTHREAD_EVENTS_MSGQ_MAX_MSGS, OPENTHREAD_EVENTS_MSGQ_ALIGNMENT);

                   
/**
 * @brief  Cihazın benzersiz MAC adresini (DEVICEID üzerinden) verilen buffer'a kopyalar.
 *
 * Bu fonksiyon, cihazın donanımsal olarak tanımlı olan DEVICEID bilgisini alır
 * ve @p mac_addr ile gosterilen buffer'a kopyalar. Eger belirtilen uzunluk
 * (len), DEVICEID boyutunu aşarsa, kopyalama boyutu DEVICEID boyutuna
 * sınırlandırılır.
 *
 * @param[out] mac_addr  Cihaz MAC adresinin yazilacagi hedef buffer.
 * @param[in]  len      Kopyalanacak byte sayısı.
 *
 * @note  Eğer @p len, DEVICEID boyutundan büyükse, fonksiyon uyarı logu yazar
 *        ve kopyalanan uzunluğu DEVICEID boyutuyla sınırlar.
 */
static void write_device_mac_addr(uint8_t* mac_addr , uint8_t len)
{
    if(len > (uint8_t)sizeof(NRF_FICR->DEVICEID))
    {
        LOG_WRN("Uzunluk DEVICEID boyutunu asiyor");
        len = (uint8_t)sizeof(NRF_FICR->DEVICEID);
    }
    LOG_DBG("NRF_FICR = %d", NRF_FICR->DEVICEID[0]);
    volatile uint8_t* src = (volatile uint8_t*)P_DEVICE_MAC_ADDR;
    uint8_t* dest = mac_addr;
    while(len--) *dest++ = *src++; 
}



/**
 * @brief  OpenThread yığınını başlatır ve cihazın genişletilmiş (extended) adresini ayarlar.
 *
 * Bu fonksiyon, cihazın donanımsal MAC adresini alır, OpenThread
 * örneğine (otInstance) genişletilmiş adres olarak tanımlar ve
 * ardından OpenThread protokolünü başlatır.
 *
 * Fonksiyon, hem genişletilmiş adres ayarlama hem de OpenThread
 * başlatma işlemlerinde hata alındığında, belirli sayıda (MAX_ATTEMPTS)
 * tekrar denemesi yapar. İşlemler başarısız olursa hata kodunu döner.
 *
 * @return
 *  - OT_ERROR_NONE  : Başarılı
 *  - Diğer otError  : Extended address ayarlama veya OpenThread başlatma sırasında hata
 *
 * @note
 * - Adres ayarlama veya başlatma başarısız olursa, her deneme arasında
 *   @p RETRY_DELAY_MS kadar beklenir.
 * - Mutex kilitleme ve açma işlemleri otContext üzerinden yapılır.
 * - Başarısız durumlarda loglama ile hata bilgisi kullanıcıya iletilir.
 */
static otError openthread_init(void)
{
    otInstance* instance = openthread_get_default_instance();
    otError error = OT_ERROR_NONE;
    struct openthread_context *ot_context = openthread_get_default_context();
    write_device_mac_addr(device_mac_addr.m8, (uint8_t)OT_EXT_ADDRESS_SIZE);
    openthread_api_mutex_lock(ot_context);
    uint8_t retry_count = 0;
    do
    {
         error = otLinkSetExtendedAddress(instance, &device_mac_addr);
         if(error == OT_ERROR_NONE)
         {
            LOG_INF("Extended adres ayarlandi: %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                    device_mac_addr.m8[0], device_mac_addr.m8[1], device_mac_addr.m8[2], device_mac_addr.m8[3],
                    device_mac_addr.m8[4], device_mac_addr.m8[5], device_mac_addr.m8[6], device_mac_addr.m8[7]);
            break;
         }
        LOG_WRN("Extended adres ayarlanamadi, tekrar deneniyor... (%d/%d)", retry_count+1, MAX_ATTEMPTS);
        k_msleep(RETRY_DELAY_MS);
    } while (++retry_count < MAX_ATTEMPTS);
    if(error != OT_ERROR_NONE)
    {
        LOG_ERR("Extended adres ayarlanamadi, hata: %d", error);
        openthread_api_mutex_unlock(ot_context);
        return error;
    }
    openthread_api_mutex_unlock(ot_context);
    retry_count = 0;
     do{
        error = openthread_start(ot_context);
        if(error == OT_ERROR_NONE)
        {
            LOG_INF("OpenThread baslatildi.");
            break;
        }
        LOG_INF("OpenThread baslatilamadi, tekrar deneniyor... (%d/%d)", retry_count+1, MAX_ATTEMPTS);
        k_msleep(RETRY_DELAY_MS);

     }while(++retry_count < MAX_ATTEMPTS );
    if(error != OT_ERROR_NONE)
    {
        LOG_ERR("OpenThread baslatma hatasi: %d", error);
    }

    return error ;
}

/**
 * @brief  OpenThread üzerinde CoAP sunucusunu başlatır ve temel resource'u ekler.
 *
 * Bu fonksiyon, OpenThread instance'ını alır ve belirtilen portta
 * CoAP sunucusunu başlatmaya çalışır. Başlatma işlemi başarısız olursa
 * belirli sayıda (MAX_ATTEMPTS) tekrar denenir. Başarıyla başlatılırsa,
 * "Hlth" URI path'ine sahip temel resource eklenir.
 *
 * @return
 *  - OT_ERROR_NONE  : CoAP sunucusu başarıyla başlatıldı.
 *  - Diğer otError  : Başlatma işlemi sırasında hata oluştu.
 *
 * @note
 * - Retry sırasında her deneme arasında @p RETRY_DELAY_MS kadar beklenir.
 * - Fonksiyon, resource eklerken static otCoapResource kullanır.
 * - Başlatma başarısız olursa hata logu yazılır.
 */
static otError coap_init(void)
{
    otInstance* instance = openthread_get_default_instance();
    otError error = OT_ERROR_NONE;
    static otCoapResource general_resource = {
                        .mUriPath = "data",  
                        .mHandler = store_data_request_callback,  
                        .mContext = NULL,
                        .mNext    = NULL,
    };
    general_resource.mContext = instance;
    uint8_t retry_count = 0;
    do{
         error = otCoapStart(instance, OT_DEFAULT_COAP_PORT);
         if(error == OT_ERROR_NONE)
         {
            LOG_INF("CoAP sunucusu %d portunda baslatildi", OT_DEFAULT_COAP_PORT);
            otCoapAddResource(instance, &general_resource);
            break;
         }
         k_msleep(RETRY_DELAY_MS);
         LOG_WRN("CoAP sunucusu baslatilamadi, tekrar deneniyor... (%d/%d)", retry_count+1, MAX_ATTEMPTS);
    

    }while(retry_count++ < MAX_ATTEMPTS);
    return error ;
}


/**
 * @brief  OpenThread cihazına statik bir IPv6 unicast adresi ekler.
 *
 * Bu fonksiyon, cihazın OpenThread instance'ına (otInstance)
 * sabit tanımlı bir IPv6 adresi ekler. Adres, 128 bitlik prefix ile
 * tam adres olarak eklenir ve hem valid hem preferred olarak işaretlenir.
 *
 * @return
 *  - OT_ERROR_NONE  : Adres başarıyla eklendi.
 *  - Diğer otError  : Adres ekleme sırasında bir hata oluştu.
 *
 * @note
 * - Hata oluşursa, fonksiyon log ile hata mesajını gösterir.
 * - IPv6 adresi sabit olarak k_ipv6_address array'inde tanımlıdır.
 * - Bu fonksiyonun çağrılabilmesi için OpenThread instance'ın
 *   başlatılmış olması gerekir.
 */
static otError add_ipv6_address(void)
{
    otInstance* instance = openthread_get_default_instance();

    static const uint8_t k_ipv6_address[16] = { 
        0xfd, 0xde, 0xad, 0x00, 0xbe, 0xef, 0x00, 0x00, 
        0x9d, 0xa4, 0x48, 0x86, 0xf1, 0xa9, 0x82, 0x4e                        // Last 32 bits (192.168.2.10 in hex)
    };
    otNetifAddress netif_address = {0};
    memcpy(&netif_address.mAddress.mFields.m8[0], k_ipv6_address, OT_IP6_ADDRESS_SIZE);
    netif_address.mPrefixLength = 128;

    netif_address.mValid = true;
    netif_address.mPreferred = true;
    otError error = otIp6AddUnicastAddress(instance, &netif_address);

    if (error != OT_ERROR_NONE) {
       LOG_ERR("IPv6 adres ekleme hatasi: %s", otThreadErrorToString(error));
    }
    return error;
}

/**
 * @brief  Sistem ve OpenThread altyapısını başlatır.
 *
 * Bu fonksiyon, OpenThread cihazının çalışması için gerekli tüm
 * başlangıç işlemlerini sırasıyla gerçekleştirir:
 * 1. OpenThread yığınını başlatır (`openthread_init()`).
 * 2. CoAP sunucusunu başlatır ve temel resource'u ekler (`coap_init()`).
 * 3. Statik IPv6 adresi ekler (`add_ipv6_address()`).
 * 4. OpenThread state değişiklikleri için callback fonksiyonunu kaydeder.
 * 5. Komşu ve IPv6 adres değişiklikleri için callback fonksiyonlarını kaydeder.
 *
 * Her adımda hata oluşursa, fonksiyon hata kodunu döndürür ve log yazar.
 *
 * @param dev  Kullanılmayan cihaz parametresi (ARG_UNUSED).
 *
 * @return
 *  - 0 (int)           : Tüm başlatma işlemleri başarılı.
 *  - Diğer otError kodu : Başlatma sırasında oluşan hata.
 *
 * @note
 * - Fonksiyon, cihazın OpenThread instance'ının daha önce oluşturulmuş
 *   olmasını gerektirir.
 * - Callback fonksiyonları cihazın durumunu ve ağ olaylarını takip etmek
 *   için gereklidir.
 */
static int system_init(const struct device *dev)
{
    ARG_UNUSED(dev);
    otInstance* instance = openthread_get_default_instance();
    otError error = openthread_init();
    if(error != OT_ERROR_NONE)
    {
        LOG_ERR("OpenThread ilklendirme basarisiz, hata: %d", error);
        return (int)error ;
    } 
    error = coap_init();
    if(error != OT_ERROR_NONE)
    {
        LOG_ERR("CoAP ilklendirme basarisiz, hata: %d", error);
        return (int)error ;
    }
    error = add_ipv6_address();
    if(error != OT_ERROR_NONE)
    {
        LOG_ERR("IPv6 adres ekleme basarisiz, hata: %d", error);
        return (int)error ;
    }
    error = otSetStateChangedCallback(instance, state_changed_callback, instance);
    if(error != OT_ERROR_NONE)
    {
        LOG_ERR("State degisiklik callback kaydi basarisiz, hata: %d", error);
        return (int)error ;
    }
    
    otThreadRegisterNeighborTableCallback(instance, neighbor_callback);
    otIp6SetAddressCallback(instance, ip6_address_callback, instance);


    return (int)error ;
}



SYS_INIT(system_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);



/**
 * @brief  OpenThread CoAP mesaj callback fonksiyonu.
 *
 * Bu fonksiyon, CoAP sunucusuna gelen mesajları işler ve
 * bir message queue'ya (thread_msgq) ekler.
 *
 * @param p_context       Callback context (OpenThread instance pointer).
 * @param p_message       Gelen otMessage pointer'ı.
 * @param p_message_info  Mesaj ile ilgili ek bilgi (otMessageInfo).
 *
 * @note
 * - Mesaj queue dolu ise, mesaj loglanır ve düşürülür.
 * - Mesaj boyutu BUFFER_SIZE - 1 ile sınırlıdır.
 * - Fonksiyon thread-safe olacak şekilde sadece queue'ya ekleme yapar.
 */
static void store_data_request_callback(void *p_context, otMessage *p_message, const otMessageInfo *p_message_info)
{
    OT_UNUSED_VARIABLE(p_context);
    openthread_message_info_t msg_info;
    otInstance *instance = (otInstance *)p_context;
    msg_info.len = otMessageRead(p_message, otMessageGetOffset(p_message), msg_info.message,  BUFFER_SIZE - 1);
    memcpy(&msg_info.message_info, p_message_info, sizeof(otMessageInfo));
    if(k_msgq_put(&thread_msgq, &msg_info, K_NO_WAIT) != 0)
    {
        LOG_WRN("Mesaj kuyrugu dolu, mesaj atildi");
    }
}



/**
 * @brief  OpenThread cihaz rolü değişikliklerini işleyen callback fonksiyonu.
 *
 * Bu fonksiyon, OpenThread state değişikliklerini dinler ve özellikle
 * cihaz rolündeki değişiklikleri loglar.
 *
 * @param flags    State değişiklik bayrakları (OpenThread flags).
 * @param context  Callback context, otInstance pointer.
 *
 * @note
 * - Sadece OT_CHANGED_THREAD_ROLE bayrağı işlenir.
 * - Rol değişiklikleri log seviyesinde bilgi mesajı olarak gösterilir.
 * - Rol türleri: DISABLED, DETACHED, CHILD, ROUTER, LEADER, UNKNOWN.
 */
static void state_changed_callback(uint32_t flags, void *context)
{
    otInstance* instance = (otInstance*)context;
    if(flags & OT_CHANGED_THREAD_ROLE)
    {
        otDeviceRole role = otThreadGetDeviceRole(instance);
        switch(role)
        {
            case OT_DEVICE_ROLE_DISABLED:
                LOG_INF("Cihaz rolu: DISABLED");
                break;
            case OT_DEVICE_ROLE_DETACHED:
                LOG_INF("Cihaz rolu: DETACHED");
                break;
            case OT_DEVICE_ROLE_CHILD:
                LOG_INF("Cihaz rolu: CHILD");
                break;
            case OT_DEVICE_ROLE_ROUTER:
                LOG_INF("Cihaz rolu: ROUTER");
                break;
            case OT_DEVICE_ROLE_LEADER:
                LOG_INF("Cihaz rolu: LEADER");
                break;
            default:
                LOG_INF("Cihaz rolu: UNKNOWN");
                break;
        }
    }   
}

/**
 * @brief  OpenThread komşu tablosu (neighbor table) değişikliklerini işleyen callback fonksiyonu.
 *
 * Bu fonksiyon, komşu cihazların eklenmesi, kaldırılması veya mod değişikliklerini
 * dinler ve uygun log mesajını yazdırır.
 *
 * @param aEvent      Komşu tablosunda gerçekleşen olay tipi (OT_NEIGHBOR_TABLE_EVENT_*).
 * @param aEntryInfo  Olayla ilgili detaylı bilgi (çocuk veya router bilgisi).
 * @param aContext    Callback context (kullanılmıyor).
 *
 * @note
 * - Child ve Router cihazlar için ayrı loglar yazılır.
 * - Bilinmeyen event tipleri için uyarı logu verilir.
 */
static void neighbor_callback(otNeighborTableEvent event_type, 
                           const otNeighborTableEntryInfo *entry_info, 
                           void *context)
{
    OT_UNUSED_VARIABLE(context);
    
    switch(event_type) {
        case OT_NEIGHBOR_TABLE_EVENT_CHILD_ADDED:
            LOG_INF("Child eklendi - RLOC16: 0x%04x", entry_info->mInfo.mChild.mRloc16);
            break;
            
        case OT_NEIGHBOR_TABLE_EVENT_CHILD_REMOVED:
            LOG_INF("Child kaldirildi - RLOC16: 0x%04x", entry_info->mInfo.mChild.mRloc16);
            break;
            
        case OT_NEIGHBOR_TABLE_EVENT_CHILD_MODE_CHANGED:
            LOG_INF("Child modu degisti - RLOC16: 0x%04x", entry_info->mInfo.mChild.mRloc16);
            break;
            
        case OT_NEIGHBOR_TABLE_EVENT_ROUTER_ADDED:
            LOG_INF("Router eklendi - RLOC16: 0x%04x", entry_info->mInfo.mRouter.mRloc16);
            break;
            
        case OT_NEIGHBOR_TABLE_EVENT_ROUTER_REMOVED:
            LOG_INF("Router kaldirildi - RLOC16: 0x%04x", entry_info->mInfo.mRouter.mRloc16);
            break;
            
        default:
            LOG_WRN("Bilinmeyen neighbor olayi: %d", event_type);
            break;
    }
}



/**
 * @brief  IPv6 adres değişikliklerini işleyen OpenThread callback fonksiyonu.
 *
 * Bu fonksiyon, IPv6 adresinin eklenmesi veya kaldırılmasını loglar.
 *
 * @param aAddressInfo  IPv6 adres bilgisi (otIp6AddressInfo pointer).
 * @param aIsAdded      Adres eklendi mi? (true = eklendi, false = kaldırıldı)
 * @param aContext      Callback context, otInstance pointer.
 *
 * @note
 * - Adres ekleme/kaldırma durumuna göre LOG_INF mesajı yazılır.
 * - Adres string formatına çevrilir ve prefix, scope, preferred bilgileri loglanır.
 */
static void ip6_address_callback(const otIp6AddressInfo *address_info,
                               bool is_added,
                               void *context)
{
    char addr_str[OT_IP6_ADDRESS_STRING_SIZE];
    otIp6AddressToString(address_info->mAddress, addr_str, sizeof(addr_str));

    if (is_added)
    {
        LOG_INF("IPv6 adres eklendi: %s, prefix: %d, scope: %d, preferred: %d",
                addr_str, address_info->mPrefixLength, address_info->mScope, address_info->mPreferred);
    }
    else
    {
        LOG_INF("IPv6 adres kaldirildi: %s, prefix: %d, scope: %d, preferred: %d",
                addr_str, address_info->mPrefixLength, address_info->mScope, address_info->mPreferred);
    }

    OT_UNUSED_VARIABLE(context);
}

/**
 * @brief  OpenThread mesaj işleme thread fonksiyonu.
 *
 * Bu thread, CoAP mesajlarını message queue'dan alır, loglar ve
 * istenirse multicast gönderimi için hazırlar.
 *
 * @param arg1  Kullanılmıyor.
 * @param arg2  Kullanılmıyor.
 * @param arg3  Kullanılmıyor.
 *
 * @note
 * - Mesaj queue dolu ise thread K_FOREVER ile bekler.
 * - Gelen mesaj bilgisi loglanır: kaynak ve hedef IPv6 adresleri, port ve mesaj uzunluğu.
 * - Mesaj işlendikten sonra buffer sıfırlanır.
 */
void openthread_event_thread(void* arg1,void* arg2,void* arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);
    openthread_message_info_t msg_info = {0};
    while(true)
    {
        k_msgq_get(&thread_msgq, &msg_info, K_FOREVER);
        char peer_addr_str[OT_IP6_ADDRESS_STRING_SIZE];
        char sock_addr_str[OT_IP6_ADDRESS_STRING_SIZE];
        otIp6AddressToString(&msg_info.message_info.mPeerAddr, peer_addr_str, sizeof(peer_addr_str));
        otIp6AddressToString(&msg_info.message_info.mSockAddr, sock_addr_str, sizeof(sock_addr_str)); 
        LOG_INF("CoAP istegi alindi");
        LOG_INF("Kaynak: %s: PORT: %d", peer_addr_str, msg_info.message_info.mPeerPort);
        LOG_INF("Hedef:  %s: PORT: %d", sock_addr_str, msg_info.message_info.mSockPort);
        LOG_INF("Mesaj: %.*s", msg_info.len, msg_info.message);

        memset(&msg_info, 0, sizeof(msg_info));

 
    }
}

K_THREAD_DEFINE(openthread_event_thread_id, OPENTHREAD_EVENTS_THREAD_STACK_SIZE, openthread_event_thread, NULL, NULL, NULL, OPENTHREAD_EVENTS_THREAD_PRIORITY, 0, 0);



