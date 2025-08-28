## OPENTHREAD_SERVER

Bu proje, Zephyr RTOS uzerinde nRF52840 ile OpenThread FTD ve CoAP sunucusu calistirir. CoAP isteklerini karsilar, mesajlari kuyruklar ve loglar.

OPENTHREAD_CLIENT ile birlikte client-server demo sistemi oluşturur.

### Ornek Log Ciktisi (Gelen CoAP Mesaji)
```text
[00:01:27.178,436] <inf> openthread_events: CoAP istegi alindi
[00:01:27.178,466] <inf> openthread_events: Kaynak: fdde:ad00:beef:0:7a70:6363:7820:cea1: PORT: 5683
[00:01:27.178,527] <inf> openthread_events: Hedef:  fdde:ad00:beef:0:9da4:4886:f1a9:824e: PORT: 5683
[00:01:27.178,558] <inf> openthread_events: Mesaj: Merhaba OpenThread
```


