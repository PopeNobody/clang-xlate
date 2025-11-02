#include <iostream>
#include <string>
#include <vector>
#include <json.hh>
#include <httplib.h>
#include <cstdlib>

using json = nlohmann::json;
using namespace std;

// === CONFIG ===
const string MCP_URL = "localhost";
const int MCP_PORT = 3000;
const string XAI_API_KEY = "xai_...";  // ← YOUR KEY
const string MODEL = "grok-4";

// === TOOL DEFINITIONS ===
const json TOOLS = {
    {
        {"type", "function"},
        {"function", {
            {"name", "search_knowledge"},
            {"description", "Search your persistent knowledge graph semantically"},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"query", {{"type", "string"}}},
                    {"limit", {{"type", "integer"}, {"default", 5}}}
                }},
                {"required", {"query"}}
            }}
        }}
    },
    {
        {"type", "function"},
        {"function", {
            {"name", "create_note"},
            {"description", "Create a new note in the knowledge graph"},
            {"parameters", {
                {"type", "object"},
                {"properties", {
                    {"title", {{"type", "string"}}},
                    {"content", {{"type", "string"}}},
                    {"tags", {{"type", "array"}, {"items", {{"type", "string"}}}, {"default", {}}}}
                }},
                {"required", {"title", "content"}}
            }}
        }}
    }
};

// === MCP CLIENT ===
json call_mcp(const string& method, const json& params) {
    httplib::Client cli(MCP_URL, MCP_PORT);
    json payload = {
        {"jsonrpc", "2.0"},
        {"id", 1},
        {"method", method},
        {"params", params}
    };

    auto res = cli.Post("/call", payload.dump(), "application/json");
    if (!res || res->status != 200) {
        return {{"error", "MCP server unreachable or failed"}};
    }
    return json::parse(res->body)["result"];
}

// === XAI CLIENT ===
json grok_chat(const json& messages, bool with_tools = true) {
    httplib::Client cli("api.x.ai", 443);
    cli.set_bearer_token_auth(XAI_API_KEY);

    json body = {
        {"model", MODEL},
        {"messages", messages},
        {"temperature", 0.7}
    };
    if (with_tools) body["tools"] = TOOLS;
    body["tool_choice"] = "auto";

    auto res = cli.Post("/v1/chat/completions", body.dump(), "application/json");
    if (!res || res->status != 200) {
        cerr << "xAI API error: " << (res ? res->body : "no response") << endl;
        exit(1);
    }
    return json::parse(res->body);
}

// === TOOL HANDLER ===
json handle_tool_call(const json& tool_call) {
    string name = tool_call["function"]["name"];
    auto func = tool_call.at("function");
    auto str = func.at("arguments").get<std::string>();
    auto args = json::parse(str);
    if (name == "search_knowledge") {
        return call_mcp("search_knowledge", args);
    } else if (name == "create_note") {
        return call_mcp("create_note", args);
    }
    return {{"error", "Unknown tool: " + name}};
}

// === MAIN ===
int main() {
    cout << "Grok + MCP (C++): Type 'quit' to exit.\n\n";

    vector<json> messages = {
        {{"role", "system"}, {"content", "You are Grok. Use tools to access and update the user's knowledge graph."}}
    };

    while (true) {
        cout << "You: ";
        string input;
        getline(cin, input);
        if (input == "quit") break;

        messages.push_back({{"role", "user"}, {"content", input}});

        while (true) {
            json response = grok_chat(messages);
            json msg = response["choices"][0]["message"];
            messages.push_back(msg);

            if (!msg.contains("tool_calls")) {
                cout << "Grok: " << msg["content"].get<string>() << "\n\n";
                break;
            }

            for (const auto& tool_call : msg["tool_calls"]) {
                json result = handle_tool_call(tool_call);
                messages.push_back({
                    {"role", "tool"},
                    {"tool_call_id", tool_call["id"]},
                    {"content", result.dump()}
                });
            }
        }
    }
    return 0;
}
