当使用 json 库时，你可以按照以下步骤使用它来处理 JSON 数据：

### 步骤 1：包含头文件
首先，在你的 C++ 代码中包含 JSON 库的头文件。

```cpp
#include "json.hpp"
```

### 步骤 2：创建和操作 JSON 对象
使用 json 库，你可以创建、操作和处理 JSON 对象。

#### 创建 JSON 对象
```cpp
// 创建一个空的 JSON 对象
json empty_json;

// 创建一个带有初始值的 JSON 对象
json person = {
    {"name", "John"},
    {"age", 30},
    {"isStudent", true}
};
```

#### 向 JSON 对象添加内容
```cpp
// 向已有 JSON 对象中添加新的键值对
person["city"] = "New York";
person["grades"] = {95, 87, 92};
```

#### 访问和修改 JSON 对象的值
```cpp
// 获取 JSON 对象中的值
std::string name = person["name"]; // 获取名为 "name" 的值

// 修改 JSON 对象中的值
person["age"] = 31; // 修改 "age" 的值为 31
```

#### 检查 JSON 对象中的键是否存在
```cpp
// 检查 JSON 对象中是否存在某个键
if (person.find("city") != person.end()) {
    // 存在键 "city"
}
```

#### 将 JSON 对象转换为字符串
```cpp
// 将 JSON 对象转换为字符串
std::string person_str = person.dump(); // 将 JSON 对象转换为字符串
```

#### 从字符串中解析 JSON 对象
```cpp
// 从字符串解析 JSON 对象
std::string json_str = "{\"name\":\"John\",\"age\":30,\"city\":\"New York\"}";
json parsed_json = json::parse(json_str); // 解析 JSON 字符串
```

### 步骤 3：使用 JSON 对象
使用创建、操作和解析的 JSON 对象，你可以根据需要进行序列化、反序列化、添加、删除、修改键值对等操作。 JSON 库提供了丰富的方法来处理 JSON 数据。
下面结合http库一起使用的方法，请看以下示例代码：

#### 使用 `cpp-httplib` 创建简单的 HTTP 服务器

```cpp
#include "httplib.h"

int main() {
    httplib::Server svr;

    // 处理根路径请求
    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content("Hello, World!", "text/plain");
    });

    // 设置静态文件服务
    svr.set_base_dir("./www");

    // 监听端口
    svr.listen("localhost", 8080);

    return 0;
}
```

这段代码创建了一个简单的 HTTP 服务器，处理根路径 `/` 的 GET 请求并返回 "Hello, World!"，同时设置了静态文件服务。

#### 使用 `json库` 创建和操作 JSON 对象

```cpp
#include "json.hpp"
#include <iostream>

using json = nlohmann::json;

int main() {
    // 创建 JSON 对象
    json person = {
        {"name", "John"},
        {"age", 30},
        {"isStudent", true}
    };

    // 向 JSON 对象中添加内容
    person["city"] = "New York";
    person["grades"] = {95, 87, 92};

    // 访问和修改 JSON 对象的值
    std::string name = person["name"];
    person["age"] = 31;

    // 将 JSON 对象转换为字符串并输出
    std::string person_str = person.dump();
    std::cout << person_str << std::endl;

    // 从字符串解析 JSON 对象
    std::string json_str = "{\"name\":\"John\",\"age\":30,\"city\":\"New York\"}";
    json parsed_json = json::parse(json_str);

    return 0;
}
```

这个示例演示了如何创建、操作和解析 JSON 对象。它创建了一个包含个人信息的 JSON 对象，添加了新的键值对、修改了值，并演示了将 JSON 对象转换为字符串和从字符串解析为 JSON 对象的过程。