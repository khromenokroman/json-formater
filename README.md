# JsonFormater

````bash
apt install -y build-essential cmake libfmt-dev nlohmann-json3-dev dpkg-dev libcpp-httplib-dev
````

## Сборка из исходников

```bash 
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr ..
cmake --build . -j$(nproc)
cmake --install .
```

## Сборка DEB

Пакет собирается через CPack:

```bash 
cd build cpack -G DEB
apt install -y ./json-formater_<версия>_amd64.deb
```

После установки будут размещены:

- бинарник: `/usr/bin/json-formater`
- конфиг(Настройки): `/etc/json-formater/cfg.json`
- unit-файл systemd: `/usr/lib/systemd/system/json-formater.service`

### Параметры(cfg.json)

- `port` — порт, на котором запускается HTTP-сервер
- `log_level` — уровень логирования для `syslog`

Возможные уровнилогирования:

- `LOG_EMERG` — 0
- `LOG_ALERT` — 1
- `LOG_CRIT` — 2
- `LOG_ERR` — 3
- `LOG_WARNING` — 4
- `LOG_NOTICE` — 5
- `LOG_INFO` — 6
- `LOG_DEBUG` — 7

## Запуск

После установки сервис можно запускать так:

```bash 
systemctl daemon-reload
systemctl enable json-formater
systemctl start json-formater
```

Проверка статуса:

```bash 
systemctl status json-formater
```

По умолчанию сервер доступен на: http://localhost:8080
