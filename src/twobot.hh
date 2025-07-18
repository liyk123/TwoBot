#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <optional>
#include <memory>
#include <unordered_map>
#include <functional>
#include <nlohmann/json.hpp>
#include <variant>
#include <concepts>
#include <future>

namespace twobot
{
    struct EventType{
        std::string_view post_type;
        std::string_view sub_type;

        bool operator==(const EventType &other) const {
            return post_type == other.post_type && sub_type == other.sub_type;
        }
    };
};

namespace std{
    template<>
    struct hash<twobot::EventType> {
        size_t operator()(const twobot::EventType &event) const {
            auto left_hash = hash<std::string_view>()(event.post_type);
            auto right_hash = hash<std::string_view>()(event.sub_type);
            return  left_hash ^ (right_hash << 1);
        }
    };
};

namespace twobot {

    // 服务器配置
    struct Config{
        std::string host;
        std::uint16_t  api_port;
        std::uint16_t  ws_port;
        std::optional<std::string> token;
    };

    // Api集合，所有对机器人调用的接口都在这里
    struct ApiSet{

        bool testConnection();

		struct SyncMode { bool isPost; };
		struct AsyncMode { bool needResp; };
        using ApiMode = std::variant<SyncMode, AsyncMode>;

        struct SyncConfig {
            std::string host;
            uint16_t port;
            std::optional<std::string> token;
        };
        struct AsyncConfig { uint64_t id; };
        using ApiConfig = std::variant<SyncConfig, AsyncConfig>;

        using SyncResult = std::pair<bool, nlohmann::json>;
        using ApiResult = std::future<SyncResult>;
        // 万api之母，负责提起所有的api的请求
        ApiResult callApi(const std::string &api_name, const nlohmann::json &data);

        // 下面要实现onebot标准的所有api

        /** 
        send_private_msg 发送私聊消息
        参数
            字段名	数据类型	默认值	说明
            user_id	number	-	对方 QQ 号
            message	message	-	要发送的内容
            auto_escape	boolean	false	消息内容是否作为纯文本发送（即不解析 CQ 码），只在 message 字段是字符串时有效
        响应数据
            字段名	数据类型	说明
            message_id	number (int32)	消息 ID
        */
        ApiResult sendPrivateMsg(uint64_t user_id, const std::string &message, bool auto_escape = false);
        
        /**
        send_group_msg 发送群消息
        参数
            字段名	数据类型	默认值	说明
            group_id	number	-	群号
            message	message	-	要发送的内容
            auto_escape	boolean	false	消息内容是否作为纯文本发送（即不解析 CQ 码），只在 message 字段是字符串时有效
        响应数据
            字段名	数据类型	说明
            message_id	number (int32)	消息 ID
        */
        ApiResult sendGroupMsg(uint64_t group_id, const std::string &message, bool auto_escape = false);
        
        /**
        send_msg 发送消息
        参数
            字段名	数据类型	默认值	说明
            message_type	string	-	消息类型，支持 private、group，分别对应私聊、群组，如不传入，则根据传入的 *_id 参数判断
            user_id	number	-	对方 QQ 号（消息类型为 private 时需要）
            group_id	number	-	群号（消息类型为 group 时需要）
            message	message	-	要发送的内容
            auto_escape	boolean	false	消息内容是否作为纯文本发送（即不解析 CQ 码），只在 message 字段是字符串时有效
        响应数据
            字段名	数据类型	说明
            message_id	number (int32)	消息 ID
        */
        ApiResult sendMsg(std::string message_type, uint64_t user_id, uint64_t group_id, const std::string &message, bool auto_escape = false);
        
        /**
        delete_msg 撤回消息
        参数
            字段名	数据类型	默认值	说明
            message_id	number (int32)	-	消息 ID
        响应数据
        无
        */
        ApiResult deleteMsg(uint32_t message_id);
        
        /**
        get_msg 获取消息
        参数
            字段名	数据类型	说明
            message_id	number (int32)	消息 ID
        响应数据
            字段名	数据类型	说明
            time	number (int32)	发送时间
            message_type	string	消息类型，同 消息事件
            message_id	number (int32)	消息 ID
            real_id	number (int32)	消息真实 ID
            sender	object	发送人信息，同 消息事件
            message	message	消息内容
        */
        ApiResult getMsg(uint32_t message_id);
        
        /**
        get_forward_msg 获取合并转发消息
        参数
            字段名	数据类型	说明
            id	string	合并转发 ID
        响应数据
            字段名	类型	说明
            message	message	消息内容，使用 消息的数组格式 表示，数组中的消息段全部为 node 消息段
        */
        ApiResult getForwardMsg(uint32_t id);

        /**
        send_like 发送好友赞
        参数
            字段名	数据类型	默认值	说明
            user_id	number	-	对方 QQ 号
            times	number	1	赞的次数，每个好友每天最多 10 次
        响应数据
            无
        **/
        ApiResult sendLike(uint64_t user_id, uint32_t times = 1);

        /**
        set_group_kick 群组踢人
        参数
            字段名	数据类型	默认值	说明
            group_id	number	-	群号
            user_id	number	-	要踢的 QQ 号
            reject_add_request	boolean	false	拒绝此人的加群请求
        响应数据
            无
        */
        ApiResult setGroupKick(uint64_t group_id, uint64_t user_id, bool reject_add_request = false);

        /**
        set_group_ban 群组单人禁言
        参数
            字段名	数据类型	默认值	说明
            group_id	number	-	群号
            user_id	number	-	要禁言的 QQ 号
            duration	number	30 * 60	禁言时长，单位秒，0 表示取消禁言
        响应数据
            无
        */
        ApiResult setGroupBan(uint64_t group_id, uint64_t user_id, uint32_t duration = 30 * 60);

        /** 
        set_group_anonymous_ban 群组匿名用户禁言
        参数
            字段名	数据类型	默认值	说明
            group_id	number	-	群号
            anonymous	object	-	可选，要禁言的匿名用户对象（群消息上报的 anonymous 字段）
            anonymous_flag 或 flag	string	-	可选，要禁言的匿名用户的 flag（需从群消息上报的数据中获得）
            duration	number	30 * 60	禁言时长，单位秒，无法取消匿名用户禁言
        上面的 anonymous 和 anonymous_flag 两者任选其一传入即可，若都传入，则使用 anonymous。

        响应数据
            无
        */
        ApiResult setGroupAnonymousBan(uint64_t group_id, const std::string& anonymous, const std::string& flag, uint32_t duration = 30 * 60);

        /** 
        set_group_whole_ban 群组全员禁言
        参数
            字段名	数据类型	默认值	说明
            group_id	number	-	群号
            enable	boolean	true	是否禁言
        响应数据
            无
        */
        ApiResult setGroupWholeBan(uint64_t group_id, bool enable = true);

        /**
        set_group_admin 群组设置管理员
        参数
            字段名	数据类型	默认值	说明
            group_id	number	-	群号
            user_id	number	-	要设置的管理员 QQ 号
            enable	boolean	true	是否设置为管理员
        响应数据
            无
        */
        ApiResult setGroupAdmin(uint64_t group_id, uint64_t user_id, bool enable = true);

        /**
        set_group_anonymous 群组匿名设置
        参数
            字段名	数据类型	默认值	说明
            group_id	number	-	群号
            enable	boolean	true	是否允许匿名聊天
        响应数据
            无
        */
        ApiResult setGroupAnonymous(uint64_t group_id, bool enable = true);

        /**
        set_group_card 群名片设置（群备注）
        参数
            字段名	数据类型	默认值	说明
            group_id	number	-	群号
            user_id	number	-	要设置的群名片 QQ 号
            card	string	-	要设置的群名片内容
        响应数据
            无
        */
        ApiResult setGroupCard(uint64_t group_id, uint64_t user_id, const std::string& card);

        /**
        set_group_name 设置群名
        参数
            字段名	数据类型	默认值	说明
            group_id	number	-	群号
            name	string	-	要设置的群名
        响应数据
            无
        */
        ApiResult setGroupName(uint64_t group_id, const std::string& name);

        /**
        set_group_leave 退出群组
        参数
            字段名	数据类型	默认值	说明
            group_id	number	-	群号
            is_dismiss	boolean	false	是否解散该群组
        响应数据
            无
        */
        ApiResult setGroupLeave(uint64_t group_id, bool is_dismiss = false);


        /**
        set_group_special_title 设置群组专属头衔
        参数
            字段名	数据类型	默认值	说明
            group_id	number	-	群号
            user_id	number	-	要设置的 QQ 号
            special_title	string	空	专属头衔，不填或空字符串表示删除专属头衔
            duration	number	-1	专属头衔有效期，单位秒，-1 表示永久，不过此项似乎没有效果，可能是只有某些特殊的时间长度有效，有待测试
        响应数据
            无
        */
        ApiResult setGroupSpecialTitle(uint64_t group_id, uint64_t user_id, const std::string& special_title, int32_t duration = -1); 

        /**
        set_friend_add_request 处理加好友请求
        参数
            字段名	数据类型	默认值	说明
            flag	string	-	加好友请求的 flag（需从上报的数据中获得）
            approve	boolean	true	是否同意请求
            remark	string	空	添加后的好友备注（仅在同意时有效）
        响应数据
            无
        */
        ApiResult setFriendAddRequest(const std::string& flag, bool approve = true, const std::string& remark = "");

        /**
        set_group_add_request 处理加群请求／邀请
        参数
            字段名	数据类型	默认值	说明
            flag	string	-	加群请求的 flag（需从上报的数据中获得）
            sub_type 或 type	string	-	add 或 invite，请求类型（需要和上报消息中的 sub_type 字段相符）
            approve	boolean	true	是否同意请求／邀请
            reason	string	空	拒绝理由（仅在拒绝时有效）
        响应数据
            无
        */
        ApiResult setGroupAddRequest(const std::string& flag, const std::string& sub_type, bool approve = true, const std::string& reason = "");

        /**
        get_login_info 获取登录号信息
        参数
            无

        响应数据
            字段名	数据类型	说明
            user_id	number (int64)	QQ 号
            nickname	string	QQ 昵称
        */
        ApiResult getLoginInfo();

        /**
        get_stranger_info 获取陌生人信息
        参数
            字段名	数据类型	默认值	说明
            user_id	number	-	QQ 号
            no_cache	boolean	false	是否不使用缓存（使用缓存可能更新不及时，但响应更快）
        响应数据
            字段名	数据类型	说明
            user_id	number (int64)	QQ 号
            nickname	string	昵称
            sex	string	性别，male 或 female 或 unknown
            age	number (int32)	年龄
        */
        ApiResult getStrangerInfo(uint64_t user_id, bool no_cache = false);

        /**
        get_friend_list 获取好友列表
        参数
            无

        响应数据
            响应内容为 JSON 数组，每个元素如下：
            字段名	数据类型	说明
            user_id	number (int64)	QQ 号
            nickname	string	昵称
            remark	string	备注名
        */
        ApiResult getFriendList();

        /**
        get_group_info 获取群信息
        参数
            字段名	数据类型	默认值	说明
            group_id	number	-	群号
            no_cache	boolean	false	是否不使用缓存（使用缓存可能更新不及时，但响应更快）
        响应数据
            字段名	数据类型	说明
            group_id	number (int64)	群号
            group_name	string	群名称
            member_count	number (int32)	成员数
            max_member_count	number (int32)	最大成员数（群容量）
        */
        ApiResult getGroupInfo(uint64_t group_id, bool no_cache = false);

        /**
        get_group_list 获取群列表
        参数
            无

        响应数据
            响应内容为 JSON 数组，每个元素和上面的 get_group_info 接口相同。
        */
        ApiResult getGroupList();

        /**
        get_group_member_info 获取群成员信息
        参数
            字段名	数据类型	默认值	说明
            group_id	number	-	群号
            user_id	number	-	QQ 号
            no_cache	boolean	false	是否不使用缓存（使用缓存可能更新不及时，但响应更快）
        响应数据
            字段名	数据类型	说明
            group_id	number (int64)	群号
            user_id	number (int64)	QQ 号
            nickname	string	昵称
            card	string	群名片／备注
            sex	string	性别，male 或 female 或 unknown
            age	number (int32)	年龄
            area	string	地区
            join_time	number (int32)	加群时间戳
            last_sent_time	number (int32)	最后发言时间戳
            level	string	成员等级
            role	string	角色，owner 或 admin 或 member
            unfriendly	boolean	是否不良记录成员
            title	string	专属头衔
            title_expire_time	number (int32)	专属头衔过期时间戳
            card_changeable	boolean	是否允许修改群名片
        */
        ApiResult getGroupMemberInfo(uint64_t group_id, uint64_t user_id, bool no_cache = false);

        /**
        get_group_member_list 获取群成员列表
        参数
            字段名	数据类型	默认值	说明
            group_id	number (int64)	-	群号
        响应数据
            响应内容为 JSON 数组，每个元素的内容和上面的 get_group_member_info 接口相同，但对于同一个群组的同一个成员，获取列表时和获取单独的成员信息时，某些字段可能有所不同，例如 area、title 等字段在获取列表时无法获得，具体应以单独的成员信息为准。
        */
        ApiResult getGroupMemberList(uint64_t group_id);

        /**
        get_group_honor_info 获取群荣誉信息
        参数
            字段名	数据类型	默认值	说明
            group_id	number (int64)	-	群号
            type	string	-	要获取的群荣誉类型，可传入 talkative performer legend strong_newbie emotion 以分别获取单个类型的群荣誉数据，或传入 all 获取所有数据
        响应数据
            字段名	数据类型	说明
            group_id	number (int64)	群号
            current_talkative	object	当前龙王，仅 type 为 talkative 或 all 时有数据
            talkative_list	array	历史龙王，仅 type 为 talkative 或 all 时有数据
            performer_list	array	群聊之火，仅 type 为 performer 或 all 时有数据
            legend_list	array	群聊炽焰，仅 type 为 legend 或 all 时有数据
            strong_newbie_list	array	冒尖小春笋，仅 type 为 strong_newbie 或 all 时有数据
            emotion_list	array	快乐之源，仅 type 为 emotion 或 all 时有数据
        
        其中 current_talkative 字段的内容如下：
            字段名	数据类型	说明
            user_id	number (int64)	QQ 号
            nickname	string	昵称
            avatar	string	头像 URL
            day_count	number (int32)	持续天数

        其它各 *_list 的每个元素是一个 JSON 对象，内容如下：
            字段名	数据类型	说明
            user_id	number (int64)	QQ 号
            nickname	string	昵称
            avatar	string	头像 URL
            description	string	荣誉描述
        */
        ApiResult getGroupHonorInfo(uint64_t group_id, const std::string& type);

        /**
        get_cookies 获取 Cookies
        参数
            字段名	数据类型	默认值	说明
            domain	string	空	需要获取 cookies 的域名
        响应数据
            字段名	数据类型	说明
            cookies	string	Cookies
        */
        ApiResult getCookies(const std::string& domain = "");

        /**
        get_csrf_token 获取 CSRF Token
        参数
            无

        响应数据
            字段名	数据类型	说明
            token	number (int32)	CSRF Token
        */
        ApiResult getCsrfToken();

        /**
        get_credentials 获取 QQ 相关接口凭证
        即上面两个接口的合并。

        参数
            字段名	数据类型	默认值	说明
            domain	string	空	需要获取 cookies 的域名
        响应数据
            字段名	数据类型	说明
            cookies	string	Cookies
            csrf_token	number (int32)	CSRF Token
        */
        ApiResult getCredentials(const std::string& domain = "");

        /**
        get_record 获取语音
        提示：要使用此接口，通常需要安装 ffmpeg，请参考 OneBot 实现的相关说明。

        参数
            字段名	数据类型	默认值	说明
            file	string	-	收到的语音文件名（消息段的 file 参数），如 0B38145AA44505000B38145AA4450500.silk
            out_format	string	-	要转换到的格式，目前支持 mp3、amr、wma、m4a、spx、ogg、wav、flac
        响应数据
            字段名	数据类型	说明
            file	string	转换后的语音文件路径，如 /home/somebody/cqhttp/data/record/0B38145AA44505000B38145AA4450500.mp3
        */
        ApiResult getRecord(const std::string& file, const std::string& out_format);

        /**
        get_image 获取图片
        参数
            字段名	数据类型	默认值	说明
            file	string	-	收到的图片文件名（消息段的 file 参数），如 6B4DE3DFD1BD271E3297859D41C530F5.jpg
        响应数据
            字段名	数据类型	说明
            file	string	下载后的图片文件路径，如 /home/somebody/cqhttp/data/image/6B4DE3DFD1BD271E3297859D41C530F5.jpg
        */
        ApiResult getImage(const std::string& file);

        /**
        can_send_image 检查是否可以发送图片
        参数
            无

        响应数据
            字段名	数据类型	说明
            yes	boolean	是或否
        */
        ApiResult canSendImage();

        /**
        can_send_record 检查是否可以发送语音
        参数
            无

        响应数据
            字段名	数据类型	说明
            yes	boolean	是或否
        */
        ApiResult canSendRecord();

        /**
        get_status 获取运行状态
        参数
            无

        响应数据
            字段名	数据类型	说明
            online	boolean	当前 QQ 在线，null 表示无法查询到在线状态
            good	boolean	状态符合预期，意味着各模块正常运行、功能正常，且 QQ 在线
            ……	-	OneBot 实现自行添加的其它内容
        通常情况下建议只使用 online 和 good 这两个字段来判断运行状态，因为根据 OneBot 实现的不同，其它字段可能完全不同。
        */
        ApiResult getStatus();

        /**
        get_version_info 获取版本信息
        参数
         无

        响应数据
            字段名	数据类型	说明
            app_name	string	应用标识，如 mirai-native
            app_version	string	应用版本，如 1.2.3
            protocol_version	string	OneBot 标准版本，如 v11
            ……	-	OneBot 实现自行添加的其它内容
        */
        ApiResult getVersionInfo();

        /**
        set_restart 重启 OneBot 实现
        由于重启 OneBot 实现同时需要重启 API 服务，这意味着当前的 API 请求会被中断，因此需要异步地重启，接口返回的 status 是 async。

        参数
            字段名	数据类型	默认值	说明
            delay	number	0	要延迟的毫秒数，如果默认情况下无法重启，可以尝试设置延迟为 2000 左右
        响应数据
            无
        */
        ApiResult setRestart(int delay = 0);

        /**
        clean_cache 清理缓存
        用于清理积攒了太多的缓存文件。

        参数
            无

        响应数据
            无
        */
        ApiResult cleanCache();
    protected:
		ApiSet(const ApiConfig& config, const ApiMode& mode);
        ApiConfig m_config;
        ApiMode m_mode;
        friend class BotInstance;
    };



    namespace Event{

        template<typename T>
        concept Concept = requires(T obj) {
            { obj.getType() } -> std::same_as<EventType>;
            { obj.raw_msg } -> std::convertible_to<nlohmann::json>;
        };

        // 以 以下类为模板参数 的onEvent必须在export_functions中调用一次，才能实现模板特化导出
        struct PrivateMsg {
            static constexpr EventType getType() {
                return {"message", "private"};
            }

            uint64_t time; // 消息发送时间
            uint64_t user_id; // 发送消息的人的QQ
            uint64_t self_id; // 机器人自身QQ

            std::string raw_message; //原始文本消息（含有CQ码）
            enum SUB_TYPE {
                FRIEND, // 好友
                GROUP,  // 群私聊
                OTHER   // 其他
            } sub_type; //消息子类型

            nlohmann::json sender; // 日后进一步处理

            nlohmann::json raw_msg;
        };

        struct GroupMsg {
            static constexpr EventType getType() {
                return {"message", "group"};
            }

            uint64_t time; // 消息发送时间
            uint64_t user_id; // 发送消息的人的QQ
            uint64_t self_id; // 机器人自身QQ
            uint64_t group_id; // 群QQ
            
            std::string raw_message; //原始文本消息（含有CQ码）
            std::string group_name; // 群的名称
            enum SUB_TYPE {
                NORMAL,     // 正常消息
                ANONYMOUS,  // 系统消息
                NOTICE,     // 通知消息，如 管理员已禁止群内匿名聊天
            } sub_type; //消息子类型

            nlohmann::json sender; // 日后进一步处理

            nlohmann::json raw_msg;
        };

        struct EnableEvent {
            static constexpr EventType getType() {
                return {"meta_event", "enable"};
            }
            uint64_t time; // 事件产生的时间
            uint64_t self_id; // 机器人自身QQ

            nlohmann::json raw_msg;
        };

        struct DisableEvent {
            static constexpr EventType getType() {
                return {"meta_event", "disable"};
            }
            uint64_t time; // 事件产生的时间
            uint64_t self_id; // 机器人自身QQ

            nlohmann::json raw_msg;
        };

        struct ConnectEvent {
            static constexpr EventType getType() {
                return {"meta_event", "connect"};
            }
            uint64_t time; // 事件产生的时间
            uint64_t self_id; // 机器人自身QQ

            nlohmann::json raw_msg;
        };

        struct GroupUploadNotice {
            static constexpr EventType getType() {
                return {"notice", "group_upload"};
            }

            uint64_t time; // 事件产生的时间
            uint64_t self_id; // 机器人自身QQ
            uint64_t group_id; // 群QQ
            uint64_t user_id; // 上传文件的人的QQ
            nlohmann::json file; // 上传的文件信息,日后再进一步解析

            nlohmann::json raw_msg;
        };

        struct GroupAdminNotice {
            static constexpr EventType getType() {
                return {"notice", "group_admin"};
            }

            uint64_t time; // 事件产生的时间
            uint64_t self_id; // 机器人自身QQ
            uint64_t group_id; // 群QQ
            uint64_t user_id; // 管理员的QQ
            enum SUB_TYPE {
                SET,
                UNSET,
            } sub_type; // 事件子类型，分别表示设置和取消设置

            nlohmann::json raw_msg;
        };

        struct GroupDecreaseNotice {
            static constexpr EventType getType() {
                return {"notice", "group_decrease"};
            }

            uint64_t time; // 事件产生的时间
            uint64_t self_id; // 机器人自身QQ
            uint64_t group_id; // 群QQ
            uint64_t user_id; // 用户QQ
            uint64_t operator_id; // 操作者QQ 如果是主动退群，和user_id一致
            enum SUB_TYPE {
                LEAVE,      // 退出
                KICK,       // 被踢出
                KICK_ME,    // 机器人被踢出
            } sub_type; // 事件子类型，分别表示主动退群、成员被踢、登录号被踢

            nlohmann::json raw_msg;
        };

        struct GroupInceaseNotice {
            static constexpr EventType getType() {
                return {"notice", "group_increase"};
            }
            uint64_t time; // 事件产生的时间
            uint64_t self_id; // 机器人自身QQ
            uint64_t group_id; // 群QQ
            uint64_t user_id; // 用户QQ
            uint64_t operator_id; // 操作者QQ 如果是主动加群，和user_id一致
            enum SUB_TYPE {
                APPROVE, // 同意入群
                INVITE,  // 邀请入群
            } sub_type; // 事件子类型，分别表示管理员已同意入群、管理员邀请入群

            nlohmann::json raw_msg;
        };

        struct GroupBanNotice {
            static constexpr EventType getType() {
                return {"notice", "group_ban"};
            }
            uint64_t time; // 事件产生的时间
            uint64_t self_id; // 机器人自身QQ
            uint64_t group_id; // 群QQ
            uint64_t user_id; // 被禁言的人的QQ
            uint64_t operator_id; // 操作者QQ 如果是主动禁言，和user_id一致
            uint64_t duration; //禁言时长，单位秒
            enum SUB_TYPE {
                BAN, // 禁言
                LIFT_BAN, // 解除禁言
            } sub_type; // 事件子类型，分别表示禁言、解除禁言

            nlohmann::json raw_msg;
        };

        struct FriendAddNotice {
            static constexpr EventType getType() {
                return {"notice", "friend_add"};
            }
            uint64_t time; // 事件产生的时间
            uint64_t self_id; // 机器人自身QQ
            uint64_t user_id; // 新添加好友 QQ 号

            nlohmann::json raw_msg;
        };

        // 群消息撤回事件
        struct GroupRecallNotice {
            static constexpr EventType getType() {
                return {"notice", "group_recall"};
            }
            uint64_t time; // 事件产生的时间
            uint64_t self_id; // 机器人自身QQ
            uint64_t group_id; // 群QQ
            uint64_t message_id; // 消息ID
            uint64_t user_id; // 发送者QQ
            uint64_t operator_id; // 操作者QQ
        

            nlohmann::json raw_msg;
        };

        // 好友消息撤回事件
        struct FriendRecallNotice {
            static constexpr EventType getType() {
                return {"notice", "friend_recall"};
            }
            uint64_t time; // 事件产生的时间
            uint64_t self_id; // 机器人自身QQ
            uint64_t user_id; // 发送者QQ
            uint64_t message_id; // 消息ID

            nlohmann::json raw_msg;
        };

        // 群内通知事件，如戳一戳、群红包运气王、群成员荣誉变更
        struct GroupNotifyNotice {
            static constexpr EventType getType() {
                return {"notice", "group_notify"};
            }
            uint64_t time; // 事件产生的时间
            uint64_t self_id; // 机器人自身QQ
            uint64_t group_id; // 群QQ
            uint64_t user_id; // 发送者QQ,如戳一戳的发送者，红包的发送者，荣誉变更者
            enum SUB_TYPE {
                POKE, //戳一戳
                LUCKY_KING, //群红包运气王
                HONOR, //群成员荣誉变更
            } sub_type; // 事件子类型，分别表示戳一戳、群红包运气王、群成员荣誉变更
            std::optional<uint64_t> target_id = std::nullopt; // 如果是戳一戳，则为被戳的人的QQ，如果是群红包运气王，则为群红包的ID
            enum HonorType {
                TALKATIVE, // 龙王
                PERFORMER, // 群聊之火
                EMOTION,   // 快乐源泉
            };
            std::optional<HonorType> honor_type = std::nullopt; // 荣誉类型

            nlohmann::json raw_msg;
        };

        using Variant = std::variant<
            ConnectEvent,
            DisableEvent,
            EnableEvent,
            FriendAddNotice,
            FriendRecallNotice,
            GroupAdminNotice,
            GroupBanNotice,
            GroupDecreaseNotice,
            GroupInceaseNotice,
            GroupMsg,
            GroupNotifyNotice,
            GroupRecallNotice,
            GroupUploadNotice,
            PrivateMsg
        >;

        std::optional<Variant> construct(const EventType& evnet);
    }

    /// BotInstance是一个机器人实例，机器人实例必须通过BotInstance::createInstance()创建
    /// 因为采用了unique_ptr，所以必须通过std::move传递，可以99.99999%避免内存泄漏
    struct BotInstance{
        // 消息类型
        // 消息回调函数原型
		using Callback = std::function<void(const Event::Variant&)>;


        // 创建机器人实例
        static std::unique_ptr<BotInstance> createInstance(const Config &config);
        
        // 获取Api集合
		ApiSet getApiSet(const uint64_t& id, const ApiSet::AsyncMode& mode = { false });

		ApiSet getApiSet(const ApiSet::SyncMode& mode = { true });
        
        // 注册事件监听器
        template<Event::Concept E>
		void onEvent(std::function<void(const E&)> callback);

        // [阻塞] 启动机器人
        void start();

        ~BotInstance() = default;
    protected:
        Config config;
        std::unordered_map<EventType, Callback> event_callbacks{};
    protected:
        explicit BotInstance(const Config &config);

        friend std::default_delete<BotInstance>;
    };

    // 存放私有的东西，防止编译器优化掉模板特化
    namespace _{
        // 仅仅为了导出代码，不要当真，更不要去调用！！
        void export_functions();
    };
};
