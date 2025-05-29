#include "twobot.hh"
#include <nlohmann/json.hpp>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#endif

#include <httplib.h>
#include <brynet/base/Packet.hpp>
#include <brynet/net/http/WebSocketFormat.hpp>
#include <brynet/net/wrapper/HttpServiceBuilder.hpp>
#include <brynet/net/wrapper/ServiceBuilder.hpp>
#include <brynet/base/AppStatus.hpp>

namespace twobot {
	std::unique_ptr<BotInstance> BotInstance::createInstance(const Config& config) {
		return std::unique_ptr<BotInstance>(new BotInstance{config} );
	}

	ApiSet BotInstance::getApiSet(void *session, const bool& isPost) {
		return {config, session, isPost};
	}

	ApiSet BotInstance::getApiSet(void *session)
	{
		return getApiSet(session, false);
	}

	ApiSet BotInstance::getApiSet(const bool& isPost)
	{
		return getApiSet(nullptr, isPost);
	}

	BotInstance::BotInstance(const Config& config) 
		: config(config)
		, m_seqMap()
	{

	}

	template<class EventType>
	void BotInstance::onEvent(std::function<void(const EventType&, void *)> callback) {
		EventType event{};
		this->event_callbacks[event.getType()] = Callback([callback](const Event::EventBase& event, void *session) {
			try{
				callback(static_cast<const EventType&>(event), session);
			}catch(const std::exception &e){
				const auto & eventType = event.getType();
				std::cerr << "EventType: {" << eventType.post_type << ", " << eventType.sub_type << "}\n";
				std::cerr << "\tBotInstance::onEvent error: " << e.what() << std::endl;
			}
		});
	}

	void BotInstance::start() {
		using namespace brynet::base;
		using namespace brynet::net;
		using namespace brynet::net::http;
		auto websocket_port = config.ws_port;
		auto service = IOThreadTcpService::Create();
		service->startWorkerThread(1);

		auto ws_enter_callback = [this](const HttpSession::Ptr& httpSession,
			WebSocketFormat::WebSocketFrameType opcode,
			const std::string& payload) {
				try {
					nlohmann::json json_payload = nlohmann::json::parse(payload);
					std::string post_type;
					std::string sub_type;

					// 忽略心跳包
					if(json_payload.contains("meta_event_type"))
						if(json_payload["meta_event_type"] == "heartbeat")
							return;

					if (!json_payload.contains("post_type"))
					{
						if (json_payload.contains("echo") && json_payload["echo"].contains("seq")) 
						{
							std::size_t seq = json_payload["echo"]["seq"].get<std::size_t>();
							m_seqMap.emplace(seq, json_payload["data"]);
						}
						return;
					}

					post_type = (std::string)json_payload["post_type"];

					if (post_type == "message")
						sub_type = (std::string)json_payload["message_type"];
					else if (post_type == "meta_event")
						sub_type = (std::string)json_payload["sub_type"];
					else if (post_type == "notice")
						sub_type = (std::string)json_payload["notice_type"];
					

					EventType event_type = {
						post_type,
						sub_type
					};

					auto event = Event::EventBase::construct(event_type);
					if (!event)
						return;
					event->raw_msg = nlohmann::json::parse(payload);
					event->parse();

					if (event_callbacks.count(event_type) != 0) {
						event_callbacks[event_type](
							reinterpret_cast<const Event::EventBase&>(
								*event
							),
							httpSession.get()
						);
					}
				}
				catch (const std::exception& e) {
					std::cerr << "WebSocket CallBack Exception: " << e.what() << std::endl;
				}

		};

		wrapper::HttpListenerBuilder listener_builder;
		listener_builder
			.WithService(service)
			.AddSocketProcess([](TcpSocket& socket) {
			socket.setNodelay();
				})
			.WithMaxRecvBufferSize(static_cast<size_t>(1024 * 1024 * 4))
			.WithAddr(false, "0.0.0.0", websocket_port)
			.WithEnterCallback([ws_enter_callback](const HttpSession::Ptr& httpSession, HttpSessionHandlers& handlers) {
				handlers.setWSCallback(ws_enter_callback);
				})
            .WithReusePort()
			.asyncRun()
			;
				
		while (!brynet::base::app_kbhit())
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}

	nlohmann::json BotInstance::getApiResult(const std::size_t& seq)
	{
		std::optional<nlohmann::json> ret;
		while ((ret = m_seqMap.find(seq)) == std::nullopt)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		m_seqMap.erase(seq);
		return *ret;
	}


	void _::export_functions() {
		// 仅仅为了导出代码，不要当真，更不要去调用！！
		auto instance = BotInstance::createInstance({
			"http://localhost",
			8080,
			8081,
			std::nullopt
		});

		// 仅仅为了特化onEvent模板
		instance->onEvent<Event::GroupMsg>([](const auto&, const auto&) {});
		instance->onEvent<Event::PrivateMsg>([](const auto&, const auto&) {});
		instance->onEvent<Event::EnableEvent>([](const auto&, const auto&) {});
		instance->onEvent<Event::DisableEvent>([](const auto&, const auto&) {});
		instance->onEvent<Event::ConnectEvent>([](const auto&, const auto&) {});
		instance->onEvent<Event::GroupUploadNotice>([](const auto&, const auto&) {});
		instance->onEvent<Event::GroupAdminNotice>([](const auto&, const auto&) {});
		instance->onEvent<Event::GroupDecreaseNotice>([](const auto&, const auto&) {});
		instance->onEvent<Event::GroupInceaseNotice>([](const auto&, const auto&) {});
		instance->onEvent<Event::GroupBanNotice>([](const auto&, const auto&) {});
		instance->onEvent<Event::FriendAddNotice>([](const auto&, const auto&) {});
		instance->onEvent<Event::GroupRecallNotice>([](const auto&, const auto&) {});
		instance->onEvent<Event::FriendRecallNotice>([](const auto&, const auto&) {});
		instance->onEvent<Event::GroupNotifyNotice>([](const auto&, const auto&) {});
	}

	std::unique_ptr<Event::EventBase> Event::EventBase::construct(const EventType& event) {
		if (event.post_type == "message") {
			if (event.sub_type == "group") {
				return std::unique_ptr<Event::EventBase>(new Event::GroupMsg());
			} else if (event.sub_type == "private") {
				return std::unique_ptr<Event::EventBase>(new Event::PrivateMsg());
			}
		} else if (event.post_type == "meta_event") {
			if(event.sub_type == "enable"){
				return std::unique_ptr<Event::EventBase>(new Event::EnableEvent());
			}else if(event.sub_type == "disable"){
				return std::unique_ptr<Event::EventBase>(new Event::DisableEvent());
			}else if(event.sub_type == "connect"){
				return std::unique_ptr<Event::EventBase>(new Event::ConnectEvent());
			}
		} else if(event.post_type == "notice"){
			if(event.sub_type == "group_upload"){
				return std::unique_ptr<Event::EventBase>(new Event::GroupUploadNotice());
			} else if (event.sub_type == "group_admin"){
				return std::unique_ptr<Event::EventBase>(new Event::GroupAdminNotice());
			} else if (event.sub_type == "group_decrease"){
				return std::unique_ptr<Event::EventBase>(new Event::GroupDecreaseNotice());
			} else if (event.sub_type == "group_increase"){
				return std::unique_ptr<Event::EventBase>(new Event::GroupInceaseNotice());
			} else if (event.sub_type == "group_ban"){
				return std::unique_ptr<Event::EventBase>( new Event::GroupBanNotice());
			} else if (event.sub_type == "friend_add"){
				return std::unique_ptr<Event::EventBase>(new Event::FriendAddNotice());
			} else if (event.sub_type == "group_recall"){
				return std::unique_ptr<Event::EventBase>(new Event::GroupRecallNotice());
			} else if (event.sub_type == "friend_recall"){
				return std::unique_ptr<Event::EventBase>(new Event::FriendRecallNotice());
			} else if (event.sub_type == "group_notify"){
				return std::unique_ptr<Event::EventBase>(new Event::GroupNotifyNotice());
			}
		}
		return nullptr;
	}

	void Event::GroupMsg::parse() {
		raw_msg.get_to(*this);
	}

	void Event::PrivateMsg::parse() {
		raw_msg.get_to(*this);
	}

	void Event::EnableEvent::parse(){
		raw_msg.get_to(*this);
	}

	void Event::DisableEvent::parse(){
		raw_msg.get_to(*this);
	}

	void Event::ConnectEvent::parse(){
		raw_msg.get_to(*this);
	}

	void Event::GroupUploadNotice::parse(){
		raw_msg.get_to(*this);
	}

	void Event::GroupAdminNotice::parse(){
		raw_msg.get_to(*this);
	}

	void Event::GroupDecreaseNotice::parse(){
		raw_msg.get_to(*this);
	}

	void Event::GroupInceaseNotice::parse(){
		raw_msg.get_to(*this);
	}

	void Event::GroupBanNotice::parse(){
		raw_msg.get_to(*this);
	}

	void Event::FriendAddNotice::parse(){
		raw_msg.get_to(*this);
	}

	void Event::FriendRecallNotice::parse(){
		raw_msg.get_to(*this);
	}

	void Event::GroupRecallNotice::parse(){
		raw_msg.get_to(*this);
	}

	void Event::GroupNotifyNotice::parse(){
		raw_msg.get_to(*this);
	}
};
