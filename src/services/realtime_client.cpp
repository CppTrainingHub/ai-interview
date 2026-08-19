#include "services/realtime_client.h"
#include "common/logger.h"
#include "common/config.h"
#include "common/utils.h"
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <nlohmann/json.hpp>
#include <thread>
#include <random>
#include <sstream>
#include <iomanip>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

namespace interview {
namespace services {

class RealtimeClient::RealtimeClientImpl {
public:
    RealtimeClientImpl(const std::string& url, const std::map<std::string, std::string>& headers)
        : url_(url)
        , headers_(headers)
        , ioc_()
        , ssl_ctx_(ssl::context::tlsv12_client)
        , resolver_(ioc_)
        , ws_(nullptr)
        , connected_(false)
        , running_(false) {

        // 配置SSL上下文
        ssl_ctx_.set_default_verify_paths();
        ssl_ctx_.set_verify_mode(ssl::verify_none); // 生产环境应该验证证书

        ParseUrl();
    }

    ~RealtimeClientImpl() {
        Close();
    }

    void Connect() {
        if (connected_) {
            LOG_WARNING("Already connected");
            return;
        }

        try {
            // 解析主机和端口
            auto const results = resolver_.resolve(host_, port_);

            // 创建WebSocket SSL流
            ws_ = std::make_unique<websocket::stream<beast::ssl_stream<tcp::socket>>>(ioc_, ssl_ctx_);

            // 设置SNI主机名
            if (!SSL_set_tlsext_host_name(ws_->next_layer().native_handle(), host_.c_str())) {
                throw std::runtime_error("Failed to set SNI hostname");
            }

            // 连接到服务器
            net::connect(ws_->next_layer().next_layer(), results.begin(), results.end());

            // SSL握手
            ws_->next_layer().handshake(ssl::stream_base::client);

            // 设置WebSocket选项
            ws_->set_option(websocket::stream_base::decorator(
                [this](websocket::request_type& req) {
                    // 添加自定义头
                    for (const auto& [key, value] : headers_) {
                        req.set(key, value);
                    }
                    // 设置User-Agent，模仿Python websockets
                    req.set(boost::beast::http::field::user_agent, "Python/3.7 websockets/10.0");
                }
            ));

            // 设置为二进制模式
            ws_->binary(true);

            // 禁用自动分帧 - 确保消息作为单个帧发送
            ws_->auto_fragment(false);

            // 设置较大的写缓冲区
            ws_->write_buffer_bytes(16384);

            // 关键修复：禁用WebSocket控制帧超时（模仿Python的ping_interval=None）
            // 这可以防止在长时间静默后出现SSL/TLS错误
            ws_->control_callback([](websocket::frame_type kind, beast::string_view payload) {
                // 静默处理ping/pong，不做任何操作
                boost::ignore_unused(kind, payload);
            });

            // 禁用idle超时（Boost.Beast默认没有idle timeout，但明确设置以确保）
            ws_->set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

            // 设置超时为无限（0表示无超时）
            auto timeout_opt = websocket::stream_base::timeout();
            timeout_opt.idle_timeout = std::chrono::seconds(0);  // 禁用idle超时
            timeout_opt.handshake_timeout = std::chrono::seconds(30);  // 握手超时保持合理值
            timeout_opt.keep_alive_pings = false;  // 禁用自动ping
            ws_->set_option(timeout_opt);

            // WebSocket握手
            ws_->handshake(host_, path_);

            connected_ = true;
            LOG_INFO("Connected to ", url_);

            // 发送StartConnection请求
            SendStartConnection();

            // 发送StartSession请求
            SendStartSession();

            // 启动接收线程处理后续响应
            StartReceiveThread();

        } catch (const std::exception& e) {
            connected_ = false;
            throw std::runtime_error(std::string("Failed to connect: ") + e.what());
        }
    }

    void SendAudioData(const std::vector<uint8_t>& audio) {
        if (!connected_) {
            throw std::runtime_error("Not connected");
        }

        try {
            // 构建TaskRequest (event=TASK_REQUEST)
            auto message = common::Protocol::BuildClientAudioRequest(
                common::events::TASK_REQUEST,
                session_id_,
                audio
            );

            // 直接写入vector数据
            ws_->write(net::buffer(message));

        } catch (const std::exception& e) {
            LOG_ERROR("Failed to send audio: ", e.what());
            throw;
        }
    }

    void SendTextQuery(const std::string& text) {
        if (!connected_) {
            throw std::runtime_error("Not connected");
        }

        std::string prompt = R"(直接朗读下面文字：)" + text + R"(后续对应于面试者的回答都只回复[好的，我们继续],不要承接或评论)";

        try {
            nlohmann::json payload;
            payload["content"] = prompt;

            // 构建ChatTextQuery请求 (event=CHAT_TEXT_QUERY)
            auto message = common::Protocol::BuildFullRequest(
                common::events::CHAT_TEXT_QUERY,
                session_id_,
                payload
            );

            // 直接写入vector数据
            ws_->write(net::buffer(message));

            LOG_INFO("Sent prompt query: {}", prompt);

        } catch (const std::exception& e) {
            LOG_ERROR("Failed to send text query: {}", e.what());
            throw;
        }
    }

    common::ParsedResponse ReceiveResponse() {
        if (!connected_) {
            throw std::runtime_error("Not connected");
        }

        try {
            beast::flat_buffer buffer;
            ws_->read(buffer); // 这里会阻塞，等待数据

            // 将buffer数据转换为vector
            std::vector<uint8_t> data(buffer.size());
            net::buffer_copy(net::buffer(data), buffer.data());

            return common::Protocol::ParseResponse(data);

        } catch (const std::exception& e) {
            LOG_ERROR("Failed to receive response: ", e.what());
            throw;
        }
    }

    void SetResponseCallback(ResponseCallback callback) {
        callback_ = callback;
    }

    void StartReceiveThread() {
        if (callback_ && !running_) {
            // 启动接收线程
            running_ = true;
            receive_thread_ = std::thread([this]() {
                ReceiveLoop();
            });
            LOG_INFO("Receive thread started");
        }
    }

    void Close() {
        if (!connected_) {
            return;
        }

        running_ = false;
        LOG_DEBUG("Closing WebSocket connection...");

        try {
            // 发送FinishSession
            try {
                SendFinishSession();
                LOG_DEBUG("FinishSession sent");
            } catch (const std::exception& e) {
                LOG_WARNING("Failed to send FinishSession: {}", e.what());
            }

            // 发送FinishConnection
            try {
                SendFinishConnection();
                LOG_DEBUG("FinishConnection sent");
            } catch (const std::exception& e) {
                LOG_WARNING("Failed to send FinishConnection: {}", e.what());
            }

            // 关闭WebSocket
            if (ws_) {
                boost::system::error_code ec;
                ws_->close(websocket::close_code::normal, ec);
                if (ec) {
                    LOG_WARNING("WebSocket close error: {}", ec.message());
                }
            }
            connected_ = false;

            if (receive_thread_.joinable()) {
                receive_thread_.join();
            }

            LOG_INFO("WebSocket connection closed successfully");

        } catch (const std::exception& e) {
            LOG_ERROR("Error during connection close: {}", e.what());
            connected_ = false;  // 确保标记为已断开
        }
    }

private:
    void ParseUrl() {
        // URL解析: 格式 wss://host:port/path
        size_t pos = url_.find("://");
        if (pos == std::string::npos) {
            throw std::invalid_argument("Invalid URL format");
        }

        std::string scheme = url_.substr(0, pos);
        std::string rest = url_.substr(pos + 3);

        // 解析host和port
        pos = rest.find('/');
        std::string host_port;
        if (pos != std::string::npos) {
            host_port = rest.substr(0, pos);
            path_ = rest.substr(pos);
        } else {
            host_port = rest;
            path_ = "/";
        }

        pos = host_port.find(':');
        if (pos != std::string::npos) {
            host_ = host_port.substr(0, pos);
            port_ = host_port.substr(pos + 1);
        } else {
            host_ = host_port;
            port_ = (scheme == "wss") ? "443" : "80";
        }
    }

    void SendStartConnection() {
        nlohmann::json payload = nlohmann::json::object();
        auto message = common::Protocol::BuildFullRequest(common::events::START_CONNECTION, "", payload);
        LOG_INFO("SendStartConnection message: ", message.size(), " bytes");

        ws_->write(net::buffer(message));

        // 接收响应
        beast::flat_buffer resp_buffer;
        ws_->read(resp_buffer);

        std::vector<uint8_t> resp_data(resp_buffer.size());
        net::buffer_copy(net::buffer(resp_data), resp_buffer.data());

        auto response = common::Protocol::ParseResponse(resp_data);
        LOG_INFO("StartConnection response event: ", response.event);
    }

    void SendStartSession() {
        try {
            // 生成session_id
            session_id_ = GenerateSessionId();
            // LOG_DEBUG("Generated session_id: ", session_id_);

            // 从Config获取StartSession请求参数
            auto& config = common::Config::Instance();
            nlohmann::json payload = config.GenerateStartSessionRequest();
            // LOG_DEBUG("StartSession payload: ", payload.dump());

            auto message = common::Protocol::BuildFullRequest(common::events::START_SESSION, session_id_, payload);
            // LOG_DEBUG("Sending StartSession request...");
            // LOG_DEBUG("Message size: ", message.size(), " bytes");

            // 打印前64字节的十六进制
            std::ostringstream hex_stream;
            for (size_t i = 0; i < std::min(message.size(), size_t(64)); ++i) {
                hex_stream << std::hex << std::setw(2) << std::setfill('0') << (int)message[i] << " ";
            }
            // LOG_DEBUG("First 64 bytes (hex): ", hex_stream.str());

            ws_->write(net::buffer(message));

            LOG_INFO("StartSession request sent, response will be handled by receive thread");
        } catch (const std::exception& e) {
            LOG_ERROR("SendStartSession error: ", e.what());
            throw;
        }
    }

    void SendFinishSession() {
        nlohmann::json payload = nlohmann::json::object();
        auto message = common::Protocol::BuildFullRequest(common::events::FINISH_SESSION, session_id_, payload);

        ws_->write(net::buffer(message));
    }

    void SendFinishConnection() {
        nlohmann::json payload = nlohmann::json::object();
        auto message = common::Protocol::BuildFullRequest(common::events::FINISH_CONNECTION, "", payload);

        ws_->write(net::buffer(message));

        // 接收响应
        beast::flat_buffer resp_buffer;
        ws_->read(resp_buffer);

        std::vector<uint8_t> resp_data(resp_buffer.size());
        net::buffer_copy(net::buffer(resp_data), resp_buffer.data());

        auto response = common::Protocol::ParseResponse(resp_data);
        LOG_INFO("FinishConnection response event: ", response.event);
    }

    void ReceiveLoop() {
        while (running_ && connected_) {
            try {
                auto response = ReceiveResponse();

                if (callback_) {
                    callback_(response);
                }

                // 检查会话结束事件
                if (response.event == common::events::SESSION_FINISHED ||
                    response.event == common::events::SESSION_ENDED) {
                    LOG_INFO("Session finished event: ", response.event);
                    running_ = false;
                    break;
                }

            } catch (const std::exception& e) {
                LOG_ERROR("Receive loop error: ", e.what());
                running_ = false;
                break;
            }
        }
    }

    std::string GenerateSessionId() {
        return common::GenerateUUID();
    }

private:
    std::string url_;
    std::string host_;
    std::string port_;
    std::string path_;
    std::string session_id_;
    std::map<std::string, std::string> headers_;

    net::io_context ioc_;
    ssl::context ssl_ctx_;
    tcp::resolver resolver_;
    std::unique_ptr<websocket::stream<beast::ssl_stream<tcp::socket>>> ws_;

    bool connected_;
    bool running_;
    std::thread receive_thread_;
    ResponseCallback callback_;
};

RealtimeClient::RealtimeClient(const std::string& url,
                               const std::map<std::string, std::string>& headers)
    : pimpl_(std::make_unique<RealtimeClientImpl>(url, headers)) {
}

RealtimeClient::~RealtimeClient() = default;

void RealtimeClient::Connect() {
    pimpl_->Connect();
}

void RealtimeClient::SendAudioData(const std::vector<uint8_t>& audio) {
    pimpl_->SendAudioData(audio);
}

void RealtimeClient::SendTextQuery(const std::string& text) {
    pimpl_->SendTextQuery(text);
}

common::ParsedResponse RealtimeClient::ReceiveResponse() {
    return pimpl_->ReceiveResponse();
}

void RealtimeClient::SetResponseCallback(ResponseCallback callback) {
    pimpl_->SetResponseCallback(callback);
}

void RealtimeClient::Close() {
    pimpl_->Close();
}

} // namespace services
} // namespace interview
