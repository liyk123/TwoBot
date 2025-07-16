![TwoBot](https://socialify.git.ci/liyk123/TwoBot/image?description=1&issues=1&language=1&name=1&owner=1&stargazers=1&theme=Auto)

## Build:
* vcpkg
  - 请参阅[官方文档](https://github.com/microsoft/vcpkg)配置vcpkg，并配置`VCPKG_ROOT`的环境变量为vcpkg根目录
  + bash
    ```shell
    git clone https://github.com/liyk123/TwoBot.git
    cd TwoBot
    cmake --preset <Debug/Release>
    cmake --build --preset <Debug/Release>
    ```
  + Windows
    - 直接使用Visual Studio 2022打开TwoBot目录, 等待初始化完成单击菜单栏: 生成->全部生成

## Import:
* vcpkg
  - 目前可使用[liyk123/vcpkg](https://github.com/liyk123/vcpkg)的`pcrbotpp`分支作为vcpkg的仓库，需及时关注最新的提交
* cmake
  - 理论上可作为子模块导入到项目，未经测试。

## TODO:
+ [x] Onebot-11
    - [x] 正向http API
    - [x] 反向WS
+ [x] 现代C++特性
    - [x] 异步事件处理
    - [x] 0成本抽象
    - [x] json序列化
+ [ ] 集成vcpkg
    - [x] 引入第三方模块
    - [x] 自身模块化
    - [ ] 添加到微软的port中
+ [ ] 完善文档

## 鸣谢
- [MIT] [TwoBotFramework/TwoBot](https://github.com/TwoBotFramework/TwoBot)
- [MIT] [Onebot-11 标准文档](https://github.com/botuniverse/onebot-11)
- [MIT] [yhirose/cpp-httplib](https://github.com/yhirose/cpp-httplib/)
- [MIT] [nlohmann/json](https://github.com/nlohmann/json)
- [MIT] [IronsDu/brynet](https://github.com/IronsDu/brynet)
- [Apache-2.0] [uxlfoundation/oneTBB](https://github.com/uxlfoundation/oneTBB)
- [MIT] [bshoshany/thread-pool](https://github.com/bshoshany/thread-pool)
- [MIT] [microsoft/vcpkg](https://github.com/microsoft/vcpkg)
