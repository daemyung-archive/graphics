# 메탈 스키닝 예제

메탈을 이용해서 [스키닝](https://github.com/daemyung/graphics/tree/master/skinning)을 구현합니다.

## 요구사항

* [CMake](https://github.com/Kitware/CMake)
* [Vcpkg](https://github.com/Microsoft/vcpkg)

## 빌드

### 디펜던시 설치

```
vcpkg install spdlog
vcpkg install glfw3
vcpkg install nlohmann_json
vcpkg install tinygltf
```

### 프로젝트 생성

```
mkdir build
cmake . -B build -G Xcode -DCMAKE_TOOLCHAIN_FILE=${VCPKG_DIR}/script/buildsystem/vcpkg.cmake
```