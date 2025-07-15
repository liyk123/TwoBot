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
#include <tbb/tbb.h>
#include <BS_thread_pool.hpp>
#include "jsonex.hh"

namespace twobot {
	using PromMapType = tbb::concurrent_hash_map<std::size_t, std::promise<ApiSet::SyncResult>>;
	extern PromMapType g_promMap = {};
	using SessionMapType = tbb::concurrent_unordered_map<uint64_t, brynet::net::http::HttpSession::Ptr>;
	extern SessionMapType g_sessionMap = {};

	std::unique_ptr<BotInstance> BotInstance::createInstance(const Config& config) {
		return std::unique_ptr<BotInstance>(new BotInstance{config} );
	}

	ApiSet BotInstance::getApiSet(const uint64_t& id, const ApiSet::AsyncMode& mode) {
		return { ApiSet::AsyncConfig{id}, mode };
	}

	ApiSet BotInstance::getApiSet(const ApiSet::SyncMode& mode)
	{
		return { ApiSet::SyncConfig{config.host,config.api_port,config.token}, mode };
	}

	BotInstance::BotInstance(const Config& config) 
		: config(config)
	{

	}

	template<Event::Concept E>
	void BotInstance::onEvent(std::function<void(const E&)> callback) {
		this->event_callbacks[E::getType()] = Callback([callback](const Event::Variant& event) {
			try {
				callback(*std::get_if<E>(&event));
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
		BS::thread_pool pool;
		auto service = IOThreadTcpService::Create();
		service->startWorkerThread(1);

		auto ws_enter_callback = [this, &pool](const HttpSession::Ptr& httpSession,
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
							PromMapType::accessor acc;
							g_promMap.find(acc, seq);
							acc->second.set_value({ !data.is_null(), data });
							g_promMap.erase(acc);
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

					std::visit([&payload, httpSession](auto&& e) { 
						e.raw_msg = nlohmann::json::parse(payload); 
						e.raw_msg.get_to(e);
						if constexpr (std::is_convertible_v<decltype(e), Event::ConnectEvent>)
						{
							g_sessionMap[e.self_id] = httpSession;
						}
					}, *event);

					if (event_callbacks.count(event_type) != 0) {
						pool.detach_task([this, l_event = std::move(event)] {
							std::visit([this](auto&& e) {
								event_callbacks[e.getType()](e);
							}, *l_event);
						});
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

		while (getchar() != EOF)
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}

		pool.wait();
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
		instance->onEvent<Event::GroupMsg>([](const auto&) {});
		instance->onEvent<Event::PrivateMsg>([](const auto&) {});
		instance->onEvent<Event::EnableEvent>([](const auto&) {});
		instance->onEvent<Event::DisableEvent>([](const auto&) {});
		instance->onEvent<Event::ConnectEvent>([](const auto&) {});
		instance->onEvent<Event::GroupUploadNotice>([](const auto&) {});
		instance->onEvent<Event::GroupAdminNotice>([](const auto&) {});
		instance->onEvent<Event::GroupDecreaseNotice>([](const auto&) {});
		instance->onEvent<Event::GroupInceaseNotice>([](const auto&) {});
		instance->onEvent<Event::GroupBanNotice>([](const auto&) {});
		instance->onEvent<Event::FriendAddNotice>([](const auto&) {});
		instance->onEvent<Event::GroupRecallNotice>([](const auto&) {});
		instance->onEvent<Event::FriendRecallNotice>([](const auto&) {});
		instance->onEvent<Event::GroupNotifyNotice>([](const auto&) {});
	}
};
