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