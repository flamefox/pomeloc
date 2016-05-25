# pomeloc

a pomelo protocol unity-c# stub generator.

pomeloc 是一个针对[pomelo](https://github.com/NetEase/pomelo) 的协议生成unity3d-c#存根代码的工具，需要配合flamefox的[pomelo-unitychat-socket](https://github.com/flamefox/pomelo-unitychat)中的客户端代码使用

## 安装方法
需要cmake

windows:

需要vs2015
```batch
open your vcvarsall.bat 
cd build
run build.cmd
```
##

使用
```batch
pomeloc.exe --csharp  [--ns Proto] serverProtos.json clientProtos.json
```

生成clientProto.cs后就可以在代码里正常的使用了,以chatofpomelo的send函数为例
```csharp
 public void send(string message)
    {
        chatHandler.send(
            "pomelo",
            message,
            user,
            "*"
            );
    }
```

生成的存根如下:
```csharp
namespace Proto
{
    namespace chat
    {
        public class chatHandler
        {
            public static PomeloClient pc = null;
            public static bool send(string rid,string content,string from,string target)
            {
                JsonData data = new JsonData();
                data["rid"] = rid;
                data["content"] = content;
                data["from"] = from;
                data["target"] = target;
                pc.notify("chat.chatHandler.send", data);
                return true;
            }
        }
    }
}
```

## 规则
如果clientProto.json有而serverProto.json没有,则认为是notify
如果clientProto.json有并且serverProto.json有,则认为是request
如果只有serverProto.json有,则认为是服务器主动下发,代码中使用ServerEvent.xxx调用

要求前端默认开启dict,proto

客户端连接成功后需要手动更新对应存根代码的客户端对象pc

详细使用可以看[demo](https://github.com/flamefox/pomelo-unitychat)