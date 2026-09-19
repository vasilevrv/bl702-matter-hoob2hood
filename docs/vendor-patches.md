# Изменения вендорского кода

Репозиторий не хранит полную копию Matter или Bouffalo SDK. `bootstrap.sh` загружает точные upstream-ревизии и применяет два небольших patch-файла.

## `patches/connectedhomeip.patch`

### `examples/platform/bouffalolab/common/plat/platform.cpp`

Код OTA обёрнут в `CHIP_DEVICE_CONFIG_ENABLE_OTA_REQUESTOR`. В этой прошивке OTA отключён, поэтому ненужный requestor не создаётся и не занимает RAM.

### `src/app/clusters/network-commissioning/ThreadScanResponse.cpp`

Временный массив результатов Thread scan перенесён с heap на стек обработчика. Это освобождает дефицитный heap BL702 во время commissioning и устраняет падение на `AddNOC` с `Memory Allocate Failed`.

### `src/platform/bouffalolab/common/BflbConfig_littlefs.cpp`

Добавлена проверка ошибок открытия LittleFS/config storage. Без неё неудачная инициализация PSM могла привести к обращению через недействительный объект и аппаратному исключению.

## `patches/bouffalolab-components.patch`

### `network/thread/openthread_port/ot_sys_iot.c`

Проверка `CFG_USE_PSRAM` сделана корректной и при неопределённом macro. Это позволяет собирать конфигурацию XT-ZB2 без PSRAM без глобального подавления предупреждений препроцессора.

### `platform/hosal/bl702_hal/bl_flash.c` и `bl_flash.h`

Добавлен API регистрации разрешённого диапазона flash. Boot2 защищает области firmware/partition table, но PSM Matter должен оставаться доступен LittleFS. Диапазон берётся из MTD во время выполнения — адрес `0x1EA000` в код не зашит.

### `stage/littlefs/port/lfs_xip_flash.c`

- PSM-регион из MTD регистрируется как разрешённый для записи;
- операции erase/write на BL702 выполняются через варианты `*_need_lock` при отключённых прерываниях;
- возвращается реальный код ошибки flash.

Это исправляет первоначальный отказ форматирования LittleFS (`format ret=-1`) и делает хранение fabric/credentials работоспособным.

## Что больше не патчится

В старом рабочем дереве также менялись глобальный список build-target’ов, Python builder, flashing helper, общий linker script и отладочный boot banner. Для standalone-проекта они не нужны:

- GN вызывается напрямую из `scripts/build.sh`;
- linker script лежит рядом с приложением;
- прошивка выполняется `scripts/flash.sh` с явными параметрами;
- временные диагностические сообщения LittleFS удалены.

