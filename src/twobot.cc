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
#include "jsonex.hh"

namespace twobot {
	extern std::unordered_map<std::size_t, std::promise<ApiSet::SyncApiResult>> g_promMap = {};

	std::unique_ptr<BotInstance> BotInstance::createInstance(const Config& config) {
		return std::unique_ptr<BotInstance>(new BotInstance{config} );
	}

	ApiSet BotInstance::getApiSet(const std::any& session, const bool& isPost) {
		return {config, session, isPost};
	}

	ApiSet BotInstance::getApiSet(const std::any& session)
	{
		return getApiSet(session, false);
	}

	ApiSet BotInstance::getApiSet(const bool& isPost)
	{
		return getApiSet({}, isPost);
	}

	BotInstance::BotInstance(const Config& config) 
		: config(config)
	{

	}

	template<Event::Concept E>
	void BotInstance::onEvent(std::function<void(const E&, const std::any&)> callback) {
		this->event_callbacks[E::getType()] = Callback([callback](const Event::Variant& event, const std::any& session) {
			try {
				callback(*std::get_if<E>(&event), session);
			}
			catch (const std::exception& e) {
				const auto& eventType = E::getType();
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

		std::deque<std::future<void>> tasks;

		auto ws_enter_callback = [this, &tasks](const HttpSession::Ptr& httpSession,
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
						if (json_payload["echo"]["seq"].is_number_integer()) 
						{
							auto seq = json_payload["echo"]["seq"].get<std::size_t>();
							const auto& data = json_payload["data"];
							g_promMap[seq].set_value({ !data.is_null(), data });
							g_promMap.erase(seq);
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

					auto event = Event::construct(event_type);
					if (!event.has_value())
						return;

					std::visit([&payload](auto&& e) { 
						e.raw_msg = nlohmann::json::parse(payload); 
						e.raw_msg.get_to(e); 
					}, *event);

					if (event_callbacks.count(event_type) != 0) {
						auto task = std::async([this, l_event = std::move(event), httpSession] {
							std::visit([this, httpSession](auto &&e) {
								event_callbacks[e.getType()](e, httpSession);
							}, *l_event);
						});
						tasks.push_back(std::move(task));
					}
				}
				catch (const std::exception& e) {
					std::cerr << "WebSocket CallBack Exception: " << e.what() << std::endl;
				}

		};

		auto httpHeaderCallback = [this](const HTTPParser& httpParser, const HttpSession::Ptr& httpSession) {
			if (config.token && config.token != httpParser.getValue("Authorization").substr(sizeof("Bearer ")-1))
			{
				std::cerr << "Authorization failed!" << std::endl;
				httpSession->postClose();
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
			.WithEnterCallback([ws_enter_callback, httpHeaderCallback](const HttpSession::Ptr& httpSession, HttpSessionHandlers& handlers) {
				handlers.setHeaderCallback(httpHeaderCallback);
				handlers.setWSCallback(ws_enter_callback);
				})
            .WithReusePort()
			.asyncRun()
			;

		std::jthread watcher([&tasks](std::stop_token stopToken) {
			while (!stopToken.stop_requested())
			{
				if (!tasks.empty())
				{
					tasks.front().wait();
					tasks.pop_front();
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		});

		while (getchar() != EOF)
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}

		watcher.request_stop();
	}

	template<Event::Concept T>
	inline auto _construct_pair() -> std::pair<EventType, std::function<Event::Variant()>>
	{
		return { T::getType(), [] {return T(); } };
	}

	std::optional<Event::Variant> Event::construct(const EventType& event) {
		static const std::unordered_map<EventType, std::function<Event::Variant()>> constructs = {
			_construct_pair<ConnectEvent>(),
			_construct_pair<DisableEvent>(),
			_construct_pair<EnableEvent>(),
			_construct_pair<FriendAddNotice>(),
			_construct_pair<FriendRecallNotice>(),
			_construct_pair<GroupAdminNotice>(),
			_construct_pair<GroupBanNotice>(),
			_construct_pair<GroupDecreaseNotice>(),
			_construct_pair<GroupInceaseNotice>(),
			_construct_pair<GroupMsg>(),
			_construct_pair<GroupNotifyNotice>(),
			_construct_pair<GroupRecallNotice>(),
			_construct_pair<GroupUploadNotice>(),
			_construct_pair<PrivateMsg>()
		};
		auto itRet = constructs.find(event);
		if (itRet != constructs.end())
		{
			return itRet->second();
		}
		return std::nullopt;
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
		instance->onEvent<Event::GroupMsg>([](const auto&, const std::any&) {});
		instance->onEvent<Event::PrivateMsg>([](const auto&, const std::any&) {});
		instance->onEvent<Event::EnableEvent>([](const auto&, const std::any&) {});
		instance->onEvent<Event::DisableEvent>([](const auto&, const std::any&) {});
		instance->onEvent<Event::ConnectEvent>([](const auto&, const std::any&) {});
		instance->onEvent<Event::GroupUploadNotice>([](const auto&, const std::any&) {});
		instance->onEvent<Event::GroupAdminNotice>([](const auto&, const std::any&) {});
		instance->onEvent<Event::GroupDecreaseNotice>([](const auto&, const std::any&) {});
		instance->onEvent<Event::GroupInceaseNotice>([](const auto&, const std::any&) {});
		instance->onEvent<Event::GroupBanNotice>([](const auto&, const std::any&) {});
		instance->onEvent<Event::FriendAddNotice>([](const auto&, const std::any&) {});
		instance->onEvent<Event::GroupRecallNotice>([](const auto&, const std::any&) {});
		instance->onEvent<Event::FriendRecallNotice>([](const auto&, const std::any&) {});
		instance->onEvent<Event::GroupNotifyNotice>([](const auto&, const std::any&) {});
	}
};
