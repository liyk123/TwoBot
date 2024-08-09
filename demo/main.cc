#include <twobot.hh>
#include <iostream>
#include <httplib.h>

/// 警告: 这个项目使用了JSON for modern C++, 必须使用UTF8编码，不然会出现异常。
/// warning: this project uses JSON for modern C++, must use UTF8 encoding, otherwise it will throw exception.

using twobot::Config;
using twobot::BotInstance;
using twobot::ApiSet;
using namespace twobot::Event;
using twobot::Session;

static inline std::string getRealID(uint64_t vid)
{
	std::string realID = "";
    std::stringstream ss;
    ss << vid;
    nlohmann::json data = {
        { "type", "2"},
        { "id", ss.str()}
    };
    httplib::Client client("http://10.8.0.1:9444");
    auto r = client.Get("/getid", httplib::Params(data), {});
    //const auto& r = instance->getApiSet(false).callApi("/getid", data);
    if (r != nullptr)
    {
        auto ret = nlohmann::json::parse(r->body);
        std::cout << ret.dump() << std::endl;
        realID = ret.value("id", realID);
    }
    return realID;
}

int main(int argc, char** args) {
    // 解决UTF8编码，中文乱码问题，不需要可以不加
#ifdef _WIN32
    system("chcp 65001 && cls");
#endif
    Config config = {
        "10.8.0.1",
        5700,
        9444
    };
    auto instance = BotInstance::createInstance(config);


    if(!instance->getApiSet(false).testConnection())
    {
        std::cerr << "HTTP测试失败，请启动onebot服务器，并配置HTTP端口！" << std::endl;
    }
    else
    {
        std::cout << "HTTP测试通过!" << std::endl;
    }

    instance->onEvent<GroupMsg>([&instance](const GroupMsg & msg, const Session::Ptr& session){
        twobot::ApiSet::ApiResult r = {};
        if (msg.raw_message == "你好") 
        {
            r = instance->getApiSet().sendGroupMsg(msg.group_id, "你好，我是twobot！");
        }
		else if (msg.raw_message.find("AT我") != std::string::npos) 
        {
			std::string at = "[CQ:at,qq=" + std::to_string(msg.user_id) + "]";
			r = instance->getApiSet().sendGroupMsg(msg.group_id, at + "要我at你干啥？");
        }
        else if (msg.raw_message == "getGroupMemberList") 
        {
            r = instance->getApiSet(session).getGroupMemberList(msg.group_id);
            instance->getApiSet().sendGroupMsg(msg.group_id, "获取成功");
        }
        else if (msg.raw_message == "getAvatar")
        {
            std::string group_id = getRealID(msg.group_id);
            std::string user_id = getRealID(msg.user_id);
			nlohmann::json param = {
                {"group_id", "B6240B4B49D1EAC79F423C7C39D5D460"},
                {"user_id", "CCBEAF4DD759BE6C3580672243972A69"}
            };
            r = instance->getApiSet().callApi("/get_avatar", param);
			if (r.second != nullptr) {
                instance->getApiSet().sendGroupMsg(msg.group_id, r.second.dump(4));
            }
        }
        std::cout << r.second.dump(4) << std::endl;
    });

    instance->onEvent<PrivateMsg>([&instance](const PrivateMsg & msg, const Session::Ptr& session){
        if (msg.raw_message == "你好")
        {
            ApiSet set = instance->getApiSet(session);
            const auto &r = set.sendPrivateMsg(msg.user_id, "你好，我是twobot！");
            std::cout << r << std::endl;
        }
    });


    instance->onEvent<EnableEvent>([&instance](const EnableEvent & msg, const Session::Ptr& session){
        std::cout << "twobot已启动！机器人QQ："<< msg.self_id << std::endl;
    });

    instance->onEvent<DisableEvent>([&instance](const DisableEvent & msg, const Session::Ptr& session){
        std::cout << "twobot已停止！" << std::endl;
    });

    instance->onEvent<ConnectEvent>([&instance](const ConnectEvent & msg, const Session::Ptr& session){
        std::cout << "twobot已连接！" << std::endl;
    });


    // 启动机器人
    instance->start();
    
    return 0;
}
