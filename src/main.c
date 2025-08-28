#include <zephyr/kernel.h>

/**
 * @brief Uygulama giris noktasi.
 *
 * Zephyr tarafindan `SYS_INIT` ile kurulan altyapi ve OpenThread/CoAP
 * ilklendirmeleri calistiktan sonra ana thread burada calisir.
 * Ornek uygulamada ek bir is yapilmamaktadir.
 *
 * @return 0 basarili, diger durumlar icin hata kodu.
 */
int main(void)
{
        return 0;
}
