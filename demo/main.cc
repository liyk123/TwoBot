#include <twobot.hh>
#include <iostream>
#include <httplib.h>
/// 警告: 这个项目使用了JSON for modern C++, 必须使用UTF8编码，不然会出现异常。
/// warning: this project uses JSON for modern C++, must use UTF8 encoding, otherwise it will throw exception.

using twobot::Config;
using twobot::BotInstance;
using twobot::ApiSet;
using namespace twobot::Event;

int main(int argc, char** args) {
    // 解决UTF8编码，中文乱码问题，不需要可以不加
#ifdef _WIN32
    system("chcp 65001 && cls");
#endif
    const char* localeName = "zh_CN.UTF-8";
    std::setlocale(LC_ALL, localeName);
    std::locale::global(std::locale(localeName));
    Config config = {
        "10.9.0.1",
        5700,
        9444
    };
    auto instance = BotInstance::createInstance(config);


    if (!instance->getApiSet().testConnection())
    {
        std::cerr << "HTTP测试失败，请启动onebot服务器，并配置HTTP端口！" << std::endl;
    }
    else
    {
        std::cout << "HTTP测试通过!" << std::endl;
    }

	instance->onEvent<GroupMsg>([&instance](const GroupMsg& msg) -> coro::task<> {
		twobot::ApiSet::ApiResult r = []() -> ApiSet::ApiResult { co_return{}; }();
        if (msg.raw_message == "你好") 
        {
            r = instance->getApiSet().sendGroupMsg(msg.group_id, "你好，我是twobot！");
        }
		else if (msg.raw_message.find("AT我") != std::string::npos) 
        {
			std::string at = std::format("[CQ:at,qq={}]", msg.user_id);
			r = instance->getApiSet().sendGroupMsg(msg.group_id, at + "要我at你干啥？");
        }
        else if (msg.raw_message == "头像")
        {
            auto cqcode = std::format("[CQ:avatar,qq={}]", msg.user_id);
			r = instance->getApiSet().sendGroupMsg(msg.group_id, cqcode);
        }
        else if (msg.raw_message == "私聊")
        {
            r = instance->getApiSet().sendPrivateMsg(msg.user_id, "你好，我是twobot！");
        }

        std::cout << coro::sync_wait(r) << std::endl;
        co_return;
    });

	instance->onEvent<PrivateMsg>([&instance](const PrivateMsg& msg) -> coro::task<> {
        twobot::ApiSet::SyncResult r = {};
        if (msg.raw_message == "你好")
        {
            r = co_await instance->getApiSet(msg.self_id, { true }).sendPrivateMsg(msg.user_id, "你好，我是twobot！");
        }
        else if (msg.raw_message == "头像")
        {
            nlohmann::json param = {
                {"user_id", msg.user_id}
            };
			auto cqcode = std::format("[CQ:avatar,qq={}]", msg.user_id);
			r = co_await instance->getApiSet(msg.self_id).sendPrivateMsg(msg.user_id, cqcode);
        }
        std::cout << r << std::endl;
        co_return;
    });

	instance->onEvent<EnableEvent>([&instance](const EnableEvent& msg) -> coro::task<> {
        std::cout << "twobot已启动！机器人QQ："<< msg.self_id << std::endl;
        co_return;
    });

	instance->onEvent<DisableEvent>([&instance](const DisableEvent& msg) -> coro::task<> {
        std::cout << "twobot已停止！ID: " << msg.self_id << std::endl;
        co_return;
    });

	instance->onEvent<ConnectEvent>([&instance](const ConnectEvent& msg) -> coro::task<> {
        std::cout << "twobot已连接！ID: " << msg.self_id << std::endl;
        co_return;
    });


    // 启动机器人
    instance->start();
    
    return 0;
}
