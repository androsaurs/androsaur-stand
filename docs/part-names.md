# Названия деталей

Имена — ключи словаря `assemble()` ([pantilt.py](../cadpy/pantilt.py)); те же имена в просмотрщике и в STEP.

| Имя в модели | Что это | Файл |
| --- | --- | --- |
| `base` | Основание: подошва 85 × 85 и ножка (цилиндр с полостью для проводов) | pantilt.py |
| `carrier` | Лоток нижнего серво, надет на ножку | pantilt.py |
| `servo_pan` | Нижнее серво MG996R (габарит) | pantilt.py |
| `plate_horn` | Боковина со стороны качалки | pantilt.py |
| `plate_shaft` | Боковина с выступом | pantilt.py |
| `servo_tilt` | Верхнее серво MG996R (габарит) | pantilt.py |
| `platform` | Площадка наклона с держателем | pantilt.py |
| `body` | Спинка держателя (два выступа-направляющие) | clip_holder.py |
| `grip` | Захват под телефон 76,3 мм (в названии владельца раньше «82 мм»), U-образная скоба | clip_holder.py |
| `phone` | Телефон Redmi 12 без чехла, габарит 168,6 × 76,3 × 9,0 | clip_holder.py |
| `plug`, `cable` | Штекер Type-C и жёсткий макет кабеля | clip_holder.py |
| `box_lower`, `box_upper` | Нижняя и верхняя половины корпуса | enclosure.py |
| `shelf` | Полка внутри нижней половины | enclosure.py |
| `foot0`…`foot3` | Ножки корпуса | enclosure.py |
| `speaker` | Динамик FD92-1 (габарит) | speaker.py |
| `esp32` | ESP32-S3 DevKitC-1 (габарит) | esp32.py |
| `amp` | Усилитель TPA3118 (габарит) | amp.py |
| `dac` | ЦАП PCM5102A (габарит) | amp.py |
| `psu` | Блок питания, куб-заглушка 60 × 38 × 26 | amp.py |

«Ножка» (подставки) — цилиндр `base`, на который надет `carrier`. Не путать с «ножками корпуса» (`foot0`…`foot3`) и
«ножками полки» (три стойки полки). Прежние названия во FreeCAD (`PanCarrier`, `ServoSide`, `ShaftSide`, `TiltPlatform` и др.) остались только в истории git.
