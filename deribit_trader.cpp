#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include "json.hpp"

using json = nlohmann::json;
namespace beast = boost::beast;       
namespace http = beast::http;           
namespace net = boost::asio;           
using tcp = boost::asio::ip::tcp;     
namespace ssl = boost::asio::ssl;    
  
// Please modify client id and client secret 
const std::string client_id = "CLIENT_ID"; 
const std::string client_secret = "CLIENT_SECRET"; 
const std::string API_BASE = "test.deribit.com";
const std::string API_PORT = "443";

class DeribitMarketTrader {
private:
    net::io_context ioc;
    ssl::context ctx;
    std::string access_token;
    std::unique_ptr<beast::ssl_stream<beast::tcp_stream>> stream;

    void initializeStream() {
        stream = std::make_unique<beast::ssl_stream<beast::tcp_stream>>(ioc, ctx);
        auto const results = net::ip::tcp::resolver(ioc).resolve(API_BASE, API_PORT);
        beast::get_lowest_layer(*stream).connect(results);
        stream->handshake(ssl::stream_base::client);
    }

    std::string sendHttpRequest(const std::string& endpoint, 
                              const std::string& method, 
                              const json& data = json(), 
                              bool isPrivate = false) {
        try {
            if (!stream) {
                initializeStream();
            }

            http::request<http::string_body> req{http::verb::post, endpoint, 11};
            req.set(http::field::host, API_BASE);
            req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
            req.set(http::field::content_type, "application/json");
            
            if (isPrivate && !access_token.empty()) {
                req.set(http::field::authorization, "Bearer " + access_token);
            }

            json rpcRequest = {
                {"jsonrpc", "2.0"},
                {"id", std::chrono::system_clock::now().time_since_epoch().count()},
                {"method", method}
            };
            
            if (!data.empty()) {
                rpcRequest["params"] = data;
            }

            req.body() = rpcRequest.dump();
            req.prepare_payload();

            beast::get_lowest_layer(*stream).expires_after(std::chrono::seconds(5));
            http::write(*stream, req);

            beast::flat_buffer buffer;
            http::response<http::dynamic_body> res;
            http::read(*stream, buffer, res);

            return beast::buffers_to_string(res.body().data());
        }
        catch (const std::exception& e) {
            std::cerr << "Error in request: " << e.what() << std::endl;
            stream.reset();
            return "{\"error\": \"" + std::string(e.what()) + "\"}";
        }
    }

public:
    DeribitMarketTrader() : ctx(ssl::context::tlsv12_client) {
        ctx.set_default_verify_paths();
        ctx.set_verify_mode(ssl::verify_peer);
    }

    bool authenticate() {
        json authParams = {
            {"grant_type", "client_credentials"},
            {"client_id", client_id},
            {"client_secret", client_secret}
        };

        std::string response = sendHttpRequest("/api/v2/public/auth", "public/auth", authParams);
        
        try {
            json jsonData = json::parse(response);
            if (jsonData.contains("result") && jsonData["result"].contains("access_token")) {
                access_token = jsonData["result"]["access_token"];
                return true;
            }
        } catch (const std::exception& e) {
            std::cerr << "Authentication error: " << e.what() << std::endl;
        }
        return false;
    }
    void cancelOrder(const std::string& order_id) {
    json params = {
        {"order_id", order_id}
    };
    sendHttpRequest("/api/v2/private/cancel", "private/cancel", params, true);
}

void modifyOrder(const std::string& order_id, const json& new_params) {
    json params = {
        {"order_id", order_id}
    };
    params.update(new_params);
    sendHttpRequest("/api/v2/private/edit", "private/edit", params, true);
}





// Market buy/sell function

   json getInstrumentDetails(const std::string& instrument_name) {
    json params = {
        {"instrument_name", instrument_name}
    };
    
    std::string response = sendHttpRequest("/api/v2/public/get_instrument", 
                                         "public/get_instrument", 
                                         params);
    return json::parse(response);
}

json marketBuy(const std::string& instrument_name, 
              double amount, 
              bool reduce_only = false,
              const std::string& label = "",
              double max_slippage = 0.05) {
    try {
        std::cout << "\n[DEBUG] Starting market buy operation..." << std::endl;
        
        // Getting instrument details first
        auto instrument_info = getInstrumentDetails(instrument_name);
        std::cout << "[DEBUG] Instrument info: " << instrument_info.dump(2) << std::endl;
        
        if (!instrument_info.contains("result")) {
            throw std::runtime_error("Unable to get instrument details");
        }
        
        // Getting contract size and tick size
        int contract_size = instrument_info["result"]["contract_size"];
        std::cout << "[DEBUG] Contract size: " << contract_size << std::endl;
        
        // Getting current market price
        auto orderbook = getOrderBook(instrument_name, 1);
        if (!orderbook.contains("result") || 
            !orderbook["result"].contains("best_ask_price")) {
            throw std::runtime_error("Unable to get current market price");
        }
        
        double market_price = orderbook["result"]["best_ask_price"];
        std::cout << "[DEBUG] Current market price: " << market_price << std::endl;

        // Calculating number of contracts
        double usd_value = amount * market_price;
        int num_contracts = static_cast<int>(usd_value);
        // Rounding to nearest multiple of contract_size
        num_contracts = (num_contracts / contract_size) * contract_size;
        
        std::cout << "[DEBUG] BTC amount: " << amount << std::endl;
        std::cout << "[DEBUG] USD value: " << usd_value << std::endl;
        std::cout << "[DEBUG] Adjusted contracts: " << num_contracts << std::endl;

        if (num_contracts <= 0) {
            throw std::runtime_error("Amount too small for minimum contract size");
        }

        json params = {
            {"instrument_name", instrument_name},
            {"amount", num_contracts},
            {"type", "market"},
            {"reduce_only", reduce_only}
        };

        if (!label.empty()) {
            params["label"] = label;
        }

        if (max_slippage > 0) {
            double max_price = market_price * (1 + max_slippage);
            params["max_price"] = max_price;
            std::cout << "[DEBUG] Max price set to: " << max_price << std::endl;
        }

        std::cout << "[DEBUG] Sending order with params: " << params.dump(2) << std::endl;
        
        std::string response = sendHttpRequest("/api/v2/private/buy", 
                                             "private/buy", 
                                             params, 
                                             true);
        
        std::cout << "[DEBUG] Raw API response: " << response << std::endl;
        
        json result = json::parse(response);
        
        if (result.contains("error")) {
            std::cout << "[ERROR] Order error: " << result["error"].dump(2) << std::endl;
            return result;
        }
        
        return result;
    } catch (const std::exception& e) {
        std::cout << "[ERROR] Exception in market buy: " << e.what() << std::endl;
        return json{{"error", e.what()}};
    }
}
    // Market sell function
    json marketSell(const std::string& instrument_name, 
                   double amount, 
                   bool reduce_only = false,
                   const std::string& label = "",
                   double max_slippage = 0.05) {
        
        json params = {
            {"instrument_name", instrument_name},
            {"amount", amount},
            {"type", "market"},
            {"reduce_only", reduce_only}
        };

        if (!label.empty()) {
            params["label"] = label;
        }

        // Getting current market price to calculate max_show_amount
        auto orderbook = getOrderBook(instrument_name, 1);
        if (orderbook.contains("result") && 
            orderbook["result"].contains("best_bid_price")) {
            
            double market_price = orderbook["result"]["best_bid_price"];
            params["max_show_amount"] = amount;  
            
            // Adding slippage protection
            if (max_slippage > 0) {
                double min_price = market_price * (1 - max_slippage);
                params["min_price"] = min_price;
            }
        }

        std::string response = sendHttpRequest("/api/v2/private/sell", 
                                             "private/sell", 
                                             params, 
                                             true);
        return json::parse(response);
    }

    json getOrderBook(const std::string& instrument_name, int depth = 1) {
        json params = {
            {"instrument_name", instrument_name},
            {"depth", depth}
        };
        
        std::string response = sendHttpRequest("/api/v2/public/get_order_book", 
                                             "public/get_order_book", 
                                             params);
        return json::parse(response);
    }

    // Getting positions
    json getPositions(const std::string& currency) {
        json params = {
            {"currency", currency}
        };
        
        std::string response = sendHttpRequest("/api/v2/private/get_positions", 
                                             "private/get_positions", 
                                             params, 
                                             true);
        return json::parse(response);
    }

    // Getting account summary
    json getAccountSummary(const std::string& currency) {
        json params = {
            {"currency", currency}
        };
        
        std::string response = sendHttpRequest("/api/v2/private/get_account_summary", 
                                             "private/get_account_summary", 
                                             params, 
                                             true);
        return json::parse(response);
    }
};

int main() {
    DeribitMarketTrader trader;

    if (!trader.authenticate()) {
        std::cerr << "Authentication failed" << std::endl;
        return 1;
    }
    std::cout << "Successfully authenticated!" << std::endl;

    try {
        std::string instrument = "ETH-PERPETUAL";
        double amount = 2;

        auto buyOrder = trader.marketBuy(instrument, amount, false, "test_buy", 0.02);
        if (buyOrder.contains("error")) {
            std::cout << "Buy order failed: " << buyOrder["error"] << std::endl;
        } else {
            std::cout << "Buy order succeeded: " << buyOrder["result"].dump(2) << std::endl;
        }

        
        std::string orderId = buyOrder["result"]["order"]["order_id"];
        trader.cancelOrder(orderId);
        std::cout << "Buy order cancelled" << std::endl;

        
        json modifiedParams = {
            {"amount", 0.05},
            {"label", "modified_buy"}
        };
        trader.modifyOrder(orderId, modifiedParams);
        std::cout << "Buy order modified" << std::endl;

       
        auto orderBook = trader.getOrderBook(instrument, 5);
        std::cout << "Order book: " << orderBook.dump(2) << std::endl;

        
        auto positions = trader.getPositions("ETH");
        std::cout << "User positions: " << positions.dump(2) << std::endl;

       
        auto accountSummary = trader.getAccountSummary("ETH");
        std::cout << "Account summary: " << accountSummary.dump(2) << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}