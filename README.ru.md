# pz-arm64

**Project Zomboid (Build 41 и Build 42) нативно на aarch64-устройствах (консоли, SBC)** – инструмент совместимости
для Steam, который запускает игру на *нативной arm64 Java VM* и эмулирует только несколько x86-64 библиотек,
которые идут вместе с игрой.

[English version](README.md)

## В чём идея
Игра написана на Java, но в Linux-версии есть только x86-64 нативные библиотеки (освещение, популяция зомби,
поиск пути, физика, звук FMOD, сеть Steam …). Эмуляция *всей* игры (FEX / Box64) работает медленно и нестабильно.
pz-arm64 делает иначе:

* Java-код игры исполняется на **нативной arm64 JVM** (Temurin 17 для B41, 25 для B42) с **arm64-версией LWJGL** и
  системным **Mesa**;
* x86-64 `.so`-библиотеки игры загружаются **внутрь процесса JVM через [Box64](https://github.com/ptitSeb/box64) в режиме
  библиотеки**, а тонкая прослойка пробрасывает каждый JNI-вызов в эмулируемый код (идея и JNI-слой –
  из [Zomdroid](https://github.com/Not-a-dude/zomdroid));
* все привязки **генерируются при запуске из файлов самой игры** – **папка игры не изменяется**, обновления Steam и
  проверка целостности продолжают работать.

Работает: меню, одиночная игра, освещение, зомби, транспорт, физика, звук FMOD, Steam (мастерская, список серверов) и
сетевая игра – с хорошим FPS, и в Build 41, и в Build 42.

**Проверено на:** Retroid Pocket 6 (Snapdragon 8 Gen 2, Adreno 740, Mesa freedreno) под управлением
[Armada OS](https://github.com/armada-os/armada) (SteamOS-подобный дистрибутив для ARM-консолей). На других
aarch64-устройствах должно работать в принципе, но не проверялось.

## Требования
* Linux aarch64 с установленным Steam, рабочим OpenGL-драйвером (Mesa / freedreno, panfrost …) и графической сессией
* Project Zomboid, установленный через Steam (обычная Linux-версия, не Windows/Proton)
* `python3`, `curl`, `tar`, `sha256sum`; glibc ≥ 2.35
* ~400 МБ на диске, ~3 ГБ свободной RAM для игры

## Установка
**Предпочтительный способ – скачать готовый релиз** на [странице релизов GitHub](https://github.com/EvilToasterDBU/pz-arm64/releases)
(`pz-arm64-linux-aarch64.tar.gz`), распаковать и запустить установщик:
```bash
tar xzf pz-arm64-linux-aarch64.tar.gz && cd pz-arm64 && ./install.sh
```
Параметры: `--steam-dir DIR`, `--no-b41`, `--no-b42`. Альтернатива одной строкой (сама скачает последний релиз):
```bash
curl -fsSL https://github.com/EvilToasterDBU/pz-arm64/releases/latest/download/install.sh | PZ_ARM64_REPO=EvilToasterDBU/pz-arm64 bash
```
Сборка из исходников возможна, но для обычного использования не нужна (см. ниже).

Затем перезапустите Steam и в *Project Zomboid → Свойства → Совместимость* выберите
**Project Zomboid (arm64 native)**. Установщик скачивает Eclipse Temurin и нативные библиотеки LWJGL arm64 с официальных
источников (проверяет контрольные суммы); файлы игры не скачиваются и не распространяются.

Удаление: `./uninstall.sh`.

## Настройки
`compatibilitytools.d/pz-arm64/pz-arm64.json` (всё необязательно): `sound`, `steam`, `voice`, `xmx`, `jvm_flags`,
`game_args`, `box64_env`, `hide_touchscreens` – описание в [английском README](README.md#options).

Логи: `~/Zomboid/pz-arm64.log` (лаунчер) и `~/Zomboid/projectzomboid.sh.log` (игра и мост).

## Сборка и устройство
[docs/BUILDING.md](docs/BUILDING.md), [docs/HOW-IT-WORKS.md](docs/HOW-IT-WORKS.md), [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md).

## Ограничения
* Проверено только на одном устройстве (Retroid Pocket 6 / Armada OS); отчёты о других SoC / GPU / дистрибутивах очень приветствуются.
* Build 41: тапы по сенсорному экрану не срабатывали на экране Terms of Service при первом запуске (курсор двигался, клик не проходил); геймпад и виртуальная мышь Steam Input работают. В меню Build 42 касания работают.
* Голосовой чат отключён. Новые сборки игры могут потребовать правок: таблицы привязок генерируются автоматически,
  но обновление всё равно способно что-то сломать.
* Проект не связан с The Indie Stone, Valve, Firelight (FMOD), Zomdroid и Box64.

## Благодарности
Проект опирается на чужую работу:

* **[Zomdroid](https://github.com/Not-a-dude/zomdroid)** (liamelui, Not-a-dude и участники) – идея запуска Box64 как
  библиотеки внутри JVM и JNI-слой, от которого происходит наш мост (MIT).
* **[Box64](https://github.com/ptitSeb/box64)** – ptitSeb и участники: эмулятор x86-64, исполняющий нативные библиотеки
  игры; используется через форк [zomdroid-box64](https://github.com/Not-a-dude/zomdroid-box64) (MIT).
* **[Assimp](https://github.com/assimp/assimp)** – загрузка моделей (из него собирается `libjassimp64.so`, BSD-3).
* **[LWJGL](https://www.lwjgl.org)** – arm64-привязки (OpenGL, GLFW, stb, jemalloc) (BSD-3).
* **[Eclipse Temurin](https://adoptium.net)** (Adoptium) – arm64-среды Java (GPLv2 + Classpath Exception).
* **[Mesa](https://www.mesa3d.org)** – OpenGL-драйвер (freedreno), на котором рисует игра.
* **[Armada OS](https://github.com/armada-os/armada)** – платформа, на которой проект разрабатывался и проверялся.
* **The Indie Stone** – сама Project Zomboid. Нужна собственная копия игры.

Полная информация о лицензиях: [NOTICE](NOTICE) и [licenses/](licenses/).

## Лицензия
MIT для кода проекта ([LICENSE](LICENSE)); сторонние компоненты – в [NOTICE](NOTICE).
