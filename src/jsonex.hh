#pragma once
#include "twobot.hh"

namespace nlohmann {
    template <typename T>
    struct adl_serializer<std::optional<T>> {
        static void to_json(json& j, const std::optional<T>& opt) {
            if (opt == std::nullopt) {
                j = nullptr;
            }
            else {
                j = *opt; // this will call adl_serializer<T>::to_json which will
                // find the free function to_json in T's namespace!
            }
        }

        static void from_json(const json& j, std::optional<T>& opt) {
            if (j.is_null()) {
                opt = std::nullopt;
            }
            else {
                opt = j.template get<T>(); // same as above, but with
                // adl_serializer<T>::from_json
            }
        }
    };
}

namespace twobot {
    namespace Event {
        NLOHMANN_JSON_SERIALIZE_ENUM(PrivateMsg::SUB_TYPE, {
            {PrivateMsg::SUB_TYPE::FRIEND, "friend"},
            {PrivateMsg::SUB_TYPE::GROUP, "group"},
            {PrivateMsg::SUB_TYPE::OTHER, "other"},
        })

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(PrivateMsg, time, user_id, self_id, raw_message, sub_type, sender)

        NLOHMANN_JSON_SERIALIZE_ENUM(GroupMsg::SUB_TYPE, {
            {GroupMsg::SUB_TYPE::NORMAL, "normal"},
            {GroupMsg::SUB_TYPE::ANONYMOUS, "anonymous"},
            {GroupMsg::SUB_TYPE::NOTICE, "notice"},
        })

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GroupMsg, time, user_id, self_id, group_id, raw_message, group_name, sub_type, sender)

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EnableEvent, time, self_id)

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(DisableEvent, time, self_id)

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ConnectEvent, time, self_id)

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GroupUploadNotice, time, user_id, self_id, group_id, file)

        NLOHMANN_JSON_SERIALIZE_ENUM(GroupAdminNotice::SUB_TYPE, {
            {GroupAdminNotice::SUB_TYPE::SET, "set"},
            {GroupAdminNotice::SUB_TYPE::UNSET, "unset"},
        })

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GroupAdminNotice, time, user_id, self_id, group_id, sub_type)

        NLOHMANN_JSON_SERIALIZE_ENUM(GroupDecreaseNotice::SUB_TYPE, {
            {GroupDecreaseNotice::SUB_TYPE::LEAVE, "leave"},
            {GroupDecreaseNotice::SUB_TYPE::KICK, "kick"},
            {GroupDecreaseNotice::SUB_TYPE::KICK_ME, "kick_me"}
        })

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GroupDecreaseNotice, time, user_id, self_id, group_id, operator_id, sub_type)

        NLOHMANN_JSON_SERIALIZE_ENUM(GroupInceaseNotice::SUB_TYPE, {
            {GroupInceaseNotice::SUB_TYPE::APPROVE, "approve"},
            {GroupInceaseNotice::SUB_TYPE::INVITE, "invite"}
        })

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GroupInceaseNotice, time, user_id, self_id, group_id, operator_id, sub_type)

        NLOHMANN_JSON_SERIALIZE_ENUM(GroupBanNotice::SUB_TYPE, {
            {GroupBanNotice::SUB_TYPE::BAN, "ban"},
            {GroupBanNotice::SUB_TYPE::LIFT_BAN, "lift_ban"}
        })

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GroupBanNotice, time, user_id, self_id, group_id, operator_id, duration, sub_type)

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(FriendAddNotice, time, user_id, self_id)

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GroupRecallNotice, time, user_id, self_id, group_id, message_id, operator_id)

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(FriendRecallNotice, time, user_id, self_id, message_id)

        NLOHMANN_JSON_SERIALIZE_ENUM(GroupNotifyNotice::SUB_TYPE, {
            {GroupNotifyNotice::SUB_TYPE::POKE, "poke"},
            {GroupNotifyNotice::SUB_TYPE::LUCKY_KING, "lucky_king"},
            {GroupNotifyNotice::SUB_TYPE::HONOR, "honor"}
        })

        NLOHMANN_JSON_SERIALIZE_ENUM(GroupNotifyNotice::HonorType, {
            {GroupNotifyNotice::HonorType::TALKATIVE, "talkative"},
            {GroupNotifyNotice::HonorType::PERFORMER, "performer"},
            {GroupNotifyNotice::HonorType::EMOTION, "emotion"}
        })

        NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GroupNotifyNotice, time, user_id, self_id, group_id, sub_type, target_id, honor_type)
    }
}