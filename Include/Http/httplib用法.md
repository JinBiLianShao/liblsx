### 步骤 1：安装 `cpp-httplib`
1. 下载 `cpp-httplib` 库或者通过包管理器安装。
2. 将 `httplib.h` 文件包含到你的 C++ 项目中。

### 步骤 2：创建 HTTP 服务器
以下是一个基本的示例代码，演示了如何创建一个简单的 HTTP 服务器并响应 GET 请求：

```cpp
#include "httplib.h"

int main() {
    httplib::Server svr;

    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content("Hello, World!", "text/plain");
    });

    svr.listen("localhost", 8080);

    return 0;
}
```

这个例子创建了一个简单的服务器，监听本地 `localhost` 的 `8080` 端口，并对根路径 `/` 的 GET 请求响应 "Hello, World!"。

### 步骤 3：处理不同的 HTTP 请求
你可以使用 `Get()`、`Post()`、`Put()`、`Delete()` 等函数来处理不同类型的 HTTP 请求。例如：

```cpp
svr.Get("/hello", [](const httplib::Request& req, httplib::Response& res) {
    res.set_content("Hello!", "text/plain");
});

svr.Post("/data", [](const httplib::Request& req, httplib::Response& res) {
    res.set_content("Received POST request!", "text/plain");
});
```

### 步骤 4：运行服务器
编译并运行你的 C++ 代码，这样你的服务器就会在指定的端口上运行了。

## 更多功能使用介绍

### 路由处理
你可以使用 `Get()`、`Post()`、`Put()`、`Delete()` 等函数来处理不同类型的 HTTP 请求，并基于不同的路径进行路由。例如：

```cpp
svr.Get("/hello", [](const httplib::Request& req, httplib::Response& res) {
    res.set_content("Hello!", "text/plain");
});

svr.Post("/data", [](const httplib::Request& req, httplib::Response& res) {
    res.set_content("Received POST request!", "text/plain");
});
```

### 文件服务
你可以使用 `set_base_dir()` 函数指定一个静态文件服务的基本路径，使得服务器可以提供静态文件（如 HTML、CSS、JS 文件等）的访问。

```cpp
svr.set_base_dir("./public");
```

### 文件上传
`cpp-httplib` 也支持文件上传功能。你可以使用 `has_file()` 和 `get_file_value()` 来检查并获取上传的文件内容。

```cpp
svr.Post("/upload", [](const httplib::Request& req, httplib::Response& res) {
    if (req.has_file("file")) {
        auto file = req.get_file_value("file");
        // 处理上传的文件
    } else {
        // 没有上传文件的处理逻辑
    }
});
```

### JSON 支持
`cpp-httplib` 也提供了简单的 JSON 支持。你可以使用 `set_content()` 将 JSON 格式的数据返回给客户端。
这里需要借助json库。

```cpp
#include "json.hpp"

using json = nlohmann::json;

svr.Get("/json", [](const httplib::Request& req, httplib::Response& res) {
    json data = {
        {"name", "John"},
        {"age", 30},
        {"city", "New York"}
    };

    res.set_content(data.dump(), "application/json");
});
```

### HTTPS 支持
`cpp-httplib` 也支持 HTTPS。你可以使用 `svr.set_ssl_cert_path()` 和 `svr.set_ssl_key_path()` 来设置 SSL 证书和密钥的路径，从而启用 HTTPS。

### 自定义响应头
你可以使用 `set_header()` 方法来自定义响应的头部信息。这对于控制缓存、跨域访问等非常有用。

```cpp
svr.Get("/custom", [](const httplib::Request& req, httplib::Response& res) {
    res.set_content("Custom Header", "text/plain");
    res.set_header("Custom-Header", "Value");
});
```

### 客户端请求
`cpp-httplib` 也可以用作 HTTP 客户端。你可以使用 `httplib::Client` 发起 GET、POST 等请求并处理响应。

```cpp
httplib::Client cli("localhost", 8080);

auto res = cli.Get("/hello");
if (res && res->status == 200) {
    std::cout << res->body << std::endl;
}
```

### 重定向
`cpp-httplib` 提供了 `Redirect()` 方法来实现重定向功能，将请求重定向到另一个 URL。

```cpp
svr.Get("/redirect", [](const httplib::Request& req, httplib::Response& res) {
    res.set_redirect("/hello");
});
```

### Cookie 操作
你可以使用 `set_cookie()` 方法来设置 Cookie，并使用 `get_header_value()` 获取请求中的 Cookie。

```cpp
svr.Get("/cookie", [](const httplib::Request& req, httplib::Response& res) {
    res.set_content("Set Cookie", "text/plain");
    res.set_cookie("user", "John");
});

svr.Get("/get_cookie", [](const httplib::Request& req, httplib::Response& res) {
    auto user = req.get_header_value("Cookie");
    res.set_content("Hello " + user, "text/plain");
});
```

### 客户端文件下载
你可以使用 `httplib::Client` 的 `Download()` 方法从服务器上下载文件。

```cpp
httplib::Client cli("localhost", 8080);

auto res = cli.Get("/download");
if (res && res->status == 200) {
    // 下载文件到本地
    std::ofstream ofs("downloaded_file.txt", std::ios::binary);
    ofs.write(res->body.data(), res->body.size());
}
```

### 自定义错误响应
使用 `status` 方法可以返回自定义的 HTTP 状态码和消息。

```cpp
svr.Get("/error", [](const httplib::Request& req, httplib::Response& res) {
    res.status = 404;
    res.set_content("Not Found", "text/plain");
});
```

### 身份验证
你可以通过设置响应头部来实现简单的身份验证，例如 Basic Auth。

```cpp
svr.Get("/auth", [](const httplib::Request& req, httplib::Response& res) {
    if (!req.has_header("Authorization")) {
        res.status = 401;
        res.set_header("WWW-Authenticate", "Basic realm=Restricted");
        res.set_content("Unauthorized", "text/plain");
        return;
    }

    // 根据 Authorization 处理身份验证
});
```

### 文件下载
除了客户端文件下载外，你也可以在服务器端设置文件下载的响应。

```cpp
svr.Get("/download", [](const httplib::Request& req, httplib::Response& res) {
    res.set_content("File Content", "text/plain");
    res.set_header("Content-Disposition", "attachment; filename=file.txt");
});
```

### 多路复用
`cpp-httplib` 支持多路复用，可以处理多个并发连接，提高服务器的性能和效率。

```cpp
// 创建服务器时设置多路复用
httplib::Server svr;
svr.set_keep_alive_max_count(100); // 设置最大并发连接数
```

### 服务器关闭
你可以使用 `stop()` 方法来关闭服务器。

```cpp
svr.Get("/shutdown", [&](const httplib::Request& req, httplib::Response& res) {
    svr.stop();
});
```

### 超时设置
通过 `set_read_timeout()` 和 `set_write_timeout()` 方法，可以设置读写超时时间。

```cpp
svr.set_read_timeout(5, 0); // 设置读取超时时间为 5 秒
svr.set_write_timeout(10, 0); // 设置写入超时时间为 10 秒
```

### 自定义日志
使用 `set_logger()` 方法，可以设置自定义的日志记录器，记录服务器的活动。

