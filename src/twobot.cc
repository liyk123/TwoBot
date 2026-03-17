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
#include "jsonex.hh"

namespace twobot {
	using ApiResultMapType = tbb::concurrent_hash_map<std::size_t, ApiSet::SyncResult>;
	extern ApiResultMapType g_apiResultMap = {};
	using SessionMapType = tbb::concurrent_unordered_map<uint64_t, brynet::net::http::HttpSession::Ptr>;
	extern SessionMapType g_sessionMap = {};
	using ApiEventMapType = tbb::concurrent_hash_map<std::size_t, std::shared_ptr<coro::event>>;
	extern ApiEventMapType g_apiEventMap = {};

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
	void BotInstance::onEvent(std::function<coro::task<>(const E&)> callback) {
		event_callbacks.emplace(E::getType(), Callback([callback](const Event::Variant& event) -> coro::task<> {
			try {
				co_await callback(*std::get_if<E>(&event));
			}
			catch (const std::exception& e) {
				const auto& eventType = E::getType();
				std::cerr << "EventType: {" << eventType.post_type << ", " << eventType.sub_type << "}\n";
				std::cerr << "\tBotInstance::onEvent error: " << e.what() << std::endl;
			}
			co_return;
		}));
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
						if (json_payload["echo"]["seq"].is_number_integer()) 
						{
							auto seq = json_payload["echo"]["seq"].get<std::size_t>();
							const auto& data = json_payload["data"];
							{
								ApiResultMapType::accessor acc;
								g_apiResultMap.find(acc, seq);
								acc->second = { !data.is_null(), data };
							}
							{
								ApiEventMapType::accessor acc;
								g_apiEventMap.find(acc, seq);
								acc->second->set();
								g_apiEventMap.erase(acc);
							}
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

					auto it = event_callbacks.find(event_type);
					if (it != event_callbacks.end()) {
						auto coroTask = [](Callback callback, Event::Variant e) -> coro::task<> {
							co_await coro::default_executor::executor()->schedule();
							co_await callback(e);
							co_return;
						};
						coro::default_executor::executor()->spawn(coroTask(it->second, *event));
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

		std::promise<void>().get_future().wait();
	}

	template<typename... T>
	concept VariadicConcept = (Event::Concept<T> && ...);

	template<VariadicConcept... T>
	inline void _construct_call(const EventType& event, std::optional<Event::Variant>& obj)
	{
        ([&] { return T::getType() == event ? (obj.emplace(std::in_place_type<T>), true) : false; }() || ...);
	}

	std::optional<Event::Variant> Event::construct(const EventType& event) {
		std::optional<Event::Variant> obj;
		_construct_call<
			GroupMsg,
			PrivateMsg,
			ConnectEvent,
			DisableEvent,
			EnableEvent,
			FriendAddNotice,
			FriendRecallNotice,
			GroupAdminNotice,
			GroupBanNotice,
			GroupDecreaseNotice,
			GroupInceaseNotice,
			GroupNotifyNotice,
			GroupRecallNotice,
			GroupUploadNotice
		>(event, obj);
		return obj;
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
		instance->onEvent<Event::GroupMsg>([](const auto&) -> coro::task<> { co_return; });
		instance->onEvent<Event::PrivateMsg>([](const auto&) -> coro::task<> { co_return; });
		instance->onEvent<Event::EnableEvent>([](const auto&) -> coro::task<> { co_return; });
		instance->onEvent<Event::DisableEvent>([](const auto&) -> coro::task<> { co_return; });
		instance->onEvent<Event::ConnectEvent>([](const auto&) -> coro::task<> { co_return; });
		instance->onEvent<Event::GroupUploadNotice>([](const auto&) -> coro::task<> { co_return; });
		instance->onEvent<Event::GroupAdminNotice>([](const auto&) -> coro::task<> { co_return; });
		instance->onEvent<Event::GroupDecreaseNotice>([](const auto&) -> coro::task<> { co_return; });
		instance->onEvent<Event::GroupInceaseNotice>([](const auto&) -> coro::task<> { co_return; });
		instance->onEvent<Event::GroupBanNotice>([](const auto&) -> coro::task<> { co_return; });
		instance->onEvent<Event::FriendAddNotice>([](const auto&) -> coro::task<> { co_return; });
		instance->onEvent<Event::GroupRecallNotice>([](const auto&) -> coro::task<> { co_return; });
		instance->onEvent<Event::FriendRecallNotice>([](const auto&) -> coro::task<> { co_return; });
		instance->onEvent<Event::GroupNotifyNotice>([](const auto&) -> coro::task<> { co_return; });
	}
};
