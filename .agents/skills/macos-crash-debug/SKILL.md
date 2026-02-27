---
name: macos-crash-debug
description: 在 macOS 上对 C++/native 二进制启动崩溃进行系统性定位。适用于程序运行时出现 SIGABRT/SIGSEGV/EXC_BAD_ACCESS、退出码异常（134/139）、或者启动即崩溃且无任何日志输出的场景。包含插桩、对照复现、lldb 回溯、静态初始化定位等完整工作流。
---

# macOS 崩溃分析工作流（C++ Native）

## 快速判断：先确定崩溃阶段

```
main() 之前 → 静态/全局初始化阶段
main() 之后、业务执行前 → 参数解析 / 单例首次访问
业务执行中 → 具体功能链路
```

崩溃信号参考：

| 信号 / 退出码 | 典型原因 |
|---|---|
| SIGABRT (134) | `abort()`、`malloc` 双重释放、`std::terminate` |
| SIGSEGV / EXC_BAD_ACCESS (139) | 空指针/野指针、越界访问 |
| 无输出直接退出 | 静态初始化期崩溃（日志模块本身尚未就绪） |

---

## 第一步：对照复现（最快排除假设）

先在不同场景各运行一次，快速缩小范围：

```bash
# 场景 A：正常工作目录
./bin/your-binary

# 场景 B：空目录（排除工作目录依赖）
mkdir /tmp/empty-test && cd /tmp/empty-test && /path/to/binary

# 场景 C：仓库根目录（有完整配置文件）
cd /path/to/repo && ./bin/your-binary
```

若所有场景都崩溃 → 与工作目录/配置无关，转向静态初始化。  
若仅特定目录崩溃 → 重点排查配置文件加载、路径计算。

---

## 第二步：lldb 抓崩溃回溯

```bash
lldb --batch -o run -k "bt all" -k quit -- ./bin/your-binary
```

关键看：

1. **栈顶模块名** — 是系统库（`libc++`/`libsystem`）还是你自己的代码
2. **是否出现 `_ZGIW...` 符号** — 这是 C++ 模块初始化器，说明崩在静态初始化阶段
3. **报错文本** — `malloc: pointer being freed was not allocated` → 双重释放；`EXC_BAD_ACCESS` → 内存访问越界

对照模块初始化符号表排查顺序：

```bash
nm -n ./bin/your-binary | awk '/_ZGIW/ {print}'
```

---

## 第三步：定位静态初始化根因

重点排查以下模式，**任意一种都可能引发启动期崩溃**：

### 3a. 全局静态对象过早初始化

```cpp
// 危险：模块加载时立即执行，此时依赖模块可能未就绪
static auto btools = load_buildtools();  // ← 高风险
```

修复方式：改为函数内静态（首次调用时才初始化）：

```cpp
auto& get_buildtools() {
    static auto btools = load_buildtools();
    return btools;
}
```

### 3b. 循环模块依赖

```
模块 A → import 模块 B
模块 B → import 模块 A   ← 循环，编译期报错
```

修复：把共用的基础能力（如日志）单独提取为叶子模块，不再依赖任何业务模块。

### 3c. 配置文件路径依赖工作目录

如果代码用 `std::filesystem::current_path()` 在静态初始化期捕获工作目录，
在不同目录启动时可能访问不存在的路径。

---

## 第四步：代码插桩（精准追踪崩溃前最后步骤）

在关键节点加日志，确保**崩溃前最后一条日志**可见。优先插桩位置：

```
1. 全局/静态初始化入口
2. 配置文件 exists() 检查前后
3. 文件 open()/parse() 前后（分类捕获异常）
4. 外部命令执行（popen 返回值检查）
```

异常捕获要分类（不要用裸 `catch(...)`）：

```cpp
try {
    auto json = nlohmann::json::parse(std::ifstream(path));
    // ...
} catch (const nlohmann::json::parse_error& e) {
    log::error("JSON 解析失败: {}", e.what());
} catch (const std::filesystem::filesystem_error& e) {
    log::error("文件系统错误: {}", e.what());
} catch (const std::exception& e) {
    log::error("读取配置异常: {}", e.what());
} catch (...) {
    log::error("未知异常");
}
```

**注意**：`catch(...)` 静默吞掉异常（不打印 `what()`），会让配置加载失败在日志中完全不可见。

---

## 第五步：验证修复与稳定性测试

修复后，连续多次运行确认无随机性崩溃：

```bash
for i in $(seq 1 10); do
    ./bin/your-binary >/dev/null 2>&1
    code=$?
    echo "run$i=$code"
    [ $code -ne 0 ] && { echo "FAILED"; exit 1; }
done
echo "All stable"
```

---

## 常见陷阱速查

| 陷阱 | 现象 | 解法 |
|---|---|---|
| 全局静态对象过早初始化 | 无日志输出直接崩溃 | 改为函数内静态延迟初始化 |
| 模块循环依赖 | 编译报 `circular dependency` | 提取叶子模块 |
| `catch(...)` 静默吞异常 | 配置加载失败无法感知 | 分类捕获并打印 `what()` |
| 日志模块依赖平台模块反向依赖日志 | 循环依赖编译失败 | 日志模块只依赖标准库 |
| Release 符号全被剥离 | `lldb` 只看到匿名符号 | 用 `nm -n` 对照初始化器顺序 |
| 配置文件路径依赖 cwd | 不同目录运行行为不一致 | 区分"启动时工作目录"与"当前目录" |

---

## 参考资料

- [LLDB 官方文档](https://lldb.llvm.org/use/tutorial.html)
- [C++ 静态初始化顺序问题（Static Initialization Order Fiasco）](https://en.cppreference.com/w/cpp/language/initialization)
- [C++ 模块（Modules）依赖规则 - cppreference](https://en.cppreference.com/w/cpp/language/modules)
- [nlohmann/json 异常文档](https://json.nlohmann.me/home/exceptions/)
- [本项目实际分析报告](docs/crash-analysis-d2x-in-d2mcpp.md)
