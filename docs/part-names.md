# Названия текущей модели

| Название | Исходник в cad/ | Экземпляр в механизме |
| --- | --- | --- |
| Основание | parts/base/base.FCStd | Base |
| Ножка | Цилиндрическая часть Base, на которую надет PanCarrier | Часть Base |
| Крепление нижнего серво | parts/pan-carrier/pan-carrier.FCStd | PanCarrier |
| Боковина со стороны качалки | parts/servo-side/servo-side.FCStd | ServoSide |
| Боковина с выступом | parts/shaft-side/shaft-side.FCStd | ShaftSide |
| Площадка наклона | parts/tilt-platform/tilt-platform.FCStd | TiltPlatform |
| Вставка в паз / качалка наклона | parts/tilt-horn/tilt-horn.FCStd | ServoSideInsert |
| Круглая качалка | parts/pan-horn/pan-horn.FCStd | BaseRoundHorn |
| Нижний серво / серво №1 | components/mg996r/mg996r.FCStd | PanMG996R |
| Верхний серво / серво №2 | Тот же источник MG996R | TiltMG996R |
| Держатель телефона | parts/phone-holder/phone-holder.FCStd | Holder в подсборке держателя |
| Телефон — габарит | components/phone/phone.FCStd | Phone в подсборке держателя |

Внутри tilt-platform.FCStd четыре элемента: PlatformPlate, ServoMountTall,
ServoMountShort и AxleSupport. Они составляют одну деталь и сохранены в принятых
относительных положениях. Скрытых альтернатив площадки в общей сборке больше нет.

Основание — вся Base с подошвой и ножкой. Ножка — только цилиндр.
Слова «вал» и «выступ ShaftSide» обозначают разные элементы.
Текущий фасад к телефону направлен по +X, вверх +Z, ось наклона вдоль Y.
Старые названия деталей прежнего макета в текущей модели не используются.
