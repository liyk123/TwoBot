#include "twobot.hh"
#include "nlohmann/json_fwd.hpp"
#include <string>
#include <httplib.h>
#include <utility>
#include <brynet/net/http/HttpService.hpp>
#include <tbb/tbb.h>

namespace twobot 
{
    extern std::atomic<std::size_t> g_seq = 0;
    using PromMapType = tbb::concurrent_hash_map<std::size_t, std::promise<ApiSet::SyncResult>>;
    extern PromMapType g_promMap;
    using brynet::net::http::HttpSession;
    using SessionMapType = tbb::concurrent_unordered_map<uint64_t, HttpSession::Ptr>;
    extern SessionMapType g_sessionMap;
    template<class... Ts> struct overload : Ts... { using Ts::operator()...; };
    template<class... Ts> overload(Ts...) -> overload<Ts...>;

    inline ApiSet::ApiResult callApiAsync(const std::string& api_name, const nlohmann::json& data, const ApiSet::AsyncConfig config, const ApiSet::AsyncMode& mode)
    {
        ApiSet::ApiResult ret;
        std::promise<ApiSet::SyncResult> prom;
        nlohmann::json content =
        {
            {"action", api_name.substr(1)},
            {"params", data},
        };
        std::size_t seq = g_seq++;
        ret = prom.get_future();
        if (mode.needResp)
        {
            content["echo"]["seq"] = seq;
            g_promMap.insert({ seq, std::move(prom) });
        }
        else
        {
            prom.set_value({ false, {} });
        }
        auto wsFrame = brynet::net::http::WebSocketFormat::wsFrameBuild(content.dump());
        g_sessionMap[config.id]->send(std::move(wsFrame));
        return ret;
    }

    inline ApiSet::ApiResult callApiSync(const std::string& api_name, const nlohmann::json& data, const ApiSet::SyncConfig& config, const ApiSet::SyncMode& mode)
    {
        ApiSet::ApiResult ret;
        std::promise<ApiSet::SyncResult> prom;
        ApiSet::SyncResult result{ false, {} };
        httplib::Client client(config.host, config.port);
        httplib::Headers headers = {
            {"Content-Type", "application/json"}
        };
        if (config.token.has_value()) {
            headers.insert(
                std::make_pair(
                    std::string{ "Authorization" },
                    "Bearer " + *config.token
                )
            );
        }
        httplib::Response response = {};

        if (mode.isPost)
        {
            auto r = client.Post(
                api_name,
                headers,
                data.dump(),
                "application/json"
            );
            if (r != nullptr) {
                response = *r;
            }
        }
        else
        {
            httplib::Params params = {};
            if (!data.empty())
            {
                params = data;
            }
            auto r = client.Get(api_name, params, headers);
            if (r != nullptr) {
                response = *r;
            }
        }

        result.first = (response.status == 200);

        try {
            result.second = nlohmann::json::parse(response.body, nullptr, !result.first);
        }
        catch (const std::exception& e) {
            result.second = nlohmann::json{
                {"error",e.what()}
            };
        }
        ret = prom.get_future();
        prom.set_value(result);
        return ret;
    }

    bool ApiSet::testConnection() {
        return callApi("/get_version_info", {}).get().first;
    }

	ApiSet::ApiSet(const ApiConfig& config, const ApiSet::ApiMode& mode)
        : m_config(config)
        , m_mode(mode)
    {

    }

    ApiSet::ApiResult ApiSet::callApi(const std::string &api_name, const nlohmann::json &data) {
		auto callApiImpl = overload{
            [&](AsyncConfig config, AsyncMode mode) {
                return callApiAsync(api_name, data, config, mode);
            },
            [&](SyncConfig config, SyncMode mode) {
                return callApiSync(api_name, data, config, mode);
            },
            [](AsyncConfig, SyncMode) -> ApiResult { return {}; },
            [](SyncConfig, AsyncMode) -> ApiResult { return {}; }
        };
        return std::visit(callApiImpl, m_config, m_mode);
    }

    ApiSet::ApiResult ApiSet::sendPrivateMsg(uint64_t user_id, const std::string &message, bool auto_escape){
        nlohmann::json data = {
            {"user_id", user_id},
            {"message", message},
            {"auto_escape", auto_escape}
        };
        return callApi("/send_private_msg", data);
    }

    ApiSet::ApiResult ApiSet::sendGroupMsg(uint64_t group_id, const std::string &message, bool auto_escape){
        nlohmann::json data = {
            {"group_id", group_id},
            {"message", message},
            {"auto_escape", auto_escape}
        };
        return callApi("/send_group_msg", data);
    }

    ApiSet::ApiResult ApiSet::sendMsg(std::string message_type, uint64_t user_id, uint64_t group_id, const std::string &message, bool auto_escape){
        nlohmann::json data = {
            {"message_type", message_type},
            {"user_id", user_id},
            {"group_id", group_id},
            {"message", message},
            {"auto_escape", auto_escape}
        };
        return callApi("/send_msg", data);
    }

    ApiSet::ApiResult ApiSet::deleteMsg(uint32_t message_id){
        nlohmann::json data = {
            {"message_id", message_id}
        };
        return callApi("/delete_msg", data);
    }

    ApiSet::ApiResult ApiSet::getMsg(uint32_t message_id){
        nlohmann::json data = {
            {"message_id", message_id}
        };
        return callApi("/get_msg", data);
    }

    ApiSet::ApiResult ApiSet::getForwardMsg(uint32_t id){
        nlohmann::json data = {
            {"id", id}
        };
        return callApi("/get_forward_msg", data);
    }

    ApiSet::ApiResult ApiSet::sendLike(uint64_t user_id, uint32_t times){
        nlohmann::json data = {
            {"user_id", user_id},
            {"times", times}
        };
        return callApi("/send_like", data);
    }

    ApiSet::ApiResult ApiSet::setGroupKick(uint64_t group_id, uint64_t user_id, bool reject_add_request){
        nlohmann::json data = {
            {"group_id", group_id},
            {"user_id", user_id},
            {"reject_add_request", reject_add_request}
        };
        return callApi("/set_group_kick", data);
    }

    ApiSet::ApiResult ApiSet::setGroupBan(uint64_t group_id, uint64_t user_id, uint32_t duration){
        nlohmann::json data = {
            {"group_id", group_id},
            {"user_id", user_id},
            {"duration", duration}
        };
        return callApi("/set_group_ban", data);
    }

    ApiSet::ApiResult ApiSet::setGroupAnonymousBan(uint64_t group_id, const std::string& anonymous, const std::string& flag, uint32_t duration){
        nlohmann::json data = {
            {"group_id", group_id},
            {"anonymous", anonymous},
            {"flag", flag},
            {"duration", duration}
        };
        return callApi("/set_group_anonymous_ban", data);
    }

    //ApiResult setGroupWholeBan(uint64_t group_id, bool enable = true);
    ApiSet::ApiResult ApiSet::setGroupWholeBan(uint64_t group_id, bool enable){
        nlohmann::json data = {
            {"group_id", group_id},
            {"enable", enable}
        };
        return callApi("/set_group_whole_ban", data);
    }

    ApiSet::ApiResult ApiSet::setGroupAdmin(uint64_t group_id, uint64_t user_id, bool enable){
        nlohmann::json data = {
            {"group_id", group_id},
            {"user_id", user_id},
            {"enable", enable}
        };
        return callApi("/set_group_admin", data);
    }

    ApiSet::ApiResult ApiSet::setGroupAnonymous(uint64_t group_id, bool enable){
        nlohmann::json data = {
            {"group_id", group_id},
            {"enable", enable}
        };
        return callApi("/set_group_anonymous", data);
    }

    ApiSet::ApiResult ApiSet::setGroupCard(uint64_t group_id, uint64_t user_id, const std::string& card){
        nlohmann::json data = {
            {"group_id", group_id},
            {"user_id", user_id},
            {"card", card}
        };
        return callApi("/set_group_card", data);
    }

    // ApiResult setGroupName(uint64_t group_id, const std::string& name);
    ApiSet::ApiResult ApiSet::setGroupName(uint64_t group_id, const std::string& name){
        nlohmann::json data = {
            {"group_id", group_id},
            {"name", name}
        };
        return callApi("/set_group_name", data);
    }

    // ApiResult setGroupLeave(uint64_t group_id, bool is_dismiss = false);
    ApiSet::ApiResult ApiSet::setGroupLeave(uint64_t group_id, bool is_dismiss){
        nlohmann::json data = {
            {"group_id", group_id},
            {"is_dismiss", is_dismiss}
        };
        return callApi("/set_group_leave", data);
    }

    // ApiResult setGroupSpecialTitle(uint64_t group_id, uint64_t user_id, const std::string& special_title, int32_t duration = -1); 
    ApiSet::ApiResult ApiSet::setGroupSpecialTitle(uint64_t group_id, uint64_t user_id, const std::string& special_title, int32_t duration){
        nlohmann::json data = {
            {"group_id", group_id},
            {"user_id", user_id},
            {"special_title", special_title},
            {"duration", duration}
        };
        return callApi("/set_group_special_title", data);
    }

    // ApiResult setFriendAddRequest(const std::string& flag, bool approve = true, const std::string& remark = "");
    ApiSet::ApiResult ApiSet::setFriendAddRequest(const std::string& flag, bool approve, const std::string& remark){
        nlohmann::json data = {
            {"flag", flag},
            {"approve", approve},
            {"remark", remark}
        };
        return callApi("/set_friend_add_request", data);
    }

    // ApiResult setGroupAddRequest(const std::string& flag, const std::string& sub_type, bool approve = true, const std::string& reason = "");
    ApiSet::ApiResult ApiSet::setGroupAddRequest(const std::string& flag, const std::string& sub_type, bool approve, const std::string& reason){
        nlohmann::json data = {
            {"flag", flag},
            {"sub_type", sub_type},
            {"approve", approve},
            {"reason", reason}
        };
        return callApi("/set_group_add_request", data);
    }

    // ApiResult getLoginInfo();
    ApiSet::ApiResult ApiSet::getLoginInfo(){
        return callApi("/get_login_info", {});
    }

    // ApiResult getStrangerInfo(uint64_t user_id, bool no_cache = false);
    ApiSet::ApiResult ApiSet::getStrangerInfo(uint64_t user_id, bool no_cache){
        nlohmann::json data = {
            {"user_id", user_id},
            {"no_cache", no_cache}
        };
        return callApi("/get_stranger_info", data);
    }

    // ApiResult getFriendList();
    ApiSet::ApiResult ApiSet::getFriendList(){
        return callApi("/get_friend_list", {});
    }

    // ApiResult getGroupInfo(uint64_t group_id, bool no_cache = false);
    ApiSet::ApiResult ApiSet::getGroupInfo(uint64_t group_id, bool no_cache){
        nlohmann::json data = {
            {"group_id", group_id},
            {"no_cache", no_cache}
        };
        return callApi("/get_group_info", data);
    }

    // ApiResult getGroupList();
    ApiSet::ApiResult ApiSet::getGroupList(){
        return callApi("/get_group_list", {});
    }

    // ApiResult getGroupMemberInfo(uint64_t group_id, uint64_t user_id, bool no_cache = false);
    ApiSet::ApiResult ApiSet::getGroupMemberInfo(uint64_t group_id, uint64_t user_id, bool no_cache){
        nlohmann::json data = {
            {"group_id", group_id},
            {"user_id", user_id},
            {"no_cache", no_cache}
        };
        return callApi("/get_group_member_info", data);
    }

    // ApiResult getGroupMemberList(uint64_t group_id);
    ApiSet::ApiResult ApiSet::getGroupMemberList(uint64_t group_id){
        nlohmann::json data = {
            {"group_id", group_id}
        };
        return callApi("/get_group_member_list", data);
    }

    // ApiResult getGroupHonorInfo(uint64_t group_id, const std::string& type);
    ApiSet::ApiResult ApiSet::getGroupHonorInfo(uint64_t group_id, const std::string& type){
        nlohmann::json data = {
            {"group_id", group_id},
            {"type", type}
        };
        return callApi("/get_group_honor_info", data);
    }

    // ApiResult getCookies(const std::string& domain = "");
    ApiSet::ApiResult ApiSet::getCookies(const std::string& domain){
        nlohmann::json data = {
            {"domain", domain}
        };
        return callApi("/get_cookies", data);
    }

    // ApiResult getCsrfToken();
    ApiSet::ApiResult ApiSet::getCsrfToken(){
        return callApi("/get_csrf_token", {});
    }

    // ApiResult getCredentials(const std::string& domain = "");
    ApiSet::ApiResult ApiSet::getCredentials(const std::string& domain){
        nlohmann::json data = {
            {"domain", domain}
        };
        return callApi("/get_credentials", data);
    }

    // ApiResult getRecord(const std::string& file, const std::string& out_format);
    ApiSet::ApiResult ApiSet::getRecord(const std::string& file, const std::string& out_format){
        nlohmann::json data = {
            {"file", file},
            {"out_format", out_format}
        };
        return callApi("/get_record", data);
    }

    // ApiResult getImage(const std::string& file);
    ApiSet::ApiResult ApiSet::getImage(const std::string& file){
        nlohmann::json data = {
            {"file", file}
        };
        return callApi("/get_image", data);
    }

    // ApiResult canSendImage();
    ApiSet::ApiResult ApiSet::canSendImage(){
        return callApi("/can_send_image", {});
    }

    //  ApiResult canSendRecord();
    ApiSet::ApiResult ApiSet::canSendRecord(){
        return callApi("/can_send_record", {});
    }

    //  ApiResult getStatus();
    ApiSet::ApiResult ApiSet::getStatus(){
        return callApi("/get_status", {});
    }

    // ApiResult getVersionInfo();
    ApiSet::ApiResult ApiSet::getVersionInfo(){
        return callApi("/get_version_info", {});
    }

    // ApiResult setRestart(int delay = 0);
    ApiSet::ApiResult ApiSet::setRestart(int delay){
        nlohmann::json data = {
            {"delay", delay}
        };
        return callApi("/set_restart", data);
    }

    // ApiResult cleanCache();
    ApiSet::ApiResult ApiSet::cleanCache(){
        return callApi("/clean_cache", {});
    }
}