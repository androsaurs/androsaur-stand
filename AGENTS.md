# Androsaur Stand: модель на build123d

Главная модель — код в `cadpy/` (Python, build123d, BRep). Прежняя модель во FreeCAD и её макросы удалены из рабочего дерева
(владелец отказался от FreeCAD 2026-11-04); они остались только в истории git. Dispatch в текущей сессии запрещён владельцем.

Перед работой прочитать README.md, docs/design.md, docs/enclosure.md, docs/part-names.md, docs/interfaces.md,
docs/model-guide.md и docs/open-issues.md. Требования — docs/requirements.md.

Единственный источник геометрии — код в `cadpy/` (`pantilt.py`, `clip_holder.py`, `enclosure.py`, `speaker.py`, `esp32.py`,
`amp.py`); STEP в `exports/` производный. Просмотр: `cadpy/viewer.sh`; проверка пересечений: `cadpy/check.py -2 45 90 100`;
экспорт для FreeCAD: `cadpy/export.py`; картинки для документации: `cadpy/shots.py`.

При просьбе запустить вьювер обязательно прочитать `docs/viewer.md`.
Запускать `cadpy/viewer.sh`: он сохраняет модель для позднего открытия и обновления вкладок.
Проверку отображения в браузере агент не делает: после запуска сообщить владельцу адрес
`http://127.0.0.1:3939/viewer`, владелец проверяет вкладку сам.

Владелец требует строго выполнять указанные действия. Не менять конструкцию, геометрию, оси или размещения соседних деталей
без разрешения, следующего из задачи. При перемещении по одной оси сохранять остальные координаты.

При нарезке brim (брим, кайма) по умолчанию выключен: `--set brim_type=no_brim --set brim_width=0`.
Добавлять кайму можно только после явного согласования владельца. Предлагать её только для маленьких,
тонких или неустойчивых деталей с малой опорой на стол. Для широких устойчивых деталей кайму не предлагать.
После нарезки проверять отсутствие `;TYPE:Brim` в G-code; `auto_brim` без согласования не использовать.

Изменение геометрии требует проверок по существу: пересечения, зазоры, посадки, движение, изготовление (печать без поддержек).
Сообщать численные результаты. Архивные источники в `references/` не редактировать. Размеры и допущения записывать
(в коде и в документах); новые названия — в docs/part-names.md, состав — docs/bom.csv.

История состояния до перехода на build123d: коммит 9d254f3 в истории git. Старые файлы FreeCAD в рабочее дерево
не возвращать без отдельной задачи.

## Прошивка: запуск ESP-IDF

Проверенное окружение — ESP-IDF 5.5, Python 3.10.12. Перед запуском `idf.py` в каждой новой shell-сессии
явно выбрать существующее Python-окружение и активировать ESP-IDF:

```bash
export IDF_PYTHON_ENV_PATH=/home/hram/.espressif/python_env/idf5.5_py3.10_env
source /home/hram/esp/esp-idf/export.sh
cd /mnt/fastdata/hram/projects/androsaurs/androsaur-stand/firmware
python /home/hram/esp/esp-idf/tools/idf.py \
  -B build-ble -DSDKCONFIG=sdkconfig.ble.local.config build
```

Прошивка подключённой ESP32-S3 (когда запись на плату входит в задачу):

```bash
python /home/hram/esp/esp-idf/tools/idf.py \
  -B build-ble -DSDKCONFIG=sdkconfig.ble.local.config \
  -p /dev/ttyACM0 -b 921600 flash
```

Использовать `build-ble` и `sdkconfig.ble.local.config`: старый sdkconfig может выключить BLE.
Если ESP-IDF ищет отсутствующее окружение для системного Python (например, `idf5.5_py3.14_env`),
сначала выполнить активацию выше и проверить `python --version` и `idf.py --version`.
Это само по себе не означает, что рабочее окружение отсутствует; не переустанавливать его по такой ошибке.
