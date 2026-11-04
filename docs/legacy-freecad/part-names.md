# Названия текущей модели

| Название | Исходник в cad/ | Экземпляр в механизме |
| --- | --- | --- |
| Основание | parts/base/base.FCStd | Base |
| Ножка | Цилиндрическая часть Base, на которую надет PanCarrier | Часть Base |
| Крепление нижнего серво | parts/pan-carrier-brep/pan-carrier-brep.FCStd | PanCarrier |
| Боковина со стороны качалки | parts/servo-side-brep/servo-side-brep.FCStd | ServoSide |
| Боковина с выступом | parts/shaft-side-brep/shaft-side-brep.FCStd | ShaftSide |
| Площадка наклона | parts/tilt-platform-brep/tilt-platform-brep.FCStd | TiltPlatform |
| Вставка в паз / качалка наклона | parts/tilt-horn/tilt-horn.FCStd | ServoSideInsert |
| Круглая качалка | parts/pan-horn/pan-horn.FCStd | BaseRoundHorn |
| Нижний серво / серво №1 | components/mg996r-brep/mg996r-brep.FCStd | PanMG996R |
| Верхний серво / серво №2 | Тот же источник MG996R (BRep) | TiltMG996R |
| Держатель телефона | parts/phone-holder/phone-holder.FCStd | Holder в подсборке держателя |
| Телефон — габарит | components/phone/phone.FCStd | Phone в подсборке держателя |

Блоки крепления серво в tilt-platform-brep вместе с рёбрами и отверстиями сдвинуты на 1,3151 мм
вдоль оси Y платформы (параметр `SERVO_MOUNT_SHIFT_Y` в scripts/build-brep-parts.FCMacro).
Исходная сетка MG996R — components/mg996r/mg996r.FCStd, она остаётся источником и эталоном для BRep-габарита.

Основание — вся Base с подошвой и ножкой. Ножка — только цилиндр.
Слова «вал» и «выступ ShaftSide» обозначают разные элементы.
Текущий фасад к телефону направлен по +X, вверх +Z, ось наклона вдоль Y.
Старые названия деталей прежнего макета в текущей модели не используются.

Отдельно собран переданный владельцем держатель из двух деталей:
`cad/assemblies/phone-holder-two-piece.FCStd` (ещё не установлен в stand).
`Body` — спинка с нижним упором, источник `parts/phone-holder-body/`;
`Grip82mm` / `82mm` — поперечный захват, источник `parts/phone-grip-82mm/`.
Обе детали — BRep, построенные из исходных STL скриптом `scripts/build-phone-holder-brep.FCMacro`
(с 2026-11-02; прежние сетки и правки `Body` — убранный крюк и полка, отверстия — заменены, они в Git, коммит 709eb6d).
